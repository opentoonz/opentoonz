#pragma once

#ifndef CELLDATA_INCLUDED
#define CELLDATA_INCLUDED

#include "toonzqt/dvmimedata.h"

#include "toonz/txshcell.h"
#include "tsound.h"

//=============================================================================
// forward declarations
class TXsheet;
class TXshColumn;

//=============================================================================
// TCellData
//-----------------------------------------------------------------------------

class TCellData final : public DvMimeData {
  std::vector<TXshCell> m_cells;
  int m_rowCount, m_colCount;

public:
  TCellData();
  TCellData(const TCellData *src);
  ~TCellData();

  int getRowCount() const { return m_rowCount; }
  int getColCount() const { return m_colCount; }
  int getCellCount() const { return m_cells.size(); }

  const TXshCell getCell(int index) const { return m_cells[index]; }
  const TXshCell getCell(int row, int col) const;

  TCellData *clone() const override { return new TCellData(this); }

  // data <- xsh
  void setCells(TXsheet *xsh, int r0, int c0, int r1, int c1);

  // data -> xsh;
  /*! If insert == true insert cells and shift old one.
If column type don't match (sound vs not sound) don't set column cells.
If doZeraryClone == true clone zerary cells fx.
If skipEmptyCells == false do not skip setting empty cells in data*/
  bool getCells(TXsheet *xsh, int r0, int c0, int &r1, int &c1,
                bool insert = true, bool doZeraryClone = true,
                bool skipEmptyCells = true) const;

  // Paste only cell numbers.
  // As a special behavior, enable to copy one column and paste into
  // multiple columns.
  bool getNumbers(TXsheet *xsh, int r0, int c0, int &r1, int &c1) const;

  //! Return true if cell in TCellData can be set in \b xsh xsheet.
  bool canChange(TXsheet *xsh, int c0) const;

protected:
  bool canChange(TXshColumn *column, int index) const;
  void cloneZeraryFx(int index, std::vector<TXshCell> &cells) const;
};

#endif
