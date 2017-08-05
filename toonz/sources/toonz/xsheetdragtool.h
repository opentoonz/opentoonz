#pragma once

#ifndef XSHEET_DRAG_TOOL_H
#define XSHEET_DRAG_TOOL_H

#include "tgeometry.h"
#include "cellposition.h"

// forward declaration
class QPainter;
class XsheetViewer;
class QMouseEvent;
class QDropEvent;
class TXshSoundColumn;

namespace XsheetGUI {

class DragTool {
  XsheetViewer *m_viewer;

public:
  DragTool(XsheetViewer *viewer);
  virtual ~DragTool();

  XsheetViewer *getViewer() const { return m_viewer; }

  //! chiama l'update del QWidget relativo
  void refreshCellsArea();
  void refreshRowsArea();
  void refreshColumnsArea();

  virtual void onClick(const CellPosition &pos) {}
  virtual void onDrag(const CellPosition &pos) {}
  virtual void onRelease(const CellPosition &pos) {}

  virtual void onClick(const QMouseEvent *event);
  virtual void onDrag(const QMouseEvent *event);
  virtual void onRelease(const QMouseEvent *event);

  virtual void onClick(const QDropEvent *event){};
  virtual void onDrag(const QDropEvent *event){};
  virtual void onRelease(const QDropEvent *event){};

  virtual void drawCellsArea(QPainter &p) {}
  virtual void drawRowsArea(QPainter &p) {}
  virtual void drawColumnsArea(QPainter &p) {}

  static DragTool *makeSelectionTool(XsheetViewer *viewer);
  static DragTool *makeLevelMoverTool(XsheetViewer *viewer);
  static DragTool *makeLevelExtenderTool(XsheetViewer *viewer,
                                         bool invert = false);
  static DragTool *makeSoundLevelModifierTool(XsheetViewer *viewer);
  static DragTool *makeKeyframeMoverTool(XsheetViewer *viewer);
  static DragTool *makeCellKeyframeMoverTool(XsheetViewer *viewer);
  static DragTool *makeKeyFrameHandleMoverTool(XsheetViewer *viewer,
                                               bool isEaseOut, int keyRow);
  static DragTool *makeNoteMoveTool(XsheetViewer *viewer);

  static DragTool *makeKeyOnionSkinMaskModifierTool(XsheetViewer *viewer,
                                                    bool isFos);
  static DragTool *makeCurrentFrameModifierTool(XsheetViewer *viewer);
  static DragTool *makePlayRangeModifierTool(XsheetViewer *viewer);
  static DragTool *makeSoundScrubTool(XsheetViewer *viewer,
                                      TXshSoundColumn *sc);
  static DragTool *makeDragAndDropDataTool(XsheetViewer *viewer);

  static DragTool *makeColumnSelectionTool(XsheetViewer *viewer);
  static DragTool *makeColumnLinkTool(XsheetViewer *viewer);
  static DragTool *makeColumnMoveTool(XsheetViewer *viewer);
  static DragTool *makeVolumeDragTool(XsheetViewer *viewer);
};

void setPlayRange(int r0, int r1, int step, bool withUndo = true);

bool getPlayRange(int &r0, int &r1, int &step);

bool isPlayRangeEnabled();

}  // namespace XsheetGUI

#endif
