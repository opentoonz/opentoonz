

#include "tregion.h"
#include "tregionoutline.h"
#include "tellipticbrushP.h"

#include "tstrokeoutline.h"

using namespace tellipticbrush;

#define USE_LENGTH
//#define DEBUG_DRAW_TANGENTS

//********************************************************************************
//    EXPLANATION
//********************************************************************************

/*! \file tellipticbrush.cpp
\brief This code performs the outlinization of a \a brush stroke with respect to
a
       secondary \a path stroke. This is used to draw Adobe Illustrator-like
       vectors whose brush is itself a custom vector (and by extension, a
complex
       vector image).

       Generalization: Introduce a repeat % and a superposition % in the
algorithm
*/

/* TECHNICAL:

  We have two strokes: one is the 'guideline', the other is the 'brush', and the
  purpose of this algorithm is that of bending the brush to follow the
  guideline.

  The brush is supposed to be lying horizontally, inside a rectangle
  corresponding
  to the bounding box of the image containing the brush.
  The line segment connecting the midpoints of the vertical image bbox edges
  maps
  to the guideline's centerline.
  Such mapping makes [bbox.x0, bbox.x1] -> [0, 1] in a linear fashion, where the
  image interval represents the parametric space of the guideline.

  Each vertical scanline of the bbox is further mapped to the segment extruding
  from
  the guideline's centerline along its normal (multiplied in length by the
  guideline's thickness).
*/

//********************************************************************************
//    Geometric helpers
//********************************************************************************

namespace {

double getX(const TThickPoint &P0, const TThickPoint &P1, const TThickPoint &P2,
            double t) {
  double one_t = 1.0 - t;
  return P0.x * sq(one_t) + 2.0 * P1.x * t * one_t + P2.x * sq(t);
}

//--------------------------------------------------------------------------------------------

TPointD normal(const TPointD &p, bool left) {
  TPointD n(-p.y, p.x);
  return (1.0 / norm(n)) * (left ? n : -n);
}

//--------------------------------------------------------------------------------------------

TPointD normal(const TPointD &p) {
  return (1.0 / norm(p)) * TPointD(-p.y, p.x);
}

//--------------------------------------------------------------------------------------------

void getHRange(const TThickQuadratic &ttq, double &x0, double &x1) {
  const TPointD &P0 = ttq.getP0();
  const TPointD &P1 = ttq.getP1();
  const TPointD &P2 = ttq.getP2();

  // Get the horizontal range of the chunk
  x0 = std::min({x0, P0.x, P2.x}), x1 = std::max({x1, P0.x, P2.x});

  double t = (P0.x - P1.x) / (P0.x + P2.x - 2.0 * P1.x);
  if (t > 0.0 && t < 1.0) {
    double x = getX(P0, P1, P2, t);
    x0 = std::min(x0, x), x1 = std::max(x1, x);
  }
}

void getHRange(const TThickQuadratic &ttq, double t0, double t1, double &x0,
               double &x1) {
  const TPointD &P0 = ttq.getP0();
  const TPointD &P1 = ttq.getP1();
  const TPointD &P2 = ttq.getP2();

  double x0_ = getX(P0, P1, P2, t0);
  double x1_ = getX(P0, P1, P2, t1);

  // Get the horizontal range of the chunk
  x0 = std::min({x0, x0_, x1_}), x1 = std::max({x1, x0_, x1_});

  double t = (P0.x - P1.x) / (P0.x + P2.x - 2.0 * P1.x);
  if (t > t0 && t < t1) {
    double x = getX(P0, P1, P2, t);
    x0 = std::min(x0, x), x1 = std::max(x1, x);
  }
}

void getHRange(const TStroke &stroke, double &x0, double &x1) {
  int i, nChunks = stroke.getChunkCount();
  for (i = 0; i < nChunks; ++i) getHRange(*stroke.getChunk(i), x0, x1);
}

//********************************************************************************
//    Outlinization Data
//********************************************************************************

struct StrokeOutlinizationData final
    : public tellipticbrush::OutlinizationData {
  double m_x0, m_x1, m_xRange;
  double m_y0, m_yScale;

public:
  StrokeOutlinizationData() : OutlinizationData() {}

  StrokeOutlinizationData(const TStroke &stroke, const TRectD &strokeBox,
                          const TOutlineUtil::OutlineParameter &options)
      : OutlinizationData(options)
      , m_x0(strokeBox.x0)
      , m_x1(strokeBox.x1)
      , m_xRange(m_x1 - m_x0)
      , m_y0(0.5 * (strokeBox.y0 + strokeBox.y1))
      , m_yScale(1.0 / (strokeBox.y1 - strokeBox.y0)) {}

  void buildPoint(const CenterlinePoint &p, bool isNextD, CenterlinePoint &ref,
                  bool isRefNextD, CenterlinePoint &out);
  int buildPoints(const CenterlinePoint &p, CenterlinePoint &ref,
                  CenterlinePoint *out);
  int buildPoints(const TStroke &stroke, const TStroke &path,
                  CenterlinePoint &cp, CenterlinePoint *out);

  bool getChunkAndT_param(const TStroke &path, double x, int &chunk, double &t);
  bool getChunkAndT_length(const TStroke &path, double x, int &chunk,
                           double &t);

  double toW(double x);
};

//--------------------------------------------------------------------------------------------

double StrokeOutlinizationData::toW(double x) {
  return tcrop((x - m_x0) / m_xRange, 0.0, 1.0);
}

//--------------------------------------------------------------------------------------------

bool StrokeOutlinizationData::getChunkAndT_param(const TStroke &path, double x,
                                                 int &chunk, double &t) {
  double w = toW(x);
  return !path.getChunkAndT(w, chunk, t);
}

//--------------------------------------------------------------------------------------------

bool StrokeOutlinizationData::getChunkAndT_length(const TStroke &path, double x,
                                                  int &chunk, double &t) {
  double s = toW(x) * path.getLength();
  return !path.getChunkAndTAtLength(s, chunk, t);
}

//--------------------------------------------------------------------------------------------

void StrokeOutlinizationData::buildPoint(const CenterlinePoint &p, bool pNextD,
                                         CenterlinePoint &ref, bool refNextD,
                                         CenterlinePoint &out) {
  TThickPoint &refD = refNextD ? ref.m_nextD : ref.m_prevD;

  const TThickPoint *pD;
  TThickPoint *outD;
  bool *outHasD;
  if (pNextD) {
    pD      = &p.m_nextD;
    outD    = &out.m_nextD;
    outHasD = &out.m_hasNextD;
  } else {
    pD      = &p.m_prevD;
    outD    = &out.m_prevD;
    outHasD = &out.m_hasPrevD;
  }

  // Build position
  refD = (1.0 / norm(refD)) * refD;
  TPointD normalDirection(-refD.y, refD.x);

  double yPercentage = (p.m_p.y - m_y0) * m_yScale;
  double yRelative   = yPercentage * ref.m_p.thick;
  double yFactor     = ref.m_p.thick * m_yScale;

  out.m_p = TThickPoint(ref.m_p.x + yRelative * normalDirection.x,
                        ref.m_p.y + yRelative * normalDirection.y,
                        p.m_p.thick * yFactor);

  // Build direction
  double stretchedDY = pD->x * yPercentage * refD.thick + pD->y * yFactor;

  *outD = TThickPoint(refD.x * pD->x - refD.y * stretchedDY,
                      refD.y * pD->x + refD.x * stretchedDY,
                      pD->thick * (1.0 + refD.thick));

  bool covered  = (sq(outD->x) + sq(outD->y) < sq(outD->thick) + tolPar);
  out.m_covered = out.m_covered && covered;

  *outHasD = *outHasD && !covered;
}

//--------------------------------------------------------------------------------------------

/*    EXPLANATION:

        \ _   \         The path stroke with centerline point C has 2 outward
     \      \  \        directions (prevD and nextD), which may be different.
      \      |* \       Therefore, there may also be 2 different envelope
   directions
       \   * .   \      from C (the *s in the drawing).
        C    .    |     When the brush stroke 'hits' one envelope direction, it
       /   * .   /      transfers on the other e.d., and continues on that side.
      /   _ / * /
     /  /      /
       |      /
*/

//! Build points resulting from the association between p (must have pos and
//! dirs
//! already built) and ref, returning the number of output points stored in
//! out[] (at max 2).
int StrokeOutlinizationData::buildPoints(const CenterlinePoint &p,
                                         CenterlinePoint &ref,
                                         CenterlinePoint *out) {
  out[0] = out[1]  = p;
  out[0].m_covered = out[1].m_covered = true;  // Coverage is rebuilt

  bool refSymmetric =
      ref.m_hasPrevD && ref.m_hasNextD && ref.m_nextD == ref.m_prevD;
  bool pSymmetric = p.m_hasPrevD && p.m_hasNextD && p.m_nextD == p.m_prevD;

  // Build prev
  bool prevSideIsNext =
      (p.m_prevD.x < 0) ? true : (p.m_prevD.x > 0) ? false : ref.m_hasNextD;
  bool hasPrev =
      p.m_hasPrevD && (prevSideIsNext ? ref.m_hasNextD : ref.m_hasPrevD);
  int prevIdx = hasPrev ? 0 : -1;

  if (hasPrev) {
    CenterlinePoint &outPoint = out[prevIdx];
    buildPoint(p, false, ref, prevSideIsNext, outPoint);
  }

  if (refSymmetric && pSymmetric) {
    // Copy prev to next
    if (hasPrev) {
      CenterlinePoint &outPoint = out[prevIdx];

      outPoint.m_hasNextD = outPoint.m_hasPrevD;
      outPoint.m_nextD    = outPoint.m_prevD;

      return 1;
    }

    return 0;
  }

  // Build next
  bool nextSideIsNext =
      (p.m_nextD.x > 0) ? true : (p.m_nextD.x < 0) ? false : ref.m_hasNextD;
  bool hasNext =
      p.m_hasNextD && (nextSideIsNext ? ref.m_hasNextD : ref.m_hasPrevD);
  int nextIdx =
      hasNext ? hasPrev ? ((int)prevSideIsNext != nextSideIsNext) : 0 : -1;

  if (hasNext) {
    CenterlinePoint &outPoint = out[nextIdx];
    buildPoint(p, true, ref, nextSideIsNext, outPoint);
  }

  // Fill in unbuilt directions if necessary
  if (hasPrev && hasNext && prevIdx != nextIdx) {
    CenterlinePoint &outPrev = out[prevIdx];
    CenterlinePoint &outNext = out[nextIdx];

    if (dist(outPrev.m_p, outNext.m_p) > 1e-4) {
      // If there are 2 full output points, make their unbuilt directions match
      outPrev.m_nextD = outNext.m_prevD = 0.5 * (outNext.m_p - outPrev.m_p);
      bool covered = (sq(outPrev.m_nextD.x) + sq(outPrev.m_nextD.y) <
                      sq(outPrev.m_nextD.thick) + tolPar);

      outPrev.m_hasNextD = outNext.m_hasPrevD = !covered;
      outPrev.m_covered                       = outPrev.m_covered && covered;
      outNext.m_covered                       = outNext.m_covered && covered;
    } else {
      // Merge the 2 existing ones
      nextIdx = prevIdx;

      outPrev.m_nextD    = outNext.m_nextD;
      outPrev.m_hasNextD = outNext.m_hasPrevD;
      outPrev.m_covered  = outPrev.m_covered && outNext.m_covered;
    }
  }

  return std::max(prevIdx, nextIdx) + 1;
}

//--------------------------------------------------------------------------------------------

int StrokeOutlinizationData::buildPoints(const TStroke &stroke,
                                         const TStroke &path,
                                         CenterlinePoint &cp,
                                         CenterlinePoint *out) {
  const TThickQuadratic &ttq = *stroke.getChunk(cp.m_chunkIdx);

  const TThickPoint &P0 = ttq.getP0();
  const TThickPoint &P1 = ttq.getP1();
  const TThickPoint &P2 = ttq.getP2();

  double x = getX(P0, P1, P2, cp.m_t);

  double pathT;
  int pathChunk;
#ifdef USE_LENGTH
  bool ok = getChunkAndT_length(path, x, pathChunk, pathT);
#else
  bool ok = getChunkAndT_param(path, x, pathChunk, pathT);
#endif
  assert(ok);

  CenterlinePoint pathCp(pathChunk, pathT);

  cp.buildPos(stroke);
  cp.buildDirs(stroke);
  pathCp.buildPos(path);
  pathCp.buildDirs(path);

  return buildPoints(cp, pathCp, out);
}

//********************************************************************************
//    Path-Altered Brush Linearizator (base class)
//********************************************************************************

class ReferenceLinearizator : public tellipticbrush::StrokeLinearizator {
protected:
  const TStroke *m_path;
  StrokeOutlinizationData m_data;

public:
  ReferenceLinearizator(const TStroke *stroke, const TStroke *path,
                        const StrokeOutlinizationData &data);

  virtual void linearize(std::vector<CenterlinePoint> &cPoints, int chunk,
                         double t1) = 0;
};

//--------------------------------------------------------------------------------------------

ReferenceLinearizator::ReferenceLinearizator(
    const TStroke *stroke, const TStroke *path,
    const StrokeOutlinizationData &data)
    : StrokeLinearizator(stroke), m_path(path), m_data(data) {}

//********************************************************************************
//    Brush Linearizator on Path inter-chunk points
//********************************************************************************

class ReferenceChunksLinearizator final : public ReferenceLinearizator {
  double m_w0, m_w1;

public:
  ReferenceChunksLinearizator(const TStroke *stroke, const TStroke *path,
                              const StrokeOutlinizationData &data)
      : ReferenceLinearizator(stroke, path, data) {}

  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) override;
  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk,
                 double t1) override;

  void addCenterlinePoints(std::vector<CenterlinePoint> &cPoints,
                           int brushChunk, double x0, double x1);
  void addCenterlinePoints(std::vector<CenterlinePoint> &cPoints,
                           int strokeChunk, double strokeT, int refChunk);
};

//--------------------------------------------------------------------------------------------

void ReferenceChunksLinearizator::linearize(
    std::vector<CenterlinePoint> &cPoints, int chunk) {
  // Get the stroke chunk
  const TThickQuadratic &ttq = *this->m_stroke->getChunk(chunk);

  // Get the chunk's horizontal range
  double x0 = (std::numeric_limits<double>::max)(), x1 = -x0;
  getHRange(ttq, x0, x1);

  // Now, we have to add all points corresponding to the intersections between
  // the relative
  // vertical projection of path's chunk endpoints, and the stroke
  addCenterlinePoints(cPoints, chunk, x0, x1);
}

//--------------------------------------------------------------------------------------------

void ReferenceChunksLinearizator::linearize(
    std::vector<CenterlinePoint> &cPoints, int chunk, double t1) {
  if (cPoints.empty()) return;

  // Get the stroke chunk
  const TThickQuadratic &ttq = *this->m_stroke->getChunk(chunk);

  // Get the chunk's horizontal range
  double x0 = (std::numeric_limits<double>::max)(), x1 = -x0;
  getHRange(ttq, cPoints[0].m_t, t1, x0, x1);

  // Now, we have to add all points corresponding to the intersections between
  // the relative
  // vertical projection of path's chunk endpoints, and the stroke

  addCenterlinePoints(cPoints, chunk, x0, x1);
}

//--------------------------------------------------------------------------------------------

void ReferenceChunksLinearizator::addCenterlinePoints(
    std::vector<CenterlinePoint> &cPoints, int chunk, double x0, double x1) {
  const TThickQuadratic &ttq = *this->m_stroke->getChunk(chunk);
  const TStroke &path        = *this->m_path;

  int chunk0, chunk1;
  double t0, t1;

#ifdef USE_LENGTH
  bool ok0 = m_data.getChunkAndT_length(path, x0, chunk0, t0);
  bool ok1 = m_data.getChunkAndT_length(path, x1, chunk1, t1);
#else
  bool ok0   = m_data.getChunkAndT_param(path, x0, chunk0, t0);
  bool ok1   = m_data.getChunkAndT_param(path, x1, chunk1, t1);
#endif

  assert(ok0 && ok1);

  const TPointD &P0 = ttq.getP0();
  const TPointD &P1 = ttq.getP1();
  const TPointD &P2 = ttq.getP2();

  double A      = P0.x + P2.x - 2.0 * P1.x;
  double B      = P1.x - P0.x;
  double delta_ = sq(B) - P0.x * A;  // actual delta = delta_ + x * A;

  int i, initialSize = cPoints.size();
  for (i = chunk0; i < chunk1; ++i) {
#ifdef USE_LENGTH
    double s = std::min(path.getLength(i, 1.0) / path.getLength(), 1.0);
    double x = m_data.m_x0 + m_data.m_xRange * s;
#else
    double w = path.getW(i, 1.0);
    double x = m_data.m_x0 + m_data.m_xRange * w;
#endif

    double delta = delta_ + x * A;
    if (delta < 0) continue;

    // Add first solution
    double t = (sqrt(delta) - B) / A;
    if (t > 0.0 && t < 1.0)  // 0 and 1 are dealt outside
      addCenterlinePoints(cPoints, chunk, t, i);

    if (delta < tolPar) continue;

    // Add second solution
    t = -(sqrt(delta) + B) / A;
    if (t > 0.0 && t < 1.0) addCenterlinePoints(cPoints, chunk, t, i);
  }

  // As points may be mixed (by parameter), sort them.
  std::sort(cPoints.begin() + initialSize, cPoints.end());
}

//--------------------------------------------------------------------------------------------

void ReferenceChunksLinearizator::addCenterlinePoints(
    std::vector<CenterlinePoint> &cPoints, int strokeChunk, double strokeT,
    int refChunk) {
  CenterlinePoint p(strokeChunk, strokeT);
  CenterlinePoint ref(refChunk, 1.0);

  CenterlinePoint newPoints[2];

  p.buildPos(*m_stroke);
  p.buildDirs(*m_stroke);
  ref.buildPos(*m_path);
  ref.buildDirs(*m_path);

  int i, count = m_data.buildPoints(p, ref, newPoints);
  for (i = 0; i < count; ++i) cPoints.push_back(newPoints[i]);
}

//********************************************************************************
//    Recursive (regular) Reference Stroke Linearizator
//********************************************************************************

class RecursiveReferenceLinearizator final : public ReferenceLinearizator {
public:
  typedef void (RecursiveReferenceLinearizator::*SubdivisorFuncPtr)(
      std::vector<CenterlinePoint> &cPoints, CenterlinePoint &cp0,
      CenterlinePoint &cp1);

  SubdivisorFuncPtr m_subdivisor;

public:
  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) override;
  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk,
                 double t1) override;

  void subdivide(std::vector<CenterlinePoint> &cPoints, CenterlinePoint &cp0,
                 CenterlinePoint &cp1);
  void subdivideCenterline(std::vector<CenterlinePoint> &cPoints,
                           CenterlinePoint &cp0, CenterlinePoint &cp1);

public:
  RecursiveReferenceLinearizator(const TStroke *stroke, const TStroke *path,
                                 const StrokeOutlinizationData &data)
      : ReferenceLinearizator(stroke, path, data)
      , m_subdivisor(&RecursiveReferenceLinearizator::subdivide) {}
};

//--------------------------------------------------------------------------------------------

void RecursiveReferenceLinearizator::linearize(
    std::vector<CenterlinePoint> &cPoints, int chunk) {
  linearize(cPoints, chunk, 1.0);
}

//--------------------------------------------------------------------------------------------

void RecursiveReferenceLinearizator::linearize(
    std::vector<CenterlinePoint> &cPoints, int chunk, double t1) {
  if (cPoints.empty()) return;

  const TStroke &stroke      = *this->m_stroke;
  const TThickQuadratic &ttq = *stroke.getChunk(chunk);

  const TStroke &path = *this->m_path;

  // Sort the interval (SHOULD BE DONE OUTSIDE?)
  std::stable_sort(cPoints.begin(), cPoints.end());

  std::vector<CenterlinePoint> addedPoints;

  unsigned int i, size_1 = cPoints.size() - 1;
  for (i = 0; i < size_1; ++i) {
    CenterlinePoint &cp1 = cPoints[i], cp2 = cPoints[i + 1];
    if (cp2.m_t - cp1.m_t > 1e-4)
      (this->*m_subdivisor)(addedPoints, cPoints[i], cPoints[i + 1]);
  }

  if (cPoints[size_1].m_t < t1) {
    double t, x = (t1 == 1.0) ? ttq.getP2().x
                              : getX(ttq.getP0(), ttq.getP1(), ttq.getP2(), t1);
    int refChunk;

#ifdef USE_LENGTH
    bool ok = m_data.getChunkAndT_length(path, x, refChunk, t);
#else
    bool ok  = m_data.getChunkAndT_param(path, x, refChunk, t);
#endif

    CenterlinePoint strokeCpEnd(chunk, t1);
    CenterlinePoint refCp(refChunk, t);
    CenterlinePoint newPoints[2];

    strokeCpEnd.buildPos(*m_stroke);
    strokeCpEnd.buildDirs(*m_stroke);
    refCp.buildPos(*m_path);
    refCp.buildDirs(*m_path);

    int count = m_data.buildPoints(strokeCpEnd, refCp, newPoints);
    if (count == 1)  // Otherwise, this is either impossible, or already covered
      (this->*m_subdivisor)(addedPoints, cPoints[size_1], newPoints[0]);
  }

  cPoints.insert(cPoints.end(), addedPoints.begin(), addedPoints.end());
}

//--------------------------------------------------------------------------------------------

void RecursiveReferenceLinearizator::subdivide(
    std::vector<CenterlinePoint> &cPoints, CenterlinePoint &cp0,
    CenterlinePoint &cp1) {
  if (!(cp0.m_hasNextD && cp1.m_hasPrevD)) return;

  const TStroke &stroke      = *this->m_stroke;
  const TThickQuadratic &ttq = *stroke.getChunk(cp0.m_chunkIdx);

  const TStroke &path = *this->m_path;

  // Build the distance of next from the outline of cp's 'envelope extension'

  TPointD envDirL0, envDirR0, envDirL1, envDirR1;
  buildEnvelopeDirections(cp0.m_p, cp0.m_nextD, envDirL0, envDirR0);
  buildEnvelopeDirections(cp1.m_p, cp1.m_prevD, envDirL1, envDirR1);

  TPointD diff(convert(cp1.m_p) - convert(cp0.m_p));
  double d = std::max(fabs(envDirL0 * (diff + cp1.m_p.thick * envDirL1 -
                                       cp0.m_p.thick * envDirL0)),
                      fabs(envDirR0 * (diff + cp1.m_p.thick * envDirR1 -
                                       cp0.m_p.thick * envDirR0)));

  if (d > m_data.m_pixSize && cp1.m_t - cp0.m_t > 1e-4) {
    CenterlinePoint strokeMidPoint(cp0.m_chunkIdx, 0.5 * (cp0.m_t + cp1.m_t));
    CenterlinePoint newPoints[2];

    int count = m_data.buildPoints(*this->m_stroke, *this->m_path,
                                   strokeMidPoint, newPoints);
    if (count == 1)  // Otherwise, this is either impossible, or already covered
    {
      subdivide(cPoints, cp0,
                newPoints[0]);  // should I use strokeMidPoint(Prev) here ?
      subdivide(cPoints, newPoints[0], cp1);

      cPoints.push_back(newPoints[0]);
    }
  }
}

//--------------------------------------------------------------------------------------------

void RecursiveReferenceLinearizator::subdivideCenterline(
    std::vector<CenterlinePoint> &cPoints, CenterlinePoint &cp0,
    CenterlinePoint &cp1) {
  if (cp0.m_covered || !cp0.m_hasNextD) return;

  // Build the distance of next from cp's 'direction extension'
  TPointD dir((1.0 / norm(cp0.m_nextD)) * cp0.m_nextD);
  TPointD diff(convert(cp1.m_p) - convert(cp0.m_p));

  double d = fabs(dir.x * diff.y - dir.y * diff.x);

  if (d > m_data.m_pixSize && cp1.m_t - cp0.m_t > 1e-4) {
    CenterlinePoint strokeMidPoint(cp0.m_chunkIdx, 0.5 * (cp0.m_t + cp1.m_t));
    CenterlinePoint newPoints[2];

    int count = m_data.buildPoints(*this->m_stroke, *this->m_path,
                                   strokeMidPoint, newPoints);
    if (count == 1)  // Otherwise, this is either impossible, or already covered
    {
      subdivide(cPoints, cp0,
                newPoints[0]);  // should I use strokeMidPoint(Prev) here ?
      subdivide(cPoints, newPoints[0], cp1);

      cPoints.push_back(newPoints[0]);
    }
  }
}

//********************************************************************************
//    Make Outline Implementation (stroke version)
//********************************************************************************

//********************************************************************************
//    Make Outline Implementation (stroke version)
//********************************************************************************

/*
  Quick container to store all the linearization features to be supported.
  \note The set should be appropriately ordered so that linearizator dependance
  can be supported (linearizators may work depending on knowledge of the other
  linearized points)
*/
struct LinearizatorsSet {
  static const int nLinearizators = 2;

  ReferenceChunksLinearizator m_refChunksLinearizator;
  RecursiveReferenceLinearizator m_recursiveRefLinearizator;

  ReferenceLinearizator *m_linearizatorPtrs[nLinearizators];

public:
  LinearizatorsSet(const TStroke &stroke, const TStroke &path,
                   const StrokeOutlinizationData &data)
      : m_refChunksLinearizator(&stroke, &path, data)
      , m_recursiveRefLinearizator(&stroke, &path, data) {
    m_linearizatorPtrs[0] = &m_refChunksLinearizator;
    m_linearizatorPtrs[1] = &m_recursiveRefLinearizator;
  }

  ReferenceLinearizator *operator[](int i) { return m_linearizatorPtrs[i]; }
  const int size() const { return nLinearizators; }
};

}  // namespace

//============================================================================================

void TOutlineUtil::makeOutline(const TStroke &path, const TStroke &stroke,
                               const TRectD &strokeBox, TStrokeOutline &outline,
                               const TOutlineUtil::OutlineParameter &options) {
  // Build outlinization data
  StrokeOutlinizationData data(stroke, strokeBox, options);

  // Build a set of linearizators for the specified stroke
  LinearizatorsSet linearizators(stroke, path, data);
  CenterlinePoint newPoints[2];

  std::vector<CenterlinePoint> cPoints, chunkPoints;
  int i, chunksCount = stroke.getChunkCount();
  for (i = 0; i < chunksCount; ++i) {
    chunkPoints.clear();

    CenterlinePoint cp(i, 0.0);
    int j, count = data.buildPoints(stroke, path, cp, newPoints);
    for (j = 0; j < count; ++j) chunkPoints.push_back(newPoints[j]);

    int linearsCount = linearizators.size();
    for (j = 0; j < linearsCount; ++j) {
      StrokeLinearizator *linearizator = linearizators[j];
      linearizator->linearize(chunkPoints, i);
    }

    // These points are just PUSH_BACK'd to the vector. A sorting must be
    // performed
    // before storing them in the overall centerline points vector
    std::stable_sort(chunkPoints.begin(), chunkPoints.end());

    cPoints.insert(cPoints.end(), chunkPoints.begin(), chunkPoints.end());
  }

  // Build the final point.
  CenterlinePoint cPoint(chunksCount - 1, 1.0);

  int count = data.buildPoints(stroke, path, cPoint, newPoints);
  for (i = 0; i < count; ++i) cPoints.push_back(newPoints[i]);

  // If no centerline point was built, no outline point can, too.
  // This specifically happens when the supplied path is a point.
  if (cPoints.empty()) return;

  // In the selfLoop case, use its info to modify the initial point.
  if (stroke.isSelfLoop()) {
    CenterlinePoint &lastCp = cPoints[cPoints.size() - 1];

    cPoints[0].m_prevD    = cPoint.m_prevD;
    cPoints[0].m_hasPrevD = true;
    lastCp.m_nextD        = cPoint.m_prevD;
    lastCp.m_hasNextD     = true;
  }

#ifdef DEBUG_DRAW_TANGENTS
  {
    // Debug - draw centerline directions (derivatives)
    glBegin(GL_LINES);
    glColor3d(1.0, 0.0, 0.0);

    unsigned int i, size = cPoints.size();
    for (i = 0; i < size; ++i) {
      glVertex2d(cPoints[i].m_p.x, cPoints[i].m_p.y);
      glVertex2d(
          cPoints[i].m_p.x + cPoints[i].m_nextD.x * cPoints[i].m_p.thick,
          cPoints[i].m_p.y + cPoints[i].m_nextD.y * cPoints[i].m_p.thick);
    }

    glEnd();
  }
#endif

  // Now, build the outline associated to the linearized centerline
  buildOutline(stroke, cPoints, outline, data);
}

//============================================================================================

/*
TRectD TOutlineUtil::computeBBox(const TStroke &stroke, const TStroke& path)
{
  typedef TStroke::OutlineOptions OOpts;

  //First, calculate the usual stroke bbox
  TRectD roundBBox(::computeBBox(stroke));
  const OOpts& oOptions(stroke.outlineOptions());

  if(oOptions.m_capStyle != OOpts::PROJECTING_CAP && oOptions.m_joinStyle !=
OOpts::MITER_JOIN)
    return roundBBox;

  //Build interesting centerline points (in this case, junction points)
  std::vector<CenterlinePoint> cPoints;
  int i, chunksCount = stroke.getChunkCount();
  for(i=0; i<chunksCount; ++i)
  {
    CenterlinePoint cPoint(i, 0.0);

    cPoint.buildPos(stroke);
    cPoint.buildDirs(stroke);
    cPoints.push_back(cPoint);
  }

  //Build the final point.
  CenterlinePoint cPoint(chunksCount-1, 1.0);

  cPoint.buildPos(stroke);
  cPoint.buildDirs(stroke);

  //In the selfLoop case, use its info to modify the initial point.
  if(stroke.isSelfLoop())
  {
    cPoints[0].m_prevD = cPoint.m_prevD;
    cPoints[0].m_hasPrevD = true;
    cPoint.m_nextD = cPoint.m_prevD;
    cPoint.m_hasNextD = true;
  }

  cPoints.push_back(cPoint);

  //Now, add the associated 'extending' outline points
  OutlineBuilder outBuilder(OutlinizationData(), stroke);
  TRectD extensionBBox(
    (std::numeric_limits<double>::max)(),
    (std::numeric_limits<double>::max)(),
    -(std::numeric_limits<double>::max)(),
    -(std::numeric_limits<double>::max)()
  );

  unsigned int j, cPointsCount = cPoints.size();
  for(j=0; ; ++j)
  {
    //Search the next uncovered point
    for(; j < cPointsCount && cPoints[j].m_covered; ++j)
      ;

    if(j >= cPointsCount)
      break;

    //Build the associated outline points
    outBuilder.buildOutlineExtensions(extensionBBox, cPoints[j]);
  }

  //Finally, merge the 2 bboxes
  return roundBBox + extensionBBox;
}
*/

//============================================================================================

namespace {

void makeCenterline(const TStroke &path, const TEdge &edge,
                    const TRectD &regionBox, std::vector<T3DPointD> &outline) {
  int initialOutlineSize = outline.size();

  static const TOutlineUtil::OutlineParameter options;
  const TStroke &stroke = *edge.m_s;

  double w0 = edge.m_w0, w1 = edge.m_w1;
  bool reversed    = w1 < w0;
  if (reversed) w0 = edge.m_w1, w1 = edge.m_w0;

  // Build outlinization data
  StrokeOutlinizationData data(stroke, regionBox, options);

  // Build a set of linearizators for the specified stroke
  LinearizatorsSet linearizators(stroke, path, data);
  CenterlinePoint newPoints[2];

  linearizators.m_recursiveRefLinearizator.m_subdivisor =
      &RecursiveReferenceLinearizator::subdivideCenterline;

  std::vector<CenterlinePoint> chunkPoints;

  int i, chunk0, chunk1;
  double t0, t1;

  bool ok0 = !edge.m_s->getChunkAndT(w0, chunk0, t0);
  bool ok1 = !edge.m_s->getChunkAndT(w1, chunk1, t1);

  assert(ok0 && ok1);

  double tStart = t0;
  for (i = chunk0; i < chunk1; ++i, tStart = 0.0) {
    chunkPoints.clear();

    CenterlinePoint cp(i, tStart);
    int j, count = data.buildPoints(stroke, path, cp, newPoints);
    for (j = 0; j < count; ++j) chunkPoints.push_back(newPoints[j]);

    int linearsCount = linearizators.size();
    for (j = 0; j < linearsCount; ++j) {
      ReferenceLinearizator *linearizator = linearizators[j];
      linearizator->linearize(chunkPoints, i, 1.0);
    }

    // These points are just PUSH_BACK'd to the vector. A sorting must be
    // performed
    // before storing them in the overall centerline points vector
    std::stable_sort(chunkPoints.begin(), chunkPoints.end());

    int size = chunkPoints.size();
    outline.reserve(outline.size() + size);

    for (j = 0; j < size; ++j) {
      const TPointD &point = chunkPoints[j].m_p;
      outline.push_back(T3DPointD(point.x, point.y, 0.0));
    }
  }

  // The last one with t1 as endpoint
  {
    chunkPoints.clear();

    CenterlinePoint cp(chunk1, tStart);
    int j, count = data.buildPoints(stroke, path, cp, newPoints);
    for (j = 0; j < count; ++j) chunkPoints.push_back(newPoints[j]);

    int linearsCount = linearizators.size();
    for (j = 0; j < linearsCount; ++j) {
      ReferenceLinearizator *linearizator = linearizators[j];
      linearizator->linearize(chunkPoints, chunk1, t1);
    }

    std::stable_sort(chunkPoints.begin(), chunkPoints.end());

    int size = chunkPoints.size();
    outline.reserve(outline.size() + size);

    for (j = 0; j < size; ++j) {
      const TPointD &point = chunkPoints[j].m_p;
      outline.push_back(T3DPointD(point.x, point.y, 0.0));
    }
  }

  // Finally, add the endpoint
  CenterlinePoint cp(chunk1, t1);
  int j, count = data.buildPoints(stroke, path, cp, newPoints);
  for (j = 0; j < count; ++j) {
    const TPointD &point = newPoints[j].m_p;
    outline.push_back(T3DPointD(point.x, point.y, 0.0));
  }

  // Eventually, reverse the output
  if (reversed)
    std::reverse(outline.begin() + initialOutlineSize, outline.end());
}

//--------------------------------------------------------------------------------------------

void makeOutlineRaw(const TStroke &path, const TRegion &region,
                    const TRectD &regionBox, std::vector<T3DPointD> &outline) {
  // Deal with each edge independently
  int e, edgesCount = region.getEdgeCount();
  for (e = 0; e < edgesCount; ++e)
    makeCenterline(path, *region.getEdge(e), regionBox, outline);
}

}  // namespace

//--------------------------------------------------------------------------------------------

void TOutlineUtil::makeOutline(const TStroke &path, const TRegion &region,
                               const TRectD &regionBox,
                               TRegionOutline &outline) {
  outline.m_doAntialiasing = true;

  // Build the external boundary
  {
    outline.m_exterior.resize(1);
    outline.m_exterior[0].clear();

    makeOutlineRaw(path, region, regionBox, outline.m_exterior[0]);
  }

  // Build internal boundaries
  {
    outline.m_interior.clear();

    int i, subRegionNumber = region.getSubregionCount();
    outline.m_interior.resize(subRegionNumber);

    for (i = 0; i < subRegionNumber; i++)
      makeOutlineRaw(path, *region.getSubregion(i), regionBox,
                     outline.m_interior[i]);
  }

  outline.m_bbox = region.getBBox();
}
