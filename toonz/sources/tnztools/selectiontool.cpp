

#include "selectiontool.h"
#include "rasterselectiontool.h"
#include "vectorselectiontool.h"
#include "tcurveutil.h"
#include "tenv.h"
#include "drawutil.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"
#include "toonz/stage2.h"
#include "toonz/tobjecthandle.h"

#include <QKeyEvent>

using namespace ToolUtils;
using namespace DragSelectionTool;

TEnv::StringVar SelectionType("SelectionType", "Rectangular");

//-----------------------------------------------------------------------------

DragSelectionTool::DragTool *createNewMoveSelectionTool(SelectionTool *st) {
  VectorSelectionTool *vst = dynamic_cast<VectorSelectionTool *>(st);
  RasterSelectionTool *rst = dynamic_cast<RasterSelectionTool *>(st);
  if (vst)
    return new DragSelectionTool::VectorMoveSelectionTool(vst);
  else if (rst)
    return new DragSelectionTool::RasterMoveSelectionTool(rst);
  return 0;
}

//-----------------------------------------------------------------------------

DragSelectionTool::DragTool *createNewRotationTool(SelectionTool *st) {
  VectorSelectionTool *vst = dynamic_cast<VectorSelectionTool *>(st);
  RasterSelectionTool *rst = dynamic_cast<RasterSelectionTool *>(st);
  if (vst)
    return new DragSelectionTool::VectorRotationTool(vst);
  else if (rst)
    return new DragSelectionTool::RasterRotationTool(rst);
  return 0;
}

//-----------------------------------------------------------------------------

DragSelectionTool::DragTool *createNewFreeDeformTool(SelectionTool *st) {
  VectorSelectionTool *vst = dynamic_cast<VectorSelectionTool *>(st);
  RasterSelectionTool *rst = dynamic_cast<RasterSelectionTool *>(st);
  if (vst)
    return new DragSelectionTool::VectorFreeDeformTool(vst);
  else if (rst)
    return new DragSelectionTool::RasterFreeDeformTool(rst);
  return 0;
}

//-----------------------------------------------------------------------------

DragSelectionTool::DragTool *createNewScaleTool(SelectionTool *st, int type) {
  VectorSelectionTool *vst = dynamic_cast<VectorSelectionTool *>(st);
  RasterSelectionTool *rst = dynamic_cast<RasterSelectionTool *>(st);
  if (vst)
    return new DragSelectionTool::VectorScaleTool(vst, type);
  else if (rst)
    return new DragSelectionTool::RasterScaleTool(rst, type);
  return 0;
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

// Return index of point with min x or y
int tminPoint(std::vector<TPointD> points, bool isX) {
  int i;
  int index = 0;
  TPointD p = points[0];
  for (i = 1; i < (int)points.size(); i++) {
    TPointD nextP = points[i];
    if (isX && p.x < nextP.x || !isX && p.y < nextP.y) continue;
    index = i;
  }
  return index;
}

//-----------------------------------------------------------------------------

int tminPoint(TPointD p0, TPointD p1, bool isX) {
  std::vector<TPointD> v;
  v.push_back(p0);
  v.push_back(p1);
  return tminPoint(v, isX);
}

//=============================================================================
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// FourPoints
//-----------------------------------------------------------------------------

FourPoints DragSelectionTool::FourPoints::orderedPoints() const {
  FourPoints newPoints;
  int i;
  std::vector<TPointD> allPoints;
  allPoints.push_back(m_p00);
  allPoints.push_back(m_p01);
  allPoints.push_back(m_p10);
  allPoints.push_back(m_p11);
  int minXindex1 = tminPoint(allPoints, true);
  std::vector<TPointD> points;
  for (i = 0; i < 4; i++)
    if (i != minXindex1) points.push_back(allPoints[i]);
  int minXindex2 = tminPoint(points, true);

  int index = tminPoint(allPoints[minXindex1], points[minXindex2], false);
  TPointD newPoint1 = allPoints[minXindex1];
  TPointD newPoint2 = points[minXindex2];
  if (index == 1) tswap(newPoint1, newPoint2);
  newPoints.setP00(newPoint1);
  newPoints.setP01(newPoint2);

  std::vector<TPointD> points2;
  for (i = 0; i < 3; i++)
    if (i != minXindex2) points2.push_back(points[i]);

  index = tminPoint(points2, false);
  newPoints.setP10(points2[index]);
  newPoints.setP11(points2[(index == 0) ? 1 : 0]);
  return newPoints;
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::FourPoints::getPoint(int index) const {
  if (index == 0)
    return m_p00;
  else if (index == 1)
    return m_p10;
  else if (index == 2)
    return m_p11;
  else if (index == 3)
    return m_p01;
  else if (index == 4)
    return (m_p00 + m_p10) * 0.5;
  else if (index == 5)
    return (m_p10 + m_p11) * 0.5;
  else if (index == 6)
    return (m_p11 + m_p01) * 0.5;
  else if (index == 7)
    return (m_p01 + m_p00) * 0.5;
  return TPointD();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::FourPoints::setPoint(int index, const TPointD &p) {
  if (index == 0)
    m_p00 = p;
  else if (index == 1)
    m_p10 = p;
  else if (index == 2)
    m_p11 = p;
  else if (index == 3)
    m_p01 = p;
}

//-----------------------------------------------------------------------------

FourPoints DragSelectionTool::FourPoints::enlarge(double d) {
  TPointD v   = normalize(getP10() - getP00());
  TPointD p00 = getP00() - d * v;
  TPointD p10 = getP10() + d * v;
  v           = normalize(getP11() - getP10());
  p10         = p10 - d * v;
  TPointD p11 = getP11() + d * v;
  v           = normalize(getP01() - getP11());
  p11         = p11 - d * v;
  TPointD p01 = getP01() + d * v;
  v           = normalize(getP00() - getP01());
  p01         = p01 - d * v;
  p00         = p00 + d * v;
  return FourPoints(p00, p01, p10, p11);
}

//-----------------------------------------------------------------------------

bool DragSelectionTool::FourPoints::isEmpty() {
  return ((getP00().x == getP01().x && getP01().x == getP10().x &&
           getP10().x == getP11().x) ||
          (getP00().y == getP01().y && getP01().y == getP10().y &&
           getP10().y == getP11().y));
}

//-----------------------------------------------------------------------------

void DragSelectionTool::FourPoints::empty() {
  m_p00 = TPointD();
  m_p01 = TPointD();
  m_p10 = TPointD();
  m_p11 = TPointD();
}

//-----------------------------------------------------------------------------

bool DragSelectionTool::FourPoints::contains(TPointD p) {
  double maxDistance =
      std::max(tdistance2(getP00(), getP11()), tdistance2(getP10(), getP01()));
  TPointD outP = p + maxDistance * TPointD(1, 1);
  TSegment segment(outP, p);
  std::vector<DoublePair> d;
  int inters = intersect(TSegment(getP00(), getP10()), segment, d);
  inters += intersect(TSegment(getP10(), getP11()), segment, d);
  inters += intersect(TSegment(getP11(), getP01()), segment, d);
  inters += intersect(TSegment(getP01(), getP00()), segment, d);
  return inters % 2 == 1;
}

//-----------------------------------------------------------------------------

TRectD DragSelectionTool::FourPoints::getBox() const {
  double x0 = std::min({getP00().x, getP10().x, getP01().x, getP11().x});
  double y0 = std::min({getP00().y, getP10().y, getP01().y, getP11().y});
  double x1 = std::max({getP00().x, getP10().x, getP01().x, getP11().x});
  double y1 = std::max({getP00().y, getP10().y, getP01().y, getP11().y});
  return TRectD(TPointD(x0, y0), TPointD(x1, y1));
}

//-----------------------------------------------------------------------------

FourPoints &DragSelectionTool::FourPoints::operator=(const TRectD &r) {
  setP00(r.getP00());
  setP01(r.getP01());
  setP10(r.getP10());
  setP11(r.getP11());
  return *this;
}

//-----------------------------------------------------------------------------

bool DragSelectionTool::FourPoints::operator==(const FourPoints &p) const {
  return getP00() == p.getP00() && getP01() == p.getP01() &&
         getP10() == p.getP10() && getP11() == p.getP11();
}

//-----------------------------------------------------------------------------

FourPoints DragSelectionTool::FourPoints::operator*(const TAffine &aff) const {
  FourPoints p;
  p.setP00(aff * getP00());
  p.setP10(aff * getP10());
  p.setP11(aff * getP11());
  p.setP01(aff * getP01());
  return p;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::drawFourPoints(const FourPoints &rect,
                                       const TPixel32 &color,
                                       unsigned short stipple,
                                       bool doContrast) {
  GLint src, dst;
  bool isEnabled;
  tglColor(color);
  if (doContrast) {
    if (color == TPixel32::Black) tglColor(TPixel32(90, 90, 90));
    isEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC, &src);
    glGetIntegerv(GL_BLEND_DST, &dst);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
  }

  if (stipple != 0xffff) {
    glLineStipple(1, stipple);
    glEnable(GL_LINE_STIPPLE);
  }

  glBegin(GL_LINE_STRIP);
  tglVertex(rect.getP00());
  tglVertex(rect.getP01());
  tglVertex(rect.getP11());
  tglVertex(rect.getP10());
  tglVertex(rect.getP00());
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  if (doContrast) {
    if (!isEnabled) glDisable(GL_BLEND);
    glBlendFunc(src, dst);
  }
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoMoveCenter
//-----------------------------------------------------------------------------

class UndoMoveCenter final : public TUndo {
  SelectionTool *m_tool;
  TAffine m_aff;

public:
  UndoMoveCenter(SelectionTool *tool, const TAffine &aff)
      : m_tool(tool), m_aff(aff) {}
  ~UndoMoveCenter() {}
  void undo() const override {
    m_tool->setCenter(m_aff.inv() * m_tool->getCenter());
    m_tool->invalidate();
  }
  void redo() const override {
    m_tool->setCenter(m_aff * m_tool->getCenter());
    m_tool->invalidate();
  }
  int getSize() const override { return sizeof(*this) + sizeof(*m_tool); }

  QString getHistoryString() override { return QObject::tr("Move Center"); }
};

//=============================================================================
// MoveCenterTool
//-----------------------------------------------------------------------------

class MoveCenterTool final : public DragTool {
  TPointD m_startPos;
  TAffine m_transform;

public:
  MoveCenterTool(SelectionTool *tool)
      : DragTool(tool), m_startPos(), m_transform() {}
  void translateCenter(TAffine aff) {
    getTool()->setCenter(aff * getTool()->getCenter());
    m_transform *= aff;
    getTool()->invalidate();
  }
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_startPos = pos;
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    TPointD delta      = pos - m_startPos;
    FourPoints bbox    = getTool()->getBBox();
    TPointD bboxCenter = 0.5 * (bbox.getP11() + bbox.getP00());
    double maxDistance2 =
        32 * getTool()->getPixelSize() * getTool()->getPixelSize();
    TAffine aff       = m_transform.inv() * TTranslation(delta);
    TPointD newCenter = aff * getTool()->getCenter();
    if (tdistance2(newCenter, bboxCenter) < maxDistance2)
      translateCenter(TTranslation(bboxCenter - getTool()->getCenter()));
    else
      translateCenter(aff);
  }
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    UndoMoveCenter *undo = new UndoMoveCenter(getTool(), m_transform);
    TUndoManager::manager()->add(undo);
  }
  void draw() override {}
};

//=============================================================================
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// DeformTool
//-----------------------------------------------------------------------------

DragSelectionTool::DeformTool::DeformTool(SelectionTool *tool)
    : DragTool(tool)
    , m_curPos()
    , m_isDragging(false)
    , m_startScaleValue(tool->m_deformValues.m_scaleValue) {}

//-----------------------------------------------------------------------------

int DragSelectionTool::DeformTool::getSymmetricPointIndex(int index) const {
  if (index == 0 || index == 4 || index == 1 || index == 5) return index + 2;
  return index - 2;
}

//-----------------------------------------------------------------------------

int DragSelectionTool::DeformTool::getBeforePointIndex(int index) const {
  if (index < 4) return (index == 0) ? 7 : index + 3;
  return index - 4;
}

//-----------------------------------------------------------------------------

int DragSelectionTool::DeformTool::getNextPointIndex(int index) const {
  if (index < 4) return index + 4;
  return (index == 7) ? 0 : index - 3;
}

//-----------------------------------------------------------------------------

int DragSelectionTool::DeformTool::getBeforeVertexIndex(int index) const {
  if (index < 4) return (index == 0) ? 3 : index - 1;
  return index - 4;
}

//-----------------------------------------------------------------------------

int DragSelectionTool::DeformTool::getNextVertexIndex(int index) const {
  if (index < 4) return (index == 3) ? 0 : index + 1;
  return (index == 7) ? 0 : index - 3;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::DeformTool::leftButtonDown(const TPointD &pos,
                                                   const TMouseEvent &e) {
  m_isDragging = true;
  m_curPos     = pos;
  setStartPos(pos);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::DeformTool::leftButtonUp(const TPointD &pos,
                                                 const TMouseEvent &e) {
  addTransformUndo();
  m_isDragging = false;
}

//=============================================================================
// Rotation
//-----------------------------------------------------------------------------

DragSelectionTool::Rotation::Rotation(DeformTool *deformTool)
    : m_curAng(), m_dstAng(), m_deformTool(deformTool) {}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::Rotation::getStartCenter() const {
  return m_deformTool->getTool()->getCenter();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::Rotation::leftButtonDrag(const TPointD &pos,
                                                 const TMouseEvent &e) {
  SelectionTool *tool  = m_deformTool->getTool();
  TPointD center       = tool->getCenter();
  TPointD curPos       = m_deformTool->getCurPos();
  TPointD delta        = pos - curPos;
  TPointD a            = pos - center;
  TPointD b            = (pos - delta) - center;
  double a2            = norm2(a);
  double b2            = norm2(b);
  const double epsilon = 1e-8;
  double dang          = 0;
  double scale         = 1;
  if (a2 <= epsilon || b2 <= epsilon) return;
  dang = asin(cross(a, b) / sqrt(a2 * b2)) * -M_180_PI;
  if (e.isShiftPressed()) {
    m_dstAng += dang;
    double ang = tfloor((int)(m_dstAng + 22.5), 45);
    dang       = ang - m_curAng;
    m_curAng   = ang;
  } else {
    m_dstAng += dang;
    dang     = m_dstAng - m_curAng;
    m_curAng = m_dstAng;
  }

  tool->m_deformValues.m_rotationAngle =
      tool->m_deformValues.m_rotationAngle + dang;
  m_deformTool->transform(TRotation(center, dang), dang);
  m_deformTool->setCurPos(pos);
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::Rotation::draw() {
  tglDrawSegment(m_deformTool->getCurPos(),
                 m_deformTool->getTool()->getCenter());
}

//=============================================================================
// FreeDeform
//-----------------------------------------------------------------------------

DragSelectionTool::FreeDeform::FreeDeform(DeformTool *deformTool)
    : m_deformTool(deformTool) {}

//-----------------------------------------------------------------------------

void DragSelectionTool::FreeDeform::leftButtonDrag(const TPointD &pos,
                                                   const TMouseEvent &e) {
  SelectionTool *tool = m_deformTool->getTool();
  TPointD delta       = pos - m_deformTool->getCurPos();
  TPointD center      = tool->getCenter();
  int index           = tool->getSelectedPoint();
  FourPoints bbox     = tool->getBBox();
  FourPoints newBbox  = bbox;
  if (index < 4)
    bbox.setPoint(index, bbox.getPoint(index) + delta);
  else {
    int beforeIndex = m_deformTool->getBeforeVertexIndex(index);
    bbox.setPoint(beforeIndex, bbox.getPoint(beforeIndex) + delta);
    bbox.setPoint(index, bbox.getPoint(index) + delta);
    int nextIndex = m_deformTool->getNextVertexIndex(index);
    bbox.setPoint(nextIndex, bbox.getPoint(nextIndex) + delta);
  }
  tool->setBBox(bbox);
  m_deformTool->setCurPos(pos);
  m_deformTool->applyTransform(bbox);
}

//=============================================================================
// MoveSelection
//-----------------------------------------------------------------------------

DragSelectionTool::MoveSelection::MoveSelection(DeformTool *deformTool)
    : m_deformTool(deformTool), m_lastDelta(), m_firstPos() {}

//-----------------------------------------------------------------------------

void DragSelectionTool::MoveSelection::leftButtonDown(const TPointD &pos,
                                                      const TMouseEvent &e) {
  m_deformTool->setCurPos(pos);
  m_firstPos = pos;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::MoveSelection::leftButtonDrag(const TPointD &pos,
                                                      const TMouseEvent &e) {
  TAffine aff;
  TPointD curPos = m_deformTool->getCurPos();
  TPointD delta  = pos - curPos;
  if (e.isShiftPressed()) {
    if (m_lastDelta == TPointD()) {
      TPointD totalDelta = curPos - m_firstPos;
      aff                = TTranslation(totalDelta).inv();
    } else
      aff = TTranslation(m_lastDelta).inv();
    if (fabs((curPos - m_firstPos).x) > fabs((curPos - m_firstPos).y))
      m_lastDelta = TPointD((curPos - m_firstPos).x, 0);
    else
      m_lastDelta = TPointD(0, (curPos - m_firstPos).y);
    aff *= TTranslation(m_lastDelta);
  } else
    aff         = TTranslation(delta);
  double factor = 1.0 / Stage::inch;
  m_deformTool->getTool()->m_deformValues.m_moveValue =
      m_deformTool->getTool()->m_deformValues.m_moveValue + factor * delta;
  m_deformTool->transform(aff);
  m_deformTool->setCurPos(pos);
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================
// Scale
//-----------------------------------------------------------------------------

DragSelectionTool::Scale::Scale(DeformTool *deformTool, int type)
    : m_deformTool(deformTool)
    , m_startCenter(deformTool->getTool()->getCenter())
    , m_type(type)
    , m_isShiftPressed(false)
    , m_isAltPressed(false)
    , m_scaleInCenter(true) {
  int i;
  for (i = 0; i < (int)m_deformTool->getTool()->getBBoxsCount(); i++)
    m_startBboxs.push_back(m_deformTool->getTool()->getBBox(i));
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::Scale::getIntersectionPoint(const TPointD &point0,
                                                       const TPointD &point1,
                                                       const TPointD &point2,
                                                       const TPointD &point3,
                                                       const TPointD &p) const {
  // Parametri della retta passante per point0, point1
  double d1x = point0.x - point1.x;
  double m1  = d1x == 0 ? 0 : (point0.y - point1.y) / d1x;
  double q1  = point1.y - m1 * point1.x;
  // Parametri della retta passante per p parallela alla retta passante per
  // point1, point2
  double d2x = point2.x - point3.x;
  double m2  = d2x == 0 ? 0 : (point2.y - point3.y) / d2x;
  double q2  = p.y - m2 * p.x;
  // Calcolo l'intersezione tra le due rette
  double x, y, m, q;
  if (d1x == 0) {
    x = point0.x;
    m = m2;
    q = q2;
  } else if (d2x == 0) {
    x = p.x;
    m = m1;
    q = q1;
  } else {
    assert(m1 != m2);
    x = (q1 - q2) / (m2 - m1);
    m = m1;
    q = q1;
  }
  y = m * x + q;
  return TPointD(x, y);
}

//-----------------------------------------------------------------------------

DragSelectionTool::FourPoints DragSelectionTool::Scale::bboxScale(
    int index, const FourPoints &oldBbox, const TPointD &pos) {
  FourPoints bbox = oldBbox;
  TPointD p       = oldBbox.getPoint(index);
  int nextIndex   = m_deformTool->getNextVertexIndex(index);
  TPointD nextP   = oldBbox.getPoint(nextIndex);
  int nextIndex2  = m_deformTool->getNextVertexIndex(nextIndex);
  TPointD next2P  = oldBbox.getPoint(nextIndex2);
  TPointD newP    = getIntersectionPoint(next2P, nextP, nextP, p, pos);
  bbox.setPoint(nextIndex, newP);

  int beforeIndex  = m_deformTool->getBeforeVertexIndex(index);
  TPointD beforeP  = oldBbox.getPoint(beforeIndex);
  int before2Index = m_deformTool->getBeforeVertexIndex(beforeIndex);
  TPointD before2P = oldBbox.getPoint(before2Index);
  newP             = getIntersectionPoint(before2P, beforeP, beforeP, p, pos);
  bbox.setPoint(beforeIndex, newP);

  if (index < 4) bbox.setPoint(index, pos);

  return bbox;
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::Scale::computeScaleValue(int movedIndex,
                                                    const FourPoints newBbox) {
  TPointD p = m_startBboxs[0].getPoint(movedIndex);
  if (movedIndex < 4) {
    int beforeIndex = m_deformTool->getBeforePointIndex(movedIndex);
    int nextIndex   = m_deformTool->getNextPointIndex(movedIndex);
    FourPoints bbox = bboxScale(nextIndex, newBbox, p);
    TPointD scale1  = computeScaleValue(beforeIndex, bbox);
    bbox            = bboxScale(beforeIndex, newBbox, p);
    TPointD scale2  = computeScaleValue(nextIndex, bbox);
    if (movedIndex % 2 == 0)
      return TPointD(scale1.x, scale2.y);
    else
      return TPointD(scale2.x, scale1.y);
  }
  int symmetricIndex = m_deformTool->getSymmetricPointIndex(movedIndex);
  TPointD s          = m_startBboxs[0].getPoint(symmetricIndex);
  TPointD center     = m_scaleInCenter ? m_startCenter : s;
  TPointD nearP =
      m_startBboxs[0].getPoint(m_deformTool->getBeforePointIndex(movedIndex));
  TPointD pc   = getIntersectionPoint(nearP, p, p, s, center);
  TPointD newp = newBbox.getPoint(movedIndex);
  TPointD news = newBbox.getPoint(symmetricIndex);
  TPointD newNearP =
      newBbox.getPoint(m_deformTool->getBeforePointIndex(movedIndex));
  TPointD newpc = getIntersectionPoint(newNearP, newp, newp, news, center);

  double newD             = tdistance2(newpc, center);
  double oldD             = tdistance2(pc, center);
  double f                = sqrt(newD / oldD) - 1;
  TPointD startScaleValue = m_deformTool->getStartScaleValue();
  if (movedIndex % 2 == 1) {
    double sign = (pc.x < center.x && newpc.x < center.x) ||
                          (pc.x > center.x && newpc.x > center.x)
                      ? 1
                      : -1;
    double x =
        startScaleValue.x == 0 ? f : startScaleValue.x + startScaleValue.x * f;
    return TPointD(sign * x, startScaleValue.y);
  } else {
    double sign = (pc.y < center.y && newpc.y < center.y) ||
                          (pc.y > center.y && newpc.y > center.y)
                      ? 1
                      : -1;
    double y =
        startScaleValue.y == 0 ? f : startScaleValue.y + startScaleValue.y * f;
    return TPointD(startScaleValue.x, sign * y);
  }
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::Scale::getScaledPoint(int index,
                                                 const FourPoints &oldBbox,
                                                 const TPointD scaleValue,
                                                 const TPointD center) {
  TPointD p          = oldBbox.getPoint(index);
  int symmetricIndex = m_deformTool->getSymmetricPointIndex(index);
  TPointD s          = oldBbox.getPoint(symmetricIndex);
  if (index < 4) {
    int beforeIndex = m_deformTool->getBeforePointIndex(index);
    int nextIndex   = m_deformTool->getNextPointIndex(index);
    TPointD newbp   = getScaledPoint(beforeIndex, oldBbox, scaleValue, center);
    TPointD newnp   = getScaledPoint(nextIndex, oldBbox, scaleValue, center);
    TPointD bp = oldBbox.getPoint(m_deformTool->getBeforePointIndex(index));
    TPointD np = oldBbox.getPoint(m_deformTool->getNextPointIndex(index));
    TPointD in = getIntersectionPoint(np, p, bp, p, newbp);
    return getIntersectionPoint(newbp, in, np, p, newnp);
  }
  TPointD nearP = oldBbox.getPoint(m_deformTool->getBeforePointIndex(index));
  TPointD nearS =
      oldBbox.getPoint(m_deformTool->getBeforePointIndex(symmetricIndex));
  TPointD pc = getIntersectionPoint(nearP, p, p, s, center);
  TPointD sc = getIntersectionPoint(nearS, s, p, s, center);
  if (center == pc) return pc;
  TPointD v       = normalize(center - pc);
  double currentD = tdistance(sc, pc);
  double startD   = (index % 2 == 1)
                      ? currentD / m_deformTool->getStartScaleValue().x
                      : currentD / m_deformTool->getStartScaleValue().y;
  double factor = (index % 2 == 1) ? scaleValue.x : scaleValue.y;
  double d = (currentD - startD * factor) * tdistance(center, pc) / currentD;
  return TPointD(pc.x + d * v.x, pc.y + d * v.y);
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::Scale::getNewCenter(int index, const FourPoints bbox,
                                               const TPointD scaleValue) {
  int xIndex, yIndex;
  if (index < 4) {
    xIndex = m_deformTool->getBeforePointIndex(index);
    yIndex = m_deformTool->getNextPointIndex(index);
  } else {
    xIndex =
        m_deformTool->getNextPointIndex(m_deformTool->getNextPointIndex(index));
    yIndex = index;
  }
  if (index % 2 == 1) tswap(xIndex, yIndex);
  FourPoints xBbox = bboxScale(xIndex, bbox, m_startCenter);
  TPointD xCenter  = getScaledPoint(
      xIndex, xBbox, scaleValue,
      xBbox.getPoint(m_deformTool->getSymmetricPointIndex(xIndex)));
  FourPoints yBbox = bboxScale(yIndex, bbox, m_startCenter);
  TPointD yCenter  = getScaledPoint(
      yIndex, yBbox, scaleValue,
      yBbox.getPoint(m_deformTool->getSymmetricPointIndex(yIndex)));
  TPointD in = getIntersectionPoint(bbox.getP00(), bbox.getP10(), bbox.getP10(),
                                    bbox.getP11(), xCenter);
  return getIntersectionPoint(in, xCenter, bbox.getP00(), bbox.getP10(),
                              yCenter);
}

//-----------------------------------------------------------------------------

FourPoints DragSelectionTool::Scale::bboxScaleInCenter(
    int index, const FourPoints &oldBbox, const TPointD newPos,
    TPointD &scaleValue, const TPointD center, bool recomputeScaleValue) {
  TPointD oldp = oldBbox.getPoint(index);
  if (areAlmostEqual(oldp.x, newPos.x, 1e-2) &&
      areAlmostEqual(oldp.y, newPos.y, 1e-2))
    return oldBbox;
  FourPoints bbox                     = bboxScale(index, oldBbox, newPos);
  if (recomputeScaleValue) scaleValue = computeScaleValue(index, bbox);
  if (!m_scaleInCenter) return bbox;
  int symmetricIndex = m_deformTool->getSymmetricPointIndex(index);
  // Gestisco il caso particolare in cui uno dei fattori di scalatura e' -100% e
  // center e' al centro della bbox
  if (bbox.getPoint(index) == oldBbox.getPoint(symmetricIndex)) {
    bbox.setPoint(symmetricIndex, oldBbox.getPoint(index));
    bbox.setPoint(m_deformTool->getNextPointIndex(symmetricIndex),
                  oldBbox.getPoint(m_deformTool->getBeforePointIndex(index)));
    bbox.setPoint(m_deformTool->getBeforePointIndex(symmetricIndex),
                  oldBbox.getPoint(m_deformTool->getNextPointIndex(index)));
  } else
    bbox =
        bboxScale(symmetricIndex, bbox,
                  getScaledPoint(symmetricIndex, oldBbox, scaleValue, center));
  return bbox;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::Scale::leftButtonDown(const TPointD &pos,
                                              const TMouseEvent &e) {
  m_isShiftPressed = e.isShiftPressed();
  m_isAltPressed   = e.isAltPressed();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::Scale::leftButtonDrag(const TPointD &pos,
                                              const TMouseEvent &e) {
  SelectionTool *tool = m_deformTool->getTool();
  bool isBboxReset    = false;
  if (m_isShiftPressed != e.isShiftPressed() ||
      m_isAltPressed != e.isAltPressed()) {
    m_deformTool->applyTransform(m_startBboxs[0]);
    tool->setBBox(m_startBboxs[0]);
    tool->setCenter(m_startCenter);
    isBboxReset      = true;
    m_isShiftPressed = e.isShiftPressed();
    m_isAltPressed   = e.isAltPressed();
  }
  TPointD newPos    = pos;
  int selectedIndex = tool->getSelectedPoint();
  if (m_isShiftPressed && m_type == GLOBAL) {
    TPointD point = tool->getBBox().getPoint(selectedIndex);
    TPointD delta;
    if (!isBboxReset)
      delta = pos - m_deformTool->getCurPos();
    else
      delta            = pos - m_deformTool->getStartPos();
    int symmetricIndex = m_deformTool->getSymmetricPointIndex(selectedIndex);
    TPointD symmetricPoint = tool->getBBox().getPoint(symmetricIndex);
    TPointD v              = normalize(point - symmetricPoint);
    delta                  = v * (v * delta);
    newPos                 = point + delta;
  }
  m_scaleInCenter = m_isAltPressed;
  m_deformTool->setCurPos(pos);
  TPointD scaleValue = m_deformTool->transform(selectedIndex, newPos);
  tool->m_deformValues.m_scaleValue = scaleValue;
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================
// SelectionTool
//-----------------------------------------------------------------------------

SelectionTool::SelectionTool(int targetType)
    : TTool("T_Selection")
    , m_firstTime(true)
    , m_dragTool(0)
    , m_what(Outside)
    , m_leftButtonMousePressed(false)
    , m_shiftPressed(false)
    , m_selecting(false)
    , m_mousePosition(TPointD())
    , m_stroke(0)
    , m_justSelected(false)
    , m_strokeSelectionType("Type:")
    , m_deformValues()
    , m_cursorId(ToolCursor::CURSOR_ARROW) {
  bind(targetType);
  m_prop.bind(m_strokeSelectionType);

  m_strokeSelectionType.addValue(RECT_SELECTION);
  m_strokeSelectionType.addValue(FREEHAND_SELECTION);
  m_strokeSelectionType.addValue(POLYLINE_SELECTION);
  m_strokeSelectionType.setId("Type");
}

//-----------------------------------------------------------------------------

SelectionTool::~SelectionTool() {
  delete m_dragTool;
  if (m_stroke) {
    delete m_stroke;
    m_stroke = 0;
  }
  if (!m_freeDeformers.empty()) clearPointerContainer(m_freeDeformers);
}

//-----------------------------------------------------------------------------

void SelectionTool::clearDeformers() { clearPointerContainer(m_freeDeformers); }

//-----------------------------------------------------------------------------

TPointD SelectionTool::getCenter(int index) const {
  if (m_centers.empty()) return TPointD();
  assert((int)m_centers.size() > index);
  return m_centers[index];
}

//-----------------------------------------------------------------------------

void SelectionTool::setCenter(const TPointD &center, int index) {
  if (m_centers.empty()) return;
  assert((int)m_centers.size() > index);
  m_centers[index] = center;
}

//-----------------------------------------------------------------------------

int SelectionTool::getBBoxsCount() const { return m_bboxs.size(); }

//-----------------------------------------------------------------------------

DragSelectionTool::FourPoints SelectionTool::getBBox(int index) const {
  if (m_bboxs.empty()) return DragSelectionTool::FourPoints();
  assert((int)m_bboxs.size() > index);
  return m_bboxs[index];
}

//-----------------------------------------------------------------------------

void SelectionTool::setBBox(const DragSelectionTool::FourPoints &points,
                            int index) {
  if (m_bboxs.empty()) return;
  assert((int)m_bboxs.size() > index);
  m_bboxs[index] = points;
}

//-----------------------------------------------------------------------------

FreeDeformer *SelectionTool::getFreeDeformer(int index) const {
  if (m_freeDeformers.empty()) return 0;
  return m_freeDeformers[index];
}

//-----------------------------------------------------------------------------

void SelectionTool::updateTranslation() {
  m_strokeSelectionType.setQStringName(tr("Type:"));
}
//-----------------------------------------------------------------------------

void SelectionTool::updateAction(TPointD pos, const TMouseEvent &e) {
  TImageP image    = getImage(false);
  TToonzImageP ti  = image;
  TRasterImageP ri = image;
  TVectorImageP vi = image;

  m_what     = Outside;
  m_cursorId = ToolCursor::StrokeSelectCursor;

  if (!ti && !vi && !ri) return;

  bool shift = e.isShiftPressed();

  if (shift && !m_leftButtonMousePressed && isModifiableSelectionType()) {
    m_what     = ADD_SELECTION;
    m_cursorId = ToolCursor::SplineEditorCursorAdd;
  } else if (m_leftButtonMousePressed)
    return;

  FourPoints bbox = getBBox();

  double pixelSize = getPixelSize();
  if (!bbox.isEmpty()) {
    double maxDist  = 8 * pixelSize;
    double maxDist2 = maxDist * maxDist;
    double p        = (12 * pixelSize) - 3 * pixelSize;
    m_selectedPoint = NONE;
    if (tdistance2(bbox.getP00(), pos) < maxDist2 + p)
      m_selectedPoint = P00;
    else if (tdistance2(bbox.getP11(), pos) < maxDist2 + p)
      m_selectedPoint = P11;
    else if (tdistance2(bbox.getP01(), pos) < maxDist2 + p)
      m_selectedPoint = P01;
    else if (tdistance2(bbox.getP10(), pos) < maxDist2 + p)
      m_selectedPoint = P10;

    if (tdistance2(bbox.getBottomLeft() + TPointD(-p, -p), pos) < maxDist2) {
      m_what     = ROTATION;
      m_cursorId = ToolCursor::RotBottomLeft;
      return;
    } else if (tdistance2(bbox.getBottomRight() + TPointD(p, -p), pos) <
               maxDist2) {
      m_what     = ROTATION;
      m_cursorId = ToolCursor::RotBottomRight;
      return;
    } else if (tdistance2(bbox.getTopRight() + TPointD(p, p), pos) < maxDist2) {
      m_what     = ROTATION;
      m_cursorId = ToolCursor::RotCursor;
      return;
    } else if (tdistance2(bbox.getTopLeft() + TPointD(-p, p), pos) < maxDist2) {
      m_what     = ROTATION;
      m_cursorId = ToolCursor::RotTopLeft;
      return;
    }
    maxDist  = 5 * pixelSize;
    maxDist2 = maxDist * maxDist;
    if (tdistance2(bbox.getP00(), pos) < maxDist2 ||
        tdistance2(bbox.getP11(), pos) < maxDist2 ||
        tdistance2(bbox.getP01(), pos) < maxDist2 ||
        tdistance2(bbox.getP10(), pos) < maxDist2) {
      if (!e.isCtrlPressed() || isLevelType() || isSelectedFramesType()) {
        m_what = SCALE;
        if (tdistance2(bbox.getTopRight(), pos) < maxDist2 ||
            tdistance2(bbox.getBottomLeft(), pos) < maxDist2)
          m_cursorId = ToolCursor::ScaleCursor;
        else
          m_cursorId = ToolCursor::ScaleInvCursor;
      } else {
        m_cursorId = ToolCursor::DistortCursor;
        m_what     = DEFORM;
      }
      return;
    }
    if (isCloseToSegment(pos, TSegment(bbox.getPoint(0), bbox.getPoint(3)),
                         maxDist))
      m_selectedPoint = P0M;
    else if (isCloseToSegment(pos, TSegment(bbox.getPoint(1), bbox.getPoint(2)),
                              maxDist))
      m_selectedPoint = P1M;
    else if (isCloseToSegment(pos, TSegment(bbox.getPoint(3), bbox.getPoint(2)),
                              maxDist))
      m_selectedPoint = PM1;
    else if (isCloseToSegment(pos, TSegment(bbox.getPoint(0), bbox.getPoint(1)),
                              maxDist))
      m_selectedPoint = PM0;
    if (m_selectedPoint == P0M || m_selectedPoint == P1M) {
      if (!e.isCtrlPressed() || isLevelType() || isSelectedFramesType()) {
        m_cursorId = ToolCursor::ScaleHCursor;
        m_what     = SCALE_X;
      } else {
        m_cursorId = ToolCursor::DistortCursor;
        m_what     = DEFORM;
      }
      return;
    }
    if (m_selectedPoint == PM1 || m_selectedPoint == PM0) {
      if (!e.isCtrlPressed() || isLevelType() || isSelectedFramesType()) {
        m_cursorId = ToolCursor::ScaleVCursor;
        m_what     = SCALE_Y;
      } else {
        m_cursorId = ToolCursor::DistortCursor;
        m_what     = DEFORM;
      }
      return;
    }
    if (!isLevelType() && !isSelectedFramesType() &&
        tdistance2(getCenter(), pos) < maxDist2) {
      m_what = MOVE_CENTER;
      return;
    }
    TPointD hpos = bbox.getP10() - TPointD(14 * pixelSize, 15 * pixelSize);
    TRectD rect(hpos - TPointD(14 * pixelSize, 5 * pixelSize),
                hpos + TPointD(14 * pixelSize, 5 * pixelSize));
    if (!m_deformValues.m_isSelectionModified && rect.contains(pos) && vi &&
        !TTool::getApplication()->getCurrentObject()->isSpline()) {
      m_what     = GLOBAL_THICKNESS;
      m_cursorId = ToolCursor::PumpCursor;
      return;
    }
  }
  m_selectedPoint = NONE;
  if ((isLevelType() || isSelectedFramesType()) && !isSameStyleType()) {
    m_what = Inside;
    ToolCursor::LevelSelectCursor;
  }

  if (shift) return;
  if (!vi && bbox.contains(pos)) {
    m_what = Inside;
    if (isLevelType() || isSelectedFramesType())
      m_cursorId = ToolCursor::LevelSelectCursor;
    else
      m_cursorId = ToolCursor::MoveCursor;
  }
}

//-----------------------------------------------------------------------------

void SelectionTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TImageP image = getImage(false);
  if (!image) return;
  if (m_polyline.size() == 0) {
    modifySelectionOnClick(image, pos, e);

    if (m_what == ROTATION) m_dragTool = createNewRotationTool(this);
    if (!e.isShiftPressed() && m_what == Inside)
      m_dragTool = createNewMoveSelectionTool(this);
    else if (m_what == MOVE_CENTER)
      m_dragTool = new MoveCenterTool(this);
    else if (m_what == SCALE)
      m_dragTool = createNewScaleTool(this, 0);
    else if (m_what == SCALE_X)
      m_dragTool = createNewScaleTool(this, 1);
    else if (m_what == SCALE_Y)
      m_dragTool = createNewScaleTool(this, 2);
    else if (m_what == DEFORM)
      m_dragTool = createNewFreeDeformTool(this);
    else if (m_what == GLOBAL_THICKNESS)
      m_dragTool = new VectorChangeThicknessTool((VectorSelectionTool *)this);
    if (m_dragTool) m_dragTool->leftButtonDown(pos, e);
  } else
    m_selecting = true;
  if (m_selecting) {
    if (m_stroke) {
      delete m_stroke;
      m_stroke = 0;
    }
    if (m_strokeSelectionType.getValue() == FREEHAND_SELECTION)
      startFreehand(pos);
    if (m_strokeSelectionType.getValue() == POLYLINE_SELECTION)
      addPointPolyline(pos);
    else if (m_polyline.size() != 0)
      m_polyline.clear();
  }
  m_firstPos = m_curPos    = pos;
  m_leftButtonMousePressed = true;
  m_shiftPressed           = e.isShiftPressed();
}

//-----------------------------------------------------------------------------

void SelectionTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  updateAction(pos, e);

  if (m_strokeSelectionType.getValue() == POLYLINE_SELECTION) {
    m_mousePosition = pos;
    invalidate();
  }
}

//-----------------------------------------------------------------------------

bool SelectionTool::keyDown(QKeyEvent *event) {
  if (isSelectionEmpty()) return false;

  TPointD delta;

  switch (event->key()) {
  case Qt::Key_Up:
    delta.y = 1;
    break;
  case Qt::Key_Down:
    delta.y = -1;
    break;
  case Qt::Key_Left:
    delta.x = -1;
    break;
  case Qt::Key_Right:
    delta.x = 1;
    break;
  default:
    return false;
    break;
  }

  if (event->modifiers() & Qt::ShiftModifier) {
    delta.x *= 10.0;
    delta.y *= 10.0;
  } else if (event->modifiers() & Qt::ControlModifier) {
    delta.x *= 0.1;
    delta.y *= 0.1;
  }

  TImageP image = getImage(true);

  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  TVectorImageP vi = (TVectorImageP)image;

  if (!ti && !vi && !ri) return false;

  DragTool *dragTool = createNewMoveSelectionTool(this);
  TAffine aff        = TTranslation(delta);
  dragTool->transform(aff);
  double factor = 1.0 / Stage::inch;
  m_deformValues.m_moveValue += factor * delta;
  dragTool->addTransformUndo();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  delete dragTool;
  dragTool = 0;

  invalidate();
  return true;
}

//-----------------------------------------------------------------------------

int SelectionTool::getCursorId() const {
  TImageP image    = getImage(false);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  TVectorImageP vi = (TVectorImageP)image;

  if (!ti && !vi && !ri) return ToolCursor::StrokeSelectCursor;

  return m_cursorId;
}

//-----------------------------------------------------------------------------

void SelectionTool::drawPolylineSelection() {
  if (m_polyline.empty()) return;
  TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                     ? TPixel32::White
                     : TPixel32::Black;
  tglColor(color);
  tglDrawCircle(m_polyline[0], 2);
  glBegin(GL_LINE_STRIP);
  for (UINT i = 0; i < m_polyline.size(); i++) tglVertex(m_polyline[i]);
  tglVertex(m_mousePosition);
  glEnd();
}

//-----------------------------------------------------------------------------

void SelectionTool::drawFreehandSelection() {
  if (m_track.isEmpty()) return;
  TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                     ? TPixel32::White
                     : TPixel32::Black;
  tglColor(color);
  m_track.drawAllFragments();
}

//-----------------------------------------------------------------------------

void SelectionTool::drawRectSelection(const TImage *image) {
  const TVectorImage *vi   = dynamic_cast<const TVectorImage *>(image);
  unsigned short stipple   = 0x3F33;
  FourPoints selectingRect = m_selectingRect;
  if (vi && m_curPos.x >= m_firstPos.x) stipple = 0xFF00;
  drawFourPoints(selectingRect, TPixel32::Black, stipple, true);
}

//-----------------------------------------------------------------------------

void SelectionTool::drawCommandHandle(const TImage *image) {
  const TVectorImage *vi = dynamic_cast<const TVectorImage *>(image);
  TPixel32 frameColor(127, 127, 127);
  FourPoints rect = getBBox();

  drawFourPoints(rect, frameColor, 0xffff, true);

  tglColor(frameColor);

  if (m_dragTool) m_dragTool->draw();

  double pixelSize = getPixelSize();
  if (!isLevelType() && !isSelectedFramesType())
    tglDrawCircle(getCenter(), pixelSize * 4);

  drawSquare(rect.getP00(), pixelSize * 4, frameColor);
  drawSquare(rect.getP01(), pixelSize * 4, frameColor);
  drawSquare(rect.getP10(), pixelSize * 4, frameColor);
  drawSquare(rect.getP11(), pixelSize * 4, frameColor);

  if (vi && !m_deformValues.m_isSelectionModified) {
    TPointD thickCommandPos =
        rect.getP10() - TPointD(14 * pixelSize, 15 * pixelSize);
    drawRectWhitArrow(thickCommandPos, pixelSize);
  }

  drawSquare(0.5 * (rect.getP10() + rect.getP11()), pixelSize * 4, frameColor);
  drawSquare(0.5 * (rect.getP01() + rect.getP11()), pixelSize * 4, frameColor);
  drawSquare(0.5 * (rect.getP10() + rect.getP00()), pixelSize * 4, frameColor);
  drawSquare(0.5 * (rect.getP01() + rect.getP00()), pixelSize * 4, frameColor);
}

//-----------------------------------------------------------------------------

void SelectionTool::onActivate() {
  if (m_firstTime) {
    m_strokeSelectionType.setValue(::to_wstring(SelectionType.getValue()));
    m_firstTime = false;
  }
  if (isLevelType() || isSelectedFramesType()) return;

  doOnActivate();
}

//-----------------------------------------------------------------------------

void SelectionTool::onDeactivate() {
  if (isLevelType() || isSelectedFramesType()) return;

  doOnDeactivate();
}

//-----------------------------------------------------------------------------

void SelectionTool::onSelectionChanged() {
  computeBBox();
  invalidate();
  m_polyline.clear();
}

//-----------------------------------------------------------------------------

bool SelectionTool::onPropertyChanged(std::string propertyName) {
  if (propertyName == m_strokeSelectionType.getName()) {
    SelectionType = ::to_string(m_strokeSelectionType.getValue());
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del lazzo.
//! Viene inizializzato \b m_firstPos
void SelectionTool::startFreehand(const TPointD &pos) {
  m_track.clear();
  m_firstPos       = pos;
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(pos, 0), pixelSize * pixelSize);
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del lazzo.
void SelectionTool::freehandDrag(const TPointD &pos) {
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(pos, 0), pixelSize * pixelSize);
}

//-----------------------------------------------------------------------------

//! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene creato
//! lo stroke rappresentante il lazzo.
void SelectionTool::closeFreehand(const TPointD &pos) {
  if (m_track.isEmpty()) return;
  double pixelSize = getPixelSize();
  m_track.add(TThickPoint(m_firstPos, 0), pixelSize * pixelSize);
  m_track.filterPoints();
  double error = (30.0 / 11) * pixelSize;
  m_stroke     = m_track.makeStroke(error);
  m_stroke->setStyle(1);
}

//-----------------------------------------------------------------------------

//! Viene aggiunto un punto al vettore m_polyline.
void SelectionTool::addPointPolyline(const TPointD &pos) {
  m_firstPos      = pos;
  m_mousePosition = pos;
  m_polyline.push_back(pos);
}

//-----------------------------------------------------------------------------

//! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
//! m_polyline.front() alla fine del vettore).
void SelectionTool::closePolyline(const TPointD &pos) {
  if (m_polyline.size() <= 1) return;
  if (m_polyline.back() != pos) m_polyline.push_back(pos);
  if (m_polyline.back() != m_polyline.front())
    m_polyline.push_back(m_polyline.front());

  std::vector<TThickPoint> strokePoints;
  for (UINT i = 0; i < m_polyline.size() - 1; i++) {
    strokePoints.push_back(TThickPoint(m_polyline[i], 0));
    strokePoints.push_back(
        TThickPoint(0.5 * (m_polyline[i] + m_polyline[i + 1]), 0));
  }
  strokePoints.push_back(TThickPoint(m_polyline.back(), 0));
  m_polyline.clear();
  m_stroke = new TStroke(strokePoints);
  assert(m_stroke->getPoint(0) == m_stroke->getPoint(1));
  invalidate();
}

//-----------------------------------------------------------------------------

// returns true if the pressed key is recognized and processed in the tool
// instead of triggering the shortcut command.
bool SelectionTool::isEventAcceptable(QEvent *e) {
  if (!isEnabled()) return false;
  if (isSelectionEmpty()) return false;
  // arrow keys will be used for moving the selected region
  QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
  int key             = keyEvent->key();
  return (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left ||
          key == Qt::Key_Right);
}