

// Toonz app
#include "tapp.h"

// Toonz stage structures
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tcamera.h"
#include "toonz/tframehandle.h"

// Scene viewer
#include "sceneviewer.h"

// Qt event-handling includes
#include <QMouseEvent>

#include "subcameramanager.h"

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {
inline bool bitwiseContains(UCHAR flag, UCHAR state) {
  return flag == (flag | state);
}

inline bool bitwiseExclude(UCHAR flag, UCHAR state) {
  return bitwiseContains(~state, flag);
}
}

//********************************************************************************
//    Classes implementation
//********************************************************************************

//================================
//    PreviewSubCameraManager
//--------------------------------

PreviewSubCameraManager::PreviewSubCameraManager()
    : m_mousePressed(false), m_dragType(NODRAG), m_clickAndDrag(false) {}

//----------------------------------------------------------------------

PreviewSubCameraManager::~PreviewSubCameraManager() {}

//----------------------------------------------------------------------

PreviewSubCameraManager *PreviewSubCameraManager::instance() {
  static PreviewSubCameraManager theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------------

TRect PreviewSubCameraManager::getEditingCameraInterestRect() const {
  if (m_mousePressed)
    return m_editingInterestRect;
  else {
    // Return the actual current camera's interest rect
    TCamera *currCamera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    return currCamera->getInterestRect();
  }
}

//-----------------------------------------------------------------------------

TRectD PreviewSubCameraManager::getEditingCameraInterestStageRect() const {
  TCamera *currCamera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();

  if (m_mousePressed)
    // Build the stage rect associated with m_editingInterestRect
    return currCamera->getCameraToStageRef() *
           TRectD(m_editingInterestRect.x0, m_editingInterestRect.y0,
                  m_editingInterestRect.x1 + 1, m_editingInterestRect.y1 + 1);
  else
    // Return the actual current camera's stage interest rect
    return currCamera->getInterestStageRect();
}

//-----------------------------------------------------------------------------

TPointD PreviewSubCameraManager::winToCamera(SceneViewer *viewer,
                                             const QPointF &pos) const {
  TPointD worldPos(viewer->winToWorld(pos));

  TApp *app = TApp::instance();
  TAffine stageToWorldRef(app->getCurrentXsheet()->getXsheet()->getCameraAff(
      app->getCurrentFrame()->getFrame()));

  TCamera *currCamera = app->getCurrentScene()->getScene()->getCurrentCamera();
  return currCamera->getStageToCameraRef() * stageToWorldRef.inv() * worldPos;
}

//-----------------------------------------------------------------------------

TPointD PreviewSubCameraManager::cameraToWin(SceneViewer *viewer,
                                             const TPointD &cameraPos) const {
  TApp *app = TApp::instance();
  TAffine stageToWorldRef(app->getCurrentXsheet()->getXsheet()->getCameraAff(
      app->getCurrentFrame()->getFrame()));
  TCamera *currCamera = app->getCurrentScene()->getScene()->getCurrentCamera();

  TPointD worldPos(stageToWorldRef * currCamera->getCameraToStageRef() *
                   cameraPos);
  return viewer->worldToPos(worldPos);
}

//----------------------------------------------------------------------

bool PreviewSubCameraManager::mousePressEvent(SceneViewer *viewer,
                                              const TMouseEvent &event) {
  if (viewer->is3DView()) return true;

  m_mousePressed  = true;
  m_mousePressPos = event.mousePos() * getDevPixRatio();
  m_dragType      = getSubCameraDragEnum(viewer, m_mousePressPos);

  if (bitwiseExclude(m_dragType, OUTER))
    m_cameraMousePressPos = winToCamera(viewer, m_mousePressPos);

  return false;
}

//----------------------------------------------------------------------

bool PreviewSubCameraManager::mouseMoveEvent(SceneViewer *viewer,
                                             const TMouseEvent &event) {
  if (viewer->is3DView()) return true;
  QPointF curPos(event.mousePos() * getDevPixRatio());
  if (event.buttons() == Qt::LeftButton) {
    if (!bitwiseContains(m_dragType, INNER)) {
      if (abs(curPos.x() - m_mousePressPos.x()) > 10 ||
          abs(curPos.y() - m_mousePressPos.y()) > 10)
        m_clickAndDrag = true;
    }

    if (m_clickAndDrag == true) {
      // Write the temporary preview subcamera to current camera
      TPointD worldMousePressPos(viewer->winToWorld(m_mousePressPos));
      TPointD worldCurPos(viewer->winToWorld(curPos));

      TApp *app = TApp::instance();
      TAffine cameraAffInv(
          app->getCurrentXsheet()
              ->getXsheet()
              ->getCameraAff(app->getCurrentFrame()->getFrame())
              .inv());

      worldMousePressPos = cameraAffInv * worldMousePressPos;
      worldCurPos        = cameraAffInv * worldCurPos;

      TRectD worldPreviewSubCameraRect(
          std::min(worldMousePressPos.x, worldCurPos.x),
          std::min(worldMousePressPos.y, worldCurPos.y),
          std::max(worldMousePressPos.x, worldCurPos.x),
          std::max(worldMousePressPos.y, worldCurPos.y));

      TCamera *camera = app->getCurrentScene()->getScene()->getCurrentCamera();
      // camera->setInterestStageRect(worldPreviewSubCameraRect);

      TRectD previewSubCameraD(camera->getStageToCameraRef() *
                               worldPreviewSubCameraRect);
      m_editingInterestRect =
          TRect(previewSubCameraD.x0, previewSubCameraD.y0,
                previewSubCameraD.x1 - 1, previewSubCameraD.y1 - 1) *
          TRect(camera->getRes());

      viewer->update();
    } else {
      TPoint dragDistance = getSubCameraDragDistance(viewer, curPos);

      // Adjust the camera subrect
      TCamera *camera =
          TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
      TRect subRect(camera->getInterestRect());

      if (bitwiseExclude(m_dragType, OUTER))
        subRect += dragDistance;
      else {
        if (bitwiseContains(m_dragType, DRAG_LEFT))
          subRect.x0 = subRect.x0 + dragDistance.x;
        else if (bitwiseContains(m_dragType, DRAG_RIGHT))
          subRect.x1 = subRect.x1 + dragDistance.x;
        if (bitwiseContains(m_dragType, DRAG_BOTTOM))
          subRect.y0 = subRect.y0 + dragDistance.y;
        else if (bitwiseContains(m_dragType, DRAG_TOP))
          subRect.y1 = subRect.y1 + dragDistance.y;
      }

      m_editingInterestRect = subRect * TRect(camera->getRes());

      viewer->update();
    }
  } else {
    UCHAR dragEnum = getSubCameraDragEnum(viewer, curPos);

    if (dragEnum == NODRAG)
      viewer->setCursor(Qt::ArrowCursor);
    else if (bitwiseExclude(dragEnum, OUTER))
      viewer->setCursor(Qt::SizeAllCursor);
    else
      switch (dragEnum) {
      case DRAG_LEFT:
      case DRAG_RIGHT:
        viewer->setCursor(Qt::SizeHorCursor);
        break;
      case DRAG_TOP:
      case DRAG_BOTTOM:
        viewer->setCursor(Qt::SizeVerCursor);
        break;
      case DRAG_LEFT | DRAG_TOP:
      case DRAG_RIGHT | DRAG_BOTTOM:
        viewer->setCursor(Qt::SizeFDiagCursor);
        break;
      case DRAG_LEFT | DRAG_BOTTOM:
      case DRAG_RIGHT | DRAG_TOP:
        viewer->setCursor(Qt::SizeBDiagCursor);
        break;
      default:
        viewer->setCursor(Qt::ArrowCursor);
        break;
      }
  }

  // In case, perform the pan
  return event.buttons() == Qt::MidButton;
}

//----------------------------------------------------------------------

bool PreviewSubCameraManager::mouseReleaseEvent(SceneViewer *viewer) {
  if (viewer->is3DView()) return true;

  m_mousePressed = false;
  m_dragType     = NODRAG;
  m_clickAndDrag = false;

  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  camera->setInterestRect(m_editingInterestRect);

  // Request a previewer update. Observe that whereas this previewer may not be
  // in preview mode,
  // another visible one may.
  Previewer::instance(true)->updateView();

  // Refresh viewer
  viewer->update();

  return false;
}

//-----------------------------------------------------------------------------

//! Builds the drag enum and camera distance for subcamera refinement drags.
UCHAR PreviewSubCameraManager::getSubCameraDragEnum(SceneViewer *viewer,
                                                    const QPointF &mousePos) {
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TRect subCamera = camera->getInterestRect();

  if (subCamera.getLx() <= 0 || subCamera.getLy() <= 0) return NODRAG;

  TPointD cameraPosL(winToCamera(viewer, mousePos - QPointF(10, 0)));
  TPointD cameraPosR(winToCamera(viewer, mousePos + QPointF(10, 0)));
  TPointD cameraPosT(winToCamera(viewer, mousePos - QPointF(0, 10)));
  TPointD cameraPosB(winToCamera(viewer, mousePos + QPointF(0, 10)));

  TRectD cameraPosBox(
      std::min({cameraPosL.x, cameraPosR.x, cameraPosT.x, cameraPosB.x}),
      std::min({cameraPosL.y, cameraPosR.y, cameraPosT.y, cameraPosB.y}),
      std::max({cameraPosL.x, cameraPosR.x, cameraPosT.x, cameraPosB.x}),
      std::max({cameraPosL.y, cameraPosR.y, cameraPosT.y, cameraPosB.y}));

  TRectD subCameraD(subCamera.x0, subCamera.y0, subCamera.x1 + 1,
                    subCamera.y1 + 1);

  // Now, find out the drag enums, in case the mouse pos is near a sensible part
  // of the rect
  UCHAR dragType = NODRAG;

  if (cameraPosBox.y0 < subCameraD.y1) dragType |= INNER_TOP;
  if (cameraPosBox.y1 > subCameraD.y0) dragType |= INNER_BOTTOM;
  if (cameraPosBox.x0 < subCameraD.x1) dragType |= INNER_RIGHT;
  if (cameraPosBox.x1 > subCameraD.x0) dragType |= INNER_LEFT;

  if (cameraPosBox.y1 > subCameraD.y1) dragType |= OUTER_TOP;
  if (cameraPosBox.y0 < subCameraD.y0) dragType |= OUTER_BOTTOM;
  if (cameraPosBox.x1 > subCameraD.x1) dragType |= OUTER_RIGHT;
  if (cameraPosBox.x0 < subCameraD.x0) dragType |= OUTER_LEFT;

  return dragType;
}

//-----------------------------------------------------------------------------

TPoint PreviewSubCameraManager::getSubCameraDragDistance(
    SceneViewer *viewer, const QPointF &mousePos) {
  // Build the camera drag distance
  if (m_clickAndDrag) return TPoint();

  TPointD cameraMousePos(winToCamera(viewer, mousePos));

  if (bitwiseExclude(m_dragType, OUTER)) {
    TPointD resultD(cameraMousePos - m_cameraMousePressPos);
    return TPoint(resultD.x, resultD.y);
  }

  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TRect subCamera = camera->getInterestRect();
  TRectD subCameraD(subCamera.x0, subCamera.y0, subCamera.x1 + 1,
                    subCamera.y1 + 1);

  TPoint result;
  if (bitwiseContains(m_dragType, DRAG_LEFT))
    result.x = cameraMousePos.x - subCameraD.x0;
  else if (bitwiseContains(m_dragType, DRAG_RIGHT))
    result.x = cameraMousePos.x - subCameraD.x1;
  if (bitwiseContains(m_dragType, DRAG_BOTTOM))
    result.y = cameraMousePos.y - subCameraD.y0;
  else if (bitwiseContains(m_dragType, DRAG_TOP))
    result.y = cameraMousePos.y - subCameraD.y1;

  return result;
}

//-----------------------------------------------------------------------------
/*! Delete sub camera frame. Executed from context menu of the viewer.
*/
void PreviewSubCameraManager::deleteSubCamera(SceneViewer *viewer) {
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  camera->setInterestRect(TRect());

  Previewer::instance(true)->updateView();

  // Refresh viewer
  viewer->update();
}
