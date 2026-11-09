#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

class TrmClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY syncingChanged)
    Q_PROPERTY(QString lastSyncTime READ lastSyncTime NOTIFY rateChanged)

public:
    explicit TrmClient(const QString &cacheFilePath = {}, QObject *parent = nullptr);
    ~TrmClient() override = default;

    double rate() const { return m_rate; }
    bool isSyncing() const { return m_isSyncing; }
    QString lastSyncTime() const;

    Q_INVOKABLE void syncRate();
    double convertUsdToCop(double usd) const { return usd * m_rate; }

signals:
    void rateChanged(double newRate);
    void syncingChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    void loadCache();
    void saveCache(double rate, const QDateTime &timestamp);

    QString m_cacheFile;
    double m_rate = 4100.0;
    QDateTime m_lastSync;
    bool m_isSyncing = false;
    QNetworkAccessManager *m_nam = nullptr;
};
