# YZ UI Template — Developer Documentation

Custom docking framework extracted from OpenToonz. C++17 / Qt6.

## Guides

| Guide | Description |
|-------|-------------|
| [Getting Started](getting-started.md) | Build, run, directory structure, first launch |
| [Panels](panels.md) | Create custom panels, TPanelFactory, lifecycle, OpenFloatingPanel |
| [Rooms](rooms.md) | Named workspaces, panel layout, XML persistence, room-bound panels |
| [Toolbars](toolbars.md) | Create toolbars, bind CommandManager actions, icons |
| [Menus](menus.md) | CommandManager, DVAction, menu building, panel auto-discovery |
| [Themes](themes.md) | 8 QSS themes, :TOONZCOLORS, ThemeManager, SvgIconEngine |
| [Internationalization](i18n.md) | Add languages, .ts files, QTranslator, hot-switch |
| [Docking Engine](docking.md) | DockLayout, Region tree, drag/dock/float/maximize |
| [Configuration](configuration.md) | XML config format, settings/rooms/popups/shortcuts |
| [Architecture](architecture.md) | Class hierarchy, signal flow, action system, singletons |

## Quick Reference

### Create a custom panel

```cpp
class MyFactory : public TPanelFactory {
public:
    MyFactory() : TPanelFactory("MyPanel") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("My Panel");
        panel->setWidget(new QWidget(panel));
    }
};
static MyFactory f;
static OpenFloatingPanel cmd("MI_OpenMyPanel", "MyPanel", QObject::tr("My Panel"));
```

### Add to a Room

```cpp
TPanel* p = TPanelFactory::createPanel(room, "MyPanel");
room->addDockWidget(p);
room->dockLayout()->dockItem(p, otherPanel, Region::right);
```

### Add a menu action

```cpp
cm->createAction("MI_MyAction", "My Action", "Ctrl+M");
setCommandHandler("MI_MyAction", this, &MainWindow::myMethod);
```

### Add a toolbar button

```cpp
QAction* act = CommandManager::instance()->getAction("MI_MyAction", true);
toolbar->addAction(act);
```

### Switch theme

```cpp
AppContext::instance()->setCurrentTheme("Dark");
```

### Switch language

```cpp
AppContext::instance()->setCurrentLanguage("中文");
```
