#include "appcontext.h"
#include <QStandardPaths>
#include <QDir>

AppContext* AppContext::m_instance = nullptr;

AppContext* AppContext::instance() {
    if (!m_instance)
        m_instance = new AppContext();
    return m_instance;
}

AppContext::AppContext(QObject* parent)
    : QObject(parent)
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString iniPath = appData + "/settings.ini";
    m_settings = std::make_unique<QSettings>(iniPath, QSettings::IniFormat);
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
