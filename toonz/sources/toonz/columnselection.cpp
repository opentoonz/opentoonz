

#include "columnselection.h"

// Tnz6 includes
#include "subscenecommand.h"
#include "menubarcommandids.h"
#include "columncommand.h"
#include "tapp.h"
#include "levelcommand.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheet.h"
#include "toonz/columnfan.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/levelproperties.h"
#include "orientation.h"
#include "toonz/preferences.h"
#include "toonz/txshchildlevel.h"

// TnzCore includes
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tundo.h"

namespace {
// obtain level set contained in the column specified by indices
// it is used for checking and updating the scene cast when pasting
// based on TXsheet::getUsedLevels
void getLevelSetFromColumnIndices(const std::set<int>& indices,
                                  std::set<TXshLevel*>& levelSet) {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (auto c : indices) {
    TXshColumnP column = const_cast<TXsheet*>(xsh)->getColumn(c);
    if (!column) continue;

    TXshCellColumn* cellColumn = column->getCellColumn();
    if (!cellColumn) continue;

    int r0, r1;
    if (!cellColumn->getRange(r0, r1)) continue;

    TXshLevel* level = 0;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = cellColumn->getCell(r);
      if (cell.isEmpty() || !cell.m_level) continue;

      if (level != cell.m_level.getPointer()) {
        level = cell.m_level.getPointer();
        levelSet.insert(level);
        if (level->getChildLevel()) {
          TXsheet* childXsh = level->getChildLevel()->getXsheet();
          childXsh->getUsedLevels(levelSet);
        }
      }
    }
  }
}

}  // namespace

//=============================================================================
// TColumnSelection
//-----------------------------------------------------------------------------

TColumnSelection::TColumnSelection() : m_reframePopup(0) {}

//-----------------------------------------------------------------------------

TColumnSelection::~TColumnSelection() {}

//-----------------------------------------------------------------------------

void TColumnSelection::enableCommands() {
  enableCommand(this, MI_Cut, &TColumnSelection::cutColumns);
  enableCommand(this, MI_Copy, &TColumnSelection::copyColumns);
  enableCommand(this, MI_Paste, &TColumnSelection::pasteColumns);
  enableCommand(this, MI_PasteAbove, &TColumnSelection::pasteColumnsAbove);
  enableCommand(this, MI_Clear, &TColumnSelection::deleteColumns);
  enableCommand(this, MI_InsertAbove, &TColumnSelection::insertColumnsAbove);
  enableCommand(this, MI_Insert, &TColumnSelection::insertColumns);
  enableCommand(this, MI_Collapse, &TColumnSelection::collapse);
  enableCommand(this, MI_ExplodeChild, &TColumnSelection::explodeChild);
  enableCommand(this, MI_Resequence, &TColumnSelection::resequence);
  enableCommand(this, MI_CloneChild, &TColumnSelection::cloneChild);
  enableCommand(this, MI_FoldColumns, &TColumnSelection::hideColumns);
  enableCommand(this, MI_Reframe1, &TColumnSelection::reframe1Cells);
  enableCommand(this, MI_Reframe2, &TColumnSelection::reframe2Cells);
  enableCommand(this, MI_Reframe3, &TColumnSelection::reframe3Cells);
  enableCommand(this, MI_Reframe4, &TColumnSelection::reframe4Cells);
  enableCommand(this, MI_ReframeWithEmptyInbetweens,
                &TColumnSelection::reframeWithEmptyInbetweens);
}

//-----------------------------------------------------------------------------

bool TColumnSelection::isEmpty() const { return m_indices.empty(); }

//-----------------------------------------------------------------------------

void TColumnSelection::copyColumns() {
  m_indices.erase(-1);  // Ignore camera column
  ColumnCmd::copyColumns(m_indices);
}

//-----------------------------------------------------------------------------
// pasteColumns will insert columns before the first column in the selection
void TColumnSelection::pasteColumns() {
  std::set<int> indices;
  if (isEmpty())  // in case that no columns are selected
    indices.insert(0);
  else if (*m_indices.begin() < 0)  // Do nothing
    return;
  else
    indices.insert(*m_indices.begin());

  TUndoManager::manager()->beginBlock();

  ColumnCmd::pasteColumns(indices);

  if (!indices.empty()) {
    // make sure that the levels contained in the pasted columns are registered
    // in the scene cast it may rename the level if there is another level with
    // the same name
    std::set<TXshLevel*> pastedLevels;
    getLevelSetFromColumnIndices(indices, pastedLevels);
    LevelCmd::addMissingLevelsToCast(pastedLevels);
  }

  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------
// pasteColumnsAbove will insert columns after the last column in the selection
void TColumnSelection::pasteColumnsAbove() {
  std::set<int> indices;
  if (isEmpty()) {  // in case that no columns are selected
    TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    indices.insert(xsh->getFirstFreeColumnIndex());
  } else
    indices.insert(*m_indices.rbegin() + 1);

  TUndoManager::manager()->beginBlock();

  ColumnCmd::pasteColumns(indices);

  if (!indices.empty()) {
    // make sure that the levels contained in the pasted columns are registered
    // in the scene cast it may rename the level if there is another level with
    // the same name
    std::set<TXshLevel*> pastedLevels;
    getLevelSetFromColumnIndices(indices, pastedLevels);
    LevelCmd::addMissingLevelsToCast(pastedLevels);
  }

  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------
void TColumnSelection::deleteColumns() {
  if (!ColumnCmd::checkExpressionReferences(m_indices)) return;

  ColumnCmd::deleteColumns(m_indices, false, false);
}

//-----------------------------------------------------------------------------

void TColumnSelection::cutColumns() {
  copyColumns();
  deleteColumns();
}

//-----------------------------------------------------------------------------

void TColumnSelection::insertColumns() {
  ColumnCmd::insertEmptyColumns(m_indices);
}

//-----------------------------------------------------------------------------

void TColumnSelection::insertColumnsAbove() {
  ColumnCmd::insertEmptyColumns(m_indices, true);
}

//-----------------------------------------------------------------------------
void TColumnSelection::collapse() {
  m_indices.erase(-1);  // Ignore camera column
  if (m_indices.empty()) return;
  SubsceneCmd::collapse(m_indices);
}

//-----------------------------------------------------------------------------

void TColumnSelection::explodeChild() {
  if (m_indices.size() != 1) return;
  SubsceneCmd::explode(*m_indices.begin());
}

//-----------------------------------------------------------------------------

static bool canMergeColumns(int column, int mColumn, bool forMatchlines) {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  if (column < 0 || mColumn < 0) return false;

  if (!xsh || !xsh->getColumn(column) || xsh->getColumn(column)->isLocked())
    return false;

  int start, end;
  xsh->getCellRange(column, start, end);

  if (start > end) return false;
  std::vector<TXshCell> cell(end - start + 1);
  std::vector<TXshCell> mCell(end - start + 1);

  xsh->getCells(start, column, cell.size(), &(cell[0]));
  xsh->getCells(start, mColumn, cell.size(), &(mCell[0]));

  TXshSimpleLevel *level = 0, *mLevel = 0;
  TXshLevelP xl;

  for (int i = 0; i < (int)cell.size(); i++) {
    if (cell[i].isEmpty() || mCell[i].isEmpty()) continue;
    if (!level) {
      level = cell[i].getSimpleLevel();
      xl    = cell[i].m_level;
    }

    else if (level != cell[i].getSimpleLevel())
      return false;

    if (!mLevel)
      mLevel = mCell[i].getSimpleLevel();
    else if (mLevel != mCell[i].getSimpleLevel())
      return false;

    if (!mLevel || !level ||  // potrebbero non essere dei simplelevel
        (forMatchlines && (level->getType() != TZP_XSHLEVEL ||
                           mLevel->getType() != TZP_XSHLEVEL)))
      return false;
    else if (!forMatchlines) {
      if (level->getType() != mLevel->getType()) return false;
      if (level->getType() != PLI_XSHLEVEL && level->getType() != OVL_XSHLEVEL)
        return false;
      // Check level type write support. Based on TTool::updateEnabled()
      if (level->getType() == OVL_XSHLEVEL &&
          (level->getPath().getType() == "psd" ||  // PSD files.
           level->getPath().getType() == "gif" ||
           level->getPath().getType() == "mp4" ||
           level->getPath().getType() == "webm" ||
           level->is16BitChannelLevel() ||            // 16bpc images.
           level->getProperties()->getBpp() == 1)) {  // Black & White images.
        return false;
      }
    }
  }
  return true;
}

//-------------------------------------------------------------------------------

void TColumnSelection::selectColumn(int col, bool on) {
  if (on)
    m_indices.insert(col);
  else
    m_indices.erase(col);

  CommandManager::instance()->enable(MI_MergeColumns, false);
  CommandManager::instance()->enable(MI_ApplyMatchLines, false);
  CommandManager::instance()->enable(MI_MergeCmapped, false);

  if (m_indices.size() <= 1) return;

  std::set<int>::iterator it = m_indices.begin();
  int firstCol               = *it;

  if (firstCol < 0) return;

  for (++it; it != m_indices.end(); ++it)
    if (!canMergeColumns(firstCol, *it, false)) break;

  if (it == m_indices.end()) {
    CommandManager::instance()->enable(MI_MergeColumns, true);
    return;
  }

  it = m_indices.begin();
  for (++it; it != m_indices.end(); ++it)
    if (!canMergeColumns(firstCol, *it, true)) break;

  if (it == m_indices.end()) {
    CommandManager::instance()->enable(MI_ApplyMatchLines, true);
    CommandManager::instance()->enable(MI_MergeCmapped, true);
  }
}

//-----------------------------------------------------------------------------

void TColumnSelection::selectNone() { m_indices.clear(); }

//-----------------------------------------------------------------------------

bool TColumnSelection::isColumnSelected(int col) const {
  return m_indices.count(col) > 0;
}

//-----------------------------------------------------------------------------

void TColumnSelection::resequence() {
  if (m_indices.size() == 1) ColumnCmd::resequence(*m_indices.begin());
}

//-----------------------------------------------------------------------------

void TColumnSelection::cloneChild() {
  if (m_indices.size() == 1) ColumnCmd::cloneChild(*m_indices.begin());
}

//-----------------------------------------------------------------------------

void TColumnSelection::hideColumns() {
  TApp* app = TApp::instance();
  for (auto o : Orientations::all()) {
    ColumnFan* columnFan =
        app->getCurrentXsheet()->getXsheet()->getColumnFan(o);
    std::set<int>::iterator it = m_indices.begin();
    for (; it != m_indices.end(); ++it) columnFan->deactivate(*it);
  }
  m_indices.clear();
  app->getCurrentXsheet()->notifyXsheetChanged();
  // DA FARE (non c'e una notica per il solo cambiamento della testa delle
  // colonne)
  //  TApp::instance()->->notify(TColumnHeadChange());
  app->getCurrentScene()->setDirtyFlag(true);
}
