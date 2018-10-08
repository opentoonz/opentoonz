

#include "cellkeyframeselection.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "cellkeyframedata.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//=============================================================================
//  TCellKeyframeSelection
//-----------------------------------------------------------------------------

TCellKeyframeSelection::TCellKeyframeSelection(
    TCellSelection *cellSelection, TKeyframeSelection *keyframeSelection)
    : m_cellSelection(cellSelection)
    , m_keyframeSelection(keyframeSelection)
    , m_xsheetHandle(0) {}

//-----------------------------------------------------------------------------

TCellKeyframeSelection::~TCellKeyframeSelection() {
  delete m_cellSelection;
  delete m_keyframeSelection;
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::enableCommands() {
  enableCommand(this, MI_Copy, &TCellKeyframeSelection::copyCellsKeyframes);
  enableCommand(this, MI_Paste, &TCellKeyframeSelection::pasteCellsKeyframes);
  enableCommand(this, MI_Cut, &TCellKeyframeSelection::cutCellsKeyframes);
  enableCommand(this, MI_Clear, &TCellKeyframeSelection::deleteCellsKeyframes);
}

//-----------------------------------------------------------------------------

bool TCellKeyframeSelection::isEmpty() const {
  return m_keyframeSelection->isEmpty() && m_cellSelection->isEmpty();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::copyCellsKeyframes() {
  TCellKeyframeData *data = new TCellKeyframeData();
  // Copy cells
  int r0, c0, r1, c1;
  m_cellSelection->getSelectedCells(r0, c0, r1, c1);
  if (!isEmpty()) {
    int colCount = c1 - c0 + 1;
    int rowCount = r1 - r0 + 1;
    if (colCount <= 0 || rowCount <= 0) return;
    TXsheet *xsh        = m_xsheetHandle->getXsheet();
    TCellData *cellData = new TCellData();
    cellData->setCells(xsh, r0, c0, r1, c1);
    data->setCellData(cellData);
  }
  // Copy keyframes
  if (!isEmpty()) {
    QClipboard *clipboard       = QApplication::clipboard();
    TXsheet *xsh                = m_xsheetHandle->getXsheet();
    TKeyframeData *keyframeData = new TKeyframeData();
    keyframeData->setKeyframes(m_keyframeSelection->getSelection(), xsh);
    data->setKeyframeData(keyframeData);
  }
  // Set the cliboard
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::pasteCellsKeyframes() {
  m_cellSelection->pasteCells();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::deleteCellsKeyframes() {
  TUndoManager::manager()->beginBlock();
  m_cellSelection->deleteCells();
  m_keyframeSelection->deleteKeyframes();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::cutCellsKeyframes() {
  copyCellsKeyframes();
  TUndoManager::manager()->beginBlock();
  int r0, r1, c0, c1;
  m_cellSelection->getSelectedCells(r0, c0, r1, c1);
  m_cellSelection->cutCells(true);
  m_keyframeSelection->deleteKeyframesWithShift(r0, r1, c0, c1);
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectCellsKeyframes(int r0, int c0, int r1,
                                                  int c1) {
  m_cellSelection->selectCells(r0, c0, r1, c1);
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  m_xsheetHandle->getXsheet();
  if (r1 < r0) std::swap(r0, r1);
  if (c1 < c0) std::swap(c0, c1);
  m_keyframeSelection->clear();
  int r, c;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TStageObjectId id =
          c < 0 ? TStageObjectId::CameraId(0) : TStageObjectId::ColumnId(c);
      TStageObject *stObj = xsh->getStageObject(id);
      if (stObj->isKeyframe(r)) m_keyframeSelection->select(r, c);
    }
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectCellKeyframe(int row, int col) {
  m_cellSelection->selectCell(row, col);
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  TStageObjectId id =
      col < 0 ? TStageObjectId::CameraId(0) : TStageObjectId::ColumnId(col);
  TStageObject *stObj = xsh->getStageObject(id);
  m_keyframeSelection->clear();
  if (stObj->isKeyframe(row)) m_keyframeSelection->select(row, col);
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectNone() {
  m_cellSelection->selectNone();
  m_keyframeSelection->selectNone();
}
