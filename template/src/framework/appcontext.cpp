#include "appcontext.h"
#include "thememanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QTranslator>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
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
    m_settingsPath = appData + "/settings.xml";

    // Available languages
    m_availableLanguages << "English" << "中文" << "日本語";

    // Load settings from XML
    loadSettings();

    // Restore saved preferences
    scanAvailableThemes();
    QString savedTheme = setting("Preferences", "Theme", "Dark").toString();
    m_currentTheme = m_availableThemes.contains(savedTheme) ? savedTheme : QString("Dark");

    m_currentLanguage = setting("Preferences", "Language", "English").toString();

    // Load saved language translator at startup
    QString code = languageCode();
    if (!code.isEmpty()) loadTranslator(code);
}

//-----------------------------------------------------------------------------
// XML Settings
//-----------------------------------------------------------------------------

QString AppContext::prefixedKey(const QString& group, const QString& key) const {
    return group + "/" + key;
}

QVariant AppContext::setting(const QString& group, const QString& key,
                              const QVariant& defaultValue) const {
    return m_settingsMap.value(prefixedKey(group, key), defaultValue);
}

void AppContext::setSetting(const QString& group, const QString& key,
                             const QVariant& val) {
    m_settingsMap[prefixedKey(group, key)] = val;
}

void AppContext::loadSettings() {
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QXmlStreamReader xml(&file);
    QString currentGroup;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("group"))
                currentGroup = xml.attributes().value("name").toString();
            else if (xml.name() == QStringLiteral("value") && !currentGroup.isEmpty()) {
                QString key = xml.attributes().value("key").toString();
                QString val = xml.readElementText();
                m_settingsMap[prefixedKey(currentGroup, key)] = val;
            }
        }
    }
    file.close();
}

void AppContext::saveSettings() {
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    // Collect groups
    QSet<QString> groups;
    for (const QString& pk : m_settingsMap.keys()) {
        int slash = pk.indexOf('/');
        if (slash > 0) groups.insert(pk.left(slash));
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("settings");

    for (const QString& group : groups) {
        xml.writeStartElement("group");
        xml.writeAttribute("name", group);

        QString prefix = group + "/";
        for (auto it = m_settingsMap.begin(); it != m_settingsMap.end(); ++it) {
            if (it.key().startsWith(prefix)) {
                QString key = it.key().mid(prefix.length());
                xml.writeStartElement("value");
                xml.writeAttribute("key", key);
                xml.writeCharacters(it.value().toString());
                xml.writeEndElement(); // value
            }
        }

        xml.writeEndElement(); // group
    }

    xml.writeEndElement(); // settings
    xml.writeEndDocument();
    file.close();
}

//-----------------------------------------------------------------------------

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
        setSetting("Preferences", "Theme", theme);
        saveSettings();
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
    content.replace("../Default/imgs/black/", ":/icons/black/");
    content.replace("../Default/imgs/white/", ":/icons/white/");
    content.replace("imgs/black/", ":/icons/black/");
    content.replace("imgs/white/", ":/icons/white/");
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
    return QString();
}

void AppContext::loadTranslator(const QString& langCode) {
    QString fsPath = QApplication::applicationDirPath()
                   + "/translations/" + langCode + "/template.qm";
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
    setSetting("Preferences", "Language", language);
    saveSettings();

    unloadTranslator();
    QString code = languageCode();
    if (!code.isEmpty()) loadTranslator(code);

    emit languageChanged(language);
}

QStringList AppContext::availableLanguages() const {
    return m_availableLanguages;
}
