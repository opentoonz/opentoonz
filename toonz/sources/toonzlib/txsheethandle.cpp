

// TnzLib includes
#include "toonz/textureutils.h"

#include "toonz/txsheethandle.h"

//=============================================================================
// TXsheeHandle
//-----------------------------------------------------------------------------

TXsheetHandle::TXsheetHandle() : m_xsheet(0) {}

//-----------------------------------------------------------------------------

TXsheetHandle::~TXsheetHandle() {}

//-----------------------------------------------------------------------------

TXsheet *TXsheetHandle::getXsheet() const { return m_xsheet; }

//-----------------------------------------------------------------------------

void TXsheetHandle::setXsheet(TXsheet *xsheet) {
  if (m_xsheet == xsheet) return;

  m_xsheet = xsheet;

  if (m_xsheet) {
    texture_utils::invalidateTextures(m_xsheet);  // We'll be editing m_xsheet -
                                                  // so destroy every texture of
                                                  // his
    emit xsheetSwitched();
  }
}
