#pragma once

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "parsers/usageparser.h"

class QSqlQuery;

struct UsageSummary {
    double totalCostUsd = 0.0;
    qint64 totalTokens = 0;
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    qint64 thinkingTokens = 0;
    int totalTurns = 0;
    int totalSessions = 0;
};

struct StreakInfo {
    int currentStreak = 0;
    int maxStreak = 0;
};

// Incremental-sync bookmark for one file under one source. `found` is false when
// no row is recorded, which lets parsers decide to parse a file from scratch.
struct FileSyncState {
    bool found = false;
    qint64 mtime = 0;
    qint64 size = 0;
};

class Storage : public QObject {
    Q_OBJECT

public:
    explicit Storage(const QString &dbPath = {}, QObject *parent = nullptr);
    ~Storage() override;

    bool init();
    int syncFromPiSessions(const QString &customSessionsDir = {});

    // Parse one source through its parser, skip files whose stamp is unchanged,
    // and record fresh stamps only after a successful insert. Returns inserted rows.
    int syncSource(const QString &source, const UsageParser &parser, const QString &rootPath);

    UsageSummary getSummary(qint64 startTimeMs, qint64 endTimeMs) const;
    StreakInfo getStreak(const QDate &referenceDate = QDate::currentDate()) const;
    QVariantList getHeatmapDays(qint64 startTimeMs, qint64 endTimeMs) const;
    QVariantList getBreakdown(const QString &dimension, qint64 startTimeMs, qint64 endTimeMs) const;

    QString defaultSessionsDir() const;
    QString connectionName() const { return m_connectionName; }
    static QString extractProjectFromDirName(const QString &dirName);

private:
    bool migrateEventsToSourceSchema(QSqlQuery *query);
    bool eventsHaveColumn(QSqlQuery *query, const QString &table, const QString &column) const;
    bool eventsHaveCompositeUnique(QSqlQuery *query) const;

    FileSyncState fileSyncState(const QString &source, const QString &filePath) const;
    bool setFileSyncState(const QString &source, const QString &filePath, qint64 mtime, qint64 size);
    int insertEvents(const QList<NormalizedEvent> &events);

    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};
