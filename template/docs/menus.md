# Menu System

## Architecture

The menu system is built on `CommandManager`, a singleton that maps string IDs to QActions and handlers.

```
CommandManager
├── m_idTable:        map<string, Node*>      // ID → node
├── m_qactionTable:   map<QAction*, Node*>    // action → node
└── m_shortcutTable:  map<string, Node*>      // shortcut → node

Node
├── m_id:        string
├── m_type:      CommandType (MenuFile, MenuView, MenuWindows, ...)
├── m_qaction:   QAction*   (the visible action)
└── m_handler:   CommandHandlerInterface* (called on execute)
```

## Defining Actions

All actions are defined in `MainWindow::defineActions()`:

```cpp
void MainWindow::defineActions() {
    CommandManager* cm = CommandManager::instance();

    // Basic action: id, name, default shortcut
    cm->createAction("MI_SaveLayout", "Save Layout", "Ctrl+S");

    // Action with handler
    setCommandHandler("MI_Quit", this, &MainWindow::close);
}
```

### CommandManager::createAction signature

```cpp
QAction* createAction(const char* id, const char* name, const char* defaultShortcut);
```

This creates a `DVAction` (auto-connects to `CommandManager::execute`) and registers it via `define()`.

## Action → Handler Chain

```
User clicks menu / presses shortcut
  → QAction::triggered()
    → DVAction::onTriggered()            // auto-connected in constructor
      → CommandManager::execute(this)    // looks up action in m_qactionTable
        → node->handler->execute()       // CommandHandlerInterface
          → MainWindow::method()         // the actual implementation
```

## Defining Custom Handlers

```cpp
// 1. Member function (void, no args)
setCommandHandler("MI_MyAction", this, &MainWindow::myMethod);

// 2. Custom handler class
struct MyHandler : CommandHandlerInterface {
    void execute() override {
        // Do something
    }
};
CommandManager::instance()->setHandler("MI_MyAction", new MyHandler());
```

The handler is owned by `CommandManager` — it will be deleted when the node is destroyed.

## Menu Bar Building

Menus are built in `StackedMenuBar::buildDefaultMenuBar()`:

```cpp
QMenuBar* bar = new QMenuBar(this);

// File menu — actions by ID
QMenu* fileMenu = bar->addMenu(tr("File"));
fileMenu->addAction(
    CommandManager::instance()->getAction("MI_SaveLayout", true));

// View menu — auto-populated from MenuWindowsCommandType panels
QMenu* viewMenu = bar->addMenu(tr("View"));
std::vector<QAction*> winActions;
CommandManager::instance()->getActions(MenuWindowsCommandType, winActions);
for (QAction* act : winActions)
    viewMenu->addAction(act);

// Settings → Theme submenu (custom, not via CommandManager)
QMenu* settingsMenu = bar->addMenu(tr("Settings"));
QMenu* themeMenu = settingsMenu->addMenu(tr("Theme"));
// ... dynamically add theme actions
```

## Panel Auto-Discovery

Panel toggle commands use `MenuWindowsCommandType`. They are auto-registered by `OpenFloatingPanel` static instances:

```cpp
// In register.cpp — runs at static init, before main()
static OpenFloatingPanel openLogPanelCmd("MI_OpenLogPanel", "LogPanel", tr("Log"));
```

The `MenuItemHandler` constructor registers with `CommandManager`, creating a `MenuWindowsCommandType` node. The View menu iterates all such nodes via `getActions(MenuWindowsCommandType, ...)`.

## Command Types

| Type | Purpose |
|------|---------|
| `MenuFileCommandType` | File menu actions |
| `MenuViewCommandType` | View menu actions |
| `MenuWindowsCommandType` | Panel toggle actions (auto-fills View menu) |
| `MenuHelpCommandType` | Help menu actions |
| `ToolCommandType` | Tool palette actions |
| `MiscCommandType` | Miscellaneous |
| `HiddenCommandType` | Hidden (no menu, shortcut only) |

## Shortcut System

User-customized shortcuts are persisted to `shortcuts.xml`:

```xml
<shortcuts>
  <shortcut id="MI_SaveLayout" value="Ctrl+S"/>
  <shortcut id="MI_Quit" value="Ctrl+Q"/>
</shortcuts>
```

`CommandManager::loadShortcuts()` is called at startup. It reads the XML and overrides default shortcuts.
