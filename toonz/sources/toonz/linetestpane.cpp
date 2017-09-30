

#include "linetestpane.h"
#ifdef LINETEST
#include "xsheetdragtool.h"
#include "toutputproperties.h"
#include "tapp.h"
#include "toutputproperties.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"

#include <QThread>
#include <QStackedWidget>

using namespace DVGui;

//=============================================================================
// MixAudioThread
//-----------------------------------------------------------------------------

MixAudioThread::MixAudioThread(QObject *parent)
    : QThread(parent), m_abort(false), m_restart(false) {}

//-----------------------------------------------------------------------------

MixAudioThread::~MixAudioThread() {
  m_mutex.lock();
  m_abort = true;
  m_condition.wakeOne();
  m_mutex.unlock();
  wait();
}

//-----------------------------------------------------------------------------

void MixAudioThread::run() {
  forever {
    m_mutex.lock();
    int from = this->m_from;
    int to   = this->m_to;
    m_mutex.unlock();

    if (m_abort) return;

    TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
    prop->m_fromFrame              = from;
    prop->m_toFrame                = to;
    m_computedBuffer =
        TApp::instance()->getCurrentXsheet()->getXsheet()->makeSound(prop);
    m_buffer                       = TSoundTrackP();
    if (m_computedBuffer) m_buffer = m_computedBuffer->clone();

    if (!m_restart && m_buffer) emit computedBuffer();

    m_mutex.lock();
    if (!m_restart) m_condition.wait(&m_mutex);
    m_restart = false;
    m_mutex.unlock();
  }
}

//-----------------------------------------------------------------------------

void MixAudioThread::computeBuffer(int fromFrame, int toFrame) {
  QMutexLocker locker(&m_mutex);

  this->m_from = fromFrame;
  this->m_to   = toFrame;

  if (!isRunning()) {
    start(QThread::NormalPriority);
  } else {
    m_restart = true;
    m_condition.wakeOne();
  }
}

//=============================================================================
// LineTestPane
//-----------------------------------------------------------------------------

LineTestPane::LineTestPane(QWidget *parent, Qt::WFlags flags)
    : TPanel(parent)
    , m_flipConsole(0)
    , m_keyFrameButton(0)
    , m_mainTrack(0)
    , m_startTrack(0)
    , m_player(0)
    , m_trackStartFrame(0)
    , m_startPlayRange(-1)
    , m_endPlayRange(0)
    , m_bufferSize(0)
    , m_nextBufferSize(0) {
  bool ret     = true;
  QFrame *hbox = new QFrame(this);
  hbox->setFrameStyle(QFrame::StyledPanel);
  hbox->setObjectName("OnePixelMarginFrame");

  QVBoxLayout *mainLayout = new QVBoxLayout(hbox);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);

  // Viewer
  m_stackedWidget = new QStackedWidget(this);
  m_sceneViewer   = new SceneViewer(this);
  m_stackedWidget->addWidget(m_sceneViewer);
  m_lineTestViewer = new LineTestViewer(this);
  m_stackedWidget->addWidget(m_lineTestViewer);

  mainLayout->addWidget(m_stackedWidget, Qt::AlignCenter);

  TApp *app        = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(0, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  int buttons = FlipConsole::cFullConsole;

  buttons &= (~FlipConsole::eSound);
  buttons &= (~FlipConsole::eCheckBg);
  buttons &= (~FlipConsole::eWhiteBg);
  buttons &= (~FlipConsole::eBlackBg);
  buttons &= (~FlipConsole::eSave);
  buttons &= (~FlipConsole::eCompare);
  buttons &= (~FlipConsole::eSaveImg);
  buttons &= (~FlipConsole::eHisto);
  buttons &= (~FlipConsole::eCustomize);
  buttons &= (~FlipConsole::eMatte);
  buttons &= (~FlipConsole::eGRed);
  buttons &= (~FlipConsole::eGGreen);
  buttons &= (~FlipConsole::eGBlue);
  buttons &= (~FlipConsole::eRed);
  buttons &= (~FlipConsole::eGreen);
  buttons &= (~FlipConsole::eBlue);
  buttons &= (~FlipConsole::eDefineSubCamera);
  buttons &= (~FlipConsole::eDefineLoadBox);
  buttons &= (~FlipConsole::eUseLoadBox);

  m_flipConsole = new FlipConsole(mainLayout, buttons, false, m_keyFrameButton,
                                  "SceneViewerConsole");

  ret = ret && connect(m_sceneViewer, SIGNAL(onZoomChanged()),
                       SLOT(changeWindowTitle()));
  ret = ret && connect(m_lineTestViewer, SIGNAL(onZoomChanged()),
                       SLOT(changeWindowTitle()));

  ret = connect(m_flipConsole,
                SIGNAL(drawFrame(int, const ImagePainter::VisualSettings &)),
                this,
                SLOT(onDrawFrame(int, const ImagePainter::VisualSettings &)));
  ret = ret && connect(m_flipConsole, SIGNAL(playStateChanged(bool)), this,
                       SLOT(onPlayStateChanged(bool)));
  ret = ret &&
        connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                m_lineTestViewer, SLOT(onButtonPressed(FlipConsole::EGadget)));
  ret = ret &&
        connect(m_flipConsole, SIGNAL(buttonPressed(FlipConsole::EGadget)),
                m_sceneViewer, SLOT(onButtonPressed(FlipConsole::EGadget)));
  ret = ret && connect(m_flipConsole, SIGNAL(sliderReleased()), this,
                       SLOT(onFlipSliderReleased()));

  m_flipConsole->setFrameRate(app->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());

  hbox->setLayout(mainLayout);

  setWidget(hbox);

  initializeTitleBar(getTitleBar());

  ret = ret && connect(&m_mixAudioThread, SIGNAL(computedBuffer()), this,
                       SLOT(onComputedBuffer()), Qt::DirectConnection);

  ret = ret && connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));

  assert(ret);
  setCurrentViewType(0);
}

//-----------------------------------------------------------------------------

LineTestPane::~LineTestPane() {}

//-----------------------------------------------------------------------------

void LineTestPane::showEvent(QShowEvent *) {
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

  assert(ret);

  // Aggiorno FPS al valore definito nel viewer corrente.
  m_flipConsole->setActive(true);
}

//-----------------------------------------------------------------------------

void LineTestPane::hideEvent(QHideEvent *) {
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

  m_flipConsole->setActive(false);
}

//-----------------------------------------------------------------------------

int LineTestPane::computeBufferSize(int frame) {
  int bufferSize    = 0;
  double time       = 2.0;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TOutputProperties *outputSettings =
      scene->getProperties()->getOutputProperties();
  int frameRate = outputSettings->getFrameRate();
  if (m_endPlayRange > m_startPlayRange) {
    int range                        = m_endPlayRange - m_startPlayRange;
    time                             = double(range) / double(frameRate);
    int minuts                       = tround(time / 60.0);
    if (minuts == 0) minuts          = 1;
    while (time > 2.0 * minuts) time = time * 0.5;
  }
  bufferSize = tfloor(frameRate * time) + 1;
  // qDebug("BUFFER SIZE: %d", bufferSize);

  if (frame + bufferSize > m_endPlayRange) {
    bufferSize = m_endPlayRange - frame + 1;
    // qDebug("\n BUFFER SIZE: %d \n", bufferSize);
  }

  return bufferSize;
}

//-----------------------------------------------------------------------------

void LineTestPane::computeBuffer(int frame) {
  int startFrame = m_trackStartFrame + m_bufferSize;
  if (startFrame >= m_endPlayRange) {
    // qDebug("\n OPS \n");
    return;
  }
  // qDebug("COMPUTE BUFFER SIZE AT FRAME: %d", startFrame);
  m_nextBufferSize = computeBufferSize(startFrame);
  // qDebug("start frame: %d, start next buffer size: %d", startFrame,
  // m_nextBufferSize);
  m_mixAudioThread.computeBuffer(startFrame, startFrame + m_nextBufferSize);
}

//-----------------------------------------------------------------------------

void LineTestPane::initSound() {
  m_mainTrack               = 0;
  m_startTrack              = 0;
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  m_trackStartFrame         = frameHandle->getFrame();

  m_startPlayRange = 0;
  m_endPlayRange   = frameHandle->getMaxFrameIndex();
  int fromMarker, toMarker, step;
  if (XsheetGUI::getPlayRange(fromMarker, toMarker, step)) {
    m_startPlayRange = fromMarker;
    m_endPlayRange   = toMarker;
  }

  m_bufferSize = computeBufferSize(m_trackStartFrame);
  // qDebug("initSound()");
  // qDebug("m_startPlayRange: %d", m_startPlayRange);
  // qDebug("m_endPlayRange: %d", m_endPlayRange);
  // qDebug("start frame: %d, start bufferSize: %d", m_trackStartFrame,
  // m_bufferSize);

  if (xsh) {
    if (!m_player) m_player = new TSoundOutputDevice();
    // Compute the first seconds and then launch the thread to compute the next
    // buffer
    TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
    prop->m_fromFrame              = m_trackStartFrame;
    prop->m_toFrame                = m_trackStartFrame + m_bufferSize;
    m_mainTrack                    = xsh->makeSound(prop);
    if (m_trackStartFrame == m_startPlayRange) {
      // qDebug("set start track");
      m_startTrack      = m_mainTrack;
      m_startBufferSize = m_bufferSize;
    } else  // compute start track (it's necessary to avoid async in loop)
    {
      m_startBufferSize              = computeBufferSize(m_startPlayRange);
      TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
      prop->m_fromFrame              = m_startPlayRange;
      prop->m_toFrame                = m_startPlayRange + m_startBufferSize;
      m_startTrack                   = xsh->makeSound(prop);
    }
    computeBuffer(m_trackStartFrame);
  } else {
    m_mainTrack  = 0;
    m_startTrack = 0;
  }
}

//-----------------------------------------------------------------------------

void LineTestPane::playSound() {
  if (m_player && m_mainTrack) {
    try {
      int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
      double t0        = (double)(currentFrame - m_trackStartFrame) /
                  (double)(m_mainTrack->getSampleRate());
      // Pay attention at don't call play everytime (there is a big leak
      // otherwise !)
      double trackDuration = m_mainTrack->getDuration();
      // qDebug("trackDuration: %f", trackDuration);
      if (m_mainTrack && trackDuration != 0)
        if (trackDuration > t0) m_player->play(m_mainTrack, t0, trackDuration);
    } catch (TSoundDeviceException &e) {
    }
  }
}

//-----------------------------------------------------------------------------

void LineTestPane::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret = true;

  TPanelTitleBarButtonSet *viewModeButtonSet;
  viewModeButtonSet = new TPanelTitleBarButtonSet();
  int x             = -100;
  int iconWidth     = 17;
  TPanelTitleBarButton *button;

  m_cameraStandButton = new TPanelTitleBarButton(
      titleBar, ":Resources/pane_table_off.svg", ":Resources/pane_table_over.svg",
      ":Resources/pane_table_on.svg");
  m_cameraStandButton->setToolTip("Camera Stand View");
  x += 18 + iconWidth;
  titleBar->add(QPoint(x, 2), m_cameraStandButton);
  m_cameraStandButton->setButtonSet(viewModeButtonSet, 0);
  m_cameraStandButton->setPressed(true);

  m_previewButton = new TPanelTitleBarButton(
      titleBar, ":Resources/pane_preview_off.svg", ":Resources/pane_preview_over.svg",
      ":Resources/pane_preview_on.svg");
  m_previewButton->setToolTip(tr("Preview"));
  x += 5 + iconWidth;
  titleBar->add(QPoint(x, 2), m_previewButton);
  m_previewButton->setButtonSet(viewModeButtonSet, 1);

  ret = ret && connect(viewModeButtonSet, SIGNAL(selected(int)), this,
                       SLOT(setCurrentViewType(int)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void LineTestPane::onXshLevelSwitched(TXshLevel *) { changeWindowTitle(); }

//-----------------------------------------------------------------------------

void LineTestPane::changeWindowTitle() {
  TApp *app         = TApp::instance();
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
      name  = name + tr("   ::   Frame: ") + tr(toString(frame + 1).c_str());
    int col = app->getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      setWindowTitle(name);
      return;
    }
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(frame, col);
    if (cell.isEmpty()) {
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
  if (m_stackedWidget->currentIndex() == 0) {
    TAffine aff = m_sceneViewer->getViewMatrix() *
                  m_sceneViewer->getNormalZoomScale().inv();
    name = name + "  ::  Zoom : " +
           QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
  } else {
    TAffine aff = m_lineTestViewer->getViewMatrix() *
                  m_lineTestViewer->getNormalZoomScale().inv();
    name = name + "  ::  Zoom : " +
           QString::number(tround(100.0 * sqrt(aff.det()))) + "%";
  }

  setWindowTitle(name);
}

//-----------------------------------------------------------------------------

void LineTestPane::updateFrameRange() {
  TApp *app         = TApp::instance();
  TFrameHandle *fh  = app->getCurrentFrame();
  int frameIndex    = fh->getFrameIndex();
  int maxFrameIndex = fh->getMaxFrameIndex();
  m_flipConsole->setFrameRange(1, maxFrameIndex + 1, 1, frameIndex + 1);
}

//-----------------------------------------------------------------------------

void LineTestPane::updateFrameMarkers() {
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

void LineTestPane::setCurrentViewType(int index) {
  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  if (index == 1 && frameHandle->getFrameType() == TFrameHandle::LevelFrame)
    frameHandle->setFrame(frameHandle->getFrameIndex());
  m_stackedWidget->setCurrentIndex(index);
  if (index == 0) {
    CommandManager::instance()->enable("MI_ViewTable", true);
    CommandManager::instance()->enable("MI_FieldGuide", true);
    CommandManager::instance()->enable("MI_SafeArea", true);
    CommandManager::instance()->enable("MI_RasterizePli", true);
    CommandManager::instance()->enable("MI_ViewColorcard", true);
    CommandManager::instance()->enable("MI_ViewCamera", true);
    m_flipConsole->enableButton(FlipConsole::eFilledRaster, false, false);
  } else {
    CommandManager::instance()->enable("MI_ViewTable", false);
    CommandManager::instance()->enable("MI_FieldGuide", false);
    CommandManager::instance()->enable("MI_SafeArea", false);
    CommandManager::instance()->enable("MI_RasterizePli", false);
    CommandManager::instance()->enable("MI_ViewColorcard", false);
    CommandManager::instance()->enable("MI_ViewCamera", false);
    if (m_flipConsole)
      m_flipConsole->enableButton(FlipConsole::eFilledRaster, true, false);
  }
  changeWindowTitle();
}

//-----------------------------------------------------------------------------

void LineTestPane::onDrawFrame(int frame,
                               const ImagePainter::VisualSettings &settings) {
  TApp *app                 = TApp::instance();
  TFrameHandle *frameHandle = app->getCurrentFrame();

  assert(frame >= 0);
  if (frame != frameHandle->getFrameIndex() + 1) {
    int oldFrame = frameHandle->getFrame();
    frameHandle->setCurrentFrame(frame);
    if (!m_mainTrack && !frameHandle->isPlaying() &&
        !frameHandle->isEditingLevel() && oldFrame != frameHandle->getFrame())
      frameHandle->scrubXsheet(
          frame - 1, frame - 1,
          TApp::instance()->getCurrentXsheet()->getXsheet());

    // qDebug("frame: %d",frame);
    if (m_mainTrack && frameHandle->isPlaying() &&
        !frameHandle->isEditingLevel()) {
      if (frame == m_startPlayRange + 1) {
        if (m_startTrack) {
          m_mainTrack       = m_startTrack;
          m_trackStartFrame = frame;
          // qDebug("1. play at m_trackStartFrame: %d", m_trackStartFrame);
          playSound();

          m_bufferSize = m_startBufferSize;
          computeBuffer(frame);
        } else {
          // qDebug("2. play at m_trackStartFrame: %d", m_trackStartFrame);
          assert(0);
          initSound();
        }
      }
      int frameElapsed = frame - m_trackStartFrame - 1;
      if (frameElapsed >= (m_bufferSize)) {
        if (m_buffer) m_mainTrack = m_buffer->clone();

        m_trackStartFrame = frame;
        // qDebug("PLAY SOUND AT FRAME: %d", frame);
        playSound();

        // qDebug("3. play at m_trackStartFrame: %d", m_trackStartFrame);
        m_bufferSize = m_nextBufferSize;
        computeBuffer(frame);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void LineTestPane::onComputedBuffer() {
  m_buffer = TSoundTrackP();
  m_buffer = m_mixAudioThread.getBuffer();
}

//-----------------------------------------------------------------------------

void LineTestPane::onPlayStateChanged(bool value) {
  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  bool wasPlaying           = frameHandle->isPlaying();
  frameHandle->setPlaying(value);
  if (value && !frameHandle->isEditingLevel()) {
    if (m_player && wasPlaying) {
      m_player->close();
      if (m_mainTrack) m_mainTrack   = TSoundTrackP();
      if (m_startTrack) m_startTrack = TSoundTrackP();
      if (m_buffer) m_buffer         = TSoundTrackP();
    }
    initSound();
    playSound();
  } else if (m_player) {
    m_player->close();
    if (m_mainTrack) m_mainTrack   = TSoundTrackP();
    if (m_startTrack) m_startTrack = TSoundTrackP();
    if (m_buffer) m_buffer         = TSoundTrackP();
  }
}

//-----------------------------------------------------------------------------

void LineTestPane::onSceneChanged() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  assert(scene);
  m_flipConsole->updateCurrentFPS(
      scene->getProperties()->getOutputProperties()->getFrameRate());

  updateFrameRange();
  updateFrameMarkers();
  changeWindowTitle();

  int frameIndex = app->getCurrentFrame()->getFrameIndex();
  if (m_keyFrameButton->getCurrentFrame() != frameIndex)
    m_keyFrameButton->setCurrentFrame(frameIndex);
}

//-----------------------------------------------------------------------------

void LineTestPane::onSceneSwitched() {
  if (!isHidden()) onSceneChanged();
  m_flipConsole->setFrameRate(TApp::instance()
                                  ->getCurrentScene()
                                  ->getScene()
                                  ->getProperties()
                                  ->getOutputProperties()
                                  ->getFrameRate());
}

//-----------------------------------------------------------------------------

void LineTestPane::onFrameSwitched() {
  int frameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  m_flipConsole->setCurrentFrame(frameIndex + 1);
  if (m_keyFrameButton->getCurrentFrame() != frameIndex)
    m_keyFrameButton->setCurrentFrame(frameIndex);
}

//-----------------------------------------------------------------------------

void LineTestPane::onFrameTypeChanged() {
  if (TApp::instance()->getCurrentFrame()->getFrameType() ==
      TFrameHandle::LevelFrame) {
    if (m_stackedWidget->currentIndex() != 0) {
      m_cameraStandButton->setPressed(true);
      m_previewButton->setPressed(false);
      setCurrentViewType(0);
    }
  }

  updateFrameRange();
  updateFrameMarkers();
}

//-----------------------------------------------------------------------------

void LineTestPane::onFlipSliderReleased() {
  TApp::instance()->getCurrentXsheet()->getXsheet()->stopScrub();
}

#endif
