#include "storage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDebug>

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

    const QString createEvents = R"(
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT,
            message_id TEXT UNIQUE,
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
            is_subagent INTEGER
        );
    )";

    if (!q.exec(createEvents)) {
        qWarning() << "Failed to create events table:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_date ON events(date);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_project ON events(project);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_events_model ON events(model);");

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

    if (raw == "home-juan" || raw == "home")
        return "Home";

    const QString homePrefix = "home-juan-";
    if (raw.startsWith(homePrefix)) {
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
    m_db.transaction();

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(R"(
        INSERT OR IGNORE INTO events (
            session_id, message_id, timestamp, date, project, agent, model, provider,
            tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent
        ) VALUES (
            :session_id, :message_id, :timestamp, :date, :project, :agent, :model, :provider,
            :tokens_in, :tokens_out, :tokens_reasoning, :tokens_total, :cost_usd, :is_subagent
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

                if (insertQuery.exec() && insertQuery.numRowsAffected() > 0) {
                    newRecords++;
                }
            }
        }
    }

    m_db.commit();
    return newRecords;
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
