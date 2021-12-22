#pragma once

#ifndef STYLESELECTION_INCLUDED
#define STYLESELECTION_INCLUDED

#include "toonzqt/selection.h"
#include "toonz/tpalettehandle.h"
#include "tpalette.h"
#include <set>
#include <QString>

class QByteArray;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TXsheetHandle;
class TXshLevelHandle;

//=============================================================================
// TStyleSelection
//-----------------------------------------------------------------------------

class DVAPI TStyleSelection final : public TSelection {
  TPaletteHandle *m_paletteHandle;

  // Used to change level palette; in other palette (cleanup, ...) xsheetHandle
  // is not necessary.
  TXsheetHandle *m_xsheetHandle;
  // for clearing cache when the pastestyle command is executed
  TXshLevelHandle *m_levelHandle;

  int m_pageIndex;
  std::set<int> m_styleIndicesInPage;

public:
  TStyleSelection();
  TStyleSelection(TStyleSelection *styleSelection)
      : m_paletteHandle(styleSelection->getPaletteHandle())
      , m_pageIndex(styleSelection->getPageIndex())
      , m_styleIndicesInPage(styleSelection->getIndicesInPage()) {}
  ~TStyleSelection();

  void select(int pageIndex);
  void select(int pageIndex, int styleIndexInPage, bool on);
  bool isSelected(int pageIndex, int styleIndexInPage) const;
  bool isPageSelected(int pageIndex) const;
  bool canHandleStyles();
  void selectNone() override;
  bool isEmpty() const override;
  int getStyleCount() const;
  TPaletteHandle *getPaletteHandle() const { return m_paletteHandle; }
  void setPaletteHandle(TPaletteHandle *paletteHandle) {
    m_paletteHandle = paletteHandle;
  }
  TPalette *getPalette() const { return m_paletteHandle->getPalette(); }
  int getPageIndex() const { return m_pageIndex; }
  const std::set<int> &getIndicesInPage() const { return m_styleIndicesInPage; }

  void getIndices(std::set<int> &indices) const;

  // Used to change level palette; in other palette (cleanup, ...) xsheetHandle
  // is not necessary.
  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  TXsheetHandle *getXsheetHandle() const { return m_xsheetHandle; }
  void setLevelHandle(TXshLevelHandle *levelHandle) {
    m_levelHandle = levelHandle;
  }

  // commands
  void cutStyles();
  void copyStyles();
  void pasteStyles();
  void pasteStylesValues(bool pasteName, bool pasteColor = true);
  void pasteStylesValue();
  void pasteStylesColor();
  void pasteStylesName();
  void deleteStyles();
  void eraseUnusedStyle();
  void blendStyles();
  void toggleLink();
  void eraseToggleLink();

  void enableCommands() override;

  void toggleKeyframe(int frame);

  // remove link from the studio palette (if linked)
  void removeLink();
  // get back the style from the studio palette (if linked)
  void getBackOriginalStyle();
  // return true if there is at least one linked style in the selection
  bool hasLinkedStyle();
};

#endif  // STYLESELECTION_INCLUDED
