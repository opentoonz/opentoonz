#pragma once

#ifndef TXSHSOUNDTEXTCOLUMN_INCLUDED
#define TXSHSOUNDTEXTCOLUMN_INCLUDED

#include "toonz/txshcolumn.h"
#include "toonz/txshcell.h"

#include <QList>

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
//! The TXshSoundTextColumn class provides a sound column in xsheet and allows
//! its management through cell concept.
/*!Inherits \b TXshCellColumn. */
//=============================================================================

class DVAPI TXshSoundTextColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshSoundTextColumn)

public:
  TXshSoundTextColumn();
  ~TXshSoundTextColumn();

  TXshColumn::ColumnType getColumnType() const override;
  TXshSoundTextColumn *getSoundTextColumn() override { return this; }

  void createSoundTextLevel(int row, QList<QString> textList);

  bool canSetCell(const TXshCell &cell) const override;

  TXshColumn *clone() const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSoundTextColumn>;
#endif
typedef TSmartPointerT<TXshSoundTextColumn> TXshSoundTextColumnP;

#endif  // TXSHSOUNDTEXTCOLUMN_INCLUDED
