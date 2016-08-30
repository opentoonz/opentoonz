#include "audiorecordingpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"

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

  
}

//-----------------------------------------------------------------------------

AudioRecordingPopup::~AudioRecordingPopup() {
  // remove the cache sound, if it exists
  TFilePath fp(m_cacheSoundPath);
  if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::showEvent(QShowEvent* event) {

}

//-----------------------------------------------------------------------------

void AudioRecordingPopup::hideEvent(QHideEvent* event) {

}


//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AudioRecordingPopup> openAudioRecordingPopup(MI_AudioRecording);