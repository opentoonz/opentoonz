#pragma once

#ifndef CELLKEYFRAMEDATA_INCLUDED
#define CELLKEYFRAMEDATA_INCLUDED

#include "celldata.h"
#include "keyframedata.h"

//=============================================================================
// forward declarations
class TXsheet;
class TXshColumn;

//=============================================================================
// TCellKeyframeData
//-----------------------------------------------------------------------------

class TCellKeyframeData final : public DvMimeData {
  TCellData *m_cellData;
  TKeyframeData *m_keyframeData;

public:
  TCellKeyframeData(TCellData *cellData = 0, TKeyframeData *keyframeData = 0);
  TCellKeyframeData(const TCellKeyframeData *src);
  ~TCellKeyframeData();

  void setCellData(TCellData *data) { m_cellData = data; }
  TCellData *getCellData() const { return m_cellData; }
  void setKeyframeData(TKeyframeData *data) { m_keyframeData = data; }
  TKeyframeData *getKeyframeData() const { return m_keyframeData; }

  TCellKeyframeData *clone() const override {
    return new TCellKeyframeData(this);
  }
};

#endif
