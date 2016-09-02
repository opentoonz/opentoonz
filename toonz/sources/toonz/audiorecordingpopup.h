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
#include <QLabel>

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
class QLabel;
class AudioLevelsDisplay;
class QAudioProbe;
class QAudioBuffer;
class QMediaPlayer;

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
  QString m_deviceName, m_tempPath;
  DVGui::FileField *m_savePath;
  QPushButton *m_startRecordingButton, *m_stopRecordingButton, *m_refreshDevicesButton,
	  *m_okButton, *m_cancelButton, *m_insertButton, *m_playButton;
  QComboBox *m_deviceListCB;
  //QCameraImageCapture* m_cameraImageCapture;
  QString m_cacheSoundPath;
  QAudioRecorder *audioRecorder;
  QString m_file;
  QLabel *m_duration;
  AudioLevelsDisplay *m_audioLevelsDisplay;
  QAudioProbe *m_probe;
  QMediaPlayer *player;
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
	void updateDuration(qint64 duration);
	void onOkButtonPressed();
	void onCancelButtonPressed();
	void onPlayButtonPressed();
	void processBuffer(const QAudioBuffer& buffer);
	
private:
	QFile outputFile; // class member.
	QAudioInput *audioInput; // class member.
};

class AudioLevelsDisplay : public QWidget {
	Q_OBJECT
public:
	explicit AudioLevelsDisplay(QWidget *parent = 0);

	// Using [0; 1.0] range
	void setLevel(qreal level);

protected:
	void paintEvent(QPaintEvent *event);

private:
	qreal m_level;
};
#endif