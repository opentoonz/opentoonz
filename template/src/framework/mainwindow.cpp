#include "mainwindow.h"
#include "pane.h"
#include "menubar.h"
#include "menubarcommand.h"
#include "appcontext.h"
#include "docklayout.h"

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QCloseEvent>
#include <QVBoxLayout>

extern void registerDemoPanels();

//=============================================================================
// Room
//-----------------------------------------------------------------------------

void Room::save() {
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rooms/";
    QDir().mkpath(dirPath);
    QString filePath = dirPath + m_name + ".ini";
    QSettings settings(filePath, QSettings::IniFormat);
    settings.clear();

    DockLayout* layout = dockLayout();
    settings.beginGroup("room");

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item || !item->widget()) continue;
        TPanel* pane = qobject_cast<TPanel*>(item->widget());
        if (!pane) continue;

        settings.beginGroup(QString("pane_%1").arg(i));
        settings.setValue("name", QString::fromStdString(pane->getPanelType()));
        settings.setValue("geometry", pane->geometry());
        settings.endGroup();
    }

    DockLayout::State state = layout->saveState();
    settings.setValue("hierarchy", state.second);
    settings.endGroup();
}

//-----------------------------------------------------------------------------

void Room::load(const QString& path) {
    QString filePath = path;
    if (filePath.isEmpty()) {
        filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/rooms/" + m_name + ".ini";
    }
    m_path = filePath;

    QSettings settings(filePath, QSettings::IniFormat);
    settings.beginGroup("room");

    QStringList keys = settings.childGroups();
    DockLayout* layout = dockLayout();

    QStringList paneKeys;
    for (const QString& key : keys) {
        if (key.startsWith("pane_")) paneKeys.append(key);
    }
    paneKeys.sort();

    for (const QString& key : paneKeys) {
        settings.beginGroup(key);
        QString typeName = settings.value("name").toString();
        TPanel* pane = TPanelFactory::createPanel(this, typeName);
        if (pane) {
            addDockWidget(pane);
            QRect geom = settings.value("geometry").toRect();
            if (geom.isValid()) pane->setGeometry(geom);
        }
        settings.endGroup();
    }

    QString hierarchy = settings.value("hierarchy").toString();
    if (!hierarchy.isEmpty()) {
        std::vector<QRect> geoms;
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
            geoms.push_back(item && item->widget() ? item->widget()->geometry() : QRect());
        }
        layout->restoreState({geoms, hierarchy});
    }

    settings.endGroup();
    m_initialized = true;
}

//=============================================================================
// MainWindow
//-----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("YZ UI Template");
    resize(1280, 720);

    AppContext::instance()->setMainWindow(this);

    // Register demo panels (must happen before room creation)
    registerDemoPanels();

    // Create TopBar
    m_topBar = new TopBar(this);
    addToolBar(Qt::TopToolBarArea, m_topBar);
    m_topBar->setMovable(false);

    // Create stacked widget for rooms
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);

    // Define actions (must happen before menu building)
    defineActions();

    // Connect TopBar signals
    RoomTabWidget* tabs = qobject_cast<RoomTabWidget*>(m_topBar->getRoomTabWidget());
    connect(tabs, &RoomTabWidget::currentChanged, this, &MainWindow::onCurrentRoomChanged);
    connect(tabs, &RoomTabWidget::indexSwapped, this, &MainWindow::onIndexSwapped);
    connect(tabs, &RoomTabWidget::insertNewTabRoom, this, &MainWindow::insertNewRoom);
    connect(tabs, &RoomTabWidget::deleteTabRoom, this, &MainWindow::deleteRoom);
    connect(tabs, &RoomTabWidget::renameTabRoom, this, &MainWindow::renameRoom);

    // Load or create rooms
    readSettings();
}

//-----------------------------------------------------------------------------

MainWindow::~MainWindow() {
    writeSettings();
}

//-----------------------------------------------------------------------------

void MainWindow::defineActions() {
    CommandManager* cm = CommandManager::instance();

    cm->createAction("MI_SaveLayout", "Save Layout", "Ctrl+S");
    cm->createAction("MI_LoadLayout", "Load Layout", "Ctrl+O");
    cm->createAction("MI_Quit", "Quit", "Ctrl+Q");
    cm->createAction("MI_OpenLogPanel", "Log Panel", "");
    cm->createAction("MI_OpenPropertyPanel", "Property Inspector", "");
    cm->createAction("MI_OpenCanvasPanel", "Canvas", "");
    cm->createAction("MI_OpenCommandPalette", "Command Palette", "");
    cm->createAction("MI_OpenWelcomePanel", "Welcome", "");
    cm->createAction("MI_About", "About", "");

    struct QuitHandler final : CommandHandlerInterface {
        void execute() override { QApplication::quit(); }
    };
    CommandManager::instance()->setHandler("MI_Quit", new QuitHandler());
}

//-----------------------------------------------------------------------------

void MainWindow::createDefaultRooms() {
    // ===== Edit Room =====
    {
        Room* room = new Room(this);
        room->setName("Edit");
        DockLayout* layout = room->dockLayout();

        TPanel* canvas = TPanelFactory::createPanel(room, "Canvas");
        room->addDockWidget(canvas);
        layout->dockItem(canvas);

        TPanel* props = TPanelFactory::createPanel(room, "PropertyInspector");
        room->addDockWidget(props);
        layout->dockItem(props, canvas, Region::right);

        TPanel* cmd = TPanelFactory::createPanel(room, "CommandPalette");
        room->addDockWidget(cmd);
        layout->dockItem(cmd, canvas, Region::bottom);

        m_stackedWidget->addWidget(room);
        m_topBar->getRoomTabWidget()->addTab("Edit");
    }

    // ===== Debug Room =====
    {
        Room* room = new Room(this);
        room->setName("Debug");
        DockLayout* layout = room->dockLayout();

        TPanel* log = TPanelFactory::createPanel(room, "LogPanel");
        room->addDockWidget(log);
        layout->dockItem(log);

        TPanel* props = TPanelFactory::createPanel(room, "PropertyInspector");
        room->addDockWidget(props);
        layout->dockItem(props, log, Region::right);

        m_stackedWidget->addWidget(room);
        m_topBar->getRoomTabWidget()->addTab("Debug");
    }

    // ===== Settings Room =====
    {
        Room* room = new Room(this);
        room->setName("Settings");
        DockLayout* layout = room->dockLayout();

        TPanel* welcome = TPanelFactory::createPanel(room, "Welcome");
        room->addDockWidget(welcome);
        layout->dockItem(welcome);

        m_stackedWidget->addWidget(room);
        m_topBar->getRoomTabWidget()->addTab("Settings");
    }
}

//-----------------------------------------------------------------------------

void MainWindow::readSettings() {
    QSettings* settings = AppContext::instance()->settings();
    settings->beginGroup("MainWindow");
    restoreGeometry(settings->value("geometry").toByteArray());
    int lastRoom = settings->value("lastRoom", 0).toInt();
    settings->endGroup();

    QString roomsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rooms/";
    QDir dir(roomsDir);
    QStringList roomFiles = dir.entryList({"*.ini"}, QDir::Files, QDir::Name);

    if (roomFiles.isEmpty()) {
        createDefaultRooms();
    } else {
        for (const QString& file : roomFiles) {
            Room* room = new Room(this);
            QString name = file.chopped(4);
            room->setName(name);
            room->load(roomsDir + file);
            m_stackedWidget->addWidget(room);
            m_topBar->getRoomTabWidget()->addTab(name);
        }
    }

    if (m_stackedWidget->count() > 0) {
        int idx = qMin(lastRoom, m_stackedWidget->count() - 1);
        switchToRoom(idx);
        m_topBar->getRoomTabWidget()->setCurrentIndex(idx);
    }
}

//-----------------------------------------------------------------------------

void MainWindow::writeSettings() {
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        Room* room = getRoom(i);
        if (room) room->save();
    }

    QSettings* settings = AppContext::instance()->settings();
    settings->beginGroup("MainWindow");
    settings->setValue("geometry", saveGeometry());
    settings->setValue("lastRoom", m_stackedWidget->currentIndex());
    settings->endGroup();
}

//-----------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent* event) {
    writeSettings();
    event->accept();
}

//-----------------------------------------------------------------------------

void MainWindow::switchToRoom(int index) {
    if (index >= 0 && index < m_stackedWidget->count()) {
        m_stackedWidget->setCurrentIndex(index);
        Room* room = getRoom(index);
        if (room) {
            AppContext::instance()->setCurrentRoomName(room->getName());
            // Update menu bar for this room
            StackedMenuBar* smb = m_topBar->getStackedMenuBar();
            smb->createMenuBarForRoom(room->getName());
        }
    }
}

//-----------------------------------------------------------------------------

void MainWindow::onCurrentRoomChanged(int index) {
    switchToRoom(index);
}

//-----------------------------------------------------------------------------

void MainWindow::addRoom(Room* room) {
    m_stackedWidget->addWidget(room);
    m_topBar->getRoomTabWidget()->addTab(room->getName());
}

//-----------------------------------------------------------------------------

Room* MainWindow::getCurrentRoom() const {
    return qobject_cast<Room*>(m_stackedWidget->currentWidget());
}

//-----------------------------------------------------------------------------

Room* MainWindow::getRoom(int index) const {
    return qobject_cast<Room*>(m_stackedWidget->widget(index));
}

//-----------------------------------------------------------------------------

int MainWindow::getRoomCount() const {
    return m_stackedWidget->count();
}

//-----------------------------------------------------------------------------

Room* MainWindow::getRoomByName(const QString& name) {
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        Room* room = getRoom(i);
        if (room && room->getName() == name) return room;
    }
    return nullptr;
}

//-----------------------------------------------------------------------------

void MainWindow::onIndexSwapped(int first, int second) {
    // Tab reorder: swap widgets in the stacked widget
    if (first == second) return;
    QWidget* w1 = m_stackedWidget->widget(first);
    QWidget* w2 = m_stackedWidget->widget(second);
    if (!w1 || !w2) return;
    m_stackedWidget->removeWidget(w1);
    m_stackedWidget->removeWidget(w2);
    if (first < second) {
        m_stackedWidget->insertWidget(first, w2);
        m_stackedWidget->insertWidget(second, w1);
    } else {
        m_stackedWidget->insertWidget(second, w1);
        m_stackedWidget->insertWidget(first, w2);
    }
}

//-----------------------------------------------------------------------------

void MainWindow::insertNewRoom() {
    Room* room = new Room(this);
    room->setName("New Room");
    addRoom(room);
    switchToRoom(m_stackedWidget->count() - 1);
}

//-----------------------------------------------------------------------------

void MainWindow::deleteRoom(int index) {
    if (m_stackedWidget->count() <= 1) return;
    QWidget* w = m_stackedWidget->widget(index);
    m_stackedWidget->removeWidget(w);
    m_topBar->getRoomTabWidget()->removeTab(index);
    delete w;
    if (m_stackedWidget->currentIndex() < 0 && m_stackedWidget->count() > 0)
        switchToRoom(0);
}

//-----------------------------------------------------------------------------

void MainWindow::renameRoom(int index, const QString& name) {
    Room* room = getRoom(index);
    if (room) {
        room->setName(name);
        m_topBar->getRoomTabWidget()->setTabText(index, name);
    }
}

//-----------------------------------------------------------------------------

void MainWindow::onLockRoomChanged(bool locked) {
    RoomTabWidget* tabs = qobject_cast<RoomTabWidget*>(m_topBar->getRoomTabWidget());
    if (tabs) tabs->setIsLocked(locked);
}
