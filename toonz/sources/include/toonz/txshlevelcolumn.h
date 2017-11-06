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

//=============================================================================
// forward declarations
class TLevelColumnFx;
class TXshCell;

//=============================================================================
//! The TXshLevelColumn class provides a column of levels in xsheet and allows
//! its management.
/*!Inherits \b TXshCellColumn.
\n The class defines column of levels getLevelColumn(), more than \b
TXshCellColumn has
   a pointer to \b TLevelColumnFx getLevelColumnFx() and a \b string to identify
icon.
   The string is an icon identification helpful to level icon management,
getIcon()
   updateIcon().
*/
//=============================================================================

class DVAPI TXshLevelColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshLevelColumn)
  TLevelColumnFx *m_fx;
  std::string m_iconId;

  bool m_iconVisible;

public:
  bool isIconVisible() { return m_iconVisible; }
  void setIconVisible(bool visible) { m_iconVisible = visible; }

  /*!
Constructs a TXshLevelColumn with default value.
*/
  TXshLevelColumn();
  /*!
Destroys the TXshLevelColumn object.
*/
  ~TXshLevelColumn();

  TXshColumn::ColumnType getColumnType() const override;

  /*!
Return true if \b cell is empty or level of \b cell isn't a \b
TXshZeraryFxLevel.
*/
  bool canSetCell(const TXshCell &cell) const override;

  /*!
Return \b TXshLevelColumn.
*/
  TXshLevelColumn *getLevelColumn() override { return this; }

  /*!
Clone column and return a pointer to the new \b TXshColumn cloned.
*/
  TXshColumn *clone() const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  /*!
Return a pointer to \b TLevelColumnFx \b m_fx.
*/
  TLevelColumnFx *getLevelColumnFx() const;

  /*!
Return \b TFx.
*/
  TFx *getFx() const override;

  // Used in TCellData::getNumbers
  bool setNumbers(int row, int rowCount, const TXshCell cells[]);

private:
  // not implemented
  TXshLevelColumn(const TXshLevelColumn &);
  TXshLevelColumn &operator=(const TXshLevelColumn &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshLevelColumn>;
#endif

typedef TSmartPointerT<TXshLevelColumn> TXshLevelColumnP;

#endif
