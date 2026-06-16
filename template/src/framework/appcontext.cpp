#include "appcontext.h"
#include "thememanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QResource>
#include <QApplication>
#include <QDebug>

AppContext* AppContext::m_instance = nullptr;

AppContext* AppContext::instance() {
    if (!m_instance)
        m_instance = new AppContext();
    return m_instance;
}

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , m_currentTheme("Dark")
    , m_currentLanguage("English")
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString iniPath = appData + "/settings.ini";
    m_settings = std::make_unique<QSettings>(iniPath, QSettings::IniFormat);

    // Restore saved preferences
    QString savedTheme = m_settings->value("Preferences/Theme", "Dark").toString();
    if (m_availableThemes.contains(savedTheme))
        m_currentTheme = savedTheme;
    else {
        scanAvailableThemes();
        if (m_availableThemes.contains(savedTheme))
            m_currentTheme = savedTheme;
    }

    m_currentLanguage = m_settings->value("Preferences/Language", "English").toString();

    // Available languages (English is always first, built-in)
    m_availableLanguages << "English" << "中文" << "日本語";
}

void AppContext::setSettingsPath(const QString& path) {
    m_settingsPath = path;
    m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
}

void AppContext::setCurrentRoomName(const QString& name) {
    if (m_currentRoomName != name) {
        m_currentRoomName = name;
        emit roomChanged(name);
    }
}

//-----------------------------------------------------------------------------

QStringList AppContext::availableThemes() const {
    if (m_availableThemes.isEmpty())
        const_cast<AppContext*>(this)->scanAvailableThemes();
    return m_availableThemes;
}

void AppContext::scanAvailableThemes() {
    m_availableThemes.clear();
    // Themes are embedded as Qt resources under :/themes/
    QDir dir(":/themes");
    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QString qssPath = QString(":/themes/%1/%2.qss").arg(entry, entry);
        if (QFile::exists(qssPath))
            m_availableThemes << entry;
    }
}

void AppContext::setCurrentTheme(const QString& theme) {
    if (m_currentTheme != theme && m_availableThemes.contains(theme)) {
        m_currentTheme = theme;
        m_settings->setValue("Preferences/Theme", theme);
        applyTheme(theme);
        emit themeChanged(theme);
    }
}

void AppContext::setCurrentLanguage(const QString& language) {
    if (m_currentLanguage != language && m_availableLanguages.contains(language)) {
        m_currentLanguage = language;
        m_settings->setValue("Preferences/Language", language);
        // Language change requires restart
    }
}

QStringList AppContext::availableLanguages() const {
    return m_availableLanguages;
}

QString AppContext::loadStylesheet(const QString& themeName) const {
    QString qssPath = QString(":/themes/%1/%2.qss").arg(themeName, themeName);
    QFile file(qssPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot load stylesheet:" << qssPath;
        return QString();
    }
    QString content = file.readAll();
    file.close();

    // Fix relative url() paths to use resource paths
    // e.g., url("imgs/black/checkmark/check.svg") → url(":/icons/black/checkmark/check.svg")
    content.replace("url(\"imgs/black/", "url(\":/icons/black/");
    content.replace("url(\"imgs/white/", "url(\":/icons/white/");

    return content;
}

void AppContext::applyTheme(const QString& themeName) {
    QString stylesheet = loadStylesheet(themeName);
    if (!stylesheet.isEmpty()) {
        qApp->setStyleSheet(stylesheet);
        ThemeManager::getInstance().parseCustomPropertiesFromStylesheet(stylesheet);
    }
}
