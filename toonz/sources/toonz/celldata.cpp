

#include "celldata.h"
#include <assert.h>

#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshzeraryfxlevel.h"
#include "toonz/tcolumnfx.h"
#include "toonz/fxdag.h"
#include "toonz/txshlevelcolumn.h"

//-----------------------------------------------------------------------------

TCellData::TCellData() : m_rowCount(0), m_colCount(0) {}

//-----------------------------------------------------------------------------

TCellData::TCellData(const TCellData *src)
    : m_cells(src->m_cells)
    , m_rowCount(src->m_rowCount)
    , m_colCount(src->m_colCount) {}

//-----------------------------------------------------------------------------

TCellData::~TCellData() {}

//-----------------------------------------------------------------------------

const TXshCell TCellData::getCell(int row, int col) const {
  assert(0 <= row && row < m_rowCount);
  assert(0 <= col && col < m_colCount);
  return m_cells[row + col * m_rowCount];
}

//-----------------------------------------------------------------------------

// data <- xsh
void TCellData::setCells(TXsheet *xsh, int r0, int c0, int r1, int c1) {
  assert(r0 <= r1 && c0 <= c1);
  m_rowCount = r1 - r0 + 1;
  m_colCount = c1 - c0 + 1;
  assert(m_rowCount > 0);
  assert(m_colCount > 0);
  m_cells.clear();

  m_cells.resize(m_rowCount * m_colCount);

  int c;
  for (c = c0; c <= c1; c++) {
    int index          = c - c0;
    TXshColumn *column = xsh->getColumn(c);
    if (!column || column->isEmpty()) continue;
    xsh->getCells(r0, c, m_rowCount, &m_cells[index * m_rowCount]);
  }
  setText("TCellData");
}

//-----------------------------------------------------------------------------

// data -> xsh
bool TCellData::getCells(TXsheet *xsh, int r0, int c0, int &r1, int &c1,
                         bool insert, bool doZeraryClone,
                         bool skipEmptyCells) const {
  int c;
  r1           = r0 + m_rowCount - 1;
  c1           = c0 + m_colCount - 1;
  bool cellSet = false;
  for (c = c0; c < c0 + m_colCount; c++) {
    TXshColumn *column = xsh->getColumn(c);
    /*- index:選択範囲左から何番目のカラムか(一番左は0) -*/
    int index                   = c - c0;
    std::vector<TXshCell> cells = m_cells;
    bool isColumnEmpty          = true;
    if (column) {
      isColumnEmpty = column->isEmpty();
      /*- 各セルに左上→右下で順に割り振られるIndex -*/
      int cellIndex = index * m_rowCount;
      // increment the cellIndex and skip empty cells
      if (skipEmptyCells) {
        while (cellIndex < (index + 1) * m_rowCount &&
               m_cells[cellIndex].isEmpty())
          ++cellIndex;
      }
      // if the cellIndex reaches the end of the selection
      if ((int)m_cells.size() <= cellIndex)  // Celle vuote.
        return cellSet;
      /*- カラムが変更不可なら次のカラムへ -*/
      if (!canChange(column, index)) continue;
    }
    if (doZeraryClone && isColumnEmpty) cloneZeraryFx(index, cells);

    // inserisco le celle
    if (insert) xsh->insertCells(r0, c, m_rowCount);
    cellSet = xsh->setCells(r0, c, m_rowCount, &cells[index * m_rowCount]);
  }
  return cellSet;
}

//-----------------------------------------------------------------------------
// Paste only cell numbers.
// As a special behavior, enable to copy one column and paste into
// multiple columns.

bool TCellData::getNumbers(TXsheet *xsh, int r0, int c0, int &r1,
                           int &c1) const {
  r1                  = r0 + m_rowCount - 1;
  bool oneToMulti     = m_colCount == 1 && c0 < c1;
  if (!oneToMulti) c1 = c0 + m_colCount - 1;

  bool cellSet = false;

  for (int c = c0; c <= c1; c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (!column || column->isEmpty()) continue;
    TXshLevelColumn *levelColumn = column->getLevelColumn();
    if (!levelColumn) continue;

    std::vector<TXshCell> cells = m_cells;

    int sourceColIndex  = (oneToMulti) ? 0 : c - c0;
    int sourceCellIndex = sourceColIndex * m_rowCount;

    if (!canChange(column, sourceColIndex)) continue;

    bool isSet = levelColumn->setNumbers(r0, m_rowCount,
                                         &cells[sourceColIndex * m_rowCount]);

    cellSet = cellSet || isSet;
  }
  xsh->updateFrameCount();

  return cellSet;
}

//-----------------------------------------------------------------------------
/*-- c0 を起点に、TCellDataの選択範囲のカラム幅分、
        全てのカラムがcanChangeかどうか調べる。
--*/
bool TCellData::canChange(TXsheet *xsh, int c0) const {
  int c;
  for (c = c0; c < c0 + m_colCount; c++) {
    int index          = c - c0;
    TXshColumn *column = xsh->getColumn(c);
    if (!canChange(column, index)) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

bool TCellData::canChange(TXshColumn *column, int index) const {
  /*- カラムが無い、又は空のときTrue（編集可） -*/
  if (!column || column->isEmpty()) return true;
  /*- カラムがロックされていたらFalse（編集不可） -*/
  if (column->isLocked()) return false;
  /*-- 全てのセルがcanSetCellかどうか調べる 今からペーストするセルが、
          ペースト先のカラムと同種ならばtrueとなる。
  --*/
  TXshCellColumn *cellColumn = column->getCellColumn();
  assert(cellColumn);
  int i;
  for (i = index * m_rowCount; i < (index * m_rowCount) + m_rowCount; i++)
    // for(i = 0; i < m_cells.size(); i++)
    if (!cellColumn->canSetCell(m_cells[i])) return false;
  return true;
}

//-----------------------------------------------------------------------------

void TCellData::cloneZeraryFx(int index, std::vector<TXshCell> &cells) const {
  int firstNotEmptyIndex = index * m_rowCount;
  while (firstNotEmptyIndex < (index + 1) * m_rowCount &&
         m_cells[firstNotEmptyIndex].isEmpty())
    firstNotEmptyIndex++;
  if (firstNotEmptyIndex >= (index + 1) * m_rowCount) return;
  TXshZeraryFxLevel *fxLevel =
      m_cells[firstNotEmptyIndex].m_level->getZeraryFxLevel();
  if (!fxLevel) return;
  TXshZeraryFxColumn *fxColumn = fxLevel->getColumn();
  assert(fxColumn);
  TFx *zeraryFx = fxColumn->getZeraryColumnFx()->getZeraryFx();
  if (zeraryFx) {
    std::wstring fxName = zeraryFx->getName();
    TFx *newZeraryFx    = zeraryFx->clone(false);
    fxColumn->getXsheet()->getFxDag()->assignUniqueId(newZeraryFx);
    newZeraryFx->setName(fxName);
    newZeraryFx->linkParams(zeraryFx);
    TXshZeraryFxLevel *newFxLevel   = new TXshZeraryFxLevel();
    TXshZeraryFxColumn *newFxColumn = new TXshZeraryFxColumn(0);
    newFxColumn->getZeraryColumnFx()->setZeraryFx(newZeraryFx);
    newFxLevel->setColumn(newFxColumn);
    // replace the zerary fx cells by the new fx
    int r;
    for (r     = firstNotEmptyIndex; r < (index + 1) * m_rowCount; r++)
      cells[r] = TXshCell(newFxLevel, m_cells[r].getFrameId());
  }
}
