#pragma once

#include <QDateTime>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "storage.h"
#include "trmclient.h"

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentRange READ currentRange WRITE setRange NOTIFY dataChanged)
    Q_PROPERTY(double totalCostUsd READ totalCostUsd NOTIFY dataChanged)
    Q_PROPERTY(double totalCostCop READ totalCostCop NOTIFY dataChanged)
    Q_PROPERTY(double avgCostPerTurnUsd READ avgCostPerTurnUsd NOTIFY dataChanged)
    Q_PROPERTY(double avgCostPerTurnCop READ avgCostPerTurnCop NOTIFY dataChanged)
    Q_PROPERTY(qint64 totalTokens READ totalTokens NOTIFY dataChanged)
    Q_PROPERTY(qint64 inputTokens READ inputTokens NOTIFY dataChanged)
    Q_PROPERTY(qint64 outputTokens READ outputTokens NOTIFY dataChanged)
    Q_PROPERTY(qint64 thinkingTokens READ thinkingTokens NOTIFY dataChanged)
    Q_PROPERTY(int totalTurns READ totalTurns NOTIFY dataChanged)
    Q_PROPERTY(int totalSessions READ totalSessions NOTIFY dataChanged)
    Q_PROPERTY(double trmRate READ trmRate NOTIFY dataChanged)
    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY dataChanged)
    Q_PROPERTY(QString lastSyncStatus READ lastSyncStatus NOTIFY dataChanged)

    Q_PROPERTY(QVariantList heatmapData READ heatmapData NOTIFY dataChanged)
    Q_PROPERTY(QVariantList projectBreakdown READ projectBreakdown NOTIFY dataChanged)
    Q_PROPERTY(QVariantList modelBreakdown READ modelBreakdown NOTIFY dataChanged)
    Q_PROPERTY(QVariantList agentBreakdown READ agentBreakdown NOTIFY dataChanged)

public:
    explicit Backend(const QString &dbPath = {}, const QString &customSessionsDir = {}, QObject *parent = nullptr);
    ~Backend() override = default;

    QString currentRange() const { return m_currentRange; }
    void setRange(const QString &range);

    double totalCostUsd() const { return m_summary.totalCostUsd; }
    double totalCostCop() const;
    double avgCostPerTurnUsd() const;
    double avgCostPerTurnCop() const;

    qint64 totalTokens() const { return m_summary.totalTokens; }
    qint64 inputTokens() const { return m_summary.inputTokens; }
    qint64 outputTokens() const { return m_summary.outputTokens; }
    qint64 thinkingTokens() const { return m_summary.thinkingTokens; }
    int totalTurns() const { return m_summary.totalTurns; }
    int totalSessions() const { return m_summary.totalSessions; }

    double trmRate() const { return m_trm.rate(); }
    bool isSyncing() const { return m_isSyncing || m_trm.isSyncing(); }
    QString lastSyncStatus() const { return m_lastSyncStatus; }

    QVariantList heatmapData() const { return m_heatmapData; }
    QVariantList projectBreakdown() const { return m_projectBreakdown; }
    QVariantList modelBreakdown() const { return m_modelBreakdown; }
    QVariantList agentBreakdown() const { return m_agentBreakdown; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void syncTrm();

signals:
    void dataChanged();

private:
    void computeTimeRange(qint64 &outStartMs, qint64 &outEndMs) const;
    void recalculate();

    QString m_sessionsDir;
    Storage m_storage;
    TrmClient m_trm;

    QString m_currentRange = "7d";
    UsageSummary m_summary;
    QVariantList m_heatmapData;
    QVariantList m_projectBreakdown;
    QVariantList m_modelBreakdown;
    QVariantList m_agentBreakdown;

    bool m_isSyncing = false;
    QString m_lastSyncStatus;
};
