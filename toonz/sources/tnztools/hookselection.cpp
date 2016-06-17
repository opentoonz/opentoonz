

#include "hookselection.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobjecttree.h"

#include "tools/tool.h"
#include "tools/toolhandle.h"

#include "toonzqt/selectioncommandids.h"

#include <QApplication>
#include <QClipboard>

//=============================================================================
//
// HookUndo
//
//=============================================================================

HookUndo::HookUndo(const TXshLevelP &level) : m_level(level) {
  HookSet *hookSet = m_level->getHookSet();
  assert(hookSet);
  if (hookSet) m_oldHooks = *hookSet;
}

//---------------------------------------------------------------------------

HookUndo::~HookUndo() {}

//---------------------------------------------------------------------------

void HookUndo::onAdd() {
  HookSet *hookSet = m_level->getHookSet();
  assert(hookSet);
  if (hookSet) m_newHooks = *hookSet;
}

//---------------------------------------------------------------------------

void HookUndo::assignHookSet(const HookSet &src) const {
  HookSet *hookSet = m_level->getHookSet();
  assert(hookSet);
  if (hookSet) *hookSet = src;

  TTool::getApplication()
      ->getCurrentXsheet()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (tool) {
    tool->updateMatrix();
    tool->invalidate();
  }
}

//---------------------------------------------------------------------------

void HookUndo::undo() const { assignHookSet(m_oldHooks); }

//---------------------------------------------------------------------------

void HookUndo::redo() const { assignHookSet(m_newHooks); }

//---------------------------------------------------------------------------

int HookUndo::getSize() const { return sizeof(*this) + 2 * sizeof(HookSet); }

//=============================================================================
//
// HooksData
//
//=============================================================================

HooksData::HooksData(const TXshLevelP &level) : m_level(level) {}

//---------------------------------------------------------------------------

HooksData::~HooksData() {}

//---------------------------------------------------------------------------

HooksData *HooksData::clone() const {
  HooksData *newData       = new HooksData(m_level);
  newData->m_hookPositions = m_hookPositions;
  return newData;
}

//---------------------------------------------------------------------------

void HooksData::storeHookPositions(const std::vector<int> &ids) {
  if (ids.empty()) return;
  TTool::Application *app = TTool::getApplication();
  TXshLevelP level        = app->getCurrentLevel()->getLevel();
  assert(level = m_level);
  if (level != m_level || !m_level || m_level->getSimpleLevel()->isReadOnly())
    return;
  HookSet *hookSet = m_level->getHookSet();
  if (!hookSet) return;

  TFrameId fid = app->getCurrentTool()->getTool()->getCurrentFid();
  int i, idsSize = ids.size();
  m_hookPositions.clear();
  for (i = 0; i < idsSize; i++) {
    Hook *hook = hookSet->getHook(ids[i]);
    assert(hook);
    if (!hook) continue;
    TPointD aPos = hook->getAPos(fid);
    TPointD bPos = hook->getBPos(fid);
    m_hookPositions.push_back(HookPosition(ids[i], aPos, bPos));
  }
}

//---------------------------------------------------------------------------

void HooksData::restoreHookPositions() const {
  if (m_hookPositions.empty()) return;
  TTool::Application *app = TTool::getApplication();
  TXshLevelP level        = app->getCurrentLevel()->getLevel();
  assert(level = m_level);
  if (level != m_level || !m_level || m_level->getSimpleLevel()->isReadOnly())
    return;
  HookSet *hookSet = m_level->getHookSet();
  if (!hookSet) return;

  TFrameId fid = app->getCurrentTool()->getTool()->getCurrentFid();
  int i, posSize = m_hookPositions.size();

  for (i = 0; i < posSize; i++) {
    HookPosition hookPos = m_hookPositions[i];
    Hook *hook           = hookSet->getHook(hookPos.m_id);
    assert(hook);
    if (!hook) continue;
    hook->setAPos(fid, hookPos.m_aPos);
    hook->setBPos(fid, hookPos.m_bPos);
  }
}

//===========================================================================
//
// HookSelection
//
//===========================================================================

HookSelection::HookSelection() {}

//---------------------------------------------------------------------------

HookSelection::~HookSelection() {}

//---------------------------------------------------------------------------

TSelection *HookSelection::clone() const { return new HookSelection(*this); }

//---------------------------------------------------------------------------

void HookSelection::select(int id, int side) {
  m_hooks.insert(std::make_pair(id, side));
}

//---------------------------------------------------------------------------

void HookSelection::unselect(int id, int side) {
  m_hooks.erase(std::make_pair(id, side));
}

//---------------------------------------------------------------------------

bool HookSelection::isSelected(int id, int side) const {
  return m_hooks.count(std::make_pair(id, side)) > 0;
}

//---------------------------------------------------------------------------

void HookSelection::invertSelection(int id, int side) {
  if (isSelected(id, side))
    unselect(id, side);
  else
    select(id, side);
}

//---------------------------------------------------------------------------

bool HookSelection::isEmpty() const { return m_hooks.empty(); }

//---------------------------------------------------------------------------

HookSet *HookSelection::getHookSet() const {
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return 0;
  return xl->getHookSet();
}

//---------------------------------------------------------------------------

bool HookSelection::select(const TSelection *s) {
  if (const HookSelection *hs = dynamic_cast<const HookSelection *>(s)) {
    m_level = hs->m_level;
    m_hooks = hs->m_hooks;
    return true;
  } else
    return false;
}

//---------------------------------------------------------------------------

void HookSelection::enableCommands() {
  enableCommand(this, MI_Clear, &HookSelection::deleteSelectedHooks);
  enableCommand(this, MI_Copy, &HookSelection::copySelectedHooks);
  enableCommand(this, MI_Cut, &HookSelection::cutSelectedHooks);
  enableCommand(this, MI_Paste, &HookSelection::pasteSelectedHooks);
}

//---------------------------------------------------------------------------

void HookSelection::deleteSelectedHooks() {
  TTool::Application *app = TTool::getApplication();
  TTool *tool             = app->getCurrentTool()->getTool();
  if (!app) return;
  TXshLevel *xl    = app->getCurrentLevel()->getLevel();
  HookSet *hookSet = xl->getHookSet();
  if (!xl || !xl->getSimpleLevel() || !hookSet ||
      xl->getSimpleLevel()->isReadOnly())
    return;

  HookUndo *undo = new HookUndo(xl->getSimpleLevel());
  TFrameId fid   = tool->getCurrentFid();

  for (int i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    if (isSelected(i, 1) && isSelected(i, 2))
      hookSet->clearHook(hook);
    else if (isSelected(i, 2))
      hook->setBPos(fid, hook->getAPos(fid));
    else if (isSelected(i, 1))
      hook->setAPos(fid, hook->getBPos(fid));
  }
  TUndoManager::manager()->add(undo);
  app->getCurrentXsheet()->getXsheet()->getStageObjectTree()->invalidateAll();
  tool->invalidate();
}

//---------------------------------------------------------------------------

void HookSelection::copySelectedHooks() {
  if (isEmpty()) return;
  std::vector<int> ids;
  std::set<std::pair<int, int>>::iterator it;
  for (it = m_hooks.begin(); it != m_hooks.end(); it++) {
    if (std::find(ids.begin(), ids.end(), it->first) == ids.end())
      ids.push_back(it->first);
  }

  TXshLevel *xl   = TTool::getApplication()->getCurrentLevel()->getLevel();
  HooksData *data = new HooksData(xl);
  data->storeHookPositions(ids);
  QApplication::clipboard()->setMimeData(data);
}

//---------------------------------------------------------------------------

void HookSelection::cutSelectedHooks() {
  copySelectedHooks();
  TXshLevel *xl    = TTool::getApplication()->getCurrentLevel()->getLevel();
  TUndo *undo      = new HookUndo(xl);
  HookSet *hookSet = xl->getHookSet();
  std::set<std::pair<int, int>>::iterator it;
  for (it = m_hooks.begin(); it != m_hooks.end(); it++) {
    Hook *hook = hookSet->getHook(it->first);
    assert(hook);
    if (!hook) return;
    TFrameId fid =
        TTool::getApplication()->getCurrentTool()->getTool()->getCurrentFid();
    hook->eraseFrame(fid);
  }
  TUndoManager::manager()->add(undo);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//---------------------------------------------------------------------------

void HookSelection::pasteSelectedHooks() {
  const QMimeData *data      = QApplication::clipboard()->mimeData();
  const HooksData *hooksData = dynamic_cast<const HooksData *>(data);
  if (!hooksData) return;
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  TUndo *undo   = new HookUndo(xl);
  hooksData->restoreHookPositions();
  TUndoManager::manager()->add(undo);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}
