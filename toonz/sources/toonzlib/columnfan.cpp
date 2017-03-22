#include "toonz/columnfan.h"

// TnzCore includes
#include "tstream.h"

// STD includss
#include <assert.h>

using std::vector;

//=============================================================================
// ColumnFanFoldData

void ColumnFanFoldData::activate(int col) {
  int m                       = m_columns.size();
  if (col < m) m_columns[col] = true;
}
void ColumnFanFoldData::deactivate(int col) {
  while ((int)m_columns.size() <= col) m_columns.push_back(true);
  m_columns[col] = false;
}
bool ColumnFanFoldData::isActive(int col) const {
  return 0 <= col && col < (int)m_columns.size() ? m_columns[col] : true;
}

void ColumnFanFoldData::trim() {
  int m = m_columns.size();
  int i;
  for (i = m - 1; i >= 0 && m_columns[i]; i--)
    ;
  i++;
  if (i < m) m_columns.erase(m_columns.begin() + i, m_columns.end());
}

//-----------------------------------------------------------------------------

void ColumnFanFoldData::saveData(TOStream &os) {
  int index, n = (int)m_columns.size();
  for (index = 0; index < n;) {
    while (index < n && m_columns[index]) index++;
    if (index < n) {
      int firstIndex = index;
      os << index;
      index++;
      while (index < n && !m_columns[index]) index++;
      os << index - firstIndex;
    }
  }
}

//-----------------------------------------------------------------------------

void ColumnFanFoldData::loadData(TIStream &is) {
  m_columns.clear();
  while (!is.eos()) {
    int index = 0, count = 0;
    is >> index >> count;
    int j;
    for (j = 0; j < count; j++) deactivate(index + j);
  }
}

//=============================================================================
// ColumnFanGeometry

namespace {
  const int FOLDED_SIZE = 9;
}

int ColumnFanGeometry::Column::width(int unfolded) const {
  if (m_active)
    return unfolded + m_extra;
  else
    return FOLDED_SIZE;
}

ColumnFanGeometry::ColumnFanGeometry() : m_firstFreePos(0), m_unfolded(74) {}

//-----------------------------------------------------------------------------

void ColumnFanGeometry::setDimension(int unfolded) {
  m_unfolded = unfolded;
  // folded always 9
  update();
}

//-----------------------------------------------------------------------------

void ColumnFanGeometry::update() {
//???  int lastPos     = -m_unfolded;
//???  bool lastActive = true;
  int from = 0, to;
  int m    = m_columns.size();
  int i;
  for (i = 0; i < m; i++) {
/*???
    bool active = m_columns[i].m_active;
    if (lastActive)
      lastPos += m_unfolded;
    else if (active)
      lastPos += FOLDED_SIZE;
    m_columns[i].m_pos = lastPos;
    lastActive         = active;
*/
    m_columns[i].m_pos = from;
    to                 = from + m_columns[i].width(m_unfolded);
    if (m_columns[i].m_active)  // advance if this is unfolded
      from = to;
    else if (i + 1 >= m || m_columns[i + 1].m_active)  // or next is unfolded
      from = to;
  }
  m_firstFreePos = from;

  m_table.clear();
  for (i = 0; i < m; i++)
    if (m_columns[i].m_active)
//???      m_table[m_columns[i].m_pos + m_unfolded - 1] = i;
      m_table[m_columns[i].m_pos + m_columns[i].width(m_unfolded) - 1] = i;
    else if (i + 1 < m && m_columns[i + 1].m_active)
      m_table[m_columns[i + 1].m_pos - 1] = i;
    else if (i + 1 == m)
      m_table[m_firstFreePos - 1] = i;
}

//-----------------------------------------------------------------------------

int ColumnFanGeometry::layerAxisToCol(int coord) const {
  if (coord < m_firstFreePos) {
    std::map<int, int>::const_iterator it = m_table.lower_bound(coord);
    if (it == m_table.end()) return -3;
    assert(it != m_table.end());
    return it->second;
  } else
    return m_columns.size() + (coord - m_firstFreePos) / m_unfolded;
}

//-----------------------------------------------------------------------------

int ColumnFanGeometry::colToLayerAxis(int col) const {
  int m = m_columns.size();
  if (col >= 0 && col < m)
    return m_columns[col].m_pos;
  else
    return m_firstFreePos + (col - m) * m_unfolded;
}

//-----------------------------------------------------------------------------

void ColumnFanGeometry::activate(int col) {
  int m = m_columns.size();
  if (col < m) //{
    m_columns[col].m_active = true;
/*???
    int i;
    for (i = m - 1; i >= 0 && m_columns[i].m_active; i--) {
    }
    i++;
    if (i < m) {
      m = i;
      m_columns.erase(m_columns.begin() + i, m_columns.end());
    }
  }
*/
  update();
}

//-----------------------------------------------------------------------------

void ColumnFanGeometry::deactivate(int col) {
  while ((int)m_columns.size() <= col) m_columns.push_back(Column());
  m_columns[col].m_active = false;
  update();
}

//-----------------------------------------------------------------------------

bool ColumnFanGeometry::isActive(int col) const {
  return 0 <= col && col < (int)m_columns.size() ? m_columns[col].m_active
                                                 : true;
}

//-----------------------------------------------------------------------------

// bool ColumnFan::isEmpty() const { return m_columns.empty(); }

//-----------------------------------------------------------------------------

void ColumnFanGeometry::updateExtras(const ColumnFan *from, const vector<int> &extras) {
/* ???
  for (int i = 0, n = (int)from.m_columns.size(); i < n; i++)
    if (!from.isActive(i)) deactivate(i);
//???
  m_columns = from->m_columns;
  update();
*/
  m_columns.clear();
/*???
  for (int i = 0; i < from->count(); i++)
  m_columns.push_back(Column(from->isActive(i)));
*/
  for (int i = 0; i < extras.size(); i++) {
    Column add(from->isActive(i), extras[i]);
    m_columns.push_back(add);
  }
  update();
}

//-----------------------------------------------------------------------------
/*??? old
void ColumnFan::saveData(
    TOStream &os) {  // only saves indices of folded columns
  int index, n = (int)m_columns.size();
  for (index = 0; index < n;) {
    while (index < n && m_columns[index].m_active) index++;
    if (index < n) {
      int firstIndex = index;
      os << index;
      index++;
      while (index < n && !m_columns[index].m_active) index++;
      os << index - firstIndex;
    }
  }
}

//-----------------------------------------------------------------------------

void ColumnFan::loadData(TIStream &is) {
  m_columns.clear();
  m_table.clear();
  m_firstFreePos = 0;
  while (!is.eos()) {
    int index = 0, count = 0;
    is >> index >> count;
    int j;
    for (j = 0; j < count; j++) deactivate(index + j);
  }
}
*/