

#include "tparamundo.h"
//#include "tparam.h"
#include "tundo.h"

class ParamUndoManager : public TParamUndoManager {
public:
  ParamUndoManager() {}
  ~ParamUndoManager() {}
  void onChange(const TParamChange &change);
};

//-------------------------------------------------------------------

TParamUndoManager *TParamUndoManager::instance() {
  static ParamUndoManager instance;
  return &instance;
}

//-------------------------------------------------------------------

void ParamUndoManager::onChange(const TParamChange &change) {
  assert(0);

  // if (!change.m_undoing && !change.m_dragging)
  //  TUndoManager::manager()->add(change.createUndo());
}

//-------------------------------------------------------------------
