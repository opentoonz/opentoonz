

// TnzCore includes
#include "tconvert.h"
#include "tgeometry.h"
#include "tgl.h"
#include "trop.h"
#include "tstopwatch.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/stage.h"
#include "toonz/stage2.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcell.h"
#include "toonz/tcamera.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toutputproperties.h"
#include "toonz/preferences.h"
#include "toonz/tproject.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"

// TnzTools includes
#include "tools/toolhandle.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "sceneviewer.h"
#include "xsheetdragtool.h"
#include "ruler.h"
#include "menubarcommandids.h"

// Qt includes
#include <QPainter>
#include <QVBoxLayout>
#include <QAction>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QButtonGroup>
#include <QMenu>
#include <QToolBar>
#include <QMainWindow>
#include <QSettings>

#include "viewerpane.h"

enum CV_Parts {
  CVPARTS_None        = 0,
  CVPARTS_PLAYBAR     = 0x1,
  CVPARTS_FRAMESLIDER = 0x4,
  CVPARTS_End         = 0x8,
  CVPARTS_ALL         = CVPARTS_PLAYBAR | CVPARTS_FRAMESLIDER
};

using namespace DVGui;

//=============================================================================
//
// SceneViewerPanel
//
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
SceneViewerPanel::SceneViewerPanel(QWidget *parent, Qt::WindowFlags flags)
#else
SceneViewerPanel::SceneViewerPanel(QWidget *parent, Qt::WFlags flags)
#endif
    : QFrame(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("ViewerPanel");

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);

  // Viewer
  QWidget *viewer      = new QWidget(this);
  QGridLayout *viewerL = new QGridLayout(viewer);

  ImageUtils::FullScreenWidget *fsWidget =
      new ImageUtils::FullScreenWidget(viewer);

  fsWidget->setWidget(m_sceneViewer = new SceneViewer(fsWidget));
  m_sceneViewer->setIsStyleShortcutSwitchable();

  bool ret = true;
  ret      = ret && connect(m_sceneViewer, SIGNAL(onZoomChanged()),
                       SLOT(changeWindowTitle()));

  Ruler *vRuler = new Ruler(viewer, m_sceneViewer, true);
  Ruler *hRuler = new Ruler(viewer, m_sceneViewer, false);
  m_sceneViewer->setRulers(vRuler, hRuler);

  viewerL->setMargin(0);
  viewerL->setSpacing(0);
  viewerL->addWidget(vRuler, 1, 0);
  viewerL->addWidget(hRuler, 0, 1);
  viewerL->addWidget(fsWidget, 1, 1, 19, 13);
  viewer->setMinimumHeight(200);
  viewer->setLayout(viewerL);

  mainLayout->addWidget(viewer, Qt::AlignCenter);

  TApp *app        = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(0, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  std::vector<int> buttonMask = {FlipConsole::eFilledRaster,
                                 FlipConsole::eDefineLoadBox,
                                 FlipConsole::eUseLoadBox};

  m_flipConsole =
      new FlipConsole(mainLayout, buttonMask, false, m_keyFrameButton,
                      "SceneViewerConsole", this, true);

  m_flipConsole->enableButton(FlipConsole::eMatte, false, false);
  m_flipConsole->enableButton(FlipConsole::eSave, false, false);
  m_flipConsole->enableButton(FlipConsole::eCompare, false, false);
  m_flipConsole->enableButton(FlipConsole::eSaveImg, false, false);
  m_flipConsole->enableButton(FlipConsole::eGRed, false, false);
  m_flipConsole->enableButton(FlipConsole::eGGreen, false, false);
  m_flipConsole->enableButton(FlipConsole::eGBlue, false, false);
  m_flipConsole->enableButton(FlipConsole::eBlackBg, false, false);
  m_flipConsole->enableButton(FlipConsole::eWhiteBg, false, false);
  m_flipConsole->enableButton(FlipConsole::eCheckBg, false, false);

  m_flipConsole->setFrameHandle(TApp::instance()->getCurrentFrame());

  ret = ret &&
        connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
                TApp::instance()->getCurrentFrame(), SLOT(setPlaying(bool)));
  ret = ret && connect(m_flipConsole, SIGNAL(playStateChanged(bool)), this,
                       SLOT(onPlayingStatusChanged(bool)));
  ret = ret &&
        connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                m_sceneViewer, SLOT(onButtonPressed(FlipConsole::EGadget)));

  ret =
      ret && connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                     this, SLOT(onButtonPressed(FlipConsole::EGadget)));

  ret = ret && connect(m_sceneViewer, SIGNAL(previewStatusChanged()), this,
                       SLOT(update()));
  ret = ret && connect(m_sceneViewer, SIGNAL(onFlipHChanged(bool)), this,
                       SLOT(setFlipHButtonChecked(bool)));
  ret = ret && connect(m_sceneViewer, SIGNAL(onFlipVChanged(bool)), this,
                       SLOT(setFlipVButtonChecked(bool)));

  ret = ret && connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));

  assert(ret);
  m_flipConsole->setChecked(FlipConsole::eSound, true);
  m_playSound = m_flipConsole->isChecked(FlipConsole::eSound);
  m_flipConsole->setFrameRate(app->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());
  updateFrameRange(), updateFrameMarkers();

  setLayout(mainLayout);

  m_visiblePartsFlag = CVPARTS_ALL;
  updateShowHide();

  setFocusProxy(m_sceneViewer);
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*! toggle show/hide of the widgets according to m_visibleFlag
 */

void SceneViewerPanel::updateShowHide() {
  // flip console
  m_flipConsole->showHidePlaybar(m_visiblePartsFlag & CVPARTS_PLAYBAR);
  m_flipConsole->showHideFrameSlider(m_visiblePartsFlag & CVPARTS_FRAMESLIDER);
  update();
}

//-----------------------------------------------------------------------------
/*! showing the show/hide commands
 */

void SceneViewerPanel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);
  addShowHideContextMenu(menu);
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));

  // actions
  QAction *playbarSHAct     = showHideMenu->addAction(tr("Playback Toolbar"));
  QAction *frameSliderSHAct = showHideMenu->addAction(tr("Frame Slider"));

  playbarSHAct->setCheckable(true);
  playbarSHAct->setChecked(m_visiblePartsFlag & CVPARTS_PLAYBAR);
  playbarSHAct->setData((UINT)CVPARTS_PLAYBAR);

  frameSliderSHAct->setCheckable(true);
  frameSliderSHAct->setChecked(m_visiblePartsFlag & CVPARTS_FRAMESLIDER);
  frameSliderSHAct->setData((UINT)CVPARTS_FRAMESLIDER);

  QActionGroup *showHideActGroup = new QActionGroup(this);
  showHideActGroup->setExclusive(false);
  showHideActGroup->addAction(playbarSHAct);
  showHideActGroup->addAction(frameSliderSHAct);

  connect(showHideActGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onShowHideActionTriggered(QAction *)));

  showHideMenu->addSeparator();
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewCamera));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewTable));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_FieldGuide));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_SafeArea));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewBBox));
  showHideMenu->addAction(
      CommandManager::instance()->getAction(MI_ViewColorcard));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewRuler));
}

//-----------------------------------------------------------------------------
/*! slot function for show/hide the parts
 */

void SceneViewerPanel::onShowHideActionTriggered(QAction *act) {
  CV_Parts part = (CV_Parts)act->data().toUInt();
  assert(part < CVPARTS_End);

  m_visiblePartsFlag ^= part;

  updateShowHide();
}

void SceneViewerPanel::onDrawFrame(
    int frame, const ImagePainter::VisualSettings &settings) {
  TApp *app = TApp::instance();
  m_sceneViewer->setVisual(settings);
  TFrameHandle *frameHandle = app->getCurrentFrame();

  if (m_sceneViewer->isPreviewEnabled()) {
    class Previewer *pr = Previewer::instance(m_sceneViewer->getPreviewMode() ==
                                              SceneViewer::SUBCAMERA_PREVIEW);
    pr->getRaster(frame - 1, settings.m_recomputeIfNeeded);  // the 'getRaster'
                                                             // starts the
                                                             // render of the
                                                             // frame is not
                                                             // already started
    int curFrame = frame;
    if (frameHandle->isPlaying() &&
        !pr->isFrameReady(
            frame - 1))  // stops on last rendered frame until current is ready!
    {
      while (frame > 1 && !pr->isFrameReady(frame - 1)) frame--;
      m_flipConsole->setCurrentFrame(frame);
    }
  }

  // assert(frame >= 0); // frame can be negative in rare cases
  if (frame != frameHandle->getFrameIndex() + 1) {
    int oldFrame = frameHandle->getFrame();
    frameHandle->setCurrentFrame(frame);
    if (!frameHandle->isPlaying() && !frameHandle->isEditingLevel() &&
        oldFrame != frameHandle->getFrame())
      frameHandle->scrubXsheet(
          frame - 1, frame - 1,
          TApp::instance()->getCurrentXsheet()->getXsheet());
  }

  else if (settings.m_blankColor != TPixel::Transparent)
    m_sceneViewer->update();
}

//-----------------------------------------------------------------------------

SceneViewerPanel::~SceneViewerPanel() {}

//-----------------------------------------------------------------------------

void SceneViewerPanel::showEvent(QShowEvent *event) {
  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TSceneHandle *sceneHandle    = app->getCurrentScene();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  TObjectHandle *objectHandle  = app->getCurrentObject();
  TXsheetHandle *xshHandle     = app->getCurrentXsheet();

  onSceneChanged();

  bool ret = true;

  ret = ret && connect(xshHandle, SIGNAL(xsheetChanged()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneChanged()), this,
                       SLOT(onSceneChanged()));

  ret = ret && connect(sceneHandle, SIGNAL(nameSceneChanged()), this,
                       SLOT(changeWindowTitle()));
  ret =
      ret && connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
                     m_flipConsole, SLOT(onPreferenceChanged(const QString &)));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
                       SLOT(onXshLevelSwitched(TXshLevel *)));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelTitleChanged()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
                       SLOT(updateFrameRange()));

  ret = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(onFrameSwitched()));
  ret = ret && connect(frameHandle, SIGNAL(frameTypeChanged()), this,
                       SLOT(onFrameTypeChanged()));

  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()),
                       m_sceneViewer, SLOT(onToolSwitched()));

  assert(ret);

  // Aggiorno FPS al valore definito nel viewer corrente.
  // frameHandle->setPreviewFrameRate(m_fpsSlider->value());
  m_flipConsole->setActive(true);
  m_flipConsole->onPreferenceChanged("");
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::hideEvent(QHideEvent *event) {
  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TSceneHandle *sceneHandle    = app->getCurrentScene();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  TObjectHandle *objectHandle  = app->getCurrentObject();
  TXsheetHandle *xshHandle     = app->getCurrentXsheet();

  disconnect(xshHandle, SIGNAL(xsheetChanged()), this, SLOT(onSceneChanged()));

  disconnect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  disconnect(sceneHandle, SIGNAL(nameSceneChanged()), this,
             SLOT(changeWindowTitle()));
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(onSceneChanged()));
  disconnect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
             SLOT(onXshLevelSwitched(TXshLevel *)));
  disconnect(levelHandle, SIGNAL(xshLevelChanged()), this,
             SLOT(changeWindowTitle()));
  disconnect(levelHandle, SIGNAL(xshLevelTitleChanged()), this,
             SLOT(changeWindowTitle()));
  disconnect(levelHandle, SIGNAL(xshLevelChanged()), this,
             SLOT(updateFrameRange()));

  disconnect(frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(changeWindowTitle()));
  disconnect(frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(onFrameSwitched()));
  disconnect(frameHandle, SIGNAL(frameTypeChanged()), this,
             SLOT(onFrameTypeChanged()));

  disconnect(app->getCurrentTool(), SIGNAL(toolSwitched()), m_sceneViewer,
             SLOT(onToolSwitched()));
  disconnect(app->getCurrentScene(), SIGNAL(preferenceChanged(const QString &)),
             m_flipConsole, SLOT(onPreferenceChanged(const QString &)));

  m_flipConsole->setActive(false);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  repaint();
  m_sceneViewer->update();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret = true;

  TPanelTitleBarButtonSet *viewModeButtonSet;
  m_referenceModeBs = viewModeButtonSet = new TPanelTitleBarButtonSet();
  int x                                 = -232;
  int iconWidth                         = 20;
  TPanelTitleBarButton *button;

  // buttons for show / hide toggle for the field guide and the safe area
  TPanelTitleBarButtonForSafeArea *safeAreaButton =
      new TPanelTitleBarButtonForSafeArea(
          titleBar, getIconThemePath("actions/20/pane_safe.svg"));
  safeAreaButton->setToolTip(tr("Safe Area (Right Click to Select)"));
  titleBar->add(QPoint(x, 0), safeAreaButton);
  ret = ret && connect(safeAreaButton, SIGNAL(toggled(bool)),
                       CommandManager::instance()->getAction(MI_SafeArea),
                       SLOT(trigger()));
  ret = ret && connect(CommandManager::instance()->getAction(MI_SafeArea),
                       SIGNAL(triggered(bool)), safeAreaButton,
                       SLOT(setPressed(bool)));
  // initialize state
  safeAreaButton->setPressed(
      CommandManager::instance()->getAction(MI_SafeArea)->isChecked());

  button = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_grid.svg"));
  button->setToolTip(tr("Field Guide"));
  x += 1 + iconWidth;
  titleBar->add(QPoint(x, 0), button);
  ret = ret && connect(button, SIGNAL(toggled(bool)),
                       CommandManager::instance()->getAction(MI_FieldGuide),
                       SLOT(trigger()));
  ret = ret && connect(CommandManager::instance()->getAction(MI_FieldGuide),
                       SIGNAL(triggered(bool)), button, SLOT(setPressed(bool)));
  // initialize state
  button->setPressed(
      CommandManager::instance()->getAction(MI_FieldGuide)->isChecked());

  // view mode toggles
  button = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_table.svg"));
  button->setToolTip(tr("Camera Stand View"));
  x += 10 + iconWidth;
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::NORMAL_REFERENCE);
  button->setPressed(true);

  button = new TPanelTitleBarButton(titleBar,
                                    getIconThemePath("actions/20/pane_3d.svg"));
  button->setToolTip(tr("3D View"));
  x += +1 + iconWidth;
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA3D_REFERENCE);

  button = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_cam.svg"));
  button->setToolTip(tr("Camera View"));
  x += +1 + iconWidth;
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA_REFERENCE);
  ret = ret && connect(viewModeButtonSet, SIGNAL(selected(int)), m_sceneViewer,
                       SLOT(setReferenceMode(int)));

  // freeze button
  button = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_freeze.svg"));
  x += 10 + iconWidth;

  button->setToolTip(tr("Freeze"));  // RC1
  titleBar->add(QPoint(x, 0), button);
  ret = ret && connect(button, SIGNAL(toggled(bool)), m_sceneViewer,
                       SLOT(freeze(bool)));

  // preview toggles
  m_previewButton = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_preview.svg"));
  x += 10 + iconWidth;
  titleBar->add(QPoint(x, 0), m_previewButton);
  m_previewButton->setToolTip(tr("Preview"));
  ret = ret && connect(m_previewButton, SIGNAL(toggled(bool)),
                       SLOT(enableFullPreview(bool)));

  m_subcameraPreviewButton = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_subpreview.svg"));
  x += +1 + 24;  // width of pane_preview.svg = 24px

  titleBar->add(QPoint(x, 0), m_subcameraPreviewButton);
  m_subcameraPreviewButton->setToolTip(tr("Sub-camera Preview"));
  ret = ret && connect(m_subcameraPreviewButton, SIGNAL(toggled(bool)),
                       SLOT(enableSubCameraPreview(bool)));

  assert(ret);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::enableFullPreview(bool enabled) {
  m_subcameraPreviewButton->setPressed(false);
  m_sceneViewer->enablePreview(enabled ? SceneViewer::FULL_PREVIEW
                                       : SceneViewer::NO_PREVIEW);
  m_flipConsole->setProgressBarStatus(
      &Previewer::instance(false)->getProgressBarStatus());
  enableFlipConsoleForCamerastand(enabled);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::enableSubCameraPreview(bool enabled) {
  m_previewButton->setPressed(false);
  m_sceneViewer->enablePreview(enabled ? SceneViewer::SUBCAMERA_PREVIEW
                                       : SceneViewer::NO_PREVIEW);
  m_flipConsole->setProgressBarStatus(
      &Previewer::instance(true)->getProgressBarStatus());
  enableFlipConsoleForCamerastand(enabled);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::enableFlipConsoleForCamerastand(bool on) {
  m_flipConsole->enableButton(FlipConsole::eMatte, on, false);
  m_flipConsole->enableButton(FlipConsole::eSave, on, false);
  m_flipConsole->enableButton(FlipConsole::eCompare, on, false);
  m_flipConsole->enableButton(FlipConsole::eSaveImg, on, false);
  m_flipConsole->enableButton(FlipConsole::eGRed, on, false);
  m_flipConsole->enableButton(FlipConsole::eGGreen, on, false);
  m_flipConsole->enableButton(FlipConsole::eGBlue, on, false);
  m_flipConsole->enableButton(FlipConsole::eBlackBg, on, false);
  m_flipConsole->enableButton(FlipConsole::eWhiteBg, on, false);
  m_flipConsole->enableButton(FlipConsole::eCheckBg, on, false);

  m_flipConsole->enableProgressBar(on);
  m_flipConsole->enableBlanks(on);
  // m_flipConsole->update();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onXshLevelSwitched(TXshLevel *) {
  changeWindowTitle();
  m_sceneViewer->update();
  // If the level switched by using the level choose combo box in the film
  // strip,
  // the current level switches without change in the frame type (level or
  // scene).
  // For such case, update the frame range of the console here.
  if (TApp::instance()->getCurrentFrame()->isEditingLevel()) updateFrameRange();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onPlayingStatusChanged(bool playing) {
  if (playing) {
    m_playing = true;
  } else {
    m_playing = false;
    m_first   = true;
  }
  if (Preferences::instance()->getOnionSkinDuringPlayback()) return;
  OnionSkinMask osm =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  if (playing) {
    m_onionSkinActive = osm.isEnabled();
    if (m_onionSkinActive) {
      osm.enable(false);
      TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
      TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
    }
  } else {
    if (m_onionSkinActive) {
      osm.enable(true);
      TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
      TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
    }
  }
  m_sceneViewer->invalidateToolStatus();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::changeWindowTitle() {
  TApp *app = TApp::instance();
  // zoom = sqrt(m_sceneViewer->getViewMatrix().det());
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  if (!parentWidget()) return;
  int frame = app->getCurrentFrame()->getFrame();
  QString name;
  if (app->getCurrentFrame()->isEditingScene()) {
    TProject *project = scene->getProject();
    QString sceneName = QString::fromStdWString(scene->getSceneName());
    if (sceneName.isEmpty()) sceneName = tr("Untitled");
    if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");
    name = tr("Scene: ") + sceneName;
    if (frame >= 0)
      name =
          name + tr("   ::   Frame: ") + tr(std::to_string(frame + 1).c_str());
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      parentWidget()->setWindowTitle(name);
      return;
    }
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    TXshCell cell;
    if (app->getCurrentColumn()->getColumn() &&
        !app->getCurrentColumn()->getColumn()->getSoundColumn())
      cell = xsh->getCell(frame, col);
    if (cell.isEmpty()) {
      if (!m_sceneViewer->is3DView()) {
        TAffine aff = m_sceneViewer->getViewMatrix() *
                      m_sceneViewer->getNormalZoomScale().inv();
        if (m_sceneViewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
        if (m_sceneViewer->getIsFlippedY()) aff = aff * TScale(1, -1);
        name = name + tr("  ::  Zoom : ") +
               QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
        if (m_sceneViewer->getIsFlippedX() || m_sceneViewer->getIsFlippedY()) {
          name = name + tr(" (Flipped)");
        }
      }
      parentWidget()->setWindowTitle(name);
      return;
    }
    assert(cell.m_level.getPointer());
    TFilePath fp(cell.m_level->getName());
    QString imageName =
        QString::fromStdWString(fp.withFrame(cell.m_frameId).getWideString());
    name = name + tr("   ::   Level: ") + imageName;
  } else {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    if (level) {
      TFilePath fp(level->getName());
      QString imageName = QString::fromStdWString(
          fp.withFrame(app->getCurrentFrame()->getFid()).getWideString());
      name = name + tr("Level: ") + imageName;
    }
  }
  if (!m_sceneViewer->is3DView()) {
    TAffine aff = m_sceneViewer->getSceneMatrix() *
                  m_sceneViewer->getNormalZoomScale().inv();
    if (m_sceneViewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
    if (m_sceneViewer->getIsFlippedY()) aff = aff * TScale(1, -1);
    name = name + tr("  ::  Zoom : ") +
           QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
    if (m_sceneViewer->getIsFlippedX() || m_sceneViewer->getIsFlippedY()) {
      name = name + tr(" (Flipped)");
    }
  }

  parentWidget()->setWindowTitle(name);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::updateFrameRange() {
  TFrameHandle *fh  = TApp::instance()->getCurrentFrame();
  int frameIndex    = fh->getFrameIndex();
  int maxFrameIndex = fh->getMaxFrameIndex();
  if (frameIndex > maxFrameIndex) maxFrameIndex = frameIndex;
  m_flipConsole->setFrameRange(1, maxFrameIndex + 1, 1, frameIndex + 1);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::updateFrameMarkers() {
  int fromIndex, toIndex, dummy;
  XsheetGUI::getPlayRange(fromIndex, toIndex, dummy);
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  if (fh->isEditingLevel()) {
    fromIndex = 0;
    toIndex   = -1;
  }
  m_flipConsole->setMarkers(fromIndex, toIndex);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onSceneChanged() {
  updateFrameRange();
  updateFrameMarkers();
  changeWindowTitle();
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  assert(scene);
  // update fps only when the scene settings is changed
  m_flipConsole->setFrameRate(TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate(),
                              false);
  // vinz: perche veniva fatto?
  // m_flipConsole->updateCurrentFPS(scene->getProperties()->getOutputProperties()->getFrameRate());

  int frameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  if (m_keyFrameButton->getCurrentFrame() != frameIndex)
    m_keyFrameButton->setCurrentFrame(frameIndex);
  hasSoundtrack();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onSceneSwitched() {
  m_previewButton->setPressed(false);
  m_subcameraPreviewButton->setPressed(false);
  enableFlipConsoleForCamerastand(false);
  m_sceneViewer->enablePreview(SceneViewer::NO_PREVIEW);
  m_flipConsole->setChecked(FlipConsole::eDefineSubCamera, false);
  m_flipConsole->setFrameRate(TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());
  m_sceneViewer->setEditPreviewSubcamera(false);
  onSceneChanged();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onFrameSwitched() {
  int frameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  m_flipConsole->setCurrentFrame(frameIndex + 1);
  if (m_keyFrameButton->getCurrentFrame() != frameIndex)
    m_keyFrameButton->setCurrentFrame(frameIndex);

  if (m_playing && m_playSound) {
    if (m_first == true && hasSoundtrack()) {
      playAudioFrame(frameIndex);
    } else if (m_hasSoundtrack) {
      playAudioFrame(frameIndex);
    }
  }
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::onFrameTypeChanged() {
  if (TApp::instance()->getCurrentFrame()->getFrameType() ==
      TFrameHandle::LevelFrame) {
    if (m_sceneViewer->isPreviewEnabled()) {
      m_previewButton->setPressed(false);
      m_subcameraPreviewButton->setPressed(false);
      enableFlipConsoleForCamerastand(false);
      m_sceneViewer->enablePreview(SceneViewer::NO_PREVIEW);
    }
    CameraTestCheck::instance()->setIsEnabled(false);
    CleanupPreviewCheck::instance()->setIsEnabled(false);
  }

  m_flipConsole->setChecked(FlipConsole::eDefineSubCamera, false);
  m_sceneViewer->setEditPreviewSubcamera(false);

  updateFrameRange();
  updateFrameMarkers();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::playAudioFrame(int frame) {
  if (m_first) {
    m_first = false;
    m_fps   = TApp::instance()
                ->getCurrentScene()
                ->getScene()
                ->getProperties()
                ->getOutputProperties()
                ->getFrameRate();
    m_samplesPerFrame = m_sound->getSampleRate() / std::abs(m_fps);
  }
  if (!m_sound) return;
  m_viewerFps = m_flipConsole->getCurrentFps();
  double s0 = frame * m_samplesPerFrame, s1 = s0 + m_samplesPerFrame;

  // make the sound stop if the viewerfps is higher so the next sound can play
  // on time.
  if (m_fps < m_viewerFps)
    TApp::instance()->getCurrentXsheet()->getXsheet()->stopScrub();
  TApp::instance()->getCurrentXsheet()->getXsheet()->play(m_sound, s0, s1,
                                                          false);
}

//-----------------------------------------------------------------------------

bool SceneViewerPanel::hasSoundtrack() {
  if (m_sound != NULL) {
    m_sound         = NULL;
    m_hasSoundtrack = false;
    m_first         = true;
  }
  TXsheetHandle *xsheetHandle    = TApp::instance()->getCurrentXsheet();
  TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
  if (!m_sceneViewer->isPreviewEnabled()) prop->m_isPreview = true;
  try {
    m_sound = xsheetHandle->getXsheet()->makeSound(prop);
  } catch (TSoundDeviceException &e) {
    if (e.getType() == TSoundDeviceException::NoDevice) {
      std::cout << ::to_string(e.getMessage()) << std::endl;
    } else {
      throw TSoundDeviceException(e.getType(), e.getMessage());
    }
  }
  if (m_sound == NULL) {
    m_hasSoundtrack = false;
    return false;
  } else {
    m_hasSoundtrack = true;
    return true;
  }
}

void SceneViewerPanel::onButtonPressed(FlipConsole::EGadget button) {
  if (button == FlipConsole::eSound) {
    m_playSound = !m_playSound;
  }
}

void SceneViewerPanel::setFlipHButtonChecked(bool checked) {
  m_flipConsole->setChecked(FlipConsole::eFlipHorizontal, checked);
}

void SceneViewerPanel::setFlipVButtonChecked(bool checked) {
  m_flipConsole->setChecked(FlipConsole::eFlipVertical, checked);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::setVisiblePartsFlag(UINT flag) {
  m_visiblePartsFlag = flag;
  updateShowHide();
}

// SaveLoadQSettings
void SceneViewerPanel::save(QSettings &settings) const {
  settings.setValue("visibleParts", m_visiblePartsFlag);
}

void SceneViewerPanel::load(QSettings &settings) {
  m_visiblePartsFlag = settings.value("visibleParts", CVPARTS_ALL).toUInt();
  updateShowHide();
}
