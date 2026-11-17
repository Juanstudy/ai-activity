#include "claudeparser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

QString ClaudeParser::source() const {
    return QStringLiteral("claude");
}

qint64 ClaudeParser::timestampMs(const QJsonValue &value, const QFileInfo &fileInfo) {
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

QString ClaudeParser::projectFromCwd(const QString &cwd, const QString &slug) {
    if (cwd.isEmpty())
        return slug;
    const QString home = QDir::homePath();
    if (!cwd.startsWith(home))
        return slug;
    QString relative = cwd.mid(home.size());
    while (relative.startsWith('/'))
        relative.remove(0, 1);
    return relative.isEmpty() ? slug : relative;
}

ParseResult ClaudeParser::parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const {
    ParseResult result;
    QDir rootDir(rootPath);
    if (!rootDir.exists())
        return result;

    const QFileInfoList projectDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &projectDirInfo : projectDirs) {
        const QString slug = projectDirInfo.fileName();
        QFileInfoList files;
        QDir projectDir(projectDirInfo.absoluteFilePath());
        files.append(projectDir.entryInfoList({"*.jsonl"}, QDir::Files));

        // Subagent transcripts live one level below the project slug.
        QDir subagentDir(projectDir.path() + "/subagents");
        if (subagentDir.exists())
            files.append(subagentDir.entryInfoList({"*.jsonl"}, QDir::Files));

        for (const QFileInfo &fileInfo : files) {
            const QString filePath = fileInfo.absoluteFilePath();
            const qint64 mtime = fileInfo.lastModified().toMSecsSinceEpoch();
            const qint64 size = fileInfo.size();
            if (shouldSkip && shouldSkip(filePath, mtime, size))
                continue;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            // Pass 1: collect the file's assistant events and its session cost.
            QList<NormalizedEvent> events;
            QSet<QString> seenIds;
            double sessionCostUsd = 0.0;

            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                if (line.isEmpty())
                    continue;

                const QJsonDocument doc = QJsonDocument::fromJson(line);
                if (!doc.isObject())
                    continue;
                const QJsonObject rootObj = doc.object();
                const QString type = rootObj.value("type").toString();

                if (type == "cost-state") {
                    const QJsonValue costValue = rootObj.value("totalCostUSD");
                    if (costValue.isDouble())
                        sessionCostUsd = costValue.toDouble();
                    continue;
                }
                if (type != "assistant")
                    continue;

                const QJsonObject msgObj = rootObj.value("message").toObject();
                const QJsonObject usageObj = msgObj.value("usage").toObject();
                if (usageObj.isEmpty())
                    continue;

                // Claude repeats message.id on several lines; uuid is the real
                // record identity and must drive dedupe (and the token sum).
                QString messageId = rootObj.value("uuid").toString();
                if (messageId.isEmpty())
                    messageId = msgObj.value("id").toString();
                if (messageId.isEmpty() || seenIds.contains(messageId))
                    continue;
                seenIds.insert(messageId);

                const qint64 inputTokens = static_cast<qint64>(usageObj.value("input_tokens").toDouble());
                const qint64 outputTokens = static_cast<qint64>(usageObj.value("output_tokens").toDouble());
                const qint64 cacheWrite = static_cast<qint64>(usageObj.value("cache_creation_input_tokens").toDouble());
                const qint64 cacheRead = static_cast<qint64>(usageObj.value("cache_read_input_tokens").toDouble());
                const QJsonObject details = usageObj.value("output_tokens_details").toObject();
                const qint64 thinking = static_cast<qint64>(details.value("thinking_tokens").toDouble());

                const bool isSubagent = rootObj.value("isSidechain").toBool()
                                        || !rootObj.value("agentId").toString().isEmpty();

                NormalizedEvent event;
                event.source = source();
                event.sessionId = rootObj.value("sessionId").toString();
                event.messageId = messageId;
                event.timestampMs = timestampMs(rootObj.value("timestamp"), fileInfo);
                event.project = projectFromCwd(rootObj.value("cwd").toString(), slug);
                event.agent = isSubagent ? QStringLiteral("subagent") : QStringLiteral("claude");
                event.model = msgObj.value("model").toString();
                if (event.model.isEmpty())
                    event.model = QStringLiteral("unknown");
                // Cache reads/writes are input-side; thinking is reported apart
                // from output and included in the normalized total.
                event.tokensIn = inputTokens + cacheWrite + cacheRead;
                event.tokensOut = outputTokens;
                event.tokensReasoning = thinking;
                event.tokensTotal = event.tokensIn + event.tokensOut + event.tokensReasoning;
                event.isSubagent = isSubagent;

                events.append(event);
            }

            // Pass 2: prorate the session-level total by each event's token
            // share. These are estimates, not actual per-message charges.
            if (sessionCostUsd > 0.0) {
                qint64 sessionTokens = 0;
                for (const NormalizedEvent &event : events)
                    sessionTokens += event.tokensTotal;
                if (sessionTokens > 0) {
                    for (NormalizedEvent &event : events) {
                        event.costUsd = sessionCostUsd
                                        * (static_cast<double>(event.tokensTotal)
                                           / static_cast<double>(sessionTokens));
                    }
                }
            }

            result.events.append(events);
            result.files.append({filePath, mtime, size});
        }
    }
    return result;
}
