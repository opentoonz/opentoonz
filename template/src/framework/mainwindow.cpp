#include "mainwindow.h"
#include "pane.h"
#include "menubar.h"
#include "menubarcommand.h"
#include "appcontext.h"
#include "docklayout.h"
#include "preferences/prefpopup.h"

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QStyle>

extern void registerDemoPanels();

//=============================================================================
// Room
//-----------------------------------------------------------------------------

void Room::save() {
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rooms/";
    QDir().mkpath(dirPath);
    QString filePath = dirPath + m_name + ".xml";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    DockLayout* layout = dockLayout();
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("room");
    xml.writeAttribute("name", m_name);
    xml.writeStartElement("panels");

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item || !item->widget()) continue;
        TPanel* pane = qobject_cast<TPanel*>(item->widget());
        if (!pane) continue;

        xml.writeStartElement("panel");
        xml.writeAttribute("index", QString::number(i));
        xml.writeAttribute("name", QString::fromStdString(pane->getPanelType()));
        QRect g = pane->geometry();
        xml.writeAttribute("x", QString::number(g.x()));
        xml.writeAttribute("y", QString::number(g.y()));
        xml.writeAttribute("w", QString::number(g.width()));
        xml.writeAttribute("h", QString::number(g.height()));
        xml.writeEndElement(); // panel
    }

    xml.writeEndElement(); // panels

    DockLayout::State state = layout->saveState();
    xml.writeTextElement("hierarchy", state.second);

    xml.writeEndElement(); // room
    xml.writeEndDocument();
    file.close();
}

//-----------------------------------------------------------------------------

void Room::load(const QString& path) {
    QString filePath = path;
    if (filePath.isEmpty()) {
        filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/rooms/" + m_name + ".xml";
    }
    m_path = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_initialized = true;
        return;
    }

    DockLayout* layout = dockLayout();
    QXmlStreamReader xml(&file);
    QString hierarchy;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("panel")) {
                QString typeName = xml.attributes().value("name").toString();
                int x = xml.attributes().value("x").toInt();
                int y = xml.attributes().value("y").toInt();
                int w = xml.attributes().value("w").toInt();
                int h = xml.attributes().value("h").toInt();

                TPanel* pane = TPanelFactory::createPanel(this, typeName);
                if (pane) {
                    addDockWidget(pane);
                    if (w > 0 && h > 0) pane->setGeometry(x, y, w, h);
                }
            } else if (xml.name() == QStringLiteral("hierarchy")) {
                hierarchy = xml.readElementText();
            }
        }
    }

    if (!hierarchy.isEmpty()) {
        std::vector<QRect> geoms;
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
            geoms.push_back(item && item->widget() ? item->widget()->geometry() : QRect());
        }
        layout->restoreState({geoms, hierarchy});
    }

    file.close();
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

    // Define actions (must happen before menu + toolbar building)
    defineActions();

    // Create toolbars
    createToolBars();

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

    // File actions
    cm->createAction("MI_SaveLayout", "Save Layout", "Ctrl+S");
    cm->createAction("MI_LoadLayout", "Load Layout", "Ctrl+O");
    cm->createAction("MI_Quit", "Quit", "Ctrl+Q");

    // Panel toggle actions (MenuWindowsCommandType for View menu auto-fill)
    cm->createAction("MI_OpenLogPanel", "Log Panel", "", MenuWindowsCommandType);
    cm->createAction("MI_OpenPropertyPanel", "Property Inspector", "", MenuWindowsCommandType);
    cm->createAction("MI_OpenCanvasPanel", "Canvas", "", MenuWindowsCommandType);
    cm->createAction("MI_OpenCommandPalette", "Command Palette", "", MenuWindowsCommandType);
    cm->createAction("MI_OpenWelcomePanel", "Welcome", "", MenuWindowsCommandType);
    cm->createAction("MI_OpenComboViewer", "Combo Viewer", "", MenuWindowsCommandType);

    // Toolbar actions (top)
    cm->createAction("MI_NewRoom", "New Room", "Ctrl+N");
    cm->createAction("MI_LockRooms", "Lock Rooms", "");
    cm->createAction("MI_MaximizePanel", "Maximize Panel", "`");
    cm->createAction("MI_Preferences", "Preferences", "Ctrl+P");
    cm->createAction("MI_About", "About", "");

    struct QuitHandler final : CommandHandlerInterface {
        void execute() override { QApplication::quit(); }
    };
    CommandManager::instance()->setHandler("MI_Quit", new QuitHandler());

    struct PrefHandler final : CommandHandlerInterface {
        void execute() override {
            PreferencesPopup popup;
            popup.exec();
        }
    };
    CommandManager::instance()->setHandler("MI_Preferences", new PrefHandler());

    // View actions (fullscreen)
    cm->createAction("MI_FullScreenWindow", "Full Screen", "Ctrl+`");
    cm->createAction("MI_SeeThroughWindow", "See Through Mode", "Alt+`");

    // Toolbar handlers
    setCommandHandler("MI_NewRoom", this, &MainWindow::insertNewRoom);
    setCommandHandler("MI_MaximizePanel", this, &MainWindow::maximizePanel);
    setCommandHandler("MI_FullScreenWindow", this, &MainWindow::fullScreenWindow);
    setCommandHandler("MI_SeeThroughWindow", this, &MainWindow::seeThroughWindow);
}

//-----------------------------------------------------------------------------

void MainWindow::createToolBars() {
    QStyle* style = QApplication::style();

    // ===== Top Horizontal Toolbar =====
    m_topToolBar = new QToolBar(tr("Main Toolbar"), this);
    m_topToolBar->setObjectName("MainToolBar");
    m_topToolBar->setMovable(false);
    m_topToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_topToolBar->setIconSize(QSize(20, 20));

    QAction* newRoomAct = CommandManager::instance()->getAction("MI_NewRoom", true);
    newRoomAct->setIcon(style->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_topToolBar->addAction(newRoomAct);

    m_topToolBar->addSeparator();

    QAction* saveAct = CommandManager::instance()->getAction("MI_SaveLayout", true);
    saveAct->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    m_topToolBar->addAction(saveAct);

    QAction* loadAct = CommandManager::instance()->getAction("MI_LoadLayout", true);
    loadAct->setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    m_topToolBar->addAction(loadAct);

    m_topToolBar->addSeparator();

    QAction* aboutAct = CommandManager::instance()->getAction("MI_About", true);
    aboutAct->setIcon(style->standardIcon(QStyle::SP_MessageBoxInformation));
    m_topToolBar->addAction(aboutAct);

    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, m_topToolBar);

    // ===== Left Vertical Toolbar =====
    m_leftToolBar = new QToolBar(tr("Panel Toolbar"), this);
    m_leftToolBar->setObjectName("PanelToolBar");
    m_leftToolBar->setOrientation(Qt::Vertical);
    m_leftToolBar->setMovable(false);
    m_leftToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_leftToolBar->setIconSize(QSize(20, 20));

    QAction* logAct = CommandManager::instance()->getAction("MI_OpenLogPanel", true);
    logAct->setIcon(style->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_leftToolBar->addAction(logAct);

    QAction* propAct = CommandManager::instance()->getAction("MI_OpenPropertyPanel", true);
    propAct->setIcon(style->standardIcon(QStyle::SP_FileDialogInfoView));
    m_leftToolBar->addAction(propAct);

    QAction* canvasAct = CommandManager::instance()->getAction("MI_OpenCanvasPanel", true);
    canvasAct->setIcon(style->standardIcon(QStyle::SP_ComputerIcon));
    m_leftToolBar->addAction(canvasAct);

    QAction* cmdAct = CommandManager::instance()->getAction("MI_OpenCommandPalette", true);
    cmdAct->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    m_leftToolBar->addAction(cmdAct);

    QAction* welcomeAct = CommandManager::instance()->getAction("MI_OpenWelcomePanel", true);
    welcomeAct->setIcon(style->standardIcon(QStyle::SP_TitleBarContextHelpButton));
    m_leftToolBar->addAction(welcomeAct);

    addToolBar(Qt::LeftToolBarArea, m_leftToolBar);
}

//-----------------------------------------------------------------------------

void MainWindow::createDefaultRooms() {
    // ===== Edit Room =====
    {
        Room* room = new Room(this);
        room->setName("Edit");
        DockLayout* layout = room->dockLayout();

        TPanel* viewer = TPanelFactory::createPanel(room, "ComboViewer");
        room->addDockWidget(viewer);
        layout->dockItem(viewer);

        TPanel* props = TPanelFactory::createPanel(room, "PropertyInspector");
        room->addDockWidget(props);
        layout->dockItem(props, viewer, Region::right);

        TPanel* cmd = TPanelFactory::createPanel(room, "CommandPalette");
        room->addDockWidget(cmd);
        layout->dockItem(cmd, viewer, Region::bottom);

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

    // ===== Design Room (Figma-style layout) =====
    {
        Room* room = new Room(this);
        room->setName("Design");
        DockLayout* layout = room->dockLayout();

        // Top: Design toolbar (fixed height)
        TPanel* toolbar = TPanelFactory::createPanel(room, "DesignToolbar");
        room->addDockWidget(toolbar);
        layout->dockItem(toolbar);

        // Center: Canvas with rulers
        TPanel* canvas = TPanelFactory::createPanel(room, "Canvas");
        room->addDockWidget(canvas);
        layout->dockItem(canvas, toolbar, Region::bottom);

        // Left: Layers panel
        TPanel* layers = TPanelFactory::createPanel(room, "Layers");
        room->addDockWidget(layers);
        layout->dockItem(layers, canvas, Region::left);

        // Right: Properties panel
        TPanel* props = TPanelFactory::createPanel(room, "Properties");
        room->addDockWidget(props);
        layout->dockItem(props, canvas, Region::right);

        m_stackedWidget->addWidget(room);
        m_topBar->getRoomTabWidget()->addTab("Design");
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
    auto* ctx = AppContext::instance();
    restoreGeometry(ctx->setting("MainWindow", "geometry").toByteArray());
    int lastRoom = ctx->setting("MainWindow", "lastRoom", 0).toInt();

    QString roomsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/rooms/";
    QDir dir(roomsDir);
    QStringList roomFiles = dir.entryList({"*.xml"}, QDir::Files, QDir::Name);

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

    auto* ctx = AppContext::instance();
    ctx->setSetting("MainWindow", "geometry", saveGeometry());
    ctx->setSetting("MainWindow", "lastRoom", m_stackedWidget->currentIndex());
    ctx->saveSettings();
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
            // Show/hide room-bound panels
            updatePanelVisibility();
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

//-----------------------------------------------------------------------------
// Fullscreen
//-----------------------------------------------------------------------------

void MainWindow::fullScreenWindow() {
    if (isFullScreen()) {
        if (m_wasMaximized)
            showMaximized();
        else
            showNormal();
    } else {
        m_wasMaximized = isMaximized();
        showFullScreen();
    }
}

//-----------------------------------------------------------------------------
// See-Through Window Mode (opacity slider popup)
//-----------------------------------------------------------------------------

class SeeThroughPopup : public QWidget {
    Q_OBJECT
public:
    SeeThroughPopup(QWidget* mainWindow)
        : QWidget(nullptr, Qt::Window | Qt::WindowStaysOnTopHint)
        , m_mainWindow(mainWindow), m_savedOpacity(1.0)
    {
        setWindowTitle(tr("See Through Mode"));
        setFixedSize(320, 80);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        auto* topLayout = new QHBoxLayout();
        topLayout->addWidget(new QLabel(tr("Opacity:"), this));

        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setRange(2, 100);
        m_slider->setValue(100);
        m_slider->setTickPosition(QSlider::TicksBelow);
        m_slider->setTickInterval(10);
        connect(m_slider, &QSlider::valueChanged, this, &SeeThroughPopup::onSliderChanged);
        topLayout->addWidget(m_slider);

        m_toggleBtn = new QPushButton(tr("Toggle"), this);
        m_toggleBtn->setCheckable(true);
        m_toggleBtn->setToolTip(tr("Hold Alt to toggle full transparent"));
        connect(m_toggleBtn, &QPushButton::clicked, this, &SeeThroughPopup::onToggle);
        topLayout->addWidget(m_toggleBtn);

        layout->addLayout(topLayout);
    }

protected:
    void showEvent(QShowEvent*) override {
        m_slider->setValue(static_cast<int>(m_savedOpacity * 100));
        onSliderChanged(m_slider->value());
    }
    void hideEvent(QHideEvent*) override {
        m_savedOpacity = m_slider->value() / 100.0;
        m_mainWindow->setWindowOpacity(1.0);
    }

private slots:
    void onToggle() {
        if (m_toggleBtn->isChecked()) {
            bool altHeld = QApplication::keyboardModifiers() & Qt::AltModifier;
            double opacity = altHeld ? 0.0 : 1.0;
            m_mainWindow->setWindowOpacity(opacity);
        } else {
            m_mainWindow->setWindowOpacity(m_slider->value() / 100.0);
        }
    }
    void onSliderChanged(int value) {
        m_mainWindow->setWindowOpacity(value / 100.0);
        m_slider->setToolTip(QString::number(value) + "%");
    }

private:
    QWidget* m_mainWindow;
    QSlider* m_slider;
    QPushButton* m_toggleBtn;
    double m_savedOpacity;
};

void MainWindow::seeThroughWindow() {
    if (!m_seeThroughPopup)
        m_seeThroughPopup = new SeeThroughPopup(this);
    m_seeThroughPopup->setVisible(!m_seeThroughPopup->isVisible());
}

//-----------------------------------------------------------------------------
// Panel Maximize
//-----------------------------------------------------------------------------

void MainWindow::maximizePanel() {
    Room* room = getCurrentRoom();
    if (!room) return;
    DockLayout* layout = room->dockLayout();

    // If there is already a maximized dock, release it
    DockWidget* maximized = layout->getMaximized();
    if (maximized) {
        maximized->maximizeDock();
        return;
    }

    // Find the dock widget under the cursor and maximize it
    QWidget* widgetUnderCursor = layout->containerOf(QCursor::pos());
    TDockWidget* tdw = qobject_cast<TDockWidget*>(widgetUnderCursor);
    if (tdw) tdw->maximizeDock();
}

//-----------------------------------------------------------------------------
// Room-Bound Panel Visibility
//-----------------------------------------------------------------------------

void MainWindow::updatePanelVisibility() {
    Room* currentRoom = getCurrentRoom();
    if (!currentRoom) return;

    QString currentRoomName = currentRoom->getName();

    // Iterate ALL rooms, show/hide room-bound panels
    for (int r = 0; r < getRoomCount(); ++r) {
        Room* room = getRoom(r);
        if (!room) continue;

        DockLayout* layout = room->dockLayout();
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
            if (!item || !item->widget()) continue;
            TPanel* pane = qobject_cast<TPanel*>(item->widget());
            if (!pane || !pane->isRoomBound()) continue;

            if (pane->getBoundRoomName() == currentRoomName) {
                // Panel belongs to current room — show it
                pane->show();
            } else {
                // Panel belongs to another room — hide it
                pane->hide();
            }
        }
    }
}

#include "mainwindow.moc"
