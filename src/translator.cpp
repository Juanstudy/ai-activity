#include "translator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

Translator::Translator(const QString &languageFile, const QString &catalogDirectory, QObject *parent)
    : QTranslator(parent),
      m_languageFile(languageFile.isEmpty() ? QDir::homePath() + "/.config/omarchy/language" : languageFile),
      m_catalogDirectory(catalogDirectory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    reload();
}

QString Translator::readLanguage(const QString &path) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const auto trimmed = QString::fromUtf8(file.readAll()).trimmed();
        if (!trimmed.isEmpty())
            return trimmed;
    }
    return "en";
}

QLocale Translator::formatLocale(const QString &language) {
    if (language.isEmpty())
        return QLocale::c();
    return QLocale(language);
}

QString Translator::pluralCategory(const QString &language, long long count) {
    Q_UNUSED(language);
    return count == 1 ? "one" : "other";
}

QString Translator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const {
    Q_UNUSED(context);
    Q_UNUSED(disambiguation);
    if (!sourceText)
        return {};

    const auto key = QString::fromUtf8(sourceText);
    const auto it = m_strings.constFind(key);
    if (it == m_strings.constEnd())
        return key;

    if (it->isObject() && n >= 0) {
        const auto obj = it->toObject();
        const auto category = pluralCategory(m_language, n);
        if (obj.contains(category))
            return obj.value(category).toString();
        if (obj.contains("other"))
            return obj.value("other").toString();
    }
    if (it->isString())
        return it->toString();

    return key;
}

void Translator::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    QString ancestor = m_languageFile;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_languageFile};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const auto lang = readLanguage(m_languageFile);
    if (lang != m_language) {
        m_language = lang;
        m_strings.clear();

        QFile catalog(m_catalogDirectory + "/" + lang + ".json");
        if (catalog.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(catalog.readAll());
            if (doc.isObject()) {
                const auto obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it)
                    m_strings.insert(it.key(), it.value());
            }
        }
        QLocale::setDefault(formatLocale(m_language));
        emit changed();
    }
}
