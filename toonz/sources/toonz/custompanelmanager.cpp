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

#include <QUiLoader>
#include <QAbstractButton>
#include <QList>
#include <QToolButton>
#include <QPainter>
#include <QMouseEvent>

namespace {

const TFilePath CustomPanelFolderName("custompanels");
const TFilePath customPaneFolderPath() {
  return ToonzFolder::getMyModuleDir() + CustomPanelFolderName;
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
// browse the custom panel settings and regisiter to the menu
void CustomPanelManager::loadCustomPanelEntries() {
  QAction* menuAct = CommandManager::instance()->getAction(MI_OpenCustomPanels);
  if (!menuAct) return;
  DVMenuAction* menu = dynamic_cast<DVMenuAction*>(menuAct->menu());
  if (!menu) return;

  if (!menu->isEmpty()) menu->clear();

  TFilePath customPanelsFolder = customPaneFolderPath();
  if (TSystem::doesExistFileOrLevel(customPanelsFolder)) {
    TFilePathSet fileList =
        TSystem::readDirectory(customPanelsFolder, false, true, false);
    if (!fileList.empty()) {
      QList<QString> fileNames;
      for (auto file : fileList) {
        // accept only .ui files
        if (file.getType() != "ui") continue;
        fileNames.append(QString::fromStdString(file.getName()));
      }
      if (!fileNames.isEmpty()) {
        menu->setActions(fileNames);
        menu->addSeparator();
      }
    }
  }
  // register an empty action with the label "Custom Panel Editor".
  // actual command will be called in OpenCustomPanelCommandHandler
  // in order to prevent double calling of the command
  menu->addAction(
      CommandManager::instance()->getAction(MI_CustomPanelEditor)->text());
  
  registerCustomPanelCommands();
  
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
  TPanel* panel     = new TPanel(parent);
  QString panelType = "Custom_" + panelName;
  panel->setPanelType(panelType.toStdString());
  panel->setObjectName(panelType);

  TFilePath customPanelsFp =
      customPaneFolderPath() + TFilePath(panelName + ".ui");
  QUiLoader loader;
  QFile file(customPanelsFp.getQString());

  file.open(QFile::ReadOnly);
  QWidget* customWidget = loader.load(&file, panel);
  file.close();

  initializeControl(customWidget);

  panel->setWindowTitle(panelName);
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

  TFilePathSet fileList =
      TSystem::readDirectory(customPanelsFolder, false, true, false);
  
  QList<QString> currentPanels;
  for (auto file : fileList) {
    if (file.getType() != "ui") continue;
    currentPanels.append(QString::fromStdString(file.getName()));
  }
  
  m_registeredPanelIds.clear();

  MainWindow* mainWindow = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (!mainWindow) return;

  for (const QString& panelName : currentPanels) {
    QString commandId = "MI_CustomPanel_" + panelName;
    
    QAction* existingAction = CommandManager::instance()->getAction(
        commandId.toStdString().c_str(), false);
    if (existingAction) {
      m_registeredPanelIds.append(commandId);
      continue;
    }

    // Improved display name for better searchability
    // Replace underscores and hyphens with spaces for readability
    QString cleanName = panelName;
    cleanName.replace('_', ' ');
    cleanName.replace('-', ' ');
    
    // Create display name with [Panel] prefix
    QString displayName = "[Panel] " + cleanName;
    
    QAction* action = new DVAction(displayName, mainWindow);
    mainWindow->addAction(action);
    
    // Define command with improved naming for search
    // Add searchable keywords in the command definition
    CommandManager::instance()->define(
        commandId.toStdString().c_str(),
        CustomPanelCommandType,  // Custom Panels subcategory under Windows
        "",
        action,
        "");

    class CustomPanelHandler : public CommandHandlerInterface {
      QString m_panelName;
    public:
      CustomPanelHandler(const QString& name) : m_panelName(name) {}
      void execute() override {
        TMainWindow* currentRoom = TApp::instance()->getCurrentRoom();
        if (!currentRoom) return;
        
        std::string panelType = ("Custom_" + m_panelName).toStdString();
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
        new CustomPanelHandler(panelName));

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
    int index          = menu->getTriggeredActionIndex();

    // the last action is for opening custom panel editor, in which the index is
    // not set.
    if (index == -1) {
      CommandManager::instance()->getAction(MI_CustomPanelEditor)->trigger();
      return;
    }

    QString panelId = menu->actions()[index]->text();

    OpenFloatingPanel::getOrOpenFloatingPanel("Custom_" +
                                              panelId.toStdString());
  }
} openCustomPanelCommandHandler;
