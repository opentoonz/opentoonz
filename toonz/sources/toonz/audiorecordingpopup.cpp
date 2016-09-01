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

#include <QFile>
#include <QDebug>

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
  m_refreshDevicesButtong = new QPushButton(tr("Refresh"));
  m_deviceListCB = new QComboBox();
  //QCameraImageCapture* m_cameraImageCapture;
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
		  gridLay->addWidget(m_refreshDevicesButtong, 0, 3, Qt::AlignCenter);

		  gridLay->addWidget(m_startRecordingButton, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
		  gridLay->addWidget(m_stopRecordingButton, 1, 3, Qt::AlignRight | Qt::AlignVCenter);

	  }
	  gridLay->setColumnStretch(0, 0);
	  gridLay->setColumnStretch(1, 0);

	  mainLay->addLayout(gridLay);
  }
  m_topLayout->addLayout(mainLay, 0);


  connect(m_startRecordingButton, SIGNAL(clicked()), this, SLOT(startRecording()));
  connect(m_stopRecordingButton, SIGNAL(clicked()), this, SLOT(stopRecording()));
}


void AudioRecordingPopup::startRecording()
{
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

//-----------------------------------------------------------------------------

AudioRecordingPopup::~AudioRecordingPopup() {
  // remove the cache sound, if it exists
  //TFilePath fp(m_cacheSoundPath);
  //if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::showEvent(QShowEvent* event) {

}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::hideEvent(QHideEvent* event) {

}


//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AudioRecordingPopup> openAudioRecordingPopup(MI_AudioRecording);