

#include "toonz/tonionskinmaskhandle.h"

//=============================================================================
// TColumnHandle
//-----------------------------------------------------------------------------

TOnionSkinMaskHandle::TOnionSkinMaskHandle() : m_onionSkinMask() {}

//-----------------------------------------------------------------------------

TOnionSkinMaskHandle::~TOnionSkinMaskHandle() {}

//-----------------------------------------------------------------------------

const OnionSkinMask &TOnionSkinMaskHandle::getOnionSkinMask() const {
  return m_onionSkinMask;
}

//-----------------------------------------------------------------------------

void TOnionSkinMaskHandle::setOnionSkinMask(
    const OnionSkinMask &onionSkinMask) {
  m_onionSkinMask = onionSkinMask;
  emit onionSkinMaskSwitched();
}

//-----------------------------------------------------------------------------

void TOnionSkinMaskHandle::clear() { m_onionSkinMask.clear(); }
