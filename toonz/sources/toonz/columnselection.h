#pragma once

#ifndef TCOLUMNSELECTION_H
#define TCOLUMNSELECTION_H

#include "toonzqt/selection.h"
#include "tgeometry.h"
#include <set>
#include "toonz/txshlevel.h"

class ReframePopup;

//=============================================================================
// TColumnSelection
//-----------------------------------------------------------------------------

class TColumnSelection final : public TSelection {
  std::set<int> m_indices;
  ReframePopup *m_reframePopup;

public:
  TColumnSelection();
  ~TColumnSelection();

  bool isEmpty() const override;
  void selectColumn(int col, bool on = true);
  void selectNone() override;

  bool isColumnSelected(int col) const;

  const std::set<int> &getIndices() const { return m_indices; }

  void enableCommands() override;
  void setAlternativeCommandNames();

  void copyColumns();
  void pasteColumns();
  void pasteColumnsAbove();
  void deleteColumns();
  void cutColumns();
  void insertColumns();
  void insertColumnsAbove();

  void collapse();
  void explodeChild();
  void resequence();
  void cloneChild();

  void hideColumns();

  void reframeCells(int count);
  void reframe1Cells() { reframeCells(1); }
  void reframe2Cells() { reframeCells(2); }
  void reframe3Cells() { reframeCells(3); }
  void reframe4Cells() { reframeCells(4); }

  void reframeWithEmptyInbetweens();
  static void getLevelSetFromColumnIndices(const std::set<int>& indices,
      std::set<TXshLevel*>& levelSet);
};

#endif  // TCELLSELECTION_H
