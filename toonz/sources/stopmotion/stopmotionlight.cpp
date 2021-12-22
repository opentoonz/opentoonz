#include "stopmotionlight.h"

#include "tenv.h"
#include "stopmotion.h"

#include <QDialog>
#include <QApplication>
#include <QDesktopWidget>
#include <QWindow>

TEnv::IntVar StopMotionBlackCapture("StopMotionBlackCapture", 0);

//=============================================================================
//=============================================================================

StopMotionLight::StopMotionLight() {
  m_blackCapture = StopMotionBlackCapture;

  m_fullScreen1 = new QDialog();
  m_fullScreen1->setModal(false);
  m_fullScreen1->setStyleSheet("background-color:black;");
  m_screenCount = QApplication::desktop()->screenCount();
  if (m_screenCount > 1) {
    m_fullScreen2 = new QDialog();
    m_fullScreen2->setModal(false);
    m_fullScreen2->setStyleSheet("background-color:black;");
    if (m_screenCount > 2) {
      m_fullScreen3 = new QDialog();
      m_fullScreen3->setModal(false);
      m_fullScreen3->setStyleSheet("background-color:black;");
    }
  }
}
StopMotionLight::~StopMotionLight() {}

//-----------------------------------------------------------------

void StopMotionLight::setBlackCapture(bool on) {
  m_blackCapture         = on;
  StopMotionBlackCapture = int(on);
  emit(blackCaptureSignal(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen1Color(TPixel32 color) {
  m_screen1Color = color;
  emit(screen1ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen2Color(TPixel32 color) {
  m_screen2Color = color;
  emit(screen2ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen3Color(TPixel32 color) {
  m_screen3Color = color;
  emit(screen3ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen1UseOverlay(bool on) {
  m_useScreen1Overlay = on;
  emit(screen1OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen2UseOverlay(bool on) {
  m_useScreen2Overlay = on;
  emit(screen2OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen3UseOverlay(bool on) {
  m_useScreen3Overlay = on;
  emit(screen3OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::showOverlays() {
  if (getBlackCapture()) {
    m_fullScreen1->setStyleSheet("background-color: rgb(0,0,0);");
    if (m_screenCount > 1) {
      m_fullScreen2->setStyleSheet("background-color: rgb(0,0,0);");
    }
    if (m_screenCount > 2) {
      m_fullScreen3->setStyleSheet("background-color: rgb(0,0,0);");
    }
  } else {
    QString style1 = QString("background-color: rgb(%1,%2,%3);")
                         .arg(m_screen1Color.r)
                         .arg(m_screen1Color.g)
                         .arg(m_screen1Color.b);
    m_fullScreen1->setStyleSheet(style1);
    if (m_screenCount > 1) {
      QString style2 = QString("background-color: rgb(%1,%2,%3);")
                           .arg(m_screen2Color.r)
                           .arg(m_screen2Color.g)
                           .arg(m_screen2Color.b);
      m_fullScreen2->setStyleSheet(style2);
    }
    if (m_screenCount > 2) {
      QString style3 = QString("background-color: rgb(%1,%2,%3);")
                           .arg(m_screen3Color.r)
                           .arg(m_screen3Color.g)
                           .arg(m_screen3Color.b);
      m_fullScreen3->setStyleSheet(style3);
    }
  }
  bool shown       = false;
  bool isTimeLapse = StopMotion::instance()->m_isTimeLapse;
  if ((getBlackCapture() || m_useScreen1Overlay) && !isTimeLapse) {
    m_fullScreen1->showFullScreen();
    m_fullScreen1->setGeometry(QApplication::desktop()->screenGeometry(0));
    shown = true;
  }
  if (m_screenCount > 1 && (getBlackCapture() || m_useScreen2Overlay) &&
      !isTimeLapse) {
    m_fullScreen2->showFullScreen();
    m_fullScreen2->setGeometry(QApplication::desktop()->screenGeometry(1));
    shown = true;
  }
  if (m_screenCount > 2 && (getBlackCapture() || m_useScreen3Overlay) &&
      !isTimeLapse) {
    m_fullScreen3->showFullScreen();
    m_fullScreen3->setGeometry(QApplication::desktop()->screenGeometry(2));
    shown = true;
  }

  if (shown) {
    // this allows the full screen qdialogs to go full screen before
    // taking a photo
    qApp->processEvents(QEventLoop::AllEvents, 1500);
  }
  m_overlaysReady = true;
}

//-----------------------------------------------------------------

void StopMotionLight::hideOverlays() {
  if ((getBlackCapture() || m_useScreen1Overlay)) {
#ifndef WIN32
    m_fullScreen1->showNormal();
#endif
    m_fullScreen1->close();
    m_fullScreen1->windowHandle()->close();
  }
  if (m_screenCount > 1 && (getBlackCapture() || m_useScreen2Overlay)) {
#ifndef WIN32
    m_fullScreen2->showNormal();
#endif
    m_fullScreen2->close();
    m_fullScreen2->windowHandle()->close();
  }
  if (m_screenCount > 2 && (getBlackCapture() || m_useScreen3Overlay)) {
#ifndef WIN32
    m_fullScreen3->showNormal();
#endif
    m_fullScreen3->close();
    m_fullScreen3->windowHandle()->close();
  }
  m_overlaysReady = false;
}

//-----------------------------------------------------------------

bool StopMotionLight::useOverlays() {
  return m_blackCapture || m_useScreen1Overlay || m_useScreen2Overlay ||
         m_useScreen3Overlay;
}
