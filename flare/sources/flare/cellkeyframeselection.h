#pragma once

#ifndef TCELLKEYFRAMESELECTION_H
#define TCELLKEYFRAMESELECTION_H

#include "toonzqt/selection.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "tgeometry.h"
#include <set>

class TXsheetHandle;

//=============================================================================
// TCellKeyframeSelection
//-----------------------------------------------------------------------------

class TCellKeyframeSelection final : public TSelection {
  TCellSelection *m_cellSelection;
  TKeyframeSelection *m_keyframeSelection;

  TXsheetHandle *m_xsheetHandle;

public:
  TCellKeyframeSelection(TCellSelection *cellSelection,
                         TKeyframeSelection *keyframeSelection);
  ~TCellKeyframeSelection();

  TCellSelection *getCellSelection() { return m_cellSelection; }
  TKeyframeSelection *getKeyframeSelection() { return m_keyframeSelection; }

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void enableCommands() override;

  bool isEmpty() const override;

  void copyCellsKeyframes();
  void pasteCellsKeyframes();
  void deleteCellsKeyframes();
  void cutCellsKeyframes();

  //! \note: puo' anche essere r0>r1 o c0>c1
  void selectCellsKeyframes(int r0, int c0, int r1, int c1);
  void selectCellKeyframe(int row, int col);
  void selectNone() override;

  /*
  void getSelectedCells(int &r0, int &c0, int &r1, int &c1) const;
  Range getSelectedCells() const;

bool isCellSelected(int r , int c) const;
bool isRowSelected(int row) const;
bool isColSelected(int col) const;

bool areAllColSelectedLocked() const;

  //commands
void reverseCells();
  void swingCells();
  void incrementCells();
void duplicateCells();
  void randomCells();
  void stepCells(int count);
  void eachCells(int count);
void step2Cells() {stepCells(2);}
void step3Cells() {stepCells(3);}
void step4Cells() {stepCells(4);}
void each2Cells() {eachCells(2);}
void each3Cells() {eachCells(3);}
void each4Cells() {eachCells(4);}
  void rollupCells();
  void rolldownCells();

  void setKeyframes();
  void cloneLevel();
void insertCells();

void openTimeStretchPopup();*/
};

#endif  // TCELLKEYFRAMESELECTION_H
