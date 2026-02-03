#pragma once

#ifndef FUNCTIONPANELTOOLS_H
#define FUNCTIONPANELTOOLS_H

#include "toonzqt/functionpanel.h"
#include "tdoublekeyframe.h"
#include "toonz/doubleparamcmd.h"

#include <QList>
// class QMouseEvent;
class KeyframeSetter;
class TDoubleParam;

class FunctionPanel::DragTool {
public:
  DragTool() {}
  virtual ~DragTool() {}
  virtual void click(QMouseEvent *e) {}
  virtual void drag(QMouseEvent *e) {}
  virtual void release(QMouseEvent *e) {}

  virtual void draw(QPainter &p) {}
};

class MoveFrameDragTool final : public FunctionPanel::DragTool {
  FunctionPanel *m_panel;
  TFrameHandle *m_frameHandle;

public:
  MoveFrameDragTool(FunctionPanel *panel, TFrameHandle *frameHandle);
  void drag(QMouseEvent *e) override;
};

class PanDragTool final : public FunctionPanel::DragTool {
  FunctionPanel *m_panel;
  QPoint m_oldPos;
  bool m_xLocked, m_yLocked;

public:
  PanDragTool(FunctionPanel *panel, bool xLocked, bool yLocked);
  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
};

class ZoomDragTool final : public FunctionPanel::DragTool {
  FunctionPanel *m_panel;
  QPoint m_startPos, m_oldPos;
  int m_zoomType;

public:
  enum ZoomType { FrameZoom = 1, ValueZoom = 2 };
  ZoomDragTool(FunctionPanel *panel, ZoomType zoomType)
      : m_panel(panel), m_zoomType((int)zoomType) {}

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;
};

class RectSelectTool final : public FunctionPanel::DragTool {
  FunctionPanel *m_panel;
  TDoubleParam *m_curve;
  QPoint m_startPos;
  QRect m_rect;

public:
  RectSelectTool(FunctionPanel *panel, TDoubleParam *curve)
      : m_panel(panel), m_curve(curve) {}

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;

  void draw(QPainter &painter) override;
};

class MovePointDragTool final : public FunctionPanel::DragTool {
  FunctionPanel *m_panel;
  QPoint m_startPos, m_oldPos;
  double m_deltaFrame;
  // length and kIndex of speedinout handles which can change because of point
  // moving
  double m_speed0Length;
  int m_speed0Index;
  double m_speed1Length;
  int m_speed1Index;
  std::vector<KeyframeSetter *> m_setters;
  bool m_groupEnabled;
  FunctionSelection *m_selection;

public:
  MovePointDragTool(FunctionPanel *panel, TDoubleParam *curve);
  ~MovePointDragTool();

  void addKeyframe2(int kIndex);
  // void addKeyframe(int kIndex) {m_setter->selectKeyframe(kIndex);}
  void createKeyframe(double frame);
  void selectKeyframes(double frame);

  void setSelection(FunctionSelection *selection);

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;
};

class MoveHandleDragTool final : public FunctionPanel::DragTool {
public:
  typedef FunctionPanel::Handle Handle;

private:
  FunctionPanel *m_panel;
  TDoubleParam *m_curve;
  QPoint m_startPos;  //, m_oldPos;
  double m_deltaFrame;
  Handle m_handle;
  int m_kIndex;
  TDoubleKeyframe m_keyframe;
  KeyframeSetter m_setter;
  double m_segmentWidth;
  QPointF m_nSpeed;  // speedInOut constraint

public:
  FunctionTreeModel::ChannelGroup *m_channelGroup;

  MoveHandleDragTool(FunctionPanel *panel, TDoubleParam *curve, int kIndex,
                     Handle handle);

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;
};

class MoveGroupHandleDragTool final : public FunctionPanel::DragTool {
public:
  typedef FunctionPanel::Handle Handle;

private:
  FunctionPanel *m_panel;
  double m_keyframePosition;
  Handle m_handle;
  std::vector<std::pair<TDoubleKeyframe, KeyframeSetter *>> m_setters;

public:
  MoveGroupHandleDragTool(FunctionPanel *panel, double keyframePosition,
                          Handle handle);
  ~MoveGroupHandleDragTool();

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;
};

class StretchPointDragTool final : public FunctionPanel::DragTool {
private:
  FunctionPanel *m_panel;
  TDoubleParam *m_curve;
  double m_clickedFrame;

  struct keyInfo {
    int kIndex;
    double orgFramePos;
    TPointD orgSpeedIn;
    TPointD orgSpeedOut;
    KeyframeSetter *setter;
  };

  QList<keyInfo> m_keys;
  bool m_moveLeft;
  double m_previousRange;

public:
  StretchPointDragTool(FunctionPanel *panel, TDoubleParam *curve, int leftId,
                       int rightId, bool moveLeft);
  ~StretchPointDragTool();

  void click(QMouseEvent *e) override;
  void drag(QMouseEvent *e) override;
  void release(QMouseEvent *e) override;
};
#endif
