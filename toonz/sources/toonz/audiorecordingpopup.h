#pragma once

#ifndef AUDIORECORDINGPOPUPPOPUP_H
#define AUDIORECORDINGPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

#include <QFrame>
#include <QAudioInput>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QObject>

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
class QAudioRecorder;
class QFile;

namespace DVGui {
class FileField;
class IntField;
}


//=============================================================================
// AudioRecordingPopup
//-----------------------------------------------------------------------------

class AudioRecordingPopup : public DVGui::Dialog {
  Q_OBJECT

  //QCamera* m_currentCamera;
  QString m_deviceName;
  DVGui::FileField *m_savePath;
  QPushButton *m_startRecordingButton, *m_stopRecordingButton, *m_refreshDevicesButtong;
  QComboBox *m_deviceListCB;
  //QCameraImageCapture* m_cameraImageCapture;
  QString m_cacheSoundPath;
  QAudioRecorder *audioRecorder;
  QString m_file;
public:
	AudioRecordingPopup();
  ~AudioRecordingPopup();

protected:
  void showEvent(QShowEvent* event);
  void hideEvent(QHideEvent* event);
  //void keyPressEvent(QKeyEvent* event);

protected slots:


	
private slots:
	void startRecording();
	void stopRecording();
private:
	QFile outputFile; // class member.
	QAudioInput *audioInput; // class member.
};

#endif