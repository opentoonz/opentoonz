#pragma once

#ifndef AUDIORECORDINGPOPUPPOPUP_H
#define AUDIORECORDINGPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

#include <QFrame>

// forward decl.

class QComboBox;
class QLineEdit;
class QSlider;
class QCheckBox;
class QPushButton;
class QVideoFrame;
class QTimer;
class QIntValidator;
class QRegExpValidator;

namespace DVGui {
class FileField;
class IntField;
}


//=============================================================================
// PencilTestPopup
//-----------------------------------------------------------------------------

class AudioRecordingPopup : public DVGui::Dialog {
  Q_OBJECT

  //QCamera* m_currentCamera;
  QString m_deviceName;

  //QCameraImageCapture* m_cameraImageCapture;
  QString m_cacheSoundPath;


public:
	AudioRecordingPopup();
  ~AudioRecordingPopup();

protected:
  void showEvent(QShowEvent* event);
  void hideEvent(QHideEvent* event);
  void keyPressEvent(QKeyEvent* event);

protected slots:

};

#endif