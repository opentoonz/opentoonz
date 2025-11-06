
#include "viewtools.h"

#include "tools/cursors.h"
#include "tgeometry.h"
#include "tenv.h"

#include <math.h>

#include "tgl.h"

TEnv::IntVar RotateOnCameraCenter("RotateOnCameraCenter", 0);
TEnv::IntVar RotateByStep("RotateByStep", 1);

TEnv::DoubleVar RotateStepAngle("RotateStepAngle", 5);
TEnv::DoubleVar RotateAngle("RotateAngle", 90);  // Rotate commands' angle

namespace {

//=============================================================================
// Zoom Tool
//-----------------------------------------------------------------------------

class ZoomTool final : public TTool {
  int m_oldX;
  TPointD m_center;
  bool m_dragging;
  double m_factor;

public:
  ZoomTool(std::string name)
      : TTool(name), m_dragging(false), m_oldX(0), m_factor(1) {
    bind(TTool::AllTargets);
  }

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateMatrix() override { return setMatrix(TAffine()); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    m_dragging = true;
    int v      = 1;
    if (e.isAltPressed()) v = -1;
    m_oldX = e.m_pos.x;
    // m_center = getViewer()->winToWorld(e.m_pos);
    m_center = TPointD(e.m_pos.x, e.m_pos.y);
    m_factor = 1;
    invalidate();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    int d    = m_oldX - e.m_pos.x;
    m_oldX   = e.m_pos.x;
    double f = exp(-d * 0.01);
    m_factor = f;
    m_viewer->zoom(m_center, f);
  }
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    m_dragging = false;
    invalidate();
  }

  void rightButtonDown(const TPointD &, const TMouseEvent &e) override {
    if (!m_viewer) return;
    invalidate();
  }

  void draw() override {
    if (!m_dragging) return;

    TPointD center   = m_viewer->winToWorld(m_center);
    double pixelSize = getPixelSize();
    double unit      = pixelSize;
    glPushMatrix();
    glTranslated(center.x, center.y, 0);
    glScaled(unit, unit, unit);
    glColor3f(1, 0, 0);

    double u = 4;
    glBegin(GL_LINES);
    glVertex2d(0, -10);
    glVertex2d(0, 10);
    glVertex2d(-10, 0);
    glVertex2d(10, 0);
    glEnd();

    glPopMatrix();
  }

  int getCursorId() const override { return ToolCursor::ZoomCursor; }

} zoomTool("T_Zoom"), zoomViewTool("T_ZoomView");

//=============================================================================
// Hand Tool
//-----------------------------------------------------------------------------

class HandTool final : public TTool {
  TStopWatch m_sw;
  TPointD m_oldPos;

public:
  HandTool(std::string name) : TTool(name) { bind(TTool::AllTargets); }

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateMatrix() override { return setMatrix(TAffine()); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    m_oldPos = e.m_pos;
    m_sw.start(true);
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    if (m_sw.getTotalTime() < 10) return;
    m_sw.stop();
    m_sw.start(true);
    TPointD delta = e.m_pos - m_oldPos;
    delta.y       = -delta.y;
    m_viewer->pan(delta);
    m_oldPos = e.m_pos;
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    m_sw.stop();
  }

  int getCursorId() const override { return ToolCursor::PanCursor; }

} handTool("T_Hand"), handViewTool("T_HandView");

}  // namespace

//=============================================================================
// Rotate Tool
//-----------------------------------------------------------------------------

RotateTool::RotateTool(std::string name)
    : TTool(name)
    , m_dragging(false)
    , m_cameraCentered("Rotate On Camera Center", false)
    , m_rotateByStep("Rotate by Step", true)
    , m_rotateStepAngle("Step Angle:", 0, 360, 0, true)
    , m_rotateCommandAngle("Rotate Left/Right Angle:", 0, 360, 90, true)
    , m_angle(0) {
  bind(TTool::AllTargets);
  m_cameraCentered.setId("RotateOnCamCenter");
  m_rotateStepAngle.setId("StepAngle");
  m_rotateByStep.setId("RotateByStep");
  m_rotateCommandAngle.setId(
      "RotateCommandAngle");  // For rotate left/right commands

  m_prop.bind(m_cameraCentered);
  m_prop.bind(m_rotateStepAngle);
  m_prop.bind(m_rotateByStep);
  m_prop.bind(m_rotateCommandAngle);
}

void RotateTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  if (!m_viewer) return;

  m_isCtrlPressed = e.isCtrlPressed();
  m_isAltPressed  = e.isAltPressed();
  m_angle         = 0.0;
  m_dragging      = true;
  m_oldPos        = pos;
  m_oldMousePos   = e.m_pos;
  // m_center = TPointD(0,0);
  m_sw.start(true);
  invalidate();

  // m_center =
  // viewAffine.inv()*TPointD(0,0);//m_viewer->winToWorld(m_viewer);
  // virtual TPointD winToWorld(const TPoint &winPos) const = 0;
}

void RotateTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_viewer) return;
  if (m_sw.getTotalTime() < 50) return;
  m_sw.stop();
  m_sw.start(true);

  TPointD p = pos;
  if (m_viewer->is3DView()) {
    TPointD d     = e.m_pos - m_oldMousePos;
    m_oldMousePos = e.m_pos;
    double factor = 0.5;
    m_viewer->rotate3D(factor * d.x, -factor * d.y);
  } else {
    TPointD a = p - m_center;
    TPointD b = m_oldPos - m_center;
    if (norm2(a) > 0 && norm2(b) > 0) {
      double ang        = asin(cross(b, a) / (norm(a) * norm(b))) * M_180_PI;
      m_angle           = m_angle + ang;
      double finalAngle = m_angle;
      double step       = m_rotateStepAngle.getValue();

      if ((!m_isAltPressed && m_rotateByStep.getValue()) && step > 0) {
        finalAngle = std::round(m_angle / step) * step;

        if (e.m_pos != m_oldMousePos) {
          m_viewer->rotate(m_center, finalAngle);
        }
      } else
        m_viewer->rotate(m_center, finalAngle);
    }
  }
  m_oldPos = p;
}

void RotateTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  m_dragging = false;
  invalidate();
  m_sw.stop();
}

void RotateTool::draw() {
  glColor3f(1, 0, 0);
  double u = 50;
  if (m_cameraCentered.getValue())
    m_center = TPointD(0, 0);
  else {
    TAffine aff = m_viewer->getViewMatrix().inv();
    if (m_viewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
    if (m_viewer->getIsFlippedY()) aff = aff * TScale(1, -1);
    u        = u * sqrt(aff.det());
    m_center = aff * TPointD(0, 0);
  }
  tglDrawSegment(TPointD(-u + m_center.x, m_center.y),
                 TPointD(u + m_center.x, m_center.y));
  tglDrawSegment(TPointD(m_center.x, -u + m_center.y),
                 TPointD(m_center.x, u + m_center.y));
}

int RotateTool::getCursorId() const { return ToolCursor::RotateCursor; }

void RotateTool::onActivate() {
  if (m_firstTime) {
    m_cameraCentered.setValue(RotateOnCameraCenter != 0);
    m_rotateStepAngle.setValue((double)RotateStepAngle);
    m_rotateCommandAngle.setValue((double)RotateAngle);
    m_rotateByStep.setValue(RotateByStep != 0);
    m_firstTime = false;
  }
}

bool RotateTool::onPropertyChanged(std::string propertyName, bool addToUndo) {
  if (propertyName == "Rotate On Camera Center")
    RotateOnCameraCenter = m_cameraCentered.getValue() ? 1 : 0;
  else if (propertyName == "Rotate Angle:")
    RotateAngle = m_rotateStepAngle.getValue();
  else if (propertyName == "Rotate by Step")
    RotateByStep = m_rotateByStep.getValue() ? 1 : 0;
  else if (propertyName == "Rotate Left/Right Angle:")
    RotateAngle = m_rotateCommandAngle.getValue();
  return true;
}

RotateTool rotateTool("T_Rotate"), rotateViewTool("T_RotateView");