
#include "flowlinestrokestyle.h"

#include "tcolorfunctions.h"
#include "tcurves.h"

#define BUFFER_OFFSET(bytes) ((GLubyte *)NULL + (bytes))
#define V_BUFFER_SIZE 1000

namespace {
double getMaxThickness(const TStroke *s) {
  int count           = s->getControlPointCount();
  double maxThickness = -1;

  for (int i = 0; i < s->getControlPointCount(); i++) {
    double thick = s->getControlPoint(i).thick;
    if (thick > maxThickness) maxThickness = thick;
  }
  return maxThickness;
}

struct float2 {
  float x;
  float y;
};

inline double dot(const TPointD &v1, const TPointD &v2) {
  return v1.x * v2.x + v1.y * v2.y;
}

//-----------------------------------------------------------------------------

inline bool isLinearPoint(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2) {
  return (tdistance(p0, p1) < 0.02) && (tdistance(p1, p2) < 0.02);
}

//-----------------------------------------------------------------------------

//! Ritorna \b true se il punto \b p1 e' una cuspide.
bool isCuspPoint(const TPointD &p0, const TPointD &p1, const TPointD &p2) {
  TPointD p0_p1(p0 - p1), p2_p1(p2 - p1);
  double n1 = norm(p0_p1), n2 = norm(p2_p1);

  // Partial linear points are ALWAYS cusps (since directions from them are
  // determined by neighbours, not by the points themselves)
  if ((n1 < 0.02) || (n2 < 0.02)) return true;

  p0_p1 = p0_p1 * (1.0 / n1);
  p2_p1 = p2_p1 * (1.0 / n2);

  return (p0_p1 * p2_p1 > 0) ||
         (fabs(cross(p0_p1, p2_p1)) > 0.09);  // more than 5° is yes

  // Distance-based check. Unscalable...
  // return
  // !areAlmostEqual(tdistance(p0,p2),tdistance(p0,p1)+tdistance(p1,p2),2);
}

//-----------------------------------------------------------------------------
/*! Insert a point in the most long chunk between chunk \b indexA and chunk \b
 * indexB. */
void insertPoint(TStroke *stroke, int indexA, int indexB) {
  assert(stroke);
  int j          = 0;
  int chunkCount = indexB - indexA;
  if (chunkCount % 2 == 0) return;
  double length = 0;
  double firstW, lastW;
  for (j = indexA; j < indexB; j++) {
    // cerco il chunk piu' lungo
    double w0 = stroke->getW(stroke->getChunk(j)->getP0());
    double w1;
    if (j == stroke->getChunkCount() - 1)
      w1 = 1;
    else
      w1 = stroke->getW(stroke->getChunk(j)->getP2());
    double length0 = stroke->getLength(w0);
    double length1 = stroke->getLength(w1);
    if (length < length1 - length0) {
      firstW = w0;
      lastW  = w1;
      length = length1 - length0;
    }
  }
  stroke->insertControlPoints((firstW + lastW) * 0.5);
}
};  // namespace
//-----------------------------------------------------------------------------

FlowLineStrokeStyle::FlowLineStrokeStyle()
    : m_color(TPixel32(100, 200, 200, 255))
    , m_density(0.25)
    , m_extension(5.0)
    , m_widthScale(5.0)
    , m_straightenEnds(true) {}

//-----------------------------------------------------------------------------

TColorStyle *FlowLineStrokeStyle::clone() const {
  return new FlowLineStrokeStyle(*this);
}

//-----------------------------------------------------------------------------

int FlowLineStrokeStyle::getParamCount() const { return ParamCount; }

//-----------------------------------------------------------------------------

TColorStyle::ParamType FlowLineStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  switch (index) {
  case Density:
  case Extension:
  case WidthScale:
    return TColorStyle::DOUBLE;
  case StraightenEnds:
    return TColorStyle::BOOL;
  }
  return TColorStyle::DOUBLE;
}

//-----------------------------------------------------------------------------

QString FlowLineStrokeStyle::getParamNames(int index) const {
  assert(0 <= index && index < ParamCount);
  switch (index) {
  case Density:
    return QCoreApplication::translate("FlowLineStrokeStyle", "Density");
  case Extension:
    return QCoreApplication::translate("FlowLineStrokeStyle", "Extension");
  case WidthScale:
    return QCoreApplication::translate("FlowLineStrokeStyle", "Width Scale");
  case StraightenEnds:
    return QCoreApplication::translate("FlowLineStrokeStyle",
                                       "Straighten Ends");
  }
  return QString();
}

//-----------------------------------------------------------------------------

void FlowLineStrokeStyle::getParamRange(int index, double &min,
                                        double &max) const {
  assert(0 <= index && index < ParamCount);
  switch (index) {
  case Density:
    min = 0.2;
    max = 5.0;
    break;
  case Extension:
    min = 0.0;
    max = 20.0;
    break;
  case WidthScale:
    min = 1.0;
    max = 50.0;
    break;
  }
}

//-----------------------------------------------------------------------------

double FlowLineStrokeStyle::getParamValue(TColorStyle::double_tag,
                                          int index) const {
  assert(0 <= index && index < ParamCount);
  switch (index) {
  case Density:
    return m_density;
  case Extension:
    return m_extension;
  case WidthScale:
    return m_widthScale;
  }
  return 0.0;
}

//-----------------------------------------------------------------------------

void FlowLineStrokeStyle::setParamValue(int index, double value) {
  assert(0 <= index && index < ParamCount);
  switch (index) {
  case Density:
    m_density = value;
    break;
  case Extension:
    m_extension = value;
    break;
  case WidthScale:
    m_widthScale = value;
    break;
  }
  updateVersionNumber();
}

//-----------------------------------------------------------------------------

bool FlowLineStrokeStyle::getParamValue(TColorStyle::bool_tag,
                                        int index) const {
  assert(index == StraightenEnds);
  return m_straightenEnds;
}

//-----------------------------------------------------------------------------

void FlowLineStrokeStyle::setParamValue(int index, bool value) {
  assert(index == StraightenEnds);
  m_straightenEnds = value;
}

//-----------------------------------------------------------------------------

void FlowLineStrokeStyle::drawStroke(const TColorFunction *cf,
                                     const TStroke *stroke) const {
  auto lerp = [](float val1, float val2, float ratio) {
    return val1 * (1.0 - ratio) + val2 * ratio;
  };
  auto length2 = [](TPointD p) { return p.x * p.x + p.y * p.y; };

  // Length and position of the stroke is in "actual size" (i.e. not in pixels).
  // 1unit = 1/53.3333inches
  // Strokeの長さや座標は実寸（１単位＝1/53.3333インチ）
  double length = stroke->getLength();
  if (length <= 0) return;

  double maxThickness = getMaxThickness(stroke) * m_widthScale;
  if (maxThickness <= 0) return;

  TStroke *tmpStroke = new TStroke(*stroke);

  // straighten ends. replace the control point only if the new handle will
  // become longer
  if (m_straightenEnds && stroke->getControlPointCount() >= 5 &&
      !stroke->isSelfLoop()) {
    TPointD newPos = tmpStroke->getControlPoint(0) * 0.75 +
                     tmpStroke->getControlPoint(3) * 0.25;
    TPointD oldVec =
        tmpStroke->getControlPoint(1) - tmpStroke->getControlPoint(0);
    TPointD newVec = newPos - tmpStroke->getControlPoint(0);
    if (length2(oldVec) < length2(newVec))
      tmpStroke->setControlPoint(1, newPos);

    int lastId = stroke->getControlPointCount() - 1;
    newPos     = tmpStroke->getControlPoint(lastId) * 0.75 +
             tmpStroke->getControlPoint(lastId - 3) * 0.25;
    oldVec = tmpStroke->getControlPoint(lastId - 1) -
             tmpStroke->getControlPoint(lastId);
    newVec = newPos - tmpStroke->getControlPoint(lastId);
    if (length2(oldVec) < length2(newVec))
      tmpStroke->setControlPoint(lastId - 1, newPos);
  }

  // see ControlPointEditorStroke::adjustChunkParity() in
  // controlpointselection.cpp
  int firstChunk;
  int secondChunk = tmpStroke->getChunkCount();
  int i;
  for (i = tmpStroke->getChunkCount() - 1; i > 0; i--) {
    if (tdistance(tmpStroke->getChunk(i - 1)->getP0(),
                  tmpStroke->getChunk(i)->getP2()) < 0.5)
      continue;
    TPointD p0 = tmpStroke->getChunk(i - 1)->getP1();
    TPointD p1 = tmpStroke->getChunk(i - 1)->getP2();
    TPointD p2 = tmpStroke->getChunk(i)->getP1();
    if (isCuspPoint(p0, p1, p2) || isLinearPoint(p0, p1, p2)) {
      firstChunk = i;
      insertPoint(tmpStroke, firstChunk, secondChunk);
      secondChunk = firstChunk;
    }
  }
  insertPoint(tmpStroke, 0, secondChunk);

  // thin lines amount 細線の本数
  int count = 2 * (int)std::ceil(maxThickness * m_density) - 1;

  TPixel32 color;
  if (cf)
    color = (*(cf))(m_color);
  else
    color = m_color;

  float2 *vertBuf  = new float2[V_BUFFER_SIZE];
  int maxVertCount = 0;

  glEnableClientState(GL_VERTEX_ARRAY);

  int centerId = (count - 1) / 2;

  TThickPoint p0 = tmpStroke->getThickPoint(0);
  TThickPoint p1 = tmpStroke->getThickPoint(1);
  double d01     = tdistance(p0, p1);
  if (d01 == 0) return;

  int len = (int)(d01);
  if (len == 0) return;

  TPointD v0 = tmpStroke->getSpeed(0);
  TPointD v1 = tmpStroke->getSpeed(1);

  if (norm2(v0) == 0 || norm2(v1) == 0) return;

  TPointD startVec = -normalize(v0);
  TPointD endVec   = normalize(v1);

  for (int i = 0; i < count; i++) {
    double widthRatio =
        (count == 1) ? 0.0 : (double)(i - centerId) / (double)centerId;
    double extensionLength =
        (1.0 - std::abs(widthRatio)) * maxThickness * m_extension;

    glColor4ub(color.r, color.g, color.b, color.m);

    int divAmount = len * 5;
    if (divAmount + 3 > V_BUFFER_SIZE) divAmount = V_BUFFER_SIZE - 3;

    float2 *vp = vertBuf;

    for (int j = 0; j <= divAmount; j++, vp++) {
      // current position ratio  今の座標の割合位置
      double t = double(j) / double(divAmount);

      // position ratio on the stroke ストローク上のratio位置
      double w = t;
      // unit vector perpendicular to the stroke 鉛直方向の単位ベクトル
      TPointD v = rotate90(normalize(tmpStroke->getSpeed(w)));
      assert(0 <= w && w <= 1);
      // position on the stroke ストローク上の位置
      TPointD p = tmpStroke->getPoint(w);

      TPointD pos = p + v * maxThickness * widthRatio;

      if (j == 0) {
        TPointD sPos = pos + startVec * extensionLength;
        *vp          = {float(sPos.x), float(sPos.y)};
        vp++;
      }
      *vp = {float(pos.x), float(pos.y)};
      if (j == divAmount) {
        vp++;
        TPointD ePos = pos + endVec * extensionLength;
        *vp          = {float(ePos.x), float(ePos.y)};
      }
    }

    glVertexPointer(2, GL_FLOAT, 0, vertBuf);
    // draw
    glDrawArrays(GL_LINE_STRIP, 0, divAmount + 3);
  }
  glColor4d(0, 0, 0, 1);
  glDisableClientState(GL_VERTEX_ARRAY);
  delete[] vertBuf;
  delete tmpStroke;
}

//-----------------------------------------------------------------------------

TRectD FlowLineStrokeStyle::getStrokeBBox(const TStroke *stroke) const {
  TRectD rect = TColorStyle::getStrokeBBox(stroke);
  double margin =
      getMaxThickness(stroke) * m_widthScale * std::max(1.0, m_extension);
  return rect.enlarge(margin);
}

//-----------------------------------------------------------------------------
