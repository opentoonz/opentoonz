#pragma once

#ifndef PALETTE_DATA_INCLUDED
#define PALETTE_DATA_INCLUDED

#include "toonzqt/dvmimedata.h"
#include "tpalette.h"
#include <set>
#include <vector>

class TPalette;

//=============================================================================
// PaletteData
//-----------------------------------------------------------------------------
/*! Useful to set data in drag and drop event styles or palette.
*/
class PaletteData final : public DvMimeData {
  TPalette *m_palette;
  std::set<int> m_styleIndicesInPage;
  int m_pageIndex;

public:
  PaletteData() : m_palette(0), m_pageIndex(-1) {}

  ~PaletteData() {}

  PaletteData *clone() const override;

  void setPaletteData(TPalette *palette, int pageIndex,
                      std::set<int> styleIndicesInPage);
  void setPalette(TPalette *palette);

  bool hasStyleIndeces() const {
    return m_pageIndex != -1 && m_styleIndicesInPage.size() > 0;
  }

  bool hasOnlyPalette() const { return !hasStyleIndeces(); }

  TPalette *getPalette() const { return m_palette; }
  int getPageIndex() const { return m_pageIndex; }

  const std::set<int> &getIndicesInPage() const { return m_styleIndicesInPage; }
};

#endif
