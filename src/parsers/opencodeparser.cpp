#include "opencodeparser.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtGlobal>
#include <QDebug>

QString OpenCodeParser::source() const {
    return QStringLiteral("opencode");
}

QString OpenCodeParser::extractProject(const QString &sessionPath, const QString &sessionDir,
                                       const QString &cwd, const QString &slug) {
    if (!sessionPath.isEmpty())
        return sessionPath;
    const QString dir = !cwd.isEmpty() ? cwd : sessionDir;
    if (!dir.isEmpty()) {
        const QString home = QDir::homePath();
        if (dir.startsWith(home)) {
            QString relative = dir.mid(home.size());
            while (relative.startsWith('/'))
                relative.remove(0, 1);
            if (!relative.isEmpty())
                return relative;
            return QStringLiteral("Home");
        }
        return dir;
    }
    return slug.isEmpty() ? QStringLiteral("Home") : slug;
}

ParseResult OpenCodeParser::parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const {
    ParseResult result;
    if (rootPath.isEmpty())
        return result;

    QString dbPath = rootPath;
    QFileInfo info(dbPath);
    if (info.isDir()) {
        dbPath = QDir(rootPath).filePath(QStringLiteral("opencode.db"));
        info = QFileInfo(dbPath);
    }
    if (!info.exists() || !info.isFile())
        return result;

    const QString walPath = dbPath + QStringLiteral("-wal");
    const QFileInfo walInfo(walPath);
    qint64 mtime = info.lastModified().toMSecsSinceEpoch();
    qint64 size = info.size();
    if (walInfo.exists()) {
        mtime = qMax(mtime, walInfo.lastModified().toMSecsSinceEpoch());
        size += walInfo.size();
    }

    if (shouldSkip && shouldSkip(dbPath, mtime, size))
        return result;

    const QString connName = QStringLiteral("opencode_parser_")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QList<NormalizedEvent> events;
    QMap<QString, double> sessionCosts;
    QMap<QString, qint64> sessionTokens;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            qWarning() << "OpenCodeParser: cannot open" << dbPath << ":" << db.lastError().text();
        } else {
            QSqlQuery query(db);
            const QString sql = QStringLiteral(
                "SELECT "
                "  m.id, "
                "  m.session_id, "
                "  m.time_created, "
                "  m.data, "
                "  s.parent_id, "
                "  s.directory, "
                "  s.path, "
                "  s.slug, "
                "  s.title, "
                "  s.cost "
                "FROM message m "
                "JOIN session s ON m.session_id = s.id "
                "WHERE json_extract(m.data, '$.role') = 'assistant' "
                "ORDER BY s.id, m.time_created ASC;");

            if (!query.exec(sql)) {
                qWarning() << "OpenCodeParser query error:" << query.lastError().text();
            } else {
                QSet<QString> seenIds;
                while (query.next()) {
                    const QString messageId = query.value(0).toString();
                    if (messageId.isEmpty() || seenIds.contains(messageId))
                        continue;
                    seenIds.insert(messageId);

                    const QString sessionId = query.value(1).toString();
                    const qint64 colTimeCreated = query.value(2).toLongLong();
                    const QString dataStr = query.value(3).toString();
                    const QString parentId = query.value(4).toString();
                    const QString directory = query.value(5).toString();
                    const QString path = query.value(6).toString();
                    const QString slug = query.value(7).toString();
                    const double sessionCost = query.value(9).toDouble();

                    if (!sessionCosts.contains(sessionId))
                        sessionCosts.insert(sessionId, sessionCost);

                    const QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8());
                    if (!doc.isObject())
                        continue;
                    const QJsonObject dataObj = doc.object();
                    if (dataObj.value("role").toString() != "assistant")
                        continue;

                    const QJsonObject tokensObj = dataObj.value("tokens").toObject();
                    const qint64 input = static_cast<qint64>(tokensObj.value("input").toDouble());
                    const qint64 output = static_cast<qint64>(tokensObj.value("output").toDouble());
                    const qint64 reasoning = static_cast<qint64>(tokensObj.value("reasoning").toDouble());
                    const qint64 reportedTotal = static_cast<qint64>(tokensObj.value("total").toDouble());

                    qint64 cacheRead = 0;
                    qint64 cacheWrite = 0;
                    const QJsonObject cacheObj = tokensObj.value("cache").toObject();
                    if (!cacheObj.isEmpty()) {
                        cacheRead = static_cast<qint64>(cacheObj.value("read").toDouble());
                        cacheWrite = static_cast<qint64>(cacheObj.value("write").toDouble());
                    }

                    qint64 timestampMs = colTimeCreated;
                    if (timestampMs <= 0) {
                        const QJsonObject timeObj = dataObj.value("time").toObject();
                        const QJsonValue createdVal = timeObj.value("created");
                        if (createdVal.isDouble())
                            timestampMs = static_cast<qint64>(createdVal.toDouble());
                    }
                    if (timestampMs <= 0)
                        timestampMs = mtime;

                    const QJsonObject pathObj = dataObj.value("path").toObject();
                    const QString cwd = pathObj.value("cwd").toString();
                    const QString project = extractProject(path, directory, cwd, slug);

                    QString model = dataObj.value("modelID").toString();
                    if (model.isEmpty())
                        model = dataObj.value("model").toString();
                    if (model.isEmpty())
                        model = QStringLiteral("unknown");

                    QString provider = dataObj.value("providerID").toString();
                    if (provider.isEmpty())
                        provider = dataObj.value("provider").toString();
                    if (provider.isEmpty())
                        provider = QStringLiteral("opencode");

                    const QString agentField = dataObj.value("agent").toString();
                    const bool isSubagent = !parentId.isEmpty()
                                            || agentField.contains("subagent", Qt::CaseInsensitive);

                    NormalizedEvent event;
                    event.source = source();
                    event.sessionId = sessionId;
                    event.messageId = messageId;
                    event.timestampMs = timestampMs;
                    event.project = project;
                    event.agent = isSubagent ? QStringLiteral("subagent") : QStringLiteral("opencode");
                    event.model = model;
                    event.provider = provider;
                    event.tokensIn = input + cacheRead + cacheWrite;
                    event.tokensOut = output;
                    event.tokensReasoning = reasoning;
                    event.tokensTotal = reportedTotal > 0
                                            ? reportedTotal
                                            : (event.tokensIn + event.tokensOut + event.tokensReasoning);
                    event.costUsd = dataObj.value("cost").toDouble();
                    event.isSubagent = isSubagent;

                    sessionTokens[sessionId] += event.tokensTotal;
                    events.append(event);
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);

    // Prorate session cost if messages reported no individual cost.
    for (auto it = sessionCosts.constBegin(); it != sessionCosts.constEnd(); ++it) {
        const QString &sessionId = it.key();
        const double sessCost = it.value();
        if (sessCost <= 0.0)
            continue;

        bool hasIndividualCost = false;
        for (const NormalizedEvent &ev : events) {
            if (ev.sessionId == sessionId && ev.costUsd > 0.0) {
                hasIndividualCost = true;
                break;
            }
        }

        const qint64 totalTokens = sessionTokens.value(sessionId, 0);
        if (!hasIndividualCost && totalTokens > 0) {
            for (NormalizedEvent &ev : events) {
                if (ev.sessionId == sessionId) {
                    ev.costUsd = sessCost * (static_cast<double>(ev.tokensTotal)
                                             / static_cast<double>(totalTokens));
                }
            }
        }
    }

    result.events = events;
    result.files.append({dbPath, mtime, size});
    return result;
}
