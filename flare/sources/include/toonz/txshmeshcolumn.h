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
//    TXshMeshColumn declaration
//*******************************************************************************

class DVAPI TXshMeshColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshMeshColumn)

  bool m_iconVisible;

public:
  // Icon visibility management
  bool isIconVisible() const { return m_iconVisible; }
  void setIconVisible(bool visible) { m_iconVisible = visible; }

  // Construction
  TXshMeshColumn();

  // Type information
  TXshColumn::ColumnType getColumnType() const override { return eMeshType; }
  TXshMeshColumn *getMeshColumn() override { return this; }

  // Cloning
  TXshColumn *clone() const override;

  // Cell validation
  bool canSetCell(const TXshCell &cell) const override;

  // Serialization
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  // Disable copying and moving
  TXshMeshColumn(const TXshMeshColumn &)            = delete;
  TXshMeshColumn &operator=(const TXshMeshColumn &) = delete;

  TXshMeshColumn(TXshMeshColumn &&)            = delete;
  TXshMeshColumn &operator=(TXshMeshColumn &&) = delete;

private:
  // Private helper methods can be added here if needed
};

//---------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TXshMeshColumn>;
#endif

// Smart pointer typedef for TXshMeshColumn
typedef TSmartPointerT<TXshMeshColumn> TXshMeshColumnP;

#endif  // TXSHMESHCOLUMN_H
