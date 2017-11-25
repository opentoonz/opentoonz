#include "autoinputcellnumberpopup.h"

// Tnz includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "cellselection.h"
#include "columnselection.h"
#include "penciltestpopup.h"  // for FrameNumberLineEdit

// TnzQt includes
#include "toonzqt/intfield.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/preferences.h"

#include "tundo.h"
#include "historytypes.h"

#include <QMainWindow>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include <iostream>

namespace {

class AutoInputCellNumberUndo final : public TUndo {
  int m_increment, m_interval, m_step, m_repeat;
  int m_from, m_to;
  int m_r0, m_r1;
  bool m_isOverwrite;
  std::vector<int> m_columnIndices;
  std::vector<TXshLevelP> m_levels;

  int m_rowsCount;
  std::unique_ptr<TXshCell[]> m_beforeCells;

public:
  AutoInputCellNumberUndo(int increment, int interval, int step, int repeat,
                          int from, int to, int r0, int r1, bool isOverwrite,
                          std::vector<int> columnIndices,
                          std::vector<TXshLevelP> levels);

  ~AutoInputCellNumberUndo() {}

  void undo() const override;
  void redo() const override;
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Auto Input Cell Numbers : %1")
        .arg((m_isOverwrite) ? QObject::tr("Overwrite")
                             : QObject::tr("Insert"));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }

  int rowsCount() { return m_rowsCount; }
};
};

//-----------------------------------------------------------------------------
// executing this on column selection, set r1 = -1.
// Here are the expected behaviors
// 1. Cell Selection + Overwrite
//    Cells will be input from the top of the selected range.
//    New arrangement CANNOT run over the selected range.
//    If the new arrangement is shorter than the selected range,
//    excess cells will not be cleared but keep their contents.
//    (It is the same behavior as Celsys' QuickChecker)
// 2. Cell Selection + Insert
//    New arrangement will be inserted before the selected range.
//    If the selected range has multiple columns, then the inserted
//    cells will be expanded to the longest arrangement with empty cells.
// 3. Column Selection + Overwrite
//    Cells will be input from the top of the columns.
//    New arrangement CAN run over the existing column range.
//    If the new arrangement is shorter than the selected range,
//    excess cells will not be cleared but keep their contents.
// 4. Column Selection + Insert
//    New arrangement will be inserted at the top of the columns.
//    If multiple columns are selected, then the inserted cells
//    will be expanded to the longest arrangement with empty cells.
//

AutoInputCellNumberUndo::AutoInputCellNumberUndo(int increment, int interval,
                                                 int step, int repeat, int from,
                                                 int to, int r0, int r1,
                                                 bool isOverwrite,
                                                 std::vector<int> columnIndices,
                                                 std::vector<TXshLevelP> levels)
    : m_increment(increment)
    , m_interval(interval)
    , m_step(step)
    , m_repeat(repeat)
    , m_from(from)
    , m_to(to)
    , m_r0(r0)
    , m_r1(r1)
    , m_isOverwrite(isOverwrite)
    , m_columnIndices(columnIndices)
    , m_levels(levels) {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
    m_increment *= 10;

  int maxInputFrameAmount = 0;
  int frameRangeMin       = std::min(m_from, m_to);
  int frameRangeMax       = std::max(m_from, m_to);

  if (m_increment == 0) {
    // obtain the maximum frame amount to be input
    for (int lv = 0; lv < m_levels.size(); lv++) {
      TXshLevelP level = m_levels.at(lv);
      std::vector<TFrameId> fids;
      level->getFids(fids);
      int fCount = 0;
      for (auto itr = fids.begin(); itr != fids.end(); ++itr) {
        if ((*itr).getNumber() >= frameRangeMin &&
            (*itr).getNumber() <= frameRangeMax) {
          fCount++;
        } else if ((*itr).getNumber() > frameRangeMax)
          break;
      }
      if (maxInputFrameAmount < fCount) maxInputFrameAmount = fCount;
    }
  } else
    maxInputFrameAmount = (int)std::ceil(
        (float)(frameRangeMax - frameRangeMin + 1) / (float)m_increment);

  maxInputFrameAmount *= m_repeat;

  // compute maximum row amount to be arranged
  if (maxInputFrameAmount == 0) {
    m_rowsCount = 0;
    return;
  } else
    m_rowsCount =
        maxInputFrameAmount * m_step + (maxInputFrameAmount - 1) * m_interval;

  // on overwrite case, keep existing cells for undo
  if (m_isOverwrite) {
    int rowUpTo =
        (m_r1 == -1) ? m_rowsCount - 1 : std::min(m_r1, m_r0 + m_rowsCount - 1);
    m_beforeCells.reset(
        new TXshCell[m_columnIndices.size() * (rowUpTo - m_r0 + 1)]);
    int k = 0;
    for (int c = 0; c < m_columnIndices.size(); ++c) {
      for (int r = m_r0; r <= rowUpTo; ++r)
        m_beforeCells[k++] =
            TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(
                r, m_columnIndices.at(c));
    }
  }
}

void AutoInputCellNumberUndo::undo() const {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // on overwrite case, retrieve cells
  if (m_isOverwrite) {
    int rowUpTo =
        (m_r1 == -1) ? m_rowsCount - 1 : std::min(m_r1, m_r0 + m_rowsCount - 1);
    int k = 0;
    for (int c = 0; c < m_columnIndices.size(); ++c) {
      for (int r = m_r0; r <= rowUpTo; ++r) {
        if (m_beforeCells[k].isEmpty())
          xsh->clearCells(r, m_columnIndices.at(c));
        else
          xsh->setCell(r, m_columnIndices.at(c), m_beforeCells[k]);
        k++;
      }
    }
  } else {  // on insert case, remove inserted cells
    for (int c = 0; c < m_columnIndices.size(); ++c)
      xsh->removeCells(m_r0, m_columnIndices.at(c), m_rowsCount);
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void AutoInputCellNumberUndo::redo() const {
  TApp::instance()->getCurrentXsheet()->getXsheet()->autoInputCellNumbers(
      m_increment, m_interval, m_step, m_repeat, m_from, m_to, m_r0, m_r1,
      m_isOverwrite, m_columnIndices, m_levels, m_rowsCount);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================

AutoInputCellNumberPopup::AutoInputCellNumberPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, true,
             "AutoInputCellNumberPopup") {
  setWindowTitle(tr("Auto Input Cell Number"));

  m_from      = new FrameNumberLineEdit(this);
  m_increment = new DVGui::IntLineEdit(this, 0, 0);
  m_to        = new FrameNumberLineEdit(this);
  m_interval  = new DVGui::IntLineEdit(this, 0, 0);
  m_step      = new DVGui::IntLineEdit(this, 1, 1);
  m_repeat    = new DVGui::IntLineEdit(this, 1, 1);

  m_overwriteBtn         = new QPushButton(tr("Overwrite"), this);
  m_insertBtn            = new QPushButton(tr("Insert"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);

  m_increment->setToolTip(
      tr("Setting this value 0 will automatically \npick up all frames in the "
         "selected level."));

  // layout

  QGridLayout *mainLay = new QGridLayout();
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("From frame"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_from, 0, 1);
    mainLay->addWidget(new QLabel(tr(" ", "from frame"), this), 0, 2, 1, 2);

    mainLay->addWidget(new QLabel(tr("with"), this), 1, 0, 1, 2,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_increment, 1, 2);
    mainLay->addWidget(new QLabel(tr("frames increment"), this), 1, 3);

    mainLay->addWidget(new QLabel(tr("To frame"), this), 2, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_to, 2, 1);
    mainLay->addWidget(new QLabel(tr(" ", "to frame"), this), 2, 2, 1, 2);

    mainLay->addWidget(new QLabel(tr("inserting"), this), 3, 0, 1, 2,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_interval, 3, 2);
    mainLay->addWidget(new QLabel(tr("empty cell intervals"), this), 3, 3);

    mainLay->addWidget(new QLabel(tr("with"), this), 4, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_step, 4, 1);
    mainLay->addWidget(new QLabel(tr("cell steps"), this), 4, 2, 1, 2);

    mainLay->addWidget(new QLabel(tr("Repeat"), this), 5, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_repeat, 5, 1);
    mainLay->addWidget(new QLabel(tr("times"), this), 5, 2, 1, 2);
  }
  m_topLayout->addLayout(mainLay);

  m_buttonFrame = new QFrame(this);
  m_buttonFrame->setObjectName("dialogButtonFrame");
  m_buttonFrame->setFrameStyle(QFrame::StyledPanel);
  m_buttonFrame->setFixedHeight(45);

  m_buttonLayout = new QHBoxLayout;
  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  m_buttonLayout->setAlignment(Qt::AlignHCenter);
  {
    m_buttonLayout->addSpacing(20);
    m_buttonLayout->addWidget(m_overwriteBtn, 0);
    m_buttonLayout->addWidget(m_insertBtn, 0);
    m_buttonLayout->addWidget(cancelBtn, 0);
    m_buttonLayout->addSpacing(20);
  }
  m_buttonFrame->setLayout(m_buttonLayout);
  layout()->addWidget(m_buttonFrame);

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_overwriteBtn, SIGNAL(clicked()), this,
                       SLOT(onOverwritePressed()));
  ret = ret &&
        connect(m_insertBtn, SIGNAL(clicked()), this, SLOT(onInsertPressed()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(close()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::onOverwritePressed() { doExecute(true); }

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::onInsertPressed() { doExecute(false); }

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::doExecute(bool overwrite) {
  std::vector<int> columnIndices;
  std::vector<TXshLevelP> levels;
  int r0, r1;
  bool found = getTarget(columnIndices, levels, r0, r1);
  // just in case the execution is done to empty column
  if (!found) {
    DVGui::MsgBox(DVGui::WARNING,
                  tr("No available cells or columns are selected."));
    return;
  }
  AutoInputCellNumberUndo *undo = new AutoInputCellNumberUndo(
      m_increment->getValue(), m_interval->getValue(), m_step->getValue(),
      m_repeat->getValue(), m_from->getValue(), m_to->getValue(), r0, r1,
      overwrite, columnIndices, levels);
  // if no cells will be arranged, then return
  if (undo->rowsCount() == 0) {
    DVGui::MsgBox(DVGui::WARNING,
                  tr("Selected level has no frames between From and To."));
    delete undo;
    return;
  }
  undo->redo();
  TUndoManager::manager()->add(undo);
  // on cell selection & insert case, select the inserted cell range
  if (!overwrite && r1 != -1) {
    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (!selection) return;
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
    if (!cellSelection) return;
    int c0, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    cellSelection->selectCells(r0, c0, r0 + undo->rowsCount() - 1, c1);
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
  // If exection is properly completed, then close this popup
  close();
}

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::onSelectionChanged() {
  // enable buttons only if they will work
  m_overwriteBtn->setEnabled(false);
  m_insertBtn->setEnabled(false);

  std::vector<int> columnIndices;
  std::vector<TXshLevelP> levels;
  int r0, r1;
  bool found = getTarget(columnIndices, levels, r0, r1, true);

  // if no reference level is found, return
  if (!found) return;

  int from, to;
  std::vector<TFrameId> fids;
  levels.front()->getFids(fids);
  from = fids.front().getNumber();
  to   = fids.back().getNumber();

  m_from->setValue(from);
  m_to->setValue(to);

  // enable buttons
  m_overwriteBtn->setEnabled(true);
  m_insertBtn->setEnabled(true);
}

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::showEvent(QShowEvent *) {
  bool ret = connect(TApp::instance()->getCurrentSelection(),
                     SIGNAL(selectionChanged(TSelection *)), this,
                     SLOT(onSelectionChanged()));
  assert(ret);
  onSelectionChanged();
}

//-----------------------------------------------------------------------------

void AutoInputCellNumberPopup::hideEvent(QHideEvent *) {
  bool ret = disconnect(TApp::instance()->getCurrentSelection(),
                        SIGNAL(selectionChanged(TSelection *)), this,
                        SLOT(onSelectionChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

bool AutoInputCellNumberPopup::getTarget(std::vector<int> &columnIndices,
                                         std::vector<TXshLevelP> &levels,
                                         int &r0, int &r1, bool forCheck) {
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  if (!selection) return false;

  // something must be selected
  if (selection->isEmpty()) return false;

  // selection must be cells or collumns
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(selection);
  TColumnSelection *columnSelection =
      dynamic_cast<TColumnSelection *>(selection);
  if (!cellSelection && !columnSelection) return false;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // TXshLevelP refLevel;

  if (cellSelection) {
    // if cells are selected, selected range must contain at least one available
    // level
    int c0, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    for (int col = c0; col <= c1; col++) {
      // column should not be empty or locked
      if (xsh->isColumnEmpty(col)) continue;
      TXshColumn *column = xsh->getColumn(col);
      if (column->isLocked()) continue;
      // column type should be "level"
      if (column->getColumnType() != TXshColumn::eLevelType) continue;
      // try to find the topmost available level in the column
      for (int row = r0; row <= r1; row++) {
        TXshCell cell = xsh->getCell(row, col);
        if (cell.isEmpty()) continue;
        TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
        TXshChildLevel *childLevel   = cell.getChildLevel();
        if (!simpleLevel && !childLevel) continue;
        if (simpleLevel) {
          TFrameId fid = simpleLevel->getFirstFid();
          // the level should not be single-framed
          if (fid.getNumber() == TFrameId::EMPTY_FRAME ||
              fid.getNumber() == TFrameId::NO_FRAME)
            continue;
        }
        levels.push_back(cell.m_level);
        columnIndices.push_back(col);
        break;
      }
      // if some level found, break the loop for check
      if (forCheck && !levels.empty()) break;
    }
  }

  else {  // columnSelection case
    r0                    = 0;
    r1                    = -1;
    std::set<int> indices = columnSelection->getIndices();
    // try to find the leftmost available column
    for (auto id = indices.begin(); id != indices.end(); ++id) {
      TXshColumn *column = xsh->getColumn(*id);
      if (!column) continue;
      if (column->isEmpty() || column->isLocked()) continue;
      // column type should be "level"
      if (column->getColumnType() != TXshColumn::eLevelType) continue;
      // try to find the topmost available level in the column
      TXshCellColumn *xshColumn = column->getCellColumn();
      if (!xshColumn) continue;
      int tmpR0, tmpR1;
      column->getRange(tmpR0, tmpR1);
      for (int row = tmpR0; row <= tmpR1; row++) {
        TXshCell cell = xshColumn->getCell(row);
        if (cell.isEmpty()) continue;
        TXshSimpleLevel *simpleLevel = cell.getSimpleLevel();
        TXshChildLevel *childLevel   = cell.getChildLevel();
        if (!simpleLevel && !childLevel) continue;
        if (simpleLevel) {
          TFrameId fid = simpleLevel->getFirstFid();
          // the level should not be single-framed
          if (fid.getNumber() == TFrameId::EMPTY_FRAME ||
              fid.getNumber() == TFrameId::NO_FRAME)
            continue;
        }
        levels.push_back(cell.m_level);
        columnIndices.push_back(*id);
        break;
      }
      // if some level found, break the loop for check
      if (forCheck && !levels.empty()) break;
    }
  }

  return !levels.empty();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<AutoInputCellNumberPopup> openAutoInputCellNumberPopup(
    MI_AutoInputCellNumber);
