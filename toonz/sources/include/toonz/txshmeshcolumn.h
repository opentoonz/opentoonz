#pragma once

#ifndef TXSHMESHCOLUMN_H
#define TXSHMESHCOLUMN_H

#include "toonz/txshcolumn.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//*******************************************************************************
//    TXshMeshColumn  declaration
//*******************************************************************************

class DVAPI TXshMeshColumn : public TXshCellColumn {
  PERSIST_DECLARATION(TXshMeshColumn)

public:
  TXshMeshColumn();

  TXshColumn::ColumnType getColumnType() const override { return eMeshType; }
  TXshMeshColumn *getMeshColumn() override { return this; }

  TXshColumn *clone() const override;

  bool canSetCell(const TXshCell &cell) const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &is) override;

private:
  // Not copiable
  TXshMeshColumn(const TXshMeshColumn &);
  TXshMeshColumn &operator=(const TXshMeshColumn &);
};

//---------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TXshMeshColumn>;
#endif

typedef TSmartPointerT<TXshMeshColumn> TXshMeshColumnP;

#endif  // TXSHMESHCOLUMN_H
