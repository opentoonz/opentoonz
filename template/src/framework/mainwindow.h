#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>
#include <QSettings>
#include <QString>
#include <QMap>
#include <memory>
#include "tdockwindows.h"

class TPanel;
class TopBar;

class Room : public TMainWindow {
    Q_OBJECT
public:
    Room(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags())
        : TMainWindow(parent, flags), m_initialized(false) {}

    QString getName() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    QString getPath() const { return m_path; }
    void setPath(const QString& path) { m_path = path; }

    void save();
    void load(const QString& path);
    bool notInitialized() const { return !m_initialized; }
    void initialize() { load(m_path); }

private:
    QString m_path;
    QString m_name;
    bool m_initialized = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    Room* getCurrentRoom() const;
    Room* getRoom(int index) const;
    Room* getRoomByName(const QString& name);
    int getRoomCount() const;

    void addRoom(Room* room);
    void switchToRoom(int index);
    void defineActions();

public slots:
    void onCurrentRoomChanged(int index);
    void onIndexSwapped(int first, int second);
    void insertNewRoom();
    void deleteRoom(int index);
    void renameRoom(int index, const QString& name);
    void onLockRoomChanged(bool locked);

protected:
    void closeEvent(QCloseEvent*) override;
    void readSettings();
    void writeSettings();

private:
    void createDefaultRooms();
    void createToolBars();

    TopBar* m_topBar = nullptr;
    QToolBar* m_topToolBar = nullptr;
    QToolBar* m_leftToolBar = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    bool m_saveSettingsOnQuit = true;
    int m_oldRoomIndex = 0;
};
