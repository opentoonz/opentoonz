

#include "lipsyncpopup.h"

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

// TnzCore includes
#include "filebrowsermodel.h"
#include "xsheetdragtool.h"
#include "historytypes.h"

// Qt includes
#include <QHBoxLayout>
#include <QPushButton>
#include <QMainWindow>
#include <QApplication>
#include <QTextStream>
#include <QPainter>

//=============================================================================
/*! \class LipSyncPopup
                \brief The LipSyncPopup class provides a modal dialog to
   apply lip sync text data to an image column.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

//============================================================
//  Lip Sync Undo Class
//============================================================

class LipSyncUndo final : public TUndo {
public:
  LipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
              std::vector<TFrameId> activeFrameIds, QStringList textLines,
              int lastFrame, std::vector<TFrameId> previousFrameIds,
              std::vector<TXshLevelP> previousLevels, int startFrame);
  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Apply Lip Sync Data");
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

//-----------------------------------------------------------------------------
// LipSyncUndo Constructor
//-----------------------------------------------------------------------------

LipSyncUndo::LipSyncUndo(int col, TXshSimpleLevel *sl, TXshLevelP cl,
                         std::vector<TFrameId> activeFrameIds,
                         QStringList textLines, int lastFrame,
                         std::vector<TFrameId> previousFrameIds,
                         std::vector<TXshLevelP> previousLevels, int startFrame)
    : m_col(col)
    , m_sl(sl)
    , m_cl(cl)
    , m_textLines(textLines)
    , m_lastFrame(lastFrame)
    , m_previousFrameIds(previousFrameIds)
    , m_previousLevels(previousLevels)
    , m_activeFrameIds(activeFrameIds)
    , m_startFrame(startFrame) {}

void LipSyncUndo::undo() const {
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

//-----------------------------------------------------------------------------
// LipSyncUndo::redo() - Apply lip sync data to cells
//-----------------------------------------------------------------------------

void LipSyncUndo::redo() const {
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
    QString shape      = m_textLines.at(currentLine + 1).toLower();
    TFrameId currentId = TFrameId();
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

//-----------------------------------------------------------------------------
// LipSyncPopup Constructor
//-----------------------------------------------------------------------------

LipSyncPopup::LipSyncPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "LipSyncPopup")
    , m_sl(nullptr)
    , m_cl(nullptr)
    , m_childLevel()
    , m_col(0)
    , m_valid(false)
    , m_isEditingLevel(false) {
  setWindowTitle(tr("Apply Lip Sync Data"));
  setFixedWidth(860);
  setFixedHeight(400);
  m_applyButton = new QPushButton(tr("Apply"), this);
  m_applyButton->setEnabled(false);

  // Create phoneme labels
  m_aiLabel    = new QLabel(tr("A I Drawing"));
  m_oLabel     = new QLabel(tr("O Drawing"));
  m_eLabel     = new QLabel(tr("E Drawing"));
  m_uLabel     = new QLabel(tr("U Drawing"));
  m_lLabel     = new QLabel(tr("L Drawing"));
  m_wqLabel    = new QLabel(tr("W Q Drawing"));
  m_mbpLabel   = new QLabel(tr("M B P Drawing"));
  m_fvLabel    = new QLabel(tr("F V Drawing"));
  m_restLabel  = new QLabel(tr("Rest Drawing"));
  m_otherLabel = new QLabel(tr("C D G K N R S Th Y Z"));

  // Create input fields
  m_startAt   = new DVGui::IntLineEdit(this, 0);
  m_restToEnd = new QCheckBox(tr("Extend Rest Drawing to End Marker"), this);

  // Create placeholder image
  QImage placeHolder(160, 90, QImage::Format_ARGB32);
  placeHolder.fill(Qt::white);

  for (int i = 0; i < 10; i++) {
    m_pixmaps[i] = QPixmap::fromImage(placeHolder);
  }

  for (int i = 0; i < 10; i++) {
    m_imageLabels[i] = new QLabel();
    m_imageLabels[i]->setPixmap(m_pixmaps[i]);
    m_textLabels[i] = new QLabel("temp", this);
  }

  m_file = new DVGui::FileField(this, QString(""));
  m_file->setFileMode(QFileDialog::ExistingFile);
  QStringList filters;
  filters << "txt" << "dat";
  m_file->setFilters(QStringList(filters));
  m_file->setMinimumWidth(500);

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
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);
  {
    QGridLayout *phonemeLay = new QGridLayout();
    phonemeLay->setContentsMargins(10, 10, 10, 10);
    phonemeLay->setVerticalSpacing(10);
    phonemeLay->setHorizontalSpacing(10);

    int i = 0;  // navButtons index
    int j = 0;  // imageLabels index
    int k = 0;  // textLabels index

    // First row: Phoneme labels
    phonemeLay->addWidget(m_aiLabel, 0, 0, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_eLabel, 0, 2, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_oLabel, 0, 4, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_uLabel, 0, 6, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_fvLabel, 0, 8, 1, 2, Qt::AlignCenter);
    // Second row: Images
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
    // Third row: Text labels
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
    // Fourth row: Navigation buttons
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
    // Empty row
    phonemeLay->addWidget(new QLabel("", this), 4, Qt::AlignCenter);

    // Fifth row: Phoneme labels
    phonemeLay->addWidget(m_lLabel, 5, 0, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_mbpLabel, 5, 2, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_wqLabel, 5, 4, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_otherLabel, 5, 6, 1, 2, Qt::AlignCenter);
    phonemeLay->addWidget(m_restLabel, 5, 8, 1, 2, Qt::AlignCenter);
    // Sixth row: Images
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
    // Seventh row: Text labels
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
    // Eighth row: Navigation buttons
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
    phonemeLay->addWidget(new QLabel("", this), 9, Qt::AlignCenter);
    phonemeLay->addWidget(new QLabel(tr("Insert at Frame: ")), 10, 0, 1, 1,
                          Qt::AlignRight);
    phonemeLay->addWidget(m_startAt, 10, 1, 1, 1, Qt::AlignLeft);
    phonemeLay->addWidget(m_restToEnd, 10, 2, 1, 6, Qt::AlignLeft);

    m_topLayout->addLayout(phonemeLay, 0);
  }

  // Bottom layout
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(10);
  {
    QHBoxLayout *fileLay = new QHBoxLayout();
    fileLay->addWidget(new QLabel(tr("Lip Sync Data File: ")), Qt::AlignLeft);
    fileLay->addWidget(m_file);
    m_buttonLayout->addLayout(fileLay);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_applyButton);
  }

  //-----------------------------------------------------------------------------
  // signal connections
  //-----------------------------------------------------------------------------
  for (int i = 0; i < 20; i++) {
    connect(m_navButtons[i], &QPushButton::clicked, this,
            [this, i]() { imageNavClicked(i); });
  }

  connect(m_applyButton, &QPushButton::clicked, this,
          &LipSyncPopup::onApplyButton);
  connect(m_file, &DVGui::FileField::pathChanged, this,
          &LipSyncPopup::onPathChanged);
  connect(m_startAt, &DVGui::IntLineEdit::editingFinished, this,
          &LipSyncPopup::onStartValueChanged);
}

//-----------------------------------------------------------------------------

void LipSyncPopup::generateThumbnails() {
  if (!m_sl && !m_cl) {
    // Show placeholders
    QImage placeHolder(160, 90, QImage::Format_ARGB32);
    placeHolder.fill(Qt::gray);
    for (int i = 0; i < 10; i++) {
      m_pixmaps[i] = QPixmap::fromImage(placeHolder);
      m_imageLabels[i]->setPixmap(m_pixmaps[i]);
      m_textLabels[i]->setText("");
    }
    update();
    return;
  }

  // Generate all thumbnails
  for (int i = 0; i < 10 && i < static_cast<int>(m_activeFrameIds.size());
       i++) {
    updateThumbnail(i);
  }
}

//-----------------------------------------------------------------------------

void LipSyncPopup::updateThumbnail(int index) {
  if (index < 0 || index >= 10) return;

  if (!m_sl && !m_cl) return;

  TFrameId frameId;
  if (index < static_cast<int>(m_activeFrameIds.size())) {
    frameId = m_activeFrameIds[index];
  } else {
    return;
  }

  const qreal dpr = qApp->devicePixelRatio();

  if (m_sl) {
    // Generate thumbnail
    QPixmap pm = IconGenerator::instance()->getSizedIcon(m_sl, frameId, "");

    if (!pm.isNull()) {
      QPixmap scaled = pm.scaled(int(160 * dpr), int(90 * dpr),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);

      scaled.setDevicePixelRatio(dpr);

      m_pixmaps[index] = scaled;
      m_imageLabels[index]->setPixmap(scaled);

      m_textLabels[index]->setText(tr("Drawing: ") +
                                   QString::number(frameId.getNumber()));
    }
  } else if (m_cl) {
    // For child levels, show placeholder
    QImage img(int(160 * dpr), int(90 * dpr), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::gray);

    QPainter p(&img);
    p.setPen(Qt::black);

    // Find frame index
    auto it =
        std::find(m_levelFrameIds.begin(), m_levelFrameIds.end(), frameId);
    int frameIndex = (it != m_levelFrameIds.end())
                         ? std::distance(m_levelFrameIds.begin(), it) + 1
                         : index + 1;

    p.drawText(img.rect(), tr("SubXSheet Frame ") + QString::number(frameIndex),
               QTextOption(Qt::AlignCenter));
    p.end();

    m_pixmaps[index] = QPixmap::fromImage(img);
    m_imageLabels[index]->setPixmap(m_pixmaps[index]);
    m_textLabels[index]->setText(tr("Frame ") + QString::number(frameIndex));
  }
}

//-----------------------------------------------------------------------------

void LipSyncPopup::onIconGenerated() {
  // Update thumbnails if popup is visible
  if (!isVisible() || !isEnabled()) return;

  // Force update of all thumbnails
  generateThumbnails();
  update();
}

//-----------------------------------------------------------------------------

void LipSyncPopup::showEvent(QShowEvent *) {
  // reset
  m_activeFrameIds.clear();
  m_levelFrameIds.clear();
  m_sl         = nullptr;
  m_cl         = nullptr;
  m_childLevel = TXshLevelP();
  m_startAt->setValue(1);
  m_valid = false;
  m_applyButton->setEnabled(false);
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentScene()->getScene()->getXsheet();
  m_col        = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
  int row      = app->getCurrentFrame()->getFrame();
  m_isEditingLevel = app->getCurrentFrame()->isEditingLevel();
  m_startAt->setValue(row + 1);
  m_startAt->clearFocus();

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
    return;
  }

  // Get frame IDs
  if (m_sl) {
    m_sl->getFids(m_levelFrameIds);
  } else if (m_cl && m_childLevel) {
    // For child levels, use placeholder frames
    m_levelFrameIds.clear();
    for (int i = 1; i <= 10; i++) {
      m_levelFrameIds.push_back(TFrameId(i));
    }
  }

  if (!m_levelFrameIds.empty()) {
    // Fill with first 10 frames or repeat the first one
    for (int i = 0; i < 10; i++) {
      if (i < static_cast<int>(m_levelFrameIds.size())) {
        m_activeFrameIds.push_back(m_levelFrameIds[i]);
      } else {
        m_activeFrameIds.push_back(m_levelFrameIds[0]);
      }
    }
  } else {
    // Clear active frame ids if no level frames
    m_activeFrameIds.clear();
    m_activeFrameIds.resize(10, TFrameId());
  }

  // Connect to IconGenerator for asynchronous thumbnail updates
  connect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
          &LipSyncPopup::onIconGenerated, Qt::QueuedConnection);

  // Generate initial thumbnails
  generateThumbnails();
}

//-----------------------------------------------------------------------------

void LipSyncPopup::hideEvent(QHideEvent *event) {
  if (IconGenerator::instance()) {
    disconnect(IconGenerator::instance(), &IconGenerator::iconGenerated, this,
               &LipSyncPopup::onIconGenerated);
  }
  Dialog::hideEvent(event);  // call base
}

//-----------------------------------------------------------------------------

void LipSyncPopup::onApplyButton() {
  if (!m_valid || (!m_sl && !m_cl)) {
    DVGui::warning(tr("Invalid configuration or data."));
    hide();
    return;
  }

  if (m_textLines.size() < 2) {
    DVGui::warning(tr("Invalid data file. Need at least one phoneme entry."));
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

  // Store previous state for undo
  std::vector<TFrameId> previousFrameIds;
  std::vector<TXshLevelP> previousLevels;

  for (int previousFrame = startFrame; previousFrame < lastFrame;
       previousFrame++) {
    TXshCell cell = xsh->getCell(previousFrame, m_col);
    previousFrameIds.push_back(cell.m_frameId);
    previousLevels.push_back(cell.m_level);
  }

  LipSyncUndo *undo =
      new LipSyncUndo(m_col, m_sl, m_childLevel, m_activeFrameIds, m_textLines,
                      lastFrame, previousFrameIds, previousLevels, startFrame);
  TUndoManager::manager()->add(undo);
  undo->redo();
  hide();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::imageNavClicked()
//-----------------------------------------------------------------------------

void LipSyncPopup::imageNavClicked(int id) {
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

  // Update only the changed thumbnail
  updateThumbnail(frameNumber);
}

//-----------------------------------------------------------------------------

void LipSyncPopup::paintEvent(QPaintEvent *event) {
  // Base class implementation
  Dialog::paintEvent(event);
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onPathChanged()
//-----------------------------------------------------------------------------

void LipSyncPopup::onPathChanged() {
  m_textLines.clear();
  QString path = m_file->getPath();
  if (path.isEmpty()) {
    m_valid = false;
    m_applyButton->setEnabled(false);
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    DVGui::warning(tr("Unable to open the file: \n") + file.errorString());
    m_valid = false;
    m_applyButton->setEnabled(false);
    return;
  }

  QTextStream in(&file);

  while (!in.atEnd()) {
    QString line        = in.readLine();
    QStringList entries = line.split(' ', Qt::SkipEmptyParts);
    if (entries.size() != 2) continue;

    bool ok;
    // make sure the first entry is a number
    entries.at(0).toInt(&ok);
    if (!ok) continue;
    // make sure the second entry isn't a number;
    entries.at(1).toInt(&ok);
    if (ok) continue;
    m_textLines << entries;
  }

  if (m_textLines.size() <= 1) {
    DVGui::warning(tr("Invalid data file."));
    m_valid = false;
    m_applyButton->setEnabled(false);
  } else {
    m_valid = true;
    m_applyButton->setEnabled(true);
  }

  file.close();
}

//-----------------------------------------------------------------------------
// LipSyncPopup::onStartValueChanged()
//-----------------------------------------------------------------------------

void LipSyncPopup::onStartValueChanged() {
  int value = m_startAt->getValue();
  if (value < 1) m_startAt->setValue(1);
}

//-----------------------------------------------------------------------------

LipSyncPopup::~LipSyncPopup() {
  if (IconGenerator::instance()) {
    IconGenerator::instance()->disconnect(this);
  }
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<LipSyncPopup> openLipSyncPopup(MI_LipSyncPopup);
