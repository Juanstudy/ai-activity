#include "t3parser.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

QString T3Parser::source() const {
    return QStringLiteral("t3");
}

QString T3Parser::projectFromWorkspace(const QString &workspaceRoot) {
    if (workspaceRoot.isEmpty())
        return QStringLiteral("No project");
    const QString home = QDir::homePath();
    if (workspaceRoot.startsWith(home)) {
        QString relative = workspaceRoot.mid(home.size());
        while (relative.startsWith('/'))
            relative.remove(0, 1);
        if (relative.isEmpty())
            return QStringLiteral("Home");
        if (relative == QLatin1String(".t3/scratch") || relative.startsWith(QLatin1String(".t3/scratch/")))
            return QStringLiteral("No project");
        return relative;
    }
    return workspaceRoot;
}

qint64 T3Parser::timestampMs(const QJsonValue &value, const QFileInfo &fileInfo) {
    qint64 result = 0;
    if (value.isDouble()) {
        result = static_cast<qint64>(value.toDouble());
    } else if (value.isString()) {
        result = QDateTime::fromString(value.toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
        if (result <= 0)
            result = QDateTime::fromString(value.toString(), Qt::ISODate).toMSecsSinceEpoch();
    }
    if (result <= 0)
        result = fileInfo.lastModified().toMSecsSinceEpoch();
    return result;
}

QMap<QString, T3ThreadInfo> T3Parser::readThreadMap(const QString &dbPath) {
    QMap<QString, T3ThreadInfo> map;
    if (dbPath.isEmpty() || !QFileInfo::exists(dbPath))
        return map;

    const QString connName = QStringLiteral("t3_parser_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        db.setDatabaseName(dbPath);
        if (db.open()) {
            QSqlQuery query(db);
            const QString sql = QStringLiteral(
                "SELECT json_extract(pt.payload_json, '$.nativeThreadRef.nativeId'), "
                "       p.workspace_root, p.title, "
                "       json_extract(t.payload_json, '$.lineage.relationshipToParent') "
                "FROM orchestration_v2_projection_provider_threads pt "
                "JOIN orchestration_v2_projection_threads t ON t.thread_id = pt.thread_id "
                "JOIN projection_projects p ON p.project_id = t.project_id");
            if (query.exec(sql)) {
                while (query.next()) {
                    const QString convId = query.value(0).toString();
                    const QString ws = query.value(1).toString();
                    const QString title = query.value(2).toString();
                    const QString relationship = query.value(3).toString();
                    if (!convId.isEmpty()) {
                        QString proj = projectFromWorkspace(ws);
                        if (proj == QStringLiteral("No project") && !title.isEmpty() && title != QStringLiteral("No project"))
                            proj = title;
                        const bool isSubagent = (relationship == QLatin1String("subagent"));
                        map.insert(convId, {proj, isSubagent});
                    }
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return map;
}

ParseResult T3Parser::parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const {
    ParseResult result;
    if (rootPath.isEmpty())
        return result;

    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists())
        return result;

    QString dbPath;
    if (rootInfo.isDir()) {
        dbPath = QDir(rootPath).filePath(QStringLiteral("statev2.sqlite"));
        if (!QFileInfo::exists(dbPath)) {
            QDir parentDir = rootInfo.dir();
            if (QFileInfo::exists(parentDir.filePath(QStringLiteral("statev2.sqlite"))))
                dbPath = parentDir.filePath(QStringLiteral("statev2.sqlite"));
        }
    } else if (rootInfo.fileName() == QLatin1String("statev2.sqlite")) {
        dbPath = rootInfo.absoluteFilePath();
    }

    const QMap<QString, T3ThreadInfo> threadMap = readThreadMap(dbPath);

    QFileInfoList files;
    if (rootInfo.isFile() && rootInfo.fileName() == QLatin1String("transcript.jsonl")) {
        files.append(rootInfo);
    } else if (rootInfo.isDir()) {
        QDirIterator it(rootPath, QStringList() << QStringLiteral("transcript.jsonl"),
                        QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            files.append(it.fileInfo());
        }
    }

    for (const QFileInfo &fileInfo : files) {
        const QString filePath = fileInfo.absoluteFilePath();
        const qint64 mtime = fileInfo.lastModified().toMSecsSinceEpoch();
        const qint64 size = fileInfo.size();

        if (shouldSkip && shouldSkip(filePath, mtime, size))
            continue;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QDir dir = fileInfo.dir();
        if (dir.dirName() == QLatin1String("logs"))
            dir.cdUp();
        if (dir.dirName() == QLatin1String(".system_generated"))
            dir.cdUp();
        QString convId = dir.dirName();
        if (convId.isEmpty() || convId == QLatin1String("brain"))
            convId = fileInfo.baseName();

        const T3ThreadInfo threadInfo = threadMap.value(convId, {QStringLiteral("No project"), false});

        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            if (line.isEmpty())
                continue;

            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject())
                continue;

            const QJsonObject rootObj = doc.object();
            if (rootObj.value("type").toString() != QLatin1String("PLANNER_RESPONSE"))
                continue;
            if (rootObj.value("status").toString() != QLatin1String("DONE"))
                continue;

            NormalizedEvent event;
            event.source = QStringLiteral("t3");
            event.sessionId = convId;
            const qint64 stepIndex = static_cast<qint64>(rootObj.value("step_index").toDouble());
            event.messageId = QStringLiteral("%1:%2").arg(convId).arg(stepIndex);
            event.timestampMs = timestampMs(rootObj.value("created_at"), fileInfo);
            event.project = threadInfo.project;
            event.agent = threadInfo.isSubagent ? QStringLiteral("subagent") : QStringLiteral("antigravity");
            event.model = rootObj.value("model").toString();
            if (event.model.isEmpty())
                event.model = QStringLiteral("gemini");
            event.provider = QStringLiteral("antigravity");

            const qint64 inTokens = static_cast<qint64>(rootObj.value("input_tokens").toDouble());
            const qint64 cacheTokens = static_cast<qint64>(rootObj.value("cache_read_tokens").toDouble());
            event.tokensIn = inTokens + cacheTokens;
            event.tokensOut = static_cast<qint64>(rootObj.value("output_tokens").toDouble());
            event.tokensReasoning = 0;
            event.tokensTotal = event.tokensIn + event.tokensOut;
            event.costUsd = 0.0;
            event.isSubagent = threadInfo.isSubagent;

            result.events.append(event);
        }

        result.files.append({filePath, mtime, size});
    }

    return result;
}
