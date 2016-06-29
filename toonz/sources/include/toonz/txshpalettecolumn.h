#pragma once

#ifndef TXSHPALETTECOLUMN_INCLUDED
#define TXSHPALETTECOLUMN_INCLUDED

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

//=============================================================================
// forward declarations
class TPaletteColumnFx;

//=============================================================================
//! The TXshPaletteColumn class represent a column in xsheet containing
//! TXshPaletteLevel
/*!Inherits \b TXshColumn.
\n
*/
//=============================================================================

class DVAPI TXshPaletteColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshPaletteColumn)
  TPaletteColumnFx *m_fx;

public:
  TXshPaletteColumn();
  ~TXshPaletteColumn();

  TXshColumn::ColumnType getColumnType() const override;

  TXshPaletteColumn *getPaletteColumn() override { return this; }

  TXshColumn *clone() const override;

  TPaletteColumnFx *getPaletteColumnFx() const { return m_fx; }
  TFx *getFx() const override;
  void setFx(TFx *fx);

  bool canSetCell(const TXshCell &cell) const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &is) override;

private:
  // not implemented
  TXshPaletteColumn(const TXshPaletteColumn &);
  TXshPaletteColumn &operator=(const TXshPaletteColumn &);
};

#ifdef _WIN32
template class TSmartPointerT<TXshPaletteColumn>;
#endif

typedef TSmartPointerT<TXshPaletteColumn> TXshPaletteColumnP;

#endif
