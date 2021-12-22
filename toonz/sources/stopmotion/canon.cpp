#include "canon.h"
#include "stopmotion.h"

#include "tapp.h"
#include "tenv.h"
#include "toonz/stage.h"
#include "toonz/tscenehandle.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"

#include <QCoreApplication>
#include <QFile>

TEnv::IntVar StopMotionUseScaledImages("StopMotionUseScaledImages", 1);

namespace {
bool l_quitLoop = false;
}

//-----------------------------------------------------------------------------

Canon::Canon() {
  m_useScaledImages = StopMotionUseScaledImages;
#ifdef WITH_CANON
  buildAvMap();
  buildIsoMap();
  buildTvMap();
  buildModeMap();
  buildExposureMap();
  buildWhiteBalanceMap();
  buildColorTemperatures();
  buildImageQualityMap();
  buildPictureStyleMap();
#endif
}

Canon::~Canon() {}

//-----------------------------------------------------------------

void Canon::onImageReady(const bool& status) { m_converterSucceeded = status; }

//-----------------------------------------------------------------

void Canon::onFinished() { l_quitLoop = true; }

//-----------------------------------------------------------------

#ifdef WITH_CANON
EdsError Canon::initializeCanonSDK() {
  m_error = EdsInitializeSDK();
  if (m_error == EDS_ERR_OK) {
    m_isSDKLoaded = true;
  }
  if (!m_error)
    m_error =
        EdsSetCameraAddedHandler(Canon::handleCameraAddedEvent, (EdsVoid*)this);
  return m_error;
}

//-----------------------------------------------------------------

EdsCameraListRef Canon::getCameraList() {
  if (!m_isSDKLoaded) initializeCanonSDK();
  if (m_isSDKLoaded) {
    m_error = EdsGetCameraList(&m_cameraList);
  }
  return m_cameraList;
}

//-----------------------------------------------------------------

EdsError Canon::releaseCameraList() {
  if (m_cameraList != NULL) m_error = EdsRelease(m_cameraList);
  return m_error;
}

//-----------------------------------------------------------------

int Canon::getCameraCount() {
  if (m_cameraList == NULL) {
    getCameraList();
  }
  if (m_cameraList != NULL) {
    m_error = EdsGetChildCount(m_cameraList, &m_count);
    if (m_count == 0) {
      m_error = EDS_ERR_DEVICE_NOT_FOUND;
      if (m_sessionOpen) {
        StopMotion::instance()->m_liveViewStatus = 0;
        m_sessionOpen                            = false;
      }
    }
    return m_count;
  } else
    return -1;
}

//-----------------------------------------------------------------

EdsError Canon::getCamera(int index) {
  if (m_count == 0) {
    m_error = EDS_ERR_DEVICE_NOT_FOUND;
  }
  if (m_count > 0) {
    m_error = EdsGetChildAtIndex(m_cameraList, index, &m_camera);
  }
  return m_error;
}

//-----------------------------------------------------------------

EdsError Canon::releaseCamera() {
  if (m_camera != NULL) {
    m_error = EdsRelease(m_camera);
  }
  return m_error;
}

//-----------------------------------------------------------------

void Canon::cameraAdded() { StopMotion::instance()->refreshCameraList(); }

//-----------------------------------------------------------------

void Canon::closeCanonSDK() {
  if (m_isSDKLoaded) {
    EdsTerminateSDK();
  }
}

//-----------------------------------------------------------------

void Canon::closeAll() {
#ifdef WITH_CANON
  if (m_camera) releaseCamera();
  if (m_cameraList != NULL) releaseCameraList();
  if (m_isSDKLoaded) closeCanonSDK();
#endif
}

//-----------------------------------------------------------------

void Canon::resetCanon(bool liveViewOpen) {
#ifdef WITH_CANON
  m_proxyDpi             = TPointD(0.0, 0.0);
  m_proxyImageDimensions = TDimension(0, 0);

  if (m_sessionOpen && getCameraCount() > 0) {
    if (liveViewOpen) {
      endCanonLiveView();
    }
    closeCameraSession();
  }

#endif
}

//-----------------------------------------------------------------

EdsError Canon::openCameraSession() {
  if (m_camera != NULL) {
    m_error = EdsOpenSession(m_camera);
    if (m_error == EDS_ERR_OK) m_sessionOpen = true;
  }
  m_error = EdsSetObjectEventHandler(m_camera, kEdsObjectEvent_All,
                                     Canon::handleObjectEvent, (EdsVoid*)this);

  m_error =
      EdsSetPropertyEventHandler(m_camera, kEdsPropertyEvent_All,
                                 Canon::handlePropertyEvent, (EdsVoid*)this);

  m_error = EdsSetCameraStateEventHandler(
      m_camera, kEdsStateEvent_All, Canon::handleStateEvent, (EdsVoid*)this);

  // We can't handle raw images yet, so make sure we are getting jpgs
  if (getCurrentImageQuality().contains("RAW"))
    setImageQuality("Large Fine Jpeg");

  EdsUInt32 saveto        = kEdsSaveTo_Host;
  m_error                 = EdsSetPropertyData(m_camera, kEdsPropID_SaveTo, 0,
                               sizeof(EdsUInt32), &saveto);
  EdsCapacity newCapacity = {0x7FFFFFFF, 0x1000, 1};
  m_error                 = EdsSetCapacity(m_camera, newCapacity);
  return m_error;
}

//-----------------------------------------------------------------

EdsError Canon::closeCameraSession() {
  if (m_camera != NULL) {
    m_error       = EdsCloseSession(m_camera);
    m_sessionOpen = false;
  }
  return m_error;
}

//-----------------------------------------------------------------

void Canon::refreshOptions() {
  getAvailableShutterSpeeds();
  getAvailableIso();
  getAvailableApertures();
  getAvailableExposureCompensations();
  getAvailableWhiteBalances();
  buildColorTemperatures();
  getAvailableImageQualities();
  getAvailablePictureStyles();
}

//-----------------------------------------------------------------

std::string Canon::getCameraName() {
  EdsChar name[EDS_MAX_NAME];
  EdsError err = EDS_ERR_OK;
  EdsDataType dataType;
  EdsUInt32 dataSize;
  m_error = EdsGetPropertySize(m_camera, kEdsPropID_ProductName, 0, &dataType,
                               &dataSize);

  if (m_error == EDS_ERR_OK) {
    m_error =
        EdsGetPropertyData(m_camera, kEdsPropID_ProductName, 0, dataSize, name);
  }
  m_cameraName = name;
  return m_cameraName;
}

//-----------------------------------------------------------------

QString Canon::getMode() {
  EdsError err = EDS_ERR_OK;
  EdsDataType modeType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_AEMode, 0, &modeType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_AEMode, 0, sizeof(size), &data);

  return QString::fromStdString(m_modeMap[data]);
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableIso() {
  EdsPropertyDesc* IsoDesc = new EdsPropertyDesc;
  EdsError err             = EDS_ERR_OK;
  m_isoOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_ISOSpeed, IsoDesc);
  int count = IsoDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_isoOptions.push_back(
          QString::fromStdString(m_isoMap[IsoDesc->propDesc[i]]));
      i++;
    }
  }
  delete IsoDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableShutterSpeeds() {
  EdsPropertyDesc* TvDesc = new EdsPropertyDesc;
  EdsError err            = EDS_ERR_OK;
  m_shutterSpeedOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_Tv, TvDesc);
  int count = TvDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_shutterSpeedOptions.push_back(
          QString::fromStdString(m_tvMap[TvDesc->propDesc[i]]));
      i++;
    }
  }
  delete TvDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableApertures() {
  EdsPropertyDesc* AvDesc = new EdsPropertyDesc;
  EdsError err            = EDS_ERR_OK;
  m_apertureOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_Av, AvDesc);
  int count = AvDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_apertureOptions.push_back(
          QString::fromStdString(m_avMap[AvDesc->propDesc[i]]));
      i++;
    }
  }
  delete AvDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableExposureCompensations() {
  EdsPropertyDesc* exposureDesc = new EdsPropertyDesc;
  EdsError err                  = EDS_ERR_OK;
  m_exposureOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_ExposureCompensation,
                           exposureDesc);
  int count = exposureDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_exposureOptions.push_back(
          QString::fromStdString(m_exposureMap[exposureDesc->propDesc[i]]));
      i++;
    }
  }
  delete exposureDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableWhiteBalances() {
  EdsPropertyDesc* whiteBalanceDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_whiteBalanceOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_WhiteBalance, whiteBalanceDesc);
  int count = whiteBalanceDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_whiteBalanceOptions.push_back(QString::fromStdString(
          m_whiteBalanceMap[whiteBalanceDesc->propDesc[i]]));
      i++;
    }
  }
  delete whiteBalanceDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailableImageQualities() {
  EdsPropertyDesc* imageQualityDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_imageQualityOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_ImageQuality, imageQualityDesc);
  int count = imageQualityDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      QString quality = QString::fromStdString(
          m_imageQualityMap[imageQualityDesc->propDesc[i]]);
      if (!quality.contains("RAW")) {
        m_imageQualityOptions.push_back(quality);
      }
      i++;
    }
  }
  delete imageQualityDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::getAvailablePictureStyles() {
  EdsPropertyDesc* pictureStyleDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_pictureStyleOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_PictureStyle, pictureStyleDesc);
  int count = pictureStyleDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_pictureStyleOptions.push_back(QString::fromStdString(
          m_pictureStyleMap[pictureStyleDesc->propDesc[i]]));
      i++;
    }
  }
  delete pictureStyleDesc;
  return err;
}

//-----------------------------------------------------------------

void Canon::buildColorTemperatures() {
  m_colorTempOptions.clear();
  int i = 2800;
  while (i <= 10000) {
    m_colorTempOptions.push_back(QString::number(i));
    i += 100;
  }
}

//-----------------------------------------------------------------

QString Canon::getCurrentShutterSpeed() {
  EdsError err = EDS_ERR_OK;
  EdsDataType tvType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_Tv, 0, &tvType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_Tv, 0, sizeof(size), &data);

  return QString::fromStdString(m_tvMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentIso() {
  EdsError err = EDS_ERR_OK;
  EdsDataType isoType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ISOSpeed, 0, &isoType, &size);
  err =
      EdsGetPropertyData(m_camera, kEdsPropID_ISOSpeed, 0, sizeof(size), &data);

  return QString::fromStdString(m_isoMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentAperture() {
  EdsError err = EDS_ERR_OK;
  EdsDataType avType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_Av, 0, &avType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_Av, 0, sizeof(size), &data);

  return QString::fromStdString(m_avMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentExposureCompensation() {
  EdsError err = EDS_ERR_OK;
  EdsDataType exposureType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ExposureCompensation, 0,
                           &exposureType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ExposureCompensation, 0,
                           sizeof(size), &data);

  return QString::fromStdString(m_exposureMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentWhiteBalance() {
  EdsError err = EDS_ERR_OK;
  EdsDataType whiteBalanceType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_WhiteBalance, 0,
                           &whiteBalanceType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_WhiteBalance, 0, sizeof(size),
                           &data);
  std::string wbString = m_whiteBalanceMap[data];
  return QString::fromStdString(m_whiteBalanceMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentImageQuality() {
  EdsError err = EDS_ERR_OK;
  EdsDataType imageQualityType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ImageQuality, 0,
                           &imageQualityType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ImageQuality, 0, sizeof(size),
                           &data);
  std::string wbString = m_imageQualityMap[data];
  return QString::fromStdString(m_imageQualityMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentPictureStyle() {
  EdsError err = EDS_ERR_OK;
  EdsDataType pictureStyleType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_PictureStyle, 0,
                           &pictureStyleType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(size),
                           &data);
  std::string wbString = m_pictureStyleMap[data];
  return QString::fromStdString(m_pictureStyleMap[data]);
}

//-----------------------------------------------------------------

QString Canon::getCurrentColorTemperature() {
  EdsError err = EDS_ERR_OK;
  EdsDataType colorTempType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ColorTemperature, 0,
                           &colorTempType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ColorTemperature, 0,
                           sizeof(size), &data);

  return QString::number(data);
}

//-----------------------------------------------------------------

QString Canon::getCurrentBatteryLevel() {
  EdsError err = EDS_ERR_OK;
  EdsDataType batteryLevelType;
  EdsUInt32 size;
  EdsUInt32 data;
  QString result;

  err = EdsGetPropertySize(m_camera, kEdsPropID_BatteryLevel, 0,
                           &batteryLevelType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_BatteryLevel, 0, sizeof(size),
                           &data);
  if (data == 0xffffffff) {
    result = tr("AC Power");
  } else if (data >= 80) {
    result = tr("Full");
  } else {
    // at least  the 60D reports battery values as 59, 49. . .
    // round up
    if (data % 10 == 9) data += 1;
    result = QString::number(data) + "%";
  }
  return result;
}

//-----------------------------------------------------------------

EdsError Canon::setShutterSpeed(QString shutterSpeed) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_tvMap.begin();
  while (it != m_tvMap.end()) {
    if (it->second == shutterSpeed.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_Tv, 0, sizeof(value), &value);
  emit(shutterSpeedChangedSignal(shutterSpeed));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setIso(QString iso) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_isoMap.begin();
  while (it != m_isoMap.end()) {
    if (it->second == iso.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ISOSpeed, 0, sizeof(value),
                           &value);
  emit(isoChangedSignal(iso));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setAperture(QString aperture) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_avMap.begin();
  while (it != m_avMap.end()) {
    if (it->second == aperture.toStdString()) {
      value = it->first;
      err =
          EdsSetPropertyData(m_camera, kEdsPropID_Av, 0, sizeof(value), &value);
      if (err == EDS_ERR_OK) {
        break;
      } else {
        // some aperture values have two entries in the avMap
        it++;
        value = it->first;
        err   = EdsSetPropertyData(m_camera, kEdsPropID_Av, 0, sizeof(value),
                                 &value);
        break;
      }
    }
    it++;
  }
  emit(apertureChangedSignal(aperture));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setExposureCompensation(QString exposure) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_exposureMap.begin();
  while (it != m_exposureMap.end()) {
    if (it->second == exposure.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ExposureCompensation, 0,
                           sizeof(value), &value);
  emit(exposureChangedSignal(exposure));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setWhiteBalance(QString whiteBalance) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_whiteBalanceMap.begin();
  while (it != m_whiteBalanceMap.end()) {
    if (it->second == whiteBalance.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_WhiteBalance, 0, sizeof(value),
                           &value);
  emit(whiteBalanceChangedSignal(whiteBalance));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setImageQuality(QString quality) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_imageQualityMap.begin();
  while (it != m_imageQualityMap.end()) {
    if (it->second == quality.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ImageQuality, 0, sizeof(value),
                           &value);
  emit(imageQualityChangedSignal(quality));
  m_proxyImageDimensions = TDimension(0, 0);
  m_proxyDpi             = TPointD(0.0, 0.0);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setPictureStyle(QString style) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_pictureStyleMap.begin();
  while (it != m_pictureStyleMap.end()) {
    if (it->second == style.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(value),
                           &value);
  err = EdsSetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(value),
                           &value);
  emit(pictureStyleChangedSignal(style));
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::setColorTemperature(QString temp) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  value = temp.toInt();

  err = EdsSetPropertyData(m_camera, kEdsPropID_ColorTemperature, 0,
                           sizeof(value), &value);
  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_ColorTemperature, 0,
                           sizeof(value), &value);
  emit(colorTemperatureChangedSignal(temp));
  return err;
}

//-----------------------------------------------------------------

void Canon::setUseScaledImages(bool on) {
  m_useScaledImages         = on;
  StopMotionUseScaledImages = int(on);
  emit(scaleFullSizeImagesSignal(on));
}

//-----------------------------------------------------------------

bool Canon::downloadImage(EdsBaseRef object) {
  EdsError err        = EDS_ERR_OK;
  EdsStreamRef stream = NULL;
  EdsDirectoryItemInfo dirItemInfo;

  err = EdsGetDirectoryItemInfo(object, &dirItemInfo);
  err = EdsCreateMemoryStream(0, &stream);
  err = EdsDownload(object, dirItemInfo.size, stream);
  EdsDownloadComplete(object);

  // tj code

#ifdef MACOSX
  UInt64 mySize = 0;
#else
  unsigned __int64 mySize = 0;
#endif
  unsigned char* data = NULL;
  err                 = EdsGetPointer(stream, (EdsVoid**)&data);
  err                 = EdsGetLength(stream, &mySize);

  int width, height, pixelFormat;
  // long size;
  int inSubsamp, inColorspace;
  // unsigned long jpegSize;
  tjhandle tjInstance   = NULL;
  unsigned char* imgBuf = NULL;
  tjInstance            = tjInitDecompress();
  tjDecompressHeader3(tjInstance, data, mySize, &width, &height, &inSubsamp,
                      &inColorspace);

  pixelFormat = TJPF_BGRX;
  imgBuf = (unsigned char*)tjAlloc(width * height * tjPixelSize[pixelFormat]);
  int flags = 0;
  flags |= TJFLAG_BOTTOMUP;
  int tempWidth, tempHeight;

  if (m_useScaledImages) {
    int factorsNum;
    tjscalingfactor scalingFactor = {1, 1};
    tjscalingfactor* factor       = tjGetScalingFactors(&factorsNum);
    int intRatio                  = (float)width / (float)height * 100.0;
    int i                         = 0;

    TCamera* camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimension res = camera->getRes();

    // find the scaling factor that is at least as big as the current camera
    while (i < factorsNum) {
      scalingFactor = factor[i];
      tempWidth     = TJSCALED(width, scalingFactor);
      if (tempWidth < res.lx && i > 0) {
        scalingFactor = factor[i - 1];
        break;
      }
      i++;
    }
    // make sure the scaling factor has the right aspect ratio
    while (i >= 0) {
      tempWidth  = TJSCALED(width, scalingFactor);
      tempHeight = TJSCALED(height, scalingFactor);
      if ((int)((float)tempWidth / (float)tempHeight * 100.0) == intRatio) {
        break;
      }
      i--;
      scalingFactor = factor[i];
    }
  } else {
    tempWidth  = width;
    tempHeight = height;
  }

  if (m_useScaledImages || !getCurrentImageQuality().contains("Large")) {
    TCamera* camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimensionD size       = camera->getSize();
    m_proxyImageDimensions = TDimension(tempWidth, tempHeight);
    double minimumDpi      = std::min(m_proxyImageDimensions.lx / size.lx,
                                 m_proxyImageDimensions.ly / size.ly);
    m_proxyDpi             = TPointD(minimumDpi, minimumDpi);
  }

  tjDecompress2(tjInstance, data, mySize, imgBuf, tempWidth,
                tempWidth * tjPixelSize[pixelFormat], tempHeight, pixelFormat,
                flags);

  StopMotion::instance()->m_newImage = TRaster32P(tempWidth, tempHeight);
  StopMotion::instance()->m_newImage->lock();
  uchar* rawData = StopMotion::instance()->m_newImage->getRawData();
  memcpy(rawData, imgBuf, tempWidth * tempHeight * tjPixelSize[pixelFormat]);
  StopMotion::instance()->m_newImage->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  // end tj code

  if (m_useScaledImages) {
    QFile fullImage(StopMotion::instance()->m_tempFile);
    fullImage.open(QIODevice::WriteOnly);
    QDataStream dataStream(&fullImage);
    dataStream.writeRawData((const char*)data, mySize);
    fullImage.close();
  }

  EdsRelease(stream);
  stream = NULL;
  if (object) EdsRelease(object);

  if (err == EDS_ERR_OK) {
    emit(newCanonImageReady());
  }

  return err;
}

//-----------------------------------------------------------------

EdsError Canon::takePicture() {
  EdsError err;
  err = EdsSendCommand(m_camera, kEdsCameraCommand_PressShutterButton,
                       kEdsCameraCommand_ShutterButton_Completely_NonAF);
  err = EdsSendCommand(m_camera, kEdsCameraCommand_PressShutterButton,
                       kEdsCameraCommand_ShutterButton_OFF);
  return err;
}

//-----------------------------------------------------------------

void Canon::extendCameraOnTime() {
  EdsError err;
  EdsInt32 param = 0;
  err = EdsSendCommand(m_camera, kEdsCameraCommand_ExtendShutDownTimer, param);
}

//-----------------------------------------------------------------

EdsError Canon::startCanonLiveView() {
  if (m_camera && m_sessionOpen) {
    EdsError err = EDS_ERR_OK;
    // Get the output device for the live view image
    EdsUInt32 device;
    err = EdsGetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                             sizeof(device), &device);
    // PC live view starts by setting the PC as the output device for the live
    // view image.
    if (err == EDS_ERR_OK) {
      device |= kEdsEvfOutputDevice_PC;
      err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                               sizeof(device), &device);
    }
    if (err == EDS_ERR_OK) {
      StopMotion::instance()->m_liveViewStatus = 1;
    }
    // A property change event notification is issued from the camera if
    // property settings are made successfully. Start downloading of the live
    // view image once the property change notification arrives.
    return err;
  } else
    return EDS_ERR_DEVICE_NOT_FOUND;
}

//-----------------------------------------------------------------

EdsError Canon::endCanonLiveView() {
  EdsError err = EDS_ERR_OK;
  // Get the output device for the live view image
  EdsUInt32 device;
  err = EdsGetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                           sizeof(device), &device);
  // PC live view ends if the PC is disconnected from the live view image output
  // device.
  if (err == EDS_ERR_OK) {
    device &= ~kEdsEvfOutputDevice_PC;
    err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                             sizeof(device), &device);
  }
  StopMotion::instance()->m_liveViewStatus = 0;
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::zoomLiveView() {
  if (!m_sessionOpen || !StopMotion::instance()->m_liveViewStatus > 0)
    return EDS_ERR_DEVICE_INVALID;
  EdsError err = EDS_ERR_OK;
  if (m_pickLiveViewZoom) toggleZoomPicking();
  if (m_liveViewZoom == 1) {
    m_liveViewZoom = 5;
    m_zooming      = true;
    StopMotion::instance()->toggleNumpadForFocusCheck(true);
  } else if (m_liveViewZoom == 5) {
    m_liveViewZoom = 1;
    m_zooming      = false;
    StopMotion::instance()->toggleNumpadForFocusCheck(false);
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_Zoom, 0,
                           sizeof(m_liveViewZoom), &m_liveViewZoom);
  if (m_liveViewZoom == 5) setZoomPoint();
  emit(focusCheckToggled(m_liveViewZoom > 1));
  return err;
}

//-----------------------------------------------------------------

void Canon::makeZoomPoint(TPointD pos) {
  m_liveViewZoomPickPoint = pos;
  calculateZoomPoint();
}

//-----------------------------------------------------------------

void Canon::toggleZoomPicking() {
  if (m_sessionOpen && StopMotion::instance()->m_liveViewStatus > 0 &&
      !m_zooming) {
    if (m_pickLiveViewZoom) {
      m_pickLiveViewZoom = false;
      StopMotion::instance()->toggleNumpadForFocusCheck(false);
    } else {
      m_pickLiveViewZoom = true;
      if (m_liveViewZoomPickPoint == TPointD(0.0, 0.0)) {
        makeZoomPoint(m_liveViewZoomPickPoint);
      }
      StopMotion::instance()->toggleNumpadForFocusCheck(true);
    }
  }
  emit(pickFocusCheckToggled(m_pickLiveViewZoom));
}

//-----------------------------------------------------------------

EdsError Canon::setZoomPoint() {
  // make sure this is set AFTER starting zoom
  EdsError err              = EDS_ERR_OK;
  m_liveViewZoomReadyToPick = false;
  EdsPoint zoomPoint;

  calculateZoomPoint();

  zoomPoint.x = m_finalZoomPoint.x;
  zoomPoint.y = m_finalZoomPoint.y;

  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_ZoomPosition, 0,
                           sizeof(zoomPoint), &zoomPoint);
  m_liveViewZoomReadyToPick = true;
  return err;
}

//-----------------------------------------------------------------

void Canon::calculateZoomPoint() {
  m_fullImageDimensions = StopMotion::instance()->m_fullImageDimensions;
  m_fullImageDpi        = StopMotion::instance()->m_fullImageDpi;

  bool outOfBounds = false;
  if (m_liveViewZoomPickPoint == TPointD(0.0, 0.0)) {
    m_calculatedZoomPoint =
        TPoint(m_fullImageDimensions.lx / 2, m_fullImageDimensions.ly / 2);
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRectDimensions.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRectDimensions.y / 2);
  } else {
    // get the image size in OpenToonz dimensions
    double maxFullWidth =
        (double)m_fullImageDimensions.lx / m_fullImageDpi.x * Stage::inch;
    double maxFullHeight =
        (double)m_fullImageDimensions.ly / m_fullImageDpi.y * Stage::inch;
    // OpenToonz coordinates are based on center at 0, 0
    // convert that to top left based coordinates
    double newX = m_liveViewZoomPickPoint.x + maxFullWidth / 2.0;
    double newY = -m_liveViewZoomPickPoint.y + maxFullHeight / 2.0;
    // convert back to the normal image dimensions to talk to the camera
    m_calculatedZoomPoint.x = newX / Stage::inch * m_fullImageDpi.x;
    m_calculatedZoomPoint.y = newY / Stage::inch * m_fullImageDpi.x;
    // the Canon SDK wants the top left corner of the zoom rect, not the center
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRectDimensions.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRectDimensions.y / 2);
    // finally make sure everything actually fits on the image
    if (m_finalZoomPoint.x < 0) {
      m_finalZoomPoint.x = 0;
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.y < 0) {
      m_finalZoomPoint.y = 0;
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.x >
        m_fullImageDimensions.lx - (m_zoomRectDimensions.x)) {
      m_finalZoomPoint.x = m_fullImageDimensions.lx - (m_zoomRectDimensions.x);
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.y >
        m_fullImageDimensions.ly - (m_zoomRectDimensions.y)) {
      m_finalZoomPoint.y = m_fullImageDimensions.ly - (m_zoomRectDimensions.y);
      outOfBounds        = true;
    }

    // if out of bounds, recalculate backwards to get the correct location
    if (outOfBounds) {
      TPoint tempCalculated;
      // recenter the point in the rect
      tempCalculated.x = m_finalZoomPoint.x + (m_zoomRectDimensions.x / 2);
      tempCalculated.y = m_finalZoomPoint.y + (m_zoomRectDimensions.y / 2);
      // convert to OpenToonz Dimensions
      newX = tempCalculated.x / m_fullImageDpi.x * Stage::inch;
      newY = tempCalculated.y / m_fullImageDpi.y * Stage::inch;
      // get center based coordinates
      m_liveViewZoomPickPoint.x = newX - (maxFullWidth / 2.0);
      m_liveViewZoomPickPoint.y = (newY - (maxFullHeight / 2.0)) * -1;
    }
  }

  // calculate the zoom rectangle position on screen

  // get the image size in OpenToonz dimensions
  double maxFullWidth =
      (double)m_fullImageDimensions.lx / m_fullImageDpi.x * Stage::inch;
  double maxFullHeight =
      (double)m_fullImageDimensions.ly / m_fullImageDpi.y * Stage::inch;
  m_zoomRect =
      TRect(m_finalZoomPoint.x, m_finalZoomPoint.y + m_zoomRectDimensions.y,
            m_finalZoomPoint.x + m_zoomRectDimensions.x, m_finalZoomPoint.y);
  TRect tempCalculated;

  // convert to OpenToonz Dimensions
  tempCalculated.x0 = m_zoomRect.x0 / m_fullImageDpi.x * Stage::inch;
  tempCalculated.y0 = m_zoomRect.y0 / m_fullImageDpi.y * Stage::inch;
  tempCalculated.x1 = m_zoomRect.x1 / m_fullImageDpi.x * Stage::inch;
  tempCalculated.y1 = m_zoomRect.y1 / m_fullImageDpi.y * Stage::inch;
  // get center based coordinates
  m_zoomRect.x0 = tempCalculated.x0 - (maxFullWidth / 2.0);
  m_zoomRect.y0 = (tempCalculated.y0 - (maxFullHeight / 2.0)) * -1;
  m_zoomRect.x1 = tempCalculated.x1 - (maxFullWidth / 2.0);
  m_zoomRect.y1 = (tempCalculated.y1 - (maxFullHeight / 2.0)) * -1;
}

//-----------------------------------------------------------------

bool Canon::downloadEVFData() {
  EdsError err            = EDS_ERR_OK;
  EdsStreamRef stream     = NULL;
  EdsEvfImageRef evfImage = NULL;

  // Create memory stream.
  err = EdsCreateMemoryStream(0, &stream);
  // Create EvfImageRef.
  if (err == EDS_ERR_OK) {
    err = EdsCreateEvfImageRef(stream, &evfImage);
  }
  // Download live view image data.
  if (err == EDS_ERR_OK) {
    err = EdsDownloadEvfImage(m_camera, evfImage);
  }

  EdsDataType evfZoom;
  EdsDataType evfZoomPos;
  EdsDataType evfZoomRect;
  EdsUInt32 size;
  EdsUInt32 sizePos;
  EdsUInt32 sizeRect;
  EdsUInt32 sizeImagePos;
  EdsUInt32 sizeCoordSys;
  EdsUInt32 zoomAmount;
  EdsPoint zoomPos;
  EdsRect zoomRect;
  EdsPoint imagePos;
  EdsSize coordSys;

  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_Zoom, 0, &evfZoom, &size);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_Zoom, 0, sizeof(size),
                           &zoomAmount);

  // this is the top left corner of the zoomed image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ZoomPosition, 0,
                           &evfZoomPos, &sizePos);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomPosition, 0, sizePos,
                           &zoomPos);
  // this is the size of the zoomed image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ZoomRect, 0, &evfZoomRect,
                           &sizeRect);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomRect, 0, sizeRect,
                           &zoomRect);

  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ImagePosition, 0,
                           &evfZoomRect, &sizeImagePos);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ImagePosition, 0,
                           sizeImagePos, &imagePos);
  // this returns the size of the full image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_CoordinateSystem, 0,
                           &evfZoomRect, &sizeCoordSys);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_CoordinateSystem, 0,
                           sizeCoordSys, &coordSys);

  // Get the incidental data of the image.
  if (err == EDS_ERR_OK) {
    JpgConverter* converter = new JpgConverter;
    converter->setStream(stream);
    converter->setScale(m_useScaledImages);

    connect(converter, SIGNAL(imageReady(bool)), this, SLOT(onImageReady(bool)),
            Qt::QueuedConnection);
    connect(converter, SIGNAL(finished()), this, SLOT(onFinished()),
            Qt::QueuedConnection);

    converter->start();

    while (!l_quitLoop)
      QCoreApplication::processEvents(QEventLoop::AllEvents |
                                      QEventLoop::WaitForMoreEvents);

    l_quitLoop                                 = false;
    StopMotion::instance()->m_liveViewImage    = converter->getImage();
    StopMotion::instance()->m_hasLiveViewImage = true;
    delete converter;
    if (stream != NULL) {
      EdsRelease(stream);
    }
    stream = NULL;
    if (evfImage != NULL) {
      EdsRelease(evfImage);
      evfImage = NULL;
    }
    if (!m_converterSucceeded) return EDS_ERR_UNEXPECTED_EXCEPTION;

    // sometimes zoomRect returns the wrong dimensions if called too fast
    if (zoomRect.size.width < 5000 && zoomRect.size.height < 5000) {
      m_zoomRectDimensions = TPoint(zoomRect.size.width, zoomRect.size.height);
      if (m_zoomRect == TRect(0, 0, 0, 0)) {
        m_zoomRect = TRect(zoomPos.x, zoomPos.y + zoomRect.size.height,
                           zoomPos.x + zoomRect.size.width, zoomPos.y);
        calculateZoomPoint();
      }
    }
    if (zoomAmount == 5 && m_zoomRectDimensions == TPoint(0, 0)) {
      setZoomPoint();
    }
    StopMotion::instance()->m_fullImageDimensions =
        TDimension(coordSys.width, coordSys.height);

    return true;
  }

  return false;
}

//-----------------------------------------------------------------

EdsError Canon::focusNear() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near1);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::focusFar() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far1);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::focusNear2() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near2);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::focusFar2() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far2);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::focusNear3() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near3);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::focusFar3() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far3);
  return err;
}

//-----------------------------------------------------------------

EdsError Canon::handleObjectEvent(EdsObjectEvent event, EdsBaseRef object,
                                  EdsVoid* context) {
  if (event == kEdsObjectEvent_DirItemRequestTransfer) {
    instance()->downloadImage(object);
  }

  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError Canon::handlePropertyEvent(EdsPropertyEvent event,
                                    EdsPropertyID property, EdsUInt32 param,
                                    EdsVoid* context) {
  if (property == kEdsPropID_Evf_OutputDevice &&
      event == kEdsPropertyEvent_PropertyChanged) {
    if (StopMotion::instance()->m_liveViewStatus == 1)
      StopMotion::instance()->m_liveViewStatus = 2;
  }
  if (property == kEdsPropID_AEMode &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->modeChanged());
  }
  if (property == kEdsPropID_BatteryLevel &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->modeChanged());
  }
  if (property == kEdsPropID_Av &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->apertureOptionsChanged());
  }

  if (property == kEdsPropID_Av && event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->apertureChangedSignal(instance()->getCurrentAperture()));
  }

  if (property == kEdsPropID_Tv &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->shutterSpeedOptionsChanged());
  }
  if (property == kEdsPropID_Tv && event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->shutterSpeedChangedSignal(
        instance()->getCurrentShutterSpeed()));
  }
  if (property == kEdsPropID_ISOSpeed &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->isoOptionsChanged());
  }
  if (property == kEdsPropID_ISOSpeed &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->isoChangedSignal(instance()->getCurrentIso()));
  }
  if (property == kEdsPropID_ExposureCompensation &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->exposureOptionsChanged());
  }
  if (property == kEdsPropID_ExposureCompensation &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->exposureChangedSignal(
        instance()->getCurrentExposureCompensation()));
  }
  if (property == kEdsPropID_WhiteBalance &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->whiteBalanceOptionsChanged());
  }
  if (property == kEdsPropID_WhiteBalance &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->whiteBalanceChangedSignal(
        instance()->getCurrentWhiteBalance()));
  }
  if (property == kEdsPropID_PictureStyle &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->pictureStyleOptionsChanged());
  }
  if (property == kEdsPropID_PictureStyle &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->pictureStyleChangedSignal(
        instance()->getCurrentPictureStyle()));
  }
  if (property == kEdsPropID_ImageQuality &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->imageQualityOptionsChanged());
  }
  if (property == kEdsPropID_ImageQuality &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->imageQualityChangedSignal(
        instance()->getCurrentImageQuality()));
  }

  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError Canon::handleStateEvent(EdsStateEvent event, EdsUInt32 parameter,
                                 EdsVoid* context) {
  if (event == kEdsStateEvent_Shutdown) {
    instance()->m_sessionOpen = false;
    instance()->releaseCamera();
    if (instance()->m_sessionOpen && instance()->getCameraCount() > 0) {
      // instance()->closeCameraSession();
    }
    StopMotion::instance()->m_liveViewStatus = 0;
    emit(instance()->canonCameraChanged(QString("")));
  }
  if (event == kEdsStateEvent_WillSoonShutDown) {
    instance()->extendCameraOnTime();
    if (StopMotion::instance()->m_liveViewStatus > 1) {
      StopMotion::instance()->toggleLiveView();
    }
  }
  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError Canon::handleCameraAddedEvent(EdsVoid* context) {
  instance()->cameraAdded();
  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

void Canon::buildAvMap() {
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x00, "00"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x08, "1"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x0B, "1.1"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x0C, "1.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x0D, "1.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x10, "1.4"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x13, "1.6"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x14, "1.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x15, "1.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x18, "2"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x1B, "2.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x1C, "2.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x1D, "2.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x20, "2.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x23, "3.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x24, "3.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x25, "3.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x28, "4"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x2B, "4.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x2C, "4.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x2D, "5.0"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x30, "5.6"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x33, "6.3"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x34, "6.7"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x35, "7.1"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x38, "8"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x3B, "9"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x3C, "9.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x3D, "10"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x40, "11"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x43, "13"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x44, "13"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x45, "14"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x48, "16"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x4B, "18"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x4C, "19"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x4D, "20"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x50, "22"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x53, "25"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x54, "27"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x55, "29"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x58, "32"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x5B, "36"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x5C, "38"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x5D, "40"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x60, "45"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x63, "51"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x64, "54"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x65, "57"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x68, "64"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x6B, "72"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x6C, "76"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x6D, "80"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0x70, "91"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0xFF, "Auto"));
  m_avMap.insert(std::pair<EdsUInt32, const char*>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void Canon::buildIsoMap() {
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x00, "Auto"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x28, "6"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x30, "12"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x38, "25"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x40, "50"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x48, "100"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x4b, "125"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x4d, "160"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x50, "200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x53, "250"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x55, "320"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x58, "400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x5b, "500"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x5d, "640"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x60, "800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x63, "1000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x65, "1250"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x68, "1600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x6b, "2000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x6d, "2500"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x70, "3200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x73, "4000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x75, "5000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x78, "6400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x7b, "8000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x7d, "10000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x80, "12800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x83, "16000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x85, "20000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x88, "25600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x8b, "32000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x8d, "40000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x90, "51200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0x98, "102400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0xa0, "204800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0xa8, "409600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char*>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void Canon::buildTvMap() {
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x04, "Auto"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x0c, "Bulb"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x10, "30\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x13, "25\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x14, "20\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x15, "20\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x18, "15\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x1B, "13\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x1C, "10\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x1D, "10\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x20, "8\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x23, "6\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x24, "6\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x25, "5\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x28, "4\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x2B, "3\"2"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x2C, "3\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x2D, "2\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x30, "2\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x33, "1\"6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x34, "1\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x35, "1\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x38, "1\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x3B, "0\"8"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x3C, "0\"7"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x3D, "0\"6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x40, "0\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x43, "0\"4"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x44, "0\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x45, "0\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x48, "1/4"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x4B, "1/5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x4C, "1/6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x4D, "1/6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x50, "1/8"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x53, "1/10"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x54, "1/10"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x55, "1/13"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x58, "1/15"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x5B, "1/20"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x5C, "1/20"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x5D, "1/25"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x60, "1/30"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x63, "1/40"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x64, "1/45"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x65, "1/50"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x68, "1/60"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x6B, "1/80"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x6C, "1/90"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x6D, "1/100"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x70, "1/125"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x73, "1/160"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x74, "1/180"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x75, "1/200"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x78, "1/250"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x7B, "1/320"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x7C, "1/350"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x7D, "1/400"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x80, "1/500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x83, "1/640"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x84, "1/750"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x85, "1/800"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x88, "1/1000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x8B, "1/1250"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x8C, "1/1500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x8D, "1/1600"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x90, "1/2000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x93, "1/2500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x94, "1/3000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x95, "1/3200"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x98, "1/4000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x9B, "1/5000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x9C, "1/6000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0x9D, "1/6400"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0xA0, "1/8000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char*>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void Canon::buildModeMap() {
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(0, "P"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(1, "Tv"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(2, "Av"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(3, "M"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(55, "FV"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(4, "Bulb"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(5, "A-DEP"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(6, "DEP"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(7, "C1"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(16, "C2"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(17, "C3"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(8, "Lock"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(9, "GreenMode"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(10, "Night Portrait"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(11, "Sports"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(13, "LandScape"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(14, "Close-Up"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(15, "No Strobo"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(12, "Portrait"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(19, "Creative Auto"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(20, "Movies"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(25, "SCN"));
  m_modeMap.insert(
      std::pair<EdsUInt32, const char*>(22, "Scene Intelligent Auto"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(29, "Creative filters"));
  m_modeMap.insert(std::pair<EdsUInt32, const char*>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void Canon::buildExposureMap() {
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x28, "+5"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x25, "+4 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x24, "+4 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x23, "+4 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x20, "+4"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x1D, "+3 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x1C, "+3 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x1B, "+3 1/3"));

  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x18, "+3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x15, "+2 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x14, "+2 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x13, "+2 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x10, "+2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x0d, "+1 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x0c, "+1 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x0b, "+1 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x08, "+1"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x05, "+2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x04, "+1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x03, "+1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0x00, "0"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xfd, "-1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xfc, "-1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xfb, "-2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xf8, "-1"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xf5, "-1 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xf4, "-1 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xf3, "-1 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xf0, "-2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xed, "-2 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xec, "-2 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xeb, "-2 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xe8, "-3"));

  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xE5, "-3 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xE4, "-3 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xE3, "-3 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xE0, "-4"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xDD, "-4 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xDC, "-4 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xDB, "-4 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char*>(0xD8, "-5"));

  m_exposureMap.insert(
      std::pair<EdsUInt32, const char*>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void Canon::buildWhiteBalanceMap() {
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char*>(0, "Auto: Ambience Priority"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(1, "Daylight"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(2, "Cloudy"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(3, "Tungsten"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(4, "Fluorescent"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(5, "Flash"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(6, "Manual"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(8, "Shade"));
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char*>(9, "Color Temperature"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(10, "Custom 1"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(11, "Custom 2"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(12, "Custom 3"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(15, "Manual 2"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(16, "Manual 3"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(18, "Manual 4"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(19, "Manual 5"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(20, "Custom 4"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char*>(21, "Custom 5"));
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char*>(23, "Auto: White Priority"));
}

//-----------------------------------------------------------------

void Canon::buildImageQualityMap() {
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_LR, "RAW"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRLJF, "RAW + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRMJF, "RAW + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRSJF, "RAW + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRLJN, "RAW + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRMJN, "RAW + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRSJN, "RAW + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRS1JF, "RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRS1JN, "RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRS2JF, "RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRS3JF, "RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRLJ, "RAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRM1J, "RAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRM2J, "RAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LRSJ, "RAW + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MR, "Middle Raw(Small RAW1)"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRLJF, "Middle Raw(Small RAW1) + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRMJF, "Middle Raw(Small RAW1) + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRSJF, "Middle Raw(Small RAW1) + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRLJN, "Middle Raw(Small RAW1) + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRMJN, "Middle Raw(Small RAW1) + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRSJN, "Middle Raw(Small RAW1) + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRS1JF, "Middle RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRS1JN, "Middle RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRS2JF, "Middle RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRS3JF, "Middle RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRLJ, "Middle Raw + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRM1J, "Middle Raw + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRM2J, "Middle Raw + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MRSJ, "Middle Raw + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SR, "Small RAW(Small RAW2)"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRLJF, "Small RAW(Small RAW2) + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRMJF, "Small RAW(Small RAW2) + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRSJF, "Small RAW(Small RAW2) + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRLJN, "Small RAW(Small RAW2) + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRMJN, "Small RAW(Small RAW2) + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRSJN, "Small RAW(Small RAW2) + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRS1JF, "Small RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRS1JN, "Small RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRS2JF, "Small RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRS3JF, "Small RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRLJ, "Small RAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRM1J, "Small RAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRM2J, "Small RAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SRSJ, "Small RAW + Small Jpeg"));

  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_CR, "CRAW"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRLJF, "CRAW + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRMJF, "CRAW + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM1JF, "CRAW + Middle1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM2JF, "CRAW + Middle2 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRSJF, "CRAW + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRS1JF, "CRAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRS2JF, "CRAW + Small2 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRS3JF, "CRAW + Small3 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRLJN, "CRAW + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRMJN, "CRAW + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM1JN, "CRAW + Middle1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM2JN, "CRAW + Middle2 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRSJN, "CRAW + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRS1JN, "CRAW + Small1 Normal Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRLJ, "CRAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM1J, "CRAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRM2J, "CRAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_CRSJ, "CRAW + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LJF, "Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_LJN, "Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MJF, "Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_MJN, "Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SJF, "Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_SJN, "Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_S1JF, "Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char*>(
      EdsImageQuality_S1JN, "Small1 Normal Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_S2JF, "Small2 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_S3JF, "Small3 Jpeg"));

  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_LJ, "Large Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_M1J, "Middle1 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_M2J, "Middle2 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char*>(EdsImageQuality_SJ, "Small Jpeg"));
}

//-----------------------------------------------------------------

void Canon::buildPictureStyleMap() {
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_Standard, "Standard"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_Portrait, "Portrait"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char*>(
      kEdsPictureStyle_Landscape, "Landscape"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_Neutral, "Neutral"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_Faithful, "Faithful"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char*>(
      kEdsPictureStyle_Monochrome, "Monochrome"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_Auto, "Auto"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char*>(
      kEdsPictureStyle_FineDetail, "Fine Detail"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_User1, "User 1"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_User2, "User 2"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_User3, "User 3"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_PC1, "Computer 1"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_PC2, "Computer 2"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char*>(kEdsPictureStyle_PC3, "Computer 3"));
}
#endif