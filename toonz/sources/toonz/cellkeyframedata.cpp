

#include "cellkeyframedata.h"

//=============================================================================
//  TCellKeyframeData
//-----------------------------------------------------------------------------

TCellKeyframeData::TCellKeyframeData(TCellData *cellData,
                                     TKeyframeData *keyframeData)
    : m_cellData(cellData), m_keyframeData(keyframeData) {}

//-----------------------------------------------------------------------------

TCellKeyframeData::TCellKeyframeData(const TCellKeyframeData *src) {
  m_cellData     = src->getCellData()->clone();
  m_keyframeData = src->getKeyframeData()->clone();
}

//-----------------------------------------------------------------------------

TCellKeyframeData::~TCellKeyframeData() {}
