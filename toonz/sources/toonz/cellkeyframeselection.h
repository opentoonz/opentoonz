#pragma once

#ifndef TCELLKEYFRAMESELECTION_H
#define TCELLKEYFRAMESELECTION_H

#include "toonzqt/selection.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "tgeometry.h"
#include <memory>

class TXsheetHandle;

//=============================================================================
// TCellKeyframeSelection
//-----------------------------------------------------------------------------

class TCellKeyframeSelection final : public TSelection {
  std::unique_ptr<TCellSelection> m_cellSelection;
  std::unique_ptr<TKeyframeSelection> m_keyframeSelection;
  TXsheetHandle *m_xsheetHandle = nullptr;

public:
  TCellKeyframeSelection(TCellSelection *cellSelection,
                         TKeyframeSelection *keyframeSelection);
  ~TCellKeyframeSelection() = default;  // unique_ptr handles deletion

  // Prohibit copying and moving
  TCellKeyframeSelection(const TCellKeyframeSelection &)            = delete;
  TCellKeyframeSelection &operator=(const TCellKeyframeSelection &) = delete;
  TCellKeyframeSelection(TCellKeyframeSelection &&)                 = delete;
  TCellKeyframeSelection &operator=(TCellKeyframeSelection &&)      = delete;

  TCellSelection *getCellSelection() const { return m_cellSelection.get(); }
  TKeyframeSelection *getKeyframeSelection() const {
    return m_keyframeSelection.get();
  }

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void enableCommands() override;

  bool isEmpty() const override;

  void copyCellsKeyframes();
  void pasteCellsKeyframes();
  void deleteCellsKeyframes();
  void cutCellsKeyframes();

  //! \note: r0, c0, r1, c1 can be in any order (r0>r1 or c0>c1)
  void selectCellsKeyframes(int r0, int c0, int r1, int c1);
  void selectCellKeyframe(int row, int col);
  void selectNone() override;
};

#endif  // TCELLKEYFRAMESELECTION_H
