#include "custompanelmanager.h"

#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "pane.h"
#include "mainwindow.h"
#include "shortcutpopup.h"
#include "tapp.h"
#include "toolpresetcommandmanager.h"

// ToonzLib
#include "toonz/toonzfolders.h"
// ToonzCore
#include "tsystem.h"
#include "tfilepath.h"

#include <QUiLoader>
#include <QAbstractButton>
#include <QList>
#include <QToolButton>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QFileInfo>
#include <algorithm>

namespace {

const TFilePath CustomPanelFolderName("custompanels");
const TFilePath customPaneFolderPath() {
  return ToonzFolder::getMyModuleDir() + CustomPanelFolderName;
}

// Keep in sync with Custom Panel Editor template tree depth.
const int kMaxCustomPanelFolderDepth = 3;

QString normalizePanelId(QString id) {
  id.replace('\\', '/');
  while (id.startsWith('/')) id.remove(0, 1);
  return id;
}

QString commandIdFromPanelId(const QString& panelId) {
  return "MI_CustomPanel_" + QString(panelId).replace('/', "__");
}

QString panelTypeFromPanelId(const QString& panelId) {
  return "Custom_" + QString(panelId).replace('/', "__");
}

QString relativePanelId(const TFilePath& file, const TFilePath& rootFolder) {
  TFilePath relative = file - rootFolder;
  return normalizePanelId(relative.withType("").getQString());
}

TFilePath panelFilePath(const QString& panelId) {
  TFilePath path              = customPaneFolderPath();
  const QStringList parts     = normalizePanelId(panelId).split('/', Qt::SkipEmptyParts);
  for (int i = 0; i < parts.size(); ++i) {
    if (i == parts.size() - 1)
      path = path + TFilePath(parts[i] + ".ui");
    else
      path = path + TFilePath(parts[i]);
  }
  return path;
}

void collectUiPanelIds(const TFilePath& folder, const TFilePath& rootFolder,
                       int depth, QStringList& outIds) {
  if (!TSystem::doesExistFileOrLevel(folder)) return;

  TFilePathSet entries = TSystem::readDirectory(folder, false, false, false);

  QList<TFilePath> dirs;
  QList<TFilePath> files;
  for (const auto& entry : entries) {
    if (TFileStatus(entry).isDirectory())
      dirs.append(entry);
    else if (entry.getType() == "ui")
      files.append(entry);
  }

  std::sort(dirs.begin(), dirs.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });
  std::sort(files.begin(), files.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });

  if (depth < kMaxCustomPanelFolderDepth) {
    for (const auto& dir : dirs)
      collectUiPanelIds(dir, rootFolder, depth + 1, outIds);
  }

  for (const auto& file : files)
    outIds.append(relativePanelId(file, rootFolder));
}

void populateCustomPanelMenu(QMenu* menu, const TFilePath& folder, int depth,
                             QStringList& menuPanelIds) {
  if (!menu || !TSystem::doesExistFileOrLevel(folder)) return;

  TFilePathSet entries = TSystem::readDirectory(folder, false, false, false);

  QList<TFilePath> dirs;
  QList<TFilePath> files;
  for (const auto& entry : entries) {
    if (TFileStatus(entry).isDirectory())
      dirs.append(entry);
    else if (entry.getType() == "ui")
      files.append(entry);
  }

  std::sort(dirs.begin(), dirs.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });
  std::sort(files.begin(), files.end(),
            [](const TFilePath& a, const TFilePath& b) {
              return QString::compare(QString::fromStdString(a.getName()),
                                     QString::fromStdString(b.getName()),
                                     Qt::CaseInsensitive) < 0;
            });

  if (depth < kMaxCustomPanelFolderDepth) {
    for (const auto& dir : dirs) {
      QMenu* subMenu =
          new QMenu(QString::fromStdString(dir.getName()), menu);
      populateCustomPanelMenu(subMenu, dir, depth + 1, menuPanelIds);
      if (subMenu->isEmpty()) {
        delete subMenu;
        continue;
      }
      menu->addMenu(subMenu);
    }
  }

  for (const auto& file : files) {
    // Proxy menu actions (not the CommandManager actions): a QAction can only
    // belong to one menu, and command actions must stay available for shortcuts.
    QString panelId = relativePanelId(file, customPaneFolderPath());
    QAction* leaf =
        menu->addAction(QString::fromStdString(file.getName()));
    leaf->setData(menuPanelIds.size());
    menuPanelIds.append(panelId);
  }
}

}  // namespace

//-----------------------------------------------------------------------------

MyScroller::MyScroller(Qt::Orientation orientation, CommandId command1,
                       CommandId command2, QWidget* parent)
    : QWidget(parent), m_orientation(orientation) {
  m_actions[0] = CommandManager::instance()->getAction(command1);
  m_actions[1] = CommandManager::instance()->getAction(command2);
}

void MyScroller::paintEvent(QPaintEvent*) {
  QPainter p(this);

  p.setPen(m_scrollerBorderColor);
  p.setBrush(m_scrollerBGColor);

  p.drawRect(rect().adjusted(0, 0, -1, -1));

  if (m_orientation == Qt::Horizontal) {
    for (int i = 1; i <= 7; i++) {
      int xPos = width() * i / 8;
      p.drawLine(xPos, 0, xPos, height());
    }
  } else {  // vertical
    for (int i = 1; i <= 7; i++) {
      int yPos = height() * i / 8;
      p.drawLine(0, yPos, width(), yPos);
    }
  }
}

void MyScroller::mousePressEvent(QMouseEvent* event) {
  m_anchorPos =
      (m_orientation == Qt::Horizontal) ? event->pos().x() : event->pos().y();
}

void MyScroller::mouseMoveEvent(QMouseEvent* event) {
  int currentPos =
      (m_orientation == Qt::Horizontal) ? event->pos().x() : event->pos().y();
  static int threshold = 5;
  if (m_anchorPos - currentPos >= threshold && m_actions[0]) {
    m_actions[0]->trigger();
    m_anchorPos = currentPos;
  } else if (currentPos - m_anchorPos >= threshold && m_actions[1]) {
    m_actions[1]->trigger();
    m_anchorPos = currentPos;
  }
}

//-----------------------------------------------------------------------------

CustomPanelManager* CustomPanelManager::instance() {
  static CustomPanelManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

QString CustomPanelManager::menuPanelIdAt(int index) const {
  if (index < 0 || index >= m_menuPanelIds.size()) return QString();
  return m_menuPanelIds.at(index);
}

//-----------------------------------------------------------------------------
// browse the custom panel settings and regisiter to the menu
void CustomPanelManager::loadCustomPanelEntries() {
  QAction* menuAct = CommandManager::instance()->getAction(MI_OpenCustomPanels);
  if (!menuAct) return;
  DVMenuAction* menu = dynamic_cast<DVMenuAction*>(menuAct->menu());
  if (!menu) return;

  if (!menu->isEmpty()) menu->clear();
  m_menuPanelIds.clear();

  registerCustomPanelCommands();

  TFilePath customPanelsFolder = customPaneFolderPath();
  if (TSystem::doesExistFileOrLevel(customPanelsFolder)) {
    populateCustomPanelMenu(menu, customPanelsFolder, 0, m_menuPanelIds);
    if (!menu->isEmpty()) menu->addSeparator();
  }

  // Text-only action: OpenCustomPanelCommandHandler opens the editor when
  // index is unset. Do not add the real MI_CustomPanelEditor action here —
  // QAction can only live in one menu at a time.
  menu->addAction(
      CommandManager::instance()->getAction(MI_CustomPanelEditor)->text());

  // Register tool presets as commands for Custom Panels
  ToolPresetCommandManager::instance()->registerToolPresetCommands();

  // Register universal size commands
  ToolPresetCommandManager::instance()->registerSizeCommands();

  // Initialize signal connections for automatic checked state updates
  ToolPresetCommandManager::instance()->initialize();
}

//-----------------------------------------------------------------------------

TPanel* CustomPanelManager::createCustomPanel(const QString panelName,
                                              QWidget* parent) {
  // Panel types encode folder separators as "__" (see panelTypeFromPanelId).
  QString panelId = normalizePanelId(panelName);
  panelId.replace("__", "/");
  TPanel* panel     = new TPanel(parent);
  QString panelType = panelTypeFromPanelId(panelId);
  panel->setPanelType(panelType.toStdString());
  panel->setObjectName(panelType);

  TFilePath customPanelsFp = panelFilePath(panelId);
  QUiLoader loader;
  QFile file(customPanelsFp.getQString());

  file.open(QFile::ReadOnly);
  QWidget* customWidget = loader.load(&file, panel);
  file.close();

  initializeControl(customWidget);

  panel->setWindowTitle(QFileInfo(customPanelsFp.getQString()).completeBaseName());
  panel->setWidget(customWidget);

  // Enable room binding feature (handled by TPanel base class)
  panel->addRoomBindButton();

  return panel;
}

//-----------------------------------------------------------------------------

void CustomPanelManager::initializeControl(QWidget* customWidget) {
  // Update checked states before initializing controls
  ToolPresetCommandManager::instance()->updateCheckedStates();
  
  // connect buttons and commands
  QList<QAbstractButton*> allButtons =
      customWidget->findChildren<QAbstractButton*>();
  for (auto button : allButtons) {
    QAction* action = CommandManager::instance()->getAction(
        button->objectName().toStdString().c_str());
    if (!action) continue;

    CommandManager::instance()->enlargeIcon(
        button->objectName().toStdString().c_str(), button->iconSize());

    if (QToolButton* tb = dynamic_cast<QToolButton*>(button)) {
      tb->setDefaultAction(action);
      tb->setObjectName("CustomPanelButton");
      
      // Build stylesheet with visible checked state
      QString styleSheet;
      
      if (tb->toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
        int padding = (tb->height() - button->iconSize().height() -
                       tb->font().pointSize() * 1.33) /
                      3;
        if (padding > 0) {
          styleSheet = QString("QToolButton#CustomPanelButton { padding-top: %1; }").arg(padding);
        }
      }
      
      // Add checked state styles ONLY for new commands
      // (Brush Presets and Universal Sizes)
      std::string cmdId = button->objectName().toStdString();
      bool isBrushPreset = (cmdId.find("MI_BrushPreset_") == 0);
      bool isUniversalSize = (cmdId.find("MI_ToolSize_") == 0);
      
      // No custom stylesheet - let buttons inherit colors from the application theme
      // This ensures proper adaptation when the user switches themes
      if (!styleSheet.isEmpty()) {
        tb->setStyleSheet(styleSheet);
      }
      
      continue;
    }

    if (action->isCheckable()) {
      button->setCheckable(true);
      button->setChecked(action->isChecked());
      customWidget->connect(button, SIGNAL(clicked(bool)), action,
                            SLOT(setChecked(bool)));
      customWidget->connect(action, SIGNAL(toggled(bool)), button,
                            SLOT(setChecked(bool)));
    } else {
      customWidget->connect(button, SIGNAL(clicked(bool)), action,
                            SLOT(trigger()));
    }
    if (!button->text().isEmpty()) button->setText(action->text());

    button->setIcon(action->icon());
    
    // No custom stylesheet for checked buttons - let them inherit from theme
    // button->addAction(action);
  }

  // other custom controls
  QList<QWidget*> allWidgets = customWidget->findChildren<QWidget*>();
  for (auto widget : allWidgets) {
    // ignore buttons
    if (dynamic_cast<QAbstractButton*>(widget)) continue;
    // ignore if the widget already has a layout
    if (widget->layout() != nullptr) continue;

    QString name           = widget->objectName();
    QWidget* customControl = nullptr;
    if (name.startsWith("HScroller")) {
      QStringList ids = name.split("__");
      if (ids.size() != 3 || ids[0] != "HScroller") continue;
      customControl =
          new MyScroller(Qt::Horizontal, ids[1].toStdString().c_str(),
                         ids[2].toStdString().c_str(), customWidget);
    } else if (name.startsWith("VScroller")) {
      QStringList ids = name.split("__");
      if (ids.size() != 3 || ids[0] != "VScroller") continue;
      customControl =
          new MyScroller(Qt::Vertical, ids[1].toStdString().c_str(),
                         ids[2].toStdString().c_str(), customWidget);
    }

    if (customControl) {
      QHBoxLayout* lay = new QHBoxLayout();
      lay->setContentsMargins(0, 0, 0, 0);
      lay->setSpacing(0);
      lay->addWidget(customControl);
      widget->setLayout(lay);
    }
  }
}

//-----------------------------------------------------------------------------

void CustomPanelManager::registerCustomPanelCommands() {
  TFilePath customPanelsFolder = customPaneFolderPath();
  if (!TSystem::doesExistFileOrLevel(customPanelsFolder)) return;

  QStringList currentPanels;
  collectUiPanelIds(customPanelsFolder, customPanelsFolder, 0, currentPanels);

  m_registeredPanelIds.clear();

  MainWindow* mainWindow = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (!mainWindow) return;

  for (const QString& panelId : currentPanels) {
    QString commandId = commandIdFromPanelId(panelId);

    QAction* existingAction = CommandManager::instance()->getAction(
        commandId.toStdString().c_str(), false);
    if (existingAction) {
      existingAction->setVisible(true);
      m_registeredPanelIds.append(commandId);
      continue;
    }

    // Display / search name: basename only (folder context is in the menu path)
    QString baseName = panelId.section('/', -1);
    baseName.replace('_', ' ');
    baseName.replace('-', ' ');
    QString displayName = "[Panel] " + baseName;

    QAction* action = new DVAction(displayName, mainWindow);
    mainWindow->addAction(action);

    // Define command with improved naming for search
    CommandManager::instance()->define(
        commandId.toStdString().c_str(),
        CustomPanelCommandType,  // Custom Panels subcategory under Windows
        "",
        action,
        "");

    class CustomPanelHandler : public CommandHandlerInterface {
      QString m_panelId;
    public:
      CustomPanelHandler(const QString& id) : m_panelId(id) {}
      void execute() override {
        TMainWindow* currentRoom = TApp::instance()->getCurrentRoom();
        if (!currentRoom) return;

        std::string panelType = panelTypeFromPanelId(m_panelId).toStdString();
        QList<TPanel*> panels = currentRoom->findChildren<TPanel*>();

        for (TPanel* panel : panels) {
          if (panel->getPanelType() == panelType && !panel->isHidden()) {
            panel->close();
            return;
          }
        }

        OpenFloatingPanel::getOrOpenFloatingPanel(panelType);
      }
    };

    CommandManager::instance()->setHandler(
        commandId.toStdString().c_str(),
        new CustomPanelHandler(panelId));

    m_registeredPanelIds.append(commandId);
  }

  // Notify ShortcutPopup to refresh if it's currently open
  // This allows new custom panel commands to appear immediately without restarting
  ShortcutPopup::refreshIfOpen();

  TFilePath shortcutsFile = ToonzFolder::getMyModuleDir() + TFilePath("shortcuts.ini");
  if (!TFileStatus(shortcutsFile).doesExist()) return;

  QSettings settings(toQString(shortcutsFile), QSettings::IniFormat);
  settings.beginGroup("shortcuts");

  for (const QString& commandId : m_registeredPanelIds) {
    QString savedShortcut = settings.value(commandId, "").toString();
    if (!savedShortcut.isEmpty()) {
      QAction* action = CommandManager::instance()->getAction(
          commandId.toStdString().c_str(), false);
      if (action) {
        action->setShortcut(QKeySequence(savedShortcut));
      }
    }
  }

  settings.endGroup();
}

//-----------------------------------------------------------------------------

class OpenCustomPanelCommandHandler final : public MenuItemHandler {
public:
  OpenCustomPanelCommandHandler() : MenuItemHandler(MI_OpenCustomPanels) {}
  void execute() override {
    QAction* act = CommandManager::instance()->getAction(MI_OpenCustomPanels);
    DVMenuAction* menu = dynamic_cast<DVMenuAction*>(act->menu());
    if (!menu) return;

    int index = menu->getTriggeredActionIndex();

    // Editor entry (and any action without panel index data)
    if (index == -1) {
      CommandManager::instance()->getAction(MI_CustomPanelEditor)->trigger();
      return;
    }

    QString panelId = CustomPanelManager::instance()->menuPanelIdAt(index);
    if (panelId.isEmpty()) {
      CommandManager::instance()->getAction(MI_CustomPanelEditor)->trigger();
      return;
    }

    OpenFloatingPanel::getOrOpenFloatingPanel(
        panelTypeFromPanelId(panelId).toStdString());
  }
} openCustomPanelCommandHandler;
