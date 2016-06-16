#pragma once

#ifndef HOOK_SELECTION
#define HOOK_SELECTION

#include "toonzqt/selection.h"
#include "toonzqt/dvmimedata.h"
#include "toonz/txshlevel.h"
#include "toonz/hook.h"
#include "tundo.h"

#include <set>

//=============================================================================
// HookUndo
//-----------------------------------------------------------------------------

class HookUndo : public TUndo {
  HookSet m_oldHooks, m_newHooks;
  TXshLevelP m_level;

public:
  HookUndo(const TXshLevelP &level);
  ~HookUndo();

  void onAdd();
  void assignHookSet(const HookSet &src) const;

  void undo() const;
  void redo() const;

  int getSize() const;
};

//=============================================================================
// HooksData
//-----------------------------------------------------------------------------

class HooksData : public DvMimeData {
  struct HookPosition {
    int m_id;
    TPointD m_aPos, m_bPos;

    HookPosition(int id, const TPointD &aPos, const TPointD &bPos)
        : m_id(id), m_aPos(aPos), m_bPos(bPos) {}
  };

  std::vector<HookPosition> m_hookPositions;
  TXshLevelP m_level;

public:
  HooksData(const TXshLevelP &level);
  ~HooksData();

  HooksData *clone() const;
  void storeHookPositions(const std::vector<int> &ids);
  void restoreHookPositions() const;
};

//=============================================================================
// HookSelection
//
// Derived from TSelection. Control which hooks are currently selected
// Note that a hook is made of two different parts (used e.g. to "pass the hook"
// between the feet in a walking cycle) that can be independently selected
// (1=A,2=B)
//-----------------------------------------------------------------------------

class HookSelection : public TSelection {
  TXshLevelP m_level;
  std::set<std::pair<int, int>> m_hooks;  // hookId, side : 1=A 2=B

public:
  HookSelection();
  ~HookSelection();

  TSelection *clone() const;
  void setLevel(const TXshLevelP &level) { m_level = level; }
  void select(int id, int side);
  void unselect(int id, int side);
  bool isSelected(int id, int side) const;
  void invertSelection(int id, int side);
  bool isEmpty() const;
  void selectNone() { m_hooks.clear(); }
  HookSet *getHookSet() const;
  bool select(const TSelection *s);
  void enableCommands();

  // commands
  void deleteSelectedHooks();
  void copySelectedHooks();
  void cutSelectedHooks();
  void pasteSelectedHooks();
};

#endif  // HOOK_SELECTION
