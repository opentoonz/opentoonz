

#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/toonzscene.h"
#include "toonz/txshcell.h"
#include "toonz/txsheet.h"
#include "toonz/fxdag.h"
#include "toonz/txshzeraryfxlevel.h"

#include "tstream.h"

//=============================================================================
// TXshZeraryFxColumn

TXshZeraryFxColumn::TXshZeraryFxColumn(int frameCount)
    : m_zeraryColumnFx(new TZeraryColumnFx())
    , m_zeraryFxLevel(new TXshZeraryFxLevel()) {
  m_zeraryColumnFx->addRef();
  m_zeraryColumnFx->setColumn(this);
  m_zeraryFxLevel->addRef();
  m_zeraryFxLevel->setColumn(this);
  for (int i = 0; i < frameCount; i++)
    setCell(i, TXshCell(m_zeraryFxLevel, TFrameId(1)));
}

//-----------------------------------------------------------------------------

TXshZeraryFxColumn::TXshZeraryFxColumn(const TXshZeraryFxColumn &src)
    : m_zeraryColumnFx(new TZeraryColumnFx())
    , m_zeraryFxLevel(new TXshZeraryFxLevel()) {
  m_zeraryColumnFx->addRef();
  m_zeraryColumnFx->setColumn(this);
  m_zeraryFxLevel->addRef();
  m_zeraryFxLevel->setColumn(this);
  m_first = src.m_first;
  int i;
  for (i = 0; i < (int)src.m_cells.size(); i++)
    m_cells.push_back(TXshCell(m_zeraryFxLevel, src.m_cells[i].getFrameId()));
  assert((int)src.m_cells.size() == (int)m_cells.size());
  TFx *fx = src.getZeraryColumnFx()->getZeraryFx();
  if (fx) {
    std::wstring fxName = fx->getName();
    fx                  = fx->clone(false);
    fx->setName(fxName);
    m_zeraryColumnFx->setZeraryFx(fx);
  }
}

//-----------------------------------------------------------------------------

TXshZeraryFxColumn::~TXshZeraryFxColumn() {
  m_zeraryColumnFx->setColumn(0);
  m_zeraryColumnFx->release();
  m_zeraryFxLevel->release();
}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshZeraryFxColumn::getColumnType() const {
  return eZeraryFxType;
}

//-----------------------------------------------------------------------------

bool TXshZeraryFxColumn::canSetCell(const TXshCell &cell) const {
  return cell.isEmpty() || cell.m_level->getZeraryFxLevel() != 0;
}

//-----------------------------------------------------------------------------

TXshColumn *TXshZeraryFxColumn::clone() const {
  return new TXshZeraryFxColumn(*this);
}

//-----------------------------------------------------------------------------

TFx *TXshZeraryFxColumn::getFx() const { return m_zeraryColumnFx; }

//-----------------------------------------------------------------------------

bool TXshZeraryFxColumn::setCell(int row, const TXshCell &cell) {
  if (cell.isEmpty()) return false;
  TXshCell newCell = cell;
  // Sto settando delle celle in una colonna nuova, devo settare anche
  // l'effetto.
  if (isEmpty() && getZeraryColumnFx()->getZeraryFx() == 0) {
    newCell                      = TXshCell(m_zeraryFxLevel, cell.getFrameId());
    TXshZeraryFxLevel *fxLevel   = cell.m_level->getZeraryFxLevel();
    TXshZeraryFxColumn *fxColumn = fxLevel->getColumn();
    m_zeraryColumnFx->setZeraryFx(fxColumn->getZeraryColumnFx()->getZeraryFx());
  }

  return TXshCellColumn::setCell(row, newCell);
}

//-----------------------------------------------------------------------------

bool TXshZeraryFxColumn::setCells(int row, int rowCount,
                                  const TXshCell cells[]) {
  std::vector<TXshCell> newCells;
  bool isEmptyColumn = isEmpty() && getZeraryColumnFx()->getZeraryFx() == 0;
  int i;
  for (i = 0; i < rowCount; i++) {
    if (isEmptyColumn)
      newCells.push_back(TXshCell(m_zeraryFxLevel, cells[i].getFrameId()));
    else
      newCells.push_back(cells[i]);
  }
  // Sto settando delle celle in una colonna nuova, devo settare anche
  // l'effetto.
  if (isEmptyColumn) {
    i = 0;
    while (i < rowCount && cells[i].isEmpty()) i++;
    if (i >= rowCount) return false;
    TXshZeraryFxLevel *fxLevel =
        dynamic_cast<TXshZeraryFxLevel *>(cells[i].m_level.getPointer());
    TXshZeraryFxColumn *fxColumn = fxLevel->getColumn();
    m_zeraryColumnFx->setZeraryFx(fxColumn->getZeraryColumnFx()->getZeraryFx());
  }
  return TXshCellColumn::setCells(row, rowCount, &newCells[0]);
}

//-----------------------------------------------------------------------------

void TXshZeraryFxColumn::loadData(TIStream &is) {
  TPersist *p = 0;
  is >> p;
  if (!p) return;

  TZeraryColumnFx *fx = dynamic_cast<TZeraryColumnFx *>(p);
  fx->addRef();
  if (m_zeraryColumnFx) {
    m_zeraryColumnFx->setColumn(0);
    m_zeraryColumnFx->release();
  }
  m_zeraryColumnFx = fx;
  m_zeraryColumnFx->setColumn(this);

  int r0, r1;
  bool touched = false;
  const TXshCell cell(m_zeraryFxLevel, TFrameId(1));
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "status") {
      int status;
      is >> status;
      setStatusWord(status);
    } else if (tagName == "cells") {
      while (is.matchTag(tagName)) {
        if (tagName == "cell") {
          if (!touched) {
            touched = true;
            if (getRange(r0, r1)) removeCells(r0, r1 - r0 + 1);
          }
          int r, n;
          is >> r >> n;
          for (int i = 0; i < n; i++) setCell(r++, cell);
        } else
          throw TException("expected <cell>");
        is.closeChild();
      }
    } else
      throw TException("expected <status> or <cells>");
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshZeraryFxColumn::saveData(TOStream &os) {
  os << m_zeraryColumnFx;
  os.child("status") << getStatusWord();
  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = getCell(r);
      if (cell.isEmpty()) continue;
      int n = 1;
      while (r + n <= r1 && !getCell(r + n).isEmpty()) n++;
      os.child("cell") << r << n;
      r += n - 1;
    }
    os.closeChild();
  }
}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TXshZeraryFxColumn, "zeraryFxColumn")
