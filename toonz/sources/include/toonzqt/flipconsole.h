#pragma once

#ifndef FLICONSOLE_H
#define FLICONSOLE_H

#include <QWidget>
#include <QAction>
#include <QMap>
#include <QList>
#include <QTime>
#include <QStyleOption>
#include <QStyleOptionFrameV3>
#include <QColor>
#include <QImage>

#include "tcommon.h"
#include "tpixel.h"
#include "toonzqt/intfield.h"
#include "toonz/imagepainter.h"
#include "tstopwatch.h"
#include <QThread>
#include <QElapsedTimer>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QToolBar;
class QLabel;
class QSlider;
class QTimerEvent;
class QVBoxLayout;
class QActionGroup;
class QAbstractButton;
class QPushButton;
class QScrollBar;
class DoubleButton;
class FlipSlider;
class FlipConsole;
class ToolBarContainer;
class FlipConsoleOwner;
class TFrameHandle;
//-----------------------------------------------------------------------------

class PlaybackExecutor final : public QThread {
  Q_OBJECT

  int m_fps;
  bool m_abort;
  QElapsedTimer m_timer;

public:
  PlaybackExecutor();

  void resetFps(int fps);

  void run() override;
  void abort() { m_abort = true; }
  bool isAborted() { return m_abort; }
  void emitNextFrame(int fps) { emit nextFrame(fps, nullptr, 0); }

signals:
  void nextFrame(
      int fps, QElapsedTimer *timer,
      qint64 targetInstant);  // Must be connect with
                              // Qt::BlockingQueuedConnection connection type.
};

//-----------------------------------------------------------------------------

// Implements a flipbook slider with a progress bar in background.
class FlipSlider final : public QAbstractSlider {
  Q_OBJECT

  Q_PROPERTY(int PBHeight READ getPBHeight WRITE setPBHeight)

  Q_PROPERTY(QImage PBOverlay READ getPBOverlay WRITE setPBOverlay)
  Q_PROPERTY(QImage PBMarker READ getPBMarker WRITE setPBMarker)

  Q_PROPERTY(int PBColorMarginLeft READ getPBColorMarginLeft WRITE
                 setPBColorMarginLeft)
  Q_PROPERTY(
      int PBColorMarginTop READ getPBColorMarginTop WRITE setPBColorMarginTop)
  Q_PROPERTY(int PBColorMarginRight READ getPBColorMarginRight WRITE
                 setPBColorMarginRight)
  Q_PROPERTY(int PBColorMarginBottom READ getPBColorMarginBottom WRITE
                 setPBColorMarginBottom)

  Q_PROPERTY(int PBMarkerMarginLeft READ getPBMarkerMarginLeft WRITE
                 setPBMarkerMarginLeft)
  Q_PROPERTY(int PBMarkerMarginRight READ getPBMarkerMarginRight WRITE
                 setPBMarkerMarginRight)

  Q_PROPERTY(QColor baseColor READ getBaseColor WRITE setBaseColor)
  Q_PROPERTY(
      QColor notStartedColor READ getNotStartedColor WRITE setNotStartedColor)
  Q_PROPERTY(QColor startedColor READ getStartedColor WRITE setStartedColor)
  Q_PROPERTY(QColor finishedColor READ getFinishedColor WRITE setFinishedColor)

  bool m_enabled;
  const std::vector<UCHAR> *m_progressBarStatus;

public:
  enum { PBFrameNotStarted, PBFrameStarted, PBFrameFinished };

  FlipSlider(QWidget *parent);
  ~FlipSlider() {}

  void setProgressBarEnabled(bool enabled) { m_enabled = enabled; }

  void setProgressBarStatus(const std::vector<UCHAR> *pbStatus) {
    m_progressBarStatus = pbStatus;
  }

  const std::vector<UCHAR> *getProgressBarStatus() const {
    return m_progressBarStatus;
  }

public:
  // Properties setters-getters

  int getPBHeight() const;
  void setPBHeight(int height);

  QImage getPBOverlay() const;
  void setPBOverlay(const QImage &img);

  QImage getPBMarker() const;
  void setPBMarker(const QImage &img);

  int getPBColorMarginLeft() const;
  void setPBColorMarginLeft(int margin);

  int getPBColorMarginTop() const;
  void setPBColorMarginTop(int margin);

  int getPBColorMarginRight() const;
  void setPBColorMarginRight(int margin);

  int getPBColorMarginBottom() const;
  void setPBColorMarginBottom(int margin);

  int getPBMarkerMarginLeft() const;
  void setPBMarkerMarginLeft(int margin);

  int getPBMarkerMarginRight() const;
  void setPBMarkerMarginRight(int margin);

  QColor getBaseColor() const;
  void setBaseColor(const QColor &color);

  QColor getNotStartedColor() const;
  void setNotStartedColor(const QColor &color);

  QColor getStartedColor() const;
  void setStartedColor(const QColor &color);

  QColor getFinishedColor() const;
  void setFinishedColor(const QColor &color);

protected:
  void paintEvent(QPaintEvent *ev) override;

  void mousePressEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;

private:
  int sliderPositionFromValue(int min, int max, int pos, int span);
  int sliderValueFromPosition(int min, int max, int step, int pos, int span);
  int pageStepVal(int val);

signals:
  void flipSliderReleased();
  void flipSliderPressed();
};

//-----------------------------------------------------------------------------

class DVAPI FlipConsole final : public QWidget {
  Q_OBJECT

  QColor m_fpsFieldColor;
  Q_PROPERTY(QColor FpsFieldColor READ getFpsFieldColor WRITE setFpsFieldColor)

public:
  enum EGadget {
    eBegin,
    ePlay,
    eLoop,
    ePause,
    ePrev,
    eNext,
    eFirst,
    eLast,
    eRed,
    eGreen,
    eBlue,
    eGRed,
    eGGreen,
    eGBlue,
    eMatte,
    eFrames,
    eRate,
    eSound,
    eHisto,
    eSaveImg,
    eCompare,
    eCustomize,
    eSave,
    eDefineSubCamera,
    eFilledRaster,  // Used only in LineTest
    eDefineLoadBox,
    eUseLoadBox,
    eLocator,
    eZoomIn,
    eZoomOut,
    eFlipHorizontal,
    eFlipVertical,
    eResetView,
    eBlankFrames,
    eDecreaseGain,
    eResetGain,
    eIncreaseGain,
    // following values are hard-coded in ImagePainter
    eBlackBg = 0x40000,
    eWhiteBg = 0x80000,
    eCheckBg = 0x100000,
    eEnd
  };

  static FlipConsole *m_currentConsole;
  static QList<FlipConsole *> m_visibleConsoles;
  static bool m_isLinkedPlaying;
  static bool m_areLinked;

  // blanksEnabled==true->at begin of each loop a number of blank frames are
  // drawn (according to rpeferences settings)
  FlipConsole(QVBoxLayout *layout, std::vector<int> gadgetsMask,
              bool isLinkable, QWidget *customWidget,
              const QString &customizeId,
              FlipConsoleOwner *consoleOwner,  // call
                                               // consoleOwner->onDrawFrame()
                                               // instead of emitting drawFrame
                                               // signal
              bool enableBlanks = false);
  void enableBlanks(bool state);

  void setFrameRange(
      int from, int to, int step,
      int current = -1);  // if current==-1, current position will be ==from
  void getFrameRange(int &from, int &to, int &step) const {
    from = m_from, to = m_to, step = m_step;
  }
  void setFrameRate(int rate, bool forceUpdate = true);
  // if doShowHide==true, applies set visible, otherwise applies setEnabled
  void enableButton(UINT button, bool enable, bool doShowHide = true);
  void showCurrentFrame();
  int getCurrentFrame() const { return m_currentFrame; }
  int getCurrentFps() const { return m_fps; }
  void setChecked(UINT button, bool state);
  bool isChecked(UINT button) const;
  void setCurrentFrame(int frame, bool forceResetting = false);
  void setMarkers(int fromIndex, int toIndex) {
    m_markerFrom = fromIndex + 1;
    m_markerTo   = toIndex + 1;
  }
  void pressButton(UINT button) {
    doButtonPressed(button);
    setChecked(button, !isChecked(button));
  }
  void setStopAt(int frame);
  void setStartAt(int frame);

  // the main (currently the only) use for current flipconsole and setActive is
  // to
  // support shortcuts handling
  // setActive() should be called every time the visibility state of the console
  // changes
  // a list of visible console is maintained. calling setActive(false) for the
  // current
  // console makes automatically current the next one in the list
  static FlipConsole *getCurrent() { return m_currentConsole; };

  static void toggleLinked();

  void makeCurrent();
  void setActive(bool active);

  void pressButton(EGadget buttonId);

  void showHideAllParts(bool);
  void showHidePlaybar(bool);
  void showHideFrameSlider(bool);

  void enableProgressBar(bool enable);
  void setProgressBarStatus(const std::vector<UCHAR> *status);
  const std::vector<UCHAR> *getProgressBarStatus() const;

  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
  }

  bool isLinkable() const { return m_isLinkable; }
  void playNextFrame(QElapsedTimer *timer = nullptr, qint64 targetInstant = 0);
  void updateCurrentFPS(int val);

  bool hasButton(std::vector<int> buttonMask, FlipConsole::EGadget buttonId) {
    if (buttonMask.size() == 0) return true;
    return std::find(buttonMask.begin(), buttonMask.end(), buttonId) ==
           buttonMask.end();
  }

  void setFpsFieldColor(const QColor &color) { m_fpsFieldColor = color; }
  QColor getFpsFieldColor() const { return m_fpsFieldColor; }

  void resetGain(bool forceInit = false);
signals:

  void buttonPressed(FlipConsole::EGadget button);

  void playStateChanged(bool isPlaying);
  void sliderReleased();

private:
  UINT m_customizeMask;
  QString m_customizeId;
  QAction *m_customAction;
  PlaybackExecutor m_playbackExecutor;

  QAction *m_customSep, *m_rateSep, *m_histoSep, *m_bgSep, *m_vcrSep,
      *m_compareSep, *m_saveSep, *m_colorFilterSep, *m_soundSep, *m_subcamSep,
      *m_filledRasterSep, *m_viewerSep, *m_gainSep;

  QToolBar *m_playToolBar;
  QActionGroup *m_colorFilterGroup;
  ToolBarContainer *m_playToolBarContainer;
  QFrame *m_frameSliderFrame;

  QLabel *m_fpsLabel;
  QScrollBar *m_fpsSlider;
  DVGui::IntLineEdit *m_fpsField;
  QAction *m_fpsFieldAction;
  QAction *m_fpsLabelAction;
  QAction *m_fpsSliderAction;
  QFrame *createFpsSlider();
  QAction *m_doubleRedAction, *m_doubleGreenAction, *m_doubleBlueAction;
  DoubleButton *m_doubleRed, *m_doubleGreen, *m_doubleBlue;

  std::vector<int> m_gadgetsMask;
  int m_from, m_to, m_step;
  int m_currentFrame, m_framesCount;
  int m_stopAt = -1;
  int m_startAt =
      -1;  // used in the "play selection" mode of the viewer preview
  ImagePainter::VisualSettings m_settings;

  bool m_isPlay;
  int m_fps, m_sceneFps;
  bool m_reverse;
  int m_markerFrom, m_markerTo;
  bool m_drawBlanksEnabled;
  int m_blanksCount;
  TPixel m_blankColor;
  int m_blanksToDraw;
  bool m_isLinkable;

  QToolButton *m_resetGainBtn;
  int m_prevGainStep;

  QMap<EGadget, QAbstractButton *> m_buttons;
  QMap<EGadget, QAction *> m_actions;

  void createCustomizeMenu(bool withCustomWidget);
  void addMenuItem(UINT id, const QString &text, QMenu *menu);
  void createButton(UINT buttonMask, const char *iconStr, const QString &tip,
                    bool checkable, QActionGroup *groupIt = 0);
  QAction *createCheckedButtonWithBorderImage(
      UINT buttonMask, const char *iconStr, const QString &tip, bool checkable,
      QActionGroup *groupIt = 0, const char *cmdId = 0);
  void createOnOffButton(UINT buttonMask, const char *iconStr,
                         const QString &tip, QActionGroup *group);
  QAction *createDoubleButton(UINT buttonMask1, UINT buttonMask2,
                              const char *iconStr1, const char *iconStr2,
                              const QString &tip1, const QString &tip2,
                              QActionGroup *group, DoubleButton *&w);

  QFrame *createFrameSlider();
  void createPlayToolBar(QWidget *customWidget);
  DVGui::IntLineEdit *m_editCurrFrame;
  FlipSlider *m_currFrameSlider;

  void doButtonPressed(UINT button);
  static void pressLinkedConsoleButton(UINT button, FlipConsole *skipIt);
  void applyCustomizeMask();
  void onLoadBox(bool isDefine);

  QPushButton *m_enableBlankFrameButton;

  FlipConsoleOwner *m_consoleOwner;
  TFrameHandle *m_frameHandle;

  void adjustGain(bool increase);

protected slots:

  void OnSetCurrentFrame();
  void OnFrameSliderRelease();
  void OnFrameSliderPress();
  void OnSetCurrentFrame(int);
  void setCurrentFPS(int);
  void setCurrentFPS(bool dragging);
  inline void onButtonPressed(QAction *action) {
    onButtonPressed(action->data().toUInt());
  }
  void onButtonPressed(int button);
  void incrementCurrentFrame(int delta);
  void onNextFrame(int fps, QElapsedTimer *timer, qint64 target);
  void onCustomizeButtonPressed(QAction *);
  bool drawBlanks(int from, int to, QElapsedTimer *timer, qint64 target);
  void onSliderRelease();

  void onFPSEdited();

public slots:
  void onPreferenceChanged(const QString &);

private:
  friend class PlaybackExecutor;
  PlaybackExecutor &playbackExecutor() { return m_playbackExecutor; }
};

#endif
