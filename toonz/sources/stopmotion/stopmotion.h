#pragma once

#ifndef STOPMOTION_H
#define STOPMOTION_H

#ifdef WITH_CANON
// Canon Includes
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"
#endif

#if defined(x64)
#include "turbojpeg.h"
#endif

// Toonz Includes
#include "traster.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"

#include "webcam.h"
#include "jpgconverter.h"
#include "canon.h"
#include "stopmotionserial.h"
#include "stopmotionlight.h"

#include "toonz/txshsimplelevel.h"

#include <QObject>
#include <QThread>
#include <QSound>

class QCamera;
class QCameraInfo;
class QTimer;

enum ASPECT_RATIO { FOUR_THREE = 0, THREE_TWO, SIXTEEN_NINE, OTHER_RATIO };

class StopMotion : public QObject {  // Singleton
  Q_OBJECT

public:
  static StopMotion* instance() {
    static StopMotion _instance;
    return &_instance;
  };

private:
  StopMotion();
  ~StopMotion();

  // file stuff
  int m_frameNumber          = 1;
  int m_xSheetFrameNumber    = 1;
  QString m_levelName        = "";
  QString m_fileType         = "jpg";
  QString m_filePath         = "+extras";
  QString m_frameInfoText    = "";
  QString m_infoColorName    = "";
  QString m_frameInfoToolTip = "";

  // options
  int m_opacity     = 255.0;
  int m_subsampling = 1;
  QSize m_allowedCameraSize;
  bool m_useNumpadShortcuts      = false;
  bool m_numpadForStyleSwitching = true;
  bool m_turnOnRewind            = false;

  std::map<std::string, QAction*> m_oldActionMap;
  std::map<int, TRaster32P> m_liveViewImageMap;

  bool m_playCaptureSound = false;
  QSound* m_camSnapSound  = 0;

public:
  enum LiveViewStatus {
    LiveViewClosed = 0,
    LiveViewStarting,
    LiveViewOpen,
    LiveViewPaused
  };

  Webcam* m_webcam;
  Canon* m_canon;
  StopMotionSerial* m_serial;
  StopMotionLight* m_light;

  bool m_usingWebcam       = false;
  bool m_placeOnXSheet     = true;
  bool m_alwaysLiveView    = false;
  bool m_userCalledPause   = false;
  bool m_drawBeneathLevels = true;
  bool m_isTimeLapse       = false;
  int m_reviewTimeDSec     = 20;
  QString m_tempFile;
  TXshSimpleLevel* m_sl;

  // timers
  QTimer* m_timer;
  int m_intervalDSec     = 100;
  bool m_intervalStarted = false;
  QTimer* m_reviewTimer;
  QTimer *m_intervalTimer, *m_countdownTimer, *m_webcamOverlayTimer;

  // live view and images
  int m_liveViewStatus = LiveViewClosed;
  bool m_hasLiveViewImage, m_hasLineUpImage, m_showLineUpImage;
  bool m_alwaysUseLiveViewImages = false;
  TRaster32P m_liveViewImage, m_newImage, m_lineUpImage;
  TDimension m_liveViewImageDimensions = TDimension(0, 0);
  TDimension m_fullImageDimensions     = TDimension(0, 0);
  TPointD m_liveViewDpi                = TPointD(0.0, 0.0);
  TPointD m_fullImageDpi               = TPointD(0.0, 0.0);

  // files and frames
  void setXSheetFrameNumber(int frameNumber);
  int getXSheetFrameNumber() { return m_xSheetFrameNumber; }
  void setFrameNumber(int frameNumber);
  int getFrameNumber() { return m_frameNumber; }
  void setLevelName(QString levelName);
  QString getLevelName() { return m_levelName; }
  void setFileType(QString fileType);
  QString getFileType() { return m_fileType; }
  void setFilePath(QString filePath);
  QString getFilePath() { return m_filePath; }
  void updateLevelNameAndFrame(std::wstring levelName);
  void setToNextNewLevel();
  void nextFrame();
  void previousFrame();
  void lastFrame();
  void nextName();
  void previousName();
  void refreshFrameInfo();
  QString getFrameInfoText() { return m_frameInfoText; }
  QString getInfoColorName() { return m_infoColorName; }
  QString getFrameInfoToolTip() { return m_frameInfoToolTip; }

  // cameras
  void setWebcamResolution(QString resolution);
  std::string getTEnvCameraName();
  void setTEnvCameraName(std::string name);
  std::string getTEnvCameraResolution();
  void setTEnvCameraResolution(std::string resolution);
  void disconnectAllCameras();
  void changeCameras(int index);
  void refreshCameraList();

  // commands
  void jumpToCameraFrame();
  void removeStopMotionFrame();

  // live view and images
  bool toggleLiveView();
  void pauseLiveView();
  bool loadLineUpImage();
  bool loadLiveViewImage(int row, TRaster32P& image);
  void setLiveViewImage();
  void captureImage();
  void captureWebcamImage();
  void captureDslrImage();
  void postImportProcess();
  void toggleAlwaysUseLiveViewImages();
  bool buildLiveViewMap(TXshSimpleLevel* sl);

  // time lapse
  void toggleInterval(bool on);
  void startInterval();
  void stopInterval();
  void setIntervalDSec(int value);
  int getIntervalDSec() { return m_intervalDSec; };
  void restartInterval();

  // options
  void raiseOpacity();
  void lowerOpacity();
  void setOpacity(int opacity);
  int getOpacity() { return m_opacity; }
  void setAlwaysLiveView(bool on);
  bool getAlwaysLiveView() { return m_alwaysLiveView; }
  void setPlaceOnXSheet(bool on);
  bool getPlaceOnXSheet() { return m_placeOnXSheet; }
  void setUseNumpadShortcuts(bool on);
  bool getUseNumpadShortcuts() { return m_useNumpadShortcuts; }
  void toggleNumpadShortcuts(bool on);
  void toggleNumpadForFocusCheck(bool on);
  void setDrawBeneathLevels(bool on);
  void setReviewTimeDSec(int timeDSec);
  int getReviewTimeDSec() { return m_reviewTimeDSec; }
  void getSubsampling();
  void setSubsampling();
  int getSubsamplingValue() { return m_subsampling; }
  void setSubsamplingValue(int subsampling);
  void setPlayCaptureSound(bool on);
  bool getPlayCaptureSound() { return m_playCaptureSound; }

  // saving and loading
  void saveXmlFile();
  bool loadXmlFile();
  bool exportImageSequence();

public slots:
  // timers
  void onTimeout();
  void onReviewTimeout();
  void onIntervalCaptureTimerTimeout();
  void captureWebcamOnTimeout();
  void update();
  bool importImage();
  void onSceneSwitched();
  void onPlaybackChanged();
  void onCanonCameraChanged(QString);

signals:
  // camera stuff
  void cameraChanged(QString);
  void newCameraSelected(int, bool);
  void webcamResolutionsChanged();
  void newWebcamResolutionSelected(int);
  void updateCameraList(QString);

  // live view and images
  void newLiveViewImageReady();
  void liveViewStopped();
  void newImageReady();
  void liveViewChanged(bool);
  void liveViewOnAllFramesSignal(bool);
  void newDimensions();
  void alwaysUseLiveViewImagesToggled(bool);

  // file stuff
  void filePathChanged(QString);
  void levelNameChanged(QString);
  void fileTypeChanged(QString);
  void frameNumberChanged(int);
  void frameInfoTextChanged(QString);
  void xSheetFrameNumberChanged(int);

  // options
  void optionsChanged();
  void subsamplingChanged(int);
  void opacityChanged(int);
  void placeOnXSheetSignal(bool);
  void useNumpadSignal(bool);
  void drawBeneathLevelsSignal(bool);
  void reviewTimeChangedSignal(int);
  void playCaptureSignal(bool);

  // time lapse
  void intervalToggled(bool);
  void intervalStarted();
  void intervalStopped();
  void intervalAmountChanged(int);
};

#endif  // STOPMOTION_H