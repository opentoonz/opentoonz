#pragma once

#ifndef STYLE_DATA_INCLUDED
#define STYLE_DATA_INCLUDED

#include "dvmimedata.h"
#include <vector>

class TColorStyle;

//=============================================================================
// StyleData
//-----------------------------------------------------------------------------

class StyleData : public DvMimeData {
  std::vector<std::pair<int, TColorStyle *>> m_styles;

public:
  StyleData();
  ~StyleData();

  StyleData *clone() const;

  void addStyle(int styleIndex, TColorStyle *style);  // gets ownership

  int getStyleCount() const { return (int)m_styles.size(); }
  TColorStyle *getStyle(int index) const;
  int getStyleIndex(int index) const;
};

#endif
