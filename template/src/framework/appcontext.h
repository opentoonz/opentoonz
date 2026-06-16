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

    // Theme management
    QString currentTheme() const { return m_currentTheme; }
    void setCurrentTheme(const QString& theme);
    QStringList availableThemes() const;

    // Language management (requires restart)
    QString currentLanguage() const { return m_currentLanguage; }
    void setCurrentLanguage(const QString& language);
    QStringList availableLanguages() const;

    // Stylesheet loading
    QString loadStylesheet(const QString& themeName) const;
    void applyTheme(const QString& themeName);

signals:
    void roomChanged(const QString& roomName);
    void themeChanged(const QString& themeName);

private:
    void scanAvailableThemes();

    QStringList m_availableThemes;
    QStringList m_availableLanguages;
    explicit AppContext(QObject* parent = nullptr);
    std::unique_ptr<QSettings> m_settings;
    QString m_settingsPath;
    QString m_currentRoomName;
    QString m_currentTheme;
    QString m_currentLanguage;
    MainWindow* m_mainWindow = nullptr;

    static AppContext* m_instance;
};
