#include "webcam.h"
#include "tenv.h"

#ifdef WIN32
#include <Windows.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <mfidl.h>
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

#include <QCamera>
#include <QCameraInfo>
#include <QApplication>

TEnv::IntVar StopMotionUseDirectShow("StopMotionUseDirectShow", 1);
TEnv::IntVar StopMotionUseMjpg("StopMotionUseMjpg", 1);
//-----------------------------------------------------------------------------

Webcam::Webcam() {
  m_useDirectShow = StopMotionUseDirectShow;
  m_useMjpg       = StopMotionUseMjpg;
}

//-----------------------------------------------------------------

Webcam::~Webcam() {}

//-----------------------------------------------------------------

QList<QCameraInfo> Webcam::getWebcams() {
  m_webcams.clear();
  m_webcams = QCameraInfo::availableCameras();
  return m_webcams;
}

//-----------------------------------------------------------------

void Webcam::setWebcam(QCamera* camera) { m_webcam = camera; }

//-----------------------------------------------------------------

bool Webcam::initWebcam(int index) {
#ifdef WIN32
  if (!m_useDirectShow) {
    // the webcam order obtained from Qt isn't always the same order as
    // the one obtained from OpenCV without DirectShow
    translateIndex(index);
    m_cvWebcam.open(m_webcamIndex);
  } else {
    m_webcamIndex = index;
    m_cvWebcam.open(m_webcamIndex, cv::CAP_DSHOW);
  }
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }
#else
  m_webcamIndex = index;
  m_cvWebcam.open(index);
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }
#endif
  // mjpg is used by many webcams
  // opencv runs very slow on some webcams without it.
  if (m_useMjpg) {
    m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                   cv::VideoWriter::fourcc('m', 'j', 'p', 'g'));
    m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                   cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
  }
  m_cvWebcam.set(3, m_webcamWidth);
  m_cvWebcam.set(4, m_webcamHeight);

  return true;
}

//-----------------------------------------------------------------

void Webcam::releaseWebcam() { m_cvWebcam.release(); }

//-----------------------------------------------------------------

int Webcam::getIndexOfResolution() {
  return m_webcamResolutions.indexOf(QSize(m_webcamWidth, m_webcamHeight));
}

//-----------------------------------------------------------------

bool Webcam::getWebcamImage(TRaster32P& tempImage) {
  bool error = false;
  cv::Mat imgOriginal;
  cv::Mat imgCorrected;

  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    // mjpg is used by many webcams
    // opencv runs very slow on some webcams without it.
    if (m_useMjpg) {
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('m', 'j', 'p', 'g'));
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    m_cvWebcam.set(3, m_webcamWidth);
    m_cvWebcam.set(4, m_webcamHeight);

    if (!m_cvWebcam.isOpened()) {
      error = true;
    }
  }

  bool blnFrameReadSuccessfully =
      m_cvWebcam.read(imgOriginal);  // get next frame

  if (!blnFrameReadSuccessfully ||
      imgOriginal.empty()) {  // if frame not read successfully
    std::cout << "error: frame not read from webcam\n";
    error = true;  // print error message to std out
  }

  if (!error) {
    cv::cvtColor(imgOriginal, imgCorrected, cv::COLOR_BGR2BGRA);
    cv::flip(imgCorrected, imgCorrected, 0);
    int width  = m_cvWebcam.get(3);
    int height = m_cvWebcam.get(4);
    int size   = imgCorrected.total() * imgCorrected.elemSize();

    tempImage = TRaster32P(width, height);
    // m_liveViewImage = TRaster32P(width, height);
    tempImage->lock();
    uchar* imgBuf  = imgCorrected.data;
    uchar* rawData = tempImage->getRawData();
    memcpy(rawData, imgBuf, size);
    tempImage->unlock();
  }
  if (error) {
    return false;
  } else {
    return true;
  }
}

//-----------------------------------------------------------------

void Webcam::setUseDirectShow(int state) {
  m_useDirectShow         = state;
  StopMotionUseDirectShow = state;
  emit(useDirectShowSignal(state));
}

//-----------------------------------------------------------------

void Webcam::setUseMjpg(bool on) {
  m_useMjpg         = on;
  StopMotionUseMjpg = int(on);
  emit(useMjpgSignal(on));
}

//-----------------------------------------------------------------

void Webcam::clearWebcam() {
  m_webcamDescription = QString();
  m_webcamDeviceName  = QString();
  m_webcamIndex       = -1;
}

//-----------------------------------------------------------------

void Webcam::clearWebcamResolutions() { m_webcamResolutions.clear(); }

//-----------------------------------------------------------------

void Webcam::refreshWebcamResolutions() {
  clearWebcamResolutions();
  m_webcamResolutions = getWebcam()->supportedViewfinderResolutions();
}

//-----------------------------------------------------------------

bool Webcam::getWebcamAutofocusStatus() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return false;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_AUTOFOCUS);
    if (value > 0.0) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

//-----------------------------------------------------------------
void Webcam::setWebcamAutofocusStatus(bool on) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = on ? 1.0 : 0.0;
    m_cvWebcam.set(cv::CAP_PROP_AUTOFOCUS, value);
    value = m_cvWebcam.get(cv::CAP_PROP_AUTOFOCUS);
  }
}

//-----------------------------------------------------------------
int Webcam::getWebcamFocusValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_FOCUS);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamFocusValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_FOCUS, value);
  value = m_cvWebcam.get(cv::CAP_PROP_FOCUS);
}

//-----------------------------------------------------------------
int Webcam::getWebcamExposureValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_EXPOSURE);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamExposureValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
  m_cvWebcam.set(cv::CAP_PROP_EXPOSURE, value);
  value = m_cvWebcam.get(cv::CAP_PROP_EXPOSURE);
  getWebcamExposureValue();
}

//-----------------------------------------------------------------
int Webcam::getWebcamBrightnessValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_BRIGHTNESS);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamBrightnessValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_BRIGHTNESS, value);
  value = m_cvWebcam.get(cv::CAP_PROP_BRIGHTNESS);
}

//-----------------------------------------------------------------
int Webcam::getWebcamContrastValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_CONTRAST);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamContrastValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_CONTRAST, value);
  value = m_cvWebcam.get(cv::CAP_PROP_CONTRAST);
}

//-----------------------------------------------------------------
int Webcam::getWebcamGainValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_GAIN);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamGainValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_GAIN, value);
  value = m_cvWebcam.get(cv::CAP_PROP_GAIN);
}

//-----------------------------------------------------------------
int Webcam::getWebcamSaturationValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_SATURATION);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamSaturationValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_SATURATION, value);
  value = m_cvWebcam.get(cv::CAP_PROP_SATURATION);
}

//-----------------------------------------------------------------

void Webcam::openSettingsWindow() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_SETTINGS, 1.0);
}

//-----------------------------------------------------------------

bool Webcam::translateIndex(int index) {
  // We are using Qt to get the camera info and supported resolutions, but
  // we are using OpenCV to actually get the images.
  // The camera index from OpenCV and from Qt don't always agree,
  // So this checks the name against the correct index.
  m_webcamIndex = index;

#ifdef WIN32

  // Thanks to:
  // https://elcharolin.wordpress.com/2017/08/28/webcam-capture-with-the-media-foundation-sdk/
  // for the webcam enumeration here

#define CLEAN_ATTRIBUTES()                                                     \
  if (attributes) {                                                            \
    attributes->Release();                                                     \
    attributes = NULL;                                                         \
  }                                                                            \
  for (DWORD i = 0; i < count; i++) {                                          \
    if (&devices[i]) {                                                         \
      devices[i]->Release();                                                   \
      devices[i] = NULL;                                                       \
    }                                                                          \
  }                                                                            \
  CoTaskMemFree(devices);                                                      \
  return hr;

  HRESULT hr = S_OK;

  // this is important!!
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  UINT32 count              = 0;
  IMFAttributes* attributes = NULL;
  IMFActivate** devices     = NULL;

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Create an attribute store to specify enumeration parameters.
  hr = MFCreateAttributes(&attributes, 1);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }

  // The attribute to be requested is devices that can capture video
  hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Enummerate the video capture devices
  hr = MFEnumDeviceSources(attributes, &devices, &count);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // if there are any available devices
  if (count > 0) {
    WCHAR* nameString = NULL;
    // Get the human-friendly name of the device
    UINT32 cchName;

    for (int i = 0; i < count; i++) {
      hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                          &nameString, &cchName);
      std::string desc = m_webcamDescription.toStdString();
      if (nameString == m_webcamDescription.toStdWString()) {
        m_webcamIndex = i;
        break;
      }
      // devices[0]->ShutdownObject();
    }

    CoTaskMemFree(nameString);
  }
  // clean
  CLEAN_ATTRIBUTES()
#else
  return true;
#endif
}