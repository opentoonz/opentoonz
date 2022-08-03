#include "autolipsyncpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "iocommand.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshcell.h"
#include "toonz/sceneproperties.h"
#include "tsound_io.h"
#include "toutputproperties.h"
#include "toonz/tproject.h"
#include "thirdparty.h"

// TnzCore includes
#include "filebrowsermodel.h"
#include "xsheetdragtool.h"
#include "xsheetviewer.h"
#include "historytypes.h"
#include "tsystem.h"
#include "tenv.h"

// Qt includes
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>
#include <QTextStream>
#include <QPainter>
#include <QSignalMapper>
#include <QComboBox>
#include <QProcess>
#include <QTextEdit>
#include <QIcon>
#include <QAudio>
#include <QTimer>
#include <QStackedWidget>
#include <QSizePolicy>

//=============================================================================
/*! \class AutoLipSyncPopup
                \brief The AutoLipSyncPopup class provides a modal dialog to
   apply auto lip sync text data to a image column.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

//============================================================
//	Auto Lip Sync Undo
//============================================================

class AutoLipSyncUndo final : public TUndo {
public:
  AutoLipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
              std::vector<TFrameId> activeFrameIds, QStringList textLines,
              int size, std::vector<TFrameId> previousFrameIds,
              std::vector<TXshLevelP> previousLevels, int startFrame);
  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Apply Auto Lip Sync");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }

private:
  int m_col;
  int m_startFrame;
  TXshSimpleLevel *m_sl;
  TXshLevelP m_cl;
  QStringList m_textLines;
  int m_lastFrame;
  std::vector<TFrameId> m_previousFrameIds;
  std::vector<TXshLevelP> m_previousLevels;
  std::vector<TFrameId> m_activeFrameIds;
};

AutoLipSyncUndo::AutoLipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
                                 std::vector<TFrameId> activeFrameIds,
                                 QStringList textLines, int lastFrame,
                                 std::vector<TFrameId> previousFrameIds,
                                 std::vector<TXshLevelP> previousLevels,
                                 int startFrame)
    : m_col(col)
    , m_sl(sl)
    , m_cl(cl)
    , m_textLines(textLines)
    , m_lastFrame(lastFrame)
    , m_previousFrameIds(previousFrameIds)
    , m_previousLevels(previousLevels)
    , m_activeFrameIds(activeFrameIds)
    , m_startFrame(startFrame) {}

void AutoLipSyncUndo::undo() const {
  int i        = 0;
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();
  while (i < m_previousFrameIds.size()) {
    int currFrame  = i + m_startFrame;
    TXshCell cell  = xsh->getCell(currFrame, m_col);
    cell.m_frameId = m_previousFrameIds.at(i);
    cell.m_level   = m_previousLevels.at(i);
    xsh->setCell(currFrame, m_col, cell);
    i++;
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

void AutoLipSyncUndo::redo() const {
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();
  int i        = 0;
  int currentLine = 0;
  int size        = m_textLines.size();
  while (currentLine < m_textLines.size()) {
    int endAt;
    if (currentLine + 2 >= m_textLines.size()) {
      endAt = m_lastFrame;
    } else
      endAt = m_textLines.at(currentLine + 2).toInt() - 1;
    if (endAt <= 0) break;
    if (endAt <= i) {
      currentLine += 2;
      continue;
    }
    QString shape        = m_textLines.at(currentLine + 1).toLower();
    std::string strShape = shape.toStdString();
    TFrameId currentId   = TFrameId();
    if (shape == "ai") {
      currentId = m_activeFrameIds[0];
    } else if (shape == "e") {
      currentId = m_activeFrameIds[1];
    } else if (shape == "o") {
      currentId = m_activeFrameIds[2];
    } else if (shape == "u") {
      currentId = m_activeFrameIds[3];
    } else if (shape == "fv") {
      currentId = m_activeFrameIds[4];
    } else if (shape == "l") {
      currentId = m_activeFrameIds[5];
    } else if (shape == "mbp") {
      currentId = m_activeFrameIds[6];
    } else if (shape == "wq") {
      currentId = m_activeFrameIds[7];
    } else if (shape == "other" || shape == "etc") {
      currentId = m_activeFrameIds[8];
    } else if (shape == "rest") {
      currentId = m_activeFrameIds[9];
    }

    if (currentId.isEmptyFrame()) {
      currentLine += 2;
      continue;
    }

    while (i < endAt && i < m_lastFrame - m_startFrame) {
      int currFrame = i + m_startFrame;
      TXshCell cell = xsh->getCell(currFrame, m_col);
      if (m_sl)
        cell.m_level = m_sl;
      else
        cell.m_level = m_cl;
      cell.m_frameId = currentId;
      xsh->setCell(currFrame, m_col, cell);
      i++;
    }
    currentLine += 2;
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

AutoLipSyncPopup::AutoLipSyncPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "AutoLipSyncPopup") {
  setWindowTitle(tr("Auto Lip Sync"));
  setFixedWidth(860);
  setFixedHeight(400);

  m_audioFrame      = new QFrame(this);
  m_audioFrame->setContentsMargins(0,0,0,0);

  m_applyButton = new QPushButton(tr("Apply"), this);
  m_aiLabel     = new QLabel(tr("A I Drawing"));
  m_oLabel      = new QLabel(tr("O Drawing"));
  m_eLabel      = new QLabel(tr("E Drawing"));
  m_uLabel      = new QLabel(tr("U Drawing"));
  m_lLabel      = new QLabel(tr("L Drawing"));
  m_wqLabel     = new QLabel(tr("W Q Drawing"));
  m_mbpLabel    = new QLabel(tr("M B P Drawing"));
  m_fvLabel     = new QLabel(tr("F V Drawing"));
  m_restLabel   = new QLabel(tr("Rest Drawing"));
  m_otherLabel  = new QLabel(tr("C D G K N R S Th Y Z"));
  m_startAt     = new DVGui::IntLineEdit(this, 0);
  m_restToEnd   = new QCheckBox(tr("Extend Rest Drawing to End Marker"), this);

  QImage placeHolder(160, 90, QImage::Format_ARGB32);
  placeHolder.fill(Qt::white);

  m_rhubarb = new QProcess(this);
  m_player  = new QMediaPlayer(this);
  m_progressDialog =
      new DVGui::ProgressDialog("Analyzing audio...", "", 1, 100, this);
  m_progressDialog->hide();
  
  m_soundLevels = new QComboBox(this);
  m_playButton  = new QPushButton(tr(""), this);
  m_playIcon    = createQIcon("play");
  m_stopIcon    = createQIcon("stop");
  m_playButton->setIcon(m_playIcon);
  m_soundLevels->setMinimumSize(128, 16);

  m_columnLabel            = new QLabel(tr("Audio Source: "), this);
  QHBoxLayout *soundLayout = new QHBoxLayout();
  soundLayout->addWidget(m_columnLabel);
  soundLayout->addWidget(m_soundLevels);
  soundLayout->addWidget(m_playButton);
  soundLayout->addStretch();

  m_scriptLabel =
      new QLabel(tr("Audio Script (Optional, Improves accuracy): "), this);
  m_scriptLabel->setToolTip(
      tr("A script significantly increases the accuracy of the lip sync."));

  m_audioFile = new DVGui::FileField(this, QString(""));
  m_audioFile->setFileMode(QFileDialog::ExistingFile);
  QStringList audioFilters;
  audioFilters << "wav"
               << "aiff";
  m_audioFile->setFilters(QStringList(audioFilters));
  m_audioFile->setFixedWidth(840);

  m_scriptEdit = new QTextEdit(this);
  m_scriptEdit->setFixedHeight(80);
  m_scriptEdit->setFixedWidth(840);

  QGridLayout *rhubarbLayout = new QGridLayout();
  rhubarbLayout->addLayout(soundLayout, 0, 0, 1, 5);
  rhubarbLayout->addWidget(m_audioFile, 1, 0, 1, 5);
  rhubarbLayout->addWidget(m_scriptLabel, 2, 0, 1, 3);
  rhubarbLayout->addWidget(m_scriptEdit, 3, 0, 1, 5);
  rhubarbLayout->setSpacing(4);
  rhubarbLayout->setMargin(10);
  m_audioFrame->setLayout(rhubarbLayout);

  for (int i = 0; i < 10; i++) {
    m_pixmaps[i] = QPixmap::fromImage(placeHolder);
  }
  for (int i = 0; i < 10; i++) {
    m_imageLabels[i] = new QLabel();
    m_imageLabels[i]->setPixmap(m_pixmaps[i]);
    m_textLabels[i] = new QLabel("temp", this);
  }

  for (int i = 0; i < 20; i++) {
    if (!(i % 2)) {
      m_navButtons[i] = new QPushButton("<");
      m_navButtons[i]->setToolTip(tr("Previous Drawing"));
    } else {
      m_navButtons[i] = new QPushButton(">");
      m_navButtons[i]->setToolTip(tr("Next Drawing"));
    }
  }

  //--- layout
  m_topLayout->setMargin(0);
  m_topLayout->setSpacing(0);

  m_topLayout->addWidget(m_audioFrame);

  {
    QGridLayout *phonemeLay = new QGridLayout();
    phonemeLay->setMargin(10);
    phonemeLay->setVerticalSpacing(10);
    phonemeLay->setHorizontalSpacing(10);
    int i = 0;  // navButtons
    int j = 0;  // imageLabels
    int k = 0;  // textLabels
    phonemeLay->addWidget(m_aiLabel, 0, 0, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_eLabel, 0, 2, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_oLabel, 0, 4, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_uLabel, 0, 6, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_fvLabel, 0, 8, 1, 2, Qt::AlignCenter);

    phonemeLay->addWidget(m_imageLabels[j], 1, 0, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 1, 2, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 1, 4, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 1, 6, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 1, 8, 1, 2, Qt::AlignCenter);
    j++;

    phonemeLay->addWidget(m_textLabels[k], 2, 0, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 2, 2, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 2, 4, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 2, 6, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 2, 8, 1, 2, Qt::AlignCenter);
    k++;

    phonemeLay->addWidget(m_navButtons[i], 3, 0, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 1, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 2, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 3, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 4, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 5, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 6, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 7, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 8, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 3, 9, Qt::AlignCenter);
    i++;

    phonemeLay->addWidget(new QLabel("", this), 4, Qt::AlignCenter);

    phonemeLay->addWidget(m_lLabel, 5, 0, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_mbpLabel, 5, 2, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_wqLabel, 5, 4, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_otherLabel, 5, 6, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_restLabel, 5, 8, 1, 2, Qt::AlignCenter);

    phonemeLay->addWidget(m_imageLabels[j], 6, 0, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 6, 2, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 6, 4, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 6, 6, 1, 2, Qt::AlignCenter);
    j++;
    phonemeLay->addWidget(m_imageLabels[j], 6, 8, 1, 2, Qt::AlignCenter);
    j++;

    phonemeLay->addWidget(m_textLabels[k], 7, 0, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 7, 2, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 7, 4, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 7, 6, 1, 2, Qt::AlignCenter);
    k++;
    phonemeLay->addWidget(m_textLabels[k], 7, 8, 1, 2, Qt::AlignCenter);
    k++;

    phonemeLay->addWidget(m_navButtons[i], 8, 0, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 1, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 2, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 3, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 4, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 5, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 6, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 7, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 8, Qt::AlignCenter);
    i++;
    phonemeLay->addWidget(m_navButtons[i], 8, 9, Qt::AlignCenter);
    i++;
    m_topLayout->addLayout(phonemeLay, 0);
  }

  QHBoxLayout *optionsLay = new QHBoxLayout();
  optionsLay->setMargin(10);
  optionsLay->setSpacing(15);
  QHBoxLayout *insertAtLay = new QHBoxLayout();
  insertAtLay->setMargin(0);
  insertAtLay->setSpacing(4);
  m_insertAtLabel = new QLabel(tr("Insert at Frame: "));
  insertAtLay->addWidget(m_insertAtLabel);
  insertAtLay->addWidget(m_startAt);
  insertAtLay->addStretch();
  optionsLay->addLayout(insertAtLay);
  optionsLay->addWidget(m_restToEnd);
  m_topLayout->addLayout(optionsLay);

  m_topLayout->setAlignment(Qt::AlignHCenter);
  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(0);
  {
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_applyButton);
    m_buttonFrame->setContentsMargins(0, 0, 0, 0);
  }

  //---- signal-slot connections
  QSignalMapper *signalMapper = new QSignalMapper(this);
  bool ret                    = true;

  ret = ret && connect(signalMapper, SIGNAL(mapped(int)), this,
                       SLOT(imageNavClicked(int)));
  for (int i = 0; i < 20; i++) {
    signalMapper->setMapping(m_navButtons[i], i);
    ret = ret && connect(m_navButtons[i], SIGNAL(clicked()), signalMapper,
                         SLOT(map()));
  }

  ret = ret &&
        connect(m_applyButton, SIGNAL(clicked()), this, SLOT(onApplyButton()));
  ret = ret && connect(m_startAt, SIGNAL(editingFinished()), this,
                       SLOT(onStartValueChanged()));
  ret = ret && connect(m_playButton, &QPushButton::pressed, this,
                       &AutoLipSyncPopup::playSound);
  ret = ret && connect(m_soundLevels, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onLevelChanged(int)));
  ret = ret &&
        connect(&m_audioTimeout, SIGNAL(timeout()), this, SLOT(onAudioTimeout()));

  assert(ret);

  m_rhubarbPath = "";
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::showEvent(QShowEvent *) {
  // reset
  m_activeFrameIds.clear();
  m_levelFrameIds.clear();
  m_sl = NULL;
  m_cl = NULL;
  m_startAt->setValue(1);
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
  m_col        = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
  int row      = app->getCurrentFrame()->getFrame();
  m_isEditingLevel = app->getCurrentFrame()->isEditingLevel();
  m_startAt->setValue(row + 1);
  m_startAt->clearFocus();

  if (ThirdParty::checkRhubarb()) m_rhubarbPath = ThirdParty::getRhubarbDir();

  TXshLevelHandle *level = app->getCurrentLevel();
  m_sl                   = level->getSimpleLevel();
  if (!m_sl) {
    TXshCell cell = xsh->getCell(row, m_col);
    if (!cell.isEmpty()) {
      m_cl         = cell.m_level->getChildLevel();
      m_childLevel = cell.m_level;
    }
  }
  if (m_cl)
    DVGui::warning(
        tr("Thumbnails are not available for sub-Xsheets.\nPlease use the "
           "frame numbers for reference."));
  if (!m_sl && !m_cl) {
    DVGui::warning(tr("Unable to apply lip sync data to this column type"));
    QTimer::singleShot(0, this, SLOT(hide()));
    return;
  }
  if (m_rhubarbPath.isEmpty() || m_rhubarbPath.isNull()) {
    DVGui::warning(
        tr("Rhubarb not found, please set the location in Preferences."));
    QTimer::singleShot(0, this, SLOT(hide()));
    return;
  }
  level->getLevel()->getFids(m_levelFrameIds);
  if (m_levelFrameIds.size() > 0) {
    int i = 0;
    // load frame ids from the level
    while (i < m_levelFrameIds.size() && i < 10) {
      m_activeFrameIds.push_back(m_levelFrameIds.at(i));
      i++;
    }
    // fill unused frameIds
    while (i < 10) {
      m_activeFrameIds.push_back(m_levelFrameIds.at(0));
      i++;
    }
  }
  refreshSoundLevels();
  onLevelChanged(-1);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::hideEvent(QHideEvent *) {
  stopAllSound();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::refreshSoundLevels() {
  int currentIndex = 0;
  if (m_soundLevels->count() > 1) {
    currentIndex = m_soundLevels->currentIndex();
  }
  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
  // enumerate all sound columns
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_soundLevels->clear();
  int colCount = xsh->getColumnCount();
  for (int i = 0; i < colCount; i++) {
    TXshColumn *col     = xsh->getColumn(i);
    TXshSoundColumn *sc = col->getSoundColumn();
    if (col->getSoundColumn()) {
      QString colname = "Col" + QString::number(i + 1);
      if (viewer) {
        TStageObject *pegbar = xsh->getStageObject(viewer->getObjectId(i));
        if (pegbar) colname = QString::fromStdString(pegbar->getName());
      }
      m_soundLevels->addItem(colname, QVariant(i));
    }
  }
  m_soundLevels->addItem(tr("Choose File..."), QVariant(-1));
  if (currentIndex < m_soundLevels->count())
    m_soundLevels->setCurrentIndex(currentIndex);
  if (m_soundLevels->currentIndex() < m_soundLevels->count() - 1) {
      m_insertAtLabel->hide();
      m_startAt->hide();
  }
  else {
      m_insertAtLabel->show();
      m_startAt->show();
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::playSound() {
  int count = m_soundLevels->count();
  int level = m_soundLevels->currentData().toInt();
  if (level >= 0) {
    TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshColumn *col = xsh->getColumn(level);
    if (col) {
      TXshSoundColumn *sc = col->getSoundColumn();
      if (sc) {
        if (sc->isPlaying()) {
          sc->stop();
          m_playButton->setIcon(m_playIcon);
          m_audioTimeout.stop();
        } else {
          int r0, r1;
          xsh->getCellRange(level, r0, r1);
          double duration =
              sc->getOverallSoundTrack(r0, r1)->getDuration() * 1000;
          sc->play(r0);
          m_playButton->setIcon(m_stopIcon);
          m_playingSound = sc;
          m_audioTimeout.setSingleShot(true);
          m_audioTimeout.start(duration);
        }
      }
    }
  } else {
    if (m_player->state() == QMediaPlayer::StoppedState) {
      TFilePath tempPath(m_audioFile->getPath());
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      tempPath          = scene->decodeFilePath(tempPath);
      m_player->setMedia(QUrl::fromLocalFile(tempPath.getQString()));
      m_player->setVolume(50);
      m_player->setNotifyInterval(20);
      connect(m_player, SIGNAL(positionChanged(qint64)), this,
              SLOT(updatePlaybackDuration(qint64)));
      connect(m_player, SIGNAL(stateChanged(QMediaPlayer::State)), this,
              SLOT(onMediaStateChanged(QMediaPlayer::State)));

      m_player->play();
    } else {
      m_player->stop();
      m_playButton->setIcon(m_playIcon);
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::stopAllSound() {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int colCount = xsh->getColumnCount();
  for (int i = 0; i < colCount; i++) {
    TXshColumn *col = xsh->getColumn(i);
    TXshSoundColumn *sc = col->getSoundColumn();
    if (sc && sc->isPlaying()) sc->stop();
  }
  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
  }
  m_playButton->setIcon(m_playIcon);
  m_audioTimeout.stop();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onMediaStateChanged(QMediaPlayer::State state) {
  // stopping can happen through the stop button or the file ending
  if (state == QMediaPlayer::StoppedState) {
    m_playButton->setIcon(m_playIcon);
  } else if (m_player->state() == QMediaPlayer::PlayingState) {
    m_playButton->setIcon(m_stopIcon);
  }
}

//-----------------------------------------------------------------------------
bool AutoLipSyncPopup::setAudioFile() {
  m_audioPath  = "";
  m_startFrame = -1;
  m_deleteFile = false;
  int level    = m_soundLevels->currentData().toInt();
  if (level >= 0) {
    saveAudio();
    m_deleteFile = true;
  } else {
    TFilePath tempPath(m_audioFile->getPath());
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    tempPath          = scene->decodeFilePath(tempPath);
    m_audioPath       = tempPath.getQString();
    ;
  }
  if (m_audioPath == "" ||
      !TSystem::doesExistFileOrLevel(TFilePath(m_audioPath))) {
    DVGui::warning(tr("Please choose an audio file and try again."));
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
void AutoLipSyncPopup::saveAudio() {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  if (!TSystem::doesExistFileOrLevel(TFilePath(cacheRoot + "/rhubarb"))) {
    TSystem::mkDir(TFilePath(cacheRoot + "/rhubarb"));
  }
  TFilePath audioPath     = TFilePath(cacheRoot + "/rhubarb/temp.wav");
  std::string tempSString = audioPath.getQString().toStdString();

  int level           = m_soundLevels->currentData().toInt();
  TXsheet *xsh        = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshColumn *col     = xsh->getColumn(level);
  if (col) {
    TXshSoundColumn *sc = col->getSoundColumn();
    if (sc) {
      int r0, r1;
      xsh->getCellRange(level, r0, r1);
      TSoundTrackP st = sc->getOverallSoundTrack(r0);
      TSoundTrackWriter::save(audioPath, st);
      m_audioPath  = audioPath.getQString();
      m_startFrame = r0 + 1;
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::runRhubarb() {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  if (!TSystem::doesExistFileOrLevel(TFilePath(cacheRoot + "/rhubarb"))) {
    TSystem::mkDir(TFilePath(cacheRoot + "/rhubarb"));
  }
  m_datPath                 = TFilePath(cacheRoot + "/rhubarb/temp.dat");
  QString datPath           = m_datPath.getQString();
  std::string tempDatString = datPath.toStdString();

  QStringList args;
  args << "-o" << datPath << "-f"
       << "dat"
       << "--datUsePrestonBlair";
  if (m_scriptEdit->toPlainText() != "") {
    QString script = m_scriptEdit->toPlainText();
    const QString qPath("testQTextStreamEncoding.txt");
    QString scriptPath =
        TFilePath(cacheRoot + "/rhubarb/script.txt").getQString();

    QFile qFile(scriptPath);
    if (qFile.open(QIODevice::WriteOnly)) {
      QTextStream out(&qFile);
      out << script;
      qFile.close();
      args << "-d" << scriptPath;
    }
  }

  int frameRate = std::rint(TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getOutputProperties()
                                ->getFrameRate());
  args << "--datFrameRate" << QString::number(frameRate) << "--machineReadable";

  args << m_audioPath;
  m_progressDialog->show();
  connect(m_rhubarb, &QProcess::readyReadStandardError, this,
          &AutoLipSyncPopup::onOutputReady);

  ThirdParty::runRhubarb(*m_rhubarb, args);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onOutputReady() {
  QString output = m_rhubarb->readAllStandardError().simplified();
  output         = output.replace("\\n", "\n")
               .replace("\\\\", "\\")
               .replace("\\\"", "")
               .replace("\"", "");
  QStringList outputList =
      output.mid(2, (output.size() - 4)).split(", ", QString::SkipEmptyParts);
  if (outputList.size()) {
    QStringList outputType = outputList.at(0).split(": ");
    if (outputType.at(1) == "progress") {
      QStringList outputValue = outputList.at(1).split(": ");
      double progress         = outputValue.at(1).toDouble() * 100.0;
      m_progressDialog->setValue(progress);
    } else if (outputType.at(1) == "failure") {
      QStringList outputReason = outputList.at(1).split(": ");
      DVGui::warning(tr("Rhubarb Processing Error:\n\n") + outputReason.at(1));
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onAudioTimeout() {
  if (m_playingSound) m_playingSound->stop();
  m_playButton->setIcon(m_playIcon);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onLevelChanged(int index) {
  index     = m_soundLevels->currentIndex();
  int count = m_soundLevels->count();
  if (index == count - 1) {
    m_audioFile->show();
  } else {
    m_audioFile->hide();
  }
  int level = m_soundLevels->currentData().toInt();
  if (level >= 0) {
      m_insertAtLabel->hide();
      m_startAt->hide();
  }
  else {
      m_insertAtLabel->show();
      m_startAt->show();
  }
  stopAllSound();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onApplyButton() {
  bool hasAudio = setAudioFile();
  if (!hasAudio) return;
  stopAllSound();

  if (m_rhubarbPath.isEmpty() || m_rhubarbPath.isNull()) {
    DVGui::warning(
        tr("Rhubarb not found, please set the location in Preferences."));
    return;
  }

  runRhubarb();
  int rhubarbTimeout = ThirdParty::getRhubarbTimeout();
  if (rhubarbTimeout > 0)
    rhubarbTimeout *= 1000;
  else
    rhubarbTimeout = -1;
  m_rhubarb->waitForFinished(rhubarbTimeout);
  m_progressDialog->hide();
  QString results = m_rhubarb->readAllStandardError();
  results += m_rhubarb->readAllStandardOutput();
  m_rhubarb->close();
  int exitCode = -1;
  if (m_rhubarb->exitStatus() == QProcess::NormalExit) {
    exitCode = m_rhubarb->exitCode();
    // onOuputReady will handle displaying any error messages from rhubarb
    if (exitCode != 0) return;
  }
  std::string strResults = results.toStdString();
  m_startAt->setValue(std::max(1, m_startFrame));

  if (m_deleteFile && TSystem::doesExistFileOrLevel(TFilePath(m_audioPath)))
    TSystem::deleteFile(TFilePath(m_audioPath));
  m_deleteFile = false;

  m_valid = false;
  m_textLines.clear();
  QString path = m_datPath.getQString();
  if (path.length() == 0) {
    DVGui::warning(tr("Please choose a lip sync data file to continue."));
    return;
  }
  TFilePath tempPath(path);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  tempPath          = scene->decodeFilePath(tempPath);

  if (!TSystem::doesExistFileOrLevel(tempPath)) {
    DVGui::warning(
        tr("Cannot find the file specified. \nPlease choose a valid lip sync "
           "data file to continue."));
    return;
  }
  QFile file(tempPath.getQString());
  if (!file.open(QIODevice::ReadOnly)) {
    DVGui::warning(tr("Unable to open the file: \n") + file.errorString());
    return;
  }

  QTextStream in(&file);

  while (!in.atEnd()) {
    QString line        = in.readLine();
    QStringList entries = line.split(" ");
    if (entries.size() != 2) continue;
    bool ok;
    // make sure the first entry is a number
    int checkInt = entries.at(0).toInt(&ok);
    if (!ok) continue;
    // make sure the second entry isn't a number;
    checkInt = entries.at(1).toInt(&ok);
    if (ok) continue;
    m_textLines << entries;
  }
  if (m_textLines.size() <= 1) {
    DVGui::warning(
        tr("Invalid data file.\n Please choose a valid lip sync data file to "
           "continue."));
    m_valid = false;
    return;
  } else {
    m_valid = true;
  }

  file.close();

  if (!m_valid || (!m_sl && !m_cl)) {
    hide();
    return;
  }

  int i          = 0;
  int startFrame = m_startAt->getValue() - 1;
  TXsheet *xsh   = TApp::instance()->getCurrentScene()->getScene()->getXsheet();

  int lastFrame = m_textLines.at(m_textLines.size() - 2).toInt() + startFrame;

  if (m_restToEnd->isChecked()) {
    int r0, r1;
    TApp::instance()->getCurrentXsheet()->getXsheet()->getCellRange(m_col, r0,
                                                                    r1);
    if (lastFrame < r1 + 1) lastFrame = r1 + 1;
  }
  std::vector<TFrameId> previousFrameIds;
  std::vector<TXshLevelP> previousLevels;
  for (int previousFrame = startFrame; previousFrame < lastFrame;
       previousFrame++) {
    TXshCell cell = xsh->getCell(previousFrame, m_col);
    previousFrameIds.push_back(cell.m_frameId);
    previousLevels.push_back(cell.m_level);
  }

  AutoLipSyncUndo *undo = new AutoLipSyncUndo(
      m_col, m_sl, m_childLevel, m_activeFrameIds, m_textLines,
                      lastFrame, previousFrameIds, previousLevels, startFrame);
  TUndoManager::manager()->add(undo);
  undo->redo();
  hide();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::imageNavClicked(int id) {
  if (!m_sl && !m_cl) return;
  int direction           = id % 2 ? 1 : -1;
  int frameNumber         = id / 2;
  TFrameId currentFrameId = m_activeFrameIds[frameNumber];
  std::vector<TFrameId>::iterator it;
  it =
      std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(), currentFrameId);
  int frameIndex = std::distance(m_levelFrameIds.begin(), it);
  int newIndex;
  if (frameIndex == m_levelFrameIds.size() - 1 && direction == 1)
    newIndex = 0;
  else if (frameIndex == 0 && direction == -1)
    newIndex = m_levelFrameIds.size() - 1;
  else
    newIndex                    = frameIndex + direction;
  m_activeFrameIds[frameNumber] = m_levelFrameIds.at(newIndex);
  TXshCell newCell =
      TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getCell(
          30, m_col);
  newCell.m_frameId = m_levelFrameIds.at(newIndex);
  newCell.m_level   = m_sl;
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::paintEvent(QPaintEvent *) {
  if (m_sl || m_cl) {
    int i = 0;
    while (i < 10) {
      QPixmap pm;
      if (m_sl)
        pm = IconGenerator::instance()->getSizedIcon(
            m_sl, m_activeFrameIds[i], "_lips", TDimension(160, 90));

      if (m_cl) {
        TFrameId currentFrameId = m_activeFrameIds[i];
        std::vector<TFrameId>::iterator it;
        it = std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(),
                       currentFrameId);
        int frameIndex = std::distance(m_levelFrameIds.begin(), it);
        QImage placeHolder(160, 90, QImage::Format_ARGB32);
        placeHolder.fill(Qt::gray);
        QPainter p(&placeHolder);
        p.setPen(Qt::black);
        QRect r = placeHolder.rect();
        p.drawText(r, tr("SubXSheet Frame ") + QString::number(frameIndex + 1),
                   QTextOption(Qt::AlignCenter));
        pm = QPixmap::fromImage(placeHolder);
      }
      if (!pm.isNull()) {
        m_pixmaps[i] = pm;
        m_imageLabels[i]->setPixmap(m_pixmaps[i]);
        m_textLabels[i]->setText(
            tr("Drawing: ") + QString::number(m_activeFrameIds[i].getNumber()));
      }
      i++;
    }
  } else {
    QImage placeHolder(160, 90, QImage::Format_ARGB32);
    placeHolder.fill(Qt::gray);
    for (int i = 0; i < 10; i++) {
      m_pixmaps[i] = QPixmap::fromImage(placeHolder);
      m_imageLabels[i]->setPixmap(m_pixmaps[i]);
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onStartValueChanged() {
  int value = m_startAt->getValue();
  if (value < 1) m_startAt->setValue(1);
}

OpenPopupCommandHandler<AutoLipSyncPopup> openAutoLipSyncPopup(MI_AutoLipSyncPopup);
