#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <memory>

class Room;
class MainWindow;

class AppContext : public QObject {
    Q_OBJECT
public:
    static AppContext* instance();

    void setSettingsPath(const QString& path);
    QSettings* settings() { return m_settings.get(); }

    void setCurrentRoomName(const QString& name);
    QString currentRoomName() const { return m_currentRoomName; }

    void setMainWindow(MainWindow* mw) { m_mainWindow = mw; }
    MainWindow* mainWindow() const { return m_mainWindow; }

signals:
    void roomChanged(const QString& roomName);

private:
    explicit AppContext(QObject* parent = nullptr);
    std::unique_ptr<QSettings> m_settings;
    QString m_settingsPath;
    QString m_currentRoomName;
    MainWindow* m_mainWindow = nullptr;

    static AppContext* m_instance;
};
