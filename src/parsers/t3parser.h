#pragma once

#include "usageparser.h"
#include <QMap>
#include <QString>

class QFileInfo;
class QJsonValue;

struct T3ThreadInfo {
    QString project;
    bool isSubagent = false;
};

class T3Parser : public UsageParser {
public:
    QString source() const override;
    ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const override;

    // Helpers exposed for unit testing
    static QString projectFromWorkspace(const QString &workspaceRoot);
    static qint64 timestampMs(const QJsonValue &value, const QFileInfo &fileInfo);
    static QMap<QString, T3ThreadInfo> readThreadMap(const QString &dbPath);
};
