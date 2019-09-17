

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

#include <QApplication>
#include <QDesktopWidget>

using namespace EditToolGadgets;

GLdouble FxGadget::m_selectedColor[3] = {0.2, 0.8, 0.1};

namespace {
int getDevPixRatio() {
  static int devPixRatio = QApplication::desktop()->devicePixelRatio();
  return devPixRatio;
}
}

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

public:
  GadgetDragTool(FxGadgetController *controller, FxGadget *gadget)
      : m_controller(controller), m_gadget(gadget) {}

  TAffine getMatrix() const { return m_controller->getMatrix().inv(); }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_gadget->createUndo();
    m_gadget->leftButtonDown(getMatrix() * pos, e);
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    m_gadget->leftButtonDrag(getMatrix() * pos, e);
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    m_gadget->leftButtonUp(getMatrix() * pos, e);
    m_gadget->commitUndo();
  }
};

//*************************************************************************************
//    FxGadget  implementation
//*************************************************************************************

FxGadget::FxGadget(FxGadgetController *controller)
    : m_id(-1)
    , m_selected(false)
    , m_controller(controller)
    , m_pixelSize(1)
    , m_undo(0)
    , m_scaleFactor(1) {
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
  setPixelSize(sqrt(tglGetPixelSize2()) * getDevPixRatio());
}

//---------------------------------------------------------------------------

void FxGadget::drawTooltip(const TPointD &tooltipPos,
                           std::string tooltipPosText) {
  double unit = sqrt(tglGetPixelSize2()) * getDevPixRatio();
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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
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

//---------------------------------------------------------------------------

void PointFxGadget::leftButtonUp(const TPointD &pos, const TMouseEvent &) {}

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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
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

//---------------------------------------------------------------------------

void RadiusFxGadget::leftButtonUp(const TPointD &pos, const TMouseEvent &) {}

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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
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

//---------------------------------------------------------------------------

void DistanceFxGadget::leftButtonUp(const TPointD &pos, const TMouseEvent &) {}

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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
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
  double pixelSize = sqrt(tglGetPixelSize2()) * getDevPixRatio();
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

//---------------------------------------------------------------------------

void AngleFxGadget::leftButtonUp(const TPointD &pos, const TMouseEvent &) {}

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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
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
  double sz        = fabs(pos.x) + fabs(pos.y);
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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
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
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
};

//=============================================================================

class VectorFxGadget final : public FxGadget {
  TPointParamP m_pa, m_pb;
  int m_selected;

public:
  VectorFxGadget(FxGadgetController *controller, const TPointParamP &pa,
                 const TPointParamP &pb)
      : FxGadget(controller), m_pa(pa), m_pb(pb), m_selected(0) {
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
    glPushName(getId());
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
      drawDot(pa);
      drawDot(pb);
    } else
      drawDot(pa);
    glPopName();
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
};

//=============================================================================

class QuadFxGadget final : public FxGadget {
  TPointParamP m_pa, m_pb, m_pc, m_pd;

public:
  QuadFxGadget(FxGadgetController *controller, const TPointParamP &pa,
               const TPointParamP &pb, const TPointParamP &pc,
               const TPointParamP &pd)
      : FxGadget(controller), m_pa(pa), m_pb(pb), m_pc(pc), m_pd(pd) {
    addParam(pa->getX());
    addParam(pa->getY());
    addParam(pb->getX());
    addParam(pb->getY());
    addParam(pc->getX());
    addParam(pc->getY());
    addParam(pd->getX());
    addParam(pd->getY());
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
    TPointD pc       = getValue(m_pc);
    TPointD pd       = getValue(m_pd);
    glLineStipple(1, 0xCCCC);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINE_STRIP);
    tglVertex(pa);
    tglVertex(pb);
    tglVertex(pc);
    tglVertex(pd);
    tglVertex(pa);
    glEnd();
    glDisable(GL_LINE_STIPPLE);
    // glPopName();
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {}
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {}
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
  m_idTable[m_nextId] = gadget;
  ++m_nextId;
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
  std::map<GLuint, FxGadget *>::iterator it;
  it                       = m_idTable.find(id);
  FxGadget *selectedGadget = it != m_idTable.end() ? it->second : 0;
  if (selectedGadget != m_selectedGadget) {
    if (m_selectedGadget) m_selectedGadget->select(false);
    m_selectedGadget = selectedGadget;
    if (m_selectedGadget) m_selectedGadget->select(true);
  }
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
        oh->setObjectId(TStageObjectId::CameraId(0));
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
      if (zfx) fx          = zfx->getZeraryFx();
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
  return m_tool->getMatrix().inv() * m_tool->getCurrentColumnMatrix();
}

//---------------------------------------------------------------------------

int FxGadgetController::getCurrentFrame() const { return m_tool->getFrame(); }

//---------------------------------------------------------------------------

void FxGadgetController::invalidateViewer() { m_tool->invalidate(); }
