# yz-ui-template Design Spec

## Goal

Extract OpenToonz's custom docking/room/panel UI framework into a standalone CMake + Qt6 project template at repo root `template/`, by copying relevant source files and stripping all OpenToonz-specific dependencies (toonzlib, tnzcore, animation tools, etc.).

## Directory Structure

```
template/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── framework/
│   │   ├── docklayout.h / .cpp
│   │   ├── dockwidget.h / .cpp
│   │   ├── tdockwindows.h / .cpp
│   │   ├── pane.h / .cpp
│   │   ├── mainwindow.h / .cpp
│   │   ├── menubar.h / .cpp
│   │   ├── menubarcommand.h / .cpp
│   │   └── gutil.h / .cpp
│   ├── panels/
│   │   ├── logpanel.h / .cpp
│   │   ├── propertypanel.h / .cpp
│   │   ├── canvaspanel.h / .cpp
│   │   ├── commandpalette.h / .cpp
│   │   └── welcomepanel.h / .cpp
│   └── main.cpp
└── resources/
```

## Dependency Cut Plan

| Original | Replacement |
|----------|-------------|
| TFilePath | QString |
| TApp::instance() | AppContext singleton (currentRoom + settings only) |
| TSceneHandle / TXsheetHandle / TPaletteHandle / etc. | Deleted |
| Preferences | QSettings |
| ToonzFolder::getMyModuleDir() | QStandardPaths(AppDataLocation) |
| TEnv | QSettings key-value |
| SaveLoadQSettings | Retained as optional interface |
| CustomPanelManager | Deleted |
| FlipBook pool logic | Deleted |
| tcommon.h (DVAPI/DV_IMPORT) | Removed (static lib, no DLL export needed) |

## Docking Engine (clean copy, no modifications)

- `docklayout.h/.cpp` — Region tree layout engine, zero toonzlib deps
- `dockwidget.h/.cpp` — DockWidget base class + DockingCheck, zero toonzlib deps
- `tdockwindows.h/.cpp` — TMainWindow/TDockWidget wrappers, zero toonzlib deps

## Framework (modified copies)

- `pane.h/.cpp` — TPanel (TDockWidget subclass) + TPanelFactory (string→panel registry). Strip: TApp, Preferences, ToonzFolder, TEnv, CustomPanelManager, SaveLoadQSettings file I/O
- `mainwindow.h/.cpp` — Room (TMainWindow subclass, lazy-load from QSettings) + MainWindow (QMainWindow, QStackedWidget for rooms). Strip: TFilePath→QString, all toonzlib panel creation, FlipBook pool, TApp couplings
- `menubar.h/.cpp` — TopBar (QToolBar + RoomTabWidget + StackedMenuBar). Strip: TFilePath-based menu XML loading
- `menubarcommand.h/.cpp` — CommandManager, MenuItemHandler, DVAction. Strip: ToonzFolder-based shortcut path

## Demo Content

### 3 Room Presets
1. **Edit** — CanvasPanel (center) + PropertyInspector (right) + CommandPalette (bottom)
2. **Debug** — LogPanel (center) + PropertyInspector (right)
3. **Settings** — WelcomePanel only

### 5 Demo Panel Types
1. **LogPanel** — QPlainTextEdit with appendLine() slot, demonstrates signal/slot across panels
2. **PropertyInspector** — QTreeWidget with 2-column key-value, demonstrates selection-driven updates
3. **CanvasPanel** — QLabel placeholder with painted checkerboard, demonstrates central content area
4. **CommandPalette** — QLineEdit + QListWidget, demonstrates keyboard-driven UI
5. **WelcomePanel** — QLabel rich text, demonstrates static content panel

### TopBar
- Room tab bar (QTabBar-style) for switching rooms
- Lock room checkbox

### Menus
- File: Save Layout, Load Layout, Exit
- View: Toggle each panel type
- Help: About

## Build System

- CMake 3.16+
- Qt6 (Widgets, Core, Gui)
- Static library for framework/, executable for main.cpp + panels/
- C++17
