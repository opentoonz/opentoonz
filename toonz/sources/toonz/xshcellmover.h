#pragma once

#ifndef XSHCELLMOVER_H
#define XSHCELLMOVER_H

#include "tgeometry.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "tundo.h"
#include <QMap>
#include "xsheetdragtool.h"

#include <set>

class StageObjectsData;
class TXsheet;
class LevelMoverUndo;

class CellsMover {
  // range of the moved cells
  int m_rowCount, m_colCount;
  // current and start top-left corner of the moved cells
  // (x=column, y=row)
  TPoint m_pos, m_startPos;
  // moved cells and cells currently overwritten
  std::vector<TXshCell> m_cells, m_oldCells;
  // columns data of the moved cells
  StageObjectsData *m_columnsData;

  // bitmask of qualifiers that change the behaviour of the Mover
  int m_qualifiers;

  // helper method
  TXsheet *getXsheet() const;

  // cells[] <- xhseet (all the cells block; cells are stored column by column)
  void getCells(std::vector<TXshCell> &cells, int r, int c) const;

  // cells[] -> xsheet (possibly inserting, according to m_qualifiers)
  void setCells(const std::vector<TXshCell> &cells, int r, int c) const;

  // m_columnsData <- xsheet columns data
  void getColumnsData(int c0, int c1);

public:
  enum Qualifier {
    eCopyCells   = 0x1,  // leaves a copy of cells block at the starting point
    eInsertCells = 0x2,
    eOverwriteCells = 0x4,
    eMoveColumns =
        0x8  // move columns only (if the destination columns are empty)
  };

  CellsMover();
  ~CellsMover();

  // porcata da aggiustare
  int m_uffa;

  // initialize the Mover
  void start(int r0, int c0, int r1, int c1, int qualifiers);

  int getQualifiers() const { return m_qualifiers; }
  TPoint getStartPos() const { return m_startPos; }
  TPoint getPos() const { return m_pos; }
  int getColumnCount() const { return m_colCount; }
  int getRowCount() const { return m_rowCount; }

  void setPos(const TPoint &p) { m_pos = p; }
  void setPos(int r, int c) { setPos(TPoint(c, r)); }

  void saveCells();
  void moveCells(const TPoint &pos) const;
  void moveCells() const { moveCells(m_pos); }
  void undoMoveCells(const TPoint &pos) const;
  void undoMoveCells() const { undoMoveCells(m_pos); }
  bool canMoveCells(const TPoint &pos);

  void restoreColumns(int c) const;
  void emptyColumns(int c) const;
  TXshColumn::ColumnType getColumnTypeFromCell(int index) const;

private:
  // not implemented
  CellsMover(const CellsMover &);
  const CellsMover &operator=(const CellsMover &);
};

class LevelMoverTool : public XsheetGUI::DragTool {
protected:
  TPoint m_grabOffset;
  TPoint m_startPos, m_lastPos, m_aimedPos;  // x=col, y=row coordinates
  TDimension m_range;

  int m_qualifiers;
  bool m_validPos;
  LevelMoverUndo *m_undo;
  bool m_moved;
  bool m_columnsMoved;

  bool isTotallyEmptyColumn(int col) const;
  virtual bool canMove(const TPoint &pos);
  bool canMoveColumns(const TPoint &pos);

public:
  LevelMoverTool(XsheetViewer *viewer);
  ~LevelMoverTool();

  void onClick(const QMouseEvent *e) override;
  void onCellChange(int row, int col);
  void onDrag(const QMouseEvent *e) override;
  void onRelease(const CellPosition &pos) override;
  void drawCellsArea(QPainter &p) override;
};

#endif
