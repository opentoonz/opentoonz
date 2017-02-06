#pragma once

#ifndef COLUMNFAN_INCLUDED
#define COLUMNFAN_INCLUDED

#include "tcommon.h"

#include <vector>

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
*/
//=============================================================================

//! Shared base class
class DVAPI ColumnFan {
public:
  ColumnFan() { }
  virtual ~ColumnFan() { }

  //! Set column \b col not folded.
  virtual void activate(int col) = 0;
  //! Fold column \b col.
  virtual void deactivate(int col) = 0;
  //! Return true if column \b col is active, column is not folded, else return false.
  virtual bool isActive(int col) const = 0;

  virtual bool isEmpty() const = 0;
  virtual int count() const = 0;
};

//! Knows which columns are folded and which are unfolded
class DVAPI ColumnFanFoldData : public ColumnFan {
  std::vector<bool> m_columns; // true if active
public:

  ColumnFanFoldData() { }

  virtual void activate(int col) override;
  virtual void deactivate(int col) override;
  virtual bool isActive(int col) const override;
  virtual bool isEmpty() const override { return m_columns.size() != 0; }
  virtual int count() const override { return m_columns.size(); }

  void trim();
  void saveData(TOStream &os);
  void loadData(TIStream &is);
};

//! Knows how much each column occupies
/*!
Class provides column layer-axis coordinate too.
It's possible to know column index by column layer-axis coordinate,
colToLayerAxis() and vice versa, layerAxisToCol().
*/
class DVAPI ColumnFanGeometry : public ColumnFan {
  class Column {
    int m_extra; //! extra width added when the layer is unfolded
  public:
    bool m_active;
    int m_pos;

    Column() : m_active(true), m_pos(0), m_extra (0) {}
    Column(bool active, int extra) : m_active(active), m_pos(0), m_extra (extra) { }

    int width(int unfolded) const;
  };
  std::vector<Column> m_columns;
  std::map<int, int> m_table;
  int m_firstFreePos;
  int m_unfolded;

  /*!
  Called by activate() and deactivate() to update columns coordinates.
  */
  void update();
public:

  ColumnFanGeometry();

  void updateExtras(const ColumnFan *from, const std::vector<int> &extras);

  virtual void activate(int col) override;
  virtual void deactivate(int col) override;
  virtual bool isActive(int col) const override;
  virtual bool isEmpty() const override { return m_columns.empty(); }
  virtual int count() const override { return m_columns.size(); }

  //! Adjust column sizes when switching orientation
  void setDimension(int unfolded);

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
};

#endif
