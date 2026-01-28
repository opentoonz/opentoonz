#pragma once

#ifndef TXSHLEVELCOLUMN_INCLUDED
#define TXSHLEVELCOLUMN_INCLUDED

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

// Forward declarations
class TLevelColumnFx;
class TXshCell;
class TXshLevel;

//=============================================================================
//! The TXshLevelColumn class provides a column of levels in xsheet and allows
//! its management.
/*!
Inherits \b TXshCellColumn.

The class defines column of levels getLevelColumn(), more than \b TXshCellColumn
has a pointer to \b TLevelColumnFx getLevelColumnFx().

Note: Previously had an icon identification string for level icon management,
but it has been removed in the current implementation.
*/
//=============================================================================

class DVAPI TXshLevelColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshLevelColumn)

  TLevelColumnFx *m_fx;
  bool m_iconVisible;

public:
  // Icon visibility management
  bool isIconVisible() const { return m_iconVisible; }
  void setIconVisible(bool visible) { m_iconVisible = visible; }

  // Construction/Destruction
  TXshLevelColumn();
  ~TXshLevelColumn() override;

  // Type information
  TXshColumn::ColumnType getColumnType() const override;

  // Cell validation
  bool canSetCell(const TXshCell &cell) const override;

  // Column type casting
  TXshLevelColumn *getLevelColumn() override { return this; }

  // Cloning
  TXshColumn *clone() const override;

  // Serialization
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  // Effects access
  TLevelColumnFx *getLevelColumnFx() const;
  TFx *getFx() const override;

  // Cell operations
  // Used in TCellData::getNumbers
  // reservedLevel can be nonzero if the preferences option
  // "LinkColumnNameWithLevel" is ON and the column name is the same as some
  // level in the scene cast.
  bool setNumbers(int row, int rowCount, const TXshCell cells[],
                  TXshLevel *reservedLevel = nullptr);

  // Disable copying and moving
  TXshLevelColumn(const TXshLevelColumn &)            = delete;
  TXshLevelColumn &operator=(const TXshLevelColumn &) = delete;

  TXshLevelColumn(TXshLevelColumn &&)            = delete;
  TXshLevelColumn &operator=(TXshLevelColumn &&) = delete;

private:
  // Private helper methods if needed
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshLevelColumn>;
#endif

// Smart pointer typedef for TXshLevelColumn
typedef TSmartPointerT<TXshLevelColumn> TXshLevelColumnP;

#endif  // TXSHLEVELCOLUMN_INCLUDED
