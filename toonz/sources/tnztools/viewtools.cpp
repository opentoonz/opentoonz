

#include "tools/tool.h"
#include "tstopwatch.h"
#include "tools/cursors.h"
#include "tgeometry.h"
#include "tproperty.h"

#include <math.h>

#include "tgl.h"

namespace {

//=============================================================================
// Zoom Tool
//-----------------------------------------------------------------------------

class ZoomTool final : public TTool {
  int m_oldY;
  TPointD m_center;
  bool m_dragging;
  double m_factor;

public:
  ZoomTool() : TTool("T_Zoom"), m_dragging(false), m_oldY(0), m_factor(1) {
    bind(TTool::AllTargets);
  }

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateMatrix() override { return setMatrix(TAffine()); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    m_dragging              = true;
    int v                   = 1;
    if (e.isAltPressed()) v = -1;
    m_oldY                  = e.m_pos.y;
    // m_center = getViewer()->winToWorld(e.m_pos);
    m_center = TPointD(e.m_pos.x, e.m_pos.y);
    m_factor = 1;
    invalidate();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    int d    = m_oldY - e.m_pos.y;
    m_oldY   = e.m_pos.y;
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

    TPointD center   = m_viewer->winToWorld(TPoint(m_center.x, m_center.y));
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

} zoomTool;

//=============================================================================
// Hand Tool
//-----------------------------------------------------------------------------

class HandTool final : public TTool {
  TStopWatch m_sw;
  TPoint m_oldPos;

public:
  HandTool() : TTool("T_Hand") { bind(TTool::AllTargets); }

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
    TPoint delta = e.m_pos - m_oldPos;
    delta.y      = -delta.y;
    m_viewer->pan(delta);
    m_oldPos = e.m_pos;
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    m_sw.stop();
  }

  int getCursorId() const override { return ToolCursor::PanCursor; }

} handTool;

//=============================================================================
// Rotate Tool
//-----------------------------------------------------------------------------

class RotateTool final : public TTool {
  TStopWatch m_sw;
  TPointD m_oldPos;
  TPointD m_center;
  bool m_dragging;
  double m_angle;
  TPoint m_oldMousePos;
  TBoolProperty m_cameraCentered;
  TPropertyGroup m_prop;

public:
  RotateTool()
      : TTool("T_Rotate")
      , m_dragging(false)
      , m_cameraCentered("Rotate On Camera Center", false)
      , m_angle(0) {
    bind(TTool::AllTargets);
    m_prop.bind(m_cameraCentered);
  }

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateMatrix() override { return setMatrix(TAffine()); }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;

    m_angle       = 0.0;
    m_dragging    = true;
    m_oldPos      = pos;
    m_oldMousePos = e.m_pos;
    // m_center = TPointD(0,0);
    m_sw.start(true);
    invalidate();

    // m_center =
    // viewAffine.inv()*TPointD(0,0);//m_viewer->winToWorld(m_viewer);
    // virtual TPointD winToWorld(const TPoint &winPos) const = 0;
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_viewer) return;
    if (m_sw.getTotalTime() < 50) return;
    m_sw.stop();
    m_sw.start(true);
    TPointD p = pos;
    if (m_viewer->is3DView()) {
      TPoint d      = e.m_pos - m_oldMousePos;
      m_oldMousePos = e.m_pos;
      double factor = 0.5;
      m_viewer->rotate3D(factor * d.x, -factor * d.y);
    } else {
      TPointD a = p - m_center;
      TPointD b = m_oldPos - m_center;
      if (norm2(a) > 0 && norm2(b) > 0) {
        double ang = asin(cross(b, a) / (norm(a) * norm(b))) * M_180_PI;
        m_angle    = m_angle + ang;
        m_viewer->rotate(m_center, m_angle);
      }
    }
    m_oldPos = p;
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    m_dragging = false;
    invalidate();
    m_sw.stop();
  }

  void draw() override {
    glColor3f(1, 0, 0);
    double u = 50;
    if (m_cameraCentered.getValue())
      m_center = TPointD(0, 0);
    else {
      TAffine aff                        = m_viewer->getViewMatrix().inv();
      if (m_viewer->getIsFlippedX()) aff = aff * TScale(-1, 1);
      if (m_viewer->getIsFlippedY()) aff = aff * TScale(1, -1);
      u                                  = u * sqrt(aff.det());
      m_center                           = aff * TPointD(0, 0);
    }
    tglDrawSegment(TPointD(-u + m_center.x, m_center.y),
                   TPointD(u + m_center.x, m_center.y));
    tglDrawSegment(TPointD(m_center.x, -u + m_center.y),
                   TPointD(m_center.x, u + m_center.y));
  }

  int getCursorId() const override { return ToolCursor::RotateCursor; }

} rotateTool;

}  // namespace
