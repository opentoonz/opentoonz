#pragma once

#ifndef T_PARAMUNDO_INCLUDED
#define T_PARAMUNDO_INCLUDED

#include "tparamchange.h"

#undef DVAPI
#undef DVVAR
#ifdef TAPPTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TParamUndoManager : public TParamObserver {  // singleton
protected:
  TParamUndoManager() {}
  virtual ~TParamUndoManager(){};

public:
  static TParamUndoManager *instance();
};

#endif
