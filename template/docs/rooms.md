# Room Configuration

## Overview

A Room is a named workspace containing a set of docked panels. Rooms are managed by `MainWindow` via `QStackedWidget` — one Room is visible at a time, switched via the `RoomTabWidget` tabs in the `TopBar`.

## Room XML Format

Rooms are persisted to `rooms/<Name>.xml`. Example:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<room name="Edit">
  <panels>
    <panel index="0" name="Canvas" x="0" y="0" w="800" h="500"/>
    <panel index="1" name="PropertyInspector" x="800" y="0" w="300" h="500"/>
    <panel index="2" name="CommandPalette" x="0" y="500" w="1100" h="200"/>
  </panels>
  <hierarchy>(0,(1,2))</hierarchy>
</room>
```

- `name` — panel type string (matches TPanelFactory key)
- `x, y, w, h` — geometry within the room
- `hierarchy` — DockLayout tree string: `(parent,(child1,child2))`

## Creating a Room

In `MainWindow::createDefaultRooms()`:

```cpp
// Create a new Room
Room* room = new Room(this);
room->setName("MyRoom");              // displayed in tab + used as .xml filename

// Create and dock panels
DockLayout* layout = room->dockLayout();

TPanel* viewer = TPanelFactory::createPanel(room, "Canvas");
room->addDockWidget(viewer);
layout->dockItem(viewer);             // dock in root region

TPanel* props = TPanelFactory::createPanel(room, "PropertyInspector");
room->addDockWidget(props);
layout->dockItem(props, viewer, Region::right);  // dock to the right of viewer

TPanel* cmd = TPanelFactory::createPanel(room, "CommandPalette");
room->addDockWidget(cmd);
layout->dockItem(cmd, viewer, Region::bottom);   // dock below viewer

// Add to MainWindow
m_stackedWidget->addWidget(room);
m_topBar->getRoomTabWidget()->addTab("MyRoom");
```

## Room API

```cpp
class Room : public TMainWindow {
    Q_OBJECT
public:
    void save();                        // Save layout to rooms/<name>.xml
    void load(const QString& path);     // Load layout from XML
    void initialize();                  // load() with lazy-load flag

    QString getName() const;
    void setName(const QString& name);
};
```

Layout persistence is automatic — `MainWindow::writeSettings()` calls `room->save()` for all rooms on close.

## Room-Bound Panels

Panels can be bound to a specific Room so they auto-hide when switching to another Room:

1. **Right-click** the panel's title bar
2. Select **"Bind to Current Room"**
3. The panel will now only appear in that Room
4. Right-click again → **"Unbind from Room"** to restore global visibility

This is managed by `MainWindow::updatePanelVisibility()` which runs on every room switch:

```cpp
void MainWindow::updatePanelVisibility() {
    for each room:
        for each panel in room:
            if (panel->isRoomBound()):
                if (panel->boundRoomName == currentRoomName):  show()
                else:                                           hide()
```

## Adding Room Tabs Programmatically

```cpp
// Add new room
Room* room = new Room(this);
room->setName("New Room");
addRoom(room);  // adds to stackedWidget + tab bar

// Delete room (cannot delete last room)
deleteRoom(index);

// Rename room
renameRoom(index, "New Name");
```
