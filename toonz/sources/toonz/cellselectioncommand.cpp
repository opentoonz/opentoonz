#include <memory>

#include "cellselection.h"

// Tnz6 includes
#include "tapp.h"
#include "duplicatepopup.h"
#include "overwritepopup.h"
#include "selectionutils.h"
#include "columnselection.h"
#include "reframepopup.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelset.h"
#include "toonz/tstageobject.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/stageobjectutil.h"
#include "toonz/hook.h"
#include "toonz/levelproperties.h"
#include "toonz/childstack.h"

// TnzCore includes
#include "tsystem.h"
#include "tundo.h"
#include "tmsgcore.h"
#include "trandom.h"
#include "tpalette.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QMainWindow>

// tcg includes
#include "tcg/tcg_macros.h"
#include "tcg/tcg_unique_ptr.h"

// STD includes
#include <ctime>

//*********************************************************************************
//    Reverse Cells  command
//*********************************************************************************

namespace {

class ReverseUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;

public:
  ReverseUndo(int r0, int c0, int r1, int c1)
      : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1) {}

  void redo() const override;
  void undo() const override { redo(); }  // Reverse is idempotent :)

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Reverse"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

void ReverseUndo::redo() const {
  TCG_ASSERT(m_r1 >= m_r0 && m_c1 >= m_c0, return );

  TApp::instance()->getCurrentXsheet()->getXsheet()->reverseCells(m_r0, m_c0,
                                                                  m_r1, m_c1);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::reverseCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo =
      new ReverseUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Swing Cells  command
//*********************************************************************************

namespace {

class SwingUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;

public:
  SwingUndo(int r0, int c0, int r1, int c1)
      : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1) {}

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Swing"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

void SwingUndo::redo() const {
  TApp::instance()->getCurrentXsheet()->getXsheet()->swingCells(m_r0, m_c0,
                                                                m_r1, m_c1);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void SwingUndo::undo() const {
  TCG_ASSERT(m_r1 >= m_r0 && m_c1 >= m_c0, return );

  for (int c = m_c0; c <= m_c1; ++c)
    TApp::instance()->getCurrentXsheet()->getXsheet()->removeCells(m_r1 + 1, c,
                                                                   m_r1 - m_r0);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::swingCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo =
      new SwingUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Increment Cells  command
//*********************************************************************************

namespace {

class IncrementUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  mutable std::vector<std::pair<TRect, TXshCell>> m_undoCells;

public:
  mutable bool m_ok;

public:
  IncrementUndo(int r0, int c0, int r1, int c1)
      : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1), m_ok(true) {}

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Autoexpose"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

void IncrementUndo::redo() const {
  TCG_ASSERT(m_r1 >= m_r0 && m_c1 >= m_c0, return );

  m_undoCells.clear();
  m_ok = TApp::instance()->getCurrentXsheet()->getXsheet()->incrementCells(
      m_r0, m_c0, m_r1, m_c1, m_undoCells);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void IncrementUndo::undo() const {
  TCG_ASSERT(m_r1 >= m_r0 && m_c1 >= m_c0 && m_ok, return );

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  for (int i = m_undoCells.size() - 1; i >= 0; --i) {
    const TRect &r = m_undoCells[i].first;
    int size       = r.x1 - r.x0 + 1;

    if (m_undoCells[i].second.isEmpty())
      xsh->removeCells(r.x0, r.y0, size);
    else {
      xsh->insertCells(r.x0, r.y0, size);
      for (int j = 0; j < size; ++j)
        xsh->setCell(r.x0 + j, r.y0, m_undoCells[i].second);
    }
  }

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::incrementCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  std::unique_ptr<IncrementUndo> undo(new IncrementUndo(
      m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1));

  if (undo->redo(), !undo->m_ok) {
    DVGui::error(
        QObject::tr("Invalid selection: each selected column must contain one "
                    "single level with increasing frame numbering."));
    return;
  }

  TUndoManager::manager()->add(undo.release());
}

//*********************************************************************************
//    Random Cells  command
//*********************************************************************************

namespace {

class RandomUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;

  std::vector<int> m_shuffle;  //!< Shuffled indices
  std::vector<int> m_elffuhs;  //!< Inverse shuffle indices

public:
  RandomUndo(int r0, int c0, int r1, int c1);

  void shuffleCells(int row, int col, const std::vector<int> &data) const;

  void redo() const override;
  void undo() const override;

  int getSize() const override {
    return sizeof(*this) + 2 * sizeof(int) * m_shuffle.size();
  }

  QString getHistoryString() override { return QObject::tr("Random"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

RandomUndo::RandomUndo(int r0, int c0, int r1, int c1)
    : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1) {
  TCG_ASSERT(m_r1 >= m_r0 && m_c1 >= m_c0, return );

  int r, rowCount = r1 - r0 + 1;
  std::vector<std::pair<unsigned int, int>> rndTable(rowCount);

  TRandom rnd(std::time(0));  // Standard seeding
  for (r = 0; r < rowCount; ++r) rndTable[r] = std::make_pair(rnd.getUInt(), r);

  std::sort(rndTable.begin(), rndTable.end());  // Random sort shuffle

  m_shuffle.resize(rowCount);
  m_elffuhs.resize(rowCount);
  for (r = 0; r < rowCount; ++r) {
    m_shuffle[r]                  = rndTable[r].second;
    m_elffuhs[rndTable[r].second] = r;
  }
}

//-----------------------------------------------------------------------------

void RandomUndo::shuffleCells(int row, int col,
                              const std::vector<int> &data) const {
  int rowCount = data.size();
  assert(rowCount > 0);

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  std::vector<TXshCell> bCells(rowCount), aCells(rowCount);
  xsh->getCells(row, col, rowCount, &bCells[0]);

  for (int i = 0; i < rowCount; ++i) aCells[data[i]] = bCells[i];

  xsh->setCells(row, col, rowCount, &aCells[0]);
}

//-----------------------------------------------------------------------------

void RandomUndo::undo() const {
  for (int c = m_c0; c <= m_c1; ++c) shuffleCells(m_r0, c, m_elffuhs);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void RandomUndo::redo() const {
  for (int c = m_c0; c <= m_c1; ++c) shuffleCells(m_r0, c, m_shuffle);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::randomCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo =
      new RandomUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Step Cells  command
//*********************************************************************************

namespace {

class StepUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  int m_rowsCount, m_colsCount;

  int m_step;
  int m_newRows;

  tcg::unique_ptr<TXshCell[]> m_cells;

public:
  StepUndo(int r0, int c0, int r1, int c1, int step);

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Step %1").arg(QString::number(m_step));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

StepUndo::StepUndo(int r0, int c0, int r1, int c1, int step)
    : m_r0(r0)
    , m_c0(c0)
    , m_r1(r1)
    , m_c1(c1)
    , m_rowsCount(r1 - r0 + 1)
    , m_colsCount(c1 - c0 + 1)
    , m_step(step)
    , m_newRows(m_rowsCount * (step - 1))
    , m_cells(new TXshCell[m_rowsCount * m_colsCount]) {
  assert(m_rowsCount > 0 && m_colsCount > 0 && step > 0);
  assert(m_cells.get());

  int k = 0;
  for (int r = r0; r <= r1; ++r)
    for (int c = c0; c <= c1; ++c)
      m_cells[k++] =
          TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r, c);
}

//-----------------------------------------------------------------------------

void StepUndo::redo() const {
  TCG_ASSERT(m_rowsCount > 0 && m_colsCount > 0, return );

  TApp::instance()->getCurrentXsheet()->getXsheet()->stepCells(m_r0, m_c0, m_r1,
                                                               m_c1, m_step);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void StepUndo::undo() const {
  TCG_ASSERT(m_rowsCount > 0 && m_colsCount > 0 && m_cells.get(), return );

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  for (int c = m_c0; c <= m_c1; ++c) xsh->removeCells(m_r1 + 1, c, m_newRows);

  int k = 0;
  for (int r = m_r0; r <= m_r1; ++r)
    for (int c = m_c0; c <= m_c1; ++c) {
      if (m_cells[k].isEmpty())
        xsh->clearCells(r, c);
      else
        xsh->setCell(r, c, m_cells[k]);
      k++;
    }
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::stepCells(int step) {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo = new StepUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1,
                             m_range.m_c1, step);
  TUndoManager::manager()->add(undo);

  undo->redo();
  m_range.m_r1 += (step - 1) * m_range.getRowCount();
}

//*********************************************************************************
//    Each Cells  command
//*********************************************************************************

namespace {

class EachUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  int m_rowsCount, m_colsCount;

  int m_each;
  int m_newRows;

  tcg::unique_ptr<TXshCell[]> m_cells;

public:
  EachUndo(int r0, int c0, int r1, int c1, int each);

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Each %1").arg(QString::number(m_each));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

EachUndo::EachUndo(int r0, int c0, int r1, int c1, int each)
    : m_r0(r0)
    , m_c0(c0)
    , m_r1(r1)
    , m_c1(c1)
    , m_rowsCount(r1 - r0 + 1)
    , m_colsCount(c1 - c0 + 1)
    , m_each(each)
    , m_newRows((m_rowsCount % each) ? m_rowsCount / each + 1
                                     : m_rowsCount / each)
    , m_cells(new TXshCell[m_rowsCount * m_colsCount]) {
  assert(m_rowsCount > 0 && m_colsCount > 0 && each > 0);
  assert(m_cells.get());

  int k = 0;
  for (int r = r0; r <= r1; ++r)
    for (int c = c0; c <= c1; ++c)
      m_cells[k++] =
          TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r, c);
}

//-----------------------------------------------------------------------------

void EachUndo::redo() const {
  TCG_ASSERT(m_rowsCount > 0 && m_colsCount > 0, return );

  TApp::instance()->getCurrentXsheet()->getXsheet()->eachCells(m_r0, m_c0, m_r1,
                                                               m_c1, m_each);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void EachUndo::undo() const {
  TCG_ASSERT(m_rowsCount > 0 && m_colsCount > 0 && m_cells.get(), return );

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  for (int c = m_c0; c <= m_c1; ++c)
    xsh->insertCells(m_r0 + m_newRows, c, m_rowsCount - m_newRows);

  int k = 0;
  for (int r = m_r0; r <= m_r1; ++r)
    for (int c = m_c0; c <= m_c1; ++c) {
      if (m_cells[k].isEmpty())
        xsh->clearCells(r, c);
      else
        xsh->setCell(r, c, m_cells[k]);
      k++;
    }

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::eachCells(int each) {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo = new EachUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1,
                             m_range.m_c1, each);
  TUndoManager::manager()->add(undo);

  undo->redo();
  m_range.m_r1 = m_range.m_r0 + (m_range.m_r1 - m_range.m_r0 + each) / each - 1;
}

//*********************************************************************************
//    Reframe command : 強制的にNコマ打ちにする
//*********************************************************************************

namespace {

class ReframeUndo final : public TUndo {
  int m_r0, m_r1;
  int m_type;
  int m_nr;
  int m_withBlank;
  std::unique_ptr<TXshCell[]> m_cells;

public:
  std::vector<int> m_newRows;

  std::vector<int> m_columnIndeces;

  ReframeUndo(int r0, int r1, std::vector<int> columnIndeces, int type,
              int withBlank = -1);
  ~ReframeUndo();
  void undo() const override;
  void redo() const override;
  void repeat() const;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    if (m_withBlank == -1)
      return QObject::tr("Reframe to %1's").arg(QString::number(m_type));
    else
      return QObject::tr("Reframe to %1's with %2 blanks")
          .arg(QString::number(m_type))
          .arg(QString::number(m_withBlank));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

ReframeUndo::ReframeUndo(int r0, int r1, std::vector<int> columnIndeces,
                         int type, int withBlank)
    : m_r0(r0)
    , m_r1(r1)
    , m_type(type)
    , m_nr(0)
    , m_columnIndeces(columnIndeces)
    , m_withBlank(withBlank) {
  m_nr = m_r1 - m_r0 + 1;
  assert(m_nr > 0);
  m_cells.reset(new TXshCell[m_nr * (int)m_columnIndeces.size()]);
  assert(m_cells);
  int k = 0;
  for (int r = r0; r <= r1; r++)
    for (int c     = 0; c < (int)m_columnIndeces.size(); c++)
      m_cells[k++] = TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(
          r, m_columnIndeces[c]);

  m_newRows.clear();
}

//-----------------------------------------------------------------------------

ReframeUndo::~ReframeUndo() {}

//-----------------------------------------------------------------------------

void ReframeUndo::undo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  int rowCount = m_r1 - m_r0;
  if (rowCount < 0 || m_columnIndeces.size() < 1) return;

  for (int c = 0; c < m_columnIndeces.size(); c++) {
    /*-- コマンド後に縮んだカラムはその分引き伸ばす --*/
    if (m_newRows[c] < m_nr)
      xsh->insertCells(m_r0 + m_newRows[c], m_columnIndeces[c],
                       m_nr - m_newRows[c]);
    /*-- コマンド後に延びたカラムはその分縮める --*/
    else if (m_newRows[c] > m_nr)
      xsh->removeCells(m_r1 + 1, m_columnIndeces[c], m_newRows[c] - m_nr);
  }

  if (m_cells) {
    int k = 0;
    for (int r = m_r0; r <= m_r1; r++)
      for (int c = 0; c < m_columnIndeces.size(); c++) {
        if (m_cells[k].isEmpty())
          xsh->clearCells(r, m_columnIndeces[c]);
        else
          xsh->setCell(r, m_columnIndeces[c], m_cells[k]);
        k++;
      }
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ReframeUndo::redo() const {
  if (m_r1 - m_r0 < 0 || m_columnIndeces.size() < 1) return;

  TApp *app = TApp::instance();

  for (int c = 0; c < m_columnIndeces.size(); c++)
    app->getCurrentXsheet()->getXsheet()->reframeCells(
        m_r0, m_r1, m_columnIndeces[c], m_type, m_withBlank);

  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ReframeUndo::repeat() const {}

}  // namespace

//=============================================================================

void TCellSelection::reframeCells(int count) {
  if (isEmpty() || areAllColSelectedLocked()) return;

  std::vector<int> colIndeces;
  for (int c = m_range.m_c0; c <= m_range.m_c1; c++) colIndeces.push_back(c);

  ReframeUndo *undo =
      new ReframeUndo(m_range.m_r0, m_range.m_r1, colIndeces, count);

  for (int c = m_range.m_c0; c <= m_range.m_c1; c++) {
    int nrows = TApp::instance()->getCurrentXsheet()->getXsheet()->reframeCells(
        m_range.m_r0, m_range.m_r1, c, count);
    undo->m_newRows.push_back(nrows);
  }

  TUndoManager::manager()->add(undo);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void TColumnSelection::reframeCells(int count) {
  if (isEmpty()) return;

  int rowCount =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount();
  std::vector<int> colIndeces;
  std::set<int>::const_iterator it;
  for (it = m_indices.begin(); it != m_indices.end(); it++)
    colIndeces.push_back(*it);

  ReframeUndo *undo = new ReframeUndo(0, rowCount - 1, colIndeces, count);

  for (int c = 0; c < (int)colIndeces.size(); c++) {
    int nrows = TApp::instance()->getCurrentXsheet()->getXsheet()->reframeCells(
        0, rowCount - 1, colIndeces[c], count);
    undo->m_newRows.push_back(nrows);
  }

  TUndoManager::manager()->add(undo);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================

void TCellSelection::reframeWithEmptyInbetweens() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  std::vector<int> colIndeces;
  for (int c = m_range.m_c0; c <= m_range.m_c1; c++) colIndeces.push_back(c);

  // destruction of m_reframePopup will be done along with the main window
  if (!m_reframePopup) m_reframePopup = new ReframePopup();
  int ret                             = m_reframePopup->exec();
  if (ret == QDialog::Rejected) return;

  int step, blank;
  m_reframePopup->getValues(step, blank);

  ReframeUndo *undo =
      new ReframeUndo(m_range.m_r0, m_range.m_r1, colIndeces, step, blank);

  int maximumRow = 0;
  for (int c = m_range.m_c0; c <= m_range.m_c1; c++) {
    int nrows = TApp::instance()->getCurrentXsheet()->getXsheet()->reframeCells(
        m_range.m_r0, m_range.m_r1, c, step, blank);
    undo->m_newRows.push_back(nrows);
    if (maximumRow < nrows) maximumRow = nrows;
  }

  if (maximumRow == 0) {
    delete undo;
    return;
  }

  TUndoManager::manager()->add(undo);

  // select reframed range
  selectCells(m_range.m_r0, m_range.m_c0, m_range.m_r0 + maximumRow - 1,
              m_range.m_c1);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void TColumnSelection::reframeWithEmptyInbetweens() {
  if (isEmpty()) return;

  int rowCount =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount();
  std::vector<int> colIndeces;
  std::set<int>::const_iterator it;
  for (it = m_indices.begin(); it != m_indices.end(); it++)
    colIndeces.push_back(*it);

  if (!m_reframePopup) m_reframePopup = new ReframePopup();
  int ret                             = m_reframePopup->exec();
  if (ret == QDialog::Rejected) return;

  int step, blank;
  m_reframePopup->getValues(step, blank);

  ReframeUndo *undo = new ReframeUndo(0, rowCount - 1, colIndeces, step, blank);

  bool commandExecuted = false;
  for (int c = 0; c < (int)colIndeces.size(); c++) {
    int nrows = TApp::instance()->getCurrentXsheet()->getXsheet()->reframeCells(
        0, rowCount - 1, colIndeces[c], step, blank);
    undo->m_newRows.push_back(nrows);
    if (nrows > 0) commandExecuted = true;
  }

  if (!commandExecuted) {
    delete undo;
    return;
  }

  TUndoManager::manager()->add(undo);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//*********************************************************************************
//    Reset Step Cells  command
//*********************************************************************************

namespace {

class ResetStepUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  int m_rowsCount, m_colsCount;

  tcg::unique_ptr<TXshCell[]> m_cells;
  QMap<int, int> m_insertedCells;  //!< Count of inserted cells, by column

public:
  ResetStepUndo(int r0, int c0, int r1, int c1);

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }
};

//-----------------------------------------------------------------------------

ResetStepUndo::ResetStepUndo(int r0, int c0, int r1, int c1)
    : m_r0(r0)
    , m_c0(c0)
    , m_r1(r1)
    , m_c1(c1)
    , m_rowsCount(m_r1 - m_r0 + 1)
    , m_colsCount(m_c1 - m_c0 + 1)
    , m_cells(new TXshCell[m_rowsCount * m_colsCount]) {
  assert(m_rowsCount > 0 && m_colsCount > 0);
  assert(m_cells.get());

  TApp *app = TApp::instance();

  int k = 0;
  for (int c = c0; c <= c1; ++c) {
    TXshCell prevCell;
    m_insertedCells[c] = 0;

    for (int r = r0; r <= r1; ++r) {
      const TXshCell &cell =
          app->getCurrentXsheet()->getXsheet()->getCell(r, c);
      m_cells[k++] = cell;

      if (prevCell != cell) {
        prevCell = cell;
        m_insertedCells[c]++;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void ResetStepUndo::redo() const {
  TCG_ASSERT(m_rowsCount > 0 && m_colsCount > 0, return );

  TApp::instance()->getCurrentXsheet()->getXsheet()->resetStepCells(m_r0, m_c0,
                                                                    m_r1, m_c1);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void ResetStepUndo::undo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int k = 0;
  for (int c = m_c0; c <= m_c1; ++c) {
    xsh->removeCells(m_r0, c, m_insertedCells[c]);

    xsh->insertCells(m_r0, c, m_rowsCount);
    for (int r = m_r0; r <= m_r1; ++r) xsh->setCell(r, c, m_cells[k++]);
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::resetStepCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  TUndo *undo =
      new ResetStepUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Increase Step Cells  command
//*********************************************************************************

namespace {

class IncreaseStepUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  int m_rowsCount, m_colsCount;

  tcg::unique_ptr<TXshCell[]> m_cells;
  QMap<int, int> m_insertedCells;

public:
  mutable int m_newR1;  //!< r1 updated by TXsheet::increaseStepCells()

public:
  IncreaseStepUndo(int r0, int c0, int r1, int c1);

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }
};

//-----------------------------------------------------------------------------

IncreaseStepUndo::IncreaseStepUndo(int r0, int c0, int r1, int c1)
    : m_r0(r0)
    , m_c0(c0)
    , m_r1(r1)
    , m_c1(c1)
    , m_rowsCount(m_r1 - m_r0 + 1)
    , m_colsCount(m_c1 - m_c0 + 1)
    , m_cells(new TXshCell[m_rowsCount * m_colsCount])
    , m_newR1(m_r1) {
  assert(m_cells.get());

  int k = 0;
  for (int c = c0; c <= c1; ++c) {
    TXshCell prevCell;
    m_insertedCells[c] = 0;

    for (int r = r0; r <= r1; ++r) {
      const TXshCell &cell =
          TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r, c);
      m_cells[k++] = cell;

      if (prevCell != cell) {
        prevCell = cell;
        m_insertedCells[c]++;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void IncreaseStepUndo::redo() const {
  m_newR1 = m_r1;
  TApp::instance()->getCurrentXsheet()->getXsheet()->increaseStepCells(
      m_r0, m_c0, m_newR1, m_c1);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void IncreaseStepUndo::undo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int k = 0;
  for (int c = m_c0; c <= m_c1; ++c) {
    xsh->removeCells(m_r0, c, m_rowsCount + m_insertedCells[c]);

    xsh->insertCells(m_r0, c, m_rowsCount);
    for (int r = m_r0; r <= m_r1; ++r) xsh->setCell(r, c, m_cells[k++]);
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::increaseStepCells() {
  if (isEmpty() || areAllColSelectedLocked()) return;

  IncreaseStepUndo *undo = new IncreaseStepUndo(m_range.m_r0, m_range.m_c0,
                                                m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();

  if (undo->m_newR1 != m_range.m_r1) {
    m_range.m_r1 = undo->m_newR1;
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
}

//*********************************************************************************
//    Decrease Step Cells  command
//*********************************************************************************

namespace {

class DecreaseStepUndo final : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;
  int m_rowsCount, m_colsCount;

  tcg::unique_ptr<TXshCell[]> m_cells;
  QMap<int, int> m_removedCells;

public:
  mutable int m_newR1;  //!< r1 updated by TXsheet::decreaseStepCells()

public:
  DecreaseStepUndo(int r0, int c0, int r1, int c1);

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }
};

//-----------------------------------------------------------------------------

DecreaseStepUndo::DecreaseStepUndo(int r0, int c0, int r1, int c1)
    : m_r0(r0)
    , m_c0(c0)
    , m_r1(r1)
    , m_c1(c1)
    , m_rowsCount(m_r1 - m_r0 + 1)
    , m_colsCount(m_c1 - m_c0 + 1)
    , m_cells(new TXshCell[m_rowsCount * m_colsCount])
    , m_newR1(m_r1) {
  assert(m_cells.get());

  int k = 0;
  for (int c = c0; c <= c1; ++c) {
    TXshCell prevCell =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r0, c);
    m_removedCells[c] = 0;

    bool removed = false;
    m_cells[k++] = prevCell;

    for (int r = r0 + 1; r <= r1; ++r) {
      const TXshCell &cell =
          TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r, c);
      m_cells[k++] = cell;

      if (prevCell == cell) {
        if (!removed) {
          removed = true;
          m_removedCells[c]++;
        }
      } else {
        removed  = false;
        prevCell = cell;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void DecreaseStepUndo::redo() const {
  m_newR1 = m_r1;
  TApp::instance()->getCurrentXsheet()->getXsheet()->decreaseStepCells(
      m_r0, m_c0, m_newR1, m_c1);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void DecreaseStepUndo::undo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int k = 0;
  for (int c = m_c0; c <= m_c1; ++c) {
    xsh->removeCells(m_r0, c, m_rowsCount - m_removedCells[c]);

    xsh->insertCells(m_r0, c, m_rowsCount);
    for (int r = m_r0; r <= m_r1; ++r) xsh->setCell(r, c, m_cells[k++]);
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
}

}  // namespace

//=============================================================================

void TCellSelection::decreaseStepCells() {
  DecreaseStepUndo *undo = new DecreaseStepUndo(m_range.m_r0, m_range.m_c0,
                                                m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();

  if (undo->m_newR1 != m_range.m_r1) {
    m_range.m_r1 = undo->m_newR1;
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
}

//*********************************************************************************
//    Rollup Cells  command
//*********************************************************************************

namespace {

class RollupUndo : public TUndo {
  int m_r0, m_c0, m_r1, m_c1;

public:
  RollupUndo(int r0, int c0, int r1, int c1)
      : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1) {}

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    xsh->rollupCells(m_r0, m_c0, m_r1, m_c1);

    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    xsh->rolldownCells(m_r0, m_c0, m_r1, m_c1);

    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Roll Up"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//=============================================================================

void TCellSelection::rollupCells() {
  TUndo *undo =
      new RollupUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Rolldown Cells  command
//*********************************************************************************

namespace {

class RolldownUndo final : public RollupUndo {
public:
  RolldownUndo(int r0, int c0, int r1, int c1) : RollupUndo(r0, c0, r1, c1) {}

  void redo() const override { RollupUndo::undo(); }
  void undo() const override { RollupUndo::redo(); }

  QString getHistoryString() override { return QObject::tr("Roll Down"); }
};

}  // namespace

//=============================================================================

void TCellSelection::rolldownCells() {
  TUndo *undo =
      new RolldownUndo(m_range.m_r0, m_range.m_c0, m_range.m_r1, m_range.m_c1);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//*********************************************************************************
//    Set Keyframes  command
//*********************************************************************************

void TCellSelection::setKeyframes() {
  if (isEmpty()) return;

  // Preliminary data-fetching
  TApp *app = TApp::instance();

  TXsheetHandle *xshHandle = app->getCurrentXsheet();
  TXsheet *xsh             = xshHandle->getXsheet();

  int row = m_range.m_r0, col = m_range.m_c0;

  const TXshCell &cell = xsh->getCell(row, col);
  if (cell.getSoundLevel() || cell.getSoundTextLevel()) return;

  const TStageObjectId &id = TStageObjectId::ColumnId(col);

  TStageObject *obj = xsh->getStageObject(id);
  if (!obj) return;

  // Command body
  if (obj->isFullKeyframe(row)) {
    const TStageObject::Keyframe &key = obj->getKeyframe(row);

    UndoRemoveKeyFrame *undo = new UndoRemoveKeyFrame(id, row, key, xshHandle);
    undo->setObjectHandle(app->getCurrentObject());

    TUndoManager::manager()->add(undo);
    undo->redo();
  } else {
    UndoSetKeyFrame *undo = new UndoSetKeyFrame(id, row, xshHandle);
    undo->setObjectHandle(app->getCurrentObject());

    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  TApp::instance()->getCurrentScene()->setDirtyFlag(
      true);  // Should be moved inside the undos!
}

//*********************************************************************************
//    Clone Level  command
//*********************************************************************************

namespace {

class CloneLevelUndo final : public TUndo {
  typedef std::map<TXshSimpleLevel *, TXshLevelP> InsertedLevelsMap;
  typedef std::set<int> InsertedColumnsSet;

  struct ExistsFunc;
  class LevelNamePopup;

private:
  TCellSelection::Range m_range;

  mutable InsertedLevelsMap m_insertedLevels;
  mutable InsertedColumnsSet m_insertedColumns;
  mutable bool m_clonedLevels;

public:
  mutable bool m_ok;

public:
  CloneLevelUndo(const TCellSelection::Range &range)
      : m_range(range), m_clonedLevels(false), m_ok(false) {}

  void redo() const override;
  void undo() const override;

  int getSize() const override {
    return sizeof *this +
           (sizeof(TXshLevelP) + sizeof(TXshSimpleLevel *)) *
               m_insertedLevels.size();
  }

  QString getHistoryString() override {
    if (m_insertedLevels.empty()) return QString();
    QString str;
    if (m_insertedLevels.size() == 1) {
      str = QObject::tr("Clone  Level : %1 > %2")
                .arg(QString::fromStdWString(
                    m_insertedLevels.begin()->first->getName()))
                .arg(QString::fromStdWString(
                    m_insertedLevels.begin()->second->getName()));
    } else {
      str = QObject::tr("Clone  Levels : ");
      std::map<TXshSimpleLevel *, TXshLevelP>::const_iterator it =
          m_insertedLevels.begin();
      for (; it != m_insertedLevels.end(); ++it) {
        str += QString("%1 > %2, ")
                   .arg(QString::fromStdWString(it->first->getName()))
                   .arg(QString::fromStdWString(it->second->getName()));
      }
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::Xsheet; }

private:
  TXshSimpleLevel *cloneLevel(const TXshSimpleLevel *srcSl,
                              const TFilePath &dstPath,
                              const std::set<TFrameId> &frames) const;

  bool chooseLevelName(TFilePath &fp) const;
  bool chooseOverwrite(OverwriteDialog *dialog, TFilePath &dstPath,
                       TXshSimpleLevel *&dstSl) const;

  void cloneLevels() const;
  void insertLevels() const;
  void insertCells() const;
};

//-----------------------------------------------------------------------------

struct CloneLevelUndo::ExistsFunc final : public OverwriteDialog::ExistsFunc {
  ToonzScene *m_scene;

public:
  ExistsFunc(ToonzScene *scene) : m_scene(scene) {}

  QString conflictString(const TFilePath &fp) const override {
    return OverwriteDialog::tr(
               "Level \"%1\" already exists.\n\nWhat do you want to do?")
        .arg(QString::fromStdWString(fp.withoutParentDir().getWideString()));
  }

  bool operator()(const TFilePath &fp) const override {
    return TSystem::doesExistFileOrLevel(fp) ||
           m_scene->getLevelSet()->getLevel(*m_scene, fp);
  }
};

//-----------------------------------------------------------------------------

class CloneLevelUndo::LevelNamePopup final : public DVGui::Dialog {
  DVGui::LineEdit *m_name;
  QPushButton *m_ok, *m_cancel;

public:
  LevelNamePopup(const std::wstring &defaultLevelName)
      : DVGui::Dialog(TApp::instance()->getMainWindow(), true, true,
                      "Clone Level") {
    setWindowTitle(tr("Clone Level"));

    beginHLayout();

    QLabel *label = new QLabel(tr("Level Name:"));
    addWidget(label);

    m_name = new DVGui::LineEdit;
    addWidget(m_name);

    m_name->setText(QString::fromStdWString(defaultLevelName));

    endHLayout();

    m_ok     = new QPushButton(QObject::tr("Ok"));
    m_cancel = new QPushButton(QObject::tr("Cancel"));
    addButtonBarWidget(m_ok, m_cancel);

    m_ok->setDefault(true);

    connect(m_ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_cancel, SIGNAL(clicked()), this, SLOT(reject()));
  }

  QString getName() const { return m_name->text(); }
};

//-----------------------------------------------------------------------------

TXshSimpleLevel *CloneLevelUndo::cloneLevel(
    const TXshSimpleLevel *srcSl, const TFilePath &dstPath,
    const std::set<TFrameId> &frames) const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  int levelType = srcSl->getType();
  assert(levelType > 0);

  const std::wstring &dstName = dstPath.getWideName();

  TXshSimpleLevel *dstSl =
      scene->createNewLevel(levelType, dstName)->getSimpleLevel();

  assert(dstSl);
  dstSl->setPath(scene->codeFilePath(dstPath));
  dstSl->setName(dstName);
  dstSl->clonePropertiesFrom(srcSl);
  *dstSl->getHookSet() = *srcSl->getHookSet();

  if (levelType == TZP_XSHLEVEL || levelType == PLI_XSHLEVEL) {
    TPalette *palette = srcSl->getPalette();
    assert(palette);

    dstSl->setPalette(palette->clone());
    dstSl->getPalette()->setDirtyFlag(true);
  }

  // The level clone shell was created. Now, clone the associated frames found
  // in the selection
  std::set<TFrameId>::const_iterator ft, fEnd(frames.end());
  for (ft = frames.begin(); ft != fEnd; ++ft) {
    const TFrameId &fid = *ft;

    TImageP img = srcSl->getFullsampledFrame(*ft, ImageManager::dontPutInCache);
    if (!img) continue;

    dstSl->setFrame(*ft, img->cloneImage());
  }

  dstSl->setDirtyFlag(true);

  return dstSl;
}

//-----------------------------------------------------------------------------

bool CloneLevelUndo::chooseLevelName(TFilePath &fp) const {
  std::unique_ptr<LevelNamePopup> levelNamePopup(
      new LevelNamePopup(fp.getWideName()));
  if (levelNamePopup->exec() == QDialog::Accepted) {
    const QString &levelName = levelNamePopup->getName();

    if (isValidFileName_message(levelName)) {
      fp = fp.withName(levelName.toStdWString());
      return true;
    }
  }

  return false;
}

//-----------------------------------------------------------------------------

bool CloneLevelUndo::chooseOverwrite(OverwriteDialog *dialog,
                                     TFilePath &dstPath,
                                     TXshSimpleLevel *&dstSl) const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  ExistsFunc exists(scene);

  OverwriteDialog::Resolution acceptedRes = OverwriteDialog::ALL_RESOLUTIONS;

  TXshLevel *xl = scene->getLevelSet()->getLevel(*scene, dstPath);
  if (xl)
    acceptedRes =
        OverwriteDialog::Resolution(acceptedRes & ~OverwriteDialog::OVERWRITE);

  // Apply user's decision
  switch (dialog->execute(dstPath, exists, acceptedRes,
                          OverwriteDialog::APPLY_TO_ALL_FLAG)) {
  default:
    return false;

  case OverwriteDialog::KEEP_OLD:
    // Load the level at the preferred clone path
    if (!xl) xl = scene->loadLevel(dstPath);  // Hard load - from disk

    assert(xl);
    dstSl = xl->getSimpleLevel();
    break;

  case OverwriteDialog::OVERWRITE:
    assert(!xl);
    break;

  case OverwriteDialog::RENAME:
    break;
  }

  return true;
}

//-----------------------------------------------------------------------------

void CloneLevelUndo::cloneLevels() const {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();

  // Retrieve the simple levels and associated frames in the specified range
  typedef std::set<TFrameId> FramesSet;
  typedef std::map<TXshSimpleLevel *, FramesSet> LevelsMap;

  LevelsMap levels;
  getSelectedFrames(*xsh, m_range.m_r0, m_range.m_c0, m_range.m_r1,
                    m_range.m_c1, levels);

  if (!levels.empty()) {
    bool askCloneName = (levels.size() == 1);

    // Now, try to clone every found level in the associated range
    std::unique_ptr<OverwriteDialog> dialog;
    ExistsFunc exists(scene);

    LevelsMap::iterator lt, lEnd(levels.end());
    for (lt = levels.begin(); lt != lEnd; ++lt) {
      assert(lt->first && !lt->second.empty());

      TXshSimpleLevel *srcSl = lt->first;
      if (srcSl->getPath().getType() == "psd") continue;

      const TFilePath &srcPath = srcSl->getPath();

      // Build the destination level data
      TXshSimpleLevel *dstSl = 0;
      TFilePath dstPath      = scene->decodeFilePath(
          srcPath.withName(srcPath.getWideName() + L"_clone"));

      // Ask user to suggest an appropriate level name
      if (askCloneName && !chooseLevelName(dstPath)) continue;

      // Get a unique level path
      if (exists(dstPath)) {
        // Ask user for action
        if (!dialog.get()) dialog.reset(new OverwriteDialog);

        if (!chooseOverwrite(dialog.get(), dstPath, dstSl)) continue;
      }

      // If the destination level was not retained from existing data, it must
      // be created and cloned
      if (!dstSl) dstSl = cloneLevel(srcSl, dstPath, lt->second);

      assert(dstSl);
      m_insertedLevels[srcSl] = dstSl;
    }
  }
}

//-----------------------------------------------------------------------------

void CloneLevelUndo::insertLevels() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  InsertedLevelsMap::iterator lt, lEnd = m_insertedLevels.end();
  for (lt = m_insertedLevels.begin(); lt != lEnd; ++lt)
    scene->getLevelSet()->insertLevel(lt->second.getPointer());
}

//-----------------------------------------------------------------------------

void CloneLevelUndo::insertCells() const {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  m_insertedColumns.clear();

  // If necessary, insert blank columns AFTER the columns range.
  // Remember the indices of inserted columns, too.
  for (int c = 1; c <= m_range.getColCount(); ++c) {
    int colIndex = m_range.m_c1 + c;
    if (xsh->isColumnEmpty(
            colIndex))  // If there already is a hole, no need to insert -
      continue;         // we'll just use it.

    xsh->insertColumn(colIndex);
    m_insertedColumns.insert(colIndex);
  }

  // Now, re-traverse the selected range, and add corresponding cells
  // in the destination range
  for (int c = m_range.m_c0; c <= m_range.m_c1; ++c) {
    for (int r = m_range.m_r0; r <= m_range.m_r1; ++r) {
      const TXshCell &srcCell = xsh->getCell(r, c);
      if (TXshSimpleLevel *srcSl = srcCell.getSimpleLevel()) {
        std::map<TXshSimpleLevel *, TXshLevelP>::iterator lt =
            m_insertedLevels.find(srcSl);
        if (lt != m_insertedLevels.end()) {
          TXshCell dstCell(lt->second, srcCell.getFrameId());
          xsh->setCell(r, c + m_range.getColCount(), dstCell);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

void CloneLevelUndo::redo() const {
  if (m_clonedLevels)
    insertLevels();
  else {
    m_clonedLevels = true;
    cloneLevels();
  }

  if (m_insertedLevels.empty()) return;

  // Command succeeded, let's deal with the xsheet
  m_ok = true;
  insertCells();

  // Finally, emit notifications
  TApp *app = TApp::instance();

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifyCastChange();
}

//-----------------------------------------------------------------------------

void CloneLevelUndo::undo() const {
  assert(!m_insertedLevels.empty());

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  TXsheet *xsh    = scene->getXsheet();
  TXsheet *topXsh = scene->getChildStack()->getTopXsheet();

  // Erase inserted columns from the xsheet
  for (int i = m_range.getColCount(); i > 0; --i) {
    int index                        = m_range.m_c1 + i;
    std::set<int>::const_iterator it = m_insertedColumns.find(index);
    xsh->removeColumn(index);
    if (it == m_insertedColumns.end()) xsh->insertColumn(index);
  }

  // Attempt removal of inserted columns from the cast
  // NOTE: Cloned levels who were KEEP_OLD'd may have already been present in
  // the cast

  std::map<TXshSimpleLevel *, TXshLevelP>::const_iterator lt,
      lEnd = m_insertedLevels.end();
  for (lt = m_insertedLevels.begin(); lt != lEnd; ++lt) {
    if (!topXsh->isLevelUsed(lt->second.getPointer()))
      scene->getLevelSet()->removeLevel(lt->second.getPointer());
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifyCastChange();
}

}  // namespace

//-----------------------------------------------------------------------------

void TCellSelection::cloneLevel() {
  std::unique_ptr<CloneLevelUndo> undo(new CloneLevelUndo(m_range));

  if (undo->redo(), undo->m_ok) TUndoManager::manager()->add(undo.release());
}
