

#include "toonz/txshpalettecolumn.h"
#include "toonz/txshcell.h"
#include "toonz/tcolumnfx.h"
#include "tstream.h"

TXshPaletteColumn::TXshPaletteColumn()
    : TXshCellColumn(), m_fx(new TPaletteColumnFx()) {
  m_fx->addRef();
  m_fx->setColumn(this);
}

TXshPaletteColumn::~TXshPaletteColumn() {
  m_fx->setColumn(0);
  m_fx->release();
  m_fx = 0;
}

TXshColumn::ColumnType TXshPaletteColumn::getColumnType() const {
  return ePaletteType;
}

TXshColumn *TXshPaletteColumn::clone() const {
  TXshPaletteColumn *column = new TXshPaletteColumn();
  column->setStatusWord(getStatusWord());
  column->m_cells = m_cells;
  column->m_first = m_first;

  // column->updateIcon();
  return column;
}

TFx *TXshPaletteColumn::getFx() const { return m_fx; }

void TXshPaletteColumn::setFx(TFx *fx) {
  TPaletteColumnFx *pfx = dynamic_cast<TPaletteColumnFx *>(fx);
  assert(pfx);
  assert(m_fx);
  if (m_fx == pfx) return;
  pfx->addRef();
  m_fx->release();
  m_fx = pfx;
  pfx->setColumn(this);
}

bool TXshPaletteColumn::canSetCell(const TXshCell &cell) const {
  return cell.isEmpty() || cell.m_level->getPaletteLevel() != 0;
}

void TXshPaletteColumn::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "cells") {
      while (is.openChild(tagName)) {
        if (tagName == "cell") {
          TPersist *p = 0;
          int row = 1, rowCount = 1, drawing = 1, increment = 0;
          is >> row >> rowCount >> p >> drawing >> increment;
          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          if (xshLevel) {
            for (int i = 0; i < rowCount; i++) {
              TXshCell cell(xshLevel, drawing);
              setCell(row++, cell);
              drawing += increment;
            }
          }
        } else
          throw TException("TXshPaletteColumn, unknown tag(2): " + tagName);
        is.closeChild();
      }
    } else if (tagName == "fx") {
      TPersist *p = 0;
      is >> p;
      if (TFx *fx = dynamic_cast<TFx *>(p)) setFx(fx);
    } else {
      throw TException("TXshLevelColumn, unknown tag: " + tagName);
    }
    is.closeChild();
  }
}
void TXshPaletteColumn::saveData(TOStream &os) {
  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = getCell(r);
      if (cell.isEmpty()) continue;
      int n = 1, inc = 0, dr = cell.m_frameId.getNumber();
      if (r < r1) {
        TXshCell cell2 = getCell(r + 1);
        if (cell2.m_level.getPointer() == cell.m_level.getPointer()) {
          inc = cell2.m_frameId.getNumber() - dr;
          n++;
          for (;;) {
            if (r + n > r1) break;
            cell2 = getCell(r + n);
            if (cell2.m_level.getPointer() != cell.m_level.getPointer()) break;
            if (cell2.m_frameId.getNumber() != dr + n * inc) break;
            n++;
          }
        }
      }
      os.child("cell") << r << n << cell.m_level.getPointer() << dr << inc;
      r += n - 1;
    }
    os.closeChild();
  }
  os.child("fx") << m_fx;
}

PERSIST_IDENTIFIER(TXshPaletteColumn, "paletteColumn")
