# Panel Development

## Overview

A Panel is a dockable widget container. Panels are the primary UI building block — they can be docked, floated, maximized, closed/restored, and bound to specific Rooms.

## Creating a Panel

### Step 1: Write the Widget

Create your content widget as a plain QWidget subclass:

```cpp
// mypanel.h
#pragma once
#include <QWidget>

class MyPanel : public QWidget {
    Q_OBJECT
public:
    explicit MyPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 100);
        // Build your UI here
    }
};
```

### Step 2: Register with TPanelFactory

In `src/panels/register.cpp` (or your own file), add a static factory:

```cpp
#include "../framework/pane.h"
#include "mypanel.h"

class MyPanelFactory : public TPanelFactory {
public:
    MyPanelFactory() : TPanelFactory("MyPanel") {}  // "MyPanel" = unique type name
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("My Panel");
        panel->setWidget(new MyPanel(panel));
        panel->setIsMaximizable(true);      // allow backtick maximize
        panel->allowMultipleInstances(false); // only one instance
    }
};
static MyPanelFactory myPanelFactoryInstance;  // auto-registers at static init
```

### Step 3: Register OpenFloatingPanel Command

Add a static command so the panel appears in the View menu:

```cpp
#include "../framework/floatingpanelcommand.h"

static OpenFloatingPanel openMyPanelCmd(
    "MI_OpenMyPanel",   // command ID (must be unique)
    "MyPanel",          // matches TPanelFactory type name
    QObject::tr("My Panel")  // display name
);
```

### Step 4: Register in MainWindow

In `MainWindow::defineActions()`:

```cpp
cm->createAction("MI_OpenMyPanel", "My Panel", "");
```

## Panel Lifecycle

| Event | Method | Behavior |
|-------|--------|----------|
| Created | `TPanelFactory::initialize()` | Sets widget, title, maximizable flag |
| Clicked in View menu | `OpenFloatingPanel::execute()` | Finds hidden → restores, or creates new floating |
| Close (X button) | `TPanel::onCloseButtonPressed()` | Hides panel, removes from layout |
| Maximize (backtick) | `MainWindow::maximizePanel()` | Finds panel under cursor → `DockWidget::maximizeDock()` |
| Room switch | `MainWindow::updatePanelVisibility()` | Shows/hides based on room binding |
| Floating geometry saved | `TPanel::~TPanel()` | Saves to `popups.xml` |
| Floating geometry restored | `TPanel::restoreFloatingPanelState()` | Reads from `popups.xml` |

## Adding the Panel to a Room

In `MainWindow::createDefaultRooms()`:

```cpp
TPanel* myPanel = TPanelFactory::createPanel(room, "MyPanel");
room->addDockWidget(myPanel);
room->dockLayout()->dockItem(myPanel, existingPanel, Region::right);
```

`Region::dockItem()` position options: `Region::left`, `Region::right`, `Region::top`, `Region::bottom`.

## Panel API Reference

### TPanel (inherits TDockWidget)

```cpp
// Identification
void setPanelType(const std::string& type);
std::string getPanelType() const;

// Maximize
void setIsMaximizable(bool);
bool isMaximizable() const;

// Multiple instances
void allowMultipleInstances(bool);
bool areMultipleInstancesAllowed() const;

// Title bar
TPanelTitleBar* getTitleBar() const;

// Room binding (right-click title bar)
bool isRoomBound() const;
void setRoomBound(bool);
QString getBoundRoomName() const;
void setBoundRoomName(const QString&);

// Virtual overrides
virtual void reset();                         // Called when panel is restored
virtual int getViewType();                    // View mode (if applicable)
virtual void setViewType(int);
virtual bool widgetInThisPanelIsFocused();    // Check if content has focus
```

### TPanelFactory

```cpp
class MyFactory : public TPanelFactory {
public:
    MyFactory() : TPanelFactory("TypeName") {}
    void initialize(TPanel* panel) override;  // OR override createPanel()
    TPanel* createPanel(QWidget* parent) override;
};
```

### OpenFloatingPanel

```cpp
// Static instance auto-registers with CommandManager
OpenFloatingPanel(CommandId id, const std::string& panelType, QString title);

// Manually open a panel
TPanel* OpenFloatingPanel::getOrOpenFloatingPanel(const std::string& panelType);
```
