# YZ UI Template

A C++17 / Qt6 application startup template featuring OpenToonz's custom docking framework.

## Features

- **Custom Docking Engine** — Drag-to-dock, float, resize, rearrange panels
- **Room Workspace System** — Named workspaces with tab-based switching
- **Panel Framework** — TPanel base class with factory registration
- **Layout Persistence** — Save/restore layouts via QSettings
- **Action/Command System** — Global action registry with shortcut support
- **5 Demo Panels** — Log, Property Inspector, Canvas, Command Palette, Welcome
- **3 Room Presets** — Edit, Debug, Settings

## Build

Requirements: CMake 3.16+, Qt6 (Widgets, Core, Gui), C++17 compiler.

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

## Usage

```cpp
// Register a custom panel
class MyPanelFactory : public TPanelFactory {
public:
    MyPanelFactory() : TPanelFactory("MyPanel") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("My Panel");
        panel->setWidget(new MyWidget(panel));
    }
};
static MyPanelFactory myPanelFactoryInstance;

// Create a room
Room* room = new Room(mainWindow);
room->setName("My Room");
TPanel* panel = TPanelFactory::createPanel(room, "MyPanel");
room->addDockWidget(panel);
room->dockLayout()->dockItem(panel);
mainWindow->addRoom(room);
```

## Architecture

```
DockLayout (QLayout subclass, Region tree)
  └── DockWidget (QFrame, draggable/floating/dockable)
      └── TDockWidget (title bar + content widget)
          └── TPanel (type system, maximize, factory registration)

Room (TMainWindow, saved to .ini)
  └── MainWindow (QMainWindow, QStackedWidget of Rooms, TopBar)

CommandManager (singleton, CommandId → QAction mapping)
```

## License

Based on OpenToonz (BSD-3-Clause).
