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
//!The ColumnFan class is used to menage display columns.
/*!The class allows to fold a column by column index, deactivate(), to
   open folded columns, activate() and to know if column is folded or not, isActive().

   Class provides column x-axis coordinate too.
   It's possible to know column index by column x-axis coordinate, colToX()
   and vice versa, xToCol().
*/
//=============================================================================

class DVAPI ColumnFan
{
	class Column
	{
	public:
		bool m_active;
		int m_pos;
		Column() : m_active(true), m_pos(0) {}
	};
	std::vector<Column> m_columns;
	std::map<int, int> m_table;
	int m_firstFreePos;

	/*!
    Called by activate() and deactivate() to update columns coordinates.
  */
	void update();

public:
	/*!
    Constructs a ColumnFan with default value.
  */
	ColumnFan();

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
    Return column index of column in x-axis coordinate \b x.
    \sa colToX()
  */
	int xToCol(int x) const;
	/*!
    Return column x-axis coordinate of column identify with \b col.
    \sa xToCol()
  */
	int colToX(int col) const;

	bool isEmpty() const;

	void saveData(TOStream &os);
	void loadData(TIStream &is);
};

#endif
