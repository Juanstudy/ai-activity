#pragma once

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

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

class Storage : public QObject {
    Q_OBJECT

public:
    explicit Storage(const QString &dbPath = {}, QObject *parent = nullptr);
    ~Storage() override;

    bool init();
    int syncFromPiSessions(const QString &customSessionsDir = {});

    UsageSummary getSummary(qint64 startTimeMs, qint64 endTimeMs) const;
    StreakInfo getStreak(const QDate &referenceDate = QDate::currentDate()) const;
    QVariantList getHeatmapDays(qint64 startTimeMs, qint64 endTimeMs) const;
    QVariantList getBreakdown(const QString &dimension, qint64 startTimeMs, qint64 endTimeMs) const;

    QString defaultSessionsDir() const;
    QString connectionName() const { return m_connectionName; }
    static QString extractProjectFromDirName(const QString &dirName);

private:
    QString m_connectionName;
    QString m_dbPath;
    QSqlDatabase m_db;
};
