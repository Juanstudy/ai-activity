#pragma once

#include <QList>
#include <QString>

#include <memory>

#include "usageparser.h"

class Storage;

// One configurable slot per source. Disabled slots or slots without a parser
// (codex/opencode are not implemented yet) are never read during a sync.
struct SourceEntry {
    QString source;
    QString rootPath;
    bool enabled = false;
    std::shared_ptr<UsageParser> parser;
};

// Owns the parser set and drives incremental syncs through Storage. Every flag,
// root path and parser is replaceable so tests stay hermetic.
class SourceRegistry {
public:
    explicit SourceRegistry(Storage *storage);

    void setEnabled(const QString &source, bool enabled);
    void setRootPath(const QString &source, const QString &rootPath);
    void setParser(const QString &source, std::shared_ptr<UsageParser> parser);

    bool isEnabled(const QString &source) const;
    QString rootPath(const QString &source) const;

    // Syncs every enabled source and returns the number of inserted rows.
    int syncAll();

private:
    SourceEntry *entry(const QString &source);
    const SourceEntry *entry(const QString &source) const;

    Storage *m_storage;
    QList<SourceEntry> m_entries;
};
