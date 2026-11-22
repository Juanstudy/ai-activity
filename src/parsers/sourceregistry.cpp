#include "sourceregistry.h"

#include <QDir>
#include <utility>

#include "claudeparser.h"
#include "opencodeparser.h"
#include "pishapedparser.h"
#include "t3parser.h"
#include "storage.h"

SourceRegistry::SourceRegistry(Storage *storage) : m_storage(storage) {
    const QString home = QDir::homePath();
    m_entries = {
        {QStringLiteral("pi"), storage->defaultSessionsDir(), true, std::make_shared<PiParser>()},
        {QStringLiteral("omp"), home + "/.omp/agent/sessions", true, std::make_shared<OmpParser>()},
        {QStringLiteral("claude"), home + "/.claude/projects", true, std::make_shared<ClaudeParser>()},
        {QStringLiteral("opencode"), home + "/.local/share/opencode/opencode.db", true, std::make_shared<OpenCodeParser>()},
        {QStringLiteral("t3"), home + "/.t3/userdata", true, std::make_shared<T3Parser>()},
        // Codex parser is not implemented yet; it stays off and is never read.
        {QStringLiteral("codex"), home + "/.codex/sessions", false, nullptr},
    };
}

SourceEntry *SourceRegistry::entry(const QString &source) {
    for (SourceEntry &candidate : m_entries) {
        if (candidate.source == source)
            return &candidate;
    }
    return nullptr;
}

const SourceEntry *SourceRegistry::entry(const QString &source) const {
    for (const SourceEntry &candidate : m_entries) {
        if (candidate.source == source)
            return &candidate;
    }
    return nullptr;
}

void SourceRegistry::setEnabled(const QString &source, bool enabled) {
    if (SourceEntry *slot = entry(source))
        slot->enabled = enabled;
}

void SourceRegistry::setRootPath(const QString &source, const QString &rootPath) {
    if (SourceEntry *slot = entry(source))
        slot->rootPath = rootPath;
}

void SourceRegistry::setParser(const QString &source, std::shared_ptr<UsageParser> parser) {
    if (SourceEntry *slot = entry(source))
        slot->parser = std::move(parser);
}

bool SourceRegistry::isEnabled(const QString &source) const {
    const SourceEntry *slot = entry(source);
    return slot && slot->enabled;
}

QString SourceRegistry::rootPath(const QString &source) const {
    const SourceEntry *slot = entry(source);
    return slot ? slot->rootPath : QString();
}

int SourceRegistry::syncAll() {
    int insertedTotal = 0;
    for (const SourceEntry &slot : m_entries) {
        if (!slot.enabled || !slot.parser)
            continue;
        insertedTotal += m_storage->syncSource(slot.source, *slot.parser, slot.rootPath);
    }
    return insertedTotal;
}
