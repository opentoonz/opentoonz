#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <memory>

class Room;
class MainWindow;
class QTranslator;

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

    // Language management (hot-switch, no restart needed)
    QString currentLanguage() const { return m_currentLanguage; }
    void setCurrentLanguage(const QString& language);
    QStringList availableLanguages() const;
    QString languageCode() const;  // "chinese"/"japanese"/"" (English)

    // Stylesheet loading
    QString loadStylesheet(const QString& themeName) const;
    void applyTheme(const QString& themeName);

signals:
    void roomChanged(const QString& roomName);
    void themeChanged(const QString& themeName);
    void languageChanged(const QString& language);

private:
    void scanAvailableThemes();
    void loadTranslator(const QString& langCode);
    void unloadTranslator();

    QStringList m_availableThemes;
    QStringList m_availableLanguages;
    explicit AppContext(QObject* parent = nullptr);
    std::unique_ptr<QSettings> m_settings;
    QString m_settingsPath;
    QString m_currentRoomName;
    QString m_currentTheme;
    QString m_currentLanguage;
    MainWindow* m_mainWindow = nullptr;
    QTranslator* m_appTranslator = nullptr;

    static AppContext* m_instance;
};
