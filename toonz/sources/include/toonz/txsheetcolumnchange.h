#pragma once

#ifndef TXSHEETCOLUMNCHANGE_INCLUDED
#define TXSHEETCOLUMNCHANGE_INCLUDED

// TnzCore includes
#include "tcommon.h"
// TnzLib includes
#include "tstageobjectid.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFx;
class TDoubleParam;
//===========================================================

//*****************************************************************************************
//    TXsheetColumnChange  declaration
//*****************************************************************************************

class DVAPI TXsheetColumnChange {
public:
  enum OperationType { Insert, Remove, Move } m_type;

  int m_index1, m_index2;

public:
  TXsheetColumnChange(OperationType type, int index1, int index2 = -1)
      : m_type(type), m_index1(index1), m_index2(index2) {}
};

//*****************************************************************************************
//    TXsheetColumnChangeObserver  definition
//*****************************************************************************************

class DVAPI TXsheetColumnChangeObserver {
public:
  virtual void onChange(const TXsheetColumnChange &) = 0;
  // Call when adding fxs in which expression references may be included.
  // This can be happen when undoing removing fxs operation.
  virtual void onFxAdded(const std::vector<TFx *> &) = 0;
  // Call when adding stage objects in which expression references may be
  // included. This can be happen when undoing removing objects operation.
  virtual void onStageObjectAdded(const TStageObjectId) = 0;
  virtual bool isIgnored(TDoubleParam *)                = 0;
};

#endif  // TPARAMCHANGE_INCLUDED
