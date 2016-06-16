#pragma once

#ifndef STYLESELECTION_INCLUDED
#define STYLESELECTION_INCLUDED

#include "toonzqt/selection.h"
#include "tpalette.h"
#include <set>
#include <QString>

class QByteArray;

//=============================================================================
// TStyleSelection
//-----------------------------------------------------------------------------

class TStyleSelection : public TSelection {
  TPaletteP m_palette;
  int m_pageIndex;
  std::set<int> m_styleIndicesInPage;

public:
  TStyleSelection();
  TStyleSelection(TStyleSelection *styleSelection)
      : m_palette(styleSelection->getPalette())
      , m_pageIndex(styleSelection->getPageIndex())
      , m_styleIndicesInPage(styleSelection->getIndicesInPage()) {}
  ~TStyleSelection();

  void select(const TPaletteP &palette, int pageIndex);
  void select(const TPaletteP &palette, int pageIndex, int styleIndexInPage,
              bool on);
  bool isSelected(const TPaletteP &palette, int pageIndex,
                  int styleIndexInPage) const;
  bool isPageSelected(const TPaletteP &palette, int pageIndex) const;
  void selectNone();
  bool isEmpty() const;
  int getStyleCount() const;
  const TPaletteP &getPalette() const { return m_palette; }
  int getPageIndex() const { return m_pageIndex; }
  const std::set<int> &getIndicesInPage() const { return m_styleIndicesInPage; }

  // commands
  void cutStyles();
  void copyStyles();
  void pasteStyles();
  void pasteStylesValue();
  void deleteStyles();
  void blendStyles();
  void toggleLink();

  void enableCommands();

  // pageIndex, indicesInPage, paletteGlobalName --> byte array
  static QByteArray toByteArray(int pageIndex,
                                const std::set<int> &indicesInPage,
                                const QString paletteGlobalName);

  // pageIndex, indicesInPage, paletteGlobalName <-- byte array
  static void fromByteArray(QByteArray &byteArray, int &pageIndex,
                            std::set<int> &indicesInPage,
                            QString &paletteGlobalName);

  // selection (pageIndex, indicesInPage) -> byte array
  QByteArray toByteArray() const;

  static const char *getMimeType();
};

#endif  // STYLESELECTION_INCLUDED
