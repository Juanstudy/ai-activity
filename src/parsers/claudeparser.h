#pragma once

#include <QFileInfo>
#include <QJsonValue>

#include "usageparser.h"

// Reads ~/.claude/projects/<slug>/*.jsonl plus nested subagents/*.jsonl.
// Claude stores per-message cost as null, so each session file is parsed twice:
// once to collect assistant events and its cost-state total, then to prorate
// that total across the events by token share.
class ClaudeParser final : public UsageParser {
public:
    QString source() const override;
    ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const override;

private:
    static qint64 timestampMs(const QJsonValue &value, const QFileInfo &fileInfo);
    static QString projectFromCwd(const QString &cwd, const QString &slug);
};
