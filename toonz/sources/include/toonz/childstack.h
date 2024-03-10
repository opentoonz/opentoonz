#pragma once

#ifndef CHILDSTACK_INCLUDED
#define CHILDSTACK_INCLUDED

#include "tcommon.h"

#include "toonz/txshchildlevel.h"

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
class TXsheet;
class ToonzScene;
class TAffine;
class TXshChildLevel;

//=============================================================================
//! The ChildStack class provides a stack of xsheet and allows its management.
/*!The class has a vector of \b ChildStack::Node, a pointer to current \b
   TXsheet
   getXsheet(), and a pointer to current \b ToonzScene \b m_scene.
   The \b ChildStack::Node vector is as a subxsheet vector.

   Through this class you can create, open openChild(), close closeChild() or
   clear clear() a sub-xsheet.

   The class provides a collection of functions that return set of subxsheet
   properties, getAncestor(), getAncestorCount(), getAncestorAffine(),
   getTopXsheet().
*/

//=============================================================================
//! The Node class is a container of element necessary to define a sub-xsheet.
/*!
   The class contain a pointer to \b TXsheet \b m_xsheet, two integer to
   identify column
   \b m_col and row \b m_row, a \b TXshChildLevelP \b m_cl and a bool \b
   m_justCreated.
*/

class AncestorNode {
public:
  TXsheet *m_xsheet;
  int m_row, m_col;
  std::map<int, int> m_rowTable;
  TXshChildLevelP m_cl;
  bool m_justCreated;
  AncestorNode()
      : m_xsheet(0), m_row(0), m_col(0), m_rowTable(), m_justCreated(false) {}
};

class DVAPI ChildStack {
  std::vector<AncestorNode *> m_stack;
  TXsheet *m_xsheet;
  ToonzScene *m_scene;

public:
  /*!
Constructs a ChildStack with default value and current \b scene.
*/
  ChildStack(ToonzScene *scene);
  /*!
Destroys the ChildStack object.
*/
  ~ChildStack();

  /*!
Clear child stack.
*/
  void clear();

  /*!
Open a sub-xsheet in cell \b row, \b col. Return true if it's possible.
\n
If cell \b row, \b col is empty create a new sub-xheet empty and return true,
if cell contains a sub-xsheet set this as current xsheet and return true,
if cell contains another element return false.
*/
  bool openChild(int row, int col);

  /*!
Close current xsheet if is a sub-xsheet. Return false if stack is empty.
Set \b row, \b col to sub-xsheet row and column.
*/
  bool closeChild(int &row, int &col);

  /*!
Create a sub-xsheet in cell \b row, \b col and returns the new child level.
\n
Column col must be empty,
The current xsheet remains unchanged

*/
  TXshChildLevel *createChild(int row, int col);

  /*!
Return ancestor number, size of \b ChildStack::Node vector.
*/
  int getAncestorCount() const;

  /*!
Return a pointer to top xsheet of stack, if stack is empty to current xsheet.
*/
  TXsheet *getTopXsheet() const;
  /*!
Return a pointer to current xsheet.
*/
  TXsheet *getXsheet() const { return m_xsheet; }

  // DO NOT USE. It's just for a very dirty make up in
  // xshcolumnselection.cpp
  void setXsheet(TXsheet *xsheet) { m_xsheet = xsheet; }

  /*!
Return the ancestor and respective frame that contains \b row.
\n
Useful by edit in place. Oss.: \b row of current xsheet.
*/
  std::pair<TXsheet *, int> getAncestor(int row) const;

  /*!
Set aff to ancestor affine in \b row. Return true if all ancestors are
visible in \b row.
*/
  bool getAncestorAffine(TAffine &aff, int row) const;

  AncestorNode *getAncestorInfo(int ancestorDepth);

private:
  // not implemented
  ChildStack(const ChildStack &);
  ChildStack &operator=(const ChildStack &);
};

#endif
