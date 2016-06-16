#pragma once

#ifndef TCOLUMNSET_INCLUDED
#define TCOLUMNSET_INCLUDED

#include "tsmartpointer.h"

#undef DVAPI
#undef DVVAR
#ifdef TXSHEET_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

class DVAPI TColumnHeader : public TSmartObject {
  DECLARE_CLASS_CODE

  int m_index;  //!< The header's index in a columns set
  int m_pos;    //!< The header's screen X pos in a columns viewer
  int m_width;  //!< The header's width in a columns viewer

  bool m_inColumnsSet;  //!< (TO BE REMOVED ASAP) Whether the header
                        //!< belongs to a columns set. Should be
                        //!< redirected to a negative m_index.
public:
  TColumnHeader();
  virtual ~TColumnHeader() {}

  int getIndex() const { return m_index; }
  int getPos() const { return m_pos; }
  int getX0() const { return m_pos; }
  int getX1() const { return m_pos + m_width - 1; }

  void getCoords(int &x0, int &x1) const {
    x0 = m_pos;
    x1 = m_pos + m_width - 1;
  }

  int getWidth() const { return m_width; }

  bool inColumnsSet() const { return m_inColumnsSet; }

private:
  template <class T>
  friend class TColumnSetT;
};

//---------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TColumnHeader>;
#endif

typedef TSmartPointerT<TColumnHeader> TColumnHeaderP;

//=============================================================================

template <class T>
class TColumnSetT {
  typedef TSmartPointerT<T> ColumnP;

  std::vector<ColumnP> m_columns;
  int m_defaultWidth;

  static bool compareColumnPos(const int pos, const ColumnP &ch2) {
    return pos <= ch2->getX1();
  }

public:
  TColumnSetT(int defaultWidth = 100) : m_defaultWidth(defaultWidth) {}
  ~TColumnSetT() {}

  int getColumnCount() const { return m_columns.size(); }

  void clear() { m_columns.clear(); }

  //---------------------------------------------------------

  void col2pos(int index, int &x0, int &x1) const {
    assert(index >= 0);
    int columnCount = (int)m_columns.size();
    if (index < columnCount)
      m_columns[index]->getCoords(x0, x1);
    else {
      x0 = (columnCount > 0 ? m_columns.back()->getX1() + 1 : 0) +
           (index - columnCount) * m_defaultWidth;
      x1 = x0 + m_defaultWidth - 1;
    }
  }

  //---------------------------------------------------------
  // versione con upper_bound

  int pos2col(int pos) const {
    // n.b. endPos e' la coordinata del primo pixel non occupato da colonne
    int endPos = m_columns.empty() ? 0 : m_columns.back()->getX1() + 1;
    if (pos >= endPos)
      return m_columns.size() + (pos - endPos) / m_defaultWidth;

    typename std::vector<ColumnP>::const_iterator loc;
    loc = std::upper_bound(m_columns.begin(), m_columns.end(), pos,
                           compareColumnPos);
    return std::distance(m_columns.begin(), loc);
  }

  //---------------------------------------------------------

  const ColumnP &getColumn(int index) const {
    static const ColumnP empty;

    if (index >= 0 && index < (int)m_columns.size()) return m_columns[index];

    return empty;
  }

  //---------------------------------------------------------

  const ColumnP &touchColumn(int index, int type = 0) {
    assert(index >= 0);

    const int count = m_columns.size();
    if (index < count) return m_columns[index];

    for (int i = count; i <= index; ++i) {
      int cType   = (i != index) ? 0 : type;
      ColumnP col = T::createEmpty(cType);

      m_columns.push_back(col);
    }

    update(count);

    assert(index == (int)m_columns.size() - 1);
    return m_columns[index];
  }

  //---------------------------------------------------------

  const ColumnP &insertColumn(int index, const ColumnP &column) {
    // assert(column && column->m_index < 0);
    assert(column && !column->m_inColumnsSet);

    if (index > 0) touchColumn(index - 1);

    m_columns.insert(m_columns.begin() + index, column);
    update(index);

    return column;
  }

  //---------------------------------------------------------

  const ColumnP removeColumn(int index) {
    assert(index >= 0);

    int columnCount = m_columns.size();
    if (index >= columnCount)  // Shouldn't be asserted instead ?
      return ColumnP();

    ColumnP column = m_columns[index];
    // column->m_index = -1;                           // We should enforce
    // this. Unfortunately, must be tested extensively.
    column->m_inColumnsSet = false;

    m_columns.erase(m_columns.begin() + index);
    update(index);

    return column;
  }

  //---------------------------------------------------------

  void rollLeft(int index, int count) {
    assert(index >= 0);
    assert(count > 1);

    int columnCount                        = m_columns.size();
    if (index + count > columnCount) count = columnCount - index;

    if (count < 2) return;

    assert(0 <= index && index + count - 1 < columnCount);
    assert(count > 0);

    int i = index, j = index + count - 1;
    ColumnP tmp = m_columns[i];

    for (int k = i; k < j; ++k) m_columns[k] = m_columns[k + 1];
    m_columns[j]                             = tmp;

    update(0);
  }

  //---------------------------------------------------------

  void rollRight(int index, int count) {
    assert(index >= 0);
    assert(count > 1);

    int columnCount                        = m_columns.size();
    if (index + count > columnCount) count = columnCount - index;

    if (count < 2) return;

    assert(0 <= index && index + count - 1 < columnCount);
    assert(count > 0);

    int i = index, j = index + count - 1;
    ColumnP tmp = m_columns[j];

    for (int k = j; k > i; --k) m_columns[k] = m_columns[k - 1];
    m_columns[i]                             = tmp;

    update(0);
  }

  //---------------------------------------------------------

  void update(int fromIdx) {
    int pos = 0, index = 0;
    if (fromIdx > 0) {
      assert(fromIdx <= (int)m_columns.size());

      const ColumnP &left = m_columns[fromIdx - 1];

      pos   = left->getX1() + 1;
      index = left->m_index + 1;
    }

    int c, cCount = m_columns.size();
    for (c = fromIdx; c != cCount; ++c) {
      const ColumnP &col = m_columns[c];

      col->m_pos   = pos;
      col->m_index = index++;
      pos += col->m_width;

      col->m_inColumnsSet = true;
    }
  }

private:
  // Not copyable
  TColumnSetT(const TColumnSetT &);
  TColumnSetT &operator=(const TColumnSetT &);
};

#endif  // TCOLUMNSET_INCLUDED
