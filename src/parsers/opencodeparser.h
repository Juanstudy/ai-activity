#pragma once

#include <QFileInfo>
#include <QString>

#include "usageparser.h"

// Reads OpenCode's SQLite database (~/.local/share/opencode/opencode.db).
// Connects with mode=ro to safely read alongside an active WAL journal.
// Assistant messages hold per-message token counts, model and provider;
// session costs are prorated by token share if per-message costs are zero.
class OpenCodeParser final : public UsageParser {
public:
    QString source() const override;
    ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const override;

    static QString extractProject(const QString &sessionPath, const QString &sessionDir,
                                  const QString &cwd, const QString &slug);
};
