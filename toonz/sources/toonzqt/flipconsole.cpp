

#include "toonzqt/flipconsole.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvscrollwidget.h"
#include "toonzqt/gutil.h"
#include "toonzqt/flipconsoleowner.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/tframehandle.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tconvert.h"
#include "timagecache.h"
#include "trop.h"

#include "../toonz/tapp.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QFrame>
#include <QSlider>
#include <QTimerEvent>
#include <QToolButton>
#include <QPainter>
#include <QMouseEvent>
#include <QIcon>
#include <QAction>
#include <QWidgetAction>
#include <QStyle>
#include <QStylePainter>
#include <QStyleOption>
#include <QStyleOptionFrameV3>
#include <QSettings>
#include <QPushButton>
#include <QScrollBar>

using namespace DVGui;

//==========================================================================================
//    Preliminary stuff - local namespace
//==========================================================================================
TEnv::IntVar FlipBookWhiteBgToggle("FlipBookWhiteBgToggle", 1);
TEnv::IntVar FlipBookBlackBgToggle("FlipBookBlackBgToggle", 0);
TEnv::IntVar FlipBookCheckBgToggle("FlipBookCheckBgToggle", 0);
namespace {
// Please refer to the "qss/standard/standard.qss" file for explanations of the
// following properties.

int PBHeight;

QImage PBOverlay;
QImage PBMarker;

int PBColorMarginLeft   = 0;
int PBColorMarginTop    = 0;
int PBColorMarginRight  = 0;
int PBColorMarginBottom = 0;

int PBMarkerMarginLeft  = 0;
int PBMarkerMarginRight = 0;

QColor PBBaseColor       = QColor(235, 235, 235);
QColor PBNotStartedColor = QColor(210, 40, 40);
QColor PBStartedColor    = QColor(220, 160, 160);
QColor PBFinishedColor   = QColor(235, 235, 235);
}

//-----------------------------------------------------------------------------

int FlipSlider::getPBHeight() const { return PBHeight; }
void FlipSlider::setPBHeight(int height) {
  setFixedHeight(height);
  PBHeight = height;
}

QImage FlipSlider::getPBOverlay() const { return PBOverlay; }
void FlipSlider::setPBOverlay(const QImage &img) { PBOverlay = img; }

QImage FlipSlider::getPBMarker() const { return PBMarker; }
void FlipSlider::setPBMarker(const QImage &img) { PBMarker = img; }

int FlipSlider::getPBColorMarginLeft() const { return PBColorMarginLeft; }
void FlipSlider::setPBColorMarginLeft(int margin) {
  PBColorMarginLeft = margin;
}

int FlipSlider::getPBColorMarginTop() const { return PBColorMarginTop; }
void FlipSlider::setPBColorMarginTop(int margin) { PBColorMarginTop = margin; }

int FlipSlider::getPBColorMarginRight() const { return PBColorMarginRight; }
void FlipSlider::setPBColorMarginRight(int margin) {
  PBColorMarginRight = margin;
}

int FlipSlider::getPBColorMarginBottom() const { return PBColorMarginBottom; }
void FlipSlider::setPBColorMarginBottom(int margin) {
  PBColorMarginBottom = margin;
}

int FlipSlider::getPBMarkerMarginLeft() const { return PBMarkerMarginLeft; }
void FlipSlider::setPBMarkerMarginLeft(int margin) {
  PBMarkerMarginLeft = margin;
}

int FlipSlider::getPBMarkerMarginRight() const { return PBMarkerMarginRight; }
void FlipSlider::setPBMarkerMarginRight(int margin) {
  PBMarkerMarginRight = margin;
}

QColor FlipSlider::getBaseColor() const { return PBBaseColor; }
void FlipSlider::setBaseColor(const QColor &color) { PBBaseColor = color; }

QColor FlipSlider::getNotStartedColor() const { return PBNotStartedColor; }
void FlipSlider::setNotStartedColor(const QColor &color) {
  PBNotStartedColor = color;
}

QColor FlipSlider::getStartedColor() const { return PBStartedColor; }
void FlipSlider::setStartedColor(const QColor &color) {
  PBStartedColor = color;
}

QColor FlipSlider::getFinishedColor() const { return PBFinishedColor; }
void FlipSlider::setFinishedColor(const QColor &color) {
  PBFinishedColor = color;
}

FlipConsole *FlipConsole::m_currentConsole = 0;
QList<FlipConsole *> FlipConsole::m_visibleConsoles;
bool FlipConsole::m_isLinkedPlaying = false;
bool FlipConsole::m_areLinked       = false;

//==========================================================================================

PlaybackExecutor::PlaybackExecutor() : m_fps(25), m_abort(false) {}

//-----------------------------------------------------------------------------

void PlaybackExecutor::resetFps(int fps) { m_fps = fps; }

//-----------------------------------------------------------------------------

void PlaybackExecutor::run() {
  // (Daniele)
  // We'll build the fps considering an interval of roughly 1 second (the last
  // one).
  // However, the fps should be sampled at a faster rate. Each sample is taken
  // at
  // 1/4 second, and the last 4 samples data are stored to keep trace of the
  // last
  // second of playback.

  TStopWatch timer;
  timer.start();

  TUINT32 timeResolution =
      250;  // Use a sufficient sampling resolution (currently 1/4 sec).
            // Fps calculation is made once per sample.

  int fps = m_fps, currSample = 0;
  TUINT32 playedFramesCount = 0;
  TUINT32 loadedInstant, nextSampleInstant = timeResolution;
  TUINT32 sampleTotalLoadingTime = 0;

  TUINT32 lastFrameCounts[4] = {0, 0, 0,
                                0};  // Keep the last 4 'played frames' counts.
  TUINT32 lastSampleInstants[4] = {0, 0, 0,
                                   0};  // Same for the last sampling instants
  TUINT32 lastLoadingTimes[4] = {0, 0, 0,
                                 0};  // Same for total sample loading times

  double targetFrameTime =
      1000.0 / abs(m_fps);  // User-required time between frames

  TUINT32 emissionInstant = 0;  // Effective instant in which loading is invoked
  double emissionInstantD = 0.0;  // Double precision version of the above

  double lastLoadingTime = 0.0;  // Mean frame loading time in the last sample

  while (!m_abort) {
    emissionInstant = timer.getTotalTime();

    // Draw the next frame
    emit nextFrame(fps);  // Show the next frame, telling
                          // currently measured fps

    if (FlipConsole::m_areLinked) {
      // In case there are linked consoles, update them too.
      // Their load time must be included in the fps calculation.
      int i, consolesCount = FlipConsole::m_visibleConsoles.size();
      for (i = 0; i < consolesCount; ++i) {
        FlipConsole *console = FlipConsole::m_visibleConsoles.at(i);
        if (console->isLinkable() && console != FlipConsole::m_currentConsole)
          console->playbackExecutor().emitNextFrame(m_fps < 0 ? -fps : fps);
      }
    }

    //-------- Each nextFrame() blocks until the frame has been shown ---------

    ++playedFramesCount;
    loadedInstant = timer.getTotalTime();
    sampleTotalLoadingTime += (loadedInstant - emissionInstant);

    // Recalculate data only after the specified time resolution has passed.
    if (loadedInstant > nextSampleInstant) {
      // Sampling instant. Perform calculations.

      // Store values
      TUINT32 framesCount = playedFramesCount - lastFrameCounts[currSample];
      TUINT32 elapsedTime = loadedInstant - lastSampleInstants[currSample];
      double loadingTime =
          (sampleTotalLoadingTime - lastLoadingTimes[currSample]) /
          (double)framesCount;

      lastFrameCounts[currSample]    = playedFramesCount;
      lastSampleInstants[currSample] = loadedInstant;
      lastLoadingTimes[currSample]   = sampleTotalLoadingTime;

      currSample        = (currSample + 1) % 4;
      nextSampleInstant = loadedInstant + timeResolution;

      // Rebuild current fps
      fps             = troundp((1000 * framesCount) / (double)elapsedTime);
      targetFrameTime = 1000.0 / abs(m_fps);  // m_fps could have changed...

      // In case the playback is too slow to keep the required pace, reset the
      // emission timeline.
      // Otherwise, it should be kept as the difference needs to be compensated
      // to get the required fps.
      if ((int)emissionInstant - (int)emissionInstantD >
          20)  // Reset beyond, say, 20 msecs tolerance.
        emissionInstantD = (double)loadedInstant - loadingTime;
      else
        emissionInstantD +=
            lastLoadingTime -
            loadingTime;  // Otherwise, just adapt to the new loading time

      lastLoadingTime = loadingTime;
    }

    // Calculate the new emission instant
    emissionInstant = std::max((int)(emissionInstantD += targetFrameTime), 0);

    // Sleep until the next emission instant has been reached
    while (timer.getTotalTime() < emissionInstant) msleep(1);
  }

  m_abort = false;
}

//==========================================================================================

FlipSlider::FlipSlider(QWidget *parent)
    : QAbstractSlider(parent), m_enabled(false), m_progressBarStatus(0) {
  setObjectName("FlipSlider");
  setOrientation(Qt::Horizontal);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

//-----------------------------------------------------------------------------

void FlipSlider::paintEvent(QPaintEvent *ev) {
  QPainter p(this);

  // Draw the progress status colorbar
  QRect sliderRect(QPoint(), size());
  QRect colorRect(sliderRect.adjusted(PBMarkerMarginLeft, PBColorMarginTop,
                                      -PBMarkerMarginRight,
                                      -PBColorMarginBottom));

  int val, maxValuePlusStep = maximum() + singleStep();
  int colorWidth = colorRect.width(), colorHeight = colorRect.height();

  p.setPen(Qt::NoPen);
  int currPos = PBColorMarginLeft, nextPos;

  // paint the base of slider
  if (m_enabled && m_progressBarStatus && !m_progressBarStatus->empty()) {
    unsigned int i, pbStatusSize = m_progressBarStatus->size();
    for (i = 0, val = minimum() + singleStep(); i < pbStatusSize;
         ++i, val += singleStep()) {
      nextPos = sliderPositionFromValue(minimum(), maxValuePlusStep, val,
                                        colorWidth) +
                PBMarkerMarginLeft;
      if (i == pbStatusSize - 1) nextPos += PBMarkerMarginRight;
      p.fillRect(currPos, PBColorMarginTop, nextPos - currPos, colorHeight,
                 ((*m_progressBarStatus)[i] == PBFrameStarted)
                     ? PBStartedColor
                     : ((*m_progressBarStatus)[i] == PBFrameFinished)
                           ? PBFinishedColor
                           : PBNotStartedColor);
      currPos = nextPos;
    }

    // Draw frames outside the pb
    if (val < maximum())
      p.fillRect(currPos, PBColorMarginTop,
                 width() - PBColorMarginRight - currPos, colorHeight,
                 PBNotStartedColor);
  } else
    p.fillRect(PBColorMarginLeft, PBColorMarginTop,
               sliderRect.width() - PBColorMarginLeft - PBColorMarginRight,
               colorHeight, PBBaseColor);

  // Draw the PB Overlay
  int overlayInnerWidth =
      PBOverlay.width() - PBColorMarginLeft - PBColorMarginRight;
  int markerInnerWidth =
      PBMarker.width() - PBMarkerMarginLeft - PBMarkerMarginRight;

  p.drawImage(QRect(0, 0, PBColorMarginLeft, height()), PBOverlay,
              QRect(0, 0, PBColorMarginLeft, PBOverlay.height()));
  p.drawImage(QRect(PBColorMarginLeft, 0,
                    sliderRect.width() - PBColorMarginLeft - PBColorMarginRight,
                    height()),
              PBOverlay, QRect(PBColorMarginLeft, 0, overlayInnerWidth,
                               PBOverlay.height()));
  p.drawImage(
      QRect(width() - PBColorMarginRight, 0, PBColorMarginRight, height()),
      PBOverlay, QRect(PBOverlay.width() - PBColorMarginRight, 0,
                       PBColorMarginRight, PBOverlay.height()));

  // Draw the position marker
  currPos = sliderPositionFromValue(minimum(), maxValuePlusStep, value(),
                                    colorWidth) +
            PBMarkerMarginLeft;
  nextPos = sliderPositionFromValue(minimum(), maxValuePlusStep,
                                    value() + singleStep(), colorWidth) +
            PBMarkerMarginLeft;

  p.drawImage(
      QRect(currPos - PBMarkerMarginLeft, 0, PBMarkerMarginLeft, height()),
      PBMarker, QRect(0, 0, PBMarkerMarginLeft, PBMarker.height()));
  p.drawImage(
      QRect(currPos, 0, nextPos - currPos, height()), PBMarker,
      QRect(PBMarkerMarginLeft, 0, markerInnerWidth, PBMarker.height()));
  p.drawImage(QRect(nextPos, 0, PBMarkerMarginRight, height()), PBMarker,
              QRect(PBMarker.width() - PBMarkerMarginRight, 0,
                    PBMarkerMarginRight, PBMarker.height()));
}

//-----------------------------------------------------------------------------

inline int FlipSlider::sliderPositionFromValue(int min, int max, int val,
                                               int span) {
  return tceil(span * ((val - min) / (double)(max - min)));
}

//-----------------------------------------------------------------------------

inline int FlipSlider::sliderValueFromPosition(int min, int max, int step,
                                               int pos, int span) {
  int colorBarPos     = pos - PBColorMarginLeft;
  int colorSpan       = span - PBColorMarginLeft - PBColorMarginRight;
  int tempRelativePos = (max - min + step) * (colorBarPos / (double)colorSpan);
  return min + (tempRelativePos - tempRelativePos % step);
}

//-----------------------------------------------------------------------------

inline int FlipSlider::pageStepVal(int val) {
  return tcrop(value() + pageStep() * tsign(val - value()), minimum(),
               maximum());
}

//-----------------------------------------------------------------------------

// Mouse Press behaviour:
//  a) If middle button, just put frame to cursor position
//  b) If left button, and cursor on current frame pos, do like (a)
//  c) If left button, and cursor NOT on curr.. perform a page up/down on
//     the side of cursor pos
void FlipSlider::mousePressEvent(QMouseEvent *me) {
  emit flipSliderPressed();
  int cursorValue = sliderValueFromPosition(minimum(), maximum(), singleStep(),
                                            me->pos().x(), width());
  if (me->button() == Qt::MidButton)
    if (cursorValue == value())
      setSliderDown(true);
    else {
      // Move the page step
      setValue(pageStepVal(cursorValue));
    }

  else if (me->button() == Qt::LeftButton && cursorValue != value())
    setValue(cursorValue);
}

//-----------------------------------------------------------------------------

void FlipSlider::mouseMoveEvent(QMouseEvent *me) {
  if (isSliderDown() || me->buttons() & Qt::LeftButton) {
    int cursorValue = sliderValueFromPosition(
        minimum(), maximum(), singleStep(), me->pos().x(), width());
    setValue(cursorValue);
  }
}

//-----------------------------------------------------------------------------

void FlipSlider::mouseReleaseEvent(QMouseEvent *me) {
  setSliderDown(false);
  emit flipSliderReleased();
}

//=============================================================================

enum {
  eShowCompare         = 0x001,
  eShowBg              = 0x002,
  eShowFramerate       = 0x004,
  eShowVcr             = 0x008,
  eShowcolorFilter     = 0x010,
  eShowCustom          = 0x020,
  eShowHisto           = 0x040,
  eShowSave            = 0x080,
  eShowDefineSubCamera = 0x100,
  eShowFilledRaster    = 0x200,
  eShowDefineLoadBox   = 0x400,
  eShowUseLoadBox      = 0x800,
  eShowHowMany         = 0x1000
};

FlipConsole::FlipConsole(QVBoxLayout *mainLayout, UINT gadgetsMask,
                         bool isLinkable, QWidget *customWidget,
                         const QString &customizeId,
                         FlipConsoleOwner *consoleOwner, bool enableBlanks)
    : m_gadgetsMask(gadgetsMask)
    , m_from(1)
    , m_to(1)
    , m_step(1)
    , m_currentFrame(1)
    , m_framesCount(1)
    , m_settings()
    , m_fps(24)
    , m_sceneFps(24)
    , m_isPlay(false)
    , m_reverse(false)
    , m_doubleRed(0)
    , m_doubleGreen(0)
    , m_doubleBlue(0)
    , m_doubleRedAction(0)
    , m_doubleGreenAction(0)
    , m_doubleBlueAction(0)
    , m_fpsSlider(0)
    , m_markerFrom(0)
    , m_markerTo(-1)
    , m_playbackExecutor()
    , m_drawBlanksEnabled(enableBlanks)
    , m_blanksCount(0)
    , m_blankColor(TPixel::Transparent)
    , m_blanksToDraw(0)
    , m_isLinkable(isLinkable)
    , m_customAction(0)
    , m_customizeMask(eShowHowMany - 1)
    , m_fpsLabelAction(0)
    , m_fpsSliderAction(0)
    , m_fpsFieldAction(0)
    , m_fpsField(0)
    , m_customizeId(customizeId)
    , m_histoSep(0)
    , m_filledRasterSep(0)
    , m_bgSep(0)
    , m_vcrSep(0)
    , m_compareSep(0)
    , m_saveSep(0)
    , m_colorFilterSep(0)
    , m_subcamSep(0)
    , m_playToolBar(0)
    , m_colorFilterGroup(0)
    , m_fpsLabel(0)
    , m_consoleOwner(consoleOwner)
    , m_enableBlankFrameButton(0) {
  QString s                    = QSettings().value(m_customizeId).toString();
  if (s != "") m_customizeMask = s.toUInt();

  if (m_gadgetsMask == 0) return;

  // mainLayout->setMargin(1);
  // mainLayout->setSpacing(0);

  // create toolbars other than frame slider
  if (m_gadgetsMask & (~eFrames)) {
    createPlayToolBar(customWidget != 0);

    m_playToolBarContainer = new ToolBarContainer();

    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setMargin(0);
    hLayout->setSpacing(0);
    hLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    {
      DvScrollWidget *scrollableContainer = new DvScrollWidget;
      scrollableContainer->setWidget(m_playToolBar);
      hLayout->addWidget(scrollableContainer);

      // show fps
      if (m_gadgetsMask & eRate) {
        QFrame *fpsSliderFrame = createFpsSlider();
        hLayout->addWidget(fpsSliderFrame, 1);
      }
    }
    m_playToolBarContainer->setLayout(hLayout);

    mainLayout->addWidget(m_playToolBarContainer);
  }

  // create frame slider
  if (m_gadgetsMask & eFrames) {
    m_frameSliderFrame = createFrameSlider();
    mainLayout->addWidget(m_frameSliderFrame);
  }

  if (customWidget) {
    m_customAction = m_playToolBar->addWidget(customWidget);
    m_customSep    = m_playToolBar->addSeparator();
  }

  applyCustomizeMask();

  bool ret = connect(&m_playbackExecutor, SIGNAL(nextFrame(int)), this,
                     SLOT(onNextFrame(int)), Qt::BlockingQueuedConnection);

  assert(ret);

  // parent->setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void FlipConsole::showHideAllParts(bool isShow) {
  m_playToolBarContainer->setVisible(isShow);
  m_frameSliderFrame->setVisible(isShow);
}

//-----------------------------------------------------------------------------

void showEvent(QShowEvent *);
void hideEvent(QHideEvent *);

void FlipConsole::makeCurrent() {
  if (m_currentConsole == this) return;
  int i = m_visibleConsoles.indexOf(this);
  if (i >= 0) m_visibleConsoles.takeAt(i);
  m_visibleConsoles.append(this);
  m_currentConsole = this;
}

//-----------------------------------------------------------------------------

void FlipConsole::setActive(bool active) {
  if (active)
    makeCurrent();
  else {
    pressButton(ePause);
    int i = m_visibleConsoles.indexOf(this);
    if (i >= 0) m_visibleConsoles.takeAt(i);
    if (m_currentConsole == this) {
      if (!m_visibleConsoles.empty())
        m_currentConsole = m_visibleConsoles.last();
      else
        m_currentConsole = 0;
    }
  }
}

//-----------------------------------------------------------------------------

#define LX 21
#define LY 17
class DoubleButton final : public QToolButton {
  QAction *m_firstAction, *m_secondAction;
  QIcon::Mode m_firstMode, m_secondMode;
  QIcon::State m_firstState, m_secondState;
  bool m_enabled;

public:
  DoubleButton(QAction *firstAction, QAction *secondAction, QWidget *parent = 0)
      : QToolButton(parent)
      , m_firstAction(firstAction)
      , m_secondAction(secondAction)
      , m_firstMode(QIcon::Normal)
      , m_secondMode(QIcon::Normal)
      , m_firstState(QIcon::Off)
      , m_secondState(QIcon::Off)
      , m_enabled(true) {
    setFixedSize(LX, LY);
    setMouseTracking(true);
  }
  void setEnabledSecondButton(bool state) {
    if (!state && m_secondAction->isChecked()) m_secondAction->trigger();
    m_enabled = state;
    update();
  }

protected:
  void paintEvent(QPaintEvent *e) override {
    QPainter p(this);

    p.drawPixmap(0, 0,
                 m_firstAction->icon().pixmap(
                     QSize(LX, LY / 2),
                     m_firstAction->isChecked() ? QIcon::Normal : m_firstMode,
                     m_firstAction->isChecked() ? QIcon::On : m_firstState));

    QIcon::Mode mode =
        m_enabled ? (m_secondAction->isChecked() ? QIcon::Normal : m_secondMode)
                  : QIcon::Disabled;
    QIcon::State state =
        m_enabled ? (m_secondAction->isChecked() ? QIcon::On : m_secondState)
                  : QIcon::Off;

    p.drawPixmap(0, LY / 2 + 1,
                 m_secondAction->icon().pixmap(QSize(LX, LY / 2), mode, state));
  }

  void mousePressEvent(QMouseEvent *e) override {
    QRect firstActionRect(0, 0, LX, LY / 2);
    QRect secondActionRect(0, LY / 2 + 1, LX, LY / 2);

    QPoint pos = e->pos();
    if (firstActionRect.contains(pos)) {
      m_firstAction->trigger();
    } else {
      if (m_enabled) m_secondAction->trigger();
    }
    update();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    QPoint pos = e->pos();
    QRect firstActionRect(0, 0, LX, LY / 2);
    QRect secondActionRect(0, LY / 2 + 1, LX, LY / 2);

    m_firstState  = QIcon::Off;
    m_secondState = QIcon::Off;
    m_firstMode   = QIcon::Normal;
    m_secondMode  = QIcon::Normal;

    if (firstActionRect.contains(pos)) {
      m_firstMode = QIcon::Active;
      setToolTip(m_firstAction->toolTip());
    } else if (secondActionRect.contains(pos) && m_enabled) {
      m_secondMode = QIcon::Active;
      setToolTip(m_secondAction->toolTip());
    }
    update();
  }

  void leaveEvent(QEvent *e) override {
    m_firstMode   = QIcon::Normal;
    m_firstState  = QIcon::Off;
    m_secondMode  = QIcon::Normal;
    m_secondState = QIcon::Off;

    update();

    QToolButton::leaveEvent(e);
  }
};

//-----------------------------------------------------------------------------

void FlipConsole::enableButton(UINT button, bool enable, bool doShowHide) {
#if defined(MACOSX)  // on mac, the sound playback in flip is broken..
  if (button == eSound) enable = false;
#endif

  if (!m_playToolBar) return;

  QList<QAction *> list = m_playToolBar->actions();
  int i;
  for (i = 0; i < (int)list.size(); i++)
    if (list[i]->data().toUInt() == button) {
      if (button == eSound)
        if (doShowHide)
          m_soundSep->setVisible(enable);
        else
          m_soundSep->setEnabled(enable);
      if (button == eHisto) {
        if (doShowHide)
          m_histoSep->setVisible(enable && m_customizeMask & eShowHisto);
        else
          m_histoSep->setEnabled(enable);
      }
      if (doShowHide)
        list[i]->setVisible(enable);
      else
        list[i]->setEnabled(enable);
      if (!enable && list[i]->isChecked()) pressButton((EGadget)button);

      return;
    }

  // double buttons are special, they are not accessible directly from the
  // playtoolbar...
  switch ((EGadget)button) {
  case eGRed:
    if (m_doubleRed) m_doubleRed->setEnabledSecondButton(enable);
    break;
  case eGGreen:
    if (m_doubleGreen) m_doubleGreen->setEnabledSecondButton(enable);
    break;
  case eGBlue:
    if (m_doubleBlue) m_doubleBlue->setEnabledSecondButton(enable);
    break;
  }
}

//----------------------------------------------------------------------------

void FlipConsole::toggleLinked() {
  m_areLinked = !m_areLinked;

  int i;
  FlipConsole *playingConsole = 0;
  for (i = 0; i < m_visibleConsoles.size(); i++) {
    playingConsole = m_visibleConsoles.at(i);
    if (playingConsole->m_isLinkable &&
        playingConsole->m_playbackExecutor.isRunning())
      break;
  }

  if (i == m_visibleConsoles.size()) return;

  // if we are here, flip is playing!
  m_isLinkedPlaying = m_areLinked;
  int button =
      m_areLinked ? (playingConsole->m_isPlay ? ePlay : eLoop) : ePause;

  for (i = 0; i < m_visibleConsoles.size(); i++) {
    FlipConsole *console = m_visibleConsoles.at(i);
    if (console->m_isLinkable && console != playingConsole) {
      console->setChecked(button, true);
      console->doButtonPressed(button);
    }
  }
}

//----------------------------------------------------------------------------

bool FlipConsole::drawBlanks(int from, int to) {
  if (m_blanksCount == 0 || m_isPlay || m_framesCount <= 1) return false;

  // enable blanks only when the blank button is pressed
  if (m_enableBlankFrameButton && !m_enableBlankFrameButton->isChecked())
    return false;

  if (m_blanksToDraw > 1 ||
      (m_blanksToDraw == 0 &&
       ((m_reverse && m_currentFrame - m_step < from) ||
        (!m_reverse &&
         m_currentFrame + m_step >
             to))))  // we are on the last frame of the loop
  {
    m_blanksToDraw = (m_blanksToDraw == 0 ? m_blanksCount : m_blanksToDraw - 1);
    m_settings.m_blankColor     = m_blankColor;
    m_settings.m_drawBlankFrame = true;
    m_consoleOwner->onDrawFrame(from, m_settings);
    m_settings.m_drawBlankFrame = false;
    return true;
  }

  m_blanksToDraw = 0;
  return false;
}

//----------------------------------------------------------------------------

void FlipConsole::onNextFrame(int fps) {
  if (fps < 0)  // can be negative only if is a linked console; it means that
                // the master console is playing backward
  {
    bool reverse = m_reverse;
    m_reverse    = true;
    fps          = -fps;
    playNextFrame();
    m_reverse = reverse;
  } else
    playNextFrame();

  if (fps == -1) return;
  if (m_fpsLabel)
    m_fpsLabel->setText(tr(" FPS ") + QString::number(fps * tsign(m_fps)) +
                        "/");
  if (m_fpsField) {
    if (fps == abs(m_fps))
      m_fpsField->setLineEditBackgroundColor(Qt::green);
    else
      m_fpsField->setLineEditBackgroundColor(Qt::red);
  }
}

//----------------------------------------------------------------------------

void FlipConsole::playNextFrame() {
  int from = m_from, to = m_to;
  if (m_markerFrom <= m_markerTo) from = m_markerFrom, to = m_markerTo;

  if (m_framesCount == 0 ||
      (m_isPlay && m_currentFrame == (m_reverse ? from : to))) {
    doButtonPressed(ePause);
    setChecked(m_isPlay ? ePlay : eLoop, false);
    setChecked(ePause, true);
    if (Preferences::instance()->rewindAfterPlaybackEnabled())
      m_currentFrame = (m_reverse ? to : from);
    emit playStateChanged(false);
  } else {
    if (drawBlanks(from, to)) return;

    if (m_reverse)
      m_currentFrame =
          ((m_currentFrame - m_step < from) ? to : m_currentFrame - m_step);
    else
      m_currentFrame =
          ((m_currentFrame + m_step > to) ? from : m_currentFrame + m_step);
  }

  m_currFrameSlider->setValue(m_currentFrame);
  m_editCurrFrame->setText(QString::number(m_currentFrame));
  m_settings.m_blankColor        = TPixel::Transparent;
  m_settings.m_recomputeIfNeeded = true;
  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
}

//-----------------------------------------------------------------------------

void FlipConsole::updateCurrentFPS(int val) {
  setCurrentFPS(val);
  m_fpsSlider->setValue(m_fps);
}

//-----------------------------------------------------------------------------

void FlipConsole::setFrameRate(int val, bool forceUpdate) {
  if (m_sceneFps != val || forceUpdate) {
    if (!m_fpsSlider) return;
    m_fpsSlider->setValue(val);
    setCurrentFPS(val);
  }
  m_sceneFps = val;
}

//-----------------------------------------------------------------------------

void FlipConsole::setCurrentFPS(bool dragging) {
  setCurrentFPS(m_fpsField->getValue());
  m_fpsSlider->setValue(m_fps);
}

//-----------------------------------------------------------------------------

void FlipConsole::setCurrentFPS(int val) {
  if (m_fps == val) return;

  if (val == 0) val = 1;
  m_fps             = val;
  m_fpsField->setValue(m_fps);

  if (m_playbackExecutor.isRunning() || m_isLinkedPlaying)
    m_reverse = (val < 0);

  if (m_fpsLabel) m_fpsLabel->setText(tr(" FPS "));
  if (m_fpsField) m_fpsField->setLineEditBackgroundColor(Qt::transparent);

  m_playbackExecutor.resetFps(m_fps);
}

//-----------------------------------------------------------------------------

void FlipConsole::createButton(UINT buttonMask, const char *iconStr,
                               const QString &tip, bool checkable,
                               QActionGroup *group) {
  QIcon icon      = createQIcon(iconStr);
  QAction *action = new QAction(icon, tip, m_playToolBar);
  action->setData(QVariant(buttonMask));
  action->setCheckable(checkable);
  if (group) group->addAction(action);
  m_actions[(EGadget)buttonMask] = action;
  m_playToolBar->addAction(action);
}

//-----------------------------------------------------------------------------

QAction *FlipConsole::createCheckedButtonWithBorderImage(
    UINT buttonMask, const char *iconStr, const QString &tip, bool checkable,
    QActionGroup *group, const char *cmdId) {
  QIcon icon            = createQIcon(iconStr);
  QWidgetAction *action = new QWidgetAction(m_playToolBar);
  action->setIcon(icon);
  action->setToolTip(tip);
  action->setData(QVariant(buttonMask));
  action->setCheckable(checkable);
  if (group) group->addAction(action);
  QToolButton *button = new QToolButton(m_playToolBar);
  button->setDefaultAction(action);

  m_buttons[(EGadget)buttonMask] = button;

  if (cmdId) {
    QAction *a = CommandManager::instance()->getAction(cmdId);
    if (a) button->addAction(a);
  }

  action->setDefaultWidget(button);
  button->setObjectName("chackableButtonWithImageBorder");
  connect(button, SIGNAL(triggered(QAction *)), this,
          SLOT(onButtonPressed(QAction *)));
  // connect(action, SIGNAL(toggled(bool)), button, SLOT(setChecked(bool)));
  m_playToolBar->addAction(action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *FlipConsole::createDoubleButton(
    UINT buttonMask1, UINT buttonMask2, const char *iconStr1,
    const char *iconStr2, const QString &tip1, const QString &tip2,
    QActionGroup *group, DoubleButton *&widget) {
  QAction *action1 = new QAction(createQIcon(iconStr1), tip1, m_playToolBar);
  QAction *action2 = new QAction(createQIcon(iconStr2), tip2, m_playToolBar);
  m_actions[(EGadget)buttonMask1] = action1;
  m_actions[(EGadget)buttonMask2] = action2;

  action1->setData(QVariant(buttonMask1));
  action1->setCheckable(true);
  action2->setData(QVariant(buttonMask2));
  action2->setCheckable(true);

  if (group) {
    group->addAction(action1);
    group->addAction(action2);
  }

  widget = new DoubleButton(action1, action2, this);
  return m_playToolBar->addWidget(widget);

  // m_playToolBar->addAction(action1);
  // m_playToolBar->addAction(action2);
}

//-----------------------------------------------------------------------------

void FlipConsole::createOnOffButton(UINT buttonMask, const char *iconStr,
                                    const QString &tip, QActionGroup *group) {
  QIcon icon      = createQIconOnOff(iconStr);
  QAction *action = new QAction(icon, tip, m_playToolBar);
  action->setData(QVariant(buttonMask));
  action->setCheckable(true);
  if (group) group->addAction(action);
  m_playToolBar->addAction(action);
  m_actions[(EGadget)buttonMask] = action;
}

//----------------------------------------------------------------------------------------------

void FlipConsole::addMenuItem(UINT id, const QString &text, QMenu *menu) {
  QAction *a = new QAction(text, menu);
  a->setCheckable(true);
  a->setChecked(id & m_customizeMask);
  a->setData(QVariant(id));
  menu->addAction(a);
}

//----------------------------------------------------------------------------------------------

void FlipConsole::onCustomizeButtonPressed(QAction *a) {
  UINT id = a->data().toUInt();
  if (a->isChecked())
    m_customizeMask = m_customizeMask | id;
  else
    m_customizeMask = m_customizeMask & (~id);

  QSettings().setValue(m_customizeId, QString::number(m_customizeMask));

  applyCustomizeMask();
}

//----------------------------------------------------------------------------------------------
void FlipConsole::applyCustomizeMask() {
  enableButton(eSave, m_customizeMask & eShowSave);
  // if(m_saveSep)
  //  m_saveSep->setVisible(m_customizeMask&eShowSave);

  enableButton(eSaveImg, m_customizeMask & eShowCompare);
  enableButton(eCompare, m_customizeMask & eShowCompare);
  if (m_compareSep) m_compareSep->setVisible(m_customizeMask & eShowCompare);

  enableButton(eDefineSubCamera, m_customizeMask & eShowDefineSubCamera);
  enableButton(eDefineLoadBox, m_customizeMask & eShowDefineLoadBox);
  enableButton(eUseLoadBox, m_customizeMask & eShowUseLoadBox);
  if (m_subcamSep)
    m_subcamSep->setVisible(m_customizeMask & eShowDefineSubCamera ||
                            m_customizeMask & eShowDefineLoadBox ||
                            m_customizeMask & eShowUseLoadBox);

  enableButton(eWhiteBg, m_customizeMask & eShowBg);
  enableButton(eBlackBg, m_customizeMask & eShowBg);
  enableButton(eCheckBg, m_customizeMask & eShowBg);
  if (m_bgSep) m_bgSep->setVisible(m_customizeMask & eShowBg);

  if (m_fpsLabel && m_fpsSlider && m_fpsField) {
    m_fpsLabel->setVisible(m_customizeMask & eShowFramerate);
    m_fpsSlider->setVisible(m_customizeMask & eShowFramerate);
    m_fpsField->setVisible(m_customizeMask & eShowFramerate);
  }

  enableButton(eFirst, m_customizeMask & eShowVcr);
  enableButton(ePrev, m_customizeMask & eShowVcr);
  enableButton(ePause, m_customizeMask & eShowVcr);
  enableButton(ePlay, m_customizeMask & eShowVcr);
  enableButton(eLoop, m_customizeMask & eShowVcr);
  enableButton(eNext, m_customizeMask & eShowVcr);
  enableButton(eLast, m_customizeMask & eShowVcr);

  if (m_vcrSep) m_vcrSep->setVisible(m_customizeMask & eShowVcr);

  enableButton(eMatte, m_customizeMask & eShowcolorFilter);
  enableButton(eHisto, m_customizeMask & eShowHisto);
  if (m_histoSep) m_histoSep->setVisible(m_customizeMask & eShowHisto);

  if (m_doubleRedAction) {
    m_doubleRedAction->setVisible(m_customizeMask & eShowcolorFilter);
    m_doubleGreenAction->setVisible(m_customizeMask & eShowcolorFilter);
    m_doubleBlueAction->setVisible(m_customizeMask & eShowcolorFilter);
  } else {
    enableButton(eRed, m_customizeMask & eShowcolorFilter);
    enableButton(eGreen, m_customizeMask & eShowcolorFilter);
    enableButton(eBlue, m_customizeMask & eShowcolorFilter);
  }

  if (m_colorFilterGroup)
    m_colorFilterGroup->setVisible(m_customizeMask & eShowcolorFilter);

  if (m_colorFilterSep)
    m_colorFilterSep->setVisible(m_customizeMask & eShowcolorFilter);

  if (m_customAction) {
    bool visible = bool(m_customizeMask & eShowCustom);

    m_customAction->setVisible(visible);
    m_customSep->setVisible(visible);
  }

  enableButton(eFilledRaster, m_customizeMask & eShowFilledRaster);
  if (m_filledRasterSep)
    m_filledRasterSep->setVisible(m_customizeMask & eShowFilledRaster);

  update();
}

//----------------------------------------------------------------------------------------------

void FlipConsole::createCustomizeMenu(bool withCustomWidget) {
  if (m_gadgetsMask & eCustomize) {
    QIcon icon          = createQIcon("options");
    QToolButton *button = new QToolButton();
    button->setIcon(icon);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setObjectName("flipCustomize");

    QMenu *menu = new QMenu();
    button->setMenu(menu);

    m_playToolBar->addWidget(button);
    m_playToolBar->addSeparator();

    if (m_gadgetsMask & eSave) addMenuItem(eShowSave, tr("Save"), menu);

    if (m_gadgetsMask & eSaveImg || m_gadgetsMask & eCompare)
      addMenuItem(eShowCompare, tr("Snapshot"), menu);

    if (m_gadgetsMask & eDefineSubCamera)
      addMenuItem(eShowDefineSubCamera, tr("Define Sub-camera"), menu);
    if (m_gadgetsMask & eDefineLoadBox)
      addMenuItem(eShowDefineLoadBox, tr("Define Loading Box"), menu);
    if (m_gadgetsMask & eUseLoadBox)
      addMenuItem(eShowUseLoadBox, tr("Use Loading Box"), menu);

    if (m_gadgetsMask & eWhiteBg || m_gadgetsMask & eBlackBg ||
        m_gadgetsMask & eCheckBg)
      addMenuItem(eShowBg, tr("Background Colors"), menu);
    if (m_gadgetsMask & eRate)
      addMenuItem(eShowFramerate, tr("Framerate"), menu);

    addMenuItem(eShowVcr, tr("Playback Controls"), menu);

    if ((m_gadgetsMask & eRed) || (m_gadgetsMask & eGreen) ||
        (m_gadgetsMask & eBlue) || (m_gadgetsMask & eMatte))
      addMenuItem(eShowcolorFilter, tr("Color Channels"), menu);

    if (withCustomWidget) addMenuItem(eShowCustom, tr("Set Key"), menu);

    if (m_gadgetsMask & eHisto) addMenuItem(eShowHisto, tr("Histogram"), menu);

    if (m_gadgetsMask & eFilledRaster)
      addMenuItem(eFilledRaster, tr("Display Areas as Filled"), menu);

    bool ret = connect(menu, SIGNAL(triggered(QAction *)), this,
                       SLOT(onCustomizeButtonPressed(QAction *)));
    assert(ret);
  }
}

//-----------------------------------------------------------------------------

void FlipConsole::createPlayToolBar(bool withCustomWidget) {
  bool ret = true;

  m_playToolBar = new QToolBar(this);
  m_playToolBar->setMovable(false);
  m_playToolBar->setObjectName("FlipConsolePlayToolBar");
  m_playToolBar->setIconSize(QSize(17, 17));
  //	m_playToolBar->setObjectName("chackableButtonToolBar");

  // m_playToolBar->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

  createCustomizeMenu(withCustomWidget);

  if (m_gadgetsMask & eSave) {
    createButton(eSave, "save", tr("&Save Images"), false);
    // m_saveSep = m_playToolBar->addSeparator();
  }

  // snapshot
  bool separator = false;
  if (m_gadgetsMask & eSaveImg) {
    createButton(eSaveImg, "snapshot", tr("&Snapshot"), false);
    separator = true;
  }
  if (m_gadgetsMask & eCompare) {
    createButton(eCompare, "compare", tr("&Compare to Snapshot"), true);
    separator = true;
  }
  if (separator) m_compareSep = m_playToolBar->addSeparator();

  // sub camera
  separator = false;
  if (m_gadgetsMask & eDefineSubCamera) {
    createButton(eDefineSubCamera, "define_subcamera_preview",
                 tr("&Define Sub-camera"), true);
    separator = true;
  }
  if (m_gadgetsMask & eDefineLoadBox) {
    createButton(eDefineLoadBox, "define_subcamera_preview",
                 tr("&Define Loading Box"), true);
    separator = true;
  }
  if (m_gadgetsMask & eUseLoadBox) {
    createButton(eUseLoadBox, "use_subcamera_preview", tr("&Use Loading Box"),
                 true);
    separator = true;
  }
  if (separator) m_subcamSep = m_playToolBar->addSeparator();

  // preview BGs
  QActionGroup *group = new QActionGroup(m_playToolBar);
  if (m_gadgetsMask & eWhiteBg)
    createOnOffButton(eWhiteBg, "preview_white", tr("&White Background"),
                      group);
  if (m_gadgetsMask & eBlackBg)
    createOnOffButton(eBlackBg, "preview_black", tr("&Black Background"),
                      group);
  if (m_gadgetsMask & eCheckBg)
    createOnOffButton(eCheckBg, "preview_checkboard",
                      tr("&Checkered Background"), group);
  if (m_gadgetsMask & eWhiteBg) m_bgSep = m_playToolBar->addSeparator();

  // VCR buttons
  QActionGroup *playGroup = new QActionGroup(m_playToolBar);
  if (m_gadgetsMask & eFirst)
    createButton(eFirst, "framefirst", tr("&First Frame"), false);
  if (m_gadgetsMask & ePrev)
    createButton(ePrev, "frameprev", tr("&Previous Frame"), false);
  if (m_gadgetsMask & ePause)
    createCheckedButtonWithBorderImage(ePause, "pause", tr("Pause"), true,
                                       playGroup, "A_Flip_Pause");
  if (m_gadgetsMask & ePlay)
    createCheckedButtonWithBorderImage(ePlay, "play", tr("Play"), true,
                                       playGroup, "A_Flip_Play");
  if (m_gadgetsMask & eLoop)
    createCheckedButtonWithBorderImage(eLoop, "loop", tr("Loop"), true,
                                       playGroup, "A_Flip_Loop");

  if (m_gadgetsMask & eNext)
    createButton(eNext, "framenext", tr("&Next frame"), false);
  if (m_gadgetsMask & eLast)
    createButton(eLast, "framelast", tr("&Last Frame"), false);

  // separator
  if (m_gadgetsMask & ePlay) m_vcrSep = m_playToolBar->addSeparator();

  // Channel Selector
  m_colorFilterGroup = new QActionGroup(m_playToolBar);
  m_colorFilterGroup->setExclusive(false);
  if ((m_gadgetsMask & eRed) && !(m_gadgetsMask & eGRed))
    createButton(eRed, "channelred", tr("Red Channel"), true);
  else if ((m_gadgetsMask & eRed) && (m_gadgetsMask & eGRed))
    m_doubleRedAction = createDoubleButton(
        eRed, eGRed, "half_R", "half_bw", tr("Red Channel"),
        tr("Red Channel in Grayscale"), m_colorFilterGroup, m_doubleRed);

  if ((m_gadgetsMask & eGreen) && !(m_gadgetsMask & eGGreen))
    createButton(eGreen, "channelgreen", tr("Green Channel"), true);
  else if ((m_gadgetsMask & eGreen) && (m_gadgetsMask & eGGreen))
    m_doubleGreenAction = createDoubleButton(
        eGreen, eGGreen, "half_G", "half_bw", tr("Green Channel"),
        tr("Green Channel in Grayscale"), m_colorFilterGroup, m_doubleGreen);

  if ((m_gadgetsMask & eBlue) && !(m_gadgetsMask & eGBlue))
    createButton(eBlue, "channelblue", tr("Blue Channel"), true);
  else if ((m_gadgetsMask & eBlue) && (m_gadgetsMask & eGBlue))
    m_doubleBlueAction = createDoubleButton(
        eBlue, eGBlue, "half_B", "half_bw", tr("Blue Channel"),
        tr("Blue Channel in Grayscale"), m_colorFilterGroup, m_doubleBlue);

  ret = ret && connect(m_colorFilterGroup, SIGNAL(triggered(QAction *)), this,
                       SLOT(onButtonPressed(QAction *)));

  if (m_gadgetsMask & eMatte)
    createButton(eMatte, "channelmatte", tr("Alpha Channel"), true);

  // separator
  if (m_gadgetsMask & eRed || m_gadgetsMask & eGRed)
    m_colorFilterSep = m_playToolBar->addSeparator();

  // Sound & Histogram & Locator
  if (m_gadgetsMask & eSound || m_gadgetsMask & eHisto ||
      m_gadgetsMask & eLocator) {
    if (m_gadgetsMask & eSound) {
      createButton(eSound, "sound", tr("&Soundtrack "), true);
      m_soundSep = m_playToolBar->addSeparator();
    }
    if (m_gadgetsMask & eHisto)
      createButton(eHisto, "histograms", tr("&Histogram"), false);
    if (m_gadgetsMask & eLocator)
      createButton(eLocator, "locator", tr("&Locator"), false);
    if (m_gadgetsMask & eHisto || m_gadgetsMask & eLocator)
      m_histoSep = m_playToolBar->addSeparator();
  }

  if (m_gadgetsMask & eFilledRaster) {
    createOnOffButton(eFilledRaster, "preview_white",
                      tr("&Display Areas as Filled"), 0);
    m_filledRasterSep = m_playToolBar->addSeparator();
  }

  // for all actions in this toolbar
  ret = ret && connect(m_playToolBar, SIGNAL(actionTriggered(QAction *)), this,
                       SLOT(onButtonPressed(QAction *)));

  setChecked(ePause, true);
  setChecked(eWhiteBg, FlipBookWhiteBgToggle);
  setChecked(eBlackBg, FlipBookBlackBgToggle);
  setChecked(eCheckBg, FlipBookCheckBgToggle);
  assert(ret);
}

//-----------------------------------------------------------------------------

void FlipConsole::enableBlanks(bool state) {
  m_drawBlanksEnabled = state;
  m_blankColor        = TPixel::Transparent;
  if (m_drawBlanksEnabled)
    Preferences::instance()->getBlankValues(m_blanksCount, m_blankColor);
  else {
    m_blanksCount = 0;
    m_blankColor  = TPixel::Transparent;
  }
}

//-----------------------------------------------------------------------------
/*! call consoleOwner->onDrawFrame() intead of emitting drawFrame signal
*/
void FlipConsole::showCurrentFrame() {
  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
}

//-----------------------------------------------------------------------------

void FlipConsole::setChecked(UINT button, bool state) {
  int i;
  if (m_playToolBar) {
    QObjectList objectList = m_playToolBar->children();
    for (i = 0; i < (int)objectList.size(); i++) {
      QAction *action = dynamic_cast<QAction *>(objectList[i]);
      if (!action) {
        QToolButton *toolButton = dynamic_cast<QToolButton *>(objectList[i]);
        if (!toolButton) continue;
        action = toolButton->defaultAction();
      }
      if (action && action->data().toUInt() == button) {
        action->setChecked(state);
        return;
      }
    }
  }

  if (m_colorFilterGroup) {
    QList<QAction *> list = m_colorFilterGroup->actions();
    for (i = 0; i < (int)list.size(); i++)
      if (list[i]->data().toUInt() == button) {
        list[i]->setChecked(state);
        return;
      }
  }
}

//-----------------------------------------------------------------------------

bool FlipConsole::isChecked(UINT button) const {
  QList<QAction *> list;
  int i;

  if (m_playToolBar) {
    list = m_playToolBar->actions();
    for (i = 0; i < (int)list.size(); i++)
      if (list[i]->data().toUInt() == button) return list[i]->isChecked();
  }

  if (m_colorFilterGroup) {
    list = m_colorFilterGroup->actions();
    for (i = 0; i < (int)list.size(); i++)
      if (list[i]->data().toUInt() == button) return list[i]->isChecked();
  }

  return false;
}

//-----------------------------------------------------------------------------

void FlipConsole::pressLinkedConsoleButton(UINT button, FlipConsole *parent) {
  int i;
  assert(parent);

  for (i = 0; i < m_visibleConsoles.size(); i++) {
    FlipConsole *console = m_visibleConsoles.at(i);
    if (console->m_isLinkable && console != parent) {
      console->setChecked(button, parent ? parent->isChecked(button) : true);
      console->doButtonPressed(button);
    }
  }
}

//-----------------------------------------------------------------------------

void FlipConsole::onButtonPressed(int button) {
  makeCurrent();
  if (m_playbackExecutor.isRunning() &&
      (button == FlipConsole::ePlay || button == FlipConsole::eLoop)) {
    pressButton(ePause);
  } else
    doButtonPressed(button);

  if (m_areLinked) pressLinkedConsoleButton(button, this);
}

//-----------------------------------------------------------------------------
void FlipConsole::pressButton(EGadget buttonId) {
  if (m_visibleConsoles.indexOf(this) < 0 && m_visibleConsoles.size() > 0) {
    FlipConsole *console = m_visibleConsoles.at(0);
    console->makeCurrent();
    if (console->m_buttons.contains(buttonId)) {
      console->m_buttons[buttonId]->click();
    } else if (console->m_actions.contains(buttonId))
      console->m_actions[buttonId]->trigger();
  } else {
    if (m_buttons.contains(buttonId)) {
      m_buttons[buttonId]->click();
    } else if (m_actions.contains(buttonId))
      m_actions[buttonId]->trigger();
  }
}

//-----------------------------------------------------------------------------

void FlipConsole::onLoadBox(bool isDefine) {
  int shrink, dummy;

  Preferences::instance()->getViewValues(shrink, dummy);

  if (shrink != 1) {
#ifdef _WIN32
    MessageBox(0, "Cannot use loading box with a shrink factor! ", "Warning",
               MB_OK);
#endif
    setChecked(eUseLoadBox, false);
    setChecked(eDefineLoadBox, false);
    m_settings.m_useLoadbox = m_settings.m_defineLoadbox = false;
    return;
  }

  if (isDefine)
    m_settings.m_defineLoadbox = !m_settings.m_defineLoadbox;
  else
    m_settings.m_useLoadbox = !m_settings.m_useLoadbox;

  if (m_settings.m_defineLoadbox && m_settings.m_useLoadbox) {
    setChecked(isDefine ? eUseLoadBox : eDefineLoadBox, false);
    if (isDefine)
      m_settings.m_useLoadbox = false;
    else
      m_settings.m_defineLoadbox = false;
  }

  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
  return;
}

//-----------------------------------------------------------------------------

void FlipConsole::doButtonPressed(UINT button) {
  emit buttonPressed((FlipConsole::EGadget)button);

  int from = m_from, to = m_to;
  // When the level editing mode, ignore the preview frame range marker
  if (m_markerFrom <= m_markerTo && m_frameHandle &&
      m_frameHandle->isEditingScene())
    from = m_markerFrom, to = m_markerTo;

  bool linked = m_areLinked && m_isLinkable;

  switch (button) {
  case eFirst:
    m_currentFrame = from;
    break;
  case ePrev:
    m_currentFrame =
        (m_currentFrame - m_step < from) ? from : m_currentFrame - m_step;
    break;
  case eNext:
    m_currentFrame =
        (m_currentFrame + m_step > to) ? to : m_currentFrame + m_step;
    break;
  case eLast:
    m_currentFrame = to;
    break;
  case ePlay:
  case eLoop:
    // if (	  isChecked(ePlay,   false);
    // setChecked(eLoop,   false);
    m_editCurrFrame->disconnect();
    m_currFrameSlider->disconnect();

    m_isPlay = (button == ePlay);

    if (linked && m_isLinkedPlaying) return;

    if ((m_fps == 0 || m_framesCount == 0) && m_playbackExecutor.isRunning()) {
      doButtonPressed(ePause);
      if (m_fpsLabel) m_fpsLabel->setText(tr(" FPS ") + QString::number(m_fps));
      if (m_fpsField) m_fpsField->setLineEditBackgroundColor(Qt::transparent);
      return;
    }
    if (m_fpsLabel) m_fpsLabel->setText(tr(" FPS	") + "/");
    if (m_fpsField) m_fpsField->setLineEditBackgroundColor(Qt::red);

    m_playbackExecutor.resetFps(m_fps);
    if (!m_playbackExecutor.isRunning()) m_playbackExecutor.start();
    m_isLinkedPlaying = linked;

    m_reverse = (m_fps < 0);

    if (!linked) {
      // if the play button pressed at the end frame, then go back to the start
      // frame and play
      if (m_currentFrame <= from ||
          m_currentFrame >=
              to)  // the first frame of the playback is drawn right now
        m_currentFrame               = m_reverse ? to : from;
      m_settings.m_recomputeIfNeeded = true;
      m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
    }

    emit playStateChanged(true);
    return;

  case ePause:
    if (!m_playbackExecutor.isRunning() && !m_isLinkedPlaying) return;

    m_isLinkedPlaying = false;

    if (m_playbackExecutor.isRunning()) m_playbackExecutor.abort();

    m_isPlay       = false;
    m_blanksToDraw = 0;

    m_consoleOwner->swapBuffers();
    m_consoleOwner->changeSwapBehavior(true);

    if (m_settings.m_blankColor != TPixel::Transparent) {
      m_settings.m_blankColor        = TPixel::Transparent;
      m_settings.m_recomputeIfNeeded = true;
      m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
    }
    if (m_fpsLabel) m_fpsLabel->setText(tr(" FPS "));
    if (m_fpsField) m_fpsField->setLineEditBackgroundColor(Qt::transparent);
    // setChecked(ePlay,   false);
    // setChecked(eLoop,   false);
    connect(m_editCurrFrame, SIGNAL(editingFinished()), this,
            SLOT(OnSetCurrentFrame()));
    connect(m_currFrameSlider, SIGNAL(flipSliderReleased()), this,
            SLOT(OnFrameSliderRelease()));
    connect(m_currFrameSlider, SIGNAL(flipSliderPressed()), this,
            SLOT(OnFrameSliderPress()));
    connect(m_currFrameSlider, SIGNAL(valueChanged(int)), this,
            SLOT(OnSetCurrentFrame(int)));
    connect(m_currFrameSlider, SIGNAL(flipSliderReleased()), this,
            SLOT(onSliderRelease()));
    emit playStateChanged(false);
    return;

  case eGRed:
  case eGGreen:
  case eGBlue:
  case eRed:
  case eGreen:
  case eBlue:
  case eMatte: {
    if (button != eGRed) setChecked(eGRed, false);
    if (button != eGGreen) setChecked(eGGreen, false);
    if (button != eGBlue) setChecked(eGBlue, false);

    if (button == eGRed || button == eGGreen || button == eGBlue) {
      m_settings.m_greytones = isChecked(button);
      setChecked(eRed, false);
      setChecked(eGreen, false);
      setChecked(eBlue, false);
      setChecked(eMatte, false);
    } else
      m_settings.m_greytones = false;

    if (m_doubleRed) {
      m_doubleRed->update();
      m_doubleGreen->update();
      m_doubleBlue->update();
    }

    int colorMask = 0;
    if (isChecked(eRed) || isChecked(eGRed))
      colorMask = colorMask | TRop::RChan;
    if (isChecked(eGreen) || isChecked(eGGreen))
      colorMask = colorMask | TRop::GChan;
    if (isChecked(eBlue) || isChecked(eGBlue))
      colorMask                      = colorMask | TRop::BChan;
    if (isChecked(eMatte)) colorMask = colorMask | TRop::MChan;

    if (colorMask == (TRop::RChan | TRop::GChan | TRop::BChan) ||
        colorMask == (TRop::RChan | TRop::GChan | TRop::BChan | TRop::MChan))
      m_settings.m_colorMask = 0;
    else
      m_settings.m_colorMask = colorMask;
    break;
  }
  case eSound:
    // emit soundEnabled(isChecked(eSound));
    break;

  case eWhiteBg:
  case eBlackBg:
  case eCheckBg:
    m_settings.m_bg       = (EGadget)button;
    FlipBookWhiteBgToggle = isChecked(eWhiteBg);
    FlipBookBlackBgToggle = isChecked(eBlackBg);
    FlipBookCheckBgToggle = isChecked(eCheckBg);
    break;

  case FlipConsole::eCompare:
    m_settings.m_doCompare = !m_settings.m_doCompare;
    break;

  case eHisto:
  case eSaveImg:
  case eSave:
  case eLocator:
    // nothing to do
    return;

  case eDefineSubCamera:
    // nothing to do
    return;

  case eDefineLoadBox:
    onLoadBox(true);
    break;

  case eUseLoadBox:
    onLoadBox(false);
    break;

  case eFilledRaster:
    return;

  default:
    assert(false);
    break;
  }

  m_currFrameSlider->setValue(m_currentFrame);
  m_editCurrFrame->setText(QString::number(m_currentFrame));

  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
}

//--------------------------------------------------------------------

QFrame *FlipConsole::createFrameSlider() {
  QFrame *frameSliderFrame = new QFrame(this);

  m_editCurrFrame = new DVGui::IntLineEdit(frameSliderFrame, m_currentFrame);
  m_editCurrFrame->setToolTip(tr("Set the current frame"));
  m_editCurrFrame->setFixedWidth(40);

  m_currFrameSlider = new FlipSlider(frameSliderFrame);
  m_currFrameSlider->setToolTip(tr("Drag to play the animation"));

  m_currFrameSlider->setRange(0, 0);
  m_currFrameSlider->setValue(0);

  if (m_drawBlanksEnabled) {
    m_enableBlankFrameButton = new QPushButton(this);
    m_enableBlankFrameButton->setCheckable(true);
    m_enableBlankFrameButton->setChecked(true);

    m_enableBlankFrameButton->setFixedHeight(24);
    m_enableBlankFrameButton->setFixedWidth(66);
    m_enableBlankFrameButton->setObjectName("enableBlankFrameButton");
  }

  // layout
  QHBoxLayout *frameSliderLayout = new QHBoxLayout();
  frameSliderLayout->setSpacing(5);
  frameSliderLayout->setMargin(2);
  {
    frameSliderLayout->addWidget(m_editCurrFrame, 0);
    frameSliderLayout->addWidget(m_currFrameSlider, 1);
    if (m_drawBlanksEnabled)
      frameSliderLayout->addWidget(m_enableBlankFrameButton, 0);
  }
  frameSliderFrame->setLayout(frameSliderLayout);

  connect(m_editCurrFrame, SIGNAL(editingFinished()), this,
          SLOT(OnSetCurrentFrame()));
  connect(m_currFrameSlider, SIGNAL(valueChanged(int)), this,
          SLOT(OnSetCurrentFrame(int)));
  connect(m_currFrameSlider, SIGNAL(flipSliderReleased()), this,
          SLOT(OnFrameSliderRelease()));

  return frameSliderFrame;
}

//--------------------------------------------------------------------

QFrame *FlipConsole::createFpsSlider() {
  QFrame *fpsSliderFrame = new QFrame(this);
  // frame per second
  m_fpsLabel  = new QLabel(QString(" FPS -- /"), fpsSliderFrame);
  m_fpsSlider = new QScrollBar(Qt::Horizontal, fpsSliderFrame);
  m_fpsField  = new DVGui::IntLineEdit(fpsSliderFrame, m_fps, -60, 60);
  m_fpsField->setFixedWidth(40);

  m_fpsLabel->setMinimumWidth(m_fpsLabel->fontMetrics().width("_FPS_24___"));
  m_fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_fpsSlider->setObjectName("ViewerFpsSlider");
  m_fpsSlider->setRange(-60, 60);
  m_fpsSlider->setValue(m_fps);
  m_fpsSlider->setToolTip(tr("Set the playback frame rate"));
  m_fpsSlider->setContextMenuPolicy(Qt::NoContextMenu);

  QHBoxLayout *hLay = new QHBoxLayout();
  hLay->setSpacing(0);
  hLay->setMargin(0);
  {
    hLay->addWidget(m_fpsLabel, 0);
    hLay->addWidget(m_fpsField, 0);
    hLay->addWidget(m_fpsSlider, 1);
  }
  fpsSliderFrame->setLayout(hLay);

  connect(m_fpsSlider, SIGNAL(valueChanged(int)), this,
          SLOT(setCurrentFPS(int)));
  connect(m_fpsField, SIGNAL(editingFinished()), this, SLOT(onFPSEdited()));

  return fpsSliderFrame;
}

//--------------------------------------------------------------------

void FlipConsole::onFPSEdited(void) {
  // this will emit fpsSlider->ValueChanged as well
  m_fpsSlider->setValue(m_fpsField->getValue());
}

//--------------------------------------------------------------------

void FlipConsole::setFrameRange(int from, int to, int step, int current) {
  if (from != m_from || to != m_to || step != m_step) {
    m_from = from;
    m_to   = to;
    m_step = step;
    m_to -= (m_to - m_from) % m_step;
    m_framesCount = (m_to - m_from) / m_step + 1;
    m_currFrameSlider->blockSignals(true);
    // m_currFrameSlider->setRange(0, m_framesCount-1);
    m_currFrameSlider->setRange(m_from, m_to);
    m_currFrameSlider->setSingleStep(m_step);
    m_currFrameSlider->blockSignals(false);
  }

  if (m_playbackExecutor.isRunning() ||
      m_isLinkedPlaying)  // if in playing mode, the slider and the frame field
                          // are already set in the timer!
    return;

  // limit the current frame in the range from-to
  if (current < from)
    current = from;
  else if (current > to)
    current = to;

  m_currFrameSlider->blockSignals(true);
  setCurrentFrame(current);
  m_currFrameSlider->blockSignals(false);
}

//--------------------------------------------------------------------

void FlipConsole::incrementCurrentFrame(int delta) {
  m_currentFrame = m_currentFrame + delta;
  if (m_currentFrame < m_from)
    m_currentFrame += m_to - m_from + 1;
  else if (m_currentFrame > m_to)
    m_currentFrame -= m_to - m_from + 1;

  m_editCurrFrame->setText(QString::number(m_currentFrame));
  m_currFrameSlider->setValue(m_currentFrame);

  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);
}

//--------------------------------------------------------------------

void FlipConsole::OnSetCurrentFrame() {
  int newFrame = m_editCurrFrame->text().toInt();
  if (m_step > 1) {
    newFrame -= ((newFrame - m_from) % m_step);
    m_editCurrFrame->setText(QString::number(newFrame));
  }

  int i, deltaFrame = newFrame - m_currentFrame;

  if (m_framesCount == 0) m_editCurrFrame->setText(QString::number(1));

  if (m_framesCount == 0 || newFrame == m_currentFrame || newFrame == 0) return;

  if (newFrame > m_to) {
    m_editCurrFrame->setText(QString::number(m_currentFrame));
    return;
  }

  m_currentFrame = newFrame;
  m_currFrameSlider->setValue(m_currentFrame);

  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);

  if (m_areLinked)
    for (i = 0; i < m_visibleConsoles.size(); i++) {
      FlipConsole *console = m_visibleConsoles.at(i);
      if (console->m_isLinkable && console != this)
        console->incrementCurrentFrame(deltaFrame);
    }
}

//--------------------------------------------------------------------

void FlipConsole::onSliderRelease() { emit sliderReleased(); }

//--------------------------------------------------------------------
void FlipConsole::OnFrameSliderRelease() {
  m_settings.m_recomputeIfNeeded = true;
  m_currentFrame                 = -1;
  OnSetCurrentFrame();
}

void FlipConsole::OnFrameSliderPress() {
  m_settings.m_recomputeIfNeeded = false;
}

//---------------------
//----------------------------------

void FlipConsole::OnSetCurrentFrame(int index) {
  if (m_framesCount == 0) return;

  if (index == m_currentFrame) return;

  int deltaFrame = index - m_currentFrame;

  m_currentFrame = index;

  assert(m_currentFrame <= m_to);
  m_editCurrFrame->setText(QString::number(m_currentFrame));

  m_consoleOwner->onDrawFrame(m_currentFrame, m_settings);

  if (m_areLinked)
    for (int i = 0; i < m_visibleConsoles.size(); i++) {
      FlipConsole *console = m_visibleConsoles.at(i);
      if (console->m_isLinkable && console != this)
        console->incrementCurrentFrame(deltaFrame);
    }
}

//--------------------------------------------------------------------

void FlipConsole::setCurrentFrame(int frame, bool forceResetting) {
  m_currentFrame = (frame == -1) ? m_from : frame;
  if ((m_playbackExecutor.isRunning() || m_isLinkedPlaying) &&
      !forceResetting)  // if in playing mode, the slider and the frame field
                        // are already set in the timer!
    return;

  m_editCurrFrame->setValue(m_currentFrame);
  m_currFrameSlider->setValue(m_currentFrame);
}

//--------------------------------------------------------------------

void FlipConsole::enableProgressBar(bool enable) {
  m_currFrameSlider->setProgressBarEnabled(enable);
}

//--------------------------------------------------------------------

void FlipConsole::setProgressBarStatus(const std::vector<UCHAR> *pbStatus) {
  m_currFrameSlider->setProgressBarStatus(pbStatus);
}

//--------------------------------------------------------------------

const std::vector<UCHAR> *FlipConsole::getProgressBarStatus() const {
  return m_currFrameSlider->getProgressBarStatus();
}

//--------------------------------------------------------------------

void FlipConsole::onPreferenceChanged(const QString &prefName) {
  // react only when related properties are changed
  if (prefName != "BlankCount" && prefName != "BlankColor" &&
      !prefName.isEmpty())
    return;

  if (m_drawBlanksEnabled) {
    Preferences::instance()->getBlankValues(m_blanksCount, m_blankColor);
    if (m_blanksCount == 0) {
      if (m_enableBlankFrameButton->isVisible())
        m_enableBlankFrameButton->hide();
    } else {
      if (m_enableBlankFrameButton->isHidden())
        m_enableBlankFrameButton->show();
      QString buttonText = QString("+%1 Blank").arg(m_blanksCount);
      if (m_blanksCount > 1) buttonText += "s";
      m_enableBlankFrameButton->setText(buttonText);

      //--- use white text for dark color and vice versa
      QString textColor;
      QString dimmedTextColor;
      int val = (int)m_blankColor.r * 30 + (int)m_blankColor.g * 59 +
                (int)m_blankColor.b * 11;
      if (val < 12800) {
        textColor       = QString("white");
        dimmedTextColor = QString("rgb(200,200,200)");
      } else {
        textColor       = QString("black");
        dimmedTextColor = QString("rgb(55,55,55)");
      }

      int dc = 150;
      QColor lightBevel(std::min(m_blankColor.r + dc, 255),
                        std::min(m_blankColor.g + dc, 255),
                        std::min(m_blankColor.b + dc, 255));
      QColor darkBevel(std::max(m_blankColor.r - dc, 0),
                       std::max(m_blankColor.g - dc, 0),
                       std::max(m_blankColor.b - dc, 0));

      m_enableBlankFrameButton->setStyleSheet(
          QString("#enableBlankFrameButton{ \
              background-color: transparent; \
              padding: 2px;\
              font-weight: bold; \
              font-size: 12px; \
              color: %11;\
              border-style: inset; \
              border-left-color: rgb(%5,%6,%7); \
              border-top-color: rgb(%5,%6,%7); \
              border-right-color: rgb(%8,%9,%10); \
              border-bottom-color: rgb(%8,%9,%10); \
              border-width: 2px; \
              border-radius: 3px; \
            } \
            #enableBlankFrameButton:checked { \
              background-color: rgb(%1,%2,%3); \
              color: %4; \
              border-style: outset; \
              border-left-color: rgb(%8,%9,%10); \
              border-top-color: rgb(%8,%9,%10); \
              border-right-color: rgb(%5,%6,%7); \
              border-bottom-color: rgb(%5,%6,%7); \
            } ")
              .arg(m_blankColor.r)
              .arg(m_blankColor.g)
              .arg(m_blankColor.b)
              .arg(textColor)
              .arg(lightBevel.red())
              .arg(lightBevel.green())
              .arg(lightBevel.blue())
              .arg(darkBevel.red())
              .arg(darkBevel.green())
              .arg(darkBevel.blue())
              .arg(dimmedTextColor));
      m_enableBlankFrameButton->update();
    }
  }
}

//====================================================================

class FlipConsoleActionsCreator : AuxActionsCreator {
  void createToggleAction(QObject *parent, const char *cmdId, const char *name,
                          int buttonId) {
    QAction *action = new QAction(name, parent);
    action->setData(QVariant(buttonId));
    CommandManager::instance()->define(cmdId, MiscCommandType, "", action);
  }

public:
  void createActions(QObject *parent) override {
    /*createToggleAction(parent, "A_Flip_Play",  "Play",  FlipConsole::ePlay);
createToggleAction(parent, "A_Flip_Pause", "Pause", FlipConsole::ePause);
createToggleAction(parent, "A_Flip_Loop",  "Loop",  FlipConsole::eLoop);*/
  }
} flipConsoleActionsCreator;

//--------------------------------------------------------------------
