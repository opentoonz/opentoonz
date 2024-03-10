

#include "toonz/childstack.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/scenefx.h"

//=============================================================================
// ChildStack

ChildStack::ChildStack(ToonzScene *scene)
    : m_scene(scene), m_xsheet(new TXsheet()) {
  m_xsheet->setScene(m_scene);
  m_xsheet->addRef();
}

//-----------------------------------------------------------------------------

ChildStack::~ChildStack() {
  m_xsheet->release();
  clearPointerContainer(m_stack);
}

//-----------------------------------------------------------------------------

void ChildStack::clear() {
  m_xsheet->clearAll();
  m_xsheet->setScene(m_scene);

  clearPointerContainer(m_stack);
}

//-----------------------------------------------------------------------------

bool ChildStack::openChild(int row, int col) {
  TXshChildLevel *childLevel = 0;
  TXshCell cell              = m_xsheet->getCell(row, col);
  if (!cell.m_level) {
    return false;
  } else
    childLevel = cell.m_level->getChildLevel();
  if (!childLevel) return false;
  TXsheet *childXsheet = childLevel->getXsheet();
  AncestorNode *node   = new AncestorNode();
  node->m_row          = row;
  node->m_col          = col;
  node->m_xsheet       = m_xsheet;
  node->m_cl           = childLevel;
  node->m_justCreated  = !cell.m_level;
  int r0 = 0, r1 = -1;
  m_xsheet->getCellRange(col, r0, r1);
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = m_xsheet->getCell(r, col);
    if (cell.m_level.getPointer() == childLevel) {
      int d = cell.m_frameId.getNumber() - 1;
      std::map<int, int>::iterator it = node->m_rowTable.find(d);
      if (it == node->m_rowTable.end()) node->m_rowTable[d] = r;
    }
  }
  m_stack.push_back(node);
  m_xsheet = childXsheet;
  return true;
}

//-----------------------------------------------------------------------------

bool ChildStack::closeChild(int &row, int &col) {
  if (m_stack.empty()) return false;

  TXsheet *childXsh = m_xsheet;
  childXsh
      ->updateFrameCount();  // non dovrebbe essere necessario, ma non si sa mai
  int childFrameCount = childXsh->getFrameCount();

  AncestorNode *node = m_stack.back();
  m_stack.pop_back();

  TXsheet *parentXsh = node->m_xsheet;
  TXshChildLevelP cl = node->m_cl;
  row                = node->m_row;
  col                = node->m_col;
  bool justCreated   = node->m_justCreated;
  delete node;

  m_xsheet = parentXsh;
  m_xsheet->updateFrameCount();

  // if(cl) cl->invalidateIcon();

  TXsheet *xsh = m_xsheet;
  if (justCreated) {
    assert(xsh->getCell(row, col).m_level.getPointer() == cl.getPointer());
    if (childFrameCount > 1) {
      xsh->insertCells(row + 1, col, childFrameCount - 1);
      for (int i = 1; i < childFrameCount; i++)
        xsh->setCell(row + i, col, TXshCell(cl.getPointer(), TFrameId(1 + i)));
    }
  }

  return true;
}

//-----------------------------------------------------------------------------

TXshChildLevel *ChildStack::createChild(int row, int col) {
  TXshLevel *xl = m_scene->createNewLevel(CHILD_XSHLEVEL);
  m_xsheet->setCell(row, col, TXshCell(xl, TFrameId(1)));

  TXshCell cell = m_xsheet->getCell(row, col);
  return cell.m_level->getChildLevel();
}

//-----------------------------------------------------------------------------

int ChildStack::getAncestorCount() const { return m_stack.size(); }

//-----------------------------------------------------------------------------

TXsheet *ChildStack::getTopXsheet() const {
  return m_stack.empty() ? m_xsheet : m_stack.front()->m_xsheet;
}

//-----------------------------------------------------------------------------

std::pair<TXsheet *, int> ChildStack::getAncestor(int row) const {
  TXsheet *xsh = m_xsheet;
  int i        = m_stack.size() - 1;
  while (i >= 0) {
    std::map<int, int>::const_iterator it = m_stack[i]->m_rowTable.find(row);
    if (it == m_stack[i]->m_rowTable.end()) break;
    row = it->second;
    xsh = m_stack[i]->m_xsheet;
    i--;
  }
  return std::make_pair(xsh, row);
}

//-----------------------------------------------------------------------------

bool ChildStack::getAncestorAffine(TAffine &aff, int row) const {
  aff   = TAffine();
  int i = m_stack.size() - 1;
  while (i >= 0) {
    std::map<int, int>::const_iterator it = m_stack[i]->m_rowTable.find(row);
    if (it == m_stack[i]->m_rowTable.end()) break;

    row        = it->second;
    AncestorNode *node = m_stack[i];
    TAffine aff2;
    if (!getColumnPlacement(aff2, node->m_xsheet, row, node->m_col, false))
      return false;
    aff = aff2 * aff;
    i--;
  }
  return true;
}

//-----------------------------------------------------------------------------

AncestorNode *ChildStack::getAncestorInfo(int ancestorDepth) {
  if (ancestorDepth < 0 || ancestorDepth >= m_stack.size()) return nullptr;

  return m_stack[ancestorDepth];
}