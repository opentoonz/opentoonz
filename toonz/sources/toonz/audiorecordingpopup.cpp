#include "audiorecordingpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"

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
#include <QGroupBox>
#include <QMultimedia>
#include <QPainter>
#include <QElapsedTimer>

//
//=============================================================================

AudioRecordingPopup::AudioRecordingPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, true, "AudioRecording") {
  setWindowTitle(tr("Audio Recording"));

  layout()->setSizeConstraint(QLayout::SetNoConstraint);

  m_isPlaying    = false;
  m_syncPlayback = true;
  m_currentFrame = 0;
  m_recordButton = new QPushButton(tr("Record"));
  m_playButton   = new QPushButton(tr("Play"));
  m_saveButton   = new QPushButton(tr("Save and Insert"));
  // m_refreshDevicesButton = new QPushButton(tr("Refresh"));
  m_duration           = new QLabel("00:00");
  m_playDuration       = new QLabel("00:00");
  m_deviceListCB       = new QComboBox();
  m_audioLevelsDisplay = new AudioLevelsDisplay(this);
  m_playXSheetCB       = new QCheckBox(tr("XSheet Follows Controls"), this);
  m_timer              = new QElapsedTimer();
  m_recordedLevels     = QMap<qint64, double>();
  m_oldElapsed         = 0;
  m_probe              = new QAudioProbe;
  m_player             = new QMediaPlayer(this);
  m_console            = FlipConsole::getCurrent();
  m_audioRecorder      = new QAudioRecorder;

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
      recordGridLay->addWidget(m_recordButton, 3, 0, 1, 2,
                               Qt::AlignRight | Qt::AlignVCenter);
      recordGridLay->addWidget(m_duration, 3, 2, Qt::AlignLeft);
      recordGridLay->addWidget(m_playButton, 4, 0, 1, 2,
                               Qt::AlignRight | Qt::AlignVCenter);
      recordGridLay->addWidget(m_playDuration, 4, 2, Qt::AlignLeft);
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
    m_recordButton->setText(tr("Stop"));
    m_saveButton->setDisabled(true);
    m_playButton->setDisabled(true);
    m_recordedLevels.clear();
    m_oldElapsed = 0;
    m_playDuration->setText("00:00");
    m_timer->start();
    m_audioRecorder->record();
    // this sometimes sets to one frame off, so + 1.
    m_currentFrame = TApp::instance()->getCurrentFrame()->getFrame() + 1;
    if (m_syncPlayback && !m_isPlaying) {
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }

  } else {
    m_audioRecorder->stop();
    m_audioLevelsDisplay->setLevel(0);
    m_recordButton->setText(tr("Record"));
    m_saveButton->setEnabled(true);
    m_playButton->setEnabled(true);

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
  if (m_recordedLevels.contains(duration / 40)) {
    m_audioLevelsDisplay->setLevel(m_recordedLevels.value(duration / 40));
  }
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::onPlayButtonPressed() {
  if (m_player->state() == QMediaPlayer::StoppedState) {
    m_player->setMedia(QUrl::fromLocalFile(m_filePath.getQString()));
    m_player->setVolume(50);
    m_player->setNotifyInterval(40);
    connect(m_player, SIGNAL(positionChanged(qint64)), this,
            SLOT(updatePlaybackDuration(qint64)));
    connect(m_player, SIGNAL(stateChanged(QMediaPlayer::State)), this,
            SLOT(onMediaStateChanged(QMediaPlayer::State)));
    m_player->play();
    // this sometimes sets to one frame off, so + 1.
    m_currentFrame = TApp::instance()->getCurrentFrame()->getFrame() + 1;
    if (m_syncPlayback && !m_isPlaying) {
      m_console->pressButton(FlipConsole::ePlay);
      m_isPlaying = true;
    }
    m_playButton->setText("Stop");
    m_recordButton->setDisabled(true);
    m_saveButton->setDisabled(true);
  } else {
    m_player->stop();
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
    m_playButton->setText("Play");
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
  // get 25 signals per second
  if (m_timer->elapsed() < m_oldElapsed + 40) return;
  m_oldElapsed = m_timer->elapsed();
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
  m_recordedLevels[m_oldElapsed / 40] = maxValue;
}

void AudioRecordingPopup::onPlayStateChanged(bool playing) {
  // m_isPlaying = playing;
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::showEvent(QShowEvent *event) { resetEverything(); }

//-----------------------------------------------------------------------------

void AudioRecordingPopup::resetEverything() {
  m_saveButton->setDisabled(true);
  m_playButton->setDisabled(true);
  m_recordButton->setEnabled(true);
  m_recordButton->setText(tr("Record"));
  m_playButton->setText(tr("Play"));
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