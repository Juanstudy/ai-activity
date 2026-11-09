#include "trmclient.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>

TrmClient::TrmClient(const QString &cacheFilePath, QObject *parent)
    : QObject(parent),
      m_cacheFile(cacheFilePath) {
    if (m_cacheFile.isEmpty()) {
        const QString dataDir = QDir::homePath() + "/.local/share/ai-activity";
        QDir().mkpath(dataDir);
        m_cacheFile = dataDir + "/trm.json";
    }

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, &TrmClient::onReplyFinished);

    loadCache();
    // If cache is empty or older than 12 hours, trigger sync
    if (m_lastSync.isNull() || m_lastSync.secsTo(QDateTime::currentDateTime()) > 12 * 3600) {
        syncRate();
    }
}

QString TrmClient::lastSyncTime() const {
    if (m_lastSync.isNull())
        return "Offline default";
    return m_lastSync.toString("yyyy-MM-dd hh:mm");
}

void TrmClient::loadCache() {
    QFile file(m_cacheFile);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const auto obj = doc.object();
    const double cachedRate = obj.value("rate").toDouble();
    const QString tsStr = obj.value("timestamp").toString();

    if (cachedRate > 1000.0) {
        m_rate = cachedRate;
        m_lastSync = QDateTime::fromString(tsStr, Qt::ISODate);
        emit rateChanged(m_rate);
    }
}

void TrmClient::saveCache(double rate, const QDateTime &timestamp) {
    QFile file(m_cacheFile);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonObject obj;
    obj["rate"] = rate;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TrmClient::syncRate() {
    if (m_isSyncing)
        return;

    m_isSyncing = true;
    emit syncingChanged();

    // Standard open exchange rate API with no authentication required
    const QUrl url("https://open.er-api.com/v6/latest/USD");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ai-activity-omarchy/1.0");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    m_nam->get(request);
}

void TrmClient::onReplyFinished(QNetworkReply *reply) {
    m_isSyncing = false;
    emit syncingChanged();

    if (reply->error() == QNetworkReply::NoError) {
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            const auto rates = doc.object().value("rates").toObject();
            const double copRate = rates.value("COP").toDouble();
            if (copRate > 1000.0) {
                m_rate = copRate;
                m_lastSync = QDateTime::currentDateTime();
                saveCache(m_rate, m_lastSync);
                emit rateChanged(m_rate);
            }
        }
    } else {
        qWarning() << "TRM sync network error:" << reply->errorString();
    }
    reply->deleteLater();
}
