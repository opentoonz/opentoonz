#pragma once

#ifndef STOPMOTIONLIGHT_H
#define STOPMOTIONLIGHT_H

#include <QObject>

#include "traster.h"

class QDialog;

//=============================================================================
// StopMotionLight
//-----------------------------------------------------------------------------

class StopMotionLight : public QObject {
  Q_OBJECT

public:
  StopMotionLight();
  ~StopMotionLight();

  QDialog *m_fullScreen1, *m_fullScreen2, *m_fullScreen3;
  bool m_useScreen1Overlay = false;
  bool m_useScreen2Overlay = false;
  bool m_useScreen3Overlay = false;
  bool m_blackCapture      = true;
  bool m_overlaysReady     = false;
  int m_screenCount        = 1;
  TPixel32 m_screen1Color, m_screen2Color,
      m_screen3Color = TPixel32(0, 0, 0, 255);

  void setBlackCapture(bool on);
  bool getBlackCapture() { return m_blackCapture; }
  void setScreen1Color(TPixel32 color);
  void setScreen2Color(TPixel32 color);
  void setScreen3Color(TPixel32 color);
  void setScreen1UseOverlay(bool on);
  void setScreen2UseOverlay(bool on);
  void setScreen3UseOverlay(bool on);
  void showOverlays();
  void hideOverlays();
  bool useOverlays();

signals:
  void blackCaptureSignal(bool);
  void screen1ColorChanged(TPixel32);
  void screen2ColorChanged(TPixel32);
  void screen3ColorChanged(TPixel32);
  void screen1OverlayChanged(bool);
  void screen2OverlayChanged(bool);
  void screen3OverlayChanged(bool);
};
#endif  // STOPMOTIONLIGHT_H