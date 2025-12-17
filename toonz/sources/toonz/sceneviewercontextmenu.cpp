

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

SceneViewerContextMenu::SceneViewerContextMenu(SceneViewer *parent)
    : QMenu(parent), m_viewer(parent), m_groupIndexToBeEntered(-1) {
  TApp *app                      = TApp::instance();
  bool isEditingLevel            = app->getCurrentFrame()->isEditingLevel();
  CommandManager *commandManager = CommandManager::instance();

  /*- Delete subcamera -*/
  if (parent->isEditPreviewSubcamera()) {
    QAction *action = addAction(tr("Reset Subcamera"));
    connect(action, &QAction::triggered, parent,
            &SceneViewer::doDeleteSubCamera);
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

    QAction *action =
        commandManager->createAction(V_ShowHideFullScreen, this, !isFullScreen);
    addAction(action);
    connect(action, &QAction::triggered, fsWidget,
            &ImageUtils::FullScreenWidget::toggleFullScreen);
  }

  // swap compared
  if (parent->canSwapCompared()) {
    QAction *action = addAction(tr("Swap Compared Images"));
    connect(action, &QAction::triggered, parent, &SceneViewer::swapCompared);
  }

  if (!isEditingLevel) {
    // fit camera
    QAction *action = commandManager->createAction(V_ZoomFit, this);
    addAction(action);
    connect(action, &QAction::triggered, parent, &SceneViewer::fitToCamera);
  }

  QMenu *flipViewMenu = addMenu(tr("Flip View"));

  // flip horizontally
  QAction *action = commandManager->createAction(V_FlipX, this);
  flipViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::flipX);

  // flip vertically
  action = commandManager->createAction(V_FlipY, this);
  flipViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::flipY);

  QMenu *rotateViewMenu = addMenu(tr("Rotate View"));

  // Rotate Left
  action = commandManager->createAction(V_RotateLeft, this);
  rotateViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::rotateLeft);

  // Rotate Right
  action = commandManager->createAction(V_RotateRight, this);
  rotateViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::rotateRight);

  QMenu *resetViewMenu = addMenu(tr("Reset View"));

  // reset
  action = commandManager->createAction(V_ViewReset, this);
  resetViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::resetSceneViewer);

  // reset zoom
  action = commandManager->createAction(V_ZoomReset, this);
  resetViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::resetZoom);

  // reset rotation
  action = commandManager->createAction(V_RotateReset, this);
  resetViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::resetRotation);

  // reset position
  action = commandManager->createAction(V_PositionReset, this);
  resetViewMenu->addAction(action);
  connect(action, &QAction::triggered, parent, &SceneViewer::resetPosition);

  // actual pixel size
  action = commandManager->createAction(V_ActualPixelSize, this);
  addAction(action);
  connect(action, &QAction::triggered, parent,
          &SceneViewer::setActualPixelSize);

  // onion skin
  if (Preferences::instance()->isOnionSkinEnabled() &&
      !parent->isPreviewEnabled())
    OnioniSkinMaskGUI::addOnionSkinCommand(this);

  if (tool->getTargetType() & TTool::VectorImage) {
    auto addOptionAction = [](const QString &label, const int data,
                              const int currentData, QMenu *menu,
                              QActionGroup *group) {
      QAction *action = menu->addAction(label);
      action->setData(data);
      action->setCheckable(true);
      action->setChecked(data == currentData);
      group->addAction(action);
    };
    QMenu *guidedDrawingMenu = addMenu(tr("Vector Guided Drawing"));
    int guidedDrawingStatus  = Preferences::instance()->getGuidedDrawingType();

    QActionGroup *guidedDrawingGroup = new QActionGroup(this);
    addOptionAction(tr("Off"), 0, guidedDrawingStatus, guidedDrawingMenu,
                    guidedDrawingGroup);
    addOptionAction(tr("Closest Drawing"), 1, guidedDrawingStatus,
                    guidedDrawingMenu, guidedDrawingGroup);
    addOptionAction(tr("Farthest Drawing"), 2, guidedDrawingStatus,
                    guidedDrawingMenu, guidedDrawingGroup);
    addOptionAction(tr("All Drawings"), 3, guidedDrawingStatus,
                    guidedDrawingMenu, guidedDrawingGroup);

    connect(guidedDrawingGroup, &QActionGroup::triggered, this,
            [this](QAction *action) {
              Preferences::instance()->setValue(guidedDrawingType,
                                                action->data().toInt());

              QAction *guidedDrawingAction =
                  CommandManager::instance()->getAction(MI_VectorGuidedDrawing);
              if (guidedDrawingAction)
                guidedDrawingAction->setChecked(
                    Preferences::instance()->isGuidedDrawingEnabled());

              TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
                  "GuidedDrawingFrame");
            });

    guidedDrawingMenu->addSeparator();
    bool enableOption = guidedDrawingStatus == 1 || guidedDrawingStatus == 2;
    action            = guidedDrawingMenu->addAction(tr("Auto Inbetween"));
    action->setCheckable(true);
    action->setChecked(Preferences::instance()->getGuidedAutoInbetween());
    action->setEnabled(enableOption);

    connect(action, &QAction::triggered, this, [this]() {
      Preferences::instance()->setValue(
          guidedAutoInbetween,
          !Preferences::instance()->getGuidedAutoInbetween());
      TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
          "GuidedDrawingAutoInbetween");
    });

    guidedDrawingMenu->addSeparator();
    int guidedInterpolation = Preferences::instance()->getGuidedInterpolation();
    QActionGroup *interpolationGroup = new QActionGroup(this);
    addOptionAction(tr("Linear Interpolation"), 1, guidedInterpolation,
                    guidedDrawingMenu, interpolationGroup);
    addOptionAction(tr("Ease In Interpolation"), 2, guidedInterpolation,
                    guidedDrawingMenu, interpolationGroup);
    addOptionAction(tr("Ease Out Interpolation"), 3, guidedInterpolation,
                    guidedDrawingMenu, interpolationGroup);
    addOptionAction(tr("Ease In/Out Interpolation"), 4, guidedInterpolation,
                    guidedDrawingMenu, interpolationGroup);

    connect(interpolationGroup, &QActionGroup::triggered, this,
            [this](QAction *action) {
              Preferences::instance()->setValue(guidedInterpolationType,
                                                action->data().toInt());
              TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
                  "GuidedDrawingInterpolation");
            });

    interpolationGroup->setEnabled(enableOption);
    /*
    // Kept for reference and possible future re-enabling or removal in a future
    revision.

       guidedDrawingMenu->addSeparator(); action =
       CommandManager::instance()->getAction(MI_SelectPrevGuideStroke);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
        action =
       CommandManager::instance()->getAction(MI_SelectNextGuideStroke);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
        action =
       CommandManager::instance()->getAction(MI_SelectBothGuideStrokes);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
        action =
       CommandManager::instance()->getAction(MI_SelectGuideStrokeReset);
        action->setEnabled(true);
        guidedDrawingMenu->addAction(action);
        guidedDrawingMenu->addSeparator();
        action = CommandManager::instance()->getAction(MI_TweenGuideStrokes);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
        action =
            CommandManager::instance()->getAction(MI_TweenGuideStrokeToSelected);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
        action =
       CommandManager::instance()->getAction(MI_SelectGuidesAndTweenMode);
        action->setEnabled(enableOption);
        guidedDrawingMenu->addAction(action);
    */
    // Zero Thick
    if (!parent->isPreviewEnabled())
      ZeroThickToggleGui::addZeroThickCommand(this);
  }
  // Brush size outline
  CursorOutlineToggleGui::addCursorOutlineCommand(this);

  ViewerIndicatorToggleGui::addViewerIndicatorCommand(this);

  // preview
  if (parent->isPreviewEnabled()) {
    addSeparator();

    // save previewed frames
    action = addAction(tr("Save Previewed Frames"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_SavePreviewedFrames)));
    connect(action, &QAction::triggered, this, [this]() {
      Previewer::instance(m_viewer->getPreviewMode() ==
                          SceneViewer::SUBCAMERA_PREVIEW)
          ->saveRenderedFrames();
    });

    // regenerate preview
    action = addAction(tr("Regenerate Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegeneratePreview)));
    connect(action, &QAction::triggered, parent,
            &SceneViewer::regeneratePreview);

    // regenerate frame preview
    action = addAction(tr("Regenerate Frame Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegenerateFramePr)));
    connect(action, &QAction::triggered, parent,
            &SceneViewer::regeneratePreviewFrame);
  }
}

SceneViewerContextMenu::~SceneViewerContextMenu() {}

void SceneViewerContextMenu::addEnterGroupCommands(const TPointD &pos) {
  TVectorImageP vi = (TVectorImageP)TTool::getImage(false);
  if (!vi) return;

  if (vi->isInsideGroup() > 0) {
    addAction(CommandManager::instance()->getAction(MI_ExitGroup));
  }

  StrokeSelection *ss =
      dynamic_cast<StrokeSelection *>(TSelection::getCurrent());
  if (!ss) return;

  for (int i = 0; i < vi->getStrokeCount(); i++) {
    if (ss->isSelected(i) && vi->canEnterGroup(i)) {
      m_groupIndexToBeEntered = i;
      addAction(CommandManager::instance()->getAction(MI_EnterGroup));
      return;
    }
  }
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
  QString text    = isHidden ? tr("Show %1").arg(getName(stageObject))
                             : tr("Hide %1").arg(getName(stageObject));
  QAction *action = new QAction(text, this);
  int columnIndex = column->getIndex();

  connect(action, &QAction::triggered, this, [this, columnIndex]() {
    TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
    TXshColumn *column = xsheetHandle->getXsheet()->getColumn(columnIndex);
    if (column) {
      column->setCamstandVisible(!column->isCamstandVisible());
      xsheetHandle->notifyXsheetChanged();
    }
  });

  menu->addAction(action);
}

void SceneViewerContextMenu::addSelectCommand(QMenu *menu,
                                              const TStageObjectId &id) {
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObject *stageObject = xsh->getStageObject(id);
  if (!stageObject) return;
  QString text = (id.isTable()) ? tr("Table") : getName(stageObject);
  if (menu == this) text = tr("Select %1").arg(text);
  QAction *action = new QAction(text, this);

  connect(action, &QAction::triggered, this, [this, id]() {
    TApp *app = TApp::instance();
    if (id.isColumn()) {
      app->getCurrentColumn()->setColumnIndex(id.getIndex());
      app->getCurrentObject()->setObjectId(id);
    } else {
      app->getCurrentObject()->setObjectId(id);
      app->getCurrentTool()->setTool(T_Edit);
    }
  });

  menu->addAction(action);
}

void SceneViewerContextMenu::addLevelCommands(std::vector<int> &indices) {
  addSeparator();
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  // Register non-empty columns in the xsheet
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
    } else {
      addShowHideCommand(this, columns[0]);
    }
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

  /*-- Make all objects in the scene selectable --*/
  TStageObjectId id;
  QMenu *cameraMenu = addMenu(tr("Select Camera"));
  QMenu *pegbarMenu = addMenu(tr("Select Pegbar"));
  QMenu *columnMenu = addMenu(tr("Select Column"));

  bool hasColumns = false;

  for (int i = 0; i < xsh->getStageObjectTree()->getStageObjectCount(); i++) {
    id = xsh->getStageObjectTree()->getStageObject(i)->getId();
    if (id.isColumn()) {
      int columnIndex = id.getIndex();
      if (xsh->isColumnEmpty(columnIndex))
        continue;
      else {
        addSelectCommand(columnMenu, id);
        hasColumns = true;
      }
    } else if (id.isTable()) {
      addSelectCommand(this, id);
    } else if (id.isCamera()) {
      addSelectCommand(cameraMenu, id);
    } else if (id.isPegbar()) {
      addSelectCommand(pegbarMenu, id);
    }
  }

  // Disable if no columns
  if (!hasColumns) columnMenu->setEnabled(false);
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::enterVectorImageGroup() {
  if (m_groupIndexToBeEntered == -1) return;

  TVectorImageP vi = (TVectorImageP)TTool::getImage(false);
  if (!vi) return;
  vi->enterGroup(m_groupIndexToBeEntered);
  TSelection *selection = TSelection::getCurrent();
  if (selection) selection->selectNone();
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void SceneViewerContextMenu::exitVectorImageGroup() {
  TVectorImageP vi = (TVectorImageP)TTool::getImage(false);
  if (!vi) return;
  vi->exitGroup();
  m_viewer->update();
}

// Helper classes
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
    Preferences::instance()->setValue(show0ThickLines, enable);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }
} ZeroThickToggle;

void ZeroThickToggleGui::addZeroThickCommand(QMenu *menu) {
  static ZeroThickToggleHandler switcher;
  if (Preferences::instance()->getShow0ThickLines()) {
    QAction *hideZeroThick =
        menu->addAction(QString(QObject::tr("Hide Zero Thickness Lines")));
    QObject::connect(hideZeroThick, &QAction::triggered, &switcher,
                     &ZeroThickToggleHandler::deactivate);
  } else {
    QAction *showZeroThick =
        menu->addAction(QString(QObject::tr("Show Zero Thickness Lines")));
    QObject::connect(showZeroThick, &QAction::triggered, &switcher,
                     &ZeroThickToggleHandler::activate);
  }
}

void ZeroThickToggleGui::ZeroThickToggleHandler::activate() {
  ZeroThickToggle::enableZeroThick(true);
}

void ZeroThickToggleGui::ZeroThickToggleHandler::deactivate() {
  ZeroThickToggle::enableZeroThick(false);
}

class CursorOutlineToggle : public MenuItemHandler {
public:
  CursorOutlineToggle() : MenuItemHandler(MI_CursorOutline) {}
  void execute() {
    QAction *action = CommandManager::instance()->getAction(MI_CursorOutline);
    if (!action) return;
    bool checked = action->isChecked();
    enableCursorOutline(checked);
  }

  static void enableCursorOutline(bool enable = true) {
    Preferences::instance()->setValue(cursorOutlineEnabled, enable);
  }
} CursorOutlineToggle;

void CursorOutlineToggleGui::addCursorOutlineCommand(QMenu *menu) {
  static CursorOutlineToggleHandler switcher;
  if (Preferences::instance()->isCursorOutlineEnabled()) {
    QAction *hideCursorOutline =
        menu->addAction(QString(QObject::tr("Hide cursor size outline")));
    QObject::connect(hideCursorOutline, &QAction::triggered, &switcher,
                     &CursorOutlineToggleHandler::deactivate);
  } else {
    QAction *showCursorOutline =
        menu->addAction(QString(QObject::tr("Show cursor size outline")));
    QObject::connect(showCursorOutline, &QAction::triggered, &switcher,
                     &CursorOutlineToggleHandler::activate);
  }
}

void CursorOutlineToggleGui::CursorOutlineToggleHandler::activate() {
  CursorOutlineToggle::enableCursorOutline(true);
}

void CursorOutlineToggleGui::CursorOutlineToggleHandler::deactivate() {
  CursorOutlineToggle::enableCursorOutline(false);
}

class ViewerIndicatorToggle : public MenuItemHandler {
public:
  ViewerIndicatorToggle() : MenuItemHandler(MI_ViewerIndicator) {}
  void execute() {
    QAction *action = CommandManager::instance()->getAction(MI_ViewerIndicator);
    if (!action) return;
    bool checked = action->isChecked();
    enableViewerIndicator(checked);
  }

  static void enableViewerIndicator(bool enable = true) {
    Preferences::instance()->setValue(viewerIndicatorEnabled, enable);
  }
} ViewerIndicatorToggle;

void ViewerIndicatorToggleGui::addViewerIndicatorCommand(QMenu *menu) {
  static ViewerIndicatorToggleHandler switcher;
  if (Preferences::instance()->isViewerIndicatorEnabled()) {
    QAction *hideViewerIndicator =
        menu->addAction(QString(QObject::tr("Hide Viewer Indicators")));
    QObject::connect(hideViewerIndicator, &QAction::triggered, &switcher,
                     &ViewerIndicatorToggleHandler::deactivate);
  } else {
    QAction *showViewerIndicator =
        menu->addAction(QString(QObject::tr("Show Viewer Indicators")));
    QObject::connect(showViewerIndicator, &QAction::triggered, &switcher,
                     &ViewerIndicatorToggleHandler::activate);
  }
}

void ViewerIndicatorToggleGui::ViewerIndicatorToggleHandler::activate() {
  ViewerIndicatorToggle::enableViewerIndicator(true);
}

void ViewerIndicatorToggleGui::ViewerIndicatorToggleHandler::deactivate() {
  ViewerIndicatorToggle::enableViewerIndicator(false);
}
