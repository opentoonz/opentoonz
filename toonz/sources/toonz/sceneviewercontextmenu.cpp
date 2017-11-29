

#include "sceneviewer.h"
#include "sceneviewercontextmenu.h"

// Toonz includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "onionskinmaskgui.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tools/strokeselection.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/selection.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tstageobjectid.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tvectorimage.h"

// Qt includes
#include <QMenu>
#include <QContextMenuEvent>
#include <QSignalMapper>

static void addShowHideStageObjectCmd(QMenu *menu, const TStageObjectId &id,
                                      bool isShow) {
  TXsheet *xsh         = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObject *pegbar = xsh->getStageObject(id);
  QString cmdStr;
  if (id.isCamera())
    cmdStr = (isShow ? "Show " : "Hide ") +
             QString::fromStdString(pegbar->getName());
  else
    cmdStr = (isShow ? "Show Column" : "Hide Column") +
             QString::fromStdString(pegbar->getName());
  QAction *showHideAction = new QAction(cmdStr, menu);
  showHideAction->setData((int)id.getCode());
  menu->addAction(showHideAction);
}

static void onShowHideSelectObject(QAction *action) {
  TApp *app = TApp::instance();
  TStageObjectId id;
  id.setCode(action->data().toInt());
  if (id == TStageObjectId::NoneId) return;
  if (action->text().startsWith("Show ") ||
      action->text().startsWith("Hide ")) {
    if (id.isColumn()) {
      app->getCurrentXsheet()
          ->getXsheet()
          ->getColumn(id.getIndex())
          ->setCamstandVisible(action->text().startsWith("Show "));
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  } else if (action->text().startsWith("Select ")) {
    if (id.isColumn()) {
      app->getCurrentColumn()->setColumnIndex(id.getIndex());
      app->getCurrentObject()->setObjectId(id);
    } else {
      app->getCurrentObject()->setObjectId(id);
      app->getCurrentTool()->setTool(T_Edit);
    }
  }
}

static int addShowHideStageObjectCmds(const std::vector<int> &columnIndexes,
                                      QMenu *menu, bool isShow) {
  int ii, columnIndex = -1;
  bool flag = true;

  for (ii = columnIndexes.size() - 1; ii >= 0; ii--) {
    TStageObjectId id = TStageObjectId::ColumnId(columnIndexes[ii]);
    TXshColumn *col =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(
            columnIndexes[ii]);
    if (!col) continue;
    if (!isShow && col->isCamstandVisible()) {
      if (columnIndex == -1) columnIndex = columnIndexes[ii];
      if (flag) {
        menu->addSeparator();
        flag = false;
      }
      addShowHideStageObjectCmd(menu, id, false);
    } else if (isShow && !col->isCamstandVisible()) {
      if (flag) {
        menu->addSeparator();
        flag = false;
      }
      addShowHideStageObjectCmd(menu, id, true);
    }
  }
  return columnIndex;
}

SceneViewerContextMenu::SceneViewerContextMenu(SceneViewer *parent)
    : QMenu(parent), m_viewer(parent), m_groupIndexToBeEntered(-1) {
  TApp *app                      = TApp::instance();
  bool isEditingLevel            = app->getCurrentFrame()->isEditingLevel();
  bool ret                       = true;
  QAction *action                = 0;
  CommandManager *commandManager = CommandManager::instance();

  /*- サブカメラの消去 -*/
  if (parent->isEditPreviewSubcamera()) {
    action = addAction(tr("Reset Subcamera"));
    ret =
        ret &&
        parent->connect(action, SIGNAL(triggered()), SLOT(doDeleteSubCamera()));
    addSeparator();
  }

  // tool
  TTool *tool = app->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) tool->addContextMenuItems(this);

  // fullscreen
  if (ImageUtils::FullScreenWidget *fsWidget =
          dynamic_cast<ImageUtils::FullScreenWidget *>(
              m_viewer->parentWidget())) {
    bool isFullScreen = (fsWidget->windowState() & Qt::WindowFullScreen) != 0;

    action =
        commandManager->createAction(V_ShowHideFullScreen, this, !isFullScreen);
    addAction(action);
    ret = ret &&
          parent->connect(action, SIGNAL(triggered()), fsWidget,
                          SLOT(toggleFullScreen()));
  }

  // swap compared
  if (parent->canSwapCompared()) {
    action = addAction(tr("Swap Compared Images"));
    ret    = ret &&
          parent->connect(action, SIGNAL(triggered()), SLOT(swapCompared()));
  }

  // reset
  action = commandManager->createAction(V_ZoomReset, this);
  addAction(action);
  ret = ret &&
        parent->connect(action, SIGNAL(triggered()), SLOT(resetSceneViewer()));

  if (!isEditingLevel) {
    // fit camera
    action = commandManager->createAction(V_ZoomFit, this);
    addAction(action);
    ret = ret &&
          parent->connect(action, SIGNAL(triggered()), SLOT(fitToCamera()));
  }

  // actual pixel size
  action = commandManager->createAction(V_ActualPixelSize, this);
  addAction(action);
  ret =
      ret &&
      parent->connect(action, SIGNAL(triggered()), SLOT(setActualPixelSize()));

  // onion skin
  if (Preferences::instance()->isOnionSkinEnabled() &&
      !parent->isPreviewEnabled())
    OnioniSkinMaskGUI::addOnionSkinCommand(this);
  QMenu *guidedDrawingMenu = addMenu(tr("Vector Guided Drawing"));
  int guidedDrawingStatus  = Preferences::instance()->getGuidedDrawing();
  action                   = guidedDrawingMenu->addAction(tr("Off"));
  action->setCheckable(true);
  action->setChecked(guidedDrawingStatus == 0);
  ret = ret &&
        parent->connect(action, SIGNAL(triggered()), this,
                        SLOT(setGuidedDrawingOff()));
  action = guidedDrawingMenu->addAction(tr("Closest Drawing"));
  action->setCheckable(true);
  action->setChecked(guidedDrawingStatus == 1);
  ret = ret &&
        parent->connect(action, SIGNAL(triggered()), this,
                        SLOT(setGuidedDrawingClosest()));
  action = guidedDrawingMenu->addAction(tr("Farthest Drawing"));
  action->setCheckable(true);
  action->setChecked(guidedDrawingStatus == 2);
  ret = ret &&
        parent->connect(action, SIGNAL(triggered()), this,
                        SLOT(setGuidedDrawingFarthest()));
  action = guidedDrawingMenu->addAction(tr("All Drawings"));
  action->setCheckable(true);
  action->setChecked(guidedDrawingStatus == 3);
  ret = ret &&
        parent->connect(action, SIGNAL(triggered()), this,
                        SLOT(setGuidedDrawingAll()));
  // Zero Thick
  if (!parent->isPreviewEnabled())
    ZeroThickToggleGui::addZeroThickCommand(this);

  // preview
  if (parent->isPreviewEnabled()) {
    addSeparator();

    // save previewed frames
    action = addAction(tr("Save Previewed Frames"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_SavePreviewedFrames)));
    ret = ret &&
          parent->connect(action, SIGNAL(triggered()), this,
                          SLOT(savePreviewedFrames()));

    // regenerate preview
    action = addAction(tr("Regenerate Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegeneratePreview)));
    ret =
        ret &&
        parent->connect(action, SIGNAL(triggered()), SLOT(regeneratePreview()));

    // regenerate frame preview
    action = addAction(tr("Regenerate Frame Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegenerateFramePr)));
    ret = ret &&
          parent->connect(action, SIGNAL(triggered()),
                          SLOT(regeneratePreviewFrame()));
  }

  assert(ret);
}

SceneViewerContextMenu::~SceneViewerContextMenu() {}

void SceneViewerContextMenu::addEnterGroupCommands(const TPointD &pos) {
  bool ret         = true;
  TVectorImageP vi = (TVectorImageP)TTool::getImage(false);
  if (!vi) return;

  if (vi->isInsideGroup() > 0) {
    addAction(CommandManager::instance()->getAction(MI_ExitGroup));
  }

  StrokeSelection *ss =
      dynamic_cast<StrokeSelection *>(TSelection::getCurrent());
  if (!ss) return;

  for (int i = 0; i < vi->getStrokeCount(); i++)
    if (ss->isSelected(i) && vi->canEnterGroup(i)) {
      m_groupIndexToBeEntered = i;
      addAction(CommandManager::instance()->getAction(MI_EnterGroup));
      return;
    }

  assert(ret);
}

static QString getName(TStageObject *obj) {
  return QString::fromStdString(obj->getFullName());
}

void SceneViewerContextMenu::addShowHideCommand(QMenu *menu,
                                                TXshColumn *column) {
  bool isHidden = !column->isCamstandVisible();
  TXsheet *xsh  = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObject *stageObject =
      xsh->getStageObject(TStageObjectId::ColumnId(column->getIndex()));
  QString text = (isHidden ? tr("Show ") : tr("Hide ")) + getName(stageObject);
  QAction *action = new QAction(text, this);
  action->setData(column->getIndex());
  connect(action, SIGNAL(triggered()), this, SLOT(onShowHide()));
  menu->addAction(action);
}

void SceneViewerContextMenu::addSelectCommand(QMenu *menu,
                                              const TStageObjectId &id) {
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObject *stageObject = xsh->getStageObject(id);
  if (!stageObject) return;
  QString text           = getName(stageObject);
  if (menu == this) text = tr("Select ") + text;
  QAction *action        = new QAction(text, this);
  action->setData(id.getCode());
  connect(action, SIGNAL(triggered()), this, SLOT(onSetCurrent()));
  menu->addAction(action);
}

void SceneViewerContextMenu::addLevelCommands(std::vector<int> &indices) {
  addSeparator();
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId currentId =
      TApp::instance()->getCurrentObject()->getObjectId();

  /*- Xsheet内の、空でないColumnを登録 -*/
  std::vector<TXshColumn *> columns;
  for (int i = 0; i < (int)indices.size(); i++) {
    if (xsh->isColumnEmpty(indices[i])) continue;
    TXshColumn *column = xsh->getColumn(indices[i]);
    if (column) {
      columns.push_back(column);
    }
  }

  if (!columns.empty()) {
    // show/hide
    if (columns.size() > 1) {
      QMenu *subMenu = addMenu(tr("Show / Hide"));
      for (int i = 0; i < (int)columns.size(); i++)
        addShowHideCommand(subMenu, columns[i]);
    } else
      addShowHideCommand(this, columns[0]);
    addSeparator();
  }

  // selection
  /*
    if(selectableColumns.size()==1)
    {
      addSelectCommand(this,
    TStageObjectId::ColumnId(selectableColumns[0]->getIndex()));
    }
    else
    */

  /*-- Scene内の全Objectを選択可能にする --*/
  TStageObjectId id;
  QMenu *cameraMenu = addMenu(tr("Select Camera"));
  QMenu *pegbarMenu = addMenu(tr("Select Pegbar"));
  QMenu *columnMenu = addMenu(tr("Select Column"));

  bool flag = false;

  for (int i = 0; i < xsh->getStageObjectTree()->getStageObjectCount(); i++) {
    id = xsh->getStageObjectTree()->getStageObject(i)->getId();
    if (id.isColumn()) {
      int columnIndex = id.getIndex();
      if (xsh->isColumnEmpty(columnIndex))
        continue;
      else {
        addSelectCommand(columnMenu, id);
        flag = true;
      }
    } else if (id.isTable())
      addSelectCommand(this, id);
    else if (id.isCamera())
      addSelectCommand(cameraMenu, id);
    else if (id.isPegbar())
      addSelectCommand(pegbarMenu, id);
  }

  /*- カラムがひとつも無かったらDisable -*/
  if (!flag) columnMenu->setEnabled(false);
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::enterVectorImageGroup() {
  if (m_groupIndexToBeEntered == -1) return;

  TVectorImageP vi =
      (TVectorImageP)TTool::getImage(false);  // getCurrentImage();
  if (!vi) return;
  vi->enterGroup(m_groupIndexToBeEntered);
  TSelection *selection = TSelection::getCurrent();
  if (selection) selection->selectNone();
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::exitVectorImageGroup() {
  TVectorImageP vi =
      (TVectorImageP)TTool::getImage(false);  // getCurrentImage();
  if (!vi) return;
  vi->exitGroup();
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::onShowHide() {
  int columnIndex = qobject_cast<QAction *>(sender())->data().toInt();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  TXshColumn *column = xsheetHandle->getXsheet()->getColumn(columnIndex);
  if (column) {
    column->setCamstandVisible(!column->isCamstandVisible());
    xsheetHandle->notifyXsheetChanged();
  }
}
//-----------------------------------------------------------------------------

void SceneViewerContextMenu::onSetCurrent() {
  TStageObjectId id;
  id.setCode(qobject_cast<QAction *>(sender())->data().toUInt());
  TApp *app = TApp::instance();
  if (id.isColumn()) {
    app->getCurrentColumn()->setColumnIndex(id.getIndex());
    app->getCurrentObject()->setObjectId(id);
  } else {
    app->getCurrentObject()->setObjectId(id);
    app->getCurrentTool()->setTool(T_Edit);
  }
}

//-----------------------------------------------------------------------------
void SceneViewerContextMenu::setGuidedDrawingOff() {
  Preferences::instance()->setGuidedDrawing(0);
}

//-----------------------------------------------------------------------------
void SceneViewerContextMenu::setGuidedDrawingClosest() {
  Preferences::instance()->setGuidedDrawing(1);
}

//-----------------------------------------------------------------------------
void SceneViewerContextMenu::setGuidedDrawingFarthest() {
  Preferences::instance()->setGuidedDrawing(2);
}

//-----------------------------------------------------------------------------
void SceneViewerContextMenu::setGuidedDrawingAll() {
  Preferences::instance()->setGuidedDrawing(3);
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::savePreviewedFrames() {
  Previewer::instance(m_viewer->getPreviewMode() ==
                      SceneViewer::SUBCAMERA_PREVIEW)
      ->saveRenderedFrames();
}

class ZeroThickToggle : public MenuItemHandler {
public:
  ZeroThickToggle() : MenuItemHandler(MI_ZeroThick) {}
  void execute() {
    QAction *action = CommandManager::instance()->getAction(MI_ZeroThick);
    if (!action) return;
    bool checked = action->isChecked();
    enableZeroThick(checked);
  }

  static void enableZeroThick(bool enable = true) {
    Preferences::instance()->setShow0ThickLines(enable);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }
} ZeroThickToggle;

void ZeroThickToggleGui::addZeroThickCommand(QMenu *menu) {
  static ZeroThickToggleHandler switcher;
  if (Preferences::instance()->getShow0ThickLines()) {
    QAction *hideZeroThick =
        menu->addAction(QString(QObject::tr("Hide Zero Thickness Lines")));
    menu->connect(hideZeroThick, SIGNAL(triggered()), &switcher,
                  SLOT(deactivate()));
  } else {
    QAction *showZeroThick =
        menu->addAction(QString(QObject::tr("Show Zero Thickness Lines")));
    menu->connect(showZeroThick, SIGNAL(triggered()), &switcher,
                  SLOT(activate()));
  }
}

void ZeroThickToggleGui::ZeroThickToggleHandler::activate() {
  ZeroThickToggle::enableZeroThick(true);
}

void ZeroThickToggleGui::ZeroThickToggleHandler::deactivate() {
  ZeroThickToggle::enableZeroThick(false);
}