#pragma once

#ifndef TCELLSELECTION_H
#define TCELLSELECTION_H

#include "toonzqt/selection.h"
#include "tgeometry.h"
#include <set>

class TimeStretchPopup;
class ReframePopup;
class TXshCell;

//=============================================================================
// TCellSelection
//-----------------------------------------------------------------------------

class TCellSelection final : public TSelection {
  TimeStretchPopup *m_timeStretchPopup;
  ReframePopup *m_reframePopup;

public:
  class Range {
  public:
    int m_c0, m_r0, m_c1, m_r1;
    Range();
    bool isEmpty() const;
    bool contains(int r, int c) const;
    int getRowCount() const;
    int getColCount() const;
  };

private:
  Range m_range;

public:
  TCellSelection();
  TCellSelection(Range range) : m_range(range) {}
  ~TCellSelection();

  void enableCommands() override;

  bool isEmpty() const override;

  void copyCells();
  void pasteCells();
  void deleteCells();
  void cutCells();
  void cutCells(bool withoutCopy);

  /*- セルの上書きペースト -*/
  void overWritePasteCells();

  //! \note: puo' anche essere r0>r1 o c0>c1
  void selectCells(int r0, int c0, int r1, int c1);
  void selectCell(int row, int col);
  void selectNone() override;

  void getSelectedCells(int &r0, int &c0, int &r1, int &c1) const;
  Range getSelectedCells() const;

  bool isCellSelected(int r, int c) const;
  bool isRowSelected(int row) const;
  bool isColSelected(int col) const;

  bool areAllColSelectedLocked() const;

  // commands
  void reverseCells();
  void swingCells();
  void incrementCells();
  void randomCells();
  void stepCells(int count);
  void eachCells(int count);
  void resetStepCells();
  void increaseStepCells();
  void decreaseStepCells();
  void step2Cells() { stepCells(2); }
  void step3Cells() { stepCells(3); }
  void step4Cells() { stepCells(4); }
  void each2Cells() { eachCells(2); }
  void each3Cells() { eachCells(3); }
  void each4Cells() { eachCells(4); }
  void rollupCells();
  void rolldownCells();

  void setKeyframes();
  void pasteKeyframesInto();

  void cloneLevel();
  void insertCells();

  void openTimeStretchPopup();

  void dRenumberCells();
  void dPasteCells();

  void reframeCells(int count);
  void reframe1Cells() { reframeCells(1); }
  void reframe2Cells() { reframeCells(2); }
  void reframe3Cells() { reframeCells(3); }
  void reframe4Cells() { reframeCells(4); }

  void reframeWithEmptyInbetweens();

  void renameCells(TXshCell &cell);
  // rename cells for each columns with correspondent item in the list
  void renameMultiCells(QList<TXshCell> &cells);

  static bool isEnabledCommand(std::string commandId);
};

#endif  // TCELLSELECTION_H
