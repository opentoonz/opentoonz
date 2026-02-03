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
class QLabel;
class AudioLevelsDisplay;
class FlipConsole;
class QAudioBuffer;
class QMediaPlayer;
class QElapsedTimer;
class AudioWriterWAV;

//=============================================================================
// AudioRecordingPopup
//-----------------------------------------------------------------------------

class AudioRecordingPopup : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_recordButton, *m_refreshDevicesButton, *m_playButton,
      *m_pauseRecordingButton, *m_pausePlaybackButton, *m_saveButton;
  QComboBox *m_deviceListCB;
  QAudioInput *m_audioInput;
  AudioWriterWAV *m_audioWriterWAV;
  QLabel *m_duration, *m_playDuration;
  QCheckBox *m_playXSheetCB;
  int m_currentFrame;
  AudioLevelsDisplay *m_audioLevelsDisplay;
  QMediaPlayer *m_player;
  TFilePath m_filePath;
  FlipConsole *m_console;
  QElapsedTimer *m_timer;
  QMap<qint64, double> m_recordedLevels;
  qint64 m_startPause = 0;
  qint64 m_endPause   = 0;
  qint64 m_pausedTime = 0;
  QIcon m_playIcon;
  QIcon m_pauseIcon;
  QIcon m_recordIcon;
  QIcon m_stopIcon;
  QIcon m_refreshIcon;
  bool m_isPlaying, m_syncPlayback, m_stoppedAtEnd;
  QLabel *m_labelDevice, *m_labelSamplerate, *m_labelSamplefmt;
  QComboBox *m_comboSamplerate, *m_comboSamplefmt;
  bool m_blockAudioSettings;

public:
  AudioRecordingPopup();
  ~AudioRecordingPopup();

protected:
  void showEvent(QShowEvent *event);
  void hideEvent(QHideEvent *event);
  void makePaths();
  void resetEverything();
  void enumerateAudioDevices(const QString &deviceName);
  void reinitAudioInput();

private slots:
  void onRecordButtonPressed();
  void updateRecordDuration(qint64 duration);
  void updatePlaybackDuration(qint64 duration);
  void onPlayButtonPressed();
  void onSaveButtonPressed();
  void onPauseRecordingButtonPressed();
  void onPausePlaybackButtonPressed();
  void onPlayStateChanged(bool playing);
  void onPlayXSheetCBChanged(int status);
  void onMediaStateChanged(QMediaPlayer::State state);
  void onInputDeviceChanged();
  void onRefreshButtonPressed();
  void onAudioSettingChanged();
};

//=============================================================================
// AudioWriterWAV
//-----------------------------------------------------------------------------

class AudioWriterWAV : public QIODevice {
  Q_OBJECT
public:
  AudioWriterWAV(const QAudioFormat &format);
  bool reset(const QAudioFormat &format);

  bool start(const QString &filename, bool useMem);
  bool stop();

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;

  qreal level() const { return m_level; }
  qreal peakLevel() const { return m_peakL; }

private:
  QString m_filename;
  QFile *m_wavFile;
  QByteArray *m_wavBuff;  // if not null then use memory
  QAudioFormat m_format;
  quint64 m_wrRawB;  // Written raw bytes
  qreal m_rbytesms;
  qreal m_level, m_peakL;
  bool m_state;

  void writeWAVHeader(QFile &file);

signals:
  void update(qint64 duration);
};

//=============================================================================
// AudioLevelsDisplay
//-----------------------------------------------------------------------------

class AudioLevelsDisplay : public QWidget {
  Q_OBJECT
public:
  explicit AudioLevelsDisplay(QWidget *parent = 0);

  // Using [0; 1.0] range
  void setLevel(qreal level, qreal peak);

protected:
  void paintEvent(QPaintEvent *event);

private:
  qreal m_level;
  qreal m_peakL;
};
#endif