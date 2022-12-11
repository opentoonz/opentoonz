

#include "edittoolgadgets.h"
#include "tgl.h"
#include "toonz/tfxhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/stage.h"
#include "tools/tool.h"
#include "tfx.h"
#include "tparamcontainer.h"
#include "toonz/tcolumnfx.h"
#include "tdoubleparam.h"
#include "tparamset.h"
#include "tundo.h"
#include "tparamuiconcept.h"

#include "historytypes.h"
#include "toonzqt/gutil.h"

#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QVector2D>

using namespace EditToolGadgets;

GLdouble FxGadget::m_selectedColor[3] = {0.2, 0.8, 0.1};

namespace {
TPointD hadamard(const TPointD &v1, const TPointD &v2) {
  return TPointD(v1.x * v2.x, v1.y * v2.y);
}

#define SPIN_NUMVERTS 72

void drawSpinField(const TRectD geom, const TPointD center,
                   const double lineInterval, const double e_aspect_ratio,
                   const double e_angle) {
  static GLdouble vertices[SPIN_NUMVERTS * 2];
  static bool isInitialized = false;
  if (!isInitialized) {
    isInitialized = true;
    for (int r = 0; r < SPIN_NUMVERTS; r++) {
      double theta        = 2.0 * M_PI * (double)r / (double)SPIN_NUMVERTS;
      vertices[r * 2]     = std::cos(theta);
      vertices[r * 2 + 1] = std::sin(theta);
    }
  }
  // obtain the nearest and the furthest pos inside the geom
  TPointD nearestPos;
  nearestPos.x   = (center.x <= geom.x0)   ? geom.x0
                   : (center.x >= geom.x1) ? geom.x1
                                           : center.x;
  nearestPos.y   = (center.y <= geom.y0)   ? geom.y0
                   : (center.y >= geom.y1) ? geom.y1
                                           : center.y;
  double minDist = norm(nearestPos - center);
  TPointD farthestPos;
  farthestPos.x   = (center.x <= geom.x0)                            ? geom.x1
                    : (center.x >= geom.x1)                          ? geom.x0
                    : ((center.x - geom.x0) >= (geom.x1 - center.x)) ? geom.x0
                                                                     : geom.x1;
  farthestPos.y   = (center.y <= geom.y0)                            ? geom.y1
                    : (center.y >= geom.y1)                          ? geom.y0
                    : ((center.y - geom.y0) >= (geom.y1 - center.y)) ? geom.y0
                                                                     : geom.y1;
  double maxDist  = norm(farthestPos - center);
  double scale[2] = {1.0, 1.0};
  // adjust size for ellipse
  if (e_aspect_ratio != 1.0) {
    scale[0] = 2.0 * e_aspect_ratio / (e_aspect_ratio + 1);
    scale[1] = scale[0] / e_aspect_ratio;
    minDist *= std::min(scale[0], scale[1]);
    maxDist *= std::max(scale[0], scale[1]);
  }
  // obtain id range
  int minId = (int)std::ceil(minDist / lineInterval);
  int maxId = (int)std::floor(maxDist / lineInterval);

  glColor3dv(FxGadget::m_selectedColor);

  glEnableClientState(GL_VERTEX_ARRAY);
  glLineStipple(1, 0x00FF);
  glEnable(GL_LINE_STIPPLE);

  glVertexPointer(2, GL_DOUBLE, 0, vertices);

  glPushMatrix();

  glTranslated(center.x, center.y, 0.0);
  glRotated(e_angle, 0., 0., 1.);
  glScaled(scale[0] * lineInterval, scale[1] * lineInterval, 1.);

  for (int id = minId; id <= maxId; id++) {
    if (id == 0) continue;
    if (id % 2 == 0)
      glColor3dv(FxGadget::m_selectedColor);
    else
      glColor3d(0, 0, 1);

    glPushMatrix();
    glScaled((double)id, (double)id, 1.);
    // draw using vertex array
    glDrawArrays(GL_LINE_LOOP, 0, SPIN_NUMVERTS);
    glPopMatrix();
  }

  glDisable(GL_LINE_STIPPLE);
  glDisableClientState(GL_VERTEX_ARRAY);
  glPopMatrix();
}

#define RADIAL_FIELD_NUMSEGMENTS 5
#define RADIAL_COMPASS_NUMSEGMENTS 20

void drawRadialField(const TRectD geom, const TPointD center,
                     const double lineInterval, const double e_aspect_ratio,
                     const double e_angle, const double twist,
                     const double pivot) {
  // obtain the nearest and the furthest pos inside the geom
  TPointD nearestPos;
  nearestPos.x   = (center.x <= geom.x0)   ? geom.x0
                   : (center.x >= geom.x1) ? geom.x1
                                           : center.x;
  nearestPos.y   = (center.y <= geom.y0)   ? geom.y0
                   : (center.y >= geom.y1) ? geom.y1
                                           : center.y;
  double minDist = norm(nearestPos - center);
  TPointD farthestPos;
  farthestPos.x   = (center.x <= geom.x0)                            ? geom.x1
                    : (center.x >= geom.x1)                          ? geom.x0
                    : ((center.x - geom.x0) >= (geom.x1 - center.x)) ? geom.x0
                                                                     : geom.x1;
  farthestPos.y   = (center.y <= geom.y0)                            ? geom.y1
                    : (center.y >= geom.y1)                          ? geom.y0
                    : ((center.y - geom.y0) >= (geom.y1 - center.y)) ? geom.y0
                                                                     : geom.y1;
  double maxDist  = norm(farthestPos - center);
  double scale[2] = {1.0, 1.0};
  // adjust size for ellipse
  if (e_aspect_ratio != 1.0) {
    scale[0] = 2.0 * e_aspect_ratio / (e_aspect_ratio + 1);
    scale[1] = scale[0] / e_aspect_ratio;
    minDist *= std::min(scale[0], scale[1]);
    maxDist *= std::max(scale[0], scale[1]);
  }
  // obtain id range
  int minId =
      (minDist == 0.)
          ? 0
          : (int)std::floor(std::log2(M_PI * minDist / lineInterval)) + 1;
  int maxId = (int)std::ceil(std::log2(M_PI * maxDist / lineInterval)) + 1;

  struct LineInfo {
    double anglePos;  // original direction of the line
    int birthId;      // generation where the line started to be drawn
  };
  // register lines information
  QList<LineInfo> infoList;
  // initial lines at minId
  int initLineAmount = std::pow(2, minId);
  for (int li = 0; li < initLineAmount; li++)
    infoList.append({360.0 * (double)li / (double)initLineAmount, minId});
  for (int id = minId + 1; id <= maxId; id++) {
    // insert between the existing lines
    QList<LineInfo>::iterator itr = infoList.end();
    while (itr != infoList.begin()) {
      double ap;
      if (itr == infoList.end())
        ap = (360.0 + (*(itr - 1)).anglePos) * 0.5;
      else
        ap = ((*itr).anglePos + (*(itr - 1)).anglePos) * 0.5;
      itr = infoList.insert(itr, {ap, id});
      itr--;
    }
  }

  int unitDist =
      std::pow(2, (maxId - 1)) - ((minId == 0) ? 0 : std::pow(2, (minId - 1)));
  GLdouble *vertices =
      new GLdouble[(unitDist * RADIAL_FIELD_NUMSEGMENTS + 1) * 2];

  double radiStep = (lineInterval / M_PI) / (double)RADIAL_FIELD_NUMSEGMENTS;
  double tmpRad =
      (minId == 0) ? 0. : (std::pow(2, (minId - 1)) * lineInterval / M_PI);
  for (int v = 0; v < unitDist * RADIAL_FIELD_NUMSEGMENTS + 1; v++) {
    double tw           = twist * tmpRad / pivot;
    vertices[v * 2]     = tmpRad * std::cos(tw);
    vertices[v * 2 + 1] = tmpRad * std::sin(tw);
    tmpRad += radiStep;
  }

  glColor3d(0, 0, 1);
  glEnableClientState(GL_VERTEX_ARRAY);
  glLineStipple(1, 0x00FF);
  glEnable(GL_LINE_STIPPLE);

  glVertexPointer(2, GL_DOUBLE, 0, vertices);

  glPushMatrix();

  glTranslated(center.x, center.y, 0.0);
  glRotated(e_angle, 0., 0., 1.);
  glScaled(scale[0], scale[1], 1.);
  int vertexIdOffset =
      (minId == 0) ? 0 : std::pow(2, (minId - 1)) * RADIAL_FIELD_NUMSEGMENTS;
  for (auto line : infoList) {
    glPushMatrix();
    glRotated(line.anglePos, 0., 0., 1.);
    int startId = (line.birthId == 0) ? 0
                                      : std::pow(2, (line.birthId - 1)) *
                                                RADIAL_FIELD_NUMSEGMENTS -
                                            vertexIdOffset;
    // draw using vertex array
    glDrawArrays(GL_LINE_STRIP, startId,
                 unitDist * RADIAL_FIELD_NUMSEGMENTS - startId);
    glPopMatrix();
  }

  glDisable(GL_LINE_STIPPLE);
  glDisableClientState(GL_VERTEX_ARRAY);
  glPopMatrix();

  delete[] vertices;
}

}  // namespace

//*************************************************************************************
//    FxGadgetUndo  definition
//*************************************************************************************

class FxGadgetUndo final : public TUndo {
  struct ParamData {
    TDoubleParamP m_param;
    double m_oldValue, m_newValue;
    bool m_wasKeyframe;
  };

private:
  std::vector<ParamData> m_params;
  int m_frame;

public:
  FxGadgetUndo(const std::vector<TDoubleParamP> &params, int frame)
      : m_frame(frame) {
    m_params.resize(params.size());
    for (int i = 0; i < (int)params.size(); i++) {
      m_params[i].m_param       = params[i];
      m_params[i].m_oldValue    = params[i]->getValue(frame);
      m_params[i].m_newValue    = m_params[i].m_oldValue;
      m_params[i].m_wasKeyframe = m_params[i].m_param->isKeyframe(frame);
    }
  }

  void onAdd() override {
    for (int i = 0; i < (int)m_params.size(); i++) {
      m_params[i].m_newValue = m_params[i].m_param->getValue(m_frame);
    }
  }

  void undo() const override {
    for (int i = 0; i < (int)m_params.size(); i++) {
      if (!m_params[i].m_wasKeyframe)
        m_params[i].m_param->deleteKeyframe(m_frame);
      else
        m_params[i].m_param->setValue(m_frame, m_params[i].m_oldValue);
    }
  }

  void redo() const override {
    for (int i = 0; i < (int)m_params.size(); i++) {
      m_params[i].m_param->setValue(m_frame, m_params[i].m_newValue);
    }
  }

  int getSize() const override {
    return sizeof(*this) + m_params.size() * sizeof(ParamData);
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Modify Fx Gadget  ");
    for (int i = 0; i < (int)m_params.size(); i++) {
      str += QString::fromStdString(m_params[i].m_param->getName());
      if (i != (int)m_params.size() - 1) str += QString::fromStdString(", ");
    }
    str += QString("  Frame : %1").arg(QString::number(m_frame + 1));

    return str;
  }

  int getHistoryType() override { return HistoryType::Fx; }
};

//*************************************************************************************
//    GadgetDragTool  definition
//*************************************************************************************

class GadgetDragTool final : public DragTool {
  FxGadgetController *m_controller;
  FxGadget *m_gadget;
  TPointD m_firstPos;

public:
  GadgetDragTool(FxGadgetController *controller, FxGadget *gadget)
      : m_controller(controller), m_gadget(gadget) {}

  TAffine getMatrix() const { return m_controller->getMatrix().inv(); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_gadget->createUndo();
    m_gadget->leftButtonDown(getMatrix() * pos, e);
    m_firstPos = pos;
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    // precise control with pressing Alt key
    if (e.isAltPressed()) {
      TPointD precisePos = m_firstPos + (pos - m_firstPos) * 0.1;
      m_gadget->leftButtonDrag(getMatrix() * precisePos, e);
    } else
      m_gadget->leftButtonDrag(getMatrix() * pos, e);
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    leftButtonUp();
  }

  void leftButtonUp() override {
    m_gadget->leftButtonUp();
    m_gadget->commitUndo();
  }
};

//*************************************************************************************
//    FxGadget  implementation
//*************************************************************************************

FxGadget::FxGadget(FxGadgetController *controller, int handleCount)
    : m_id(-1)
    , m_selected(-1)
    , m_controller(controller)
    , m_pixelSize(1)
    , m_undo(0)
    , m_scaleFactor(1)
    , m_handleCount(handleCount) {
  controller->assignId(this);
}

//---------------------------------------------------------------------------

FxGadget::~FxGadget() {
  for (int i = 0; i < (int)m_params.size(); i++)
    m_params[i]->removeObserver(this);
}

//---------------------------------------------------------------------------

void FxGadget::addParam(const TDoubleParamP &param) {
  m_params.push_back(param);
  param->addObserver(this);
}

//---------------------------------------------------------------------------

double FxGadget::getValue(const TDoubleParamP &param) const {
  return param->getValue(m_controller->getCurrentFrame());
}

//---------------------------------------------------------------------------

void FxGadget::setValue(const TDoubleParamP &param, double value) {
  param->setValue(m_controller->getCurrentFrame(), value);
}

//---------------------------------------------------------------------------

TPointD FxGadget::getValue(const TPointParamP &param) const {
  return param->getValue(m_controller->getCurrentFrame());
}

//---------------------------------------------------------------------------

void FxGadget::setValue(const TPointParamP &param, const TPointD &pos) {
  param->setValue(m_controller->getCurrentFrame(), pos);
}

//---------------------------------------------------------------------------

void FxGadget::setPixelSize() {
  setPixelSize(sqrt(tglGetPixelSize2()) * m_controller->getDevPixRatio());
}

//---------------------------------------------------------------------------

void FxGadget::drawTooltip(const TPointD &tooltipPos,
                           std::string tooltipPosText) {
  double unit = sqrt(tglGetPixelSize2()) * m_controller->getDevPixRatio();
  glPushMatrix();
  glTranslated(tooltipPos.x, tooltipPos.y, 0.0);
  double sc = unit * 1.6;
  glScaled(sc, sc, 1);
  tglDrawText(TPointD(8, -3), tooltipPosText);
  glPopMatrix();
}

//---------------------------------------------------------------------------

void FxGadget::drawDot(const TPointD &pos) {
  double r = getPixelSize() * 3;
  tglDrawRect(pos.x - r, pos.y - r, pos.x + r, pos.y + r);
}

//---------------------------------------------------------------------------

void FxGadget::onChange(const TParamChange &) {
  m_controller->invalidateViewer();
}

//---------------------------------------------------------------------------

void FxGadget::createUndo() {
  assert(m_undo == 0);
  m_undo = new FxGadgetUndo(m_params, m_controller->getCurrentFrame());
}

//---------------------------------------------------------------------------

void FxGadget::commitUndo() {
  assert(m_undo);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
}

//*************************************************************************************
//    Specific Gadget Concepts  definition
//*************************************************************************************

class PointFxGadget final : public FxGadget {
  TPointD m_pos;
  TDoubleParamP m_xParam, m_yParam;

public:
  PointFxGadget(FxGadgetController *controller, const TPointParamP &param)
      : FxGadget(controller), m_xParam(param->getX()), m_yParam(param->getY()) {
    addParam(m_xParam);
    addParam(m_yParam);
  }

  PointFxGadget(FxGadgetController *controller, const TDoubleParamP &xParam,
                const TDoubleParamP &yParam)
      : FxGadget(controller), m_xParam(xParam), m_yParam(yParam) {
    addParam(m_xParam);
    addParam(m_yParam);
  }

  void draw(bool picking) override;

  TPointD getPoint() const {
    return TPointD(getValue(m_xParam), getValue(m_yParam));
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void PointFxGadget::draw(bool picking) {
  setPixelSize();
  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  TPointD pos(getPoint());
  double unit = getPixelSize();
  glPushMatrix();
  glTranslated(pos.x, pos.y, 0);
  double r = unit * 3;
  double d = unit * 6;
  glBegin(GL_LINES);
  glVertex2d(-d, 0);
  glVertex2d(-r, 0);
  glVertex2d(d, 0);
  glVertex2d(r, 0);
  glVertex2d(0, -d);
  glVertex2d(0, -r);
  glVertex2d(0, d);
  glVertex2d(0, r);
  glEnd();
  tglDrawRect(-r, -r, r, r);
  glPopMatrix();
  glPopName();

  if (isSelected()) {
    drawTooltip(pos + TPointD(7, 3) * unit, getLabel());
  }
}

//---------------------------------------------------------------------------

void PointFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {}

//---------------------------------------------------------------------------

void PointFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  if (m_xParam) setValue(m_xParam, pos.x);
  if (m_yParam) setValue(m_yParam, pos.y);
}

//=============================================================================

class RadiusFxGadget final : public FxGadget {
  TDoubleParamP m_radius;
  TPointParamP m_center;

public:
  RadiusFxGadget(FxGadgetController *controller, const TDoubleParamP &radius,
                 const TPointParamP &center)
      : FxGadget(controller), m_radius(radius), m_center(center) {
    addParam(radius);
  }

  TPointD getCenter() const;

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

TPointD RadiusFxGadget::getCenter() const {
  return m_center ? getValue(m_center) : TPointD();
}

//---------------------------------------------------------------------------

void RadiusFxGadget::draw(bool picking) {
  if (!m_radius) return;

  setPixelSize();
  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  double radius  = getValue(m_radius);
  TPointD center = getCenter();

  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  tglDrawCircle(center, radius);
  glDisable(GL_LINE_STIPPLE);
  drawDot(center + TPointD(0.707, 0.707) * radius);
  glPopName();

  if (isSelected()) {
    drawTooltip(center + TPointD(0.707, 0.707) * radius, getLabel());
  }
}

//---------------------------------------------------------------------------

void RadiusFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {}

//---------------------------------------------------------------------------

void RadiusFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  setValue(m_radius, norm(pos - getCenter()));
}

//=============================================================================

class DistanceFxGadget final : public FxGadget {
  TDoubleParamP m_distance, m_angle;
  int m_grabPos;

public:
  DistanceFxGadget(FxGadgetController *controller,
                   const TDoubleParamP &distance, const TDoubleParamP &angle)
      : FxGadget(controller)
      , m_distance(distance)
      , m_angle(angle)
      , m_grabPos(1) {
    addParam(distance);
    if (angle) addParam(angle);
  }

  TPointD getDirection() const {
    if (!m_angle) return TPointD(1.0, 0.0);

    double angle = getValue(m_angle);
    return TPointD(cos(angle), sin(angle));
  }

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void DistanceFxGadget::draw(bool picking) {
  if (!m_distance) return;
  setPixelSize();
  glColor3d(0, 0, 1);
  double d = getValue(m_distance) * getScaleFactor();
  TPointD dir(getDirection());
  TPointD u = rotate90(dir) * (getPixelSize() * 10);

  tglDrawSegment(-u, u);

  glPushName(getId());

  TPointD b, c;
  b = dir * (d * 0.5);
  c = b - dir * d;

  tglDrawSegment(b - u, b + u);
  tglDrawCircle(b, getPixelSize() * 5);

  tglDrawSegment(c - u, c + u);
  tglDrawCircle(c, getPixelSize() * 5);

  glPopName();

  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  tglDrawSegment(b, c);
  glDisable(GL_LINE_STIPPLE);
  if (isSelected()) {
    drawTooltip(b + TPointD(5, 5) * getPixelSize(), getLabel());
  }
}

//---------------------------------------------------------------------------

void DistanceFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_grabPos = (pos.x > 0.0) ? 1 : -1;
}

//---------------------------------------------------------------------------

void DistanceFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  double v = (pos * getDirection()) / getScaleFactor();
  v        = v * 2 * m_grabPos;
  setValue(m_distance, v);
}

//=============================================================================

class AngleFxGadget final : public FxGadget {
  TDoubleParamP m_param;
  TPointD m_pos;

public:
  AngleFxGadget(FxGadgetController *controller, const TDoubleParamP &param,
                const TPointD &pos);

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

AngleFxGadget::AngleFxGadget(FxGadgetController *controller,
                             const TDoubleParamP &param, const TPointD &pos)
    : FxGadget(controller), m_param(param), m_pos(pos) {
  addParam(param);
}

//---------------------------------------------------------------------------

void AngleFxGadget::draw(bool picking) {
  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  double pixelSize = sqrt(tglGetPixelSize2()) * m_controller->getDevPixRatio();
  double r         = pixelSize * 40;
  double a = pixelSize * 10, b = pixelSize * 5;
  tglDrawCircle(m_pos, r);
  double phi = getValue(m_param);
  glPushMatrix();
  glTranslated(m_pos.x, m_pos.y, 0);
  glRotated(phi, 0, 0, 1);
  glBegin(GL_LINES);
  glVertex2d(0, 0);
  glVertex2d(r, 0);
  glVertex2d(r, 0);
  glVertex2d(r - a, b);
  glVertex2d(r, 0);
  glVertex2d(r - a, -b);
  glEnd();
  glPopMatrix();
  glPopName();

  if (isSelected()) {
    drawTooltip(m_pos + TPointD(0.707, 0.707) * r, getLabel());
  }
}

//---------------------------------------------------------------------------

void AngleFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {}

//---------------------------------------------------------------------------

void AngleFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  TPointD d  = pos - m_pos;
  double phi = atan2(d.y, d.x);
  setValue(m_param, phi * M_180_PI);
}

//=============================================================================

class AngleRangeFxGadget final : public FxGadget {
  TDoubleParamP m_startAngle, m_endAngle;
  TPointParamP m_center;

  enum HANDLE { StartAngle = 0, EndAngle, None } m_handle = None;

  double m_clickedAngle;
  double m_targetAngle, m_anotherAngle;

public:
  AngleRangeFxGadget(FxGadgetController *controller,
                     const TDoubleParamP &startAngle,
                     const TDoubleParamP &endAngle, const TPointParamP &center);

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp() override;
};

//---------------------------------------------------------------------------

AngleRangeFxGadget::AngleRangeFxGadget(FxGadgetController *controller,
                                       const TDoubleParamP &startAngle,
                                       const TDoubleParamP &endAngle,
                                       const TPointParamP &center)
    : FxGadget(controller, 2)
    , m_startAngle(startAngle)
    , m_endAngle(endAngle)
    , m_center(center) {
  addParam(startAngle);
  addParam(endAngle);
  addParam(center->getX());
  addParam(center->getY());
}

//---------------------------------------------------------------------------

void AngleRangeFxGadget::draw(bool picking) {
  auto setColorById = [&](int id) {
    if (isSelected(id))
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
  };

  double pixelSize = sqrt(tglGetPixelSize2()) * m_controller->getDevPixRatio();
  double r         = pixelSize * 200;
  double a         = pixelSize * 30;

  TPointD center = getValue(m_center);
  double start   = getValue(m_startAngle);
  double end     = getValue(m_endAngle);

  glPushMatrix();
  glTranslated(center.x, center.y, 0);

  setColorById(StartAngle);
  glPushMatrix();
  glPushName(getId() + StartAngle);
  glRotated(start, 0, 0, 1);
  glBegin(GL_LINE_STRIP);
  glVertex2d(0, 0);
  glVertex2d(r, 0);
  // expand handle while dragging
  if (m_handle == StartAngle) glVertex2d(r * 5.0, 0);
  glEnd();
  glPopName();

  glPushMatrix();
  glTranslated(r * 1.05, 0, 0.0);
  glScaled(pixelSize * 1.6, pixelSize * 1.6, 1);
  glRotated(-start, 0, 0, 1);
  tglDrawText(TPointD(0, 0), "Start Angle");
  glPopMatrix();

  glPopMatrix();

  setColorById(EndAngle);
  glPushMatrix();
  glPushName(getId() + EndAngle);
  glRotated(end, 0, 0, 1);
  glBegin(GL_LINE_STRIP);
  glVertex2d(0, 0);
  glVertex2d(r, 0);
  // expand handle while dragging
  if (m_handle == EndAngle) glVertex2d(r * 5.0, 0);
  glEnd();

  glPopName();
  glPushMatrix();
  glTranslated(r * 1.05, 0, 0.0);
  glScaled(pixelSize * 1.6, pixelSize * 1.6, 1);
  glRotated(-end, 0, 0, 1);
  tglDrawText(TPointD(0, 0), "End Angle");
  glPopMatrix();

  glPopMatrix();

  // draw arc
  while (end <= start) end += 360.0;

  glColor3d(0, 0, 1);
  glBegin(GL_LINE_STRIP);
  double angle  = start;
  double dAngle = 5.0;
  while (angle <= end) {
    double rad = angle / M_180_PI;
    glVertex2d(a * std::cos(rad), a * std::sin(rad));
    angle += dAngle;
  }
  if (angle != end)
    glVertex2d(a * std::cos(end / M_180_PI), a * std::sin(end / M_180_PI));
  glEnd();

  glPopMatrix();
}

//---------------------------------------------------------------------------

void AngleRangeFxGadget::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &) {
  m_handle = (HANDLE)m_selected;
  if (m_handle == None) return;
  TPointD d             = pos - getValue(m_center);
  m_clickedAngle        = atan2(d.y, d.x) * M_180_PI;
  TDoubleParamP target  = (m_handle == StartAngle) ? m_startAngle : m_endAngle;
  TDoubleParamP another = (m_handle == StartAngle) ? m_endAngle : m_startAngle;
  m_targetAngle         = getValue(target);
  m_anotherAngle        = getValue(another);
}

//---------------------------------------------------------------------------

void AngleRangeFxGadget::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  if (m_handle == None) return;
  TDoubleParamP target = (m_handle == StartAngle) ? m_startAngle : m_endAngle;
  TPointD d            = pos - getValue(m_center);
  double angle         = atan2(d.y, d.x) * M_180_PI;
  double targetAngle   = m_targetAngle + angle - m_clickedAngle;
  // move every 10 degrees when pressing Shift key
  if (e.isShiftPressed()) targetAngle = std::round(targetAngle / 10.0) * 10.0;
  setValue(target, targetAngle);

  // move both angles when pressing Ctrl key
  if (e.isCtrlPressed()) {
    TDoubleParamP another =
        (m_handle == StartAngle) ? m_endAngle : m_startAngle;
    double anotherAngle = m_anotherAngle + angle - m_clickedAngle;
    if (e.isShiftPressed())
      anotherAngle = std::round(anotherAngle / 10.0) * 10.0;
    setValue(another, anotherAngle);
  }
}

//---------------------------------------------------------------------------

void AngleRangeFxGadget::leftButtonUp() { m_handle = None; }

//=============================================================================

class DiamondFxGadget final : public FxGadget {
  TDoubleParamP m_param;

public:
  DiamondFxGadget(FxGadgetController *controller, const TDoubleParamP &param)
      : FxGadget(controller), m_param(param) {
    addParam(param);
  }

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void DiamondFxGadget::draw(bool picking) {
  setPixelSize();

  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  double size = getValue(m_param);
  double r    = 3 * getPixelSize();

  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  glBegin(GL_LINES);
  glVertex2d(-size + r, r);
  glVertex2d(-r, size - r);
  glVertex2d(r, size - r);
  glVertex2d(size - r, r);
  glVertex2d(size - r, -r);
  glVertex2d(r, -size + r);
  glVertex2d(-r, -size + r);
  glVertex2d(-size + r, -r);
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  drawDot(-size, 0);
  drawDot(size, 0);
  drawDot(0, -size);
  drawDot(0, size);

  double d = getPixelSize() * 3;
  glPopName();
  if (isSelected()) {
    drawTooltip(TPointD(d, size - d), getLabel());
  }
}

//---------------------------------------------------------------------------

void DiamondFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  double sz = fabs(pos.x) + fabs(pos.y);
  if (sz < 0.1) sz = 0.1;
  setValue(m_param, sz);
}

//=============================================================================

class SizeFxGadget final : public FxGadget {
  TDoubleParamP m_lx, m_ly;

public:
  SizeFxGadget(FxGadgetController *controller, const TDoubleParamP &lx,
               const TDoubleParamP &ly)
      : FxGadget(controller), m_lx(lx), m_ly(ly) {
    addParam(lx);
    if (ly) addParam(ly);
  }

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void SizeFxGadget::draw(bool picking) {
  setPixelSize();

  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  double lx = getValue(m_lx), ly = m_ly ? getValue(m_ly) : lx;
  double r = getPixelSize() * 3;

  glLineStipple(1, 0xCCCC);
  glEnable(GL_LINE_STIPPLE);
  glBegin(GL_LINES);
  glVertex2d(0, 0);
  glVertex2d(lx, 0);
  glVertex2d(0, 0);
  glVertex2d(0, ly);
  glVertex2d(lx, 0);
  glVertex2d(lx, ly - r);
  glVertex2d(0, ly);
  glVertex2d(lx - r, ly);
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  drawDot(lx, ly);

  double d = getPixelSize() * 3;
  glPopName();
  if (isSelected()) {
    drawTooltip(TPointD(lx, ly), getLabel());
  }
}

//---------------------------------------------------------------------------

void SizeFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  if (m_ly)
    setValue(m_lx, std::max(pos.x, 0.1)), setValue(m_ly, std::max(pos.y, 0.1));
  else
    setValue(m_lx, std::max({pos.x, pos.y, 0.1}));
}

//=============================================================================

class RectFxGadget final : public FxGadget {
  TDoubleParamP m_width, m_height;
  TPointParamP m_center;

  int m_picked;

public:
  enum { None, Corner, HorizontalSide, VerticalSide };

public:
  RectFxGadget(FxGadgetController *controller, const TDoubleParamP &width,
               const TDoubleParamP &height, const TPointParamP &center)
      : FxGadget(controller)
      , m_width(width)
      , m_height(height)
      , m_center(center)
      , m_picked(None) {
    addParam(width);
    addParam(height);
    if (center) addParam(center->getX()), addParam(center->getY());
  }

  TPointD getCenter() const {
    return m_center ? getValue(m_center) : TPointD();
  }

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void RectFxGadget::draw(bool picking) {
  setPixelSize();

  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  glPushMatrix();
  TPointD center = getCenter();
  glTranslated(center.x, center.y, 0);
  double w_2 = 0.5 * getValue(m_width);
  double h_2 = 0.5 * getValue(m_height);
  double r   = getPixelSize() * 3;

  glLineStipple(1, 0xCCCC);
  glEnable(GL_LINE_STIPPLE);
  glBegin(GL_LINES);
  glVertex2d(-w_2 + r, -h_2);
  glVertex2d(w_2 - r, -h_2);
  glVertex2d(-w_2 + r, h_2);
  glVertex2d(w_2 - r, h_2);
  glVertex2d(-w_2, -h_2 + r);
  glVertex2d(-w_2, h_2 - r);
  glVertex2d(w_2, -h_2 + r);
  glVertex2d(w_2, h_2 - r);
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  drawDot(w_2, h_2);
  drawDot(-w_2, h_2);
  drawDot(w_2, -h_2);
  drawDot(-w_2, -h_2);

  glPopMatrix();
}

//---------------------------------------------------------------------------

void RectFxGadget::leftButtonDown(const TPointD &ppos, const TMouseEvent &) {
  TPointD pos = ppos - getCenter();
  m_picked    = None;
  double w_2  = 0.5 * getValue(m_width);
  double h_2  = 0.5 * getValue(m_height);
  double x    = fabs(pos.x);
  double y    = fabs(pos.y);
  double r    = getPixelSize() * 15;

  if (fabs(w_2 - x) < r && fabs(y - h_2) < r)
    m_picked = Corner;
  else if (fabs(w_2 - x) < r && y < h_2)
    m_picked = VerticalSide;
  else if (fabs(h_2 - y) < r && x < w_2)
    m_picked = HorizontalSide;
}

//---------------------------------------------------------------------------

void RectFxGadget::leftButtonDrag(const TPointD &ppos, const TMouseEvent &) {
  TPointD pos = ppos - getCenter();
  double w = fabs(pos.x), h = fabs(pos.y);

  if (m_picked == Corner || m_picked == VerticalSide)
    setValue(m_width, 2.0 * w);
  if (m_picked == Corner || m_picked == HorizontalSide)
    setValue(m_height, 2.0 * h);
}

//=============================================================================

class PolarFxGadget final : public FxGadget {
  TPointD m_pos;
  TDoubleParamP m_phiParam, m_lengthParam;

public:
  PolarFxGadget(FxGadgetController *controller, const TPointD &pos,
                const TDoubleParamP &phiParam, const TDoubleParamP &lengthParam)
      : FxGadget(controller)
      , m_pos(pos)
      , m_phiParam(phiParam)
      , m_lengthParam(lengthParam) {
    addParam(phiParam);
    addParam(lengthParam);
  }

  void draw(bool picking) override {
    setPixelSize();
    if (isSelected())
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
    glPushName(getId());
    double pixelSize = getPixelSize();
    double r         = getValue(m_lengthParam);
    double a = pixelSize * 10, b = pixelSize * 5, c = pixelSize * 4;
    // tglDrawCircle(m_pos, r);
    double phi = getValue(m_phiParam);
    glPushMatrix();
    glTranslated(m_pos.x, m_pos.y, 0);
    glRotated(phi, 0, 0, 1);
    double rr = r - c;
    if (rr > 0) {
      glLineStipple(1, 0xAAAA);
      glEnable(GL_LINE_STIPPLE);
      glBegin(GL_LINE_STRIP);
      glVertex2d(0, 0);
      glVertex2d(rr, 0);
      glEnd();
      glDisable(GL_LINE_STIPPLE);
    }
    glBegin(GL_LINES);
    glVertex2d(rr, 0);
    glVertex2d(rr - a, b);
    glVertex2d(rr, 0);
    glVertex2d(rr - a, -b);
    glEnd();
    glTranslated(r, 0, 0);
    glRotated(-phi, 0, 0, 1);
    drawDot(0, 0);
    glPopMatrix();
    glPopName();

    if (isSelected()) {
      double phiRad      = phi * M_PI_180;
      TPointD toolTipPos = m_pos + r * TPointD(cos(phiRad), sin(phiRad));
      drawTooltip(toolTipPos, getLabel());
    }
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    TPointD d     = pos - m_pos;
    double phi    = atan2(d.y, d.x);
    double length = norm(d);
    setValue(m_phiParam, phi * M_180_PI);
    setValue(m_lengthParam, length);
  }
};

//=============================================================================

class VectorFxGadget final : public FxGadget {
  TPointParamP m_pa, m_pb;

public:
  VectorFxGadget(FxGadgetController *controller, const TPointParamP &pa,
                 const TPointParamP &pb)
      : FxGadget(controller), m_pa(pa), m_pb(pb) {
    addParam(pa->getX());
    addParam(pa->getY());
    addParam(pb->getX());
    addParam(pb->getY());
  }

  void draw(bool picking) override {
    setPixelSize();
    if (isSelected())
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
    // glPushName(getId());
    double pixelSize = getPixelSize();
    TPointD pa       = getValue(m_pa);
    TPointD pb       = getValue(m_pb);
    TPointD dab      = pb - pa;
    double ab2       = norm2(dab);
    if (ab2 > 0.0001) {
      double ab = sqrt(ab2);
      TPointD u = dab * (1.0 / ab);
      TPointD v = rotate90(u);

      double a = pixelSize * 10, b = pixelSize * 5;
      double c = pixelSize * 4;

      TPointD pbb = pb - u * c;
      if (ab - c > 0) {
        glLineStipple(1, 0xAAAA);
        glEnable(GL_LINE_STIPPLE);
        tglDrawSegment(pa, pbb);
        glDisable(GL_LINE_STIPPLE);
      }
      tglDrawSegment(pbb, pbb - u * a + v * b);
      tglDrawSegment(pbb, pbb - u * a - v * b);
      // drawDot(pa);
      // drawDot(pb);
    }  // else
       // drawDot(pa);
    // glPopName();
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {}
};

//=============================================================================

class QuadFxGadget final : public FxGadget {
  TPointParamP m_TL, m_TR, m_BR, m_BL;

  enum HANDLE {
    Body = 0,
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft,
    TopEdge,
    RightEdge,
    BottomEdge,
    LeftEdge,
    None
  } m_handle = None;

  TPointD m_pivot;
  TPointD m_dragStartPos;
  TPointD m_startTL, m_startTR, m_startBR, m_startBL;

public:
  QuadFxGadget(FxGadgetController *controller, const TPointParamP &topLeft,
               const TPointParamP &topRight, const TPointParamP &bottomRight,
               const TPointParamP &bottomLeft)
      : FxGadget(controller, 9)
      , m_TL(topLeft)
      , m_TR(topRight)
      , m_BR(bottomRight)
      , m_BL(bottomLeft) {
    addParam(topLeft->getX());
    addParam(topLeft->getY());
    addParam(topRight->getX());
    addParam(topRight->getY());
    addParam(bottomRight->getX());
    addParam(bottomRight->getY());
    addParam(bottomLeft->getX());
    addParam(bottomLeft->getY());
  }

  void draw(bool picking) override {
    int idBase = getId();

    auto setColorById = [&](int id) {
      if (isSelected(id))
        glColor3dv(m_selectedColor);
      else
        glColor3d(0, 0, 1);
    };

    auto id2Str = [](const HANDLE handleId) -> std::string {
      switch (handleId) {
      case TopLeft:
        return "Top Left";
      case TopRight:
        return "Top Right";
      case BottomRight:
        return "Bottom Right";
      case BottomLeft:
        return "Bottom Left";
      default:
        return "";
      }
    };

    auto drawPoint = [&](const TPointD &pos, int id) {
      setColorById(id);
      glPushName(idBase + id);
      double unit = getPixelSize();
      glPushMatrix();
      glTranslated(pos.x, pos.y, 0);
      double r = unit * 3;
      tglDrawRect(-r, -r, r, r);
      glPopMatrix();
      glPopName();

      if (isSelected(id) && id >= TopLeft && id <= BottomLeft) {
        drawTooltip(pos + TPointD(7, 3) * unit,
                    id2Str((HANDLE)id) + getLabel());
      }
    };

    setPixelSize();

    // lines for moving all vertices
    glPushName(idBase + Body);
    setColorById(Body);
    double pixelSize    = getPixelSize();
    TPointD topLeft     = getValue(m_TL);
    TPointD topRight    = getValue(m_TR);
    TPointD bottomRight = getValue(m_BR);
    TPointD bottomLeft  = getValue(m_BL);
    glLineStipple(1, 0xCCCC);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINE_STRIP);
    tglVertex(topLeft);
    tglVertex(topRight);
    tglVertex(bottomRight);
    tglVertex(bottomLeft);
    tglVertex(topLeft);
    glEnd();
    glDisable(GL_LINE_STIPPLE);
    glPopName();

    // corners
    drawPoint(topLeft, TopLeft);
    drawPoint(topRight, TopRight);
    drawPoint(bottomRight, BottomRight);
    drawPoint(bottomLeft, BottomLeft);

    // center of the edges
    drawPoint((topLeft + topRight) * 0.5, TopEdge);
    drawPoint((topRight + bottomRight) * 0.5, RightEdge);
    drawPoint((bottomRight + bottomLeft) * 0.5, BottomEdge);
    drawPoint((bottomLeft + topLeft) * 0.5, LeftEdge);
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp() override;
};

//---------------------------------------------------------------------------

void QuadFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_handle       = (HANDLE)m_selected;
  m_dragStartPos = pos;
  m_startTL      = getValue(m_TL);
  m_startTR      = getValue(m_TR);
  m_startBR      = getValue(m_BR);
  m_startBL      = getValue(m_BL);
  m_pivot        = (m_startTL + m_startTR + m_startBR + m_startBL) * 0.25;
}

//---------------------------------------------------------------------------

void QuadFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  TPointD offset = pos - m_dragStartPos;

  auto scaleShape = [&](const TPointD &start, const TPointD &pivot) {
    TPointD startVec = start - pivot;
    TPointD endVec   = start + offset - pivot;
    TPointD scaleFac((startVec.x == 0.0) ? 1.0 : endVec.x / startVec.x,
                     (startVec.y == 0.0) ? 1.0 : endVec.y / startVec.y);
    if (e.isShiftPressed()) {
      if (std::abs(scaleFac.x) > std::abs(scaleFac.y))
        scaleFac.y = scaleFac.x;
      else
        scaleFac.x = scaleFac.y;
    }
    if (m_startTL != pivot)
      setValue(m_TL, pivot + hadamard((m_startTL - pivot), scaleFac));
    if (m_startTR != pivot)
      setValue(m_TR, pivot + hadamard((m_startTR - pivot), scaleFac));
    if (m_startBR != pivot)
      setValue(m_BR, pivot + hadamard((m_startBR - pivot), scaleFac));
    if (m_startBL != pivot)
      setValue(m_BL, pivot + hadamard((m_startBL - pivot), scaleFac));
  };

  auto doCorner = [&](const TPointParamP point, const TPointD &start,
                      const TPointD &opposite) {
    if (e.isCtrlPressed())
      setValue(point, start + offset);
    else if (e.isAltPressed())
      scaleShape(start, m_pivot);
    else
      scaleShape(start, opposite);
  };

  auto doEdge = [&](const TPointParamP p1, const TPointParamP p2) {
    if (e.isShiftPressed()) {
      if (std::abs(offset.x) > std::abs(offset.y))
        offset.y = 0;
      else
        offset.x = 0;
    }
    if (m_TL == p1 || m_TL == p2)
      setValue(m_TL, m_startTL + offset);
    else if (e.isAltPressed())
      setValue(m_TL, m_startTL - offset);
    if (m_TR == p1 || m_TR == p2)
      setValue(m_TR, m_startTR + offset);
    else if (e.isAltPressed())
      setValue(m_TR, m_startTR - offset);
    if (m_BR == p1 || m_BR == p2)
      setValue(m_BR, m_startBR + offset);
    else if (e.isAltPressed())
      setValue(m_BR, m_startBR - offset);
    if (m_BL == p1 || m_BL == p2)
      setValue(m_BL, m_startBL + offset);
    else if (e.isAltPressed())
      setValue(m_BL, m_startBL - offset);
  };

  auto pointRotate = [&](const TPointD pos, const double angle) {
    TPointD p = pos - m_pivot;
    return m_pivot + TPointD(p.x * std::cos(angle) - p.y * std::sin(angle),
                             p.x * std::sin(angle) + p.y * std::cos(angle));
  };

  switch (m_handle) {
  case Body:
    if (e.isCtrlPressed()) {  // rotate
      TPointD startVec   = m_dragStartPos - m_pivot;
      TPointD currentVec = pos - m_pivot;
      if (currentVec == TPointD()) return;
      double angle = std::atan2(currentVec.y, currentVec.x) -
                     std::atan2(startVec.y, startVec.x);
      if (e.isShiftPressed()) {
        angle = std::round(angle / (M_PI / 2.0)) * (M_PI / 2.0);
      }
      setValue(m_TL, pointRotate(m_startTL, angle));
      setValue(m_TR, pointRotate(m_startTR, angle));
      setValue(m_BR, pointRotate(m_startBR, angle));
      setValue(m_BL, pointRotate(m_startBL, angle));
    } else {  // translate
      // move all shapes
      if (e.isShiftPressed()) {
        if (std::abs(offset.x) > std::abs(offset.y))
          offset.y = 0;
        else
          offset.x = 0;
      }
      setValue(m_TL, m_startTL + offset);
      setValue(m_TR, m_startTR + offset);
      setValue(m_BR, m_startBR + offset);
      setValue(m_BL, m_startBL + offset);
    }
    break;
  case TopLeft:
    doCorner(m_TL, m_startTL, m_startBR);
    break;
  case TopRight:
    doCorner(m_TR, m_startTR, m_startBL);
    break;
  case BottomRight:
    doCorner(m_BR, m_startBR, m_startTL);
    break;
  case BottomLeft:
    doCorner(m_BL, m_startBL, m_startTR);
    break;
  case TopEdge:
    doEdge(m_TL, m_TR);
    break;
  case RightEdge:
    doEdge(m_TR, m_BR);
    break;
  case BottomEdge:
    doEdge(m_BR, m_BL);
    break;
  case LeftEdge:
    doEdge(m_BL, m_TL);
    break;
  default:
    break;
  }
}

//---------------------------------------------------------------------------

void QuadFxGadget::leftButtonUp() { m_handle = None; }

//=============================================================================

class LinearRangeFxGadget final : public FxGadget {
  TPointParamP m_start, m_end;

  enum HANDLE { Body = 0, Start, End, None } m_handle = None;

  TPointD m_clickedPos;
  TPointD m_targetPos, m_anotherPos;

public:
  LinearRangeFxGadget(FxGadgetController *controller,
                      const TPointParamP &startPoint,
                      const TPointParamP &endPoint);

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp() override;
};

//---------------------------------------------------------------------------

LinearRangeFxGadget::LinearRangeFxGadget(FxGadgetController *controller,
                                         const TPointParamP &startPoint,
                                         const TPointParamP &endPoint)
    : FxGadget(controller, 3), m_start(startPoint), m_end(endPoint) {
  addParam(startPoint->getX());
  addParam(startPoint->getY());
  addParam(endPoint->getX());
  addParam(endPoint->getY());
}

//---------------------------------------------------------------------------

void LinearRangeFxGadget::draw(bool picking) {
  auto setColorById = [&](int id) {
    if (isSelected(id))
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
  };

  auto drawPoint = [&]() {
    double r = getPixelSize() * 3;
    double d = getPixelSize() * 6;
    glBegin(GL_LINES);
    glVertex2d(-d, 0);
    glVertex2d(-r, 0);
    glVertex2d(d, 0);
    glVertex2d(r, 0);
    glVertex2d(0, -d);
    glVertex2d(0, -r);
    glVertex2d(0, d);
    glVertex2d(0, r);
    glEnd();
    tglDrawRect(-r, -r, r, r);
  };

  setPixelSize();
  double r = getPixelSize() * 200;
  double a = getPixelSize() * 5;

  TPointD start = getValue(m_start);
  TPointD end   = getValue(m_end);

  glPushMatrix();

  if (start != end) {
    // draw lines perpendicular to the line between ends
    double angle = std::atan2(start.x - end.x, end.y - start.y) * M_180_PI;
    // start
    setColorById(Start);
    glPushMatrix();
    glTranslated(start.x, start.y, 0);
    glRotated(angle, 0, 0, 1);
    if (m_handle == Start) glScaled(5.0, 1.0, 1.0);
    glBegin(GL_LINES);
    glVertex2d(-r, 0);
    glVertex2d(r, 0);
    glEnd();
    glPopMatrix();
    // end
    setColorById(End);
    glPushMatrix();
    glTranslated(end.x, end.y, 0);
    glRotated(angle, 0, 0, 1);
    if (m_handle == End) glScaled(5.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    glVertex2d(-r, 0);
    glVertex2d(r, 0);
    glEnd();
    glPopMatrix();

    // line body
    setColorById(Body);
    glPushName(getId() + Body);
    glBegin(GL_LINES);
    glVertex2d(start.x, start.y);
    glVertex2d(end.x, end.y);
    glEnd();
    // small dash at the center
    glPushMatrix();
    glTranslated((start.x + end.x) / 2.0, (start.y + end.y) / 2.0, 0);
    glRotated(angle, 0, 0, 1);
    glBegin(GL_LINES);
    glVertex2d(-a, 0);
    glVertex2d(a, 0);
    glEnd();
    glPopMatrix();
    glPopName();
  }

  // start point
  setColorById(Start);
  glPushName(getId() + Start);
  glPushMatrix();
  glTranslated(start.x, start.y, 0);
  drawPoint();
  glPopMatrix();
  glPopName();
  drawTooltip(start + TPointD(7, 3) * getPixelSize(), "Start");

  // end point
  setColorById(End);
  glPushName(getId() + End);
  glPushMatrix();
  glTranslated(end.x, end.y, 0);
  drawPoint();
  glPopMatrix();
  glPopName();
  drawTooltip(end + TPointD(7, 3) * getPixelSize(), "End");

  glPopMatrix();
}

//---------------------------------------------------------------------------

void LinearRangeFxGadget::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &) {
  m_handle = (HANDLE)m_selected;
  if (m_handle == None) return;
  m_clickedPos = pos;
  m_targetPos  = (m_handle == Start || m_handle == Body) ? getValue(m_start)
                                                         : getValue(m_end);
  m_anotherPos = (m_handle == Start || m_handle == Body) ? getValue(m_end)
                                                         : getValue(m_start);
}

//---------------------------------------------------------------------------

void LinearRangeFxGadget::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (m_handle == None) return;
  TPointD d = pos - m_clickedPos;

  if (m_handle == Body) {
    setValue(m_start, m_targetPos + d);
    setValue(m_end, m_anotherPos + d);
    return;
  }

  TPointParamP target = (m_handle == Start) ? m_start : m_end;

  if (m_targetPos != m_anotherPos && e.isShiftPressed()) {
    TPointD vecA = m_targetPos - m_anotherPos;
    TPointD vecB = m_targetPos + d - m_anotherPos;
    d            = vecA * ((vecA.x * vecB.x + vecA.y * vecB.y) /
                    (vecA.x * vecA.x + vecA.y * vecA.y) -
                1.0);
  }

  setValue(target, m_targetPos + d);

  if (e.isCtrlPressed()) {
    TPointParamP another = (m_handle == Start) ? m_end : m_start;
    setValue(another, m_anotherPos - d);
  }
}

//---------------------------------------------------------------------------

void LinearRangeFxGadget::leftButtonUp() { m_handle = None; }

//=============================================================================

class CompassFxGadget final : public FxGadget {
  TPointParamP m_center;
  TDoubleParamP m_ellipse_aspect_ratio;
  TDoubleParamP m_ellipse_angle;
  TDoubleParamP m_twist;

  enum HANDLE { Body = 0, Near, Far, None } m_handle = None;

  TPointD m_clickedPos, m_mousePos;
  TPointD m_targetPos, m_anotherPos;

  bool m_isSpin;

public:
  CompassFxGadget(FxGadgetController *controller,
                  const TPointParamP &centerPoint, bool isSpin = false,
                  const TDoubleParamP &ellipse_aspect_ratio = TDoubleParamP(),
                  const TDoubleParamP &ellipse_angle        = TDoubleParamP(),
                  const TDoubleParamP &twist                = TDoubleParamP());

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp() override;
};

//---------------------------------------------------------------------------

CompassFxGadget::CompassFxGadget(FxGadgetController *controller,
                                 const TPointParamP &centerPoint, bool isSpin,
                                 const TDoubleParamP &ellipse_aspect_ratio,
                                 const TDoubleParamP &ellipse_angle,
                                 const TDoubleParamP &twist)
    : FxGadget(controller, 3)
    , m_center(centerPoint)
    , m_isSpin(isSpin)
    , m_ellipse_aspect_ratio(ellipse_aspect_ratio)
    , m_ellipse_angle(ellipse_angle)
    , m_twist(twist) {
  addParam(centerPoint->getX());
  addParam(centerPoint->getY());
  if (ellipse_aspect_ratio) addParam(ellipse_aspect_ratio);
  if (ellipse_angle) addParam(ellipse_angle);
}

//---------------------------------------------------------------------------

void CompassFxGadget::draw(bool picking) {
  auto setColorById = [&](int id) {
    if (isSelected(id))
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
  };

  auto drawArrow = [&]() {
    double arrowLength = getPixelSize() * 20;
    double arrowTip    = getPixelSize() * 5;

    glBegin(GL_LINES);
    glVertex2d(-arrowLength, 0.0);
    glVertex2d(arrowLength, 0.0);

    glVertex2d(-arrowLength + arrowTip, arrowTip);
    glVertex2d(-arrowLength, 0.0);

    glVertex2d(-arrowLength + arrowTip, -arrowTip);
    glVertex2d(-arrowLength, 0.0);

    glVertex2d(arrowLength - arrowTip, arrowTip);
    glVertex2d(arrowLength, 0.0);

    glVertex2d(arrowLength - arrowTip, -arrowTip);
    glVertex2d(arrowLength, 0.0);
    glEnd();
  };

  setPixelSize();
  double lineHalf     = getPixelSize() * 100;
  double lineInterval = getPixelSize() * 50;
  double r            = getPixelSize() * 3;

  glPushMatrix();

  TPointD center = getValue(m_center);
  double dCenter = norm(center);
  double e_aspect_ratio =
      (m_ellipse_aspect_ratio) ? getValue(m_ellipse_aspect_ratio) : 1.0;
  double e_angle    = (m_ellipse_angle) ? getValue(m_ellipse_angle) : 0.0;
  TRectD cameraRect = m_controller->getCameraRect();
  double pivot      = getPixelSize() * cameraRect.getLy() / 2.0;
  double twist      = (m_twist) ? getValue(m_twist) * M_PI_180 : 0.0;

  TPointD handleVec;
  if (dCenter > lineHalf) {
    handleVec = normalize(center) * lineHalf;
    setColorById(Body);
    glPushName(getId() + Body);
    glBegin(GL_LINES);
    glVertex2d(handleVec.x * 0.95, handleVec.y * 0.95);
    glVertex2d(-handleVec.x * 0.95, -handleVec.y * 0.95);
    glEnd();
    glPopName();

    double angle = std::atan2(-center.y, -center.x) * M_180_PI;
    double theta = M_180_PI * lineInterval / dCenter;

    // draw spin lines field
    if (isSelected() && !isSelected(None) && m_ellipse_aspect_ratio &&
        m_ellipse_angle) {
      TRectD geom = m_controller->getGeometry();
      if (m_isSpin)
        drawSpinField(geom, center, lineInterval, e_aspect_ratio, e_angle);
      else
        drawRadialField(geom, center, lineInterval, e_aspect_ratio, e_angle,
                        twist, pivot);

    } else {
      // draw guides
      glColor3d(0, 0, 1);
      glLineStipple(1, 0x00FF);
      glEnable(GL_LINE_STIPPLE);
      glPushMatrix();
      glTranslated(center.x, center.y, 0);
      if (!m_isSpin) {  // radial direction

        if (areAlmostEqual(twist, 0.0)) {
          for (int i = -3; i <= 3; i++) {
            if (i == 0) continue;

            glPushMatrix();
            glRotated(theta * (double)i + angle, 0, 0, 1);
            glBegin(GL_LINES);
            glVertex2d(dCenter - lineHalf, 0.0);
            glVertex2d(dCenter + lineHalf, 0.0);
            glEnd();
            glPopMatrix();
          }
        } else if (areAlmostEqual(e_aspect_ratio, 1.0)) {  // twist case
          for (int i = -3; i <= 3; i++) {
            if (i == 0) continue;

            glPushMatrix();
            glRotated(theta * (double)i + angle, 0, 0, 1);

            glBegin(GL_LINE_STRIP);
            for (int j = 0; j <= RADIAL_COMPASS_NUMSEGMENTS; j++) {
              double tmp_d = dCenter - lineHalf +
                             (double)j * 2.0 * lineHalf /
                                 (double)RADIAL_COMPASS_NUMSEGMENTS;
              double tmp_tw_radian = twist * (tmp_d - dCenter) / pivot;
              glVertex2d(tmp_d * std::cos(tmp_tw_radian),
                         tmp_d * std::sin(tmp_tw_radian));
            }
            glEnd();
            glPopMatrix();
          }
        } else {  // elliptical + twist case
          double scale[2];
          scale[0] = 2.0 * e_aspect_ratio / (e_aspect_ratio + 1);
          scale[1] = scale[0] / e_aspect_ratio;
          glPushMatrix();
          glRotated(e_angle, 0., 0., 1.);
          glScaled(scale[0], scale[1], 1.);

          QTransform tr =
              QTransform().rotate(e_angle).scale(scale[0], scale[1]).inverted();

          for (int i = -3; i <= 3; i++) {
            if (i == 0) continue;
            double tmp_angle_radian = (theta * (double)i + angle) * M_PI_180;
            QPointF lineCenter(dCenter * std::cos(tmp_angle_radian),
                               dCenter * std::sin(tmp_angle_radian));
            lineCenter = tr.map(lineCenter);

            tmp_angle_radian   = std::atan2(lineCenter.y(), lineCenter.x());
            double dLineCenter = QVector2D(lineCenter).length();
            double tmpLineHalf = lineHalf * dLineCenter / dCenter;
            glBegin(GL_LINE_STRIP);
            for (int j = 0; j <= RADIAL_COMPASS_NUMSEGMENTS; j++) {
              double tmp_d = dLineCenter - tmpLineHalf +
                             (double)j * 2.0 * tmpLineHalf /
                                 (double)RADIAL_COMPASS_NUMSEGMENTS;
              double tmp_tw_radian = twist * (tmp_d - dLineCenter) / pivot;

              QPointF p(tmp_d * std::cos(tmp_angle_radian + tmp_tw_radian),
                        tmp_d * std::sin(tmp_angle_radian + tmp_tw_radian));
              glVertex2d(p.x(), p.y());
            }
            glEnd();
          }
          glPopMatrix();
        }
      } else {  // rotational direction
        if (areAlmostEqual(e_aspect_ratio, 1.0)) {
          for (int i = -2; i <= 2; i++) {
            double tmpRad  = dCenter + (double)i * lineInterval;
            double d_angle = (lineInterval / dCenter) * 6.0 / 10.0;
            glBegin(GL_LINE_STRIP);
            for (int r = -5; r <= 5; r++) {
              double tmpAngle = (double)r * d_angle + angle * M_PI_180;
              glVertex2d(tmpRad * std::cos(tmpAngle),
                         tmpRad * std::sin(tmpAngle));
            }
            glEnd();
          }
        } else {  // elliptical case
          double scale[2];
          scale[0] = 2.0 * e_aspect_ratio / (e_aspect_ratio + 1);
          scale[1] = scale[0] / e_aspect_ratio;
          glRotated(e_angle, 0., 0., 1.);
          glScaled(scale[0], scale[1], 1.);

          QTransform tr = QTransform()
                              .translate(center.x, center.y)
                              .rotate(e_angle)
                              .scale(scale[0], scale[1])
                              .inverted();
          QPointF begin = tr.map(QPointF(handleVec.x, handleVec.y));
          QPointF end   = tr.map(QPointF(-handleVec.x, -handleVec.y));

          angle            = std::atan2(begin.y(), begin.x());
          double distBegin = QVector2D(begin).length();
          double distEnd   = QVector2D(end).length();
          for (int i = 0; i <= 4; i++) {
            double tmpRad  = distBegin + (double)i * (distEnd - distBegin) / 4.;
            double d_angle = (lineInterval / dCenter) * 6.0 / 10.0;
            glBegin(GL_LINE_STRIP);
            for (int r = -5; r <= 5; r++) {
              double tmpAngle = (double)r * d_angle + angle;
              glVertex2d(tmpRad * std::cos(tmpAngle),
                         tmpRad * std::sin(tmpAngle));
            }
            glEnd();
          }
        }
      }
    }

    glPopMatrix();
    glDisable(GL_LINE_STIPPLE);

    for (int id = Near; id <= Far; id++) {
      TPointD hPos = (id == Near) ? handleVec : -handleVec;
      setColorById(id);
      glPushName(getId() + id);
      glPushMatrix();
      glTranslated(hPos.x, hPos.y, 0);
      tglDrawRect(-r, -r, r, r);
      glPopMatrix();
      glPopName();
    }
  }

  if (m_handle == Body) {
    glPushMatrix();
    TPointD centerOffset = center - m_targetPos;
    handleVec            = normalize(m_targetPos) * lineHalf;
    glTranslated(centerOffset.x, centerOffset.y, 0);
    glBegin(GL_LINES);
    glVertex2d(handleVec.x, handleVec.y);
    glVertex2d(-handleVec.x, -handleVec.y);
    glEnd();
    glPopMatrix();
  }
  glPopMatrix();
}

//---------------------------------------------------------------------------

void CompassFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_handle = (HANDLE)m_selected;
  if (m_handle == None) return;
  m_clickedPos = pos;
  m_targetPos  = getValue(m_center);
}

//---------------------------------------------------------------------------

void CompassFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_handle == None) return;
  TPointD d = pos - m_clickedPos;

  if (m_handle == Body) {
    setValue(m_center, m_targetPos + d);
    return;
  }

  double angle =
      std::atan2(pos.y, pos.x) - std::atan2(m_clickedPos.y, m_clickedPos.x);
  double scale = norm(pos) / norm(m_clickedPos);

  QTransform transform;
  QPointF p = transform.rotateRadians(angle)
                  .scale(scale, scale)
                  .map(QPointF(m_targetPos.x, m_targetPos.y));

  setValue(m_center, TPointD(p.x(), p.y()));
}

//---------------------------------------------------------------------------

void CompassFxGadget::leftButtonUp() { m_handle = None; }

//=============================================================================

class RainbowWidthFxGadget final : public FxGadget {
  TDoubleParamP m_widthScale;
  TDoubleParamP m_radius;
  TPointParamP m_center;

  enum HANDLE { Outside = 0, Inside, None } m_handle = None;

public:
  RainbowWidthFxGadget(FxGadgetController *controller,
                       const TDoubleParamP &widthScale,
                       const TDoubleParamP &radius, const TPointParamP &center)
      : FxGadget(controller, 2)
      , m_widthScale(widthScale)
      , m_radius(radius)
      , m_center(center) {
    addParam(widthScale);
  }

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------

void RainbowWidthFxGadget::draw(bool picking) {
  setPixelSize();
  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  double radius     = getValue(m_radius);
  TPointD center    = getValue(m_center);
  double widthScale = getValue(m_widthScale);
  double w          = widthScale * radius / 41.3;

  glPushName(getId() + Outside);
  glLineStipple(1, 0x1C47);
  glEnable(GL_LINE_STIPPLE);
  tglDrawCircle(center, radius + w);
  glDisable(GL_LINE_STIPPLE);
  drawDot(center + TPointD(0.707, 0.707) * (radius + w));
  glPopName();

  if (isSelected(Outside)) {
    drawTooltip(center + TPointD(0.707, 0.707) * (radius + w), getLabel());
  }

  glPushName(getId() + Inside);
  glLineStipple(1, 0x1C47);
  glEnable(GL_LINE_STIPPLE);
  tglDrawCircle(center, radius - w);
  glDisable(GL_LINE_STIPPLE);
  drawDot(center + TPointD(0.707, 0.707) * (radius - w));
  glPopName();

  if (isSelected(Inside)) {
    drawTooltip(center + TPointD(0.707, 0.707) * (radius - w), getLabel());
  }
}

//---------------------------------------------------------------------------

void RainbowWidthFxGadget::leftButtonDown(const TPointD &pos,
                                          const TMouseEvent &) {
  m_handle = (HANDLE)m_selected;
}

//---------------------------------------------------------------------------

void RainbowWidthFxGadget::leftButtonDrag(const TPointD &pos,
                                          const TMouseEvent &) {
  if (m_handle == None) return;

  double radius = getValue(m_radius);
  double wpos   = norm(pos - getValue(m_center));
  double width  = (m_handle == Outside) ? wpos - radius : radius - wpos;

  double scale = (width * 41.3) / (radius * 1.0);

  double min, max, step;
  m_widthScale->getValueRange(min, max, step);

  setValue(m_widthScale, std::min(max, std::max(min, scale)));
}

//=============================================================================

class EllipseFxGadget final : public FxGadget {
  TDoubleParamP m_radius;
  TDoubleParamP m_xParam, m_yParam;
  TDoubleParamP m_aspect_ratio;
  TDoubleParamP m_angle;
  TDoubleParamP m_twist;

  TPointD m_pos;

  bool m_isSpin;

  enum HANDLE { Radius = 0, Center, AngleAndAR, Twist, None } m_handle = None;

public:
  EllipseFxGadget(FxGadgetController *controller, const TDoubleParamP &radius,
                  const TPointParamP &center, const TDoubleParamP &aspect_ratio,
                  const TDoubleParamP &angle,
                  const TDoubleParamP &twist = TDoubleParamP())
      : FxGadget(controller, 4)
      , m_radius(radius)
      , m_xParam(center->getX())
      , m_yParam(center->getY())
      , m_aspect_ratio(aspect_ratio)
      , m_angle(angle)
      , m_twist(twist) {
    addParam(radius);
    addParam(m_xParam);
    addParam(m_yParam);
    addParam(m_aspect_ratio);
    addParam(m_angle);

    m_isSpin = !m_twist;
  }

  TPointD getCenter() const;

  void draw(bool picking) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp() override;
};

//---------------------------------------------------------------------------

TPointD EllipseFxGadget::getCenter() const {
  return TPointD(getValue(m_xParam), getValue(m_yParam));
}

//---------------------------------------------------------------------------

void EllipseFxGadget::draw(bool picking) {
  int idBase = getId();

  auto setColorById = [&](int id) {
    if (isSelected(id))
      glColor3dv(m_selectedColor);
    else
      glColor3d(0, 0, 1);
  };

  setPixelSize();
  glPushMatrix();

  TPointD center      = getCenter();
  double aspect_ratio = getValue(m_aspect_ratio);
  double angle        = getValue(m_angle);
  TRectD cameraRect   = m_controller->getCameraRect();
  double pivot        = getPixelSize() * cameraRect.getLy() / 2.0;

  // draw spin lines field
  if (isSelected() && !isSelected(None)) {
    double lineInterval = getPixelSize() * 50;
    TRectD geom         = m_controller->getGeometry();
    if (m_isSpin)
      drawSpinField(geom, center, lineInterval, aspect_ratio, angle);
    else {  // radial case
      double twist = getValue(m_twist) * M_PI_180;
      drawRadialField(geom, center, lineInterval, aspect_ratio, angle, twist,
                      pivot);
    }
  }

  double unit = getPixelSize();
  glTranslated(center.x, center.y, 0);

  //--- radius ---
  setColorById(Radius);
  glPushName(idBase + Radius);
  double radius = getValue(m_radius);

  double scale[2] = {1.0, 1.0};
  if (!areAlmostEqual(aspect_ratio, 1.0)) {
    scale[0] = 2.0 * aspect_ratio / (aspect_ratio + 1.0);
    scale[1] = scale[0] / aspect_ratio;
  }
  glPushMatrix();

  glRotated(angle, 0., 0., 1.);
  glScaled(scale[0], scale[1], 1.0);

  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  tglDrawCircle(TPointD(), radius);
  glDisable(GL_LINE_STIPPLE);

  glPopMatrix();

  QTransform transform = QTransform().rotate(angle).scale(scale[0], scale[1]);
  QPointF radiusHandlePos = transform.map(QPointF(0.0, radius));
  drawDot(TPointD(radiusHandlePos.x(), radiusHandlePos.y()));
  glPopName();

  if (isSelected(Radius)) {
    QPointF namePos = transform.map(QPointF(0.707, 0.707) * radius);
    drawTooltip(TPointD(namePos.x(), namePos.y()), getLabel());
  }

  //--- twist ---
  if (m_twist) {
    setColorById(Twist);
    glPushName(idBase + Twist);
    glPushMatrix();

    glRotated(angle, 0., 0., 1.);
    glScaled(scale[0], scale[1], 1.0);

    glLineStipple(1, 0x0F0F);
    glEnable(GL_LINE_STIPPLE);
    tglDrawCircle(TPointD(), pivot);
    glDisable(GL_LINE_STIPPLE);

    glPopMatrix();
    glPopName();
    if (isSelected(Twist)) {
      QPointF namePos = transform.map(QPointF(0.707, 0.707) * pivot);
      drawTooltip(TPointD(namePos.x(), namePos.y()), "Twist");
    }
  }
  //--- center ---
  setColorById(Center);
  glPushName(idBase + Center);
  double d = unit * 8;
  tglDrawCircle(TPointD(), d);

  if (radius > d) {
    glBegin(GL_LINES);
    glVertex2d(-d, 0);
    glVertex2d(d, 0);
    glVertex2d(0, -d);
    glVertex2d(0, d);
    glEnd();
  }

  glPopName();
  if (isSelected(Center)) {
    drawTooltip(TPointD(7, 3) * unit, "Center");
  }

  //---- AR and rotate
  double handleLength = unit * 100;
  radius              = std::max(radius, unit * 10);
  setColorById(AngleAndAR);
  QPointF qHandleRoot = transform.map(QPointF(radius, 0.0));
  glPushMatrix();
  glPushName(idBase + AngleAndAR);
  glTranslated(qHandleRoot.x(), qHandleRoot.y(), 0.);
  glRotated(angle, 0., 0., 1.);
  glBegin(GL_LINES);
  glVertex2d(0., 0.);
  glVertex2d(handleLength, 0.);
  glEnd();
  drawDot(TPointD(handleLength, 0.));

  glPopMatrix();
  glPopName();

  if (isSelected(AngleAndAR)) {
    double angle_radian = angle * M_PI_180;
    TPointD namePos(qHandleRoot.x() + std::cos(angle_radian) * handleLength,
                    qHandleRoot.y() + std::sin(angle_radian) * handleLength);
    drawTooltip(namePos, "Angle and Aspect");
  }

  glPopMatrix();  // cancel translation to center
}

//---------------------------------------------------------------------------

void EllipseFxGadget::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_handle = (HANDLE)m_selected;
  m_pos    = pos;
}

//---------------------------------------------------------------------------

void EllipseFxGadget::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_handle == None) return;
  if (m_handle == Radius) {
    double aspect_ratio = getValue(m_aspect_ratio);
    double angle        = getValue(m_angle);
    double scale[2]     = {1.0, 1.0};
    if (!areAlmostEqual(aspect_ratio, 1.0)) {
      scale[0] = 2.0 * aspect_ratio / (aspect_ratio + 1.0);
      scale[1] = scale[0] / aspect_ratio;
    }
    TPointD center       = getCenter();
    QTransform transform = QTransform()
                               .translate(center.x, center.y)
                               .rotate(angle)
                               .scale(scale[0], scale[1])
                               .inverted();
    QPointF transformedP = transform.map(QPointF(pos.x, pos.y));
    setValue(m_radius, QVector2D(transformedP).length());
  } else if (m_handle == Center) {
    setValue(m_xParam, pos.x);
    setValue(m_yParam, pos.y);
  } else if (m_handle == AngleAndAR) {
    // âÒì]Ç∆êLèkÇ…ï™ÇØÇÈ
    TPointD center = getCenter();
    TPointD old_v  = m_pos - center;
    TPointD new_v  = pos - center;
    if (old_v == TPointD() || new_v == TPointD()) return;

    // AR
    if (!e.isShiftPressed()) {  // lock AR when shift is pressed
      double aspect_ratio   = getValue(m_aspect_ratio);
      double pre_axisLength = 2.0 * aspect_ratio / (aspect_ratio + 1.0);
      double ratio          = norm(new_v) / norm(old_v);
      if (ratio == 0.) return;
      double new_axisLength = pre_axisLength * ratio;
      double new_ar         = new_axisLength / (2.0 - new_axisLength);
      if (new_ar < 0.1)
        new_ar = 0.1;
      else if (new_ar > 10.0)
        new_ar = 10.0;
      setValue(m_aspect_ratio, new_ar);
    }

    // angle
    if (!e.isCtrlPressed()) {  // lock angle when ctrl is pressed
      double angle = getValue(m_angle);
      double d_angle =
          std::atan2(new_v.y, new_v.x) - std::atan2(old_v.y, old_v.x);
      double new_angle = angle + d_angle * M_180_PI;
      if (new_angle < -180.0)
        new_angle += 360.0;
      else if (new_angle > 180.0)
        new_angle -= 360.0;
      setValue(m_angle, new_angle);
    }

    m_pos = pos;
  } else if (m_handle == Twist) {
    TPointD center = getCenter();
    TPointD old_v  = m_pos - center;
    TPointD new_v  = pos - center;
    if (old_v == TPointD() || new_v == TPointD()) return;
    // angle
    double twist = getValue(m_twist);
    double d_twist =
        std::atan2(new_v.y, new_v.x) - std::atan2(old_v.y, old_v.x);
    double new_twist = twist + d_twist * M_180_PI;
    if (new_twist < -180.0)
      new_twist = -180.0;
    else if (new_twist > 180.0)
      new_twist = 180.0;
    setValue(m_twist, new_twist);

    m_pos = pos;
  }
}

//---------------------------------------------------------------------------

void EllipseFxGadget::leftButtonUp() { m_handle = None; }

//=============================================================================

class VerticalPosFxGadget final : public FxGadget {
  TPointD m_pos;
  TDoubleParamP m_yParam;
  TIntEnumParamP m_mode;

public:
  VerticalPosFxGadget(FxGadgetController *controller,
                      const TDoubleParamP &param, const TIntEnumParamP &mode)
      : FxGadget(controller), m_yParam(param), m_mode(mode) {
    addParam(m_yParam);
  }

  void draw(bool picking) override;

  bool isVisible();
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
};

//---------------------------------------------------------------------------
// Dirty resolution to hide gadget when selecting unrelated modes

bool VerticalPosFxGadget::isVisible() {
  if (!m_mode) return true;
  // condition for Distance Level parameter of Iwa_FloorBumpFx
  if (m_yParam->getName() == "distanceLevel" && m_mode->getValue() != 5)
    return false;
  return true;
}

//---------------------------------------------------------------------------

void VerticalPosFxGadget::draw(bool picking) {
  if (!isVisible()) return;
  setPixelSize();
  if (isSelected())
    glColor3dv(m_selectedColor);
  else
    glColor3d(0, 0, 1);
  glPushName(getId());
  double vPos = getValue(m_yParam);
  double unit = getPixelSize();
  glPushMatrix();
  glTranslated(0, vPos, 0);
  double r = unit * 3;
  double d = unit * 300;
  glBegin(GL_LINES);
  glVertex2d(0, r);
  glVertex2d(0, -r);
  glVertex2d(-d, 0);
  glVertex2d(d, 0);
  glEnd();
  drawTooltip(TPointD(7, 7) * unit, getLabel());

  glPopMatrix();
  glPopName();
}

//---------------------------------------------------------------------------

void VerticalPosFxGadget::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &) {}

//---------------------------------------------------------------------------

void VerticalPosFxGadget::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &) {
  if (m_yParam) setValue(m_yParam, pos.y);
}

//=============================================================================

class ParallelogramFxGadget final : public FxGadget {
  TPointParamP m_pcenter, m_phoriz, m_pvert;
  VectorFxGadget *m_hVecGadget, *m_vVecGadget;
  TPointD m_clickedPos;
  TPointParamP m_pcurve;

  enum HANDLE { Body = 0, CurveAnchor, Rotation, None } m_handle = None;

public:
  ParallelogramFxGadget(FxGadgetController *controller, const TPointParamP &pc,
                        const TPointParamP &ph, const TPointParamP &pv)
      : FxGadget(controller)
      , m_pcenter(pc)
      , m_phoriz(ph)
      , m_pvert(pv)
      , m_hVecGadget(new VectorFxGadget(controller, pc, ph))
      , m_vVecGadget(new VectorFxGadget(controller, pc, pv)) {
    addParam(pc->getX());
    addParam(pc->getY());
    addParam(ph->getX());
    addParam(ph->getY());
    addParam(pv->getX());
    addParam(pv->getY());
  }

  ParallelogramFxGadget(FxGadgetController *controller, const TPointParamP &pc,
                        const TPointParamP &ph, const TPointParamP &pv,
                        const TPointParamP &pcurve)
      : FxGadget(controller, 3)
      , m_pcenter(pc)
      , m_phoriz(ph)
      , m_pvert(pv)
      , m_pcurve(pcurve)
      , m_hVecGadget(new VectorFxGadget(controller, pc, ph))
      , m_vVecGadget(new VectorFxGadget(controller, pc, pv)) {
    addParam(pc->getX());
    addParam(pc->getY());
    addParam(ph->getX());
    addParam(ph->getY());
    addParam(pv->getX());
    addParam(pv->getY());
  }

  ~ParallelogramFxGadget() {
    delete m_hVecGadget;
    delete m_vVecGadget;
  }

  void draw(bool picking) override {
    auto setColorById = [&](int id) {
      if (isSelected(id))
        glColor3dv(m_selectedColor);
      else
        glColor3d(0, 0, 1);
    };

    setPixelSize();
    setColorById(Body);
    glPushName(getId() + Body);

    double pixelSize = getPixelSize();
    double c         = pixelSize * 4;
    TPointD pc       = getValue(m_pcenter);
    TPointD ph       = getValue(m_phoriz);
    TPointD pv       = getValue(m_pvert);

    TPointD vec_h  = ph - pc;
    TPointD vec_v  = pv - pc;
    TPointD po     = ph + vec_v;
    TPointD unit_h = vec_h * (1.0 / sqrt(norm2(vec_h)));
    TPointD unit_v = vec_v * (1.0 / sqrt(norm2(vec_v)));

    glLineStipple(1, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
    tglDrawSegment(ph + unit_v * c, po);
    tglDrawSegment(pv + unit_h * c, po);
    glDisable(GL_LINE_STIPPLE);
    glPopName();

    if (m_pcurve.getPointer()) {
      TPointD pcurve = getValue(m_pcurve);
      TPointD ppivot = pc + (pcurve.x + 0.5) * vec_h + (pcurve.y + 0.5) * vec_v;

      setColorById(Body);
      glPushName(getId() + Body);
      glEnable(GL_LINE_STIPPLE);
      if (pcurve == TPointD()) {
        tglDrawSegment((pc + ph) * 0.5, vec_v + (pc + ph) * 0.5);
        tglDrawSegment((pc + pv) * 0.5, vec_h + (pc + pv) * 0.5);
      } else {
        TPointD p[2][2] = {{pc + vec_h * 0.5, pc + vec_h * 0.5 + vec_v},
                           {pc + vec_v * 0.5, pc + vec_v * 0.5 + vec_h}};
        for (int k = 0; k < 2; k++) {  // ÇΩÇƒÇÊÇ±
          glBegin(GL_LINE_STRIP);
          for (int i = 0; i <= 10; i++) {  // ï™äÑ
            double t = (double)i * 0.1;
            tglVertex((1.0 - t) * (1.0 - t) * p[k][0] +
                      2.0 * (1.0 - t) * t * ppivot + t * t * p[k][1]);
          }
          glEnd();
        }
      }
      glDisable(GL_LINE_STIPPLE);
      glPopName();

      setColorById(CurveAnchor);
      glPushName(getId() + CurveAnchor);
      glPushMatrix();
      glTranslated(ppivot.x, ppivot.y, 0);
      double r = pixelSize * 3;
      tglDrawRect(-r, -r, r, r);
      glPopMatrix();
      glPopName();
    }

    setColorById(Rotation);
    glPushName(getId() + Rotation);
    double a = pixelSize * 10, b = pixelSize * 3;
    TPointD diagonal = normalize(po - pc);
    TPointD v        = rotate90(diagonal);
    tglDrawSegment(po + v * a, po - v * a);
    tglDrawSegment(po + diagonal * b + v * a, po + diagonal * b - v * a);
    glPopName();

    m_hVecGadget->draw(picking);
    m_vVecGadget->draw(picking);
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    m_handle = (HANDLE)m_selected;
    if (m_handle == None) return;
    m_clickedPos = pos;
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    if (m_handle == None) return;
    if (m_handle == Body) {
      TPointD d = pos - m_clickedPos;
      setValue(m_pcenter, getValue(m_pcenter) + d);
      setValue(m_phoriz, getValue(m_phoriz) + d);
      setValue(m_pvert, getValue(m_pvert) + d);
    } else if (m_handle == CurveAnchor && m_pcurve.getPointer()) {
      TPointD pc    = getValue(m_pcenter);
      TPointD ph    = getValue(m_phoriz);
      TPointD pv    = getValue(m_pvert);
      TPointD vec_h = ph - pc;
      TPointD vec_v = pv - pc;
      TAffine aff(vec_h.x, vec_v.x, pc.x, vec_h.y, vec_v.y, pc.y);
      TPointD p_conv = aff.inv() * pos;
      if (p_conv.x < 0.0)
        p_conv.x = 0.0;
      else if (p_conv.x > 1.0)
        p_conv.x = 1.0;
      if (p_conv.y < 0.0)
        p_conv.y = 0.0;
      else if (p_conv.y > 1.0)
        p_conv.y = 1.0;
      setValue(m_pcurve, p_conv - TPointD(0.5, 0.5));
    } else if (m_handle == Rotation) {
      TPointD ph     = getValue(m_phoriz);
      TPointD pv     = getValue(m_pvert);
      TPointD pivot  = (ph + pv) * 0.5;
      TPointD before = m_clickedPos - pivot;
      TPointD after  = pos - pivot;
      double angle =
          std::atan2(after.y, after.x) - std::atan2(before.y, before.x);
      TAffine aff = TTranslation(pivot) * TRotation(angle * M_180_PI) *
                    TTranslation(-pivot);

      setValue(m_pcenter, aff * getValue(m_pcenter));
      setValue(m_phoriz, aff * getValue(m_phoriz));
      setValue(m_pvert, aff * getValue(m_pvert));
    }
    m_clickedPos = pos;
  }
  void leftButtonUp() override { m_handle = None; }
};

//*************************************************************************************
//    FxGadgetController  implementation
//*************************************************************************************

FxGadgetController::FxGadgetController(TTool *tool)
    : m_tool(tool)
    , m_fxHandle(tool->getApplication()->getCurrentFx())
    , m_idBase(0)
    , m_nextId(0)
    , m_selectedGadget(0)
    , m_editingNonZeraryFx(false) {
  m_idBase = m_nextId = 5000;
  connect(m_fxHandle, SIGNAL(fxSwitched()), this, SLOT(onFxSwitched()));
  connect(tool->getApplication()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
          this, SLOT(onFxSwitched()));
  onFxSwitched();
}

//---------------------------------------------------------------------------

FxGadgetController::~FxGadgetController() { clearGadgets(); }

//---------------------------------------------------------------------------

void FxGadgetController::clearGadgets() {
  std::vector<FxGadget *>::iterator it;
  for (it = m_gadgets.begin(); it != m_gadgets.end(); ++it) delete (*it);
  m_gadgets.clear();
  m_idTable.clear();
  m_selectedGadget = 0;
  m_nextId         = m_idBase;
}

//---------------------------------------------------------------------------

void FxGadgetController::assignId(FxGadget *gadget) {
  gadget->setId(m_nextId);
  for (int g = 0; g < gadget->getHandleCount(); g++) {
    m_idTable[m_nextId] = gadget;
    ++m_nextId;
  }
}

//---------------------------------------------------------------------------

void FxGadgetController::addGadget(FxGadget *gadget) {
  m_gadgets.push_back(gadget);
}

//---------------------------------------------------------------------------

void FxGadgetController::draw(bool picking) {
  glPushMatrix();
  tglMultMatrix(getMatrix());
  std::vector<FxGadget *>::iterator it;
  for (it = m_gadgets.begin(); it != m_gadgets.end(); ++it)
    (*it)->draw(picking);
  glPopMatrix();
}

//---------------------------------------------------------------------------

void FxGadgetController::selectById(unsigned int id) {
  std::map<GLuint, FxGadget *>::iterator it = m_idTable.find(id);
  FxGadget *selectedGadget = it != m_idTable.end() ? it->second : nullptr;
  if (selectedGadget != m_selectedGadget) {
    if (m_selectedGadget) m_selectedGadget->select(-1);
    m_selectedGadget = selectedGadget;
  }
  if (!m_selectedGadget) return;
  int handleId = id - m_selectedGadget->getId();
  if (!m_selectedGadget->isSelected(handleId))
    m_selectedGadget->select(handleId);
}

//---------------------------------------------------------------------------

FxGadget *FxGadgetController::allocateGadget(const TParamUIConcept &uiConcept) {
  FxGadget *gadget = 0;

  switch (uiConcept.m_type) {
  case TParamUIConcept::RADIUS: {
    assert(uiConcept.m_params.size() >= 1 && uiConcept.m_params.size() <= 2);

    TPointParamP center((uiConcept.m_params.size() >= 2)
                            ? (TPointParamP)uiConcept.m_params[1]
                            : TPointParamP());
    gadget = new RadiusFxGadget(this, uiConcept.m_params[0], center);

    break;
  }

  case TParamUIConcept::WIDTH: {
    assert(uiConcept.m_params.size() >= 1 && uiConcept.m_params.size() <= 2);

    TDoubleParamP angle((uiConcept.m_params.size() >= 2)
                            ? (TDoubleParamP)uiConcept.m_params[1]
                            : TDoubleParamP());
    gadget = new DistanceFxGadget(this, uiConcept.m_params[0], angle);
    break;
  }

  case TParamUIConcept::ANGLE: {
    assert(uiConcept.m_params.size() == 1);
    gadget = new AngleFxGadget(this, uiConcept.m_params[0], TPointD());
    break;
  }

  case TParamUIConcept::ANGLE_2: {
    assert(uiConcept.m_params.size() == 3);
    gadget =
        new AngleRangeFxGadget(this, uiConcept.m_params[0],
                               uiConcept.m_params[1], uiConcept.m_params[2]);
    break;
  }

  case TParamUIConcept::POINT: {
    assert(uiConcept.m_params.size() == 1);
    gadget = new PointFxGadget(this, uiConcept.m_params[0]);
    break;
  }

  case TParamUIConcept::POINT_2: {
    assert(uiConcept.m_params.size() == 2);
    gadget =
        new PointFxGadget(this, uiConcept.m_params[0], uiConcept.m_params[1]);
    break;
  }

  case TParamUIConcept::VECTOR: {
    assert(uiConcept.m_params.size() == 2);
    gadget =
        new VectorFxGadget(this, uiConcept.m_params[0], uiConcept.m_params[1]);
    break;
  }

  case TParamUIConcept::POLAR: {
    assert(uiConcept.m_params.size() == 2);
    gadget = new PolarFxGadget(this, TPointD(), uiConcept.m_params[0],
                               uiConcept.m_params[1]);
    break;
  }

  case TParamUIConcept::SIZE: {
    assert(uiConcept.m_params.size() >= 1 && uiConcept.m_params.size() <= 2);

    TDoubleParamP y((uiConcept.m_params.size() >= 2)
                        ? (TDoubleParamP)uiConcept.m_params[1]
                        : TDoubleParamP());
    gadget = new SizeFxGadget(this, uiConcept.m_params[0], y);
    break;
  }

  case TParamUIConcept::QUAD: {
    assert(uiConcept.m_params.size() == 4);
    gadget =
        new QuadFxGadget(this, uiConcept.m_params[0], uiConcept.m_params[1],
                         uiConcept.m_params[2], uiConcept.m_params[3]);
    break;
  }

  case TParamUIConcept::RECT: {
    assert(uiConcept.m_params.size() >= 2 && uiConcept.m_params.size() <= 3);

    TPointParamP center((uiConcept.m_params.size() >= 3)
                            ? (TPointParamP)uiConcept.m_params[2]
                            : TPointParamP());
    gadget = new RectFxGadget(this, uiConcept.m_params[0],
                              uiConcept.m_params[1], center);

    break;
  }

  case TParamUIConcept::DIAMOND: {
    assert(uiConcept.m_params.size() == 1);
    gadget = new DiamondFxGadget(this, uiConcept.m_params[0]);
    break;
  }

  case TParamUIConcept::LINEAR_RANGE: {
    assert(uiConcept.m_params.size() == 2);
    gadget = new LinearRangeFxGadget(this, uiConcept.m_params[0],
                                     uiConcept.m_params[1]);
    break;
  }

  case TParamUIConcept::COMPASS: {
    assert(uiConcept.m_params.size() == 1 || uiConcept.m_params.size() == 4);
    if (uiConcept.m_params.size() == 1)
      gadget = new CompassFxGadget(this, uiConcept.m_params[0]);
    else
      gadget = new CompassFxGadget(this, uiConcept.m_params[0], false,
                                   uiConcept.m_params[1], uiConcept.m_params[2],
                                   uiConcept.m_params[3]);
    break;
  }

  case TParamUIConcept::COMPASS_SPIN: {
    assert(uiConcept.m_params.size() == 1 || uiConcept.m_params.size() == 3);

    if (uiConcept.m_params.size() == 3)
      gadget =
          new CompassFxGadget(this, uiConcept.m_params[0], true,
                              uiConcept.m_params[1], uiConcept.m_params[2]);
    else
      gadget = new CompassFxGadget(this, uiConcept.m_params[0], true);

    break;
  }

  case TParamUIConcept::RAINBOW_WIDTH: {
    assert(uiConcept.m_params.size() == 3);
    gadget =
        new RainbowWidthFxGadget(this, uiConcept.m_params[0],
                                 uiConcept.m_params[1], uiConcept.m_params[2]);
    break;
  }

  case TParamUIConcept::ELLIPSE: {
    assert(uiConcept.m_params.size() == 4 || uiConcept.m_params.size() == 5);
    // radial blur has one more parameter (twist).
    if (uiConcept.m_params.size() == 5)
      gadget = new EllipseFxGadget(
          this, uiConcept.m_params[0], uiConcept.m_params[1],
          uiConcept.m_params[2], uiConcept.m_params[3], uiConcept.m_params[4]);
    // spin blur case
    else
      gadget = new EllipseFxGadget(this, uiConcept.m_params[0],
                                   uiConcept.m_params[1], uiConcept.m_params[2],
                                   uiConcept.m_params[3]);
    break;
  }

  case TParamUIConcept::VERTICAL_POS: {
    assert(uiConcept.m_params.size() >= 1 && uiConcept.m_params.size() <= 2);
    TIntEnumParamP mode((uiConcept.m_params.size() >= 2)
                            ? (TIntEnumParamP)uiConcept.m_params[1]
                            : TIntEnumParamP());
    gadget = new VerticalPosFxGadget(this, uiConcept.m_params[0], mode);
    break;
  }

  case TParamUIConcept::PARALLELOGRAM: {
    assert(uiConcept.m_params.size() == 3 || uiConcept.m_params.size() == 4);
    if (uiConcept.m_params.size() == 3) {
      gadget = new ParallelogramFxGadget(this, uiConcept.m_params[0],
                                         uiConcept.m_params[1],
                                         uiConcept.m_params[2]);
    } else
      gadget = new ParallelogramFxGadget(
          this, uiConcept.m_params[0], uiConcept.m_params[1],
          uiConcept.m_params[2], uiConcept.m_params[3]);
    break;
  }

  default:
    break;
  }

  if (gadget) gadget->setLabel(uiConcept.m_label);

  return gadget;
}

//---------------------------------------------------------------------------

void FxGadgetController::onFxSwitched() {
  clearGadgets();
  bool enabled = false;
  TFx *fx      = m_fxHandle ? m_fxHandle->getFx() : 0;
  if (fx) {
    int referenceColumnIndex = fx->getReferenceColumnIndex();
    if (referenceColumnIndex == -1) {
      TObjectHandle *oh = m_tool->getApplication()->getCurrentObject();
      if (!oh->getObjectId().isCamera()) {
        TXsheet *xsh = m_tool->getXsheet();
        oh->setObjectId(TStageObjectId::CameraId(xsh->getCameraColumnIndex()));
      }
      enabled = true;
    } else if (referenceColumnIndex == m_tool->getColumnIndex())
      enabled = true;
  }
  if (fx && enabled) {
    m_editingNonZeraryFx = true;
    TZeraryColumnFx *zfx = 0;
    if ((zfx = dynamic_cast<TZeraryColumnFx *>(fx)) ||
        dynamic_cast<TLevelColumnFx *>(fx))
    // WARNING! quick patch for huge bug:  I added the || with TLevelColumnFx;
    // before, the levels were considered as nonZeraryFx and the edit tool
    // gadget was not displayed! Vinz
    {
      if (zfx) fx = zfx->getZeraryFx();
      m_editingNonZeraryFx = false;
    }

    // Parse the UI Concepts returned by the fx
    TParamUIConcept *uiConcepts = 0;
    int i, count;

    fx->getParamUIs(uiConcepts, count);

    for (i = 0; i < count; ++i) {
      FxGadget *gadget = allocateGadget(uiConcepts[i]);
      if (gadget) addGadget(gadget);
    }

    delete[] uiConcepts;
  } else
    m_editingNonZeraryFx = false;

  m_tool->invalidate();
}

//---------------------------------------------------------------------------

EditToolGadgets::DragTool *FxGadgetController::createDragTool(int gadgetId) {
  selectById(gadgetId);
  if (m_selectedGadget)
    return new GadgetDragTool(this, m_selectedGadget);
  else
    return 0;
}

//---------------------------------------------------------------------------

TAffine FxGadgetController::getMatrix() {
  TFx *fx = m_fxHandle ? m_fxHandle->getFx() : 0;
  if (fx) {
    int referenceColumnIndex = fx->getReferenceColumnIndex();
    if (referenceColumnIndex == -1)
      return m_tool->getMatrix().inv();
    else if (referenceColumnIndex != m_tool->getColumnIndex())
      return m_tool->getMatrix().inv() *
             m_tool->getColumnMatrix(referenceColumnIndex, -1);
  }
  return m_tool->getMatrix().inv() * m_tool->getCurrentColumnMatrix();
}

//---------------------------------------------------------------------------

int FxGadgetController::getCurrentFrame() const { return m_tool->getFrame(); }

//---------------------------------------------------------------------------

void FxGadgetController::invalidateViewer() { m_tool->invalidate(); }

//---------------------------------------------------------------------------

int FxGadgetController::getDevPixRatio() {
  return getDevicePixelRatio(m_tool->getViewer()->viewerWidget());
}

//---------------------------------------------------------------------------

TRectD FxGadgetController::getGeometry() {
  return (m_tool->getViewer()) ? m_tool->getViewer()->getGeometry() : TRectD();
}

//---------------------------------------------------------------------------

TRectD FxGadgetController::getCameraRect() {
  return (m_tool->getViewer()) ? m_tool->getViewer()->getCameraRect()
                               : TRectD();
}
