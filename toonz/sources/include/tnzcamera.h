#pragma once

#ifndef TNZCAMERA_H
#define TNZCAMERA_H

#include "tcommon.h"
#include "timage.h"
#include "tiio.h"
#include "tthread.h"
#include "toonz/toonzscene.h"

//#define WRITE_LOG_FILE
//#define CHECK_VIDEO_FRAME_INTERVALL
//#define USE_OPENGL_SHARED

#undef DVAPI
#undef DVVAR
#ifdef TCAMERA_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class CameraLevelManager;
class CameraConnect;
class QImage;
class CaptureParameters;
class TXshSimpleLevel;

//===================================================================
// CameraImageViewer

class DVAPI CameraImageViewer {
public:
  CameraImageViewer() {}

  virtual void setImage(TRasterP ras) = 0;
};

// void processImage(unsigned char* m_ptr, int camWidth, int camHeight);

#ifndef USE_OPENGL_SHARED
//=============================================================================
// ImageProcessing

class ImageProcessing : public TThread::Runnable {
  Q_OBJECT
  unsigned char *m_ptr;
  int m_camWidth;
  int m_camHeight;

  enum AcquireType { ACQUIRE_FRAME, ACQUIRE_WHITE_IMAGE, NONE } m_acquireType;

public:
  ImageProcessing();
  ~ImageProcessing();

  void setData(unsigned char *dataPtr);
  void setImageSize(int camWidth, int camHeight) {
    m_camWidth  = camWidth;
    m_camHeight = camHeight;
  }

  void run();

  void onFinished(TThread::RunnableP task);
};
#endif

//===================================================================
// CameraTest

class DVAPI TnzCamera : public QObject {
  Q_OBJECT

  CameraImageViewer *m_viewer;
  CameraLevelManager *m_cameraLevelManager;
  CameraConnect *m_cameraConnection;

  QList<QString> m_devices;

  bool m_isCameraConnected;
  bool m_acquireFrame;
  bool m_acquireWhiteImage;

  ToonzScene *m_scene;

  TDimension m_deviceResolution;

  TPixel32 m_linesColor;

  bool m_startedImageProcessing;
#ifndef USE_OPENGL_SHARED
  TThread::Executor *m_imageProcessingExecutor;
#else
#endif

  bool m_freeze;

#ifdef CHECK_VIDEO_FRAME_INTERVALL
  double m_videoFrameIntervall;
#endif

  TnzCamera();

public:
  static TnzCamera *instance();
  bool initTnzCamera();
  ~TnzCamera();

#ifdef USE_OPENGL_SHARED
  void notifyImageChanged(int camWidth, int camHeight, unsigned char *ptr) {
    Q_EMIT imageChanged(camWidth, camHeight, ptr);
  }
#else
  TThread::Executor *getImageProcessingExecutor() const {
    return m_imageProcessingExecutor;
  }
#endif
  bool &startedImageProcessing() { return m_startedImageProcessing; }

  bool isFreezed() const { return m_freeze; }
  void freeze(bool freeze) { m_freeze = freeze; }

  void setScene(ToonzScene *scene);
  TDimension getDeviceResolution() const { return m_deviceResolution; }
  CaptureParameters *getCaptureParameters() const;

  TXshSimpleLevel *getCurrentLevel() const;
  TFrameId getCurrentFid() const;

#ifdef CHECK_VIDEO_FRAME_INTERVALL
  double getVideoFrameIntervall() const { return m_videoFrameIntervall; }
  void setVideoFrameIntervall(double intervall);
#endif

  bool isAcquireFrame() const { return m_acquireFrame; }
  bool isAcquireWhiteImage() const { return m_acquireWhiteImage; }
  void setViewImage(TRaster32P img);
  void setAcquiredImage(TRaster32P img);
  void setAcquiredWhiteImage(TRasterGR8P img);

  TRasterGR8P getWhiteImage();

  bool isCameraConnected() const { return m_isCameraConnected; }

  void updateDeviceResolution(int width, int height);
  bool setCurrentImageSize(int width, int height);
  int getImageWidth() const;
  int getImageHeight() const;

  int getContrast() const;
  int getBrightness() const;

  bool isSubtractWhiteImage() const;
  void removeWhiteImage();

  bool getUpsideDown() const;

  bool saveWithoutAlpha() const;

  TPixel32 getLinesColor() const { return m_linesColor; }
  void setLinesColor(TPixel32 color) { m_linesColor = color; }

  bool findConnectedCameras(QList<QString> &cameras);
  bool cameraConnect(wstring deviceName);
  void cameraDisconnect();
  bool onViewfinder(CameraImageViewer *viewer);
  bool onRelease(TFrameId frameId, wstring fileName, int row = 0, int col = 0,
                 bool keepWhiteImage = false);
  int saveType() const;
  void setSaveType(int value);

  // Utility function
  void keepWhiteImage();

  void notifyCameraShutDown() { Q_EMIT cameraShutDown(); }
  void notifyAbort() { Q_EMIT abort(); }

#ifdef USE_OPENGL_SHARED
protected Q_SLOTS:
  void processImage(int camWidth, int camHeight, unsigned char *ptr);
#endif

Q_SIGNALS:
  void cameraShutDown();
  void abort();
  void devicePropChanged();
  void captureFinished();
  void imageViewChanged(TRaster *ras);
  void imageSizeUpdated();
#ifdef USE_OPENGL_SHARED
  void imageChanged(int camWidth, int camHeight, unsigned char *ptr);
#endif
};

#ifdef WRITE_LOG_FILE

//===================================================================
// LogWriter

#include "tfilepath_io.h"

class LogWriter  // Singleton
{
  Tofstream m_os;

public:
  LogWriter(TFilePath fp);

  void write(QString str);

public:
  static LogWriter *instance();
  ~LogWriter();
};

#endif

#endif  // TNZCAMERA_H
