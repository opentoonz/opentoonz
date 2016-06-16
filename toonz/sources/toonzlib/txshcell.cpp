

#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"

//=============================================================================

TImageP TXshCell::getImage(bool toBeModified, int subsampling) const {
  if (TXshSimpleLevel *sl = getSimpleLevel())
    return sl->getFrame(m_frameId, toBeModified ? ImageManager::toBeModified
                                                : ImageManager::none,
                        subsampling);

  return TImageP();
}

//-----------------------------------------------------------------------------

TXshSimpleLevel *TXshCell::getSimpleLevel() const {
  return m_level ? m_level->getSimpleLevel() : 0;
}

//-----------------------------------------------------------------------------

TXshSoundLevel *TXshCell::getSoundLevel() const {
  return m_level ? m_level->getSoundLevel() : 0;
}

//-----------------------------------------------------------------------------

TXshSoundTextLevel *TXshCell::getSoundTextLevel() const {
  return m_level ? m_level->getSoundTextLevel() : 0;
}

//-----------------------------------------------------------------------------

TXshPaletteLevel *TXshCell::getPaletteLevel() const {
  return m_level ? m_level->getPaletteLevel() : 0;
}

//-----------------------------------------------------------------------------

TXshZeraryFxLevel *TXshCell::getZeraryFxLevel() const {
  return m_level ? m_level->getZeraryFxLevel() : 0;
}

//-----------------------------------------------------------------------------

TXshChildLevel *TXshCell::getChildLevel() const {
  return m_level ? m_level->getChildLevel() : 0;
}

//-----------------------------------------------------------------------------

TPalette *TXshCell::getPalette() const {
  TXshSimpleLevel *sl = getSimpleLevel();
  if (sl)
    return sl->getPalette();
  else
    return 0;
}
