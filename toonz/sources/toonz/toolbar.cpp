

#include "toolbar.h"
#include "tapp.h"
#include "pane.h"
#include "floatingpanelcommand.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QObject>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 1);

namespace {
struct {
  const char *toolName;
  bool collapsible;
  QAction *action;
} buttonLayout[] = {{T_Edit, false, 0},        {T_Selection, false, 0},
                    {"Separator_1", false, 0}, {T_Brush, false, 0},
                    {T_Geometric, false, 0},   {T_Type, true, 0},
                    {T_Fill, false, 0},        {T_PaintBrush, false, 0},
                    {"Separator_2", false, 0}, {T_Eraser, false, 0},
                    {T_Tape, false, 0},        {T_Finger, false, 0},
                    {"Separator_3", false, 0}, {T_StylePicker, false, 0},
                    {T_RGBPicker, false, 0},   {T_Ruler, false, 0},
                    {"Separator_4", false, 0}, {T_ControlPointEditor, false, 0},
                    {T_Pinch, true, 0},        {T_Pump, true, 0},
                    {T_Magnet, true, 0},       {T_Bender, true, 0},
                    {T_Iron, true, 0},         {T_Cutter, true, 0},
                    {"Separator_5", true, 0},  {T_Skeleton, true, 0},
                    {T_Tracker, true, 0},      {T_Hook, true, 0},
                    {T_Plastic, true, 0},      {"Separator_6", false, 0},
                    {T_Zoom, false, 0},        {T_Rotate, true, 0},
                    {T_Hand, false, 0},        {0, false, 0}};
}  // namespace
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

  setIconSize(QSize(20, 20));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  m_expandButton = new QToolButton(this);
  m_expandButton->setObjectName("expandButton");
  m_expandButton->setCheckable(true);
  m_expandButton->setChecked(m_isExpanded);
  m_expandButton->setArrowType((isVertical) ? Qt::DownArrow : Qt::RightArrow);

  m_expandAction = addWidget(m_expandButton);

  connect(m_expandButton, SIGNAL(toggled(bool)), this,
          SLOT(setIsExpanded(bool)));

  updateToolbar();
}

//-----------------------------------------------------------------------------
/*! Layout the tool buttons according to the state of the expandButton
 */
void Toolbar::updateToolbar() {
  TApp *app                 = TApp::instance();
  TFrameHandle *frameHandle = app->getCurrentFrame();

  if (frameHandle->isPlaying()) return;

  TXshLevelHandle *currlevel = app->getCurrentLevel();
  TXshLevel *level           = currlevel ? currlevel->getLevel() : 0;
  int levelType              = level ? level->getType() : NO_XSHLEVEL;

  TColumnHandle *colHandle = app->getCurrentColumn();
  int colIndex             = colHandle->getColumnIndex();

  int rowIndex = frameHandle->getFrameIndex();

  if (Preferences::instance()->isAutoCreateEnabled() &&
      Preferences::instance()->isAnimationSheetEnabled()) {
    // If in an empty cell, find most recent level
    if (levelType == NO_XSHLEVEL) {
      TXsheetHandle *xshHandle = app->getCurrentXsheet();
      TXsheet *xsh             = xshHandle->getXsheet();

      if (colIndex >= 0 && !xsh->isColumnEmpty(colIndex)) {
        int r0, r1;
        xsh->getCellRange(colIndex, r0, r1);
        if (0 <= r0 && r0 <= r1) {
          // level type depends on previous occupied cell
          for (int r = std::min(r1, rowIndex); r >= r0; r--) {
            TXshCell cell = xsh->getCell(r, colIndex);
            if (cell.isEmpty()) continue;
            levelType = cell.m_level->getType();
            rowIndex  = r;
            break;
          }

          if (levelType == NO_XSHLEVEL) {
            TXshCell cell = xsh->getCell(r0, colIndex);
            levelType     = cell.m_level->getType();
            rowIndex      = r0;
          }
        }
      }
    }
  }

  m_toolbarLevel = levelType;

  TTool::ToolTargetType targetType = TTool::NoTarget;

  switch (m_toolbarLevel) {
  case OVL_XSHLEVEL:
    targetType = TTool::RasterImage;
    break;
  case TZP_XSHLEVEL:
    targetType = TTool::ToonzImage;
    break;
  case PLI_XSHLEVEL:
  default:
    targetType = TTool::VectorImage;
    break;
  case MESH_XSHLEVEL:
    targetType = TTool::MeshImage;
    break;
  }

  // Hide action for now
  for (int idx = 0; buttonLayout[idx].toolName; idx++) {
    if (buttonLayout[idx].action) removeAction(buttonLayout[idx].action);
  }

  removeAction(m_expandAction);

  int levelBasedDisplay = Preferences::instance()->getLevelBasedToolsDisplay();

  bool actionEnabled     = false;
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();

  for (int idx = 0; buttonLayout[idx].toolName; idx++) {
    TTool *tool = TTool::getTool(buttonLayout[idx].toolName, targetType);
    if (tool) tool->updateEnabled(rowIndex, colIndex);
    bool isSeparator = !strncmp(buttonLayout[idx].toolName, "Separator", 9);
    bool enable =
        !levelBasedDisplay ? true : (!tool ? actionEnabled : tool->isEnabled());

    // Plastic tool should always be available so you can create a mesh
    if (!enable && !strncmp(buttonLayout[idx].toolName, T_Plastic, 9) &&
        (m_toolbarLevel & LEVELCOLUMN_XSHLEVEL))
      enable = true;

    if (!m_isExpanded && buttonLayout[idx].collapsible) continue;

    if (!buttonLayout[idx].action) {
      if (isSeparator)
        buttonLayout[idx].action = addSeparator();
      else
        buttonLayout[idx].action =
            CommandManager::instance()->getAction(buttonLayout[idx].toolName);
    }

    if (levelBasedDisplay != 2)
      buttonLayout[idx].action->setEnabled(enable);
    else if (!enable)
      continue;

    actionEnabled = addAction(buttonLayout[idx].action) || actionEnabled;

    if (isSeparator) actionEnabled = false;
  }

  addAction(m_expandAction);

  if (m_isExpanded) {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::UpArrow : Qt::LeftArrow);
    m_expandButton->setToolTip(tr("Collapse toolbar"));
  } else {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::DownArrow : Qt::RightArrow);
    m_expandButton->setToolTip(tr("Expand toolbar"));
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
  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();
  connect(columnHandle, SIGNAL(columnIndexSwitched()), this,
          SLOT(updateToolbar()));

  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(updateToolbar()));
  connect(frameHandle, SIGNAL(frameTypeChanged()), this, SLOT(updateToolbar()));

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(updateToolbar()));

  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()),
          SLOT(onToolChanged()));

  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();
  connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(updateToolbar()));

  connect(TApp::instance()->getCurrentScene(),
          SIGNAL(preferenceChanged(const QString &)), this,
          SLOT(onPreferenceChanged(const QString &)));
}

//-----------------------------------------------------------------------------

void Toolbar::hideEvent(QHideEvent *e) {
  disconnect(TApp::instance()->getCurrentLevel(), 0, this, 0);
  disconnect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
             SLOT(onToolChanged()));

  disconnect(TApp::instance()->getCurrentColumn(),
             SIGNAL(columnIndexSwitched()), this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(updateToolbar()));
  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameTypeChanged()),
             this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
             this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentScene(),
             SIGNAL(preferenceChanged(const QString &)), this,
             SLOT(onPreferenceChanged(const QString &)));
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

//-----------------------------------------------------------------------------

void Toolbar::onPreferenceChanged(const QString &prefName) {
  if (prefName == "ToolbarDisplay" || prefName.isEmpty()) updateToolbar();
}

//=============================================================================

OpenFloatingPanel openToolbarPane(MI_OpenToolbar, "ToolBar", "");
