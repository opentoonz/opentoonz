

#include "toonz/tcolumnhandle.h"

#include "toonz/txshcolumn.h"

//=============================================================================
// TColumnHandle
//-----------------------------------------------------------------------------

TColumnHandle::TColumnHandle() : m_columnIndex(-1), m_column(0) {}

//-----------------------------------------------------------------------------

TColumnHandle::~TColumnHandle() {}

//-----------------------------------------------------------------------------

TXshColumn *TColumnHandle::getColumn() const { return m_column; }

//-----------------------------------------------------------------------------

void TColumnHandle::setColumn(TXshColumn *column) {
  if (m_column == column) return;
  m_column = column;
}

//-----------------------------------------------------------------------------

void TColumnHandle::setColumnIndex(int index) {
  if (m_columnIndex == index) return;
  m_columnIndex = index;
  emit columnIndexSwitched();
}
