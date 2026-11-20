#pragma once

#include <QJsonObject>

#include "usageparser.h"

// Shared walk for the two Pi-shaped JSONL layouts (Pi and omp):
// <root>/<session-dir>/*.jsonl. It owns the directory traversal, incremental
// skipping, timestamps, project/subagent heuristics and usage extraction.
// Subclasses only rename a few fields and may post-process parsed events.
class PiShapedParser : public UsageParser {
public:
    ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const override;

protected:
    virtual QString reasoningKey() const;
    virtual QString agentLabel() const;
    virtual void applySourceFields(const QJsonObject &rootObj, const QJsonObject &msgObj,
                                   NormalizedEvent &event) const;
};

// Legacy Pi layout: reasoning lives at "reasoning" and the agent label is
// "pi"/"subagent". Kept as a parser so every source flows through one registry.
class PiParser final : public PiShapedParser {
public:
    QString source() const override;
};

// omp layout: same shape, but reasoning is "reasoningTokens" and the assistant
// message carries api/modelID/providerID at root level.
class OmpParser final : public PiShapedParser {
public:
    QString source() const override;

protected:
    QString reasoningKey() const override;
    QString agentLabel() const override;
    void applySourceFields(const QJsonObject &rootObj, const QJsonObject &msgObj,
                           NormalizedEvent &event) const override;
};
