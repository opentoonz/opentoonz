#pragma once

#ifndef CONTROLPOINTEDITORTOOL_INCLUDED
#define CONTROLPOINTEDITORTOOL_INCLUDED

#include "toonzqt/selection.h"
#include "tool.h"
#include "tstroke.h"
#include "tcurves.h"
#include "controlpointselection.h"
#include "tproperty.h"
#include "tundo.h"

//=============================================================================
// ControlPointEditorTool
//-----------------------------------------------------------------------------

class ControlPointEditorTool final : public QObject, public TTool {
  Q_OBJECT

  // Q_DECLARE_TR_FUNCTIONS(ControlPointEditorTool)

  bool m_draw;
  bool m_isMenuViewed;
  int m_lastPointSelected;
  bool m_isImageChanged;
  ControlPointSelection m_selection;
  ControlPointEditorStroke m_controlPointEditorStroke;
  std::pair<int, int> m_moveSegmentLimitation;  // Indici dei punti di controllo
                                                // che limitano la curva da
                                                // muovere
  ControlPointEditorStroke m_moveControlPointEditorStroke;  // Usate per muovere
                                                            // la curva durante
                                                            // il drag.
  TRectD m_selectingRect;
  TPointD m_pos;

  TPropertyGroup m_prop;
  TBoolProperty
      m_autoSelectDrawing;  // Consente di scegliere se swichare tra i livelli.

  enum Action {
    NONE,
    RECT_SELECTION,
    CP_MOVEMENT,
    SEGMENT_MOVEMENT,
    IN_SPEED_MOVEMENT,
    OUT_SPEED_MOVEMENT
  };
  Action m_action;

  enum CursorType { NORMAL, ADD, EDIT_SPEED, EDIT_SEGMENT, NO_ACTIVE };
  CursorType m_cursorType;

  TUndo *m_undo;
  bool m_subscribedFrameChanged;

  ToonzExt::Selector m_selector;

  StrokeId makeStrokeId(TVectorImageP vi, int index);
  void subscribeFrameChanged();

public:
  ControlPointEditorTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  // da TSelectionOwner: chiamato quando la selezione corrente viene cambiata
  void onSelectionChanged() { invalidate(); }

  // da TSelectionOwner: chiamato quando si vuole ripristinare una vecchia
  // selezione
  // attualmente non usato
  bool select(const TSelection *) { return false; }
  ControlPointEditorStroke getControlPointEditorStroke() {
    return m_controlPointEditorStroke;
  };

  void initUndo();

  void getNearestStrokeColumnIndexes(std::vector<int> &indexes, TPointD pos);

  void drawMovingSegment();
  void drawControlPoint();
  void draw() override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void rightButtonDown(const TPointD &pos, const TMouseEvent &) override;

  void moveControlPoints(const TPointD &delta);
  void moveSpeed(const TPointD &delta, bool isIn);
  void moveSegment(const TPointD &delta, bool dragging, bool isShiftPressed);

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void addContextMenuItems(QMenu *menu) override;

  void linkSpeedInOut(int index);
  void unlinkSpeedInOut(int pointIndex);

  bool keyDown(QKeyEvent *event) override;
  void onEnter() override;
  void onLeave() override;
  bool onPropertyChanged(std::string propertyName) override;

  void onActivate() override;
  void onDeactivate() override;
  void onImageChanged() override;
  int getCursorId() const override;

  // returns true if the pressed key is recognized and processed.
  bool isEventAcceptable(QEvent *e) override;

  ControlPointSelection *getSelection() { return &m_selection; }
  ToonzExt::Selector *getSelector() override;

public slots:
  void frameSwitched();
};

#endif