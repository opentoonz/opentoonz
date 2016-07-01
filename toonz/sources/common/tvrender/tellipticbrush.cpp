

// Toonz components includes
#include "tcurveutil.h"
#include "tinterval.h"

#include "tellipticbrushP.h"
#include "tstrokeoutline.h"

using namespace tellipticbrush;

//********************************************************************************
//    EXPLANATION
//********************************************************************************

/*! \file tellipticbrush.cpp
This code deals with the "outlinization" process of a TStroke instance.

The process of extracing the outline of a thick stroke can be resumed in 2 main
steps:

  1. Discretize the stroke centerline in the most appropriate centerline points,
     extracting infos about position and left/right derivatives at each.

  2. Build the outline points associated to each individual centerline point;
     eventually including additional junction points and caps.

The first major step has some sub-routines worth noting:

  1.1 Isolate regions of the stroke where the thickness speed is greater
      than the gemoetrical speed of the centerline. These points are
'self-covered'
      by their immediate neighbourhood, and thus cannot be seen - or build
outline directions.
  1.2 Some procedural style need to sample the centerline at a given length
step.
  1.3 The centerline should be sampled so that the resulting polygonal outline
      approximation is tightly close to the theoretical outline, up to an error
bound.
      The recursive approach is the simplest to deal with this issue.

The second step implements different outline styles to extrude the centerline
points.
*/

//********************************************************************************
//    Geometric Helper Functions
//********************************************************************************

//! Returns the distance between two points
double tellipticbrush::dist(const TPointD &P1, const TPointD &P2) {
  return norm(P2 - P1);
}

//------------------------------------------------------------

//! Returns the distance between two points
double tellipticbrush::dist(const TThickPoint &P1, const TThickPoint &P2) {
  return norm(P2 - P1);
}

//------------------------------------------------------------

//! Returns the angle between (unnormalized) vectors v1 and v2
double tellipticbrush::angle(const TPointD &v1, const TPointD &v2) {
  TPointD d1(v1 * (1.0 / norm(v1))), d2(v2 * (1.0 / norm(v2)));
  return atan2(cross(v1, v2), v1 * v2);
}

//------------------------------------------------------------

/*!
  Returns the intersection between two lines in the form of \b coordinates
  from a pair of the lines' starting points. Passed directions must have norm 1.

  If the system's determinant modulus is under the specified tolerance
  parameter,
  TConsts::napd is returned.
*/
TPointD tellipticbrush::intersectionCoords(const TPointD &P0, const TPointD &d0,
                                           const TPointD &P1, const TPointD &d1,
                                           double detTol) {
  // Solve P0 + x * d0 == P1 + y * d1

  double det = d0.y * d1.x - d0.x * d1.y;
  if (fabs(det) < detTol) return TConsts::napd;

  TPointD P1_P0(P1 - P0);
  return TPointD((d1.x * P1_P0.y - d1.y * P1_P0.x) / det,
                 (d0.x * P1_P0.y - d0.y * P1_P0.x) / det);
}

//------------------------------------------------------------

/*!
  Returns the left or right envelope direction of centerline point p against
  thick direction d.
*/
void tellipticbrush::buildEnvelopeDirection(const TThickPoint &p,
                                            const TThickPoint &d, bool left,
                                            TPointD &res) {
  double dNorm2 = sq(d.x) + sq(d.y);

  double a = -d.thick / dNorm2;
  double b = sqrt(dNorm2 - sq(d.thick)) / dNorm2;

  TPointD n(left ? TPointD(-d.y, d.x) : TPointD(d.y, -d.x));
  res = a * TPointD(d.x, d.y) + b * n;
}

//------------------------------------------------------------

void tellipticbrush::buildEnvelopeDirections(const TThickPoint &p,
                                             const TThickPoint &d,
                                             TPointD &resL, TPointD &resR) {
  double dNorm2 = sq(d.x) + sq(d.y);

  double a = -d.thick / dNorm2;
  double b = sqrt(dNorm2 - sq(d.thick)) / dNorm2;

  TPointD n(-d.y, d.x);
  resL = a * TPointD(d.x, d.y) + b * n;
  resR = a * TPointD(d.x, d.y) - b * n;
}

//------------------------------------------------------------

/*!
  Extrudes centerline point p against thick direction d, returning its left or
  right
  envelope displacement vector.
*/
void tellipticbrush::buildEnvelopeVector(const TThickPoint &p,
                                         const TThickPoint &d, bool left,
                                         TPointD &res) {
  buildEnvelopeDirection(p, d, left, res);
  res.x = p.thick * res.x;
  res.y = p.thick * res.y;
}

//------------------------------------------------------------

void tellipticbrush::buildEnvelopeVectors(const TThickPoint &p,
                                          const TThickPoint &d, TPointD &resL,
                                          TPointD &resR) {
  buildEnvelopeDirections(p, d, resL, resR);
  resL.x = p.thick * resL.x;
  resL.y = p.thick * resL.y;
  resR.x = p.thick * resR.x;
  resR.y = p.thick * resR.y;
}

//------------------------------------------------------------

/*!
  Builds the angle that supports a *quality* discretization of the circle
  with maximal error < m_pixSize.
*/
void tellipticbrush::buildAngularSubdivision(double radius, double angle,
                                             double err, int &nAngles) {
  /*
See "Graphic Gems", page 600.

NOTE: maxAngle is not multiplied by 2.0 as the naive pythagorical
argument would pretend. The 2.0 holds if we want to find the angle
at which the distance of the circle from its approximation is always < error.

But we want MORE. We want that to happen against the distance from EVERY
TANGENT LINE of the arc - not the arc itself.
This is coherent with the assumption that pixels orientation is not known.

It's easy to see that maxAngle just has to be not multiplied by 2.
*/

  double maxAngle = acos(1.0 - err / radius);  //* 2.0;
  nAngles         = tceil(fabs(angle) / maxAngle);
}

//------------------------------------------------------------

TRectD tellipticbrush::computeBBox(const TStroke &stroke) {
  TRectD bbox;

  int i, n = stroke.getChunkCount();
  for (i = 0; i < n; i++) bbox += stroke.getChunk(i)->getBBox();

  return bbox;
}

//********************************************************************************
//    CenterlinePoint implementation
//********************************************************************************

void tellipticbrush::CenterlinePoint::buildPos(const TStroke &stroke) {
  if (m_posBuilt) return;

  m_p        = stroke.getChunk(m_chunkIdx)->getThickPoint(m_t);
  m_posBuilt = true;
}

//------------------------------------------------------------

void tellipticbrush::CenterlinePoint::buildDirs(const TStroke &stroke) {
  if (m_dirsBuilt) return;

  int chunkPrev, chunkNext;
  double tPrev, tNext;
  bool coveredPrev, coveredNext;

  // Discriminate the boundary cases
  bool quadBoundary;
  if (m_t == 0.0) {
    quadBoundary = true;
    chunkPrev = m_chunkIdx - 1, chunkNext = m_chunkIdx;
    tPrev = 1.0, tNext = 0.0;
  } else if (m_t == 1.0) {
    quadBoundary = true;
    chunkPrev = m_chunkIdx, chunkNext = m_chunkIdx + 1;
    tPrev = 1.0, tNext = 0.0;
  } else {
    quadBoundary = false;
    chunkPrev = chunkNext = m_chunkIdx;
    tPrev = tNext = m_t;
  }

  // Build the backward direction
  if (chunkPrev >= 0) {
    const TThickQuadratic *ttqPrev = stroke.getChunk(chunkPrev);

    const TThickPoint &P0 = ttqPrev->getThickP0();
    const TThickPoint &P1 = ttqPrev->getThickP1();
    const TThickPoint &P2 = ttqPrev->getThickP2();

    if (quadBoundary && (P1 == P2))
      m_prevD = P2 - P0;  // Toonz 'Linear' CPs. Eliminating a perilous
                          // singularity this way.
    else {
      m_prevD.x = 2.0 * ((P1.x - P0.x) + tPrev * (P0.x - 2.0 * P1.x + P2.x));
      m_prevD.y = 2.0 * ((P1.y - P0.y) + tPrev * (P0.y - 2.0 * P1.y + P2.y));
      m_prevD.thick = 2.0 * ((P1.thick - P0.thick) +
                             tPrev * (P0.thick - 2.0 * P1.thick + P2.thick));
    }

    // Points whose thickness derivative does exceeds the point speed
    // cannot project envelope directions for that direction. This needs to be
    // known.
    coveredPrev = (sq(m_prevD.x) + sq(m_prevD.y) < sq(m_prevD.thick) + tolPar);

    // Accept only uncovered derivatives
    m_hasPrevD = !coveredPrev;
  } else {
    m_hasPrevD  = false;
    coveredPrev = true;  // ie prev coverage must not affect next coverage
    m_prevD     = TConsts::natp;
  }

  // Build the forward direction
  if (chunkPrev == chunkNext) {
    // If the quadratic is the same, no need to derive it twice
    m_hasNextD  = m_hasPrevD;
    m_nextD     = m_prevD;
    coveredNext = coveredPrev;
  } else if (chunkNext < stroke.getChunkCount()) {
    const TThickQuadratic *ttqNext = stroke.getChunk(chunkNext);

    const TThickPoint &P0 = ttqNext->getThickP0();
    const TThickPoint &P1 = ttqNext->getThickP1();
    const TThickPoint &P2 = ttqNext->getThickP2();

    if (quadBoundary && (P0 == P1))
      m_nextD = P2 - P0;
    else {
      m_nextD.x = 2.0 * ((P1.x - P0.x) + tNext * (P0.x - 2.0 * P1.x + P2.x));
      m_nextD.y = 2.0 * ((P1.y - P0.y) + tNext * (P0.y - 2.0 * P1.y + P2.y));
      m_nextD.thick = 2.0 * ((P1.thick - P0.thick) +
                             tNext * (P0.thick - 2.0 * P1.thick + P2.thick));
    }

    coveredNext = (sq(m_nextD.x) + sq(m_nextD.y) < sq(m_nextD.thick) + tolPar);
    m_hasNextD  = !coveredNext;
  } else {
    m_hasNextD  = false;
    coveredNext = true;  // ie prev coverage must not affect next coverage
    m_nextD     = TConsts::natp;
  }

  m_covered = (coveredPrev && coveredNext);

  m_dirsBuilt = true;
}

//********************************************************************************
//    Specialized Linearizator for common stroke drawing
//********************************************************************************

namespace {

class LengthLinearizator final : public tellipticbrush::StrokeLinearizator {
  double m_lengthStep;
  int m_countIdx;

public:
  LengthLinearizator(const TStroke *stroke, double lengthStep)
      : StrokeLinearizator(stroke), m_lengthStep(lengthStep), m_countIdx(0) {}

  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) override;
};

//--------------------------------------------------------------------------------------------

void LengthLinearizator::linearize(std::vector<CenterlinePoint> &cPoints,
                                   int chunk) {
  if (m_lengthStep == 0.0) return;

  // Retrieve the stroke length at stroke start
  double startW      = this->m_stroke->getW(chunk, 0.0);
  double startLength = this->m_stroke->getLength(startW);

  // Retrieve the quadratic's end length
  const TThickQuadratic *ttq = this->m_stroke->getChunk(chunk);
  double endLength           = startLength + ttq->getLength();

  // Build the step-length inside the chunk
  int n = tceil(startLength / m_lengthStep);
  double length;
  double t, w;
  int chk;

  for (length = n * m_lengthStep; length < endLength; length += m_lengthStep) {
    // Retrieve the new params at length. Need to use the sloppy TStroke
    // interface,
    // unfortunately...
    w = this->m_stroke->getParameterAtLength(length);

    // WARNING: TStroke's interface is COMPLETELY WRONG about what gets returned
    // by the following function. This is just *CRAZY* - however, let's take it
    // all right...
    bool ok = !this->m_stroke->getChunkAndT(w, chk, t);

    // In case something goes wrong, skip
    if (!ok || chk != chunk) continue;

    // Store the param, that NEEDS TO BE INCREMENTALLY COUNTED - as length
    // linearization
    // is typically used for special procedural vector styles that need this
    // info.
    CenterlinePoint cPoint(chk, t);
    cPoint.m_countIdx = m_countIdx += 2;  //++m_countIdx;
    cPoints.push_back(cPoint);
  }
}

//============================================================================================

class RecursiveLinearizator final : public tellipticbrush::StrokeLinearizator {
  double m_pixSize;

public:
  RecursiveLinearizator(const TStroke *stroke, double pixSize)
      : StrokeLinearizator(stroke), m_pixSize(pixSize) {}

  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) override;
  void subdivide(std::vector<CenterlinePoint> &cPoints, CenterlinePoint &cp0,
                 CenterlinePoint &cp1);
};

//--------------------------------------------------------------------------------------------

void RecursiveLinearizator::linearize(std::vector<CenterlinePoint> &cPoints,
                                      int chunk) {
  /*
Recursively linearizes the centerline, in the following way:

Take one point, together with the next. Add a point in the middle interval,
until
the next thick point is included (up to pixSize) in the 'forward-cast' envelope
of
current one. If the midpoint was added, repeat on the 2 sub-intervals.
*/

  const TStroke &stroke      = *this->m_stroke;
  const TThickQuadratic &ttq = *stroke.getChunk(chunk);

  // Sort the interval (SHOULD BE DONE OUTSIDE?)
  std::sort(cPoints.begin(), cPoints.end());

  std::vector<CenterlinePoint> addedPoints;

  unsigned int i, size_1 = cPoints.size() - 1;
  for (i = 0; i < size_1; ++i) {
    cPoints[i].buildPos(stroke);
    cPoints[i].buildDirs(stroke);

    cPoints[i + 1].buildPos(stroke);
    cPoints[i + 1].buildDirs(stroke);

    subdivide(addedPoints, cPoints[i], cPoints[i + 1]);
  }

  cPoints[size_1].buildPos(stroke);
  cPoints[size_1].buildDirs(stroke);

  CenterlinePoint cpEnd(chunk, 1.0);
  {
    const TThickPoint &P1(ttq.getThickP1());
    cpEnd.m_p = ttq.getThickP2();
    cpEnd.m_prevD =
        TThickPoint(2.0 * (cpEnd.m_p.x - P1.x), 2.0 * (cpEnd.m_p.y - P1.y),
                    2.0 * (cpEnd.m_p.thick - P1.thick));
    cpEnd.m_hasPrevD =
        true;  // The effective false case should already be dealt by sqrt...
  }

  subdivide(addedPoints, cPoints[size_1], cpEnd);

  cPoints.insert(cPoints.end(), addedPoints.begin(), addedPoints.end());
}

//--------------------------------------------------------------------------------------------

void RecursiveLinearizator::subdivide(std::vector<CenterlinePoint> &cPoints,
                                      CenterlinePoint &cp0,
                                      CenterlinePoint &cp1) {
  if (!(cp0.m_hasNextD && cp1.m_hasPrevD)) return;

  // Build the distance of next from the outline of cp's 'envelope extension'

  TPointD envDirL0, envDirR0, envDirL1, envDirR1;
  buildEnvelopeDirections(cp0.m_p, cp0.m_nextD, envDirL0, envDirR0);
  buildEnvelopeDirections(cp1.m_p, cp1.m_prevD, envDirL1, envDirR1);

  TPointD diff(convert(cp1.m_p) - convert(cp0.m_p));
  double d = std::max(fabs(envDirL0 * (diff + cp1.m_p.thick * envDirL1 -
                                       cp0.m_p.thick * envDirL0)),
                      fabs(envDirR0 * (diff + cp1.m_p.thick * envDirR1 -
                                       cp0.m_p.thick * envDirR0)));

  if (d > m_pixSize && cp1.m_t - cp0.m_t > 1e-4) {
    double midT = 0.5 * (cp0.m_t + cp1.m_t);
    CenterlinePoint midPoint(cp0.m_chunkIdx, midT);

    midPoint.buildPos(*this->m_stroke);
    midPoint.buildDirs(*this->m_stroke);

    subdivide(cPoints, cp0, midPoint);
    subdivide(cPoints, midPoint, cp1);

    cPoints.push_back(midPoint);
  }
}

//============================================================================================

class CoverageLinearizator final : public tellipticbrush::StrokeLinearizator {
public:
  CoverageLinearizator(const TStroke *stroke) : StrokeLinearizator(stroke) {}

  void linearize(std::vector<CenterlinePoint> &cPoints, int chunk) override;
};

//--------------------------------------------------------------------------------------------

void CoverageLinearizator::linearize(std::vector<CenterlinePoint> &cPoints,
                                     int chunk) {
  // Retrieve the at max 2 parameters for which:
  //    sq(d.x) + sq(d.y) == sq(d.thick) + tolPar(*)     (ie, "self-coverage"
  //    critical points)

  // It can be rewritten in the canonical form:    at^2 + bt + c == 0

  const TThickQuadratic &ttq(*this->m_stroke->getChunk(chunk));

  TThickPoint P0(ttq.getThickP0()), P1(ttq.getThickP1()), P2(ttq.getThickP2());
  if ((P0 == P1) || (P1 == P2))
    return;  // Linear speed out/in case. Straighted up in the buildDirs()

  // Remember that d = 2 [P1 - P0 + t (P0 + P2 - 2 P1)]

  T3DPointD u(P1.x - P0.x, P1.y - P0.y, P1.thick - P0.thick);
  T3DPointD v(P0.x + P2.x - 2.0 * P1.x, P0.y + P2.y - 2.0 * P1.y,
              P0.thick + P2.thick - 2.0 * P1.thick);

  double a = sq(v.x) + sq(v.y) - sq(v.z);
  if (fabs(a) < 1e-4) return;  // Little (acceleration) quadratics case

  //(*) Build tolerance - 2.0 since tolPar is already used to discriminate
  //'good' dirs. Ours must be.
  const double twiceTolPar = 2.0 * tolPar;

  double b = 2.0 * (u.x * v.x + u.y * v.y - u.z * v.z);
  double c = sq(u.x) + sq(u.y) - sq(u.z) - twiceTolPar;

  double delta = sq(b) - 4.0 * a * c;
  if (delta < 0) return;

  double sqrtDelta = sqrt(delta);
  double t0        = (-b - sqrtDelta) / (2.0 * a);
  double t1        = (-b + sqrtDelta) / (2.0 * a);

  if (t0 > 0 && t0 < 1) {
    CenterlinePoint cp(chunk, t0);
    cp.buildPos(*this->m_stroke);
    cp.buildDirs(*this->m_stroke);
    cp.m_hasNextD = false;
    cPoints.push_back(cp);
  }

  if (t1 > 0 && t1 < 1) {
    CenterlinePoint cp(chunk, t1);
    cp.buildPos(*this->m_stroke);
    cp.buildDirs(*this->m_stroke);
    cp.m_hasPrevD = false;
    cPoints.push_back(cp);
  }
}

}  // namespace

//********************************************************************************
//    Outline Builder implementation
//********************************************************************************

tellipticbrush::OutlineBuilder::OutlineBuilder(const OutlinizationData &data,
                                               const TStroke &stroke)
    : m_pixSize(data.m_pixSize)
    , m_oOptions(stroke.outlineOptions())
    , m_lastChunk(stroke.getChunkCount() - 1) {
  typedef TStroke::OutlineOptions OutlineOptions;

  switch (m_oOptions.m_capStyle) {
  case OutlineOptions::PROJECTING_CAP: {
    m_addBeginCap =
        &OutlineBuilder::addProjectingBeginCap<std::vector<TOutlinePoint>>;
    m_addEndCap =
        &OutlineBuilder::addProjectingEndCap<std::vector<TOutlinePoint>>;
    m_addBeginCap_ext = &OutlineBuilder::addProjectingBeginCap<TRectD>;
    m_addEndCap_ext   = &OutlineBuilder::addProjectingEndCap<TRectD>;
    break;
  }
  case OutlineOptions::BUTT_CAP: {
    m_addBeginCap     = &OutlineBuilder::addButtBeginCap;
    m_addEndCap       = &OutlineBuilder::addButtEndCap;
    m_addBeginCap_ext = 0;
    m_addEndCap_ext   = 0;
    break;
  }
  case OutlineOptions::ROUND_CAP:
  default:
    m_addBeginCap     = &OutlineBuilder::addRoundBeginCap;
    m_addEndCap       = &OutlineBuilder::addRoundEndCap;
    m_addBeginCap_ext = 0;
    m_addEndCap_ext   = 0;
  };

  switch (m_oOptions.m_joinStyle) {
  case OutlineOptions::MITER_JOIN: {
    m_addSideCaps =
        &OutlineBuilder::addMiterSideCaps<std::vector<TOutlinePoint>>;
    m_addSideCaps_ext = &OutlineBuilder::addMiterSideCaps<TRectD>;
    break;
  }

  case OutlineOptions::BEVEL_JOIN: {
    m_addSideCaps     = &OutlineBuilder::addBevelSideCaps;
    m_addSideCaps_ext = 0;
    break;
  }
  case OutlineOptions::ROUND_JOIN:
  default:
    m_addSideCaps     = &OutlineBuilder::addRoundSideCaps;
    m_addSideCaps_ext = 0;
  };
}

//------------------------------------------------------------

/*!
  Translates a CenterlinePoint instance into OutlinePoints, and
  adds them to the supplied vector container.
*/
void tellipticbrush::OutlineBuilder::buildOutlinePoints(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // If the centerline directions exist and match, just add their envelope
  // displacement directly
  if (cPoint.m_hasPrevD && cPoint.m_hasNextD &&
      cPoint.m_prevD == cPoint.m_nextD) {
    TPointD leftD, rightD;
    buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, true, leftD);
    buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, false, rightD);

    oPoints.push_back(
        TOutlinePoint(convert(cPoint.m_p) + rightD, cPoint.m_countIdx));
    oPoints.push_back(
        TOutlinePoint(convert(cPoint.m_p) + leftD, cPoint.m_countIdx));
  } else {
    // We have to add caps/joins together with the envelope displacements
    // Caps which are not at stroke ends are always imposed to be round.

    if (cPoint.m_hasPrevD) {
      if (cPoint.m_hasNextD)
        (this->*m_addSideCaps)(oPoints, cPoint);
      else if (cPoint.m_chunkIdx == m_lastChunk && cPoint.m_t == 1.0)
        (this->*m_addEndCap)(oPoints, cPoint);
      else
        addRoundEndCap(oPoints, cPoint);
    } else {
      if (cPoint.m_hasNextD)
        if (cPoint.m_chunkIdx == 0 && cPoint.m_t == 0.0)
          (this->*m_addBeginCap)(oPoints, cPoint);
        else
          addRoundBeginCap(oPoints, cPoint);
      else
        addCircle(oPoints, cPoint);
    }
  }
}

//------------------------------------------------------------

/*!
  Translates a CenterlinePoint instance into bounding box points,
  and adds them to the supplied (bbox) rect.
*/
void tellipticbrush::OutlineBuilder::buildOutlineExtensions(
    TRectD &bbox, const CenterlinePoint &cPoint) {
  if (!(cPoint.m_hasPrevD && cPoint.m_hasNextD &&
        cPoint.m_prevD == cPoint.m_nextD)) {
    // Only non-envelope points are interesting to the bbox builder procedure

    if (cPoint.m_hasPrevD) {
      if (cPoint.m_hasNextD && m_addSideCaps_ext)
        (this->*m_addSideCaps_ext)(bbox, cPoint);
      else if (cPoint.m_chunkIdx == m_lastChunk && cPoint.m_t == 1.0 &&
               m_addEndCap_ext)
        (this->*m_addEndCap_ext)(bbox, cPoint);
    } else {
      if (cPoint.m_hasNextD)
        if (cPoint.m_chunkIdx == 0 && cPoint.m_t == 0.0 && m_addBeginCap_ext)
          (this->*m_addBeginCap_ext)(bbox, cPoint);
    }
  }
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addCircularArcPoints(
    int idx, std::vector<TOutlinePoint> &outPoints, const TPointD &center,
    const TPointD &ray, double angle, int nAngles, int countIdx) {
  TPointD rotRay(ray);

  // Push the initial point without rotation
  outPoints[idx] = TOutlinePoint(center + ray, countIdx);
  idx += 2;

  // Build the rotation
  double sin_a = sin(angle);  // NOTE: The 'angle' input parameter CANNOT be
                              // substituted with just cos,
  double cos_a = cos(angle);  // while sin = sqrt(1.0 - sq(cos)), BECAUSE this
                              // way sin is ALWAYS > 0

  int i;
  for (i = 1; i <= nAngles; ++i, idx += 2) {
    rotRay = TPointD(rotRay.x * cos_a - rotRay.y * sin_a,
                     rotRay.x * sin_a + rotRay.y * cos_a);
    outPoints[idx] = center + rotRay;
  }
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addCircle(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // Build the angle step for (0, pi)
  int nAngles;
  double stepAngle, totAngle = angle(TPointD(1.0, 0.0), TPointD(-1.0, 0.0));

  buildAngularSubdivision(cPoint.m_p.thick, totAngle, m_pixSize, nAngles);
  stepAngle = totAngle / (double)nAngles;

  // Resize the vector to store the required points
  int idx = oPoints.size();
  oPoints.resize(oPoints.size() + 2 * (nAngles + 1), TOutlinePoint(TPointD()));

  // Add the circle points from each semi-circle
  addCircularArcPoints(idx, oPoints, convert(cPoint.m_p),
                       TPointD(cPoint.m_p.thick, 0.0), -stepAngle, nAngles,
                       cPoint.m_countIdx);
  addCircularArcPoints(idx + 1, oPoints, convert(cPoint.m_p),
                       TPointD(cPoint.m_p.thick, 0.0), stepAngle, nAngles,
                       cPoint.m_countIdx);
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addRoundBeginCap(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  TPointD rightD;
  buildEnvelopeVector(cPoint.m_p, cPoint.m_nextD, false, rightD);

  TPointD beginD(-convert(cPoint.m_nextD));
  beginD = (cPoint.m_p.thick / norm(beginD)) * beginD;

  int nAngles;
  double stepAngle, totAngle = angle(beginD, rightD);

  buildAngularSubdivision(cPoint.m_p.thick, totAngle, m_pixSize, nAngles);
  stepAngle = totAngle / (double)nAngles;

  int idx = oPoints.size();
  oPoints.resize(oPoints.size() + 2 * (nAngles + 1), TOutlinePoint(TPointD()));

  addCircularArcPoints(idx, oPoints, convert(cPoint.m_p), beginD, stepAngle,
                       nAngles, cPoint.m_countIdx);
  addCircularArcPoints(idx + 1, oPoints, convert(cPoint.m_p), beginD,
                       -stepAngle, nAngles,
                       cPoint.m_countIdx);  // we just need to take the opposite
                                            // angle to deal with left side
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addRoundEndCap(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // Build the backward envelope directions
  // Note that the situation is specular on the left and right side...
  TPointD leftD, rightD;
  buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, true, leftD);
  buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, false, rightD);

  int nAngles;
  double stepAngle, totAngle = angle(rightD, convert(cPoint.m_prevD));

  buildAngularSubdivision(cPoint.m_p.thick, totAngle, m_pixSize, nAngles);
  stepAngle = totAngle / (double)nAngles;

  int idx = oPoints.size();
  oPoints.resize(oPoints.size() + 2 * (nAngles + 1), TOutlinePoint(TPointD()));

  addCircularArcPoints(idx, oPoints, convert(cPoint.m_p), rightD, stepAngle,
                       nAngles, cPoint.m_countIdx);
  addCircularArcPoints(idx + 1, oPoints, convert(cPoint.m_p), leftD, -stepAngle,
                       nAngles, cPoint.m_countIdx);  // we just need to take the
                                                     // opposite angle to deal
                                                     // with left side
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addButtBeginCap(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // Just add the 2 basic envelope points
  TPointD leftDNext, rightDNext;
  buildEnvelopeVectors(cPoint.m_p, cPoint.m_nextD, leftDNext, rightDNext);

  // PLUS, add their midpoint, since it generates this part of stroke
  // antialias...
  TPointD leftP(convert(cPoint.m_p) + leftDNext),
      rightP(convert(cPoint.m_p) + rightDNext);
  TPointD midP(0.5 * (leftP + rightP));

  oPoints.push_back(midP);
  oPoints.push_back(midP);

  oPoints.push_back(TOutlinePoint(rightP, cPoint.m_countIdx));
  oPoints.push_back(TOutlinePoint(leftP, cPoint.m_countIdx));
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addButtEndCap(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  TPointD leftDPrev, rightDPrev;
  buildEnvelopeVectors(cPoint.m_p, cPoint.m_prevD, leftDPrev, rightDPrev);

  TPointD leftP(convert(cPoint.m_p) + leftDPrev),
      rightP(convert(cPoint.m_p) + rightDPrev);
  TPointD midP(0.5 * (leftP + rightP));

  oPoints.push_back(
      TOutlinePoint(convert(cPoint.m_p) + rightDPrev, cPoint.m_countIdx));
  oPoints.push_back(
      TOutlinePoint(convert(cPoint.m_p) + leftDPrev, cPoint.m_countIdx));

  oPoints.push_back(midP);
  oPoints.push_back(midP);
}

//------------------------------------------------------------

template <typename T>
void tellipticbrush::OutlineBuilder::addProjectingBeginCap(
    T &oPoints, const CenterlinePoint &cPoint) {
  double thick = cPoint.m_p.thick;

  // Find the base points
  TPointD leftDNext, rightDNext;
  buildEnvelopeDirections(cPoint.m_p, cPoint.m_nextD, leftDNext, rightDNext);

  TPointD leftP(convert(cPoint.m_p) + thick * leftDNext);
  TPointD rightP(convert(cPoint.m_p) + thick * rightDNext);

  // Add the intersections between the envelope directions' orthogonals and the
  // direction orthogonals
  TPointD dir(normalize(-cPoint.m_nextD));
  TPointD dirP(convert(cPoint.m_p) + thick * dir);

  TPointD cornerLCoords = intersectionCoords(
      dirP, TPointD(dir.y, -dir.x), leftP, TPointD(-leftDNext.y, leftDNext.x));

  TPointD cornerRCoords =
      intersectionCoords(dirP, TPointD(-dir.y, dir.x), rightP,
                         TPointD(rightDNext.y, -rightDNext.x));

  if (cornerLCoords.x < 0 || cornerRCoords.y < 0) return;

  // As before, midPoints must be added due to antialias
  TPointD cornerL(dirP + cornerLCoords.x * TPointD(dir.y, -dir.x));
  TPointD cornerR(dirP + cornerRCoords.x * TPointD(-dir.y, dir.x));
  TPointD midP(0.5 * (cornerL + cornerR));

  addEnvelopePoint(oPoints, midP);
  addEnvelopePoint(oPoints, midP);

  addExtensionPoint(oPoints, cornerR);
  addExtensionPoint(oPoints, cornerL);

  // Initial points must be added later, in the begin case
  addEnvelopePoint(oPoints, rightP, cPoint.m_countIdx);
  addEnvelopePoint(oPoints, leftP, cPoint.m_countIdx);
}

//------------------------------------------------------------

template <typename T>
void tellipticbrush::OutlineBuilder::addProjectingEndCap(
    T &oPoints, const CenterlinePoint &cPoint) {
  double thick = cPoint.m_p.thick;

  // Add the base points
  TPointD leftDPrev, rightDPrev;
  buildEnvelopeDirections(cPoint.m_p, cPoint.m_prevD, leftDPrev, rightDPrev);

  TPointD leftP(convert(cPoint.m_p) + thick * leftDPrev);
  TPointD rightP(convert(cPoint.m_p) + thick * rightDPrev);

  addEnvelopePoint(oPoints, rightP, cPoint.m_countIdx);
  addEnvelopePoint(oPoints, leftP, cPoint.m_countIdx);

  // Add the intersections between the envelope directions' orthogonals and the
  // direction orthogonals
  TPointD dir(normalize(cPoint.m_prevD));
  TPointD dirP(convert(cPoint.m_p) + thick * dir);

  TPointD cornerLCoords = intersectionCoords(
      dirP, TPointD(-dir.y, dir.x), leftP, TPointD(leftDPrev.y, -leftDPrev.x));

  TPointD cornerRCoords =
      intersectionCoords(dirP, TPointD(dir.y, -dir.x), rightP,
                         TPointD(-rightDPrev.y, rightDPrev.x));

  if (cornerLCoords.x < 0 || cornerRCoords.y < 0) return;

  TPointD cornerL(dirP + cornerLCoords.x * TPointD(-dir.y, dir.x));
  TPointD cornerR(dirP + cornerRCoords.x * TPointD(dir.y, -dir.x));
  TPointD midP(0.5 * (cornerL + cornerR));

  addExtensionPoint(oPoints, cornerR);
  addExtensionPoint(oPoints, cornerL);

  addEnvelopePoint(oPoints, midP);
  addEnvelopePoint(oPoints, midP);
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addRoundSideCaps(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // Side caps - this has only sense when the backward and forward
  // direction-derivatives
  // are different. This means that thay build different envelope directions.
  // So, we add
  // side caps to cover the 'elbow fractures'

  TPointD leftDPrev, leftDNext, rightDPrev, rightDNext;
  buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, true, leftDPrev);
  buildEnvelopeVector(cPoint.m_p, cPoint.m_nextD, true, leftDNext);
  buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, false, rightDPrev);
  buildEnvelopeVector(cPoint.m_p, cPoint.m_nextD, false, rightDNext);

  // This time, angle step is NOT specular
  int nAnglesL, nAnglesR;
  double totAngleL = angle(leftDPrev, leftDNext);
  double totAngleR = angle(rightDPrev, rightDNext);

  // The common case is that these angles have the same sign - thus building
  // opposites arcs of a circle
  if (tsign(totAngleL) != tsign(totAngleR)) {
    // However, there may be exceptions. We must still impose
    // the constraint about 'covering opposite arcs of a circle' -
    // it is necessary to make the outline look consistently filled.

    TPointD prevD(convert(cPoint.m_prevD)), nextD(convert(cPoint.m_nextD));

    // The only dangerous case is when the directions are near-opposed
    if (prevD * nextD < 0) {
      // Here, we must make one angle its (sign-opposite) 2*pi complement.
      // Keep the angle with the least fabs (smallest 'butterfly intersection')
      if (fabs(totAngleL) < fabs(totAngleR))
        totAngleR = (totAngleR > 0) ? totAngleR - M_2PI : totAngleR + M_2PI;
      else
        totAngleL = (totAngleL > 0) ? totAngleL - M_2PI : totAngleL + M_2PI;
    }
  }

  buildAngularSubdivision(cPoint.m_p.thick, totAngleL, m_pixSize, nAnglesL);
  buildAngularSubdivision(cPoint.m_p.thick, totAngleR, m_pixSize, nAnglesR);

  int nAngles       = std::max(nAnglesL, nAnglesR);
  double stepAngleL = totAngleL / (double)nAngles;
  double stepAngleR = totAngleR / (double)nAngles;

  if (nAnglesL == 1 && nAnglesR == 1 && fabs(totAngleL) < 0.525 &&
      fabs(totAngleR) < 0.525)  // angle < 30 degrees
  {
    // Simple case
    oPoints.push_back(
        TOutlinePoint(convert(cPoint.m_p) + rightDPrev, cPoint.m_countIdx));
    oPoints.push_back(
        TOutlinePoint(convert(cPoint.m_p) + leftDPrev, cPoint.m_countIdx));
  } else {
    int idx = oPoints.size();
    oPoints.resize(oPoints.size() + 2 * (nAngles + 1),
                   TOutlinePoint(TPointD()));

    addCircularArcPoints(idx, oPoints, convert(cPoint.m_p), rightDPrev,
                         stepAngleR, nAngles, cPoint.m_countIdx);
    addCircularArcPoints(
        idx + 1, oPoints, convert(cPoint.m_p), leftDPrev, stepAngleL, nAngles,
        cPoint.m_countIdx);  // same angle here, as this is just a stroke
                             // direction rotation
  }
}

//------------------------------------------------------------

void tellipticbrush::OutlineBuilder::addBevelSideCaps(
    std::vector<TOutlinePoint> &oPoints, const CenterlinePoint &cPoint) {
  // Build the envelope directions
  TPointD leftDPrev, leftDNext, rightDPrev, rightDNext;
  buildEnvelopeDirections(cPoint.m_p, cPoint.m_prevD, leftDPrev, rightDPrev);
  buildEnvelopeDirections(cPoint.m_p, cPoint.m_nextD, leftDNext, rightDNext);

  // Add at least 2 outline points (the prevs)
  oPoints.push_back(TOutlinePoint(
      convert(cPoint.m_p) + cPoint.m_p.thick * rightDPrev, cPoint.m_countIdx));
  oPoints.push_back(TOutlinePoint(
      convert(cPoint.m_p) + cPoint.m_p.thick * leftDPrev, cPoint.m_countIdx));

  // Only add the additional points when at least one of the envelope
  // differences
  // passing from prev to next is above the pixel size
  if (2.0 * cPoint.m_p.thick < m_pixSize) return;

  double threshold = sq(m_pixSize / cPoint.m_p.thick);

  double bevelSizeL = norm2(leftDNext - leftDPrev);
  double bevelSizeR = norm2(rightDNext - rightDPrev);

  if (bevelSizeL > threshold || bevelSizeR > threshold) {
    oPoints.push_back(convert(cPoint.m_p) + cPoint.m_p.thick * rightDNext);
    oPoints.push_back(convert(cPoint.m_p) + cPoint.m_p.thick * leftDNext);
  }
}

//------------------------------------------------------------

template <typename T>
void tellipticbrush::OutlineBuilder::addMiterSideCaps(
    T &oPoints, const CenterlinePoint &cPoint) {
  // Build the elbow side

  TPointD prevD(cPoint.m_prevD);
  prevD = (1.0 / norm(prevD)) * prevD;
  TPointD nextD(cPoint.m_nextD);
  nextD = (1.0 / norm(nextD)) * nextD;

  double cross  = prevD.x * nextD.y - prevD.y * nextD.x;
  bool leftSide = (cross < 0);  // ie elbow on the left side when turning right

  // Add the intersection point of the outline's tangential extensions
  //'Tangential extensions' are just the orthogonals to envelope directions

  // Build envelope directions
  TPointD envPrevSide, envNextSide;
  buildEnvelopeDirection(cPoint.m_p, cPoint.m_prevD, leftSide, envPrevSide);
  buildEnvelopeDirection(cPoint.m_p, cPoint.m_nextD, leftSide, envNextSide);

  // Build tangential directions
  TPointD prevTangentialD, nextTangentialD;
  if (leftSide) {
    prevTangentialD = TPointD(envPrevSide.y, -envPrevSide.x);
    nextTangentialD = TPointD(-envNextSide.y, envNextSide.x);
  } else {
    prevTangentialD = TPointD(-envPrevSide.y, envPrevSide.x);
    nextTangentialD = TPointD(envNextSide.y, -envNextSide.x);
  }

  // Build the outline points in the envelope directions
  envPrevSide = cPoint.m_p.thick * envPrevSide;
  envNextSide = cPoint.m_p.thick * envNextSide;

  TPointD p0(convert(cPoint.m_p) + envPrevSide);
  TPointD p1(convert(cPoint.m_p) + envNextSide);

  // Set coordinates bounds
  double lowerBound =
      std::max(cPoint.m_p.thick * m_oOptions.m_miterLower, m_pixSize);
  double upperBound = cPoint.m_p.thick * m_oOptions.m_miterUpper;

  // Build the intersection between the 2 lines
  TPointD cornerCoords(
      intersectionCoords(p0, prevTangentialD, p1, nextTangentialD));
  if (cornerCoords == TConsts::napd || cornerCoords.x < lowerBound ||
      cornerCoords.y > upperBound || cornerCoords.y < lowerBound ||
      cornerCoords.y > upperBound) {
    // Bevel caps
    addOutlineBuilderFunc(&OutlineBuilder::addBevelSideCaps, oPoints, cPoint);
    return;
  }

  TPointD corner(p0 + cornerCoords.x * prevTangentialD);

  TPointD envPrevNotSide, envNextNotSide;
  buildEnvelopeVector(cPoint.m_p, cPoint.m_prevD, !leftSide, envPrevNotSide);
  buildEnvelopeVector(cPoint.m_p, cPoint.m_nextD, !leftSide, envNextNotSide);

  TPointD notSidePrev(convert(cPoint.m_p) + envPrevNotSide);
  TPointD notSideNext(convert(cPoint.m_p) + envNextNotSide);
  if (leftSide) {
    addEnvelopePoint(oPoints, notSidePrev, cPoint.m_countIdx);
    addEnvelopePoint(oPoints, convert(cPoint.m_p) + envPrevSide,
                     cPoint.m_countIdx);

    addExtensionPoint(oPoints, 0.5 * (notSidePrev + notSideNext));
    addExtensionPoint(oPoints, corner);

    addEnvelopePoint(oPoints, notSideNext);
    addEnvelopePoint(oPoints, convert(cPoint.m_p) + envNextSide);
  } else {
    addEnvelopePoint(oPoints, convert(cPoint.m_p) + envPrevSide,
                     cPoint.m_countIdx);
    addEnvelopePoint(oPoints, notSidePrev, cPoint.m_countIdx);

    addExtensionPoint(oPoints, corner);
    addExtensionPoint(oPoints, 0.5 * (notSidePrev + notSideNext));

    addEnvelopePoint(oPoints, convert(cPoint.m_p) + envNextSide);
    addEnvelopePoint(oPoints, notSideNext);
  }
}

//********************************************************************************
//    Outline Builder Function
//********************************************************************************

namespace {

int buildCPointsData(const TStroke &stroke,
                     std::vector<CenterlinePoint> &cPoints) {
  // Build point positions
  unsigned int i, pointsCount = cPoints.size();
  int validPointsCount = 0;
  for (i = 0; i < pointsCount; ++i) {
    CenterlinePoint &cPoint = cPoints[i];

    cPoint.buildPos(stroke);
    cPoint.buildDirs(stroke);

    if (!cPoint.m_covered)
      // Covered points simply cannot build envelope directions (forward nor
      // backward).
      // So, don't consider them
      ++validPointsCount;
  }

  if (!validPointsCount) {
    // Only single points may end up here. We just solve the problem
    // uncovering the first point.
    cPoints[0].m_covered = false;
    validPointsCount     = 1;
  }

  return validPointsCount;
}

}  // namespace

//------------------------------------------------------------

void tellipticbrush::buildOutline(const TStroke &stroke,
                                  std::vector<CenterlinePoint> &cPoints,
                                  TStrokeOutline &outline,
                                  const OutlinizationData &data) {
  // Build the centerline points associated with passed stroke parameters
  int outlineSize = buildCPointsData(stroke, cPoints);

  // Reserve the lower bound known to the outline points
  std::vector<TOutlinePoint> &oPoints = outline.getArray();
  oPoints.reserve(2 * outlineSize);

  OutlineBuilder outBuilder(data, stroke);

  // Now, build the outline
  unsigned int i, cPointsCount = cPoints.size();
  for (i = 0;; ++i) {
    // Search the next uncovered point
    for (; i < cPointsCount && cPoints[i].m_covered; ++i)
      ;

    if (i >= cPointsCount) break;

    // Build the associated outline points
    outBuilder.buildOutlinePoints(oPoints, cPoints[i]);
  }
}

//********************************************************************************
//    Make Outline Implementation
//********************************************************************************

namespace {

/*
  Quick container to store all the linearization features to be supported.
  \note The set should be appropriately ordered so that linearizator dependance
  can be supported (linearizators may work depending on knowledge of the other
  linearized points)
*/
struct LinearizatorsSet {
  static const int nLinearizators = 3;

  LengthLinearizator m_lengthLinearizator;
  CoverageLinearizator m_coverageLinearizator;
  RecursiveLinearizator m_recursiveLinearizator;

  StrokeLinearizator *m_linearizatorPtrs[nLinearizators];

public:
  LinearizatorsSet(const TStroke &stroke, const OutlinizationData &data)
      : m_lengthLinearizator(&stroke, data.m_options.m_lengthStep)
      , m_coverageLinearizator(&stroke)
      , m_recursiveLinearizator(&stroke, data.m_pixSize) {
    m_linearizatorPtrs[0] = &m_lengthLinearizator;
    m_linearizatorPtrs[1] = &m_coverageLinearizator;
    m_linearizatorPtrs[2] = &m_recursiveLinearizator;
  }

  StrokeLinearizator *operator[](int i) { return m_linearizatorPtrs[i]; }
  const int size() const { return nLinearizators; }
};

}  // namespace

//============================================================================================

void TOutlineUtil::makeOutline(const TStroke &stroke, TStrokeOutline &outline,
                               const TOutlineUtil::OutlineParameter &options) {
  // Build outlinization data
  OutlinizationData data(options);

  // Build a set of linearizators for the specified stroke
  LinearizatorsSet linearizators(stroke, data);

  std::vector<CenterlinePoint> cPoints, chunkPoints;
  int i, chunksCount = stroke.getChunkCount();
  for (i = 0; i < chunksCount; ++i) {
    chunkPoints.clear();
    chunkPoints.push_back(CenterlinePoint(i, 0.0));

    int j, linearsCount = linearizators.size();
    for (j = 0; j < linearsCount; ++j) {
      StrokeLinearizator *linearizator = linearizators[j];
      linearizator->linearize(chunkPoints, i);
    }

    // These points are just PUSH_BACK'd to the vector. A sorting must be
    // performed
    // before storing them in the overall centerline points vector
    std::sort(chunkPoints.begin(), chunkPoints.end());

    cPoints.insert(cPoints.end(), chunkPoints.begin(), chunkPoints.end());
  }

  // Build the final point.
  CenterlinePoint last(chunksCount - 1, 1.0);

  // In the selfLoop case, use its info to modify the initial point.
  if (stroke.isSelfLoop()) {
    CenterlinePoint &first = cPoints[0];

    first.buildPos(stroke);
    first.buildDirs(stroke);
    last.buildPos(stroke);
    last.buildDirs(stroke);

    first.m_prevD    = last.m_prevD;
    first.m_hasPrevD = last.m_hasPrevD;
    last.m_nextD     = first.m_nextD;
    last.m_hasNextD  = first.m_hasNextD;
    first.m_covered = last.m_covered = (first.m_covered && last.m_covered);
  }

  cPoints.push_back(last);

  // Now, build the outline associated to the linearized centerline

  // NOTE: It's NOT NECESSARY TO BUILD ALL THE CENTERLINE POINTS BEFORE THIS!
  // It's sufficient to build the outline TOGETHER with the centeraline, for
  // each quadratic!
  buildOutline(stroke, cPoints, outline, data);
}

//============================================================================================

TRectD TOutlineUtil::computeBBox(const TStroke &stroke) {
  typedef TStroke::OutlineOptions OOpts;

  // First, calculate the usual stroke bbox
  TRectD roundBBox(::computeBBox(stroke));
  const OOpts &oOptions(stroke.outlineOptions());

  if (oOptions.m_capStyle != OOpts::PROJECTING_CAP &&
      oOptions.m_joinStyle != OOpts::MITER_JOIN)
    return roundBBox;

  // Build interesting centerline points (in this case, junction points)
  std::vector<CenterlinePoint> cPoints;
  int i, chunksCount = stroke.getChunkCount();
  for (i = 0; i < chunksCount; ++i) {
    CenterlinePoint cPoint(i, 0.0);

    cPoint.buildPos(stroke);
    cPoint.buildDirs(stroke);
    cPoints.push_back(cPoint);
  }

  // Build the final point.
  CenterlinePoint last(chunksCount - 1, 1.0);

  last.buildPos(stroke);
  last.buildDirs(stroke);

  // In the selfLoop case, use its info to modify the initial point.
  if (stroke.isSelfLoop()) {
    CenterlinePoint &first = cPoints[0];

    first.m_prevD    = last.m_prevD;
    first.m_hasPrevD = last.m_hasPrevD;
    last.m_nextD     = first.m_nextD;
    last.m_hasNextD  = first.m_hasNextD;
    first.m_covered = last.m_covered = (first.m_covered && last.m_covered);
  }

  cPoints.push_back(last);

  // Now, add the associated 'extending' outline points
  OutlineBuilder outBuilder(OutlinizationData(), stroke);
  TRectD extensionBBox((std::numeric_limits<double>::max)(),
                       (std::numeric_limits<double>::max)(),
                       -(std::numeric_limits<double>::max)(),
                       -(std::numeric_limits<double>::max)());

  unsigned int j, cPointsCount = cPoints.size();
  for (j = 0;; ++j) {
    // Search the next uncovered point
    for (; j < cPointsCount && cPoints[j].m_covered; ++j)
      ;

    if (j >= cPointsCount) break;

    // Build the associated outline points
    outBuilder.buildOutlineExtensions(extensionBBox, cPoints[j]);
  }

  // Finally, merge the 2 bboxes
  return roundBBox + extensionBBox;
}
