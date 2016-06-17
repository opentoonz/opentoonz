

#include "palettedata.h"
#include "tpalette.h"

//=============================================================================
// PaletteData
//-----------------------------------------------------------------------------

PaletteData *PaletteData::clone() const {
  PaletteData *data = new PaletteData();
  data->setPaletteData(m_palette, m_pageIndex, m_styleIndicesInPage);
  return data;
}

//-----------------------------------------------------------------------------

void PaletteData::setPaletteData(TPalette *palette, int pageIndex,
                                 std::set<int> styleIndicesInPage) {
  m_palette            = palette;
  m_pageIndex          = pageIndex;
  m_styleIndicesInPage = styleIndicesInPage;
}

//-----------------------------------------------------------------------------

void PaletteData::setPalette(TPalette *palette) {
  m_palette   = palette;
  m_pageIndex = -1;
  m_styleIndicesInPage.clear();
}
