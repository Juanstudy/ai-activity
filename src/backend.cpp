#include "backend.h"

#include <QDateTime>
#include <QDebug>

#include "parsers/sourceregistry.h"

Backend::Backend(const QString &dbPath, const QString &customSessionsDir, QObject *parent,
                 bool autoSyncTrm, SourceRegistry *registry)
    : QObject(parent),
      m_sessionsDir(customSessionsDir),
      m_storage(dbPath, this),
      m_trm(dbPath.isEmpty() ? QString() : dbPath + ".trm.json", this, autoSyncTrm) {

    connect(&m_trm, &TrmClient::rateChanged, this, &Backend::dataChanged);
    connect(&m_trm, &TrmClient::syncingChanged, this, &Backend::dataChanged);

    if (registry) {
        // Caller-supplied registry wins: tests can inject a fully controlled set.
        m_registry = registry;
    } else {
        m_ownedRegistry = std::make_unique<SourceRegistry>(&m_storage);
        m_registry = m_ownedRegistry.get();
        if (!customSessionsDir.isEmpty()) {
            // A custom sessions dir marks a hermetic caller. Restrict sync to Pi
            // rooted there so tests never read the real ~/.omp or ~/.claude.
            m_registry->setEnabled("omp", false);
            m_registry->setEnabled("claude", false);
            m_registry->setRootPath("pi", customSessionsDir);
        }
    }

    if (m_storage.init()) {
        refresh();
    } else {
        m_lastSyncStatus = "Error initializing database";
    }
}

Backend::~Backend() = default;

void Backend::setRange(const QString &range) {
    if (m_currentRange == range)
        return;

    m_currentRange = range;
    recalculate();
    emit dataChanged();
}

double Backend::totalCostCop() const {
    return m_trm.convertUsdToCop(m_summary.totalCostUsd);
}

double Backend::avgCostPerTurnUsd() const {
    if (m_summary.totalTurns <= 0)
        return 0.0;
    return m_summary.totalCostUsd / m_summary.totalTurns;
}

double Backend::avgCostPerTurnCop() const {
    if (m_summary.totalTurns <= 0)
        return 0.0;
    return totalCostCop() / m_summary.totalTurns;
}

void Backend::computeTimeRange(qint64 &outStartMs, qint64 &outEndMs) const {
    const QDateTime now = QDateTime::currentDateTime();
    outEndMs = now.toMSecsSinceEpoch();

    if (m_currentRange == "5h") {
        outStartMs = now.addSecs(-5 * 3600).toMSecsSinceEpoch();
    } else if (m_currentRange == "12h") {
        outStartMs = now.addSecs(-12 * 3600).toMSecsSinceEpoch();
    } else if (m_currentRange == "24h") {
        outStartMs = now.addDays(-1).toMSecsSinceEpoch();
    } else if (m_currentRange == "7d") {
        outStartMs = now.addDays(-7).toMSecsSinceEpoch();
    } else if (m_currentRange == "30d") {
        outStartMs = now.addDays(-30).toMSecsSinceEpoch();
    } else if (m_currentRange == "6m") {
        outStartMs = now.addMonths(-6).toMSecsSinceEpoch();
    } else if (m_currentRange == "1y") {
        outStartMs = now.addYears(-1).toMSecsSinceEpoch();
    } else { // "all"
        outStartMs = 0;
    }
}

void Backend::recalculate() {
    qint64 startMs = 0;
    qint64 endMs = 0;
    computeTimeRange(startMs, endMs);

    m_summary = m_storage.getSummary(startMs, endMs);
    m_projectBreakdown = m_storage.getBreakdown("project", startMs, endMs);
    m_modelBreakdown = m_storage.getBreakdown("model", startMs, endMs);
    m_agentBreakdown = m_storage.getBreakdown("agent", startMs, endMs);

    const auto streak = m_storage.getStreak();
    m_currentStreak = streak.currentStreak;
    m_maxStreak = streak.maxStreak;

    // Heatmap: show at least the last 52 weeks or range depending on user selection
    const QDateTime now = QDateTime::currentDateTime();
    qint64 heatmapStart = now.addDays(-364).toMSecsSinceEpoch();
    if (m_currentRange == "all" || m_currentRange == "1y") {
        heatmapStart = (m_currentRange == "all") ? 0 : now.addYears(-1).toMSecsSinceEpoch();
    }
    m_heatmapData = m_storage.getHeatmapDays(heatmapStart, now.toMSecsSinceEpoch());
}

void Backend::refresh() {
    m_isSyncing = true;
    emit dataChanged();

    const int newCount = m_registry->syncAll();
    m_lastSyncStatus = QString("Synced %1 events (%2)").arg(newCount).arg(QDateTime::currentDateTime().toString("hh:mm:ss"));

    recalculate();

    m_isSyncing = false;
    emit dataChanged();
}

void Backend::syncTrm() {
    m_trm.syncRate();
}

QString Backend::formatNumber(qint64 count) const {
    return QLocale().toString(count);
}

QString Backend::formatCompact(qint64 count) const {
    const double val = static_cast<double>(count);
    if (std::abs(val) >= 1000000000.0) {
        return QString::number(val / 1000000000.0, 'f', 2) + "B";
    }
    if (std::abs(val) >= 1000000.0) {
        return QString::number(val / 1000000.0, 'f', 2) + "M";
    }
    if (std::abs(val) >= 1000.0) {
        return QString::number(val / 1000.0, 'f', 1) + "k";
    }
    return QString::number(count);
}

QString Backend::formatTokens(qint64 count) const {
    if (std::abs(count) >= 1000000) {
        return formatCompact(count) + " (" + formatNumber(count) + ")";
    }
    return formatNumber(count);
}
