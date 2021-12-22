#pragma once

#ifndef TCOLUMNHANDLE_H
#define TCOLUMNHANDLE_H

#include <QObject>
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TXshColumn;

//=============================================================================
// TColumnHandle
//-----------------------------------------------------------------------------

class DVAPI TColumnHandle final : public QObject {
  Q_OBJECT

  TXshColumn *m_column;
  int m_columnIndex;

public:
  TColumnHandle();
  ~TColumnHandle();

  TXshColumn *getColumn() const;
  void setColumn(TXshColumn *column);

  int getColumnIndex() const { return m_columnIndex; }
  void setColumnIndex(int index);

  void notifyColumnIndexSwitched() { emit columnIndexSwitched(); }
signals:

  void columnIndexSwitched();
};

#endif  // TCOLUMNHANDLE_H
