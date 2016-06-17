

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
#include "toonz/txsheethandle.h"
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

#include "viewerpane.h"

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
    : TPanel(parent) {
  QFrame *hbox = new QFrame(this);
  hbox->setFrameStyle(QFrame::StyledPanel);
  hbox->setObjectName("ViewerPanel");

  QVBoxLayout *mainLayout = new QVBoxLayout(hbox);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);

  // Viewer
  QWidget *viewer      = new QWidget(hbox);
  QGridLayout *viewerL = new QGridLayout(viewer);

  ImageUtils::FullScreenWidget *fsWidget =
      new ImageUtils::FullScreenWidget(viewer);

  fsWidget->setWidget(m_sceneViewer = new SceneViewer(fsWidget));

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

  int buttons = FlipConsole::cFullConsole;

  buttons &= (~FlipConsole::eSound);
  buttons &= (~FlipConsole::eFilledRaster);
  buttons &= (~FlipConsole::eDefineLoadBox);
  buttons &= (~FlipConsole::eUseLoadBox);

  m_flipConsole = new FlipConsole(mainLayout, buttons, false, m_keyFrameButton,
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
  ret = ret &&
        connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                m_sceneViewer, SLOT(onButtonPressed(FlipConsole::EGadget)));

  ret = ret && connect(m_sceneViewer, SIGNAL(previewStatusChanged()), this,
                       SLOT(update()));

  ret = ret && connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));

  assert(ret);

  m_flipConsole->setFrameRate(app->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());
  updateFrameRange(), updateFrameMarkers();

  hbox->setLayout(mainLayout);

  setWidget(hbox);

  initializeTitleBar(
      getTitleBar());  // note: initializeTitleBar() refers to m_sceneViewer
}

//-----------------------------------------------------------------------------

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

  assert(frame >= 0);
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

void SceneViewerPanel::showEvent(QShowEvent *) {
  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TSceneHandle *sceneHandle    = app->getCurrentScene();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  TObjectHandle *objectHandle  = app->getCurrentObject();
  TXsheetHandle *xshHandle     = app->getCurrentXsheet();

  updateFrameRange();

  bool ret = true;

  ret = ret && connect(xshHandle, SIGNAL(xsheetChanged()), this,
                       SLOT(onSceneChanged()));

  ret = ret && connect(sceneHandle, SIGNAL(sceneChanged()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(nameSceneChanged()), this,
                       SLOT(changeWindowTitle()));

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

  ret = ret && connect(sceneHandle, SIGNAL(preferenceChanged()), m_flipConsole,
                       SLOT(onPreferenceChanged()));
  m_flipConsole->onPreferenceChanged();

  assert(ret);

  // Aggiorno FPS al valore definito nel viewer corrente.
  // frameHandle->setPreviewFrameRate(m_fpsSlider->value());
  m_flipConsole->setActive(true);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::hideEvent(QHideEvent *) {
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

  disconnect(sceneHandle, SIGNAL(preferenceChanged()), m_flipConsole,
             SLOT(onPreferenceChanged()));

  m_flipConsole->setActive(false);
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  repaint();
  m_sceneViewer->updateGL();
}

//-----------------------------------------------------------------------------

void SceneViewerPanel::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret = true;

  TPanelTitleBarButtonSet *viewModeButtonSet;
  m_referenceModeBs = viewModeButtonSet = new TPanelTitleBarButtonSet();
  int x                                 = -188;
  int iconWidth                         = 17;
  TPanelTitleBarButton *button;
  button = new TPanelTitleBarButton(titleBar, ":Resources/freeze.png",
                                    ":Resources/freeze_over.png",
                                    ":Resources/freeze_on.png");
  button->setToolTip(tr("Freeze"));
  titleBar->add(QPoint(x, 2), button);
  ret = ret && connect(button, SIGNAL(toggled(bool)), m_sceneViewer,
                       SLOT(freeze(bool)));
  ret = ret && connect(m_sceneViewer, SIGNAL(freezeStateChanged(bool)), button,
                       SLOT(setPressed(bool)));

  button = new TPanelTitleBarButton(titleBar, ":Resources/standard.png",
                                    ":Resources/standard_over.png",
                                    ":Resources/standard_on.png");
  button->setToolTip(tr("Camera Stand View"));
  x += 18 + iconWidth;
  titleBar->add(QPoint(x, 2), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::NORMAL_REFERENCE);
  button->setPressed(true);

  button = new TPanelTitleBarButton(titleBar, ":Resources/3D.png",
                                    ":Resources/3D_over.png",
                                    ":Resources/3D_on.png");
  button->setToolTip(tr("3D View"));
  x += 5 + iconWidth;
  titleBar->add(QPoint(x, 2), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA3D_REFERENCE);

  button = new TPanelTitleBarButton(titleBar, ":Resources/view_camera.png",
                                    ":Resources/view_camera_over.png",
                                    ":Resources/view_camera_on.png");
  button->setToolTip(tr("Camera View"));
  x += 5 + iconWidth;
  titleBar->add(QPoint(x, 2), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA_REFERENCE);
  ret = ret && connect(viewModeButtonSet, SIGNAL(selected(int)), m_sceneViewer,
                       SLOT(setReferenceMode(int)));

  m_previewButton = new TPanelTitleBarButton(
      titleBar, ":Resources/viewpreview.png", ":Resources/viewpreview_over.png",
      ":Resources/viewpreview_on.png");
  x += 18 + iconWidth;
  titleBar->add(QPoint(x, 2), m_previewButton);
  m_previewButton->setToolTip(tr("Preview"));
  ret = ret && connect(m_previewButton, SIGNAL(toggled(bool)),
                       SLOT(enableFullPreview(bool)));

  m_subcameraPreviewButton =
      new TPanelTitleBarButton(titleBar, ":Resources/subcamera_preview.png",
                               ":Resources/subcamera_preview_over.png",
                               ":Resources/subcamera_preview_on.png");
  x += 5 + iconWidth;
  titleBar->add(QPoint(x, 2), m_subcameraPreviewButton);
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

void SceneViewerPanel::onXshLevelSwitched(TXshLevel *) { changeWindowTitle(); }

//-----------------------------------------------------------------------------

void SceneViewerPanel::changeWindowTitle() {
  TApp *app = TApp::instance();
  // zoom = sqrt(m_sceneViewer->getViewMatrix().det());
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  int frame = app->getCurrentFrame()->getFrame();
  QString name;
  if (app->getCurrentFrame()->isEditingScene()) {
    QString sceneName = QString::fromStdWString(scene->getSceneName());
    if (sceneName.isEmpty()) sceneName = tr("Untitled");
    if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");
    name = tr("Scene: ") + sceneName;
    if (frame >= 0)
      name =
          name + tr("   ::   Frame: ") + tr(std::to_string(frame + 1).c_str());
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      setWindowTitle(name);
      return;
    }
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(frame, col);
    if (cell.isEmpty()) {
      TAffine aff = m_sceneViewer->getViewMatrix() *
                    m_sceneViewer->getNormalZoomScale().inv();
      name = name + tr("  ::  Zoom : ") +
             QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
      setWindowTitle(name);
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
    name = name + tr("  ::  Zoom : ") +
           QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
  }

  setWindowTitle(name);
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
  // vinz: perche veniva fatto?
  // m_flipConsole->updateCurrentFPS(scene->getProperties()->getOutputProperties()->getFrameRate());

  int frameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  if (m_keyFrameButton->getCurrentFrame() != frameIndex)
    m_keyFrameButton->setCurrentFrame(frameIndex);
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
