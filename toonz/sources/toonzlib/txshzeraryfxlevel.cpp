

#include "toonz/txshzeraryfxlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshzeraryfxcolumn.h"

//-----------------------------------------------------------------------------

DEFINE_CLASS_CODE(TXshZeraryFxLevel, 51)

PERSIST_IDENTIFIER(TXshZeraryFxLevel, "zeraryFxLevel")

//-----------------------------------------------------------------------------

TXshZeraryFxLevel::TXshZeraryFxLevel()
    : TXshLevel(m_classCode, L""), m_zeraryFxColumn(0) {
  m_type = ZERARYFX_XSHLEVEL;
}

//-----------------------------------------------------------------------------

TXshZeraryFxLevel::~TXshZeraryFxLevel() {
  if (m_zeraryFxColumn) m_zeraryFxColumn->release();
}

//-------------------------------------------------------------------

void TXshZeraryFxLevel::setColumn(TXshZeraryFxColumn *column) {
  m_zeraryFxColumn = column;
  m_zeraryFxColumn->addRef();
}

//-----------------------------------------------------------------------------

void TXshZeraryFxLevel::loadData(TIStream &is) {}

//-----------------------------------------------------------------------------

void TXshZeraryFxLevel::saveData(TOStream &os) {}

//-----------------------------------------------------------------------------

void TXshZeraryFxLevel::load() {}

//-----------------------------------------------------------------------------

void TXshZeraryFxLevel::save() {}
