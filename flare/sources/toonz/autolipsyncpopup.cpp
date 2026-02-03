

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
#include <QComboBox>
#include <QProcess>
#include <QTextEdit>
#include <QIcon>
#include <QAudio>
#include <QTimer>
#include <QStackedWidget>
#include <QSizePolicy>
#include <QFileInfo>
#include <QDir>

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
  TXshLevelP m_sl;
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
    : Dialog(TApp::instance()->getMainWindow(), true, true, "AutoLipSyncPopup")
    , m_audioFile(nullptr)
    , m_childLevel(nullptr)    // Initialize as nullptr
    , m_playingSound(nullptr)  // Initialize as nullptr
    , m_deleteFile(false)      // Initialize as false
    , m_processRunning(false)  // Initialize as false
    , m_userCancelled(false)   // Initialize as false
{
  setWindowTitle(tr("Auto Lip Sync"));
  setFixedWidth(860);
  setFixedHeight(400);

  m_audioFrame = new QFrame(this);
  m_audioFrame->setContentsMargins(0, 0, 0, 0);

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

  // Initialize progress dialog with cancel button
  m_progressDialog = new DVGui::ProgressDialog(tr("Analyzing audio..."),
                                               tr("Cancel"), 0, 100, this);
  m_progressDialog->hide();
  m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint,
                                  false);  // Remove X button

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
  audioFilters << "wav" << "aiff";
  m_audioFile->setFilters(QStringList(audioFilters));
  m_audioFile->setFixedWidth(840);

  m_audioFile->hide();

  m_scriptEdit = new QTextEdit(this);
  m_scriptEdit->setFixedHeight(80);
  m_scriptEdit->setFixedWidth(840);

  QGridLayout *rhubarbLayout = new QGridLayout();
  rhubarbLayout->addLayout(soundLayout, 0, 0, 1, 5);
  rhubarbLayout->addWidget(m_audioFile, 1, 0, 1, 5);
  rhubarbLayout->addWidget(m_scriptLabel, 2, 0, 1, 3);
  rhubarbLayout->addWidget(m_scriptEdit, 3, 0, 1, 5);
  rhubarbLayout->setSpacing(4);
  rhubarbLayout->setContentsMargins(10, 10, 10, 10);
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

  //--- Layout
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);

  m_topLayout->addWidget(m_audioFrame);

  {
    QGridLayout *phonemeLay = new QGridLayout();
    phonemeLay->setContentsMargins(10, 10, 10, 10);
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
  optionsLay->setContentsMargins(10, 10, 10, 10);
  optionsLay->setSpacing(15);
  QHBoxLayout *insertAtLay = new QHBoxLayout();
  insertAtLay->setContentsMargins(0, 0, 0, 0);
  insertAtLay->setSpacing(4);
  m_insertAtLabel = new QLabel(tr("Insert at Frame: "));
  insertAtLay->addWidget(m_insertAtLabel);
  insertAtLay->addWidget(m_startAt);
  insertAtLay->addStretch();
  optionsLay->addLayout(insertAtLay);
  optionsLay->addWidget(m_restToEnd);
  m_topLayout->addLayout(optionsLay);

  m_topLayout->setAlignment(Qt::AlignHCenter);
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(0);
  {
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_applyButton);
    m_buttonFrame->setContentsMargins(0, 0, 0, 0);
  }

  //---- Signal-slot connections
  for (int i = 0; i < 20; i++) {
    connect(m_navButtons[i], &QPushButton::clicked, this,
            [this, i]() { imageNavClicked(i); });
  }

  connect(m_applyButton, &QPushButton::clicked, this,
          &AutoLipSyncPopup::onApplyButton);
  connect(m_startAt, &DVGui::IntLineEdit::editingFinished, this,
          &AutoLipSyncPopup::onStartValueChanged);
  connect(m_playButton, &QPushButton::pressed, this,
          &AutoLipSyncPopup::playSound);
  connect(m_soundLevels, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AutoLipSyncPopup::onLevelChanged);
  connect(&m_audioTimeout, &QTimer::timeout, this,
          &AutoLipSyncPopup::onAudioTimeout);

  // Connect progress dialog cancel signal
  connect(m_progressDialog, &DVGui::ProgressDialog::canceled, this,
          &AutoLipSyncPopup::onAnalysisCancelled);

  m_rhubarbPath = "";
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::updateThumbnail(int index) {
  if (index < 0 || index >= 10 || (!m_sl && !m_cl)) return;

  TFrameId frameId = m_activeFrameIds[index];
  const qreal dpr  = qApp->devicePixelRatio();

  QPixmap pm;

  if (m_sl) {
    pm = IconGenerator::instance()->getSizedIcon(m_sl, frameId, "");
  } else if (m_cl) {
    // Placeholder HiDPI for sub-xsheet
    QImage img(int(160 * dpr), int(90 * dpr), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::gray);

    QPainter p(&img);
    p.setPen(Qt::black);
    auto it =
        std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(), frameId);
    int frameIndex = (it != m_levelFrameIds.end())
                         ? std::distance(m_levelFrameIds.begin(), it) + 1
                         : index + 1;
    p.drawText(img.rect(), tr("SubXSheet Frame ") + QString::number(frameIndex),
               QTextOption(Qt::AlignCenter));
    p.end();

    pm = QPixmap::fromImage(img);
  }

  if (!pm.isNull()) {
    // Scale the pixmap in memory without affecting the cache
    QPixmap scaled = pm.scaled(int(160 * dpr), int(90 * dpr),
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    m_pixmaps[index] = scaled;
    m_imageLabels[index]->setPixmap(scaled);

    m_textLabels[index]->setText(
        m_sl ? tr("Drawing: ") + QString::number(frameId.getNumber())
             : tr("Frame ") + QString::number(index + 1));
  }
}
//-----------------------------------------------------------------------------

void AutoLipSyncPopup::generateThumbnails() {
  if (!m_sl && !m_cl) return;

  // Generate all thumbnails
  for (int i = 0; i < 10 && i < static_cast<int>(m_activeFrameIds.size());
       i++) {
    updateThumbnail(i);
  }
}
//-----------------------------------------------------------------------------
void AutoLipSyncPopup::onIconGenerated() {
  // Update thumbnails if popup is visible
  if (!isVisible()) return;

  // Force update of all thumbnails
  generateThumbnails();
  update();
}
//-----------------------------------------------------------------------------

void AutoLipSyncPopup::showEvent(QShowEvent *event) {
  // Reset state
  m_activeFrameIds.clear();
  m_levelFrameIds.clear();
  m_sl         = nullptr;
  m_cl         = nullptr;
  m_childLevel = nullptr;
  m_startAt->setValue(1);

  // Reset process flags
  m_processRunning = false;
  m_userCancelled  = false;

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
  m_col        = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
  int row      = app->getCurrentFrame()->getFrame();
  m_isEditingLevel = app->getCurrentFrame()->isEditingLevel();
  m_startAt->setValue(row + 1);
  m_startAt->clearFocus();

  if (ThirdParty::checkRhubarb()) {
    m_rhubarbPath = ThirdParty::getRhubarbDir();
  }

  TXshLevelHandle *level = app->getCurrentLevel();
  m_sl                   = level->getSimpleLevel();
  if (!m_sl) {
    TXshCell cell = xsh->getCell(row, m_col);
    if (!cell.isEmpty() && cell.m_level) {
      m_cl         = cell.m_level->getChildLevel();
      m_childLevel = cell.m_level;
    }
  }

  if (m_cl) {
    DVGui::warning(
        tr("Thumbnails are not available for sub-Xsheets.\nPlease use the "
           "frame numbers for reference."));
  }

  if (!m_sl && !m_cl) {
    DVGui::warning(tr("Unable to apply lip sync data to this column type"));
    QTimer::singleShot(0, this, &AutoLipSyncPopup::hide);
    return;
  }

  if (m_rhubarbPath.isEmpty()) {
    DVGui::warning(
        tr("Rhubarb not found, please set the location in Preferences."));
    QTimer::singleShot(0, this, &AutoLipSyncPopup::hide);
    return;
  }

  // Get frames from the level
  if (m_sl) {
    m_sl->getFids(m_levelFrameIds);
  } else if (m_cl && m_childLevel) {
    // For child levels, we need a different approach
    m_levelFrameIds.clear();
    // Add placeholder frames
    for (int i = 1; i <= 10; i++) {
      m_levelFrameIds.push_back(TFrameId(i));
    }
  }

  if (!m_levelFrameIds.empty()) {
    // Fill with the first 10 frames or repeat the first one
    for (int i = 0; i < 10; i++) {
      if (i < static_cast<int>(m_levelFrameIds.size())) {
        m_activeFrameIds.push_back(m_levelFrameIds[i]);
      } else {
        m_activeFrameIds.push_back(m_levelFrameIds[0]);
      }
    }
  } else {
    // Clear active frame IDs if no level frames are available
    m_activeFrameIds.clear();
    m_activeFrameIds.resize(10, TFrameId());
  }

  refreshSoundLevels();
  // Call onLevelChanged with the current index to properly set up the UI
  onLevelChanged(m_soundLevels->currentIndex());

  // Connect to IconGenerator for asynchronous thumbnail updates
  connect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
          &AutoLipSyncPopup::onIconGenerated, Qt::QueuedConnection);

  // Generate initial thumbnails
  generateThumbnails();

  // Call base class implementation
  Dialog::showEvent(event);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::hideEvent(QHideEvent *event) {
  stopAllSound();

  // Delete temporary file only if the process has finished or was cancelled
  if (m_deleteFile && !m_audioPath.isEmpty()) {
    TFilePath fp(m_audioPath);
    if (TSystem::doesExistFileOrLevel(fp)) {
      TSystem::deleteFile(fp);
    }
  }
  m_deleteFile = false;
  m_audioPath.clear();

  // Disconnect from IconGenerator
  disconnect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
             &AutoLipSyncPopup::onIconGenerated);

  // Call base class implementation
  Dialog::hideEvent(event);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::refreshSoundLevels() {
  int currentIndex = 0;
  if (m_soundLevels->count() > 1) {
    currentIndex = m_soundLevels->currentIndex();
  }

  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
  // Enumerate all sound columns
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_soundLevels->clear();

  int colCount = xsh->getColumnCount();
  for (int i = 0; i < colCount; i++) {
    TXshColumn *col     = xsh->getColumn(i);
    TXshSoundColumn *sc = col->getSoundColumn();
    if (sc) {
      QString colname = "Col" + QString::number(i + 1);
      if (viewer) {
        TStageObject *pegbar = xsh->getStageObject(viewer->getObjectId(i));
        if (pegbar) colname = QString::fromStdString(pegbar->getName());
      }
      m_soundLevels->addItem(colname, QVariant(i));
    }
  }

  m_soundLevels->addItem(tr("Choose File..."), QVariant(-1));
  if (currentIndex < m_soundLevels->count()) {
    m_soundLevels->setCurrentIndex(currentIndex);
  }

  // Sets the visibility of the "Insert at Frame" controls
  if (m_soundLevels->currentIndex() < m_soundLevels->count() - 1) {
    m_insertAtLabel->hide();
    m_startAt->hide();
  } else {
    m_insertAtLabel->show();
    m_startAt->show();
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::playSound() {
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
          m_audioTimeout.start(static_cast<int>(duration));
        }
      }
    }
  } else {  // level < 0
    if (m_player->state() == QMediaPlayer::StoppedState) {
      // Check null pointer
      if (!m_audioFile) {
        DVGui::warning(tr("Audio file field is not available."));
        return;
      }

      if (m_audioFile->getPath().isEmpty()) {
        DVGui::warning(tr("Please select an audio file first."));
        return;
      }

      TFilePath tempPath(m_audioFile->getPath());
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      tempPath          = scene->decodeFilePath(tempPath);

      QFileInfo fileInfo(tempPath.getQString());
      if (!fileInfo.exists()) {
        DVGui::warning(tr("Audio file does not exist."));
        return;
      }

      m_player->setMedia(QUrl::fromLocalFile(tempPath.getQString()));
      m_player->setVolume(50);
      m_player->setNotifyInterval(20);

      // TODO: Real-time lip sync preview (not implemented)
      // The positionChanged signal emits the current playback position in
      // milliseconds. connect(m_player, &QMediaPlayer::positionChanged, this,
      //         &AutoLipSyncPopup::updatePlaybackPosition);
      connect(m_player, &QMediaPlayer::stateChanged, this,
              &AutoLipSyncPopup::onMediaStateChanged);

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
    TXshColumn *col     = xsh->getColumn(i);
    TXshSoundColumn *sc = col->getSoundColumn();
    if (sc && sc->isPlaying()) {
      sc->stop();
    }
  }

  if (m_player->state() != QMediaPlayer::StoppedState) {
    m_player->stop();
  }

  m_playButton->setIcon(m_playIcon);
  m_audioTimeout.stop();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onMediaStateChanged(QMediaPlayer::State state) {
  // Stopping can happen through the stop button or the file ending
  if (state == QMediaPlayer::StoppedState) {
    m_playButton->setIcon(m_playIcon);
  } else if (state == QMediaPlayer::PlayingState) {
    m_playButton->setIcon(m_stopIcon);
  }
}

//-----------------------------------------------------------------------------
bool AutoLipSyncPopup::setAudioFile() {
  m_audioPath  = "";
  m_startFrame = -1;
  m_deleteFile = false;

  int level = m_soundLevels->currentData().toInt();
  if (level >= 0) {
    saveAudio();
    m_deleteFile = true;
  } else {
    TFilePath tempPath(m_audioFile->getPath());
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    tempPath          = scene->decodeFilePath(tempPath);
    m_audioPath       = tempPath.getQString();
  }

  QFileInfo audioFileInfo(m_audioPath);
  if (!audioFileInfo.exists()) {
    DVGui::warning(tr("Please choose an audio file and try again."));
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
void AutoLipSyncPopup::saveAudio() {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  QDir cacheDir(cacheRoot);
  if (!cacheDir.exists("rhubarb")) {
    cacheDir.mkpath("rhubarb");
  }

  TFilePath audioPath = TFilePath(cacheRoot + "/rhubarb/temp.wav");

  int level       = m_soundLevels->currentData().toInt();
  TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshColumn *col = xsh->getColumn(level);

  if (col) {
    TXshSoundColumn *sc = col->getSoundColumn();
    if (sc) {
      int r0, r1;
      xsh->getCellRange(level, r0, r1);
      TSoundTrackP st = sc->getOverallSoundTrack(r0);
      if (st) {
        TSoundTrackWriter::save(audioPath, st);
        m_audioPath  = audioPath.getQString();
        m_startFrame = r0 + 1;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::runRhubarb() {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  QDir cacheDir(cacheRoot);
  if (!cacheDir.exists("rhubarb")) {
    cacheDir.mkpath("rhubarb");
  }

  m_datPath       = TFilePath(cacheRoot + "/rhubarb/temp.dat");
  QString datPath = m_datPath.getQString();

  QStringList args;
  args << "-o" << datPath << "-f" << "dat" << "--datUsePrestonBlair";

  if (!m_scriptEdit->toPlainText().isEmpty()) {
    QString script = m_scriptEdit->toPlainText();
    QString scriptPath =
        TFilePath(cacheRoot + "/rhubarb/script.txt").getQString();

    QFile qFile(scriptPath);
    if (qFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
      output.mid(2, (output.size() - 4)).split(", ", Qt::SkipEmptyParts);

  if (!outputList.isEmpty()) {
    QStringList outputType = outputList.at(0).split(": ");
    if (outputType.at(1) == "progress") {
      QStringList outputValue = outputList.at(1).split(": ");
      double progress         = outputValue.at(1).toDouble() * 100.0;
      m_progressDialog->setValue(static_cast<int>(progress));
    } else if (outputType.at(1) == "failure") {
      QStringList outputReason = outputList.at(1).split(": ");
      DVGui::warning(tr("Rhubarb Processing Error:\n\n") + outputReason.at(1));
    }
  }
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onAudioTimeout() {
  if (m_playingSound) {
    m_playingSound->stop();
  }
  m_playButton->setIcon(m_playIcon);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onLevelChanged(int index) {
  // If we don't have a valid index, use the current one
  if (index < 0 && m_soundLevels->count() > 0) {
    index = m_soundLevels->currentIndex();
  }

  int count = m_soundLevels->count();
  if (count == 0) return;

  // Control the visibility of the file field as in the original
  if (index == count - 1) {
    // "Choose File..." selected - show the file field
    if (m_audioFile) {
      m_audioFile->show();
    }
  } else {
    // Sound column selected - hide the file field
    if (m_audioFile) {
      m_audioFile->hide();
    }
  }

  // Update visibility of the "Insert at Frame" controls
  int level = m_soundLevels->currentData().toInt();
  if (level >= 0) {
    m_insertAtLabel->hide();
    m_startAt->hide();
  } else {
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

  if (m_rhubarbPath.isEmpty()) {
    DVGui::warning(
        tr("Rhubarb not found, please set the location in Preferences."));
    return;
  }

  // Reset process flags
  m_processRunning = true;
  m_userCancelled  = false;

  // Disconnect any previous connections to avoid multiple signals
  m_rhubarb->disconnect(this);

  // Connect signals for async processing
  connect(m_rhubarb,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &AutoLipSyncPopup::onRhubarbFinished);
  connect(m_rhubarb, &QProcess::errorOccurred, this,
          &AutoLipSyncPopup::onRhubarbError);

  // Show progress dialog
  m_progressDialog->setLabelText(tr("Analyzing audio..."));
  m_progressDialog->setValue(0);
  m_progressDialog->show();
  m_progressDialog->raise();
  m_progressDialog->activateWindow();

  // Get timeout from preferences (default 600 seconds = 10 minutes)
  int rhubarbTimeout = ThirdParty::getRhubarbTimeout();

  // Start timer for timeout (respect user preference)
  if (rhubarbTimeout > 0) {
    QTimer::singleShot(rhubarbTimeout * 1000, this, [this, rhubarbTimeout]() {
      if (m_processRunning) {
        DVGui::warning(tr("Rhubarb process timed out after %1 seconds. The "
                          "process may be stuck.")
                           .arg(rhubarbTimeout));
        onAnalysisCancelled();
      }
    });
  }

  runRhubarb();

  // waitForFinished() is NOT called here process is async
}

//-----------------------------------------------------------------------------
// Slot called when Rhubarb process finishes
void AutoLipSyncPopup::onRhubarbFinished(int exitCode,
                                         QProcess::ExitStatus exitStatus) {
  m_processRunning = false;
  m_progressDialog->hide();

  // If user cancelled, don't process results
  if (m_userCancelled) {
    cleanupAfterProcess();
    return;
  }

  if (exitStatus == QProcess::CrashExit) {
    DVGui::warning(tr("Rhubarb process crashed."));
    cleanupAfterProcess();
    return;
  }

  if (exitCode != 0) {
    // Error message should have been shown by onOutputReady
    cleanupAfterProcess();
    return;
  }

  // Process the results
  processRhubarbResults();
}

//-----------------------------------------------------------------------------
// Slot called when Rhubarb process has an error
void AutoLipSyncPopup::onRhubarbError(QProcess::ProcessError error) {
  m_processRunning = false;
  m_progressDialog->hide();

  // If the user cancelled, do not show an error â€” the "crash" was intentional
  if (m_userCancelled) {
    cleanupAfterProcess();
    return;
  }

  // Only show an error if it was NOT an intentional cancellation
  QString errorMsg;
  switch (error) {
  case QProcess::FailedToStart:
    errorMsg = tr("Rhubarb failed to start. Check if the path is correct.");
    break;
  case QProcess::Crashed:
    errorMsg = tr("Rhubarb crashed during execution.");
    break;
  case QProcess::Timedout:
    errorMsg = tr("Rhubarb process timed out.");
    break;
  case QProcess::WriteError:
  case QProcess::ReadError:
    errorMsg = tr("Communication error with Rhubarb process.");
    break;
  default:
    errorMsg = tr("Unknown error occurred with Rhubarb.");
  }

  DVGui::warning(errorMsg);
  cleanupAfterProcess();
}

//-----------------------------------------------------------------------------
// Slot called when user cancels analysis
void AutoLipSyncPopup::onAnalysisCancelled() {
  if (!m_processRunning) {
    m_progressDialog->hide();
    return;
  }

  // Mark that the user intentionally cancelled
  m_userCancelled = true;

  m_progressDialog->setLabelText(tr("Cancelling..."));
  m_progressDialog->show();

  if (m_rhubarb->state() == QProcess::Running) {
    m_rhubarb->terminate();
    if (!m_rhubarb->waitForFinished(2000)) {
      m_rhubarb->kill();
      m_rhubarb->waitForFinished(1000);
    }
  }

  m_progressDialog->hide();
  cleanupAfterProcess();
}

//-----------------------------------------------------------------------------
// Clean up after process completion or cancellation
void AutoLipSyncPopup::cleanupAfterProcess() {
  m_processRunning = false;

  // Clean temporary files
  if (m_deleteFile && !m_audioPath.isEmpty()) {
    TFilePath fp(m_audioPath);
    if (TSystem::doesExistFileOrLevel(fp)) {
      TSystem::deleteFile(fp);
    }
  }

  // Clean dat file if exists
  if (!m_datPath.isEmpty() && TSystem::doesExistFileOrLevel(m_datPath)) {
    TSystem::deleteFile(m_datPath);
  }

  m_deleteFile = false;
  m_progressDialog->hide();
}

//-----------------------------------------------------------------------------
// Process Rhubarb results after successful completion
void AutoLipSyncPopup::processRhubarbResults() {
  QString results = m_rhubarb->readAllStandardError();
  results += m_rhubarb->readAllStandardOutput();
  m_rhubarb->close();

  m_startAt->setValue(std::max(1, m_startFrame));

  m_valid = false;
  m_textLines.clear();

  QString path = m_datPath.getQString();
  if (path.isEmpty()) {
    DVGui::warning(tr("Please choose a lip sync data file to continue."));
    cleanupAfterProcess();
    return;
  }

  TFilePath tempPath(path);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  tempPath          = scene->decodeFilePath(tempPath);

  if (!TSystem::doesExistFileOrLevel(tempPath)) {
    DVGui::warning(
        tr("Cannot find the file specified. \nPlease choose a valid lip sync "
           "data file to continue."));
    cleanupAfterProcess();
    return;
  }

  QFile file(tempPath.getQString());
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    DVGui::warning(tr("Unable to open the file: \n") + file.errorString());
    cleanupAfterProcess();
    return;
  }

  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line        = in.readLine();
    QStringList entries = line.split(' ', Qt::SkipEmptyParts);
    if (entries.size() != 2) continue;

    bool ok;
    if (!entries.at(0).toInt(&ok) || !ok) continue;  // first must be a number
    if (entries.at(1).toInt(&ok) && ok) continue;    // second not a number

    m_textLines << entries;
  }

  file.close();

  if (m_textLines.size() <= 1) {
    DVGui::warning(
        tr("Invalid data file.\n Please choose a valid lip sync data file to "
           "continue."));
    m_valid = false;
    cleanupAfterProcess();
    return;
  } else {
    m_valid = true;
  }

  if (!m_valid || (!m_sl && !m_cl)) {
    cleanupAfterProcess();
    hide();
    return;
  }

  int startFrame = m_startAt->getValue() - 1;
  TXsheet *xsh   = TApp::instance()->getCurrentScene()->getScene()->getXsheet();

  // VERIFY if m_childLevel is still valid
  if (!m_cl && m_childLevel) {
    // Update m_childLevel if necessary
    TXshCell cell = xsh->getCell(startFrame, m_col);
    if (!cell.isEmpty() && cell.m_level) {
      m_childLevel = cell.m_level;
    }
  }

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
      m_col, m_sl, m_childLevel, m_activeFrameIds, m_textLines, lastFrame,
      previousFrameIds, previousLevels, startFrame);

  TUndoManager::manager()->add(undo);
  undo->redo();

  cleanupAfterProcess();
  hide();
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::imageNavClicked(int id) {
  if (!m_sl && !m_cl) return;

  int direction   = (id % 2) ? 1 : -1;
  int frameNumber = id / 2;

  if (frameNumber < 0 ||
      frameNumber >= static_cast<int>(m_activeFrameIds.size())) {
    return;
  }

  TFrameId currentFrameId = m_activeFrameIds[frameNumber];
  auto it =
      std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(), currentFrameId);

  if (it == m_levelFrameIds.end()) {
    return;
  }

  int frameIndex = std::distance(m_levelFrameIds.begin(), it);
  int newIndex;

  if (frameIndex == static_cast<int>(m_levelFrameIds.size()) - 1 &&
      direction == 1) {
    newIndex = 0;
  } else if (frameIndex == 0 && direction == -1) {
    newIndex = static_cast<int>(m_levelFrameIds.size()) - 1;
  } else {
    newIndex = frameIndex + direction;
  }

  if (newIndex < 0 || newIndex >= static_cast<int>(m_levelFrameIds.size())) {
    return;
  }

  m_activeFrameIds[frameNumber] = m_levelFrameIds[newIndex];

  // Update the changed thumbnail
  updateThumbnail(frameNumber);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::paintEvent(QPaintEvent *event) {
  // Call base class implementation
  Dialog::paintEvent(event);
}

//-----------------------------------------------------------------------------

void AutoLipSyncPopup::onStartValueChanged() {
  int value = m_startAt->getValue();
  if (value < 1) {
    m_startAt->setValue(1);
  }
}

//-----------------------------------------------------------------------------

AutoLipSyncPopup::~AutoLipSyncPopup() {
  // Cleanup resources
  stopAllSound();

  // Kill any running process
  if (m_processRunning && m_rhubarb->state() == QProcess::Running) {
    m_rhubarb->kill();
    m_rhubarb->waitForFinished(1000);
  }

  // Clean up temporary files
  cleanupAfterProcess();

  // Clear data
  m_activeFrameIds.clear();
  m_levelFrameIds.clear();
  m_textLines.clear();
}

OpenPopupCommandHandler<AutoLipSyncPopup> openAutoLipSyncPopup(
    MI_AutoLipSyncPopup);
