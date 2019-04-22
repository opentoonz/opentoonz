
// TnzCore includes
#include "tconvert.h"
#include "tgeometry.h"
#include "tgl.h"
#include "trop.h"
#include "tstopwatch.h"
#include "tsystem.h"

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
#include "toonz/palettecontroller.h"
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/flipconsole.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/tooloptions.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "sceneviewer.h"
#include "xsheetdragtool.h"
#include "ruler.h"
#include "menubarcommandids.h"
#include "toolbar.h"

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
#include <QLineEdit>
#include <QSlider>
#include <QButtonGroup>
#include <QMenu>
#include <QToolBar>
#include <QMainWindow>
#include <QSettings>

#include "comboviewerpane.h"

using namespace DVGui;

//=============================================================================
//
// ComboViewerPanel
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
ComboViewerPanel::ComboViewerPanel(QWidget *parent, Qt::WindowFlags flags)
#else
ComboViewerPanel::ComboViewerPanel(QWidget *parent, Qt::WFlags flags)
#endif
    : QFrame(parent) {
  TApp *app = TApp::instance();

  setFrameStyle(QFrame::StyledPanel);
  setObjectName("ComboViewerPanel");

  // ToolBar
  m_toolbar = new Toolbar(this, false);
  // Tool Options
  m_toolOptions = new ToolOptions();
  m_toolOptions->setObjectName("ComboViewerToolOptions");

  // Viewer
  ImageUtils::FullScreenWidget *fsWidget =
      new ImageUtils::FullScreenWidget(this);
  fsWidget->setWidget(m_sceneViewer = new SceneViewer(fsWidget));
  m_sceneViewer->setIsStyleShortcutSwitchable();

#if defined(Q_OS_WIN) && (QT_VERSION >= 0x050500) && (QT_VERSION < 0x050600)
  //  Workaround for QTBUG-48288
  //  This code should be removed after updating Qt.
  //  Qt may crash in handling WM_SIZE of m_sceneViewer in splash.finish(&w)
  //  in main.cpp. To suppress sending WM_SIZE, set window position here.
  //  WM_SIZE will not be sent if window poistion is not changed.
  ::SetWindowPos(reinterpret_cast<HWND>(m_sceneViewer->winId()), HWND_TOP, 0, 0,
                 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif

  m_vRuler = new Ruler(this, m_sceneViewer, true);
  m_hRuler = new Ruler(this, m_sceneViewer, false);

  m_sceneViewer->setRulers(m_vRuler, m_hRuler);

  m_keyFrameButton = new ViewerKeyframeNavigator(this, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  // FlipConsole
  int buttons = FlipConsole::cFullConsole;
  // buttons &= (~FlipConsole::eSound);
  buttons &= (~FlipConsole::eFilledRaster);
  buttons &= (~FlipConsole::eDefineLoadBox);
  buttons &= (~FlipConsole::eUseLoadBox);

  /* --- layout --- */
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(m_toolbar, 0);
    mainLayout->addWidget(m_toolOptions, 0);

    QGridLayout *viewerL = new QGridLayout();
    viewerL->setMargin(0);
    viewerL->setSpacing(0);
    {
      viewerL->addWidget(m_vRuler, 1, 0);
      viewerL->addWidget(m_hRuler, 0, 1);
      viewerL->addWidget(fsWidget, 1, 1);
    }
    mainLayout->addLayout(viewerL, 1);
    m_flipConsole =
        new FlipConsole(mainLayout, buttons, false, m_keyFrameButton,
                        "SceneViewerConsole", this, true);
  }
  setLayout(mainLayout);

  m_flipConsole->enableButton(FlipConsole::eMatte, false, false);
  m_flipConsole->enableButton(FlipConsole::eSave, false, false);
  m_flipConsole->enableButton(FlipConsole::eCompare, false, false);
  m_flipConsole->enableButton(FlipConsole::eSaveImg, false, false);
  m_flipConsole->enableButton(FlipConsole::eGRed, false, false);
  m_flipConsole->enableButton(FlipConsole::eGGreen, false, false);
  m_flipConsole->enableButton(FlipConsole::eGBlue, false, false);
  // m_flipConsole->enableButton(FlipConsole::eSound, false, false);

  m_flipConsole->setFrameRate(app->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());
  m_flipConsole->setFrameHandle(TApp::instance()->getCurrentFrame());

  bool ret = true;

  // When zoom changed, only if the viewer is active, change window title.
  ret =
      ret && connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                     this, SLOT(onButtonPressed(FlipConsole::EGadget)));
  ret = connect(m_sceneViewer, SIGNAL(onZoomChanged()),
                SLOT(changeWindowTitle()));
  ret = ret && connect(m_sceneViewer, SIGNAL(previewToggled()),
                       SLOT(changeWindowTitle()));
  ret = ret &&
        connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
                TApp::instance()->getCurrentFrame(), SLOT(setPlaying(bool)));
  ret = ret && connect(m_flipConsole, SIGNAL(playStateChanged(bool)), this,
                       SLOT(onPlayingStatusChanged(bool)));
  ret = ret &&
        connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                m_sceneViewer, SLOT(onButtonPressed(FlipConsole::EGadget)));
  ret = ret && connect(m_sceneViewer, SIGNAL(previewStatusChanged()), this,
                       SLOT(update()));
  ret = ret && connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));

  assert(ret);

  m_flipConsole->setChecked(FlipConsole::eSound, true);
  m_playSound = m_flipConsole->isChecked(FlipConsole::eSound);

  // initial state of the parts
  m_visiblePartsFlag = CVPARTS_ALL;
  updateShowHide();

  setFocusProxy(m_sceneViewer);
}

//-----------------------------------------------------------------------------
/*! toggle show/hide of the widgets according to m_visibleFlag
*/

void ComboViewerPanel::updateShowHide() {
  // toolbar
  m_toolbar->setVisible(m_visiblePartsFlag & CVPARTS_TOOLBAR);
  // tool options bar
  m_toolOptions->setVisible(m_visiblePartsFlag & CVPARTS_TOOLOPTIONS);
  // flip console
  m_flipConsole->showHideAllParts(m_visiblePartsFlag & CVPARTS_FLIPCONSOLE);
  update();
}

//-----------------------------------------------------------------------------
/*! showing the show/hide commands
*/

void ComboViewerPanel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);
  addShowHideContextMenu(menu);
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));
  // actions
  QAction *toolbarSHAct     = showHideMenu->addAction(tr("Toolbar"));
  QAction *toolOptionsSHAct = showHideMenu->addAction(tr("Tool Options Bar"));
  QAction *flipConsoleSHAct = showHideMenu->addAction(tr("Console"));

  toolbarSHAct->setCheckable(true);
  toolbarSHAct->setChecked(m_visiblePartsFlag & CVPARTS_TOOLBAR);
  toolbarSHAct->setData((UINT)CVPARTS_TOOLBAR);

  toolOptionsSHAct->setCheckable(true);
  toolOptionsSHAct->setChecked(m_visiblePartsFlag & CVPARTS_TOOLOPTIONS);
  toolOptionsSHAct->setData((UINT)CVPARTS_TOOLOPTIONS);

  flipConsoleSHAct->setCheckable(true);
  flipConsoleSHAct->setChecked(m_visiblePartsFlag & CVPARTS_FLIPCONSOLE);
  flipConsoleSHAct->setData((UINT)CVPARTS_FLIPCONSOLE);

  QActionGroup *showHideActGroup = new QActionGroup(this);
  showHideActGroup->setExclusive(false);
  showHideActGroup->addAction(toolbarSHAct);
  showHideActGroup->addAction(toolOptionsSHAct);
  showHideActGroup->addAction(flipConsoleSHAct);

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

void ComboViewerPanel::onShowHideActionTriggered(QAction *act) {
  CV_Parts part = (CV_Parts)act->data().toUInt();
  assert(part < CVPARTS_End);

  m_visiblePartsFlag ^= part;

  updateShowHide();
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::onDrawFrame(
    int frame, const ImagePainter::VisualSettings &settings) {
  TApp *app = TApp::instance();
  m_sceneViewer->setVisual(settings);
  TFrameHandle *frameHandle = app->getCurrentFrame();

  if (m_sceneViewer->isPreviewEnabled()) {
    class Previewer *pr = Previewer::instance(m_sceneViewer->getPreviewMode() ==
                                              SceneViewer::SUBCAMERA_PREVIEW);

    pr->getRaster(frame - 1);  // the 'getRaster' starts the render of the frame
                               // is not already started
    int curFrame = frame;
    if (frameHandle->isPlaying() &&
        !pr->isFrameReady(
            frame - 1))  // stops on last rendered frame until current is ready!
    {
      while (frame > 0 && !pr->isFrameReady(frame - 1)) frame--;
      if (frame == 0)
        frame = curFrame;  // if no frame is ready, I stay on current...no use
                           // to rewind
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

ComboViewerPanel::~ComboViewerPanel() {}

//-----------------------------------------------------------------------------

void ComboViewerPanel::showEvent(QShowEvent *event) {
  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TSceneHandle *sceneHandle    = app->getCurrentScene();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  TObjectHandle *objectHandle  = app->getCurrentObject();
  TXsheetHandle *xshHandle     = app->getCurrentXsheet();

  bool ret = true;

  /*!
  onSceneChanged(): called when the scene changed
  - set new scene's FPS
  - update the range of frame slider with a new framehandle
  - set the marker
  - update key frames
  */
  ret =
      connect(xshHandle, SIGNAL(xsheetChanged()), this, SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneChanged()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneChanged()), this,
                       SLOT(onSceneChanged()));

  /*!
  changeWindowTitle(): called when the scene / level / frame is changed
  - chenge the title text
  */
  ret = ret && connect(sceneHandle, SIGNAL(nameSceneChanged()), this,
                       SLOT(changeWindowTitle()));
  ret =
      ret && connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
                     m_flipConsole, SLOT(onPreferenceChanged(const QString &)));

  ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(changeWindowTitle()));
  // onXshLevelSwitched(TXshLevel*)： changeWindowTitle() + updateFrameRange()
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
                       SLOT(onXshLevelSwitched(TXshLevel *)));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelTitleChanged()), this,
                       SLOT(changeWindowTitle()));
  // updateFrameRange(): update the frame slider's range
  ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
                       SLOT(updateFrameRange()));

  // onFrameTypeChanged(): reset the marker positions in the flip console
  ret = ret && connect(frameHandle, SIGNAL(frameTypeChanged()), this,
                       SLOT(onFrameTypeChanged()));

  // onFrameChanged(): update the flipconsole according to the current frame
  ret = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(onFrameChanged()));

  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()),
                       m_sceneViewer, SLOT(onToolSwitched()));

  assert(ret);

  m_flipConsole->setActive(true);
  m_flipConsole->onPreferenceChanged("");

  // refresh
  onSceneChanged();
  changeWindowTitle();
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::hideEvent(QHideEvent *event) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentFrame(), 0, this, 0);
  disconnect(app->getCurrentScene(), 0, this, 0);
  disconnect(app->getCurrentLevel(), 0, this, 0);
  disconnect(app->getCurrentObject(), 0, this, 0);
  disconnect(app->getCurrentXsheet(), 0, this, 0);

  disconnect(app->getCurrentTool(), SIGNAL(toolSwitched()), m_sceneViewer,
             SLOT(onToolSwitched()));
  disconnect(app->getCurrentScene(), SIGNAL(preferenceChanged(const QString &)),
             m_flipConsole, SLOT(onPreferenceChanged(const QString &)));

  m_flipConsole->setActive(false);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret = true;

  TPanelTitleBarButtonSet *viewModeButtonSet;
  m_referenceModeBs = viewModeButtonSet = new TPanelTitleBarButtonSet();
  int x                                 = -232;
  int iconWidth                         = 20;
  TPanelTitleBarButton *button;

  // buttons for show / hide toggle for the field guide and the safe area
  TPanelTitleBarButtonForSafeArea *safeAreaButton =
      new TPanelTitleBarButtonForSafeArea(
          titleBar, ":Resources/pane_safe_off.svg",
          ":Resources/pane_safe_over.svg", ":Resources/pane_safe_on.svg");
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

  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_grid_off.svg",
                                    ":Resources/pane_grid_over.svg",
                                    ":Resources/pane_grid_on.svg");
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
  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_table_off.svg",
                                    ":Resources/pane_table_over.svg",
                                    ":Resources/pane_table_on.svg");
  button->setToolTip(tr("Camera Stand View"));
  x += 10 + iconWidth;
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::NORMAL_REFERENCE);
  button->setPressed(true);

  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_3d_off.svg",
                                    ":Resources/pane_3d_over.svg",
                                    ":Resources/pane_3d_on.svg");
  button->setToolTip(tr("3D View"));
  x += 21;  // width of pane_table_off.svg = 20px
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA3D_REFERENCE);

  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_cam_off.svg",
                                    ":Resources/pane_cam_over.svg",
                                    ":Resources/pane_cam_on.svg");
  button->setToolTip(tr("Camera View"));
  x += 21;  // width of pane_3d_off.svg = 20px
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA_REFERENCE);
  ret = ret && connect(viewModeButtonSet, SIGNAL(selected(int)), m_sceneViewer,
                       SLOT(setReferenceMode(int)));

  // freeze button
  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_freeze_off.svg",
                                    ":Resources/pane_freeze_over.svg",
                                    ":Resources/pane_freeze_on.svg");
  x += 10 + 20;  // width of pane_cam_off.svg = 20px

  button->setToolTip(tr("Freeze"));  // RC1
  titleBar->add(QPoint(x, 0), button);
  ret = ret && connect(button, SIGNAL(toggled(bool)), m_sceneViewer,
                       SLOT(freeze(bool)));

  // preview toggles
  m_previewButton = new TPanelTitleBarButton(
      titleBar, ":Resources/pane_preview_off.svg",
      ":Resources/pane_preview_over.svg", ":Resources/pane_preview_on.svg");
  x += 10 + iconWidth;
  titleBar->add(QPoint(x, 0), m_previewButton);
  m_previewButton->setToolTip(tr("Preview"));
  ret = ret && connect(m_previewButton, SIGNAL(toggled(bool)),
                       SLOT(enableFullPreview(bool)));

  m_subcameraPreviewButton =
      new TPanelTitleBarButton(titleBar, ":Resources/pane_subpreview_off.svg",
                               ":Resources/pane_subpreview_over.svg",
                               ":Resources/pane_subpreview_on.svg");
  x += 26;  // width of pane_preview_off.svg = 25px

  titleBar->add(QPoint(x, 0), m_subcameraPreviewButton);
  m_subcameraPreviewButton->setToolTip(tr("Sub-camera Preview"));
  ret = ret && connect(m_subcameraPreviewButton, SIGNAL(toggled(bool)),
                       SLOT(enableSubCameraPreview(bool)));

  assert(ret);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::enableFullPreview(bool enabled) {
  m_subcameraPreviewButton->setPressed(false);
  m_sceneViewer->enablePreview(enabled ? SceneViewer::FULL_PREVIEW
                                       : SceneViewer::NO_PREVIEW);
  m_flipConsole->setProgressBarStatus(
      &Previewer::instance(false)->getProgressBarStatus());
  enableFlipConsoleForCamerastand(enabled);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::enableSubCameraPreview(bool enabled) {
  m_previewButton->setPressed(false);
  m_sceneViewer->enablePreview(enabled ? SceneViewer::SUBCAMERA_PREVIEW
                                       : SceneViewer::NO_PREVIEW);
  m_flipConsole->setProgressBarStatus(
      &Previewer::instance(true)->getProgressBarStatus());
  enableFlipConsoleForCamerastand(enabled);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::enableFlipConsoleForCamerastand(bool on) {
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
  update();
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::onXshLevelSwitched(TXshLevel *) {
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

void ComboViewerPanel::onPlayingStatusChanged(bool playing) {
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
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::changeWindowTitle() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  if (!parentWidget()) return;
  int frame = app->getCurrentFrame()->getFrame();

  // put the titlebar texts in this string
  QString name;

  // if the frame type is "scene editing"
  if (app->getCurrentFrame()->isEditingScene()) {
    QString sceneName = QString::fromStdWString(scene->getSceneName());
    if (sceneName.isEmpty()) sceneName = tr("Untitled");

    if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString(" *");
    name = tr("Scene: ") + sceneName;
    if (frame >= 0)
      name =
          name + tr("   ::   Frame: ") + tr(std::to_string(frame + 1).c_str());
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      if ((m_sceneViewer->getIsFlippedX() || m_sceneViewer->getIsFlippedY()) &&
          !m_sceneViewer->is3DView()) {
        name = name + tr(" (Flipped)");
      }
      parentWidget()->setWindowTitle(name);
      return;
    }
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(frame, col);
    if (cell.isEmpty()) {
      if ((m_sceneViewer->getIsFlippedX() || m_sceneViewer->getIsFlippedY()) &&
          !m_sceneViewer->is3DView()) {
        name = name + tr(" (Flipped)");
      }
      parentWidget()->setWindowTitle(name);
      return;
    }
    assert(cell.m_level.getPointer());
    TFilePath fp(cell.m_level->getName());
    QString imageName =
        QString::fromStdWString(fp.withFrame(cell.m_frameId).getWideString());
    name = name + tr("   ::   Level: ") + imageName;

    if (m_sceneViewer->isPreviewEnabled() && !m_sceneViewer->is3DView()) {
      TAffine aff                             = m_sceneViewer->getViewMatrix();
      if (m_sceneViewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
      if (m_sceneViewer->getIsFlippedY()) aff = aff * TScale(1, -1);
      name                                    = name + "  ::  Zoom : " +
             QString::number((int)(100.0 * sqrt(aff.det()) *
                                   m_sceneViewer->getDpiFactor())) +
             "%";
    }

    // If the current level exists and some option is set in the preference,
    // set the zoom value to the current level's dpi
    else if (Preferences::instance()
                 ->isActualPixelViewOnSceneEditingModeEnabled() &&
             TApp::instance()->getCurrentLevel()->getSimpleLevel() &&
             !CleanupPreviewCheck::instance()
                  ->isEnabled()  // cleanup preview must be OFF
             &&
             !CameraTestCheck::instance()  // camera test mode must be OFF
                                           // neither
                  ->isEnabled() &&
             !m_sceneViewer->is3DView()) {
      TAffine aff                             = m_sceneViewer->getViewMatrix();
      if (m_sceneViewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
      if (m_sceneViewer->getIsFlippedY()) aff = aff * TScale(1, -1);
      name                                    = name + "  ::  Zoom : " +
             QString::number((int)(100.0 * sqrt(aff.det()) *
                                   m_sceneViewer->getDpiFactor())) +
             "%";
    }

  }
  // if the frame type is "level editing"
  else {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    if (level) {
      TFilePath fp(level->getName());
      QString imageName = QString::fromStdWString(
          fp.withFrame(app->getCurrentFrame()->getFid()).getWideString());

      name = name + tr("Level: ") + imageName;
      if (!m_sceneViewer->is3DView()) {
        TAffine aff = m_sceneViewer->getViewMatrix();
        if (m_sceneViewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
        if (m_sceneViewer->getIsFlippedY()) aff = aff * TScale(1, -1);
        name                                    = name + "  ::  Zoom : " +
               QString::number((int)(100.0 * sqrt(aff.det()) *
                                     m_sceneViewer->getDpiFactor())) +
               "%";
      }
    }
  }
  if ((m_sceneViewer->getIsFlippedX() || m_sceneViewer->getIsFlippedY()) &&
      !m_sceneViewer->is3DView()) {
    name = name + tr(" (Flipped)");
  }
  parentWidget()->setWindowTitle(name);
}

//-----------------------------------------------------------------------------
/*! update the frame range according to the current frame type
*/
void ComboViewerPanel::updateFrameRange() {
  TFrameHandle *fh  = TApp::instance()->getCurrentFrame();
  int frameIndex    = fh->getFrameIndex();
  int maxFrameIndex = fh->getMaxFrameIndex();
  if (frameIndex > maxFrameIndex) frameIndex = maxFrameIndex;
  m_flipConsole->setFrameRange(1, maxFrameIndex + 1, 1, frameIndex + 1);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::onSceneChanged() {
  TFrameHandle *fh  = TApp::instance()->getCurrentFrame();
  int frameIndex    = fh->getFrameIndex();
  int maxFrameIndex = fh->getMaxFrameIndex();
  if (frameIndex > maxFrameIndex) maxFrameIndex = frameIndex;
  // update fps only when the scene settings is changed
  m_flipConsole->setFrameRate(TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate(),
                              false);
  // update the frame slider's range with new frameHandle
  m_flipConsole->setFrameRange(1, maxFrameIndex + 1, 1, frameIndex + 1);

  // set the markers
  int fromIndex, toIndex, dummy;
  XsheetGUI::getPlayRange(fromIndex, toIndex, dummy);
  m_flipConsole->setMarkers(fromIndex, toIndex);

  // update the key frames
  if (m_keyFrameButton && (m_keyFrameButton->getCurrentFrame() != frameIndex))
    m_keyFrameButton->setCurrentFrame(frameIndex);
  hasSoundtrack();
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::onSceneSwitched() {
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

void ComboViewerPanel::onFrameChanged() {
  int frameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  m_flipConsole->setCurrentFrame(frameIndex + 1);
  if (m_keyFrameButton && (m_keyFrameButton->getCurrentFrame() != frameIndex))
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
/*! reset the marker positions in the flip console
*/
void ComboViewerPanel::onFrameTypeChanged() {
  if (TApp::instance()->getCurrentFrame()->getFrameType() ==
          TFrameHandle::LevelFrame &&
      m_sceneViewer->isPreviewEnabled()) {
    m_previewButton->setPressed(false);
    m_subcameraPreviewButton->setPressed(false);
    enableFlipConsoleForCamerastand(false);
    m_sceneViewer->enablePreview(SceneViewer::NO_PREVIEW);
  }
  m_flipConsole->setChecked(FlipConsole::eDefineSubCamera, false);
  m_sceneViewer->setEditPreviewSubcamera(false);

  updateFrameRange();

  // if in the scene editing mode, get the preview marker positions
  if (TApp::instance()->getCurrentFrame()->isEditingScene()) {
    // set the markers
    int fromIndex, toIndex, dummy;
    XsheetGUI::getPlayRange(fromIndex, toIndex, dummy);
    m_flipConsole->setMarkers(fromIndex, toIndex);
  }
  // if in the level editing mode, ignore the preview marker
  else
    m_flipConsole->setMarkers(0, -1);
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::playAudioFrame(int frame) {
  if (m_first) {
    m_first = false;
    m_fps   = TApp::instance()
                ->getCurrentScene()
                ->getScene()
                ->getProperties()
                ->getOutputProperties()
                ->getFrameRate();
    m_samplesPerFrame = m_sound->getSampleRate() / abs(m_fps);
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

bool ComboViewerPanel::hasSoundtrack() {
  if (m_sound != NULL) {
    m_sound         = NULL;
    m_hasSoundtrack = false;
    m_first         = true;
  }
  TXsheetHandle *xsheetHandle    = TApp::instance()->getCurrentXsheet();
  TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
  if (!m_sceneViewer->isPreviewEnabled()) prop->m_isPreview = true;
  m_sound = xsheetHandle->getXsheet()->makeSound(prop);
  if (m_sound == NULL) {
    m_hasSoundtrack = false;
    return false;
  } else {
    m_hasSoundtrack = true;
    return true;
  }
}

void ComboViewerPanel::onButtonPressed(FlipConsole::EGadget button) {
  if (button == FlipConsole::eSound) {
    m_playSound = !m_playSound;
  }
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::setVisiblePartsFlag(UINT flag) {
  m_visiblePartsFlag = flag;
  updateShowHide();
}

// SaveLoadQSettings
void ComboViewerPanel::save(QSettings &settings) const {
  settings.setValue("visibleParts", m_visiblePartsFlag);
}

void ComboViewerPanel::load(QSettings &settings) {
  m_visiblePartsFlag = settings.value("visibleParts", CVPARTS_ALL).toUInt();
  updateShowHide();
}