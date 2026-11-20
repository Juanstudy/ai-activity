#include "storage.h"

#include <algorithm>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringList>
#include <QUuid>
#include <QDebug>

namespace {

// The events table now carries a source dimension. Dedupe is scoped to
// (source, message_id) so two CLIs can emit the same message id without one
// silently overwriting the other.
const char *const kCreateEventsSql = R"(
    CREATE TABLE IF NOT EXISTS events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT,
        message_id TEXT,
        timestamp INTEGER,
        date TEXT,
        project TEXT,
        agent TEXT,
        model TEXT,
        provider TEXT,
        tokens_in INTEGER,
        tokens_out INTEGER,
        tokens_reasoning INTEGER,
        tokens_total INTEGER,
        cost_usd REAL,
        is_subagent INTEGER,
        source TEXT NOT NULL DEFAULT 'pi',
        UNIQUE(source, message_id)
    );
)";

const char *const kCreateSyncStateSql = R"(
    CREATE TABLE IF NOT EXISTS sync_state (
        source TEXT NOT NULL,
        file_path TEXT NOT NULL,
        mtime INTEGER NOT NULL,
        size INTEGER NOT NULL,
        PRIMARY KEY (source, file_path)
    );
)";

const char *const kEventIndexes[] = {
    "CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);",
    "CREATE INDEX IF NOT EXISTS idx_events_date ON events(date);",
    "CREATE INDEX IF NOT EXISTS idx_events_project ON events(project);",
    "CREATE INDEX IF NOT EXISTS idx_events_model ON events(model);",
};

// Backfills every legacy row as Pi history while preserving ids and values.
const char *const kCopyLegacyEventsSql = R"(
    INSERT INTO events (
        id, session_id, message_id, timestamp, date, project, agent, model, provider,
        tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent, source
    )
    SELECT
        id, session_id, message_id, timestamp, date, project, agent, model, provider,
        tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent, 'pi'
    FROM events_legacy;
)";

bool createEventIndexes(QSqlQuery *query) {
    for (const char *sql : kEventIndexes) {
        if (!query->exec(sql))
            return false;
    }
    return true;
}

}  // namespace

Storage::Storage(const QString &dbPath, QObject *parent)
    : QObject(parent),
      m_connectionName(QUuid::createUuid().toString()),
      m_dbPath(dbPath) {
    if (m_dbPath.isEmpty()) {
        const QString dataDir = QDir::homePath() + "/.local/share/ai-activity";
        QDir().mkpath(dataDir);
        m_dbPath = dataDir + "/usage.db";
    }
}

Storage::~Storage() {
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Storage::init() {
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open SQLite database at" << m_dbPath << m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode = WAL;");
    q.exec("PRAGMA synchronous = NORMAL;");

    // A fresh database gets the new schema directly. On a legacy database this
    // is a no-op and the migration below rebuilds the table instead.
    if (!q.exec(kCreateEventsSql)) {
        qWarning() << "Failed to create events table:" << q.lastError().text();
        return false;
    }

    if (!eventsHaveColumn(&q, "events", "source")) {
        if (!migrateEventsToSourceSchema(&q)) {
            qWarning() << "Failed to migrate events table to the source schema for" << m_dbPath;
            return false;
        }
    } else if (!eventsHaveCompositeUnique(&q)) {
        // Never add a weaker dedupe key or rewrite rows in place here: that
        // could silently drop history. Log loudly and leave the data intact.
        qWarning() << "events has a source column but no UNIQUE(source, message_id); "
                      "message-id dedupe is not enforced for" << m_dbPath;
    }

    if (!createEventIndexes(&q)) {
        qWarning() << "Failed to create events indexes:" << q.lastError().text();
        return false;
    }

    if (!q.exec(kCreateSyncStateSql)) {
        qWarning() << "Failed to create sync_state table:" << q.lastError().text();
        return false;
    }

    return true;
}

bool Storage::eventsHaveColumn(QSqlQuery *query, const QString &table, const QString &column) const {
    if (!query->exec(QString("PRAGMA table_info(%1);").arg(table)))
        return false;
    while (query->next()) {
        if (query->value("name").toString() == column)
            return true;
    }
    return false;
}

bool Storage::eventsHaveCompositeUnique(QSqlQuery *query) const {
    QStringList uniqueIndexes;
    if (!query->exec("PRAGMA index_list(events);"))
        return false;
    while (query->next()) {
        if (query->value("unique").toBool())
            uniqueIndexes << query->value("name").toString();
    }

    // Reuse the same query object for each index; the previous result set has
    // been fully consumed above.
    for (const QString &indexName : uniqueIndexes) {
        if (!query->exec(QString("PRAGMA index_info(%1);").arg(indexName)))
            continue;
        QStringList columns;
        while (query->next())
            columns << query->value("name").toString();
        if (columns.size() == 2 && columns.contains("source") && columns.contains("message_id"))
            return true;
    }
    return false;
}

bool Storage::migrateEventsToSourceSchema(QSqlQuery *query) {
    if (!m_db.transaction()) {
        qWarning() << "Failed to start migration transaction:" << m_db.lastError().text();
        return false;
    }

    auto execStep = [query](const QString &sql, const char *what) -> bool {
        if (!query->exec(sql)) {
            qWarning() << what << query->lastError().text();
            return false;
        }
        return true;
    };

    const bool rebuilt =
        execStep("ALTER TABLE events RENAME TO events_legacy;", "Failed to rename legacy events table:") &&
        execStep(kCreateEventsSql, "Failed to create new events table:") &&
        execStep(kCopyLegacyEventsSql, "Failed to copy legacy events rows:") &&
        execStep("DROP TABLE events_legacy;", "Failed to drop legacy events table:");

    if (!rebuilt) {
        m_db.rollback();
        return false;
    }

    if (!createEventIndexes(query)) {
        qWarning() << "Failed to recreate events indexes during migration:" << query->lastError().text();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qWarning() << "Failed to commit migration transaction:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

FileSyncState Storage::fileSyncState(const QString &source, const QString &filePath) const {
    FileSyncState state;
    QSqlQuery q(m_db);
    q.prepare("SELECT mtime, size FROM sync_state WHERE source = :source AND file_path = :path;");
    q.bindValue(":source", source);
    q.bindValue(":path", filePath);
    if (q.exec() && q.next()) {
        state.found = true;
        state.mtime = q.value("mtime").toLongLong();
        state.size = q.value("size").toLongLong();
    }
    return state;
}

bool Storage::setFileSyncState(const QString &source, const QString &filePath, qint64 mtime, qint64 size) {
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO sync_state (source, file_path, mtime, size)
        VALUES (:source, :path, :mtime, :size)
        ON CONFLICT(source, file_path) DO UPDATE SET mtime = excluded.mtime, size = excluded.size;
    )");
    q.bindValue(":source", source);
    q.bindValue(":path", filePath);
    q.bindValue(":mtime", mtime);
    q.bindValue(":size", size);
    if (!q.exec()) {
        qWarning() << "Failed to record sync state for" << filePath << q.lastError().text();
        return false;
    }
    return true;
}

QString Storage::defaultSessionsDir() const {
    return QDir::homePath() + "/.pi/agent/sessions";
}

QString Storage::extractProjectFromDirName(const QString &dirName) {
    QString raw = dirName;
    if (raw.startsWith("--"))
        raw = raw.mid(2);
    if (raw.endsWith("--"))
        raw.chop(2);

    if (raw.isEmpty())
        return "root";

    if (dirName.contains("OpenBot-Agents") || dirName.contains("gentle-agents")) {
        const auto parts = dirName.split("-", Qt::SkipEmptyParts);
        return parts.isEmpty() ? "Subagent" : QString("Agent (%1)").arg(parts.last());
    }

    const QString userName = QFileInfo(QDir::homePath()).fileName();
    const QString homePrefix = "home-" + userName + "-";
    if (raw == "home-" + userName || raw == "home")
        return "Home";

    if (!userName.isEmpty() && raw.startsWith(homePrefix)) {
        raw = raw.mid(homePrefix.length());
    } else if (raw.startsWith("home-")) {
        int secondDash = raw.indexOf('-', 5);
        if (secondDash != -1)
            raw = raw.mid(secondDash + 1);
    }

    if (raw.isEmpty())
        return "Home";

    const QStringList commonRoots = {"Projects-", "Work-", "Study-", "Downloads-", "code-", "dev-"};
    for (const auto &root : commonRoots) {
        if (raw.startsWith(root, Qt::CaseInsensitive)) {
            QString rootName = root.left(root.length() - 1);
            QString sub = raw.mid(root.length());
            return rootName + "/" + sub;
        }
    }

    return raw;
}

int Storage::syncFromPiSessions(const QString &customSessionsDir) {
    const QString targetDir = customSessionsDir.isEmpty() ? defaultSessionsDir() : customSessionsDir;
    QDir rootDir(targetDir);
    if (!rootDir.exists())
        return 0;

    int newRecords = 0;
    if (!m_db.transaction()) {
        qWarning() << "Failed to start database transaction:" << m_db.lastError().text();
        return 0;
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(R"(
        INSERT OR IGNORE INTO events (
            session_id, message_id, timestamp, date, project, agent, model, provider,
            tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent, source
        ) VALUES (
            :session_id, :message_id, :timestamp, :date, :project, :agent, :model, :provider,
            :tokens_in, :tokens_out, :tokens_reasoning, :tokens_total, :cost_usd, :is_subagent, :source
        );
    )");

    const auto sessionDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &dirInfo : sessionDirs) {
        const QString dirName = dirInfo.fileName();
        const QString project = extractProjectFromDirName(dirName);
        const bool isSubagent = dirName.contains("Agent") || dirName.contains("gentle-agents");
        const QString agentName = isSubagent ? "subagent" : "pi";

        QDir sDir(dirInfo.absoluteFilePath());
        const auto jsonlFiles = sDir.entryInfoList({"*.jsonl"}, QDir::Files);
        for (const auto &fileInfo : jsonlFiles) {
            QFile file(fileInfo.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            const QString sessionId = fileInfo.baseName();
            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                if (line.isEmpty() || !line.contains("\"usage\""))
                    continue;

                const auto doc = QJsonDocument::fromJson(line);
                if (!doc.isObject())
                    continue;

                const auto rootObj = doc.object();
                if (rootObj.value("type").toString() != "message")
                    continue;

                const auto msgObj = rootObj.value("message").toObject();
                if (msgObj.value("role").toString() != "assistant")
                    continue;

                QJsonObject usageObj = rootObj.value("usage").toObject();
                QString model = rootObj.value("model").toString();
                QString provider = rootObj.value("provider").toString();

                if (usageObj.isEmpty() && !msgObj.isEmpty()) {
                    usageObj = msgObj.value("usage").toObject();
                    if (model.isEmpty())
                        model = msgObj.value("model").toString();
                    if (provider.isEmpty())
                        provider = msgObj.value("provider").toString();
                }

                if (usageObj.isEmpty())
                    continue;

                const QString msgId = rootObj.value("id").toString();
                if (msgId.isEmpty())
                    continue;

                qint64 tsMs = 0;
                if (rootObj.contains("timestamp")) {
                    const auto tsVal = rootObj.value("timestamp");
                    if (tsVal.isDouble()) {
                        tsMs = static_cast<qint64>(tsVal.toDouble());
                    } else if (tsVal.isString()) {
                        tsMs = QDateTime::fromString(tsVal.toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
                        if (tsMs <= 0)
                            tsMs = QDateTime::fromString(tsVal.toString(), Qt::ISODate).toMSecsSinceEpoch();
                    }
                }
                if (tsMs <= 0)
                    tsMs = fileInfo.lastModified().toMSecsSinceEpoch();

                const QString dateStr = QDateTime::fromMSecsSinceEpoch(tsMs).toString("yyyy-MM-dd");
                // Already extracted model and provider above
                if (model.isEmpty())
                    model = "unknown";

                const qint64 inTokens = static_cast<qint64>(usageObj.value("input").toDouble());
                const qint64 outTokens = static_cast<qint64>(usageObj.value("output").toDouble());
                const qint64 totalTokens = static_cast<qint64>(usageObj.value("totalTokens").toDouble());
                const qint64 reasoningTokens = static_cast<qint64>(usageObj.value("reasoning").toDouble());

                double costUsd = 0.0;
                const auto costObj = usageObj.value("cost").toObject();
                if (!costObj.isEmpty()) {
                    costUsd = costObj.value("total").toDouble();
                }

                insertQuery.bindValue(":session_id", sessionId);
                insertQuery.bindValue(":message_id", msgId);
                insertQuery.bindValue(":timestamp", tsMs);
                insertQuery.bindValue(":date", dateStr);
                insertQuery.bindValue(":project", project);
                insertQuery.bindValue(":agent", agentName);
                insertQuery.bindValue(":model", model.isEmpty() ? "unknown" : model);
                insertQuery.bindValue(":provider", provider);
                insertQuery.bindValue(":tokens_in", inTokens);
                insertQuery.bindValue(":tokens_out", outTokens);
                insertQuery.bindValue(":tokens_reasoning", reasoningTokens);
                insertQuery.bindValue(":tokens_total", totalTokens > 0 ? totalTokens : (inTokens + outTokens));
                insertQuery.bindValue(":cost_usd", costUsd);
                insertQuery.bindValue(":is_subagent", isSubagent ? 1 : 0);
                insertQuery.bindValue(":source", "pi");

                if (insertQuery.exec() && insertQuery.numRowsAffected() > 0) {
                    newRecords++;
                }
            }
        }
    }

    if (!m_db.commit()) {
        qWarning() << "Failed to commit database transaction:" << m_db.lastError().text();
        m_db.rollback();
        return 0;
    }
    return newRecords;
}

int Storage::insertEvents(const QList<NormalizedEvent> &events) {
    if (!m_db.transaction()) {
        qWarning() << "Failed to start insert transaction:" << m_db.lastError().text();
        return -1;
    }

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(R"(
        INSERT OR IGNORE INTO events (
            session_id, message_id, timestamp, date, project, agent, model, provider,
            tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent, source
        ) VALUES (
            :session_id, :message_id, :timestamp, :date, :project, :agent, :model, :provider,
            :tokens_in, :tokens_out, :tokens_reasoning, :tokens_total, :cost_usd, :is_subagent, :source
        );
    )");

    int inserted = 0;
    for (const NormalizedEvent &event : events) {
        insertQuery.bindValue(":session_id", event.sessionId);
        insertQuery.bindValue(":message_id", event.messageId);
        insertQuery.bindValue(":timestamp", event.timestampMs);
        insertQuery.bindValue(":date", QDateTime::fromMSecsSinceEpoch(event.timestampMs).toString("yyyy-MM-dd"));
        insertQuery.bindValue(":project", event.project);
        insertQuery.bindValue(":agent", event.agent);
        insertQuery.bindValue(":model", event.model);
        insertQuery.bindValue(":provider", event.provider);
        insertQuery.bindValue(":tokens_in", event.tokensIn);
        insertQuery.bindValue(":tokens_out", event.tokensOut);
        insertQuery.bindValue(":tokens_reasoning", event.tokensReasoning);
        insertQuery.bindValue(":tokens_total", event.tokensTotal);
        insertQuery.bindValue(":cost_usd", event.costUsd);
        insertQuery.bindValue(":is_subagent", event.isSubagent ? 1 : 0);
        insertQuery.bindValue(":source", event.source);

        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert event" << event.messageId << insertQuery.lastError().text();
            m_db.rollback();
            return -1;
        }
        if (insertQuery.numRowsAffected() > 0)
            inserted++;
    }

    if (!m_db.commit()) {
        qWarning() << "Failed to commit insert transaction:" << m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    return inserted;
}

int Storage::syncSource(const QString &source, const UsageParser &parser, const QString &rootPath) {
    FileSkipPredicate shouldSkip = [this, &source](const QString &filePath, qint64 mtime, qint64 size) {
        const FileSyncState state = fileSyncState(source, filePath);
        return state.found && state.mtime == mtime && state.size == size;
    };

    const ParseResult result = parser.parse(rootPath, shouldSkip);
    const int inserted = insertEvents(result.events);
    if (inserted < 0)
        return 0;

    // Stamps are only durable once the rows are committed, so a crash before
    // the insert cannot leave a file marked as synced with missing events.
    for (const FileStamp &stamp : result.files)
        setFileSyncState(source, stamp.filePath, stamp.mtime, stamp.size);
    return inserted;
}

UsageSummary Storage::getSummary(qint64 startTimeMs, qint64 endTimeMs) const {
    UsageSummary summary;
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT 
            COALESCE(SUM(cost_usd), 0.0),
            COALESCE(SUM(tokens_total), 0),
            COALESCE(SUM(tokens_in), 0),
            COALESCE(SUM(tokens_out), 0),
            COALESCE(SUM(tokens_reasoning), 0),
            COUNT(*),
            COUNT(DISTINCT session_id)
        FROM events
        WHERE timestamp >= :start AND timestamp <= :end;
    )");
    q.bindValue(":start", startTimeMs);
    q.bindValue(":end", endTimeMs);

    if (q.exec() && q.next()) {
        summary.totalCostUsd = q.value(0).toDouble();
        summary.totalTokens = q.value(1).toLongLong();
        summary.inputTokens = q.value(2).toLongLong();
        summary.outputTokens = q.value(3).toLongLong();
        summary.thinkingTokens = q.value(4).toLongLong();
        summary.totalTurns = q.value(5).toInt();
        summary.totalSessions = q.value(6).toInt();
    }
    return summary;
}

StreakInfo Storage::getStreak(const QDate &referenceDate) const {
    StreakInfo info;
    QSqlQuery q(m_db);
    q.prepare("SELECT DISTINCT date FROM events ORDER BY date DESC;");
    if (!q.exec()) {
        return info;
    }

    QList<QDate> dates;
    while (q.next()) {
        const QString str = q.value(0).toString();
        const QDate d = QDate::fromString(str, Qt::ISODate);
        if (d.isValid() && (dates.isEmpty() || dates.last() != d)) {
            dates.append(d);
        }
    }

    if (dates.isEmpty()) {
        return info;
    }

    // Current streak calculation relative to referenceDate
    // The streak is active if there is activity on referenceDate OR referenceDate - 1 (yesterday)
    QDate currentExpected;
    bool streakStarted = false;

    for (const QDate &d : dates) {
        if (!streakStarted) {
            if (d == referenceDate) {
                info.currentStreak = 1;
                currentExpected = referenceDate.addDays(-1);
                streakStarted = true;
            } else if (d == referenceDate.addDays(-1)) {
                info.currentStreak = 1;
                currentExpected = referenceDate.addDays(-2);
                streakStarted = true;
            } else if (d < referenceDate.addDays(-1)) {
                // Latest activity is older than yesterday, current streak is 0
                break;
            }
        } else {
            if (d == currentExpected) {
                info.currentStreak++;
                currentExpected = currentExpected.addDays(-1);
            } else if (d < currentExpected) {
                // Gap in streak
                break;
            }
        }
    }

    // Longest streak calculation across all recorded history
    QList<QDate> ascDates = dates;
    std::sort(ascDates.begin(), ascDates.end());

    int longest = 0;
    int currentRun = 0;
    QDate prev;

    for (const QDate &d : ascDates) {
        if (prev.isNull()) {
            currentRun = 1;
        } else if (d == prev.addDays(1)) {
            currentRun++;
        } else {
            currentRun = 1;
        }
        longest = std::max(longest, currentRun);
        prev = d;
    }

    info.maxStreak = longest;
    return info;
}

QVariantList Storage::getHeatmapDays(qint64 startTimeMs, qint64 endTimeMs) const {
    QVariantList list;
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT 
            date,
            COUNT(*) as turns,
            COALESCE(SUM(cost_usd), 0.0) as cost_usd,
            COALESCE(SUM(tokens_total), 0) as tokens_total,
            COALESCE(SUM(tokens_reasoning), 0) as tokens_reasoning
        FROM events
        WHERE timestamp >= :start AND timestamp <= :end
        GROUP BY date
        ORDER BY date ASC;
    )");
    q.bindValue(":start", startTimeMs);
    q.bindValue(":end", endTimeMs);

    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item["date"] = q.value("date").toString();
            item["turns"] = q.value("turns").toInt();
            item["costUsd"] = q.value("cost_usd").toDouble();
            item["tokensTotal"] = q.value("tokens_total").toLongLong();
            item["tokensReasoning"] = q.value("tokens_reasoning").toLongLong();
            list.append(item);
        }
    }
    return list;
}

QVariantList Storage::getBreakdown(const QString &dimension, qint64 startTimeMs, qint64 endTimeMs) const {
    QVariantList list;
    QString colName = "project";
    if (dimension == "model")
        colName = "model";
    else if (dimension == "agent")
        colName = "agent";

    QSqlQuery q(m_db);
    const QString sql = QString(R"(
        SELECT 
            %1 as name,
            COUNT(*) as turns,
            COALESCE(SUM(cost_usd), 0.0) as cost_usd,
            COALESCE(SUM(tokens_total), 0) as tokens_total,
            COALESCE(SUM(tokens_reasoning), 0) as tokens_reasoning
        FROM events
        WHERE timestamp >= :start AND timestamp <= :end
        GROUP BY %1
        ORDER BY cost_usd DESC, tokens_total DESC;
    )").arg(colName);

    q.prepare(sql);
    q.bindValue(":start", startTimeMs);
    q.bindValue(":end", endTimeMs);

    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item["name"] = q.value("name").toString();
            item["turns"] = q.value("turns").toInt();
            item["costUsd"] = q.value("cost_usd").toDouble();
            item["tokensTotal"] = q.value("tokens_total").toLongLong();
            item["tokensReasoning"] = q.value("tokens_reasoning").toLongLong();
            list.append(item);
        }
    }
    return list;
}
