# Docking Engine

## Overview

The docking system is a custom `QLayout` subclass (`DockLayout`) that manages a tree of `Region` nodes. This replaces Qt's `QDockWidget` entirely, providing drag-to-dock, float, resize, and maximize.

## Core Classes

| Class | File | Role |
|-------|------|------|
| `DockLayout` | `docklayout.h` | QLayout subclass, root of docking tree |
| `DockWidget` | `docklayout.h` | QFrame subclass, draggable/floating/dockable base |
| `Region` | `docklayout.h` | Rectangular space in the layout tree |
| `DockSeparator` | `docklayout.h` | Resize handle between docked panels |
| `DockPlaceholder` | `docklayout.h` | Drop target widget during drag |
| `DockDecoAllocator` | `docklayout.h` | Factory for separators and placeholders |
| `TDockWidget` | `tdockwindows.h` | DockWidget + title bar + content widget |
| `TMainWindow` | `tdockwindows.h` | QWidget that owns a DockLayout |

## Region Tree

The layout is a binary tree of regions:

```
Root Region (horizontal)
├── Region A: DockWidget "Canvas"
└── Region B (vertical)
    ├── Region C: DockWidget "PropertyInspector"
    └── Region D: DockWidget "CommandPalette"
```

Each Region is either a **leaf** (contains a DockWidget) or a **branch** (contains child Regions). Branches have an orientation (horizontal/vertical) and DockSeparators between children.

The `hierarchy` string `(0,(1,2))` represents this tree structure.

## Docking Operations

### Docking a Widget

```cpp
DockLayout* layout = room->dockLayout();

// Dock in root region (first widget)
layout->dockItem(panel);

// Dock relative to another widget
layout->dockItem(panel, targetPanel, Region::right);   // Region::left|right|top|bottom
```

### Undocking (Float)

```cpp
layout->undockItem(panel);    // removes from layout
panel->setFloating(true);     // shows as floating window
```

### Drag & Drop Re-docking

1. User starts dragging a `DockWidget` title bar
2. `DockLayout::calculateDockPlaceholders()` creates drop target widgets
3. Green highlight regions appear at valid docking positions
4. User drops → `DockLayout::dockItem(item, placeholder)`
5. If dropped outside any placeholder → panel floats

### Maximize

Press backtick (`` ` ``) to maximize the panel under cursor. This calls:

```cpp
MainWindow::maximizePanel()
  └── layout->containerOf(QCursor::pos())   // find widget under cursor
        └── dw->maximizeDock()              // fill the room
```

Press again to restore. Only panels with `setIsMaximizable(true)` respond to maximize.

### Lock Docking

Check "Lock" in the TopBar to prevent accidental dragging. This sets `DockingCheck::setIsEnabled(false)`, which gates all drag operations.

## TDockWidget (Convenience Layer)

`TDockWidget` adds a title bar and content widget to the raw `DockWidget`:

```cpp
TDockWidget::TDockWidget(parent, flags)
  ├── QBoxLayout
  │   ├── m_titlebar (QWidget, typically TPanelTitleBar)
  │   └── m_widget (content, set via setWidget())
  ...

void TDockWidget::setWidget(QWidget* widget);       // set content
void TDockWidget::setTitleBarWidget(QWidget* bar);  // set custom title bar
void TDockWidget::setFloating(bool);                // float/dock
void TDockWidget::setMaximized(bool);               // maximize/restore
```

## Save & Restore Layout

```cpp
// Save: serialize region tree + panel positions
DockLayout::State state = layout->saveState();
// state.first  = vector<QRect> panel geometries
// state.second = QString hierarchy e.g. "(0,(1,2))"

// Restore: reconstruct region tree
layout->restoreState(state);
```

Room save/load uses this + panel type names in XML format.

## Docking Lock

```cpp
DockingCheck* check = DockingCheck::instance();
check->setIsEnabled(false);  // lock all dock layouts
check->setIsEnabled(true);   // unlock
bool locked = !check->isEnabled();
```
