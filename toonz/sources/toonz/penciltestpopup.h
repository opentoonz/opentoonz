#pragma once

#ifndef PENCILTESTPOPUP_H
#define PENCILTESTPOPUP_H

#include "toonzqt/dvdialog.h"

#include <QFrame>

// forward decl.
class QCamera;
class QCameraImageCapture;

class QComboBox;
class QLineEdit;
class QSlider;
class QCheckBox;
class QPushButton;
class QVideoFrame;

namespace DVGui {
class FileField;
class IntLineEdit;
class IntField;
}

//=============================================================================
// MyViewFinder
//-----------------------------------------------------------------------------

class MyViewFinder : public QFrame {
  Q_OBJECT

  QImage m_image;
  QImage m_previousImage;
  QCamera* m_camera;
  QRect m_imageRect;

  bool m_showOnionSkin;
  int m_onionOpacity;
  bool m_upsideDown;

public:
  MyViewFinder(QWidget* parent = 0);
  void setImage(const QImage& image) {
    m_image = image;
    update();
  }
  void setCamera(QCamera* camera) { m_camera = camera; }
  void setShowOnionSkin(bool on) { m_showOnionSkin = on; }
  void setOnionOpacity(int value) { m_onionOpacity = value; }
  void setPreviousImage(QImage& prevImage) { m_previousImage = prevImage; }

protected:
  void paintEvent(QPaintEvent* event);
  void resizeEvent(QResizeEvent* event);
protected slots:
  void onUpsideDownChecked(bool on) { m_upsideDown = on; }
};

//=============================================================================
// PencilTestPopup
//-----------------------------------------------------------------------------

class PencilTestPopup : public DVGui::Dialog {
  Q_OBJECT

  QCamera* m_currentCamera;
  QString m_deviceName;
  MyViewFinder* m_cameraViewfinder;
  QCameraImageCapture* m_cameraImageCapture;

  QComboBox *m_cameraListCombo, *m_resolutionCombo, *m_fileTypeCombo,
      *m_colorTypeCombo;
  QLineEdit* m_levelNameEdit;
  QCheckBox *m_upsideDownCB, *m_onionSkinCB;
  QPushButton *m_fileFormatOptionButton, *m_captureWhiteBGButton;
  DVGui::FileField* m_saveInFileFld;
  DVGui::IntLineEdit* m_frameNumberEdit;
  DVGui::IntField *m_thresholdFld, *m_contrastFld, *m_brightnessFld,
      *m_bgReductionFld, *m_onionOpacityFld;

  QImage m_whiteBGImg;

  int m_timerId;
  QString m_cacheImagePath;
  bool m_captureWhiteBGCue;
  bool m_captureCue;

  void processImage(QImage& procImage);
  bool importImage(QImage& image);

public:
  PencilTestPopup();
  ~PencilTestPopup();

protected:
  void timerEvent(QTimerEvent* event);
  void showEvent(QShowEvent* event);
  void hideEvent(QHideEvent* event);

protected slots:
  void refreshCameraList();
  void onCameraListComboActivated(int index);
  void onResolutionComboActivated(const QString&);
  void onFileFormatOptionButtonPressed();
  void onNextName();
  void onColorTypeComboChanged(int index);
  void onImageCaptured(int, const QImage&);
  void onCaptureWhiteBGButtonPressed();
  void onOnionCBToggled(bool);
  void onOnionOpacityFldEdited();

  void onCapture();
};

#endif