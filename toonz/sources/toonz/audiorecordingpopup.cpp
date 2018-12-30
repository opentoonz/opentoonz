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

//
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
  // m_refreshDevicesButton = new QPushButton(tr("Refresh"));
  m_duration           = new QLabel("00:00");
  m_playDuration       = new QLabel("00:00");
  m_deviceListCB       = new QComboBox();
  m_audioLevelsDisplay = new AudioLevelsDisplay(this);
  m_playXSheetCB       = new QCheckBox(tr("Sync with XSheet"), this);
  m_timer              = new QElapsedTimer();
  m_recordedLevels     = QMap<qint64, double>();
  m_oldElapsed         = 0;
  m_probe              = new QAudioProbe;
  m_player             = new QMediaPlayer(this);
  m_console            = FlipConsole::getCurrent();
  m_audioRecorder      = new QAudioRecorder;

  m_recordButton->setMaximumWidth(25);
  m_playButton->setMaximumWidth(25);
  m_pauseRecordingButton->setMaximumWidth(25);
  m_pausePlaybackButton->setMaximumWidth(25);

  QString playDisabled   = QString(":Resources/play_disabled.svg");
  QString pauseDisabled  = QString(":Resources/pause_disabled.svg");
  QString stopDisabled   = QString(":Resources/stop_disabled.svg");
  QString recordDisabled = QString(":Resources/record_disabled.svg");

  m_pauseIcon = createQIcon("pause");
  m_pauseIcon.addFile(pauseDisabled, QSize(), QIcon::Disabled);
  m_playIcon = createQIcon("play");
  m_playIcon.addFile(playDisabled, QSize(), QIcon::Disabled);
  m_recordIcon = createQIcon("record");
  m_recordIcon.addFile(recordDisabled, QSize(), QIcon::Disabled);
  m_stopIcon = createQIcon("stop");
  m_stopIcon.addFile(stopDisabled, QSize(), QIcon::Disabled);
  m_pauseRecordingButton->setIcon(m_pauseIcon);
  m_pauseRecordingButton->setIconSize(QSize(17, 17));
  m_playButton->setIcon(m_playIcon);
  m_playButton->setIconSize(QSize(17, 17));
  m_recordButton->setIcon(m_recordIcon);
  m_recordButton->setIconSize(QSize(17, 17));
  m_pausePlaybackButton->setIcon(m_pauseIcon);
  m_pausePlaybackButton->setIconSize(QSize(17, 17));

  QStringList inputs = m_audioRecorder->audioInputs();
  m_deviceListCB->addItems(inputs);
  QString selectedInput = m_audioRecorder->defaultAudioInput();
  m_deviceListCB->setCurrentText(selectedInput);
  m_audioRecorder->setAudioInput(selectedInput);

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
      recordGridLay->addWidget(m_deviceListCB, 0, 0, 1, 4, Qt::AlignCenter);
      // recordGridLay->addWidget(m_refreshDevicesButton, 0, 3, Qt::AlignLeft);
      recordGridLay->addWidget(new QLabel(tr(" ")), 1, 0, Qt::AlignCenter);
      recordGridLay->addWidget(m_audioLevelsDisplay, 2, 0, 1, 4,
                               Qt::AlignCenter);
      QHBoxLayout *recordLay = new QHBoxLayout(this);
      recordLay->setSpacing(4);
      recordLay->setContentsMargins(0, 0, 0, 0);
      {
        recordLay->addStretch();
        recordLay->addWidget(m_recordButton);
        recordLay->addWidget(m_pauseRecordingButton);
        recordLay->addWidget(m_duration);
        recordLay->addStretch();
      }
      recordGridLay->addLayout(recordLay, 3, 0, 1, 4, Qt::AlignCenter);
      QHBoxLayout *playLay = new QHBoxLayout(this);
      playLay->setSpacing(4);
      playLay->setContentsMargins(0, 0, 0, 0);
      {
        playLay->addStretch();
        playLay->addWidget(m_playButton);
        playLay->addWidget(m_pausePlaybackButton);
        playLay->addWidget(m_playDuration);
        playLay->addStretch();
      }
      recordGridLay->addLayout(playLay, 4, 0, 1, 4, Qt::AlignCenter);
      recordGridLay->addWidget(new QLabel(tr(" ")), 5, 0, Qt::AlignCenter);
      recordGridLay->addWidget(m_saveButton, 6, 0, 1, 4,
                               Qt::AlignCenter | Qt::AlignVCenter);
      recordGridLay->addWidget(m_playXSheetCB, 7, 0, 1, 4,
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

  m_probe->setSource(m_audioRecorder);
  QAudioEncoderSettings audioSettings;
  audioSettings.setCodec("audio/PCM");
  audioSettings.setSampleRate(44100);
  audioSettings.setChannelCount(1);
  audioSettings.setBitRate(16);
  audioSettings.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
  audioSettings.setQuality(QMultimedia::HighQuality);
  m_audioRecorder->setContainerFormat("wav");
  m_audioRecorder->setEncodingSettings(audioSettings);

  connect(m_probe, SIGNAL(audioBufferProbed(QAudioBuffer)), this,
          SLOT(processBuffer(QAudioBuffer)));
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
  connect(m_audioRecorder, SIGNAL(durationChanged(qint64)), this,
          SLOT(updateRecordDuration(qint64)));
  connect(m_console, SIGNAL(playStateChanged(bool)), this,
          SLOT(onPlayStateChanged(bool)));
  connect(m_deviceListCB, SIGNAL(currentTextChanged(const QString)), this,
          SLOT(onInputDeviceChanged()));
  // connect(m_refreshDevicesButton, SIGNAL(clicked()), this,
  // SLOT(onRefreshButtonPressed()));
  // connect(m_audioRecorder, SIGNAL(availableAudioInputsChanged()), this,
  // SLOT(onRefreshButtonPressed()));
}

//-----------------------------------------------------------------------------

AudioRecordingPopup::~AudioRecordingPopup() {}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onRecordButtonPressed() {
  if (m_audioRecorder->state() == QAudioRecorder::StoppedState) {
    if (m_audioRecorder->status() == QMediaRecorder::UnavailableStatus) {
      DVGui::warning(
          tr("The microphone is not available: "
             "\nPlease select a different device or check the microphone."));
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
    m_audioRecorder->setOutputLocation(
        QUrl::fromLocalFile(m_filePath.getQString()));
    if (TSystem::doesExistFileOrLevel(m_filePath)) {
      TSystem::removeFileOrLevel(m_filePath);
    }
    m_recordButton->setIcon(m_stopIcon);
    m_saveButton->setDisabled(true);
    m_playButton->setDisabled(true);
    m_pausePlaybackButton->setDisabled(true);
    m_pauseRecordingButton->setEnabled(true);
    m_recordedLevels.clear();
    m_oldElapsed   = 0;
    m_pausedTime   = 0;
    m_startPause   = 0;
    m_endPause     = 0;
    m_stoppedAtEnd = false;
    m_playDuration->setText("00:00");
    m_timer->restart();
    m_audioRecorder->record();
    // this sometimes sets to one frame off, so + 1.
    m_currentFrame = TApp::instance()->getCurrentFrame()->getFrame() + 1;
    if (m_syncPlayback && !m_isPlaying) {
      m_console->setCurrentFrame(m_currentFrame);
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }

  } else {
    m_audioRecorder->stop();
    m_audioLevelsDisplay->setLevel(0);
    m_recordButton->setIcon(m_recordIcon);
    m_saveButton->setEnabled(true);
    m_playButton->setEnabled(true);
    m_pauseRecordingButton->setDisabled(true);
    m_pauseRecordingButton->setIcon(m_pauseIcon);
    if (m_syncPlayback) {
      if (m_isPlaying) {
        m_console->pressButton(FlipConsole::ePause);
      }
      // put the frame back to before playback
      TApp::instance()->getCurrentFrame()->setCurrentFrame(m_currentFrame);
    }
    m_isPlaying = false;
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::updateRecordDuration(qint64 duration) {
  // this is only called every second or so - sometimes duration ~= 950
  // this gives some padding so it doesn't take two seconds to show one second
  // has passed
  if (duration % 1000 > 850) duration += 150;
  int minutes        = duration / 60000;
  int seconds        = (duration / 1000) % 60;
  QString strMinutes = QString::number(minutes).rightJustified(2, '0');
  QString strSeconds = QString::number(seconds).rightJustified(2, '0');
  m_duration->setText(strMinutes + ":" + strSeconds);
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::updatePlaybackDuration(qint64 duration) {
  int minutes        = duration / 60000;
  int seconds        = (duration / 1000) % 60;
  QString strMinutes = QString::number(minutes).rightJustified(2, '0');
  QString strSeconds = QString::number(seconds).rightJustified(2, '0');
  m_playDuration->setText(strMinutes + ":" + strSeconds);

  // the qmediaplayer probe doesn't work on all platforms, so we fake it by
  // using
  // a map that is made during recording
  if (m_recordedLevels.contains(duration / 20)) {
    m_audioLevelsDisplay->setLevel(m_recordedLevels.value(duration / 20));
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
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPauseRecordingButtonPressed() {
  if (m_audioRecorder->state() == QAudioRecorder::StoppedState) {
    return;
  } else if (m_audioRecorder->state() == QAudioRecorder::PausedState) {
    m_endPause = m_timer->elapsed();
    m_pausedTime += m_endPause - m_startPause;
    m_audioRecorder->record();
    m_pauseRecordingButton->setIcon(m_pauseIcon);
    if (m_syncPlayback && !m_isPlaying && !m_stoppedAtEnd) {
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }
  } else {
    m_audioRecorder->pause();
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
    m_audioLevelsDisplay->setLevel(0);
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

// Refresh isn't working right now, but I'm leaving the code in case a future
// change
// makes it work

// void AudioRecordingPopup::onRefreshButtonPressed() {
//	m_deviceListCB->clear();
//	QStringList inputs = m_audioRecorder->audioInputs();
//	int count = inputs.count();
//	m_deviceListCB->addItems(inputs);
//	QString selectedInput = m_audioRecorder->defaultAudioInput();
//	m_deviceListCB->setCurrentText(selectedInput);
//
//}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onInputDeviceChanged() {
  m_audioRecorder->setAudioInput(m_deviceListCB->currentText());
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onSaveButtonPressed() {
  if (m_audioRecorder->state() != QAudioRecorder::StoppedState) {
    m_audioRecorder->stop();
    m_audioLevelsDisplay->setLevel(0);
  }
  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
    m_audioLevelsDisplay->setLevel(0);
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

void AudioRecordingPopup::processBuffer(const QAudioBuffer &buffer) {
  // keep from processing too many times
  // get 50 signals per second
  if (m_timer->elapsed() < m_oldElapsed + 20) return;
  m_oldElapsed = m_timer->elapsed() - m_pausedTime;
  qint16 value = 0;

  if (!buffer.format().isValid() ||
      buffer.format().byteOrder() != QAudioFormat::LittleEndian)
    return;

  if (buffer.format().codec() != "audio/pcm") return;

  const qint16 *data = buffer.constData<qint16>();
  qreal maxValue     = 0;
  qreal tempValue    = 0;
  for (int i = 0; i < buffer.frameCount(); ++i) {
    tempValue                          = qAbs(qreal(data[i]));
    if (tempValue > maxValue) maxValue = tempValue;
  }
  maxValue /= SHRT_MAX;
  m_audioLevelsDisplay->setLevel(maxValue);
  m_recordedLevels[m_oldElapsed / 20] = maxValue;
}

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
  m_recordedLevels.clear();
  m_duration->setText("00:00");
  m_playDuration->setText("00:00");
  m_audioLevelsDisplay->setLevel(0);
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::hideEvent(QHideEvent *event) {
  if (m_audioRecorder->state() != QAudioRecorder::StoppedState) {
    m_audioRecorder->stop();
  }
  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
  }
  // make sure the file is freed before deleting
  m_player = new QMediaPlayer(this);
  // this should only remove files that haven't been used in the scene
  // make paths checks to only create path names that don't exist yet.
  if (TSystem::doesExistFileOrLevel(TFilePath(m_filePath.getQString()))) {
    TSystem::removeFileOrLevel(TFilePath(m_filePath.getQString()));
  }
}

//-----------------------------------------------------------------------------
// AudioLevelsDisplay Class
//-----------------------------------------------------------------------------

AudioLevelsDisplay::AudioLevelsDisplay(QWidget *parent)
    : QWidget(parent), m_level(0.0) {
  setFixedHeight(20);
  setFixedWidth(300);
}

void AudioLevelsDisplay::setLevel(qreal level) {
  if (m_level != level) {
    m_level = level;
    update();
  }
}

void AudioLevelsDisplay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  QColor color;
  if (m_level < 0.5) {
    color = Qt::green;
  }

  else if (m_level < 0.75) {
    color = QColor(204, 205, 0);  // yellow
  } else if (m_level < 0.95) {
    color = QColor(255, 115, 0);  // orange
  } else
    color = Qt::red;

  qreal widthLevel = m_level * width();
  painter.fillRect(0, 0, widthLevel, height(), color);
  painter.fillRect(widthLevel, 0, width(), height(), Qt::black);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AudioRecordingPopup> openAudioRecordingPopup(
    MI_AudioRecording);