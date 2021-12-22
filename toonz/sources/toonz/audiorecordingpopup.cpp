#include "audiorecordingpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"
#include "toonzqt/gutil.h"

// Tnzlib includes
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzfolders.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tsystem.h"
#include "tpixelutils.h"
#include "iocommand.h"

// Qt includes
#include <QMainWindow>
#include <QAudio>
#include <QMediaRecorder>
#include <QAudioProbe>
#include <QAudioRecorder>
#include <QAudioFormat>
#include <QWidget>
#include <QAudioBuffer>
#include <QMediaPlayer>
#include <QObject>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMultimedia>
#include <QPainter>
#include <QElapsedTimer>

//=============================================================================

AudioRecordingPopup::AudioRecordingPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, true, "AudioRecording") {
  setWindowTitle(tr("Audio Recording"));
  m_isPlaying            = false;
  m_syncPlayback         = true;
  m_currentFrame         = 0;
  m_recordButton         = new QPushButton(this);
  m_playButton           = new QPushButton(this);
  m_saveButton           = new QPushButton(tr("Save and Insert"));
  m_pauseRecordingButton = new QPushButton(this);
  m_pausePlaybackButton  = new QPushButton(this);
  m_refreshDevicesButton = new QPushButton(this);
  m_duration           = new QLabel("00:00.000");
  m_playDuration       = new QLabel("00:00.000");
  m_deviceListCB       = new QComboBox();
  m_audioLevelsDisplay = new AudioLevelsDisplay(this);
  m_playXSheetCB       = new QCheckBox(tr("Sync with XSheet/Timeline"), this);
  m_timer              = new QElapsedTimer();
  m_recordedLevels     = QMap<qint64, double>();
  m_player             = new QMediaPlayer(this);
  m_console            = FlipConsole::getCurrent();

  m_labelDevice     = new QLabel(tr("Device: "));
  m_labelSamplerate = new QLabel(tr("Sample rate: "));
  m_labelSamplefmt  = new QLabel(tr("Sample format: "));
  m_comboSamplerate = new QComboBox();
  m_comboSamplefmt  = new QComboBox();
  m_comboSamplerate->addItem(tr("8000 Hz"), QVariant::fromValue(8000));
  m_comboSamplerate->addItem(tr("11025 Hz"), QVariant::fromValue(11025));
  m_comboSamplerate->addItem(tr("22050 Hz"), QVariant::fromValue(22050));
  m_comboSamplerate->addItem(tr("44100 Hz"), QVariant::fromValue(44100));
  m_comboSamplerate->addItem(tr("48000 Hz"), QVariant::fromValue(48000));
  m_comboSamplerate->addItem(tr("96000 Hz"), QVariant::fromValue(96000));
  m_comboSamplerate->setCurrentIndex(3);  // 44.1KHz
  m_comboSamplefmt->addItem(tr("Mono 8-Bits"), QVariant::fromValue(9));
  m_comboSamplefmt->addItem(tr("Stereo 8-Bits"), QVariant::fromValue(10));
  m_comboSamplefmt->addItem(tr("Mono 16-Bits"), QVariant::fromValue(17));
  m_comboSamplefmt->addItem(tr("Stereo 16-Bits"), QVariant::fromValue(18));
  m_comboSamplefmt->setCurrentIndex(2);  // Mono 16-Bits

  m_recordButton->setMaximumWidth(32);
  m_playButton->setMaximumWidth(32);
  m_pauseRecordingButton->setMaximumWidth(32);
  m_pausePlaybackButton->setMaximumWidth(32);
  m_refreshDevicesButton->setMaximumWidth(25);

  QString playDisabled   = QString(":Resources/play_disabled.svg");
  QString pauseDisabled  = QString(":Resources/pause_disabled.svg");
  QString stopDisabled   = QString(":Resources/stop_disabled.svg");
  QString recordDisabled = QString(":Resources/record_disabled.svg");
  QString refreshDisabled = QString(":Resources/repeat_icon.svg");

  m_pauseIcon = createQIcon("pause");
  m_pauseIcon.addFile(pauseDisabled, QSize(), QIcon::Disabled);
  m_playIcon = createQIcon("play");
  m_playIcon.addFile(playDisabled, QSize(), QIcon::Disabled);
  m_recordIcon = createQIcon("record");
  m_recordIcon.addFile(recordDisabled, QSize(), QIcon::Disabled);
  m_stopIcon = createQIcon("stop");
  m_stopIcon.addFile(stopDisabled, QSize(), QIcon::Disabled);
  m_refreshIcon = createQIcon("repeat");
  m_refreshIcon.addFile(refreshDisabled, QSize(), QIcon::Disabled);

  m_pauseRecordingButton->setIcon(m_pauseIcon);
  m_pauseRecordingButton->setIconSize(QSize(17, 17));
  m_playButton->setIcon(m_playIcon);
  m_playButton->setIconSize(QSize(17, 17));
  m_recordButton->setIcon(m_recordIcon);
  m_recordButton->setIconSize(QSize(17, 17));
  m_pausePlaybackButton->setIcon(m_pauseIcon);
  m_pausePlaybackButton->setIconSize(QSize(17, 17));
  m_refreshDevicesButton->setIcon(m_refreshIcon);
  m_refreshDevicesButton->setIconSize(QSize(17, 17));

  // Enumerate devices and initialize default device
  enumerateAudioDevices("");
  QAudioDeviceInfo m_audioDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
  QAudioFormat format;
  format.setSampleRate(44100);
  format.setChannelCount(1);
  format.setSampleSize(16);
  format.setSampleType(QAudioFormat::SignedInt);
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setCodec("audio/pcm");
  if (!m_audioDeviceInfo.isFormatSupported(format)) {
    format = m_audioDeviceInfo.nearestFormat(format);
  }
  m_audioInput = new QAudioInput(m_audioDeviceInfo, format);
  m_audioWriterWAV = new AudioWriterWAV(format);

  // Tool tips to provide additional info to the user
  m_deviceListCB->setToolTip(tr("Audio input device to record"));
  m_comboSamplerate->setToolTip(tr("Number of samples per second, 44.1KHz = CD Quality"));
  m_comboSamplefmt->setToolTip(tr("Number of channels and bits per sample, 16-bits recommended"));
  m_playXSheetCB->setToolTip(tr("Play animation from current frame while recording/playback"));
  m_saveButton->setToolTip(tr("Save recording and insert into new column"));
  m_refreshDevicesButton->setToolTip(tr("Refresh list of connected audio input devices"));

  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(8);

  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(3);
  {
    QGridLayout *recordGridLay = new QGridLayout();
    recordGridLay->setHorizontalSpacing(2);
    recordGridLay->setVerticalSpacing(3);
    {
      recordGridLay->addWidget(m_labelDevice, 0, 0, 1, 2, Qt::AlignRight);
      recordGridLay->addWidget(m_deviceListCB, 0, 2, 1, 2, Qt::AlignLeft);

      recordGridLay->addWidget(m_labelSamplerate, 1, 0, 1, 2, Qt::AlignRight);
      recordGridLay->addWidget(m_comboSamplerate, 1, 2, 1, 1, Qt::AlignLeft);
      recordGridLay->addWidget(m_refreshDevicesButton, 1, 3, Qt::AlignRight);

      recordGridLay->addWidget(m_labelSamplefmt, 2, 0, 1, 2, Qt::AlignRight);
      recordGridLay->addWidget(m_comboSamplefmt, 2, 2, 1, 2, Qt::AlignLeft);

      recordGridLay->addWidget(m_audioLevelsDisplay, 3, 0, 1, 4,
                               Qt::AlignCenter);
      recordGridLay->addWidget(m_playXSheetCB, 4, 0, 1, 5, Qt::AlignCenter);
      QHBoxLayout *recordLay = new QHBoxLayout();
      recordLay->setSpacing(4);
      recordLay->setContentsMargins(0, 0, 0, 0);
      {
        recordLay->addStretch();
        recordLay->addWidget(m_recordButton);
        recordLay->addWidget(m_pauseRecordingButton);
        recordLay->addWidget(m_duration);
        recordLay->addStretch();
      }
      recordGridLay->addLayout(recordLay, 5, 0, 1, 4, Qt::AlignCenter);
      QHBoxLayout *playLay = new QHBoxLayout();
      playLay->setSpacing(4);
      playLay->setContentsMargins(0, 0, 0, 0);
      {
        playLay->addStretch();
        playLay->addWidget(m_playButton);
        playLay->addWidget(m_pausePlaybackButton);
        playLay->addWidget(m_playDuration);
        playLay->addStretch();
      }
      recordGridLay->addLayout(playLay, 6, 0, 1, 4, Qt::AlignCenter);
      recordGridLay->addWidget(new QLabel(tr(" ")), 7, 0, Qt::AlignCenter);
      recordGridLay->addWidget(m_saveButton, 8, 0, 1, 4,
                               Qt::AlignCenter | Qt::AlignVCenter);
    }
    recordGridLay->setColumnStretch(0, 0);
    recordGridLay->setColumnStretch(1, 0);
    recordGridLay->setColumnStretch(2, 0);
    recordGridLay->setColumnStretch(3, 0);
    recordGridLay->setColumnStretch(4, 0);
    recordGridLay->setColumnStretch(5, 0);

    mainLay->addLayout(recordGridLay);
  }
  m_topLayout->addLayout(mainLay, 0);

  makePaths();

  m_playXSheetCB->setChecked(true);

  connect(m_playXSheetCB, SIGNAL(stateChanged(int)), this,
          SLOT(onPlayXSheetCBChanged(int)));
  connect(m_saveButton, SIGNAL(clicked()), this, SLOT(onSaveButtonPressed()));
  connect(m_recordButton, SIGNAL(clicked()), this,
          SLOT(onRecordButtonPressed()));
  connect(m_playButton, SIGNAL(clicked()), this, SLOT(onPlayButtonPressed()));
  connect(m_pauseRecordingButton, SIGNAL(clicked()), this,
          SLOT(onPauseRecordingButtonPressed()));
  connect(m_pausePlaybackButton, SIGNAL(clicked()), this,
          SLOT(onPausePlaybackButtonPressed()));
  connect(m_audioWriterWAV, SIGNAL(update(qint64)), this,
          SLOT(updateRecordDuration(qint64)));
  if (m_console) connect(m_console, SIGNAL(playStateChanged(bool)), this,
          SLOT(onPlayStateChanged(bool)));
  connect(m_deviceListCB, SIGNAL(currentTextChanged(const QString)), this,
          SLOT(onInputDeviceChanged()));
  connect(m_refreshDevicesButton, SIGNAL(clicked()), this,
          SLOT(onRefreshButtonPressed()));
  connect(m_comboSamplerate, SIGNAL(currentTextChanged(const QString)), this,
          SLOT(onAudioSettingChanged()));
  connect(m_comboSamplefmt, SIGNAL(currentTextChanged(const QString)), this,
          SLOT(onAudioSettingChanged()));
}

//-----------------------------------------------------------------------------

AudioRecordingPopup::~AudioRecordingPopup() {}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onRecordButtonPressed() {
  if (m_audioInput->state() == QAudio::InterruptedState) {
    DVGui::warning(
        tr("The microphone is not available: "
           "\nPlease select a different device or check the microphone."));
    return;
  } else if (m_audioInput->state() == QAudio::StoppedState) {
    if (!m_console) {
      DVGui::warning(
          tr("Record failed: "
             "\nMake sure there's XSheet or Timeline in the room."));
      return;
    }
    // clear the player in case the file is open there
    // can't record to an opened file
    if (m_player->mediaStatus() != QMediaPlayer::NoMedia) {
      m_player->stop();
      delete m_player;
      m_player = new QMediaPlayer(this);
    }
    // I tried using a temp file in the cache, but copying and inserting
    // (rarely)
    // could cause a crash.  I think OT tried to import the level before the
    // final file was fully copied to the new location
    if (TSystem::doesExistFileOrLevel(m_filePath)) {
      TSystem::removeFileOrLevel(m_filePath);
    }
    // The audio writer support either writing to buffer or directly to disk
    // each method have their own pros and cons
    // For now using false to mimic previous QAudioRecorder behaviour
    m_audioWriterWAV->restart(m_audioInput->format());
    if (!m_audioWriterWAV->start(m_filePath.getQString(), false)) {
      DVGui::warning(
          tr("Failed to save WAV file:\nMake sure you have write permissions "
             "in folder."));
      return;
    }
    m_recordButton->setIcon(m_stopIcon);
    m_saveButton->setDisabled(true);
    m_playButton->setDisabled(true);
    m_pausePlaybackButton->setDisabled(true);
    m_pauseRecordingButton->setEnabled(true);
    m_deviceListCB->setDisabled(true);
    m_refreshDevicesButton->setDisabled(true);
    m_comboSamplerate->setDisabled(true);
    m_comboSamplefmt->setDisabled(true);
    m_recordedLevels.clear();
    m_pausedTime   = 0;
    m_startPause   = 0;
    m_endPause     = 0;
    m_stoppedAtEnd = false;
    m_playDuration->setText("00:00.000");
    m_timer->restart();
    m_audioInput->start(m_audioWriterWAV);
    // this sometimes sets to one frame off, so + 1.
    m_currentFrame = TApp::instance()->getCurrentFrame()->getFrame() + 1;
    if (m_syncPlayback && !m_isPlaying) {
      m_console->setCurrentFrame(m_currentFrame);
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }

  } else {
    m_audioInput->stop();
    bool success = m_audioWriterWAV->stop();
    if (success) {
      m_saveButton->setEnabled(true);
      m_playButton->setEnabled(true);
    }
    m_audioLevelsDisplay->setLevel(-1, -1);
    m_recordButton->setIcon(m_recordIcon);
    m_pauseRecordingButton->setDisabled(true);
    m_pauseRecordingButton->setIcon(m_pauseIcon);
    m_deviceListCB->setEnabled(true);
    m_refreshDevicesButton->setEnabled(true);
    m_comboSamplerate->setEnabled(true);
    m_comboSamplefmt->setEnabled(true);
    if (m_syncPlayback) {
      if (m_isPlaying) {
        m_console->pressButton(FlipConsole::ePause);
      }
      // put the frame back to before playback
      TApp::instance()->getCurrentFrame()->setCurrentFrame(m_currentFrame);
    }
    m_isPlaying = false;
    if (!success) {
      DVGui::warning(tr(
          "Failed to save WAV file:\nMake sure you have write permissions in folder."));
    }
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::updateRecordDuration(qint64 duration) {
  int minutes        = duration / 60000;
  int seconds        = (duration / 1000) % 60;
  int milis          = duration % 1000;
  QString strMinutes = QString::number(minutes).rightJustified(2, '0');
  QString strSeconds = QString::number(seconds).rightJustified(2, '0');
  QString strMilis   = QString::number(milis).rightJustified(3, '0');
  m_duration->setText(strMinutes + ":" + strSeconds + "." + strMilis);

  // Show and record amplitude
  qreal level = m_audioWriterWAV->level();
  qreal peakL = m_audioWriterWAV->peakLevel();
  m_audioLevelsDisplay->setLevel(level, peakL);
  m_recordedLevels[duration / 20] = level;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::updatePlaybackDuration(qint64 duration) {
  int minutes        = duration / 60000;
  int seconds        = (duration / 1000) % 60;
  int milis          = duration % 1000;
  QString strMinutes = QString::number(minutes).rightJustified(2, '0');
  QString strSeconds = QString::number(seconds).rightJustified(2, '0');
  QString strMilis   = QString::number(milis).rightJustified(3, '0');
  m_playDuration->setText(strMinutes + ":" + strSeconds + "." + strMilis);

  // the qmediaplayer probe doesn't work on all platforms, so we fake it by
  // using
  // a map that is made during recording
  if (m_recordedLevels.contains(duration / 20)) {
    m_audioLevelsDisplay->setLevel(m_recordedLevels.value(duration / 20), -1);
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPlayButtonPressed() {
  if (m_player->state() == QMediaPlayer::StoppedState) {
    m_player->setMedia(QUrl::fromLocalFile(m_filePath.getQString()));
    m_player->setVolume(50);
    m_player->setNotifyInterval(20);
    connect(m_player, SIGNAL(positionChanged(qint64)), this,
            SLOT(updatePlaybackDuration(qint64)));
    connect(m_player, SIGNAL(stateChanged(QMediaPlayer::State)), this,
            SLOT(onMediaStateChanged(QMediaPlayer::State)));
    m_playButton->setIcon(m_stopIcon);
    m_recordButton->setDisabled(true);
    m_saveButton->setDisabled(true);
    m_pausePlaybackButton->setEnabled(true);
    m_deviceListCB->setDisabled(true);
    m_refreshDevicesButton->setDisabled(true);
    m_comboSamplerate->setDisabled(true);
    m_comboSamplefmt->setDisabled(true);
    m_stoppedAtEnd = false;
    m_player->play();
    // this sometimes sets to one frame off, so + 1.
    // m_currentFrame = TApp::instance()->getCurrentFrame()->getFrame() + 1;
    if (m_syncPlayback && !m_isPlaying) {
      TApp::instance()->getCurrentFrame()->setCurrentFrame(m_currentFrame);
      m_console->setCurrentFrame(m_currentFrame);
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }
  } else {
    m_player->stop();
    m_playButton->setIcon(m_playIcon);
    m_pausePlaybackButton->setDisabled(true);
    m_pausePlaybackButton->setIcon(m_pauseIcon);
    m_deviceListCB->setEnabled(true);
    m_refreshDevicesButton->setEnabled(true);
    m_comboSamplerate->setEnabled(true);
    m_comboSamplefmt->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPauseRecordingButtonPressed() {
  if (m_audioInput->state() == QAudio::StoppedState) {
    return;
  } else if (m_audioInput->state() == QAudio::SuspendedState) {
    m_endPause = m_timer->elapsed();
    m_pausedTime += m_endPause - m_startPause;
    m_audioInput->resume();
    m_pauseRecordingButton->setIcon(m_pauseIcon);
    if (m_syncPlayback && !m_isPlaying && !m_stoppedAtEnd) {
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }
  } else {
    m_audioInput->suspend();
    m_pauseRecordingButton->setIcon(m_recordIcon);
    m_startPause = m_timer->elapsed();
    if (m_syncPlayback && m_isPlaying) {
      m_isPlaying = false;
      m_console->pressButton(FlipConsole::ePause);
    }
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPausePlaybackButtonPressed() {
  if (m_player->state() == QMediaPlayer::StoppedState) {
    return;
  } else if (m_player->state() == QMediaPlayer::PausedState) {
    m_player->play();
    m_pausePlaybackButton->setIcon(m_pauseIcon);
    if (m_syncPlayback && !m_isPlaying && !m_stoppedAtEnd) {
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }
  } else {
    m_player->pause();
    m_pausePlaybackButton->setIcon(m_playIcon);
    if (m_syncPlayback && m_isPlaying) {
      m_isPlaying = false;
      m_console->pressButton(FlipConsole::ePause);
    }
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onMediaStateChanged(QMediaPlayer::State state) {
  // stopping can happen through the stop button or the file ending
  if (state == QMediaPlayer::StoppedState) {
    m_audioLevelsDisplay->setLevel(-1, -1);
    if (m_syncPlayback) {
      if (m_isPlaying) {
        m_console->pressButton(FlipConsole::ePause);
      }
      // put the frame back to before playback
      TApp::instance()->getCurrentFrame()->setCurrentFrame(m_currentFrame);
    }
    m_playButton->setIcon(m_playIcon);
    m_pausePlaybackButton->setIcon(m_pauseIcon);
    m_pausePlaybackButton->setDisabled(true);
    m_recordButton->setEnabled(true);
    m_deviceListCB->setEnabled(true);
    m_refreshDevicesButton->setEnabled(true);
    m_comboSamplerate->setEnabled(true);
    m_comboSamplefmt->setEnabled(true);
    m_saveButton->setEnabled(true);
    m_isPlaying = false;
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPlayXSheetCBChanged(int status) {
  if (status == 0) {
    m_syncPlayback = false;
  } else
    m_syncPlayback = true;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onRefreshButtonPressed() {
  QAudioDeviceInfo m_audioDeviceInfo =
      m_deviceListCB->itemData(m_deviceListCB->currentIndex())
          .value<QAudioDeviceInfo>();

  enumerateAudioDevices(m_audioDeviceInfo.deviceName());
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onInputDeviceChanged() {
  reinitAudioInput();
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onAudioSettingChanged() {
  reinitAudioInput();
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onSaveButtonPressed() {
  if (m_audioInput->state() != QAudio::StoppedState) {
    m_audioInput->stop();
    m_audioLevelsDisplay->setLevel(-1, -1);
  }
  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
    m_audioLevelsDisplay->setLevel(-1, -1);
  }
  if (!TSystem::doesExistFileOrLevel(m_filePath)) return;

  std::vector<TFilePath> filePaths;
  filePaths.push_back(m_filePath);

  if (filePaths.empty()) return;
  if (m_syncPlayback) {
    TApp::instance()->getCurrentFrame()->setCurrentFrame(m_currentFrame);
    m_console->setCurrentFrame(m_currentFrame);
  }
  IoCmd::LoadResourceArguments args;
  args.resourceDatas.assign(filePaths.begin(), filePaths.end());
  IoCmd::loadResources(args);

  makePaths();
  resetEverything();
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::makePaths() {
  TFilePath savePath =
      TApp::instance()->getCurrentScene()->getScene()->getDefaultLevelPath(
          TXshLevelType::SND_XSHLEVEL);
  savePath =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(savePath);
  savePath = savePath.getParentDir();

  std::string strPath = savePath.getQString().toStdString();
  int number          = 1;
  TFilePath finalPath =
      savePath + TFilePath("recordedAudio" + QString::number(number) + ".wav");
  while (TSystem::doesExistFileOrLevel(finalPath)) {
    number++;
    finalPath = savePath +
                TFilePath("recordedAudio" + QString::number(number) + ".wav");
  }
  m_filePath = finalPath;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPlayStateChanged(bool playing) {
  // m_isPlaying = playing;
  if (!playing && m_isPlaying) m_stoppedAtEnd = true;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::showEvent(QShowEvent *event) { resetEverything(); }

//-----------------------------------------------------------------------------

void AudioRecordingPopup::resetEverything() {
  m_saveButton->setDisabled(true);
  m_playButton->setDisabled(true);
  m_recordButton->setEnabled(true);
  m_recordButton->setIcon(m_recordIcon);
  m_playButton->setIcon(m_playIcon);
  m_pausePlaybackButton->setIcon(m_pauseIcon);
  m_pauseRecordingButton->setIcon(m_pauseIcon);
  m_pauseRecordingButton->setDisabled(true);
  m_pausePlaybackButton->setDisabled(true);
  m_deviceListCB->setEnabled(true);
  m_refreshDevicesButton->setEnabled(true);
  m_comboSamplerate->setEnabled(true);
  m_comboSamplefmt->setEnabled(true);
  m_recordedLevels.clear();
  m_duration->setText("00:00.000");
  m_playDuration->setText("00:00.000");
  m_audioLevelsDisplay->setLevel(-1, -1);
  if (!m_console) {
    m_console = FlipConsole::getCurrent();
    if (m_console)
      connect(m_console, SIGNAL(playStateChanged(bool)), this,
              SLOT(onPlayStateChanged(bool)));
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::hideEvent(QHideEvent *event) {
  if (m_audioInput->state() != QAudio::StoppedState) {
    m_audioInput->stop();
    m_audioWriterWAV->stop();
  }
  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
  }
  // make sure the file is freed before deleting
  delete m_player;
  m_player = new QMediaPlayer(this);
  // this should only remove files that haven't been used in the scene
  // make paths checks to only create path names that don't exist yet.
  if (TSystem::doesExistFileOrLevel(TFilePath(m_filePath.getQString()))) {
    TSystem::removeFileOrLevel(TFilePath(m_filePath.getQString()));
  }
  // Free up memory used in recording
  m_recordedLevels.clear();
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::enumerateAudioDevices(const QString &selectedDeviceName) {
  const QAudioDeviceInfo &defaultDeviceInfo =
      QAudioDeviceInfo::defaultInputDevice();

  m_blockAudioSettings = true;
  m_deviceListCB->clear();
  m_deviceListCB->addItem(defaultDeviceInfo.deviceName(),
                          QVariant::fromValue(defaultDeviceInfo));

  for (auto &deviceInfo :
       QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
    if (deviceInfo != defaultDeviceInfo &&
        m_deviceListCB->findText(deviceInfo.deviceName()) == -1) {
      m_deviceListCB->addItem(deviceInfo.deviceName(),
                              QVariant::fromValue(deviceInfo));
    }
  }

  int deviceIndex = m_deviceListCB->findText(selectedDeviceName);
  if (deviceIndex != -1) m_deviceListCB->setCurrentIndex(deviceIndex);
  m_blockAudioSettings = false;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::reinitAudioInput() {
  if (m_blockAudioSettings) return;

  QAudioDeviceInfo m_audioDeviceInfo =
      m_deviceListCB->itemData(m_deviceListCB->currentIndex())
          .value<QAudioDeviceInfo>();
  int samplerate =
      m_comboSamplerate->itemData(m_comboSamplerate->currentIndex())
          .value<int>();
  int sampletype =
      m_comboSamplefmt->itemData(m_comboSamplefmt->currentIndex()).value<int>();
  int bitdepth = sampletype & 56;
  int channels = sampletype & 7;

  QAudioFormat format;
  format.setSampleRate(samplerate);
  format.setChannelCount(channels);
  format.setSampleSize(bitdepth);
  format.setSampleType(bitdepth == 8 ? QAudioFormat::UnSignedInt
                                     : QAudioFormat::SignedInt);
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setCodec("audio/pcm");
  if (!m_audioDeviceInfo.isFormatSupported(format)) {
    DVGui::warning(tr(
        "Audio format unsupported:\nNearest format will be internally used."));
    format = m_audioDeviceInfo.nearestFormat(format);
  }

  // Recreate input
  delete m_audioInput;
  m_audioInput = new QAudioInput(m_audioDeviceInfo, format);
  m_audioWriterWAV->restart(format);
}

//-----------------------------------------------------------------------------
// AudioWriterWAV Class
//-----------------------------------------------------------------------------
// IODevice to write standard WAV files, performs peak level calc
//
// 8-bits audio must be unsigned
// 16-bits audio must be signed
// 32-bits isn't supported

AudioWriterWAV::AudioWriterWAV(const QAudioFormat &format)
    : m_level(0.0)
    , m_peakL(0.0)
    , m_maxAmp(0.0)
    , m_wrRawB(0)
    , m_wavFile(NULL)
    , m_wavBuff(NULL) {
  restart(format);
}

bool AudioWriterWAV::restart(const QAudioFormat &format) {
  m_format = format;
  if (m_format.sampleSize() == 8) {
    m_rbytesms = 1000.0 / (m_format.sampleRate() * m_format.channelCount());
    m_maxAmp   = 127.0;
  } else if (m_format.sampleSize() == 16) {
    m_rbytesms = 500.0 / (m_format.sampleRate() * m_format.channelCount());
    m_maxAmp   = 32767.0;
  } else {
    // 32-bits isn't supported
    m_rbytesms = 250.0 / (m_format.sampleRate() * m_format.channelCount());
    m_maxAmp   = 1.0;
  }
  m_wrRawB = 0;
  m_peakL  = 0.0;
  if (m_wavBuff) m_wavBuff->clear();
  return this->reset();
}

// Just a tiny define to avoid a magic number
// this is the size of a WAV header with PCM format
#define AWWAV_HEADER_SIZE 44

bool AudioWriterWAV::start(const QString &filename, bool useMem) {
  open(QIODevice::WriteOnly);
  m_filename = filename;

  if (useMem) {
    m_wavBuff = new QByteArray();
  } else {
    m_wavFile = new QFile(m_filename);
    if (!m_wavFile->open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;
    m_wavFile->seek(AWWAV_HEADER_SIZE); // skip header
  }

  m_wrRawB = 0;
  m_peakL  = 0.0;
  return true;
}

bool AudioWriterWAV::stop() {
  close();

  if (m_wavBuff) {
    // Using memory
    QFile file;
    file.setFileName(m_filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    writeWAVHeader(file);
    file.write(m_wavBuff->constData(), m_wavBuff->size());
    delete m_wavBuff;
    m_wavBuff = NULL;
  } else {
    // Using disk directly
    writeWAVHeader(*m_wavFile);
    m_wavFile->close();
    delete m_wavFile;
    m_wavFile = NULL;
  }

  m_wrRawB = 0;
  m_peakL  = 0.0;
  return true;
}

void AudioWriterWAV::writeWAVHeader(QFile &file) {
  quint16 channels   = m_format.channelCount();
  quint32 samplerate = m_format.sampleRate();
  quint16 bitrate    = m_format.sampleSize();

  qint64 pos         = file.pos();
  file.seek(0);

  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  out.writeRawData("RIFF", 4);
  out << (quint32)(m_wrRawB + AWWAV_HEADER_SIZE);
  out.writeRawData("WAVEfmt ", 8);
  out << (quint32)16 << (quint16)1; // magic numbers!
  out << channels << samplerate;
  out << quint32(samplerate * channels * bitrate / 8);
  out << quint16(channels * bitrate / 8);
  out << bitrate;
  out.writeRawData("data", 4);
  out << (quint32)m_wrRawB;
}

qint64 AudioWriterWAV::readData(char *data, qint64 maxlen) {
  Q_UNUSED(data)
  Q_UNUSED(maxlen)
  return 0;
}

qint64 AudioWriterWAV::writeData(const char *data, qint64 len) {
  int tmp, peak = 0.0;

  // Measure peak
  if (m_format.sampleSize() == 8) {
    const quint8 *sdata = (const quint8 *)data;
    int slen            = len;
    for (int i = 0; i < slen; ++i) {
      tmp = qAbs<int>(sdata[i] - 128);
      if (tmp > peak) peak = tmp;
    }
  } else if (m_format.sampleSize() == 16) {
    const qint16 *sdata = (const qint16 *)data;
    int slen            = len / 2;
    for (int i = 0; i < slen; ++i) {
      tmp = qAbs<int>(sdata[i]);
      if (tmp > peak) peak = tmp;
    }
  } else {
    // 32-bits isn't supported
    peak = -1;
  }
  m_level = qreal(peak) / m_maxAmp;
  if (m_level > m_peakL) m_peakL = m_level;

  // Write to memory or disk
  if (m_wavBuff) {
    m_wavBuff->append(data, len);
  } else {
    m_wavFile->write(data, len);
  }
  m_wrRawB += len;

  // Emit an update
  emit update(qreal(m_wrRawB) * m_rbytesms);
  return len;
}

//-----------------------------------------------------------------------------
// AudioLevelsDisplay Class
//-----------------------------------------------------------------------------

AudioLevelsDisplay::AudioLevelsDisplay(QWidget *parent)
    : QWidget(parent), m_level(0.0), m_peakL(0.0) {
  setFixedHeight(20);
  setFixedWidth(300);
}

void AudioLevelsDisplay::setLevel(qreal level, qreal peakLevel) {
  if (m_level != level || m_peakL != peakLevel) {
    m_level = level;
    m_peakL = peakLevel;
    update();
  }
}

void AudioLevelsDisplay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  QColor color;

  if (m_level < 0.0) {
    return; // draw nothing...
  } else if (m_level < 0.5) {
    color = Qt::green;
  } else if (m_level < 0.75) {
    color = QColor(204, 205, 0);  // yellow
  } else if (m_level < 0.95) {
    color = QColor(255, 115, 0);  // orange
  } else
    color = Qt::red;

  int widthLevel = m_level * width();
  int widthPeakL = m_peakL * width();
  painter.fillRect(widthLevel, 0, width(), height(), Qt::black);
  if (widthPeakL >= 0) {
    if (m_peakL >= 0.995) {
      painter.fillRect(width() - 4, 0, 4, height(), Qt::red);
    } else {
      painter.setPen(QColor(64, 64, 64));  // very dark gray
      painter.drawLine(widthPeakL, 0, widthPeakL, height());
    }
  }
  painter.fillRect(0, 0, widthLevel, height(), color);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AudioRecordingPopup> openAudioRecordingPopup(
    MI_AudioRecording);
