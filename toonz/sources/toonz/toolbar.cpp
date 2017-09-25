

#include "toolbar.h"
#include "tapp.h"
#include "pane.h"
#include "floatingpanelcommand.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 0);

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

Toolbar::Toolbar(QWidget *parent, bool isVertical)
    : QToolBar(parent), m_isExpanded(ShowAllToolsToggle != 0) {
  // Fondamentale per lo style sheet
  setObjectName("toolBar");

  setMovable(false);
  if (isVertical)
    setOrientation(Qt::Vertical);
  else
    setOrientation(Qt::Horizontal);

  setIconSize(QSize(23, 23));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  bool actionAdded = addAction(CommandManager::instance()->getAction(T_Edit));
  actionAdded = addAction(CommandManager::instance()->getAction(T_Selection)) ||
                actionAdded;
  if (actionAdded) addSeparator();
  actionAdded = false;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Brush)) || actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Geometric)) ||
                actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Type)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Fill)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_PaintBrush)) ||
      actionAdded;
  if (actionAdded) addSeparator();
  actionAdded = false;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Eraser)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Tape)) || actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Finger));
  if (actionAdded) addSeparator();
  actionAdded = false;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_StylePicker)) ||
      actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_RGBPicker)) ||
                actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Ruler));
  if (actionAdded) addSeparator();
  actionAdded = false;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_ControlPointEditor)) ||
      actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Pinch)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Pump)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Magnet)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Bender)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Iron)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Cutter)) || actionAdded;
  if (actionAdded) m_sep1 = addSeparator();
  actionAdded             = false;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Skeleton)) ||
                actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Hook)) || actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Tracker)) ||
                actionAdded;
  actionAdded = addAction(CommandManager::instance()->getAction(T_Plastic)) ||
                actionAdded;
  if (actionAdded) m_sep2 = addSeparator();
  actionAdded             = false;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Zoom)) || actionAdded;
  if (actionAdded)
    CommandManager::instance()->getAction(T_Zoom)->setChecked(true);
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Rotate)) || actionAdded;
  actionAdded =
      addAction(CommandManager::instance()->getAction(T_Hand)) || actionAdded;

  m_expandButton = new QToolButton(this);
  m_expandButton->setObjectName("expandButton");
  m_expandButton->setCheckable(true);
  m_expandButton->setChecked(m_isExpanded);
  m_expandButton->setArrowType((isVertical) ? Qt::DownArrow : Qt::RightArrow);

  addWidget(m_expandButton);

  // toolbar is expanded or shrinked according to env at the beginning
  updateToolbar();

  connect(m_expandButton, SIGNAL(toggled(bool)), this,
          SLOT(setIsExpanded(bool)));
}

//-----------------------------------------------------------------------------
/*! Layout the tool buttons according to the state of the expandButton
*/
void Toolbar::updateToolbar() {
  if (m_isExpanded) {
    insertAction(CommandManager::instance()->getAction(T_Fill),
                 CommandManager::instance()->getAction(T_Type));
    insertAction(CommandManager::instance()->getAction(T_Hand),
                 CommandManager::instance()->getAction(T_Rotate));
    insertAction(m_sep2, CommandManager::instance()->getAction(T_Plastic));
    insertAction(CommandManager::instance()->getAction(T_Plastic),
                 CommandManager::instance()->getAction(T_Hook));
    insertAction(CommandManager::instance()->getAction(T_Hook),
                 CommandManager::instance()->getAction(T_Tracker));
    insertAction(CommandManager::instance()->getAction(T_Tracker),
                 CommandManager::instance()->getAction(T_Skeleton));
    insertAction(CommandManager::instance()->getAction(T_Skeleton), m_sep1);
    insertAction(m_sep1, CommandManager::instance()->getAction(T_Cutter));
    insertAction(CommandManager::instance()->getAction(T_Cutter),
                 CommandManager::instance()->getAction(T_Iron));
    insertAction(CommandManager::instance()->getAction(T_Iron),
                 CommandManager::instance()->getAction(T_Bender));
    insertAction(CommandManager::instance()->getAction(T_Bender),
                 CommandManager::instance()->getAction(T_Magnet));
    insertAction(CommandManager::instance()->getAction(T_Magnet),
                 CommandManager::instance()->getAction(T_Pump));
    insertAction(CommandManager::instance()->getAction(T_Pump),
                 CommandManager::instance()->getAction(T_Pinch));

    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::UpArrow : Qt::LeftArrow);

  } else {
    removeAction(CommandManager::instance()->getAction(T_Type));
    removeAction(CommandManager::instance()->getAction(T_Pinch));
    removeAction(CommandManager::instance()->getAction(T_Pump));
    removeAction(CommandManager::instance()->getAction(T_Magnet));
    removeAction(CommandManager::instance()->getAction(T_Bender));
    removeAction(CommandManager::instance()->getAction(T_Iron));
    removeAction(CommandManager::instance()->getAction(T_Cutter));
    removeAction(CommandManager::instance()->getAction(T_Skeleton));
    removeAction(CommandManager::instance()->getAction(T_Tracker));
    removeAction(CommandManager::instance()->getAction(T_Hook));
    removeAction(CommandManager::instance()->getAction(T_Plastic));
    removeAction(CommandManager::instance()->getAction(T_Rotate));
    removeAction(m_sep1);
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::DownArrow : Qt::RightArrow);
  }
  update();
}

//----------------------------------------------------------------------------

void Toolbar::setIsExpanded(bool expand) {
  m_isExpanded       = expand;
  ShowAllToolsToggle = (expand) ? 1 : 0;
  updateToolbar();
}

//-----------------------------------------------------------------------------

Toolbar::~Toolbar() {}

//-----------------------------------------------------------------------------

bool Toolbar::addAction(QAction *act) {
  if (!act) return false;
  QToolBar::addAction(act);
  return true;
}

//-----------------------------------------------------------------------------

void Toolbar::showEvent(QShowEvent *e) {
  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()),
          SLOT(onToolChanged()));
}

//-----------------------------------------------------------------------------

void Toolbar::hideEvent(QHideEvent *e) {
  disconnect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
             SLOT(onToolChanged()));
}

//-----------------------------------------------------------------------------

void Toolbar::onToolChanged() {
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  TTool *tool            = toolHandle->getTool();
  std::string toolName   = tool->getName();
  QAction *act = CommandManager::instance()->getAction(toolName.c_str());
  if (!act || act->isChecked()) return;
  act->setChecked(true);
}

//=============================================================================

OpenFloatingPanel openToolbarPane(MI_OpenToolbar, "ToolBar", "");
