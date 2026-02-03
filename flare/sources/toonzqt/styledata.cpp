

#include "styledata.h"
#include "tcolorstyles.h"

//=============================================================================
// StyleData
//-----------------------------------------------------------------------------

StyleData::StyleData() {}

//-----------------------------------------------------------------------------

StyleData::~StyleData() {
  int i;
  for (i = 0; i < (int)m_styles.size(); i++) delete m_styles[i].second;
}

//-----------------------------------------------------------------------------

void StyleData::addStyle(int styleIndex, TColorStyle *style) {
  m_styles.push_back(std::make_pair(styleIndex, style));
}

//-----------------------------------------------------------------------------

TColorStyle *StyleData::getStyle(int index) const {
  assert(0 <= index && index < (int)m_styles.size());
  return m_styles[index].second;
}

//-----------------------------------------------------------------------------

int StyleData::getStyleIndex(int index) const {
  assert(0 <= index && index < (int)m_styles.size());
  return m_styles[index].first;
}

//-----------------------------------------------------------------------------

StyleData *StyleData::clone() const {
  StyleData *data = new StyleData();
  for (int i = 0; i < getStyleCount(); i++)
    data->addStyle(getStyleIndex(i), getStyle(i)->clone());
  return data;
}
