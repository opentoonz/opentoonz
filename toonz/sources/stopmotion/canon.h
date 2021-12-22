#pragma once

#ifndef CANON_H
#define CANON_H

#ifdef WITH_CANON
// Canon Includes
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"
#endif

#include "turbojpeg.h"

// Toonz Includes
#include "traster.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"

#include "jpgconverter.h"

#include <QObject>

class QCamera;
class QCameraInfo;
class QDialog;
class QTimer;
class QSerialPort;

#include <QThread>

class Canon : public QObject {
  Q_OBJECT

public:
  static Canon* instance() {
    static Canon _instance;
    return &_instance;
  };

private:
#ifdef WITH_CANON
  std::map<EdsUInt32, std::string> m_avMap, m_tvMap, m_isoMap, m_modeMap,
      m_exposureMap, m_whiteBalanceMap, m_imageQualityMap, m_pictureStyleMap;
  JpgConverter* m_converter;
  static EdsError EDSCALLBACK handleObjectEvent(EdsObjectEvent event,
                                                EdsBaseRef object,
                                                EdsVoid* context);

  static EdsError EDSCALLBACK handlePropertyEvent(EdsPropertyEvent event,
                                                  EdsPropertyID property,
                                                  EdsUInt32 param,
                                                  EdsVoid* context);

  static EdsError EDSCALLBACK handleStateEvent(EdsStateEvent event,
                                               EdsUInt32 parameter,
                                               EdsVoid* context);
  static EdsError EDSCALLBACK handleCameraAddedEvent(EdsVoid* context);

  void buildAvMap();
  void buildTvMap();
  void buildIsoMap();
  void buildModeMap();
  void buildExposureMap();
  void buildWhiteBalanceMap();
  void buildImageQualityMap();
  void buildPictureStyleMap();
#endif

private:
  QStringList m_isoOptions, m_shutterSpeedOptions, m_apertureOptions,
      m_exposureOptions, m_whiteBalanceOptions, m_colorTempOptions,
      m_imageQualityOptions, m_pictureStyleOptions;

public:
  Canon();
  ~Canon();

#ifdef WITH_CANON
  EdsError m_error              = EDS_ERR_OK;
  EdsUInt32 m_count             = 0;
  EdsCameraListRef m_cameraList = NULL;
  EdsCameraRef m_camera         = NULL;
  EdsUInt32 m_liveViewZoom      = 1;
  bool m_isSDKLoaded            = false;
  bool m_sessionOpen            = false;
  bool m_zooming                = false;
  std::string m_cameraName;
  TDimension m_proxyImageDimensions = TDimension(0, 0);
  TPointD m_proxyDpi                = TPointD(0.0, 0.0);
  TPoint m_liveViewZoomOffset       = TPoint(0, 0);
  bool m_liveViewZoomReadyToPick    = true;
  TPointD m_liveViewZoomPickPoint   = TPointD(0.0, 0.0);
  TPoint m_zoomRectDimensions       = TPoint(0, 0);
  TPoint m_calculatedZoomPoint      = TPoint(0, 0);
  TPoint m_finalZoomPoint           = TPoint(0, 0);
  TRect m_zoomRect                  = TRect(0, 0, 0, 0);
#endif
  bool m_useScaledImages           = true;
  bool m_converterSucceeded        = false;
  bool m_pickLiveViewZoom          = false;
  TDimension m_fullImageDimensions = TDimension(0, 0);
  TPointD m_fullImageDpi           = TPointD(0.0, 0.0);

// Canon Commands
#ifdef WITH_CANON
  void cameraAdded();
  void closeAll();
  void resetCanon(bool liveViewOpen);
  void closeCanonSDK();
  int getCameraCount();
  std::string getCameraName();
  EdsError initializeCanonSDK();
  EdsCameraListRef getCameraList();
  EdsError releaseCameraList();
  EdsError getCamera(int index);
  EdsError releaseCamera();
  EdsError openCameraSession();
  EdsError closeCameraSession();
  bool downloadImage(EdsBaseRef object);
  EdsError takePicture();
  EdsError startCanonLiveView();
  EdsError endCanonLiveView();
  bool downloadEVFData();
  QStringList getIsoOptions() { return m_isoOptions; }
  QStringList getShutterSpeedOptions() { return m_shutterSpeedOptions; }
  QStringList getApertureOptions() { return m_apertureOptions; }
  QStringList getExposureOptions() { return m_exposureOptions; }
  QStringList getWhiteBalanceOptions() { return m_whiteBalanceOptions; }
  QStringList getColorTemperatureOptions() { return m_colorTempOptions; }
  QStringList getImageQualityOptions() { return m_imageQualityOptions; }
  QStringList getPictureStyleOptions() { return m_pictureStyleOptions; }
  EdsError getAvailableShutterSpeeds();
  EdsError getAvailableIso();
  EdsError getAvailableApertures();
  EdsError getAvailableExposureCompensations();
  EdsError getAvailableWhiteBalances();
  EdsError getAvailableImageQualities();
  EdsError getAvailablePictureStyles();
  void buildColorTemperatures();
  QString getCurrentShutterSpeed();
  QString getCurrentIso();
  QString getCurrentAperture();
  QString getCurrentExposureCompensation();
  QString getCurrentWhiteBalance();
  QString getCurrentColorTemperature();
  QString getCurrentImageQuality();
  QString getCurrentPictureStyle();
  QString getCurrentBatteryLevel();
  EdsError setShutterSpeed(QString shutterSpeed);
  EdsError setIso(QString iso);
  EdsError setAperture(QString aperture);
  EdsError setExposureCompensation(QString exposure);
  EdsError setWhiteBalance(QString whiteBalance);
  EdsError setColorTemperature(QString temp);
  EdsError setImageQuality(QString quality);
  EdsError setPictureStyle(QString style);
  QString getMode();
  void refreshOptions();
  EdsError zoomLiveView();
  EdsError setZoomPoint();
  void makeZoomPoint(TPointD pos);
  void toggleZoomPicking();
  void calculateZoomPoint();
  EdsError focusNear();
  EdsError focusFar();
  EdsError focusNear2();
  EdsError focusFar2();
  EdsError focusNear3();
  EdsError focusFar3();
  void extendCameraOnTime();
  void setUseScaledImages(bool on);
  bool getUseScaledImages() { return m_useScaledImages; }
#endif

public slots:
  void onImageReady(const bool&);
  void onFinished();

signals:
  // canon signals
  void apertureOptionsChanged();
  void isoOptionsChanged();
  void shutterSpeedOptionsChanged();
  void exposureOptionsChanged();
  void whiteBalanceOptionsChanged();
  void colorTemperatureChanged();
  void imageQualityOptionsChanged();
  void pictureStyleOptionsChanged();
  void apertureChangedSignal(QString);
  void isoChangedSignal(QString);
  void shutterSpeedChangedSignal(QString);
  void exposureChangedSignal(QString);
  void whiteBalanceChangedSignal(QString);
  void colorTemperatureChangedSignal(QString);
  void imageQualityChangedSignal(QString);
  void pictureStyleChangedSignal(QString);
  void modeChanged();
  void focusCheckToggled(bool);
  void pickFocusCheckToggled(bool);
  void scaleFullSizeImagesSignal(bool);
  void newCanonImageReady();
  void canonCameraChanged(QString);
};

#endif  // CANON_H