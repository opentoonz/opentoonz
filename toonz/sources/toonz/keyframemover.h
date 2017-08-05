#pragma once

#ifndef KEYFRAMEMOVER_H
#define KEYFRAMEMOVER_H

#include "xsheetdragtool.h"
#include "keyframeselection.h"
#include "keyframedata.h"
#include "toonz/txshcell.h"

class TXsheet;

//=============================================================================
// KeyframeMover
//-----------------------------------------------------------------------------

class KeyframeMover {
  //! Bitmask of qualifiers that change the behaviour of the Mover
  int m_qualifiers;

  typedef std::pair<int, int> KeyframePosition;  // row, col

  //! Start Selected keyframes
  std::set<KeyframePosition> m_startSelectedKeyframes;

  //! Store all keyframes position in selected StageObject.
  std::set<KeyframePosition> m_lastKeyframes;
  //! Store all keyframes in selected stageObject.
  TKeyframeData *m_lastKeyframeData;

  //! Helper method: returns the current xsheet
  TXsheet *getXsheet() const;

  // m_lastKeyframes and m_lastKeyframeData -> xsheet
  void setKeyframes();

  // m_lastKeyframes and m_lastKeyframeData <- xsheet
  void getKeyframes();

public:
  KeyframeMover();
  ~KeyframeMover();

  enum Qualifier {
    eCopyKeyframes =
        0x1,  // leaves a copy of Keyframes block at the starting point
    eMoveKeyframes =
        0x2,  // move keyframes only (if the destination keyframes are empty)
    eInsertKeyframes    = 0x4,
    eOverwriteKeyframes = 0x8
  };

  void start(TKeyframeSelection *selection, int qualifiers);

  int getQualifiers() const { return m_qualifiers; }

  bool moveKeyframes(int dr,
                     std::set<TKeyframeSelection::Position> &newPositions,
                     TKeyframeSelection *selection = 0);

  void undoMoveKeyframe() { setKeyframes(); }
};

//=============================================================================
// KeyframeMoverTool
//-----------------------------------------------------------------------------

class KeyframeMoverTool final : public XsheetGUI::DragTool {
  TKeyframeSelection m_startSelection;
  int m_offset;
  int m_firstRow, m_firstCol;
  bool m_selecting;
  bool m_justMovement;
  TPointD m_startPos, m_curPos;
  KeyframeMover *m_mover;
  bool m_firstKeyframeMovement;

  //! Helper method: returns the current keyframe selection
  TKeyframeSelection *getSelection();

  void ctrlSelect(int row, int col);
  void shiftSelect(int row, int col);

  void rectSelect(int row, int col);

public:
  KeyframeMoverTool(XsheetViewer *viewer, bool justMovement = false);

  bool canMove(const TPoint &pos);

  void onCellChange(int row, int col);

  void onClick(const QMouseEvent *event) override;
  void onDrag(const QMouseEvent *event) override;
  void onRelease(const CellPosition &pos) override;
  void drawCellsArea(QPainter &p) override;
};

#endif
