#include "pishapedparser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "storage.h"

QString PiShapedParser::reasoningKey() const {
    return QStringLiteral("reasoning");
}

QString PiShapedParser::agentLabel() const {
    return QStringLiteral("pi");
}

void PiShapedParser::applySourceFields(const QJsonObject &rootObj, const QJsonObject &msgObj,
                                       NormalizedEvent &event) const {
    Q_UNUSED(rootObj);
    Q_UNUSED(msgObj);
    Q_UNUSED(event);
}

ParseResult PiShapedParser::parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const {
    ParseResult result;
    QDir rootDir(rootPath);
    if (!rootDir.exists())
        return result;

    const QString sourceName = source();
    const QString agent = agentLabel();
    const QFileInfoList sessionDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &dirInfo : sessionDirs) {
        const QString dirName = dirInfo.fileName();
        const QString project = Storage::extractProjectFromDirName(dirName);
        // Same heuristic the legacy Pi importer uses to flag subagents.
        const bool isSubagent = dirName.contains("Agent") || dirName.contains("gentle-agents");

        QDir sessionDir(dirInfo.absoluteFilePath());
        const QFileInfoList files = sessionDir.entryInfoList({"*.jsonl"}, QDir::Files);
        for (const QFileInfo &fileInfo : files) {
            const QString filePath = fileInfo.absoluteFilePath();
            const qint64 mtime = fileInfo.lastModified().toMSecsSinceEpoch();
            const qint64 size = fileInfo.size();
            if (shouldSkip && shouldSkip(filePath, mtime, size))
                continue;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                if (line.isEmpty() || !line.contains("\"usage\""))
                    continue;

                const QJsonDocument doc = QJsonDocument::fromJson(line);
                if (!doc.isObject())
                    continue;
                const QJsonObject rootObj = doc.object();
                if (rootObj.value("type").toString() != "message")
                    continue;

                const QJsonObject msgObj = rootObj.value("message").toObject();
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

                // Numeric milliseconds, ISO-8601 (with or without ms), else the
                // file's own mtime. This matches the legacy Pi importer exactly.
                qint64 timestampMs = 0;
                if (rootObj.contains("timestamp")) {
                    const QJsonValue tsValue = rootObj.value("timestamp");
                    if (tsValue.isDouble()) {
                        timestampMs = static_cast<qint64>(tsValue.toDouble());
                    } else if (tsValue.isString()) {
                        timestampMs =
                            QDateTime::fromString(tsValue.toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
                        if (timestampMs <= 0)
                            timestampMs = QDateTime::fromString(tsValue.toString(), Qt::ISODate).toMSecsSinceEpoch();
                    }
                }
                if (timestampMs <= 0)
                    timestampMs = fileInfo.lastModified().toMSecsSinceEpoch();

                NormalizedEvent event;
                event.source = sourceName;
                event.sessionId = fileInfo.baseName();
                event.messageId = msgId;
                event.timestampMs = timestampMs;
                event.project = project;
                event.agent = agent;
                event.model = model.isEmpty() ? QStringLiteral("unknown") : model;
                event.provider = provider;
                event.tokensIn = static_cast<qint64>(usageObj.value("input").toDouble());
                event.tokensOut = static_cast<qint64>(usageObj.value("output").toDouble());
                event.tokensReasoning = static_cast<qint64>(usageObj.value(reasoningKey()).toDouble());
                const qint64 reportedTotal = static_cast<qint64>(usageObj.value("totalTokens").toDouble());
                event.tokensTotal = reportedTotal > 0 ? reportedTotal : (event.tokensIn + event.tokensOut);
                const QJsonObject costObj = usageObj.value("cost").toObject();
                if (!costObj.isEmpty())
                    event.costUsd = costObj.value("total").toDouble();
                event.isSubagent = isSubagent;

                applySourceFields(rootObj, msgObj, event);
                result.events.append(event);
            }

            // A stamp is returned for every file we consumed, even when no
            // assistant line was found, so empty files are not retried forever.
            result.files.append({filePath, mtime, size});
        }
    }
    return result;
}

QString PiParser::source() const {
    return QStringLiteral("pi");
}

QString OmpParser::source() const {
    return QStringLiteral("omp");
}

QString OmpParser::reasoningKey() const {
    return QStringLiteral("reasoningTokens");
}

QString OmpParser::agentLabel() const {
    return QStringLiteral("omp");
}

void OmpParser::applySourceFields(const QJsonObject &rootObj, const QJsonObject &msgObj,
                                  NormalizedEvent &event) const {
    Q_UNUSED(msgObj);
    // omp records the concrete model/provider as modelID/providerID; `api` is a
    // coarse label and is intentionally not used as the model column.
    const QString modelId = rootObj.value("modelID").toString();
    if (!modelId.isEmpty())
        event.model = modelId;
    const QString providerId = rootObj.value("providerID").toString();
    if (!providerId.isEmpty())
        event.provider = providerId;
}
