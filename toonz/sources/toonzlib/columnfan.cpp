

#include "toonz/columnfan.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tstream.h"

// STD includss
#include <assert.h>

//=============================================================================
// ColumnFan

ColumnFan::ColumnFan()
    : m_firstFreePos(0)
    , m_unfolded(74)
    , m_folded(9)
    , m_cameraActive(true)
    , m_cameraColumnDim(22) {}

//-----------------------------------------------------------------------------

void ColumnFan::setDimensions(int unfolded, int cameraColumn) {
  m_unfolded        = unfolded;
  m_cameraColumnDim = cameraColumn;
  // folded always 9
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::update() {
  int lastPos     = -m_unfolded;
  bool lastActive = true;
  int m           = m_columns.size();
  int i;
  for (i = 0; i < m; i++) {
    bool active = m_columns[i].m_active;
    if (lastActive)
      lastPos += m_unfolded;
    else if (active)
      lastPos += m_folded;
    m_columns[i].m_pos = lastPos;
    lastActive         = active;
  }
  m_firstFreePos = lastPos + (lastActive ? m_unfolded : m_folded);
  m_table.clear();
  for (i = 0; i < m; i++)
    if (m_columns[i].m_active)
      m_table[m_columns[i].m_pos + m_unfolded - 1] = i;
    else if (i + 1 < m && m_columns[i + 1].m_active)
      m_table[m_columns[i + 1].m_pos - 1] = i;
    else if (i + 1 == m)
      m_table[m_firstFreePos - 1] = i;
}

//-----------------------------------------------------------------------------

int ColumnFan::layerAxisToCol(int coord) const {
  if (Preferences::instance()->isXsheetCameraColumnVisible()) {
    int firstCol =
        m_cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && !m_columns[0].m_active) ? 0 : m_folded);
    if (coord < firstCol) return -1;
    coord -= firstCol;
  }
  if (coord < m_firstFreePos) {
    std::map<int, int>::const_iterator it = m_table.lower_bound(coord);
    if (it == m_table.end()) return -3;
    assert(it != m_table.end());
    return it->second;
  } else
    return m_columns.size() + (coord - m_firstFreePos) / m_unfolded;
}

//-----------------------------------------------------------------------------

int ColumnFan::colToLayerAxis(int col) const {
  int m        = m_columns.size();
  int firstCol = 0;
  if (Preferences::instance()->isXsheetCameraColumnVisible()) {
    if (col < -1) return -m_cameraColumnDim;
    if (col < 0) return 0;
    firstCol =
        m_cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && !m_columns[0].m_active) ? 0 : m_folded);
  }
  if (col >= 0 && col < m)
    return firstCol + m_columns[col].m_pos;
  else
    return firstCol + m_firstFreePos + (col - m) * m_unfolded;
}

//-----------------------------------------------------------------------------

void ColumnFan::activate(int col) {
  int m = m_columns.size();
  if (col < 0) {
    m_cameraActive = true;
    return;
  }
  if (col < m) {
    m_columns[col].m_active = true;
    int i;
    for (i = m - 1; i >= 0 && m_columns[i].m_active; i--) {
    }
    i++;
    if (i < m) {
      m = i;
      m_columns.erase(m_columns.begin() + i, m_columns.end());
    }
  }
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::deactivate(int col) {
  if (col < 0) {
    m_cameraActive = false;
    return;
  }
  while ((int)m_columns.size() <= col) m_columns.push_back(Column());
  m_columns[col].m_active = false;
  update();
}

//-----------------------------------------------------------------------------

bool ColumnFan::isActive(int col) const {
  return 0 <= col && col < (int)m_columns.size()
             ? m_columns[col].m_active
             : col < 0 ? m_cameraActive : true;
}

//-----------------------------------------------------------------------------

bool ColumnFan::isEmpty() const { return m_columns.empty(); }

//-----------------------------------------------------------------------------

void ColumnFan::copyFoldedStateFrom(const ColumnFan &from) {
  m_cameraActive = from.m_cameraActive;
  for (int i = 0, n = (int)from.m_columns.size(); i < n; i++)
    if (!from.isActive(i)) deactivate(i);
}

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

void ColumnFan::rollLeftFoldedState(int index, int count) {
  assert(index >= 0);
  int columnCount = m_columns.size();
  if (columnCount <= index) return;
  if (index + count - 1 > columnCount) count = columnCount - index + 1;
  if (count < 2) return;

  int i = index, j = index + count - 1;
  bool tmp = isActive(i);

  for (int k = i; k < j; ++k) {
    if (isActive(k) && !isActive(k + 1))
      deactivate(k);
    else if (!isActive(k) && isActive(k + 1))
      activate(k);
  }
  if (isActive(j) && !tmp)
    deactivate(j);
  else if (!isActive(j) && tmp)
    activate(j);

  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::rollRightFoldedState(int index, int count) {
  assert(index >= 0);

  int columnCount = m_columns.size();
  if (columnCount <= index) return;
  if (index + count - 1 > columnCount) count = columnCount - index + 1;
  if (count < 2) return;

  int i = index, j = index + count - 1;
  bool tmp = isActive(j);

  for (int k = j; k > i; --k) {
    if (isActive(k) && !isActive(k - 1))
      deactivate(k);
    else if (!isActive(k) && isActive(k - 1))
      activate(k);
  }
  if (isActive(i) && !tmp)
    deactivate(i);
  else if (!isActive(i) && tmp)
    activate(i);

  update();
}
