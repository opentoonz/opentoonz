# Architecture

## Class Hierarchy

```
QFrame
  └── DockWidget              (docklayout.h)   — draggable, floating, dockable base
        └── TDockWidget       (tdockwindows.h) — title bar + content widget
              └── TPanel      (pane.h)         — type system, maximize, factory

QWidget
  └── TMainWindow             (tdockwindows.h) — wraps DockLayout
        └── Room              (mainwindow.h)   — named workspace, save/load

QMainWindow
  └── MainWindow              (mainwindow.h)   — top-level, QStackedWidget of Rooms

QToolBar
  └── TopBar                  (menubar.h)      — room tabs + StackedMenuBar

QStackedWidget
  └── StackedMenuBar          (menubar.h)      — per-room menu bars

QTabBar
  └── RoomTabWidget           (menubar.h)      — tab reorder, rename, context menu

QLayout
  └── DockLayout              (docklayout.h)   — Region tree, dock/undock, saveState
```

## MainWindow Structure

```
MainWindow (QMainWindow)
├── TopBar (QToolBar, TopToolBarArea)
│   ├── StackedMenuBar (QStackedWidget)
│   │   └── QMenuBar per room (File, View, Settings, Help)
│   ├── RoomTabWidget (QTabBar) — "Edit" | "Debug" | "Settings"
│   └── QCheckBox — Lock
├── Top Toolbar (QToolBar, TopToolBarArea) — NewRoom, Save, Load, About
├── Left Toolbar (QToolBar, LeftToolBarArea) — Panel toggles
└── QStackedWidget (centralWidget)
    ├── Room "Edit" (TMainWindow → DockLayout)
    │   ├── TPanel "Canvas" (CanvasPanel widget)
    │   ├── TPanel "PropertyInspector" (PropertyInspector widget)
    │   └── TPanel "CommandPalette" (CommandPalette widget)
    ├── Room "Debug"
    └── Room "Settings"
```

## Signal Flow

```
RoomTabWidget::currentChanged  →  MainWindow::onCurrentRoomChanged
  └── switchToRoom()
        ├── QStackedWidget::setCurrentIndex()
        ├── AppContext::setCurrentRoomName()
        ├── StackedMenuBar::createMenuBarForRoom()
        └── updatePanelVisibility()  — show/hide room-bound panels

AppContext::languageChanged  →  StackedMenuBar::refreshCurrentMenu
  └── buildDefaultMenuBar() re-runs with new translator

AppContext::themeChanged  —  emitted after setStyleSheet + ThemeManager update

DVAction::triggered  →  onTriggered()  →  CommandManager::execute(action)
  └── node->handler->execute()  — calls MainWindow method
```

## Action System

```
defineActions()
  └── CommandManager::createAction(id, name, shortcut)
        └── new DVAction(name)
              └── connect(triggered → onTriggered → CommandManager::execute)
        └── CommandManager::define(id, type, shortcut, action)
              └── stores in m_idTable + m_qactionTable

setCommandHandler(id, target, &method)
  └── CommandHandlerHelper<T>(target, method)
        └── handler->execute() → (target->*method)()

Menu / Toolbar:
  └── CommandManager::getAction(id) → QAction*
        └── addAction(action) to QMenu or QToolBar
```

## Panel Lifecycle

```
1. Registration (static init)
   TPanelFactory subclass → TPanelFactory("PanelName")
   OpenFloatingPanel(cmdId, "PanelName", title) → MenuItemHandler

2. Creation
   TPanelFactory::createPanel(parent, "PanelName")
     → factory.initialize(panel)
       → panel.setWidget(new MyWidget(panel))

3. Discovery (View menu)
   CommandManager::getActions(MenuWindowsCommandType)
     → lists all OpenFloatingPanel-registered actions

4. Open / Float / Restore
   OpenFloatingPanel::getOrOpenFloatingPanel("PanelName")
     → find existing hidden/floating panel → restore
     → or create new → setFloating(true) → restoreFloatingPanelState()
```

## Config Files (XML)

```
AppData/Roaming/YZ/YZ UI Template/
├── settings.xml       — AppContext (theme, language, window geometry)
├── shortcuts.xml      — CommandManager (user shortcut overrides)
├── popups.xml         — TPanel (floating panel positions)
└── rooms/
    ├── Edit.xml       — Room::save/load (panels + hierarchy)
    ├── Debug.xml
    └── Settings.xml
```

## Key Singletons

| Class | Role |
|-------|------|
| `AppContext` | Settings, theme, language, current room name, translator |
| `CommandManager` | Action/command registry, shortcut table |
| `ThemeManager` | Icon metadata, `:TOONZCOLORS` parsing, icon colors |
| `DockingCheck` | Lock/unlock all dock layouts |
