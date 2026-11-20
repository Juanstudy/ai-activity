#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <functional>

// One assistant turn normalized across every CLI. Column names mirror the
// events table so a parser's output can be inserted without a translation layer.
struct NormalizedEvent {
    QString source;
    QString sessionId;
    QString messageId;
    qint64 timestampMs = 0;
    QString project;
    QString agent;
    QString model;
    QString provider;
    qint64 tokensIn = 0;
    qint64 tokensOut = 0;
    qint64 tokensReasoning = 0;
    qint64 tokensTotal = 0;
    double costUsd = 0.0;
    bool isSubagent = false;
};

// Stamp of a file a parser actually consumed. Storage records these after a
// successful insert so unchanged files can be skipped on the next sync.
struct FileStamp {
    QString filePath;
    qint64 mtime = 0;
    qint64 size = 0;
};

struct ParseResult {
    QList<NormalizedEvent> events;
    QList<FileStamp> files;
};

// Return true to skip a file whose stored stamp still matches. Storage builds
// this predicate from its sync_state table; tests may supply their own.
using FileSkipPredicate = std::function<bool(const QString &filePath, qint64 mtime, qint64 size)>;

// Common contract for every source parser. Implementations only read the
// configured root path; path discovery and sync decisions belong to callers.
class UsageParser {
public:
    virtual ~UsageParser() = default;

    virtual QString source() const = 0;
    virtual ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const = 0;
};
