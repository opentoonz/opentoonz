

#include "xshcellmover.h"

// Tnz6 includes
#include "tapp.h"
#include "xsheetviewer.h"
#include "cellselection.h"
#include "columncommand.h"

// TnzTools includes
#include "tools/cursors.h"
#include "tools/cursormanager.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/stageobjectsdata.h"
#include "toonzqt/selection.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tscenehandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txsheethandle.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshleveltypes.h"

// TnzBase includes
#include "tfx.h"

// Qt includes
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>

//=============================================================================

CellsMover::CellsMover()
    : m_rowCount(0)
    , m_colCount(0)
    , m_pos(0, 0)
    , m_startPos(0, 0)
    , m_columnsData(0)
    , m_qualifiers(0)
    , m_uffa(0) {}

CellsMover::~CellsMover() {
  delete m_columnsData;
  m_columnsData = 0;
}

//
// initialize the mover: allocate memory, get cells and column data
//
void CellsMover::start(int r0, int c0, int r1, int c1, int qualifiers) {
  m_rowCount = r1 - r0 + 1;
  m_colCount = c1 - c0 + 1;
  assert(m_rowCount > 0);
  assert(m_colCount > 0);
  m_startPos = m_pos = TPoint(c0, r0);
  m_qualifiers       = qualifiers;
  m_cells.resize(m_rowCount * m_colCount, TXshCell());
  m_oldCells.resize(m_rowCount * m_colCount, TXshCell());
  getCells(m_cells, r0, c0);
  getColumnsData(c0, c1);
}

//
// helper method: returns the current xsheet
//
TXsheet *CellsMover::getXsheet() const {
  return TApp::instance()->getCurrentXsheet()->getXsheet();
}

//
// cells <- xsheet
//

void CellsMover::getCells(std::vector<TXshCell> &cells, int r, int c) const {
  for (int i = 0; i < (int)cells.size(); i++) cells[i] = TXshCell();
  TXsheet *xsh                                         = getXsheet();
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (column && column->isLocked()) continue;
    xsh->getCells(r, c + i, m_rowCount, &cells[m_rowCount * i]);
  }
}

//
// xsheet <- cells
//
void CellsMover::setCells(const std::vector<TXshCell> &cells, int r,
                          int c) const {
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (column)  // skip if columns is locked or doesn't support cells operation
      if (column->getCellColumn() == 0 || column->isLocked()) continue;
    xsh->setCells(r, c + i, m_rowCount, &cells[m_rowCount * i]);
  }
}

//
// m_oldCells <- xsheet
//
void CellsMover::saveCells() {
  int r = m_pos.y;
  int c = m_pos.x;
  getCells(m_oldCells, r, c);
}

//
// xsheet <- m_cells; insert cells if qualifiers contain eInsertCells
//
void CellsMover::moveCells(const TPoint &pos) const {
  int r        = pos.y;
  int c        = pos.x;
  TXsheet *xsh = getXsheet();
  if (m_qualifiers & eInsertCells) {
    for (int i = 0; i < m_colCount; i++) {
      TXshColumn *column = xsh->getColumn(c + i);
      if (column) {
        if (column->getCellColumn() == 0 || column->isLocked()) continue;
        xsh->insertCells(r, c + i, m_rowCount);
      }
    }
  }
  setCells(m_cells, r, c);
}

void CellsMover::undoMoveCells(const TPoint &pos) const {
  TXsheet *xsh = getXsheet();
  int r        = pos.y;
  int c        = pos.x;
  if (m_qualifiers & eInsertCells) {
    for (int i = 0; i < m_colCount; i++) xsh->removeCells(r, c + i, m_rowCount);
  } else {
    for (int i = 0; i < m_colCount; i++) xsh->clearCells(r, c + i, m_rowCount);
    if (m_qualifiers & eOverwriteCells) setCells(m_oldCells, r, c);
  }
}

bool CellsMover::canMoveCells(const TPoint &pos) {
  TXsheet *xsh = getXsheet();
  if (pos.x < 0) return false;
  if (pos.x != m_startPos.x) {
    int count = 0;
    // controlla il tipo
    int i = 0;
    while (i < m_rowCount * m_colCount) {
      TXshColumn::ColumnType srcType = getColumnTypeFromCell(i);
      int dstIndex                   = pos.x + i;
      TXshColumn *dstColumn          = xsh->getColumn(dstIndex);
      if (srcType == TXshColumn::eZeraryFxType) return false;
      if (dstColumn && !dstColumn->isEmpty() &&
          dstColumn->getColumnType() != srcType)
        return false;
      if (!dstColumn || dstColumn->isLocked() == false) count++;
      i += m_rowCount;
    }
    if (count == 0) return false;
  }
  if ((m_qualifiers & CellsMover::eInsertCells) ||
      (m_qualifiers & CellsMover::eOverwriteCells))
    return true;
  int count = 0;
  for (int i = 0; i < m_colCount; i++) {
    for (int j = 0; j < m_rowCount; j++) {
      if (!xsh->getCell(pos.y + j, pos.x + i).isEmpty()) return false;
      count++;
    }
  }
  if (count == 0) return false;
  return true;
}
//
// m_columnsData <- xsheet
//

void CellsMover::getColumnsData(int c0, int c1) {
  TXsheet *xsh  = getXsheet();
  int colCount  = c1 - c0 + 1;
  m_columnsData = new StageObjectsData();
  std::set<int> ii;
  for (int i = 0; i < colCount; i++) ii.insert(c0 + i);
  m_columnsData->storeColumns(
      ii, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  m_columnsData->storeColumnFxs(
      ii, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
}

//
// xsheet <- m_columnsData, at column c
//
// Note: the restored columns are empty (i.e. no cells are copied)
//

void CellsMover::restoreColumns(int c) const {
  std::set<int> ii;
  for (int i   = 0; i < m_colCount; i++) ii.insert(c + i);
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) xsh->removeColumn(c);
  std::list<int> restoredSplineIds;
  m_columnsData->restoreObjects(
      ii, restoredSplineIds, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (!column) continue;
    TXshCellColumn *cellColumn = column->getCellColumn();
    if (!cellColumn) continue;
    int r0 = 0, r1 = -1;
    cellColumn->getRange(r0, r1);
    if (r0 <= r1) cellColumn->clearCells(r0, r1 - r0 + 1);
  }
}

//
// the columns [c,c+m_colCount-1] becme empty
//

void CellsMover::emptyColumns(int c) const {
  std::set<int> ii;
  for (int i = 0; i < m_colCount; i++) ii.insert(c + i);
  ColumnCmd::deleteColumns(ii, false, true);
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) xsh->insertColumn(c);
}

TXshColumn::ColumnType CellsMover::getColumnTypeFromCell(int index) const {
  const TXshLevelP &xl = m_cells[index].m_level;
  return xl ? TXshColumn::toColumnType(xl->getType()) : TXshColumn::eLevelType;
}

//=============================================================================

class LevelMoverUndo final : public TUndo {
  CellsMover m_cellsMover;

public:
  LevelMoverUndo() {}

  std::vector<int> m_columnsChanges;

  CellsMover *getCellsMover() { return &m_cellsMover; }

  void undo() const override {
    m_cellsMover.undoMoveCells();

    int ca       = m_cellsMover.getStartPos().x;
    int cb       = m_cellsMover.getPos().x;
    int colCount = m_cellsMover.getColumnCount();

    if (ca != cb) {
      if (m_cellsMover.m_uffa & 2) m_cellsMover.emptyColumns(cb);
      if (m_cellsMover.m_uffa & 1) m_cellsMover.restoreColumns(ca);
    }
    m_cellsMover.moveCells(m_cellsMover.getStartPos());
    TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_cellsMover.getColumnTypeFromCell(0) == TXshColumn::eSoundType)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
  void redo() const override {
    int ca       = m_cellsMover.getStartPos().x;
    int cb       = m_cellsMover.getPos().x;
    int colCount = m_cellsMover.getColumnCount();

    if (ca != cb) {
      if (m_cellsMover.m_uffa & 1) m_cellsMover.emptyColumns(ca);
      if (m_cellsMover.m_uffa & 2) m_cellsMover.restoreColumns(cb);
    }

    if ((m_cellsMover.getQualifiers() & CellsMover::eCopyCells) == 0 &&
        (m_cellsMover.getQualifiers() & CellsMover::eOverwriteCells) == 0)
      m_cellsMover.undoMoveCells(m_cellsMover.getStartPos());
    if (m_cellsMover.getQualifiers() &
        CellsMover::eOverwriteCells) {  // rimuove le celle vecchie
      int c, ra = m_cellsMover.getStartPos().y,
             rowCount = m_cellsMover.getRowCount();
      TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
      for (c = ca; c < ca + colCount; c++) xsh->clearCells(ra, c, rowCount);
    }
    m_cellsMover.moveCells();

    TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_cellsMover.getColumnTypeFromCell(0) == TXshColumn::eSoundType)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Move Level"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

LevelMoverTool::LevelMoverTool(XsheetViewer *viewer)
    : XsheetGUI::DragTool(viewer)
    , m_range(0, 0)
    , m_qualifiers(0)
    , m_validPos(false)
    , m_undo(0)
    , m_moved(false)
    , m_columnsMoved(false) {}

LevelMoverTool::~LevelMoverTool() {}

// isTotallyEmptyColumn == column empty, no fx
bool LevelMoverTool::isTotallyEmptyColumn(int col) const {
  if (col < 0) return false;
  TXsheet *xsh       = getViewer()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return true;
  if (!column->isEmpty()) return false;
  if (column->getFx()->getOutputConnectionCount() != 0) return false;
  // bisogna controllare lo stage object
  return true;
}

bool LevelMoverTool::canMove(const TPoint &pos) {
  if (!m_undo) return false;
  CellsMover *cellsMover = m_undo->getCellsMover();
  if (!cellsMover) return false;
  return cellsMover->canMoveCells(pos);
}

bool LevelMoverTool::canMoveColumns(const TPoint &pos) {
  TXsheet *xsh = getViewer()->getXsheet();
  if (pos.x < 0) return false;
  if (pos.x != m_lastPos.x) {
    int count = 0;
    // controlla il tipo
    for (int i = 0; i < m_range.lx; i++) {
      int srcIndex          = m_lastPos.x + i;
      int dstIndex          = pos.x + i;
      TXshColumn *srcColumn = xsh->getColumn(srcIndex);
      if (srcColumn && srcColumn->isLocked()) continue;
      TXshColumn *dstColumn          = xsh->getColumn(dstIndex);
      TXshColumn::ColumnType srcType = TXshColumn::eLevelType;
      if (srcColumn) srcType         = srcColumn->getColumnType();
      if (srcType == TXshColumn::eZeraryFxType) return false;
      /*
qDebug() << "check: " << srcIndex << ":" <<
(srcColumn ? QString::number(srcType) : "empty") <<
" => " << dstIndex << ":" <<
(dstColumn ? QString::number(dstColumn->getColumnType()) : "empty");
*/

      if (dstColumn && !dstColumn->isEmpty() &&
          dstColumn->getColumnType() != srcType) {
        return false;
      }
      if (!dstColumn || dstColumn->isLocked() == false) {
        count++;
      }
    }
    if (count == 0) return false;
  }
  return true;
}

void LevelMoverTool::onClick(const QMouseEvent *e) {
  QPoint pos                = e->pos();
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();

  m_qualifiers = 0;
  if (Preferences::instance()->getDragCellsBehaviour() == 1)
    m_qualifiers |= CellsMover::eMoveColumns;
  if (e->modifiers() & Qt::ControlModifier)
    m_qualifiers |= CellsMover::eCopyCells;
  if (e->modifiers() & Qt::ShiftModifier)
    m_qualifiers |= CellsMover::eInsertCells;
  if (e->modifiers() & Qt::AltModifier)
    m_qualifiers |= CellsMover::eOverwriteCells;

  int cursorType = ToolCursor::CURSOR_ARROW;
  if (m_qualifiers & CellsMover::eCopyCells)
    cursorType = ToolCursor::SplineEditorCursorAdd;
  setToolCursor(getViewer(), cursorType);

  int r0, c0, r1, c1;
  getViewer()->getCellSelection()->getSelectedCells(r0, c0, r1, c1);

  m_validPos             = true;
  m_undo                 = new LevelMoverUndo();
  CellsMover *cellsMover = m_undo->getCellsMover();
  cellsMover->start(r0, c0, r1, c1, m_qualifiers);
  m_undo->m_columnsChanges.resize(c1 - c0 + 1, 0);

  m_moved = true;
  if (m_qualifiers & CellsMover::eCopyCells) {
    if (m_qualifiers & CellsMover::eInsertCells) {
      cellsMover->moveCells();
      m_moved = true;
    } else
      m_moved = false;
  }
  m_columnsMoved = false;

  int colCount = c1 - c0 + 1;
  int rowCount = r1 - r0 + 1;
  m_range.lx   = colCount;
  m_range.ly   = rowCount;
  m_startPos.x = c0;
  m_startPos.y = r0;
  m_lastPos = m_aimedPos = m_startPos;
  m_grabOffset           = m_startPos - TPoint(col, row);

  TXsheet *xsh = getViewer()->getXsheet();

  // move
  m_validPos                      = true;
  m_lastPos                       = m_startPos;
  m_undo->getCellsMover()->m_uffa = 0;
}

void LevelMoverTool::onCellChange(int row, int col) {
  TXsheet *xsh = getViewer()->getXsheet();

  TPoint pos           = TPoint(col, row) + m_grabOffset;
  if (pos.y < 0) pos.y = 0;
  if (pos.x < 0) return;
  if (pos == m_aimedPos) return;

  m_aimedPos   = pos;
  TPoint delta = m_aimedPos - m_lastPos;

  m_validPos = canMoveColumns(pos);
  if (!m_validPos) return;

  CellsMover *cellsMover = m_undo->getCellsMover();
  if (m_moved) cellsMover->undoMoveCells();
  m_validPos = canMove(pos);

  if (m_validPos) {
    //----------------------
    if (delta.x != 0 && (m_qualifiers & CellsMover::eMoveColumns)) {
      // spostamento di colonne
      int colCount = m_range.lx;

      bool emptySrc = true;
      for (int i = 0; i < colCount; i++) {
        TXshColumn *column = xsh->getColumn(m_lastPos.x + i);
        if (column && !column->isEmpty()) {
          emptySrc = false;
          break;
        }
      }

      bool lockedDst = false;
      bool emptyDst  = true;
      for (int i = 0; i < colCount; i++) {
        if (!isTotallyEmptyColumn(pos.x + i)) emptyDst = false;
        TXshColumn *column                          = xsh->getColumn(pos.x + i);
        if (column && column->isLocked()) lockedDst = true;
      }

      if (emptySrc) {
        if (m_lastPos.x == m_startPos.x)
          m_undo->getCellsMover()->m_uffa |=
              1;  // first column(s) has/have been cleared
        cellsMover->emptyColumns(m_lastPos.x);
      }

      if (emptyDst && !lockedDst) {
        if (pos.x == m_startPos.x)
          m_undo->getCellsMover()->m_uffa &=
              ~1;  // first column(s) has/have been restored
        else
          m_undo->getCellsMover()->m_uffa |=
              2;  // columns data have been copied on the current position
        cellsMover->restoreColumns(pos.x);
      } else {
        m_undo->getCellsMover()->m_uffa &=
            ~2;  // no columns data have been copied on the current position
      }
    }

    //----------------------

    m_lastPos = pos;
    cellsMover->setPos(pos.y, pos.x);
    cellsMover->saveCells();
    cellsMover->moveCells();
    m_moved = true;
  } else {
    cellsMover->moveCells();
  }

  xsh->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  if (cellsMover->getColumnTypeFromCell(0) == TXshColumn::eSoundType)
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TRect selection(m_aimedPos, m_range);
  getViewer()->getCellSelection()->selectCells(selection.y0, selection.x0,
                                               selection.y1, selection.x1);
}

void LevelMoverTool::onDrag(const QMouseEvent *e) {
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  onCellChange(cellPosition.frame(), cellPosition.layer());
  refreshCellsArea();
  refreshColumnsArea();
}

void LevelMoverTool::onRelease(const CellPosition &pos) {
  m_validPos = false;
  m_range.lx = 0;
  m_range.ly = 0;
  setToolCursor(getViewer(), ToolCursor::CURSOR_ARROW);
  refreshCellsArea();
  if (m_lastPos != m_startPos) TUndoManager::manager()->add(m_undo);
  m_undo = 0;
}

void LevelMoverTool::drawCellsArea(QPainter &p) {
  TRect rect(m_aimedPos, m_range);
  // voglio visualizzare il rettangolo nella posizione desiderata
  // che potrebbe anche non essere possibile, ad esempio perche' fuori
  // dall'xsheet quindi devo fare un clipping. Il rettangolo potrebbe
  // anche essere interamente fuori
  if (rect.x1 < 0 || rect.y1 < 0) return;
  if (rect.x0 < 0) rect.x0 = 0;
  if (rect.y0 < 0) rect.y0 = 0;

  QRect screen = getViewer()->rangeToXYRect(CellRange(
      CellPosition(rect.y0, rect.x0), CellPosition(rect.y1 + 1, rect.x1 + 1)));
  p.setPen((m_aimedPos == m_lastPos && m_validPos) ? QColor(190, 220, 255)
                                                   : QColor(255, 0, 0));
  int i;
  for (i = 0; i < 3; i++)  // thick border inside cell
    p.drawRect(QRect(screen.topLeft() + QPoint(i, i),
                     screen.size() - QSize(2 * i, 2 * i)));
}
