#pragma once

#ifndef SUBCAMERAMANAGER_INCLUDED
#define SUBCAMERAMANAGER_INCLUDED

#include "tgeometry.h"

//==============================================================================

//  Forward declarations
class QMouseEvent;
class QPaintEvent;
class SceneViewer;
class QPoint;

//==============================================================================

//=====================================
//    SceneViewerInteractiveGadget
//-------------------------------------

/*!
  SceneViewerInteractiveGadget constitutes the base class for implementing
  objects that use the SceneViewer for user editing.
  The class contains the basic QEvent-related methods that a SceneViewer
  will invoke once an instance of this class has been assigned to it.
  The class implements a priority number which identifies the precedence
  between gadgets. Lower priority gadgets interact later and are drawn before
  with respect to higher priority ones.
  Interaction methods such as mouse events also return a boolean to specify
  whether
  the event allows further processing of lower priority gadgets.
  \note Under development yet.
*/
class SceneViewerInteractiveGadget {
public:
  virtual int getPriority() const { return 0; }

  virtual bool mousePressEvent(SceneViewer *viewer, QMouseEvent *me) {
    return true;
  }
  virtual bool mouseMoveEvent(SceneViewer *viewer, QMouseEvent *me) {
    return true;
  }
  virtual bool mouseReleaseEvent(SceneViewer *viewer, QMouseEvent *me) {
    return true;
  }

  virtual void draw(SceneViewer *viewer, QPaintEvent *pe) {}
};

//==============================================================================

//================================
//    PreviewSubCameraManager
//--------------------------------

class PreviewSubCameraManager final : public SceneViewerInteractiveGadget {
  TRect m_editingInterestRect;
  UCHAR m_dragType;
  bool m_clickAndDrag;

  QPoint m_mousePressPos;
  TPointD m_cameraMousePressPos;
  bool m_mousePressed;

public:
  enum EditPreviewDrag {
    NODRAG       = 0x0,
    OUTER_LEFT   = 0x1,
    INNER_LEFT   = 0x2,
    OUTER_RIGHT  = 0x4,
    INNER_RIGHT  = 0x8,
    OUTER_TOP    = 0x10,
    INNER_TOP    = 0x20,
    OUTER_BOTTOM = 0x40,
    INNER_BOTTOM = 0x80,
    OUTER        = OUTER_LEFT | OUTER_RIGHT | OUTER_TOP | OUTER_BOTTOM,
    INNER        = INNER_LEFT | INNER_RIGHT | INNER_TOP | INNER_BOTTOM,
    DRAG_LEFT    = OUTER_LEFT | INNER,
    DRAG_RIGHT   = OUTER_RIGHT | INNER,
    DRAG_TOP     = OUTER_TOP | INNER,
    DRAG_BOTTOM  = OUTER_BOTTOM | INNER
  };

  PreviewSubCameraManager();
  ~PreviewSubCameraManager();

public:
  static PreviewSubCameraManager *instance();

  UCHAR getDragType() const { return m_dragType; }
  void setDragType(UCHAR dragType) { m_dragType = dragType; }

  TRect getEditingCameraInterestRect() const;
  void setEditingCameraInterestRect(const TRect &rect) {
    m_editingInterestRect = rect;
  }

  TRectD getEditingCameraInterestStageRect() const;

  bool mousePressEvent(SceneViewer *viewer, const TMouseEvent &event);
  bool mouseMoveEvent(SceneViewer *viewer, const TMouseEvent &event);
  bool mouseReleaseEvent(SceneViewer *viewer);

  void deleteSubCamera(SceneViewer *viewer);

private:
  TPointD winToCamera(SceneViewer *viewer, const QPoint &pos) const;
  TPoint cameraToWin(SceneViewer *viewer, const TPointD &cameraPos) const;

  UCHAR getSubCameraDragEnum(SceneViewer *viewer, const QPoint &mousePos);
  TPoint getSubCameraDragDistance(SceneViewer *viewer, const QPoint &mousePos);
};

#endif  // SUBCAMERAMANAGER_INCLUDED
