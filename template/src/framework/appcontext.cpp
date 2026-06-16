#include "appcontext.h"
#include "thememanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QTranslator>
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
    scanAvailableThemes();
    QString savedTheme = m_settings->value("Preferences/Theme", "Dark").toString();
    m_currentTheme = m_availableThemes.contains(savedTheme) ? savedTheme : QString("Dark");

    m_currentLanguage = m_settings->value("Preferences/Language", "English").toString();

    // Available languages
    m_availableLanguages << "English" << "中文" << "日本語";

    // Load saved language translator at startup
    QString code = languageCode();
    if (!code.isEmpty()) loadTranslator(code);
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
// Theme
//-----------------------------------------------------------------------------

QStringList AppContext::availableThemes() const {
    return m_availableThemes;
}

void AppContext::scanAvailableThemes() {
    m_availableThemes.clear();
    QDir dir(":/themes");
    QStringList entries = dir.entryList({"*.qss"}, QDir::Files, QDir::Name);
    for (const QString& entry : entries)
        m_availableThemes << entry.chopped(4);
}

void AppContext::setCurrentTheme(const QString& theme) {
    if (m_currentTheme != theme && m_availableThemes.contains(theme)) {
        m_currentTheme = theme;
        m_settings->setValue("Preferences/Theme", theme);
        applyTheme(theme);
        emit themeChanged(theme);
    }
}

QString AppContext::loadStylesheet(const QString& themeName) const {
    QString qssPath = QString(":/themes/%1.qss").arg(themeName);
    QFile file(qssPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot load stylesheet:" << qssPath;
        return QString();
    }
    QString content = file.readAll();
    file.close();
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

//-----------------------------------------------------------------------------
// Language (hot-switch)
//-----------------------------------------------------------------------------

QString AppContext::languageCode() const {
    if (m_currentLanguage == "中文") return "chinese";
    if (m_currentLanguage == "日本語") return "japanese";
    return QString();  // English = no extra translator
}

void AppContext::loadTranslator(const QString& langCode) {
    // Try filesystem first (runtime directory)
    QString fsPath = QApplication::applicationDirPath()
                   + "/translations/" + langCode + "/template.qm";
    // Fallback to resource
    QString resPath = QString(":/translations/%1/template.qm").arg(langCode);

    QTranslator* t = new QTranslator(this);
    bool ok = t->load(fsPath);
    if (!ok) {
        QFile qmFile(resPath);
        if (qmFile.open(QIODevice::ReadOnly)) {
            QByteArray data = qmFile.readAll();
            ok = t->load(reinterpret_cast<const uchar*>(data.constData()), data.size());
        }
    }
    if (ok) {
        m_appTranslator = t;
        qApp->installTranslator(t);
    } else {
        delete t;
    }
}

void AppContext::unloadTranslator() {
    if (m_appTranslator) {
        qApp->removeTranslator(m_appTranslator);
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }
}

void AppContext::setCurrentLanguage(const QString& language) {
    if (m_currentLanguage == language || !m_availableLanguages.contains(language))
        return;

    m_currentLanguage = language;
    m_settings->setValue("Preferences/Language", language);

    // Hot-swap translator
    unloadTranslator();
    QString code = languageCode();
    if (!code.isEmpty()) loadTranslator(code);

    emit languageChanged(language);
}

QStringList AppContext::availableLanguages() const {
    return m_availableLanguages;
}
