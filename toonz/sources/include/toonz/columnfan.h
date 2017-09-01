#pragma once

#ifndef COLUMNFAN_INCLUDED
#define COLUMNFAN_INCLUDED

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

class TOStream;
class TIStream;

//=============================================================================
//! The ColumnFan class is used to menage display columns.
/*!The class allows to fold a column by column index, deactivate(), to
   open folded columns, activate() and to know if column is folded or not,
   isActive().

   Class provides column layer-axis coordinate too.
   It's possible to know column index by column layer-axis coordinate,
   colToLayerAxis()
   and vice versa, layerAxisToCol().
*/
//=============================================================================

class DVAPI ColumnFan {
  class Column {
  public:
    bool m_active;
    int m_pos;
    Column() : m_active(true), m_pos(0) {}
  };
  std::vector<Column> m_columns;
  std::map<int, int> m_table;
  int m_firstFreePos;
  int m_unfolded, m_folded;

  /*!
Called by activate() and deactivate() to update columns coordinates.
*/
  void update();

public:
  /*!
Constructs a ColumnFan with default value.
*/
  ColumnFan();

  //! Adjust column sizes when switching orientation
  void setDimension(int unfolded);

  /*!
Set column \b col not folded.
\sa deactivate() and isActive()
*/
  void activate(int col);
  /*!
Fold column \b col.
\sa activate() and isActive()
*/
  void deactivate(int col);
  /*!
Return true if column \b col is active, column is not folded, else return false.
\sa activate() and deactivate()
*/
  bool isActive(int col) const;

  /*!
Return column index of column in layer axis (x for vertical timeline, y for
horizontal).
\sa colToLayerAxis()
*/
  int layerAxisToCol(int layerAxis) const;
  /*!
Return layer coordinate (x for vertical timeline, y for horizontal)
of column identified by \b col.
\sa layerAxisToCol()
*/
  int colToLayerAxis(int col) const;

  void copyFoldedStateFrom(const ColumnFan &from);

  bool isEmpty() const;

  void saveData(TOStream &os);
  void loadData(TIStream &is);
};

#endif
