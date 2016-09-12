#pragma once

#ifndef AUDIORECORDINGPOPUPPOPUP_H
#define AUDIORECORDINGPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tsystem.h"
#include <QAudioInput>
#include <QObject>
#include <QLabel>
#include <QMediaPlayer>
#include <QIcon>

// forward decl.
class QComboBox;
class QCheckBox;
class QPushButton;
class QAudioRecorder;
class QLabel;
class AudioLevelsDisplay;
class FlipConsole;
class QAudioProbe;
class QAudioBuffer;
class QMediaPlayer;
class QElapsedTimer;

//=============================================================================
// AudioRecordingPopup
//-----------------------------------------------------------------------------

class AudioRecordingPopup : public DVGui::Dialog {
  Q_OBJECT

  QString m_deviceName;
  QPushButton
      *m_recordButton,  // *m_refreshDevicesButton, -refresh not working for now
      *m_playButton,
      *m_pauseRecordingButton, *m_pausePlaybackButton, *m_saveButton;
  QComboBox *m_deviceListCB;
  QAudioRecorder *m_audioRecorder;
  QLabel *m_duration, *m_playDuration;
  QCheckBox *m_playXSheetCB;
  int m_currentFrame;
  AudioLevelsDisplay *m_audioLevelsDisplay;
  QAudioProbe *m_probe;
  QMediaPlayer *m_player;
  TFilePath m_filePath;
  FlipConsole *m_console;
  QElapsedTimer *m_timer;
  QMap<qint64, double> m_recordedLevels;
  qint64 m_oldElapsed;
  qint64 m_startPause = 0;
  qint64 m_endPause   = 0;
  qint64 m_pausedTime = 0;
  QIcon m_playIcon;
  QIcon m_pauseIcon;
  QIcon m_recordIcon;
  QIcon m_stopIcon;
  bool m_isPlaying, m_syncPlayback, m_stoppedAtEnd;

public:
  AudioRecordingPopup();
  ~AudioRecordingPopup();

protected:
  void showEvent(QShowEvent *event);
  void hideEvent(QHideEvent *event);
  void makePaths();
  void resetEverything();

private slots:
  void onRecordButtonPressed();
  void updateRecordDuration(qint64 duration);
  void updatePlaybackDuration(qint64 duration);
  void onPlayButtonPressed();
  void onSaveButtonPressed();
  void onPauseRecordingButtonPressed();
  void onPausePlaybackButtonPressed();
  void processBuffer(const QAudioBuffer &buffer);
  void onPlayStateChanged(bool playing);
  void onPlayXSheetCBChanged(int status);
  void onMediaStateChanged(QMediaPlayer::State state);
  void onInputDeviceChanged();
  // void onRefreshButtonPressed();
};

//=============================================================================
// AudioLevelsDisplay
//-----------------------------------------------------------------------------

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