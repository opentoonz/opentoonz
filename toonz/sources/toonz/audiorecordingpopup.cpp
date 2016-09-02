#include "audiorecordingpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"
#include "fileselection.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/gutil.h"

// Tnzlib includes
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/namebuilder.h"
#include "toonz/levelset.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzfolders.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "iocommand.h"

// TnzCore includes
#include "tsystem.h"
#include "tpixelutils.h"

#include <algorithm>

// Qt includes
#include <QMainWindow>
#include <QAudio>
#include <QAudioInput>
#include <QMediaRecorder>
#include <QAudioProbe>
#include <QAudioRecorder>
#include <QAudioFormat>
#include <QWidget>
#include <QFile>
#include <QDebug>
#include <QAudioBuffer>
#include <QMediaPlayer>

#include <QObject>

#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QRadioButton>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QToolButton>
#include <QGroupBox>
#include <QDateTime>
#include <QMultimedia>
#include <QPainter>
#include <QKeyEvent>
#include <QCommonStyle>
#include <QTimer>
#include <QIntValidator>
#include <QRegExpValidator>

//
//=============================================================================

AudioRecordingPopup::AudioRecordingPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, false, "AudioRecording") {
  setWindowTitle(tr("Audio Recording"));

  // add maximize button to the dialog
  Qt::WindowFlags flags = windowFlags();
  flags |= Qt::WindowMaximizeButtonHint;
  setWindowFlags(flags);

  layout()->setSizeConstraint(QLayout::SetNoConstraint);

  //QString m_deviceName;
  m_savePath = new DVGui::FileField();
  m_startRecordingButton = new QPushButton(tr("Start"));
  m_stopRecordingButton = new QPushButton(tr("Stop"));
  m_playButton = new QPushButton(tr("Play"));
  m_refreshDevicesButton = new QPushButton(tr("Refresh"));
  m_okButton = new QPushButton(tr("OK"));
  m_cancelButton = new QPushButton(tr("Cancel"));
  m_duration = new QLabel("00:00:00");
  m_deviceListCB = new QComboBox();
  m_audioLevelsDisplay = new AudioLevelsDisplay(this);

  QString m_cacheSoundPath = ToonzFolder::getCacheRootFolder().getQString();


  audioRecorder = new QAudioRecorder;

  QStringList inputs = audioRecorder->audioInputs();
  m_deviceListCB->addItems(inputs);
  int i = inputs.count();
  QString selectedInput = audioRecorder->defaultAudioInput();
  std::string selInput = selectedInput.toStdString();

  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(8);
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(3);
  {
	  QGridLayout *gridLay = new QGridLayout();
	  gridLay->setHorizontalSpacing(2);
	  gridLay->setVerticalSpacing(3);
	  {
		  gridLay->addWidget(m_deviceListCB, 0, 0, Qt::AlignCenter);
		  gridLay->addWidget(m_refreshDevicesButton, 0, 3, Qt::AlignCenter);
		  gridLay->addWidget(new QLabel(tr("Duration:")), 1, 0, Qt::AlignCenter);
		  gridLay->addWidget(m_duration, 1, 1, Qt::AlignCenter);
		  gridLay->addWidget(m_audioLevelsDisplay, 2, 0, 1, 4, Qt::AlignLeft);
		  gridLay->addWidget(m_playButton, 3, 0, Qt::AlignRight | Qt::AlignVCenter);
		  gridLay->addWidget(m_startRecordingButton, 3, 2, Qt::AlignRight | Qt::AlignVCenter);
		  gridLay->addWidget(m_stopRecordingButton, 3, 3, Qt::AlignRight | Qt::AlignVCenter);
		  gridLay->addWidget(m_okButton, 4, 0, Qt::AlignRight | Qt::AlignVCenter);
		  gridLay->addWidget(m_cancelButton, 4, 3, Qt::AlignRight | Qt::AlignVCenter);

	  }
	  gridLay->setColumnStretch(0, 0);
	  gridLay->setColumnStretch(1, 0);
	  gridLay->setColumnStretch(2, 0);
	  gridLay->setColumnStretch(3, 0);
	  gridLay->setColumnStretch(4, 0);

	  mainLay->addLayout(gridLay);
  }
  m_topLayout->addLayout(mainLay, 0);
  
  m_probe = new QAudioProbe;
  player = new QMediaPlayer(this);
  m_probe->setSource(audioRecorder);
  QAudioEncoderSettings audioSettings;
  audioSettings.setCodec("audio/PCM");
  audioSettings.setSampleRate(44100);
  audioSettings.setChannelCount(1);
  audioSettings.setBitRate(16);
  audioSettings.setEncodingMode(QMultimedia::ConstantBitRateEncoding);
  audioSettings.setQuality(QMultimedia::HighQuality);
  audioRecorder->setContainerFormat("wav");
  audioRecorder->setEncodingSettings(audioSettings);
  m_file = "C:/bin/audio_qt.wav";
  audioRecorder->setOutputLocation(QUrl::fromLocalFile(m_file));

  connect(m_probe, SIGNAL(audioBufferProbed(QAudioBuffer)),
	  this, SLOT(processBuffer(QAudioBuffer)));
  


  connect(m_startRecordingButton, SIGNAL(clicked()), this, SLOT(startRecording()));
  connect(m_stopRecordingButton, SIGNAL(clicked()), this, SLOT(stopRecording()));
  connect(m_okButton, SIGNAL(clicked()), this, SLOT(onOkButtonPressed()));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(onCancelButtonPressed()));
  connect(m_playButton, SIGNAL(clicked()), this, SLOT(onPlayButtonPressed()));
  connect(audioRecorder, SIGNAL(durationChanged(qint64)), this, SLOT(updateDuration(qint64)));
}


void AudioRecordingPopup::startRecording()
{
	if (player->mediaStatus() != QMediaPlayer::NoMedia) {
		player->stop();
		delete player;
		player = new QMediaPlayer(this);
	}

	if (TSystem::doesExistFileOrLevel(TFilePath(m_file))) {
		TSystem::removeFileOrLevel(TFilePath(m_file));
	}
	audioRecorder->record();

}

void AudioRecordingPopup::stopRecording()
{
	audioRecorder->stop();

	std::vector<TFilePath> filePaths;
	filePaths.push_back(TFilePath(m_file));

	if (filePaths.empty()) return;
	TFilePath path = TApp::instance()->getCurrentScene()->getScene()->getDefaultLevelPath(TXshLevelType::SND_XSHLEVEL);
	std::string strPath = path.getQString().toStdString();
	IoCmd::LoadResourceArguments args;
	args.resourceDatas.assign(filePaths.begin(), filePaths.end());
	IoCmd::loadResources(args);
}


void AudioRecordingPopup::updateDuration(qint64 duration) {
	int minutes = duration / 60000;
	int seconds = (duration / 1000) % 60;
	//int fractions = (duration / 10) % 100;
	QString strMinutes = QString::number(minutes).rightJustified(2, '0');
	QString strSeconds = QString::number(seconds).rightJustified(2, '0');
	//QString strFractions = QString::number(fractions).rightJustified(2, '0');
	m_duration->setText(strMinutes + ":" + strSeconds);
}

void AudioRecordingPopup::onOkButtonPressed() {

}

void AudioRecordingPopup::onCancelButtonPressed() {
	hide();
}

void AudioRecordingPopup::onPlayButtonPressed() {
	player->setMedia(QUrl::fromLocalFile(m_file));
	player->setVolume(50);
	player->play();
}

// audio levels max : SHRT_MAX

//-----------------------------------------------------------------------------

AudioRecordingPopup::~AudioRecordingPopup() {
  // remove the cache sound, if it exists
  //TFilePath fp(m_cacheSoundPath);
  //if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
}


void AudioRecordingPopup::processBuffer(const QAudioBuffer& buffer)
{
	qint16 value = 0;

	if (!buffer.format().isValid() || buffer.format().byteOrder() != QAudioFormat::LittleEndian)
		return;

	if (buffer.format().codec() != "audio/pcm")
		return;

	int channelCount = 1;
	//values.fill(0, channelCount);
	int peak_value = SHRT_MAX;
	if (qFuzzyCompare(peak_value, qreal(0)))
		return;
	//value = getBufferLevel(buffer.constData<qint16>(), buffer.frameCount(), channelCount);
	const qint16 *data = buffer.constData<qint16>();
	qreal maxValue = 0;
	qreal tempValue = 0;
	for (int i = 0; i < buffer.frameCount(); ++i) {
		tempValue = qAbs(qreal(data[i]));
		if (tempValue > maxValue) maxValue = tempValue;
	}
	maxValue /= SHRT_MAX;
	//qreal level = getBufferLevel(buffer);
	m_audioLevelsDisplay->setLevel(maxValue);
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::showEvent(QShowEvent* event) {
	m_audioLevelsDisplay->update();
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::hideEvent(QHideEvent* event) {

}


AudioLevelsDisplay::AudioLevelsDisplay(QWidget *parent)
	: QWidget(parent)
	, m_level(0.0)
{
	setFixedHeight(20);
	setMinimumWidth(500);
	setMaximumWidth(600);
}

void AudioLevelsDisplay::setLevel(qreal level)
{
	if (m_level != level) {
		m_level = level;
		update();
	}
}

void AudioLevelsDisplay::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);

	QPainter painter(this);
	// draw level
	qreal widthLevel = m_level * width();
	painter.fillRect(0, 0, widthLevel, height(), Qt::red);
	// clear the rest of the control
	painter.fillRect(widthLevel, 0, width(), height(), Qt::black);
}


//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AudioRecordingPopup> openAudioRecordingPopup(MI_AudioRecording);