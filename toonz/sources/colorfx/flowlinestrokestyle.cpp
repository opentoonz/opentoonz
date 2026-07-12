

#include "flowlinestrokestyle.h"

#include "tcolorfunctions.h"
#include "tcurves.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

#define BUFFER_OFFSET(bytes) ((GLubyte *)NULL + (bytes))
#define V_BUFFER_SIZE 1000

namespace {

// Returns the maximum thickness value among all control points of the stroke
double getMaxThickness(const TStroke *s) {
  double maxThickness = -1.0;
  int cpCount         = s->getControlPointCount();
  for (int i = 0; i < cpCount; ++i) {
    double thick = s->getControlPoint(i).thick;
    if (thick > maxThickness) maxThickness = thick;
  }
  return maxThickness;
}

// Simple structure for 2D floating point vertices
struct float2 {
  float x;
  float y;
};

// Dot product of two 2D vectors
inline double dot(const TPointD &v1, const TPointD &v2) {
  return v1.x * v2.x + v1.y * v2.y;
}

// Returns true if the middle point is part of a nearly straight segment (very
// short handles)
inline bool isLinearPoint(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2) {
  return (tdistance(p0, p1) < 0.02) && (tdistance(p1, p2) < 0.02);
}

// Returns true if the angle at p1 is sharp enough to be considered a cusp
bool isCuspPoint(const TPointD &p0, const TPointD &p1, const TPointD &p2) {
  TPointD p0_p1 = p0 - p1;
  TPointD p2_p1 = p2 - p1;

  double n1 = norm(p0_p1), n2 = norm(p2_p1);

  // Very short handles are always treated as cusps
  if (n1 < 0.02 || n2 < 0.02) return true;

  p0_p1 = p0_p1 * (1.0 / n1);
  p2_p1 = p2_p1 * (1.0 / n2);

  // Same direction or angle > ~5 degrees -> cusp
  return (dot(p0_p1, p2_p1) > 0) || (std::abs(cross(p0_p1, p2_p1)) > 0.09);
}

// Inserts a control point in the middle of the longest chunk in the given range
void insertPoint(TStroke *stroke, int indexA, int indexB) {
  assert(stroke);
  if ((indexB - indexA) % 2 == 0) return;

  double bestLength = 0.0;
  double firstW = 0.0, lastW = 0.0;

  for (int j = indexA; j < indexB; ++j) {
    double w0 = stroke->getW(stroke->getChunk(j)->getP0());
    double w1 = (j == stroke->getChunkCount() - 1)
                    ? 1.0
                    : stroke->getW(stroke->getChunk(j)->getP2());

    double segLen = stroke->getLength(w1) - stroke->getLength(w0);
    if (segLen > bestLength) {
      bestLength = segLen;
      firstW     = w0;
      lastW      = w1;
    }
  }

  if (bestLength > 0.0) stroke->insertControlPoints((firstW + lastW) * 0.5);
}

}  // anonymous namespace

// RAII helper to ensure OpenGL vertex array state is properly restored
struct GLVertexArrayGuard {
  GLVertexArrayGuard() { glEnableClientState(GL_VERTEX_ARRAY); }
  ~GLVertexArrayGuard() { glDisableClientState(GL_VERTEX_ARRAY); }

  // Non-copyable
  GLVertexArrayGuard(const GLVertexArrayGuard &)            = delete;
  GLVertexArrayGuard &operator=(const GLVertexArrayGuard &) = delete;
};

//=============================================================================
// FlowLineStrokeStyle Implementation
//=============================================================================

FlowLineStrokeStyle::FlowLineStrokeStyle()
    : m_color(TPixel32(100, 200, 200, 255))
    , m_density(0.25)
    , m_extension(5.0)
    , m_widthScale(5.0)
    , m_straightenEnds(true) {}

TColorStyle *FlowLineStrokeStyle::clone() const {
  return new FlowLineStrokeStyle(*this);
}

int FlowLineStrokeStyle::getParamCount() const { return ParamCount; }

TColorStyle::ParamType FlowLineStrokeStyle::getParamType(int index) const {
  assert(0 <= index && index < getParamCount());
  switch (index) {
  case Density:
    return TColorStyle::DOUBLE;
  case Extension:
    return TColorStyle::DOUBLE;
  case WidthScale:
    return TColorStyle::DOUBLE;
  case StraightenEnds:
    return TColorStyle::BOOL;
  }
  return TColorStyle::DOUBLE;
}

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
  default:
    min = 0.0;
    max = 1.0;
    break;
  }
}

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
  default:
    return 0.0;
  }
}

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

bool FlowLineStrokeStyle::getParamValue(TColorStyle::bool_tag,
                                        int index) const {
  assert(index == StraightenEnds);
  return m_straightenEnds;
}

void FlowLineStrokeStyle::setParamValue(int index, bool value) {
  assert(index == StraightenEnds);
  m_straightenEnds = value;
}

//=============================================================================

void FlowLineStrokeStyle::drawStroke(const TColorFunction *cf,
                                     const TStroke *stroke) const {
  // Lambda to compute squared length of a 2D vector
  auto length2 = [](const TPointD &p) { return p.x * p.x + p.y * p.y; };

  // Early exit for degenerate strokes
  const double length = stroke->getLength();
  if (length <= 0.0) return;

  // Calculate maximum thickness scaled by the width parameter
  const double maxThickness = getMaxThickness(stroke) * m_widthScale;
  if (maxThickness <= 0.0) return;

  // Work on a copy so we can safely modify control points
  auto tmpStroke = std::make_unique<TStroke>(*stroke);

  // Straighten bezier handles at both ends for a cleaner flow effect
  if (m_straightenEnds && stroke->getControlPointCount() >= 5 &&
      !stroke->isSelfLoop()) {
    const int cpCount = stroke->getControlPointCount();
    const int lastId  = cpCount - 1;

    // Start of stroke: adjust the first control handle
    {
      TPointD newPos = tmpStroke->getControlPoint(0) * 0.75 +
                       tmpStroke->getControlPoint(3) * 0.25;
      TPointD oldVec =
          tmpStroke->getControlPoint(1) - tmpStroke->getControlPoint(0);
      TPointD newVec = newPos - tmpStroke->getControlPoint(0);
      if (length2(oldVec) < length2(newVec))
        tmpStroke->setControlPoint(1, newPos);
    }

    // End of stroke: adjust the last control handle
    {
      TPointD newPos = tmpStroke->getControlPoint(lastId) * 0.75 +
                       tmpStroke->getControlPoint(lastId - 3) * 0.25;
      TPointD oldVec = tmpStroke->getControlPoint(lastId - 1) -
                       tmpStroke->getControlPoint(lastId);
      TPointD newVec = newPos - tmpStroke->getControlPoint(lastId);
      if (length2(oldVec) < length2(newVec))
        tmpStroke->setControlPoint(lastId - 1, newPos);
    }
  }

  // Ensure even chunk parity around cusps and linear points (required by some
  // renderers)
  int secondChunk = tmpStroke->getChunkCount();
  for (int i = secondChunk - 1; i > 0; --i) {
    // Skip if chunks are very close together
    if (tdistance(tmpStroke->getChunk(i - 1)->getP0(),
                  tmpStroke->getChunk(i)->getP2()) < 0.5)
      continue;

    const TPointD p0 = tmpStroke->getChunk(i - 1)->getP1();
    const TPointD p1 = tmpStroke->getChunk(i - 1)->getP2();
    const TPointD p2 = tmpStroke->getChunk(i)->getP1();

    if (isCuspPoint(p0, p1, p2) || isLinearPoint(p0, p1, p2)) {
      insertPoint(tmpStroke.get(), i, secondChunk);
      secondChunk = i;
    }
  }
  insertPoint(tmpStroke.get(), 0, secondChunk);

  // Number of parallel thin lines (odd number ? symmetric around center)
  const int count =
      2 * static_cast<int>(std::ceil(maxThickness * m_density)) - 1;

  // Apply color function if present, otherwise use the base color
  TPixel32 color = m_color;
  if (cf) color = (*cf)(m_color);

  // Pre-allocated vertex buffer (stack-friendly when possible)
  std::vector<float2> vertBuf(V_BUFFER_SIZE);

  // RAII guard ensures OpenGL vertex array state is disabled even on early
  // returns
  GLVertexArrayGuard glGuard;

  // Calculate basic stroke metrics
  const TThickPoint tp0 = tmpStroke->getThickPoint(0.0);
  const TThickPoint tp1 = tmpStroke->getThickPoint(1.0);
  const double d01      = tdistance(tp0, tp1);
  if (d01 <= 0.0) return;

  // Determine number of subdivisions for smooth rendering
  const int baseDivisionsPerUnit = 5;
  int divAmount                  = static_cast<int>(d01) * baseDivisionsPerUnit;
  divAmount                      = std::min(divAmount, V_BUFFER_SIZE - 3);

  // Calculate speed vectors at start and end for extension directions
  const TPointD speed0 = tmpStroke->getSpeed(0.0);
  const TPointD speed1 = tmpStroke->getSpeed(1.0);
  if (norm2(speed0) == 0.0 || norm2(speed1) == 0.0) return;

  const TPointD startVec = -normalize(speed0);
  const TPointD endVec   = normalize(speed1);

  // Center index for symmetric width distribution
  const int centerId = (count - 1) / 2;

  // Draw each parallel line
  for (int i = 0; i < count; ++i) {
    // Width ratio ranges from -1.0 (leftmost) to 1.0 (rightmost)
    const double widthRatio =
        (count == 1) ? 0.0 : (i - centerId) / static_cast<double>(centerId);

    // Determine if this is the center line (optimization: no normal calculation
    // needed)
    const bool isCenterLine = (widthRatio == 0.0);

    // Extension length varies based on position (longer for center lines)
    const double extensionLength =
        (1.0 - std::abs(widthRatio)) * maxThickness * m_extension;

    // Set line color (all lines use the same color)
    glColor4ub(color.r, color.g, color.b, color.m);

    // Pointer to fill the vertex buffer
    float2 *vp = vertBuf.data();

    // Generate vertices for this line
    for (int j = 0; j <= divAmount; ++j) {
      const double t = j / static_cast<double>(divAmount);
      const double w = t;

      // Get position on the stroke at parameter w
      const TPointD position = tmpStroke->getPoint(w);

      // Calculate offset position based on width ratio
      TPointD offsetPos = position;

      // Only calculate normal offset if not the center line
      if (!isCenterLine) {
        const TPointD tangent = normalize(tmpStroke->getSpeed(w));
        const TPointD normal  = rotate90(tangent);
        offsetPos += normal * maxThickness * widthRatio;
      }

      // Add extension at the start of the line
      if (j == 0) {
        const TPointD startPos = offsetPos + startVec * extensionLength;
        *(vp++)                = {static_cast<float>(startPos.x),
                                  static_cast<float>(startPos.y)};
      }

      // Main vertex position
      *(vp++) = {static_cast<float>(offsetPos.x),
                 static_cast<float>(offsetPos.y)};

      // Add extension at the end of the line
      if (j == divAmount) {
        const TPointD endPos = offsetPos + endVec * extensionLength;
        *vp = {static_cast<float>(endPos.x), static_cast<float>(endPos.y)};
      }
    }

    // Draw this line as a GL line strip
    glVertexPointer(2, GL_FLOAT, 0, vertBuf.data());
    glDrawArrays(GL_LINE_STRIP, 0, divAmount + 3);
  }

  // Restore default color (optional but good practice)
  glColor4d(0.0, 0.0, 0.0, 1.0);
}

//=============================================================================

TRectD FlowLineStrokeStyle::getStrokeBBox(const TStroke *stroke) const {
  // Get the base bounding box of the stroke
  TRectD rect = TColorStyle::getStrokeBBox(stroke);

  // Calculate margin based on maximum thickness and extension parameter
  double margin =
      getMaxThickness(stroke) * m_widthScale * std::max(1.0, m_extension);

  // Return enlarged bounding box
  return rect.enlarge(margin);
}