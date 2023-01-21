#include "custompanelmanager.h"

#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "pane.h"

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

  return panel;
}

//-----------------------------------------------------------------------------

void CustomPanelManager::initializeControl(QWidget* customWidget) {
  // connect buttons and commands
  QList<QAbstractButton*> allButtons =
      customWidget->findChildren<QAbstractButton*>();
  for (auto button : allButtons) {
    std::cout << button->objectName().toStdString() << std::endl;
    QAction* action = CommandManager::instance()->getAction(
        button->objectName().toStdString().c_str());
    if (!action) continue;

    CommandManager::instance()->enlargeIcon(
        button->objectName().toStdString().c_str(), button->iconSize());

    if (QToolButton* tb = dynamic_cast<QToolButton*>(button)) {
      tb->setDefaultAction(action);
      tb->setObjectName("CustomPanelButton");
      if (tb->toolButtonStyle() == Qt::ToolButtonTextUnderIcon) {
        int padding = (tb->height() - button->iconSize().height() -
                       tb->font().pointSize() * 1.33) /
                      3;
        if (padding > 0)
          tb->setStyleSheet(QString("padding-top: %1;").arg(padding));
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
      lay->setMargin(0);
      lay->setSpacing(0);
      lay->addWidget(customControl);
      widget->setLayout(lay);
    }
  }
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
