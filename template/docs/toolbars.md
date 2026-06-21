# Toolbars

## Creating a Toolbar

Toolbars are created in `MainWindow::createToolBars()`:

```cpp
QStyle* style = QApplication::style();

// Create toolbar
m_topToolBar = new QToolBar(tr("Main Toolbar"), this);
m_topToolBar->setObjectName("MainToolBar");
m_topToolBar->setMovable(false);
m_topToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
m_topToolBar->setIconSize(QSize(20, 20));

// Add actions by ID from CommandManager
QAction* saveAct = CommandManager::instance()->getAction("MI_SaveLayout", true);
saveAct->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
m_topToolBar->addAction(saveAct);

m_topToolBar->addSeparator();

// Add to MainWindow
addToolBarBreak(Qt::TopToolBarArea);      // start new line
addToolBar(Qt::TopToolBarArea, m_topToolBar);
```

## Vertical Toolbar

```cpp
m_leftToolBar = new QToolBar(tr("Panel Toolbar"), this);
m_leftToolBar->setOrientation(Qt::Vertical);   // key difference
m_leftToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

m_leftToolBar->addAction(
    CommandManager::instance()->getAction("MI_OpenCanvasPanel", true));

addToolBar(Qt::LeftToolBarArea, m_leftToolBar);
```

## Toolbar Button Styles

```cpp
toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);    // icon only
toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);    // text only
toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); // icon + text
toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);  // icon above text
```

## Binding Toolbar Actions

All toolbar actions come from `CommandManager::getAction(id)`. The action must first be defined in `MainWindow::defineActions()`:

```cpp
// In defineActions():
cm->createAction("MI_MyAction", "My Action", "Ctrl+M");

// In createToolBars():
QAction* act = CommandManager::instance()->getAction("MI_MyAction", true);
act->setIcon(style->standardIcon(QStyle::SP_ComputerIcon));
m_topToolBar->addAction(act);
```

## Setting Icons

```cpp
// Qt built-in icons
act->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));

// Theme SVG icons (via ThemeManager)
QIcon icon = createQIcon("my_icon");  // looks up "my_icon" in icon registry
act->setIcon(icon);

// Custom QPixmap icon
act->setIcon(QIcon(":/icons/my_icon.png"));

// Text-only (no icon)
act->setIcon(QIcon());
```

## Available StandardPixmap Icons (Qt)

| SP_ Value | Icon |
|-----------|------|
| `SP_DialogSaveButton` | Save |
| `SP_DialogOpenButton` | Open |
| `SP_FileDialogNewFolder` | New Folder |
| `SP_FileDialogDetailedView` | Detailed View |
| `SP_FileDialogInfoView` | Info View |
| `SP_ComputerIcon` | Computer |
| `SP_MediaPlay` | Play |
| `SP_MessageBoxInformation` | Info |
| `SP_TitleBarContextHelpButton` | Help |
