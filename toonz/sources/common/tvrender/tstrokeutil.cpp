

// TnzCore includes
#include "tmathutil.h"
#include "tcurves.h"
#include "tbezier.h"
#include "tstrokedeformations.h"
#include "tstroke.h"
#include "tcurveutil.h"
#include "tcg_wrap.h"

// tcg includes
#include "tcg/tcg_poly_ops.h"

#define INCLUDE_HPP
#include "tcg/tcg_polylineops.h"
#include "tcg/tcg_cyclic.h"
#undef INCLUDE_HPP

#include "tstrokeutil.h"

//*********************************************************************************
//    Local namespace  stuff
//*********************************************************************************

namespace {

typedef std::vector<TThickCubic *> TThickCubicArray;
typedef std::vector<TThickQuadratic *> QuadStrokeChunkArray;

//---------------------------------------------------------------------------

int getControlPointIndex(const TStroke &stroke, double w) {
  TThickPoint p = stroke.getControlPointAtParameter(w);

  int i                 = 0;
  int controlPointCount = stroke.getControlPointCount();

  for (; i < controlPointCount; ++i)
    if (stroke.getControlPoint(i) == p) return i;

  return controlPointCount - 1;
}

//---------------------------------------------------------------------------

double findMinimum(const TStrokeDeformation &def, const TStroke &stroke,
                   double x1, double x2, double xacc, double length = 0,
                   int max_iter = 100)

{
  int j;
  double dx, f, fmid, xmid, rtb;

  f    = def.getDelta(stroke, x1) - length;
  fmid = def.getDelta(stroke, x2) - length;

  if (f == 0) return x1;
  if (fmid == 0) return x2;

  if (f * fmid > 0.0) return -1;

  rtb = f < 0.0 ? (dx = x2 - x1, x1) : (dx = x1 - x2, x2);

  for (j = 1; j <= max_iter; j++) {
    fmid = def.getDelta(stroke, xmid = rtb + (dx *= 0.5)) - length;
    if (fmid <= 0.0) rtb = xmid;
    if (fabs(dx) < xacc || fmid == 0.0) return rtb;
  }
  return -2;
}

//---------------------------------------------------------------------------

/**
 * Rationale:
 *  Suppose we want to model a segment (represented by a stroke)  so that it
 * takes the shape of a parabola (the usual case offered by the modifier). We
 * assume that: (o) stroke points lie along the y=-100 axis; (o) the x's that
 * will correspond are x1=-10 and x2=+10 (obvious from the equation).
 *
 *  The parabola may be represented on the left side by a quadratic with control
 * points: P0=(-10,-100), P1=(-5,    0), P2=( 0,    0). If we know the number of
 * linear strokes representing this parabola, we also know how many "samples"
 * are required for its linearization. This parameter can be used to
 * qualitatively determine the value with which to sample the stroke to be
 * tested; there will need to be as many points to move as there are samples in
 * the reference.
 */
double computeIncrement(double strokeLength, double pixelSize) {
  assert(pixelSize > 0 && "Pixel size is negative!!!");
  assert(strokeLength > 0 && "Stroke Length size is negative!!!");

  // height of the parabola (goes downward)
  double height = 100;

  // I suppose I'm doing at least a 100-pixel drag
  assert(height >= 100.0);

  double x = sqrt(height);

  // the point p1 will have to be at the intersection of the tangents to the two
  // extremes. The tangent of the point p2 and the x-axis, the other will have
  // versor given by the gradient at p0,
  //  ie: grad(x,-2 x)
  //  and if y = m x + q
  //  m =
  double m = 2.0 * x;

  double q = m * x - height;

  double p1x = q / m;

  double scale = strokeLength / (2.0 * x);

  TScale scaleAffine(scale, scale);

  TPointD p0 = scaleAffine * TPointD(-x, -height),
          p1 = scaleAffine * TPointD(-p1x, 0.0),
          p2 = scaleAffine * TPointD(0.0, 0.0);

  TQuadratic quadratic(p0, p1, p2);

  double step = computeStep(quadratic, pixelSize);

  //  just to add points even in the worst case.
  if (step >= 1.0) step = 0.1;

  return step;
}

//-----------------------------------------------------------------------------

void detectEdges(const std::vector<TPointD> &pointArray,
                 std::vector<UINT> &edgeIndexArray) {
  // ASSUMPTION: sharpPointArray does not contain adjacent coincident points

  int size = pointArray.size();
  // I check that there are more than three elements
  if (size < 3) return;
  //  runs pointArray and for each of its points tries to inscribe triangles
  //  (using left and right points) considering potential corners those with
  //  sides l such that dMin <= l <= dMax (actually at the first time that l >
  //  dMax: break) and with angular aperture alpha <= alphaMax.
  // Then it looks for local maxes among the potential corners in a window of
  // semiamplitude dMax(actually at the first time dMax : break is exceeded)
  //  default values: dMin = 7; dMax = dMin + 2; alphaMax = 2.6 (150 degrees)

  const double dMin     = 4;
  const double dMax     = dMin + 3;
  const double alphaMax = 2.4;  // ( 137.5 degrees)
  const double dMin2    = dMin * dMin;
  const double dMax2    = dMax * dMax;
  std::vector<double> sharpnessArray;
  sharpnessArray.push_back(M_PI);  //  the first point is a corner
  int nodeCount;
  for (nodeCount = 1; nodeCount < size - 1;
       ++nodeCount) {  //  scrolls the sharpPointArray excluding the extremes
    sharpnessArray.push_back(0);
    TPointD point(pointArray[nodeCount]);
    int leftCount;
    for (leftCount = nodeCount - 1; leftCount >= 0;
         --leftCount) {  //  Calculates the "left" sides of the inscribed
                         //  triangles...
      TPointD left  = pointArray[leftCount];
      double dLeft2 = norm2(left - point);
      if (dLeft2 < dMin2)
        continue;
      else if (dLeft2 > dMax2)
        break;
      int rightCount;
      for (rightCount = nodeCount + 1; rightCount < size;
           ++rightCount) {  // Calculates the "right" sides of the inscribed
                            // triangles...
        TPointD right  = pointArray[rightCount];
        double dRight2 = norm2(right - point);
        if (dRight2 < dMin2)
          continue;
        else if (dMax2 < dRight2)
          break;

        //  Calculates the "center" sides of the inscribed triangles
        double dCenter2 = norm2(left - right);
        assert(dLeft2 != 0.0 && dRight2 != 0.0);

        double cs =
            (dLeft2 + dRight2 - dCenter2) / (2 * sqrt(dLeft2 * dRight2));
        double alpha = acos(cs);
        if (alpha > alphaMax) continue;

        double sharpness = M_PI - alpha;

        if (sharpnessArray[nodeCount] < sharpness)
          sharpnessArray[nodeCount] = sharpness;
      }
    }
  }

  edgeIndexArray.push_back(0);  // the first point is a corner

  // I find local maxima by excluding extremes
  for (nodeCount = 1; nodeCount < size - 1;
       ++nodeCount) {  // scroll through the list excluding the extremes
    bool isCorner = true;
    TPointD point(pointArray[nodeCount]);
    int leftCount;
    for (leftCount = nodeCount - 1; leftCount >= 0;
         --leftCount) {  //  scrolls down the list of sharpPoints to the left of
                         //  node...
      TPointD left  = pointArray[leftCount];
      double dLeft2 = norm2(left - point);
      if (dLeft2 > dMax2) break;
      if (sharpnessArray[leftCount] > sharpnessArray[nodeCount]) {
        isCorner = false;
        break;
      }
    }
    if (isCorner) continue;
    int rightCount;
    for (rightCount = nodeCount + 1; rightCount < size;
         ++rightCount) {  // scrolls the list of sharpPoints to the right of
                          // node..
      TPointD right  = pointArray[rightCount];
      double dRight2 = norm2(right - point);
      if (dRight2 > dMax2) break;
      if (sharpnessArray[rightCount] > sharpnessArray[nodeCount]) {
        isCorner = false;
        break;
      }
    }
    if (isCorner) edgeIndexArray.push_back(nodeCount);
  }
  edgeIndexArray.push_back(size - 1);  //  the last point is a corner
}
}  // namespace

//*******************************************************************************
//    API  functions
//*******************************************************************************

bool increaseControlPoints(TStroke &stroke, const TStrokeDeformation &deformer,
                           double pixelSize) {
  if (isAlmostZero(stroke.getLength())) {
    return norm2(deformer.getDisplacement(stroke, 0.0)) > 0;
  }

  // step 1:
  // It's possible to have control point at not null potential
  //  but with delta equal 0 (equipotential control point)
  bool notVoidPotential = false;

  for (int i = 0; i < stroke.getControlPointCount(); ++i) {
    double par = stroke.getParameterAtControlPoint(i);
    if (deformer.getDisplacement(stroke, par) != TThickPoint()) {
      notVoidPotential = true;
      break;
    }
  }

  // step 2:
  //  increase control point checking delta of deformer
  double maxDifference =
      deformer.getMaxDiff();  // above this delta value, points are added

  int strokeControlPoint = stroke.getControlPointCount();

  // pixelSize = sq( pixelSize );

  if (pixelSize < TConsts::epsilon) pixelSize = TConsts::epsilon;

  double length = stroke.getLength(),
         // set the step function of length
      //    step = length > 1.0 ?  pixelSize * 15.0/ length : length,
      // step = 0.01,
      w = 0.0;

  double step = computeIncrement(length, pixelSize);

  double x1, x2, d1, d2, diff, offset, minimum, incr;

  incr = step;

  while (w + incr < 1.0) {
    d1 = deformer.getDelta(stroke, w);
    d2 = deformer.getDelta(stroke, w + incr);

    diff = d2 - d1;

    if (fabs(diff) >= maxDifference)  // if there is a step of potential
    {
      if (tsign(diff) > 0) {
        x1 = w;
        x2 = w + incr;
      } else {
        x1 = w + incr;
        x2 = w;
      }

      offset = (d1 + d2) * 0.5;

      // find the position of step
      minimum = findMinimum(
          deformer, stroke, x1, x2, TConsts::epsilon, offset,
          20);  // A new control point should be put between x1 and x2. where?
      // this function finds the point at which the maxdifference value is
      // exceeded

      // if minimum is not found or is equal to previous value
      //  use an heuristic...
      if (minimum < 0 || w == minimum) {
        minimum = w + incr * 0.5;
        w += step;
      }

      //... else insert a control point in minimum
      w = minimum;  // scanning resumes from the new point, in this way it
                    // thickens ...

      stroke.insertControlPoints(minimum);

      // update of step
      incr = step;
    } else
      incr += step;
  }

  // return true if control point are increased
  return (stroke.getControlPointCount() > strokeControlPoint) ||
         notVoidPotential;
}

//-----------------------------------------------------------------------------

void modifyControlPoints(TStroke &stroke, const TStrokeDeformation &deformer) {
  int cpCount = stroke.getControlPointCount();

  TThickPoint newP;

  for (int i = 0; i < cpCount; ++i) {
    newP = stroke.getControlPoint(i) +
           deformer.getDisplacementForControlPoint(stroke, i);
    if (isAlmostZero(newP.thick, 0.005)) newP.thick = 0;
    stroke.setControlPoint(i, newP);
  }
}

//-----------------------------------------------------------------------------

void modifyControlPoints(TStroke &stroke, const TStrokeDeformation &deformer,
                         std::vector<double> &controlPointLen) {
  UINT cpCount = stroke.getControlPointCount();

  TThickPoint newP;

#ifdef _DEBUG
  UINT debugVariable = controlPointLen.size();
#endif
  assert(controlPointLen.size() == cpCount);

  for (UINT i = 0; i < cpCount; ++i) {
    newP =
        stroke.getControlPoint(i) +
        deformer.getDisplacementForControlPointLen(stroke, controlPointLen[i]);
    if (isAlmostZero(newP.thick, 0.005)) newP.thick = 0;
    stroke.setControlPoint(i, newP);
  }
}

//-----------------------------------------------------------------------------

void modifyThickness(TStroke &stroke, const TStrokeDeformation &deformer,
                     std::vector<double> &controlPointLen, bool exponentially) {
  UINT cpCount = stroke.getControlPointCount();
  assert(controlPointLen.size() == cpCount);

  double disp;
  double thick;

  for (UINT i = 0; i < cpCount; ++i) {
    disp =
        (deformer.getDisplacementForControlPointLen(stroke, controlPointLen[i]))
            .thick;

    thick = stroke.getControlPoint(i).thick;

    // The additive version is straightforward.
    // The exponential version is devised to keep derivative 1 at disp == 0;
    // it is typically used when the thickness decreases.

    thick = (exponentially && thick >= 0.005) ? thick * exp(disp / thick)
                                              : thick + disp;

    if (thick < 0.005) thick = 0.0;

    stroke.setControlPoint(i, TThickPoint(stroke.getControlPoint(i), thick));
  }
}

//-----------------------------------------------------------------------------

void transform_thickness(TStroke &stroke, const double poly[], int deg) {
  int cp, cpCount = stroke.getControlPointCount();
  for (cp = 0; cp != cpCount; ++cp) {
    TThickPoint cpPoint = stroke.getControlPoint(cp);
    cpPoint.thick =
        std::max(tcg::poly_ops::evaluate(poly, deg, cpPoint.thick), 0.0);

    stroke.setControlPoint(cp, cpPoint);
  }
}

//-----------------------------------------------------------------------------

TStroke *Toonz::merge(const std::vector<TStroke *> &strokes) {
  if (strokes.empty()) return 0;

  std::vector<TThickPoint> new_stroke_cp;

  int size_stroke_array = strokes.size();

  int size_cp;

  const TStroke *ref;

  TThickPoint last = TConsts::natp;

  if (!strokes[0]) return 0;

  new_stroke_cp.push_back(strokes[0]->getControlPoint(0));
  int i, j;
  for (i = 0; i < size_stroke_array; i++) {
    ref = strokes[i];
    if (!ref) return 0;

    size_cp = ref->getControlPointCount();
    for (j = 0; j < size_cp - 1; j++) {
      const TThickPoint &pnt = ref->getControlPoint(j);

      if (last != TConsts::natp && j == 0) {
        // new_stroke_cp.push_back( (last+pnt)*0.5 );
        new_stroke_cp.push_back(last);
      }

      if (j > 0) new_stroke_cp.push_back(pnt);
    }
    // last point needs to be merged
    last = ref->getControlPoint(size_cp - 1);
  }

  new_stroke_cp.push_back(ref->getControlPoint(size_cp - 1));

  TStroke *out = new TStroke(new_stroke_cp);
  return out;
}

//-----------------------------------------------------------------------------

namespace {

class CpsReader {
  std::vector<TThickPoint> &m_cps;

public:
  typedef TPointD value_type;

public:
  CpsReader(std::vector<TThickPoint> &cps) : m_cps(cps) {}

  void openContainer(const TPointD &point) { addElement(point); }
  void addElement(const TPointD &point) {
    m_cps.push_back(TThickPoint(point, 0.0));
  }
  void closeContainer() {}
};

//===========================================================
//    Triplet to Quadratics
//===========================================================

template <typename iter_type>
double buildLength(const iter_type &begin, const iter_type &end, double tol) {
  // Build direction
  iter_type it = begin, jt;
  ++it;

  const TPointD &a = *begin, &b = *it;

  TPointD dir(normalize(b - a)), segDir;
  double dist;

  for (jt = it, ++it; it != end; jt = it, ++it) {
    segDir = *it - *jt;
    if (dir * segDir < 0) break;

    dist = tcg::point_ops::lineSignedDist(*it, a, dir);
    if (fabs(dist) > tol) {
      double s, t;
      if (dist > 0) {
        tcg::point_ops::intersectionCoords(
            *jt, segDir, a + tol * tcg::point_ops::ortLeft(dir), dir, s, t);
      } else {
        tcg::point_ops::intersectionCoords(
            *jt, segDir, a + tol * tcg::point_ops::ortRight(dir), dir, s, t);
      }

      s = tcrop(s, 0.0, 1.0);
      return (*jt + s * segDir - a) * dir;
    }
  }

  return (*jt - a) * dir;
}

//-----------------------------------------------------------------------------

/*
  Converts the specified points triplet into a sequence of quadratics' CPs
  (point
  a is not included, whereas c is).

  Conversion takes 4 parameters:

   - Adherence:     How much quadratics bend toward corners
   - Angle:         Inner product of corner's edges - full corners threshold
   - Relative:      Curvature radius/edge length    - full corners threshold
   - RelativeDist:  Tolerance about edge length build-ups

  See below for extended explanation.
*/

class TripletsConverter {
  typedef std::vector<TPointD>::const_iterator iter_type;
  typedef std::reverse_iterator<iter_type> riter_type;
  typedef tcg::cyclic_iterator<iter_type> cyclic_iter_type;
  typedef std::reverse_iterator<cyclic_iter_type> rcyclic_iter_type;

  bool m_circular;
  iter_type m_first, m_end, m_last;
  double m_adherenceTol, m_angleTol, m_relativeTol, m_relativeDistTol;

public:
  TripletsConverter(const iter_type &begin, const iter_type &end,
                    double adherenceTol, double angleTol, double relativeTol,
                    double relativeDistTol)
      : m_circular(*begin == *(end - 1))
      , m_first(m_circular ? begin + 1 : begin)
      , m_end(end)
      , m_adherenceTol(adherenceTol)
      , m_angleTol(angleTol)
      , m_relativeTol(relativeTol)
      , m_relativeDistTol(relativeDistTol) {}

  // Using bisector to convert a triplet
  void operator()(const TPointD &a, const iter_type &bt, const TPointD &c,
                  tcg::sequential_reader<std::vector<TPointD>> &output) {
    const TPointD &b = *bt;

    double prod =
        tcg::point_ops::direction(b, a) * tcg::point_ops::direction(b, c);

    if (prod > m_angleTol) {
      // Full corner
      output.addElement(0.5 * (a + b));
      output.addElement(b);
      output.addElement(0.5 * (b + c));
    } else {
      // Build the angle bisector
      TPointD a_b(a - b);
      TPointD c_b(c - b);

      double norm_a_b = norm(a_b);
      double norm_c_b = norm(c_b);

      a_b = a_b * (1.0 / norm_a_b);
      c_b = c_b * (1.0 / norm_c_b);

      TPointD v(tcg::point_ops::normalized(a_b + c_b));
      double cos_v_dir = fabs(a_b * v);

      double t1 = tcrop(m_adherenceTol / (cos_v_dir * norm_a_b), 0.0, 0.5);
      double t2 = tcrop(m_adherenceTol / (cos_v_dir * norm_c_b), 0.0, 0.5);

      if (t1 == 0.5 && t2 == 0.5) {
        // Direct conversion
        output.addElement(b);
      } else {
        // Build the quadratic split
        TPointD d(b + t1 * (a - b)), f(b + t2 * (c - b)), e(0.5 * (d + f));

        // Build curvature radiuses at the corner

        // NOTE: Both speed and acceleration would hold 2.0 as multiplier, which
        // is calculated implicitly.

        TPointD speed(f - d);

        double num = norm(speed);
        if (num <= TConsts::epsilon) {
          // Curvature radius is 0 - full corner
          output.addElement(0.5 * (a + b));
          output.addElement(b);
          output.addElement(0.5 * (b + c));
        } else {
          num = 2.0 * num * num *
                num;  // would be * 8 = 2^3, divided by the 4 below

          double den1 =
              fabs(cross(speed, a - d));  // * 4, from both args of the cross
          double den2 = fabs(cross(speed, c - f));

          double radius1 = (den1 == 0.0) ? 0.0 : num / den1;
          double radius2 = (den1 == 0.0) ? 0.0 : num / den2;

          // Build edges length
          double length1, length2;
          if (m_circular) {
            cyclic_iter_type it(bt, m_first, m_end, 0);
            cyclic_iter_type it1(bt, m_first, m_end, 1);
            cyclic_iter_type it_1(bt, m_first, m_end, -1);
            rcyclic_iter_type rit(it + 1), rit1(it_1 + 1);

            length1 = buildLength(rit, rit1, 0.25);
            length2 = buildLength(it, it1, 0.25);
          } else {
            riter_type rit(bt + 1), rend(m_first);

            length1 = buildLength(rit, rend, m_relativeDistTol);
            length2 = buildLength(bt, m_end, m_relativeDistTol);
          }

          // Test curvature radiuses against edge length
          if (radius1 / length1 < m_relativeTol &&  // both must hold
              radius2 / length2 < m_relativeTol) {
            // Full corner
            output.addElement(0.5 * (a + b));
            output.addElement(b);
            output.addElement(0.5 * (b + c));
          } else {
            // Quadratic split
            output.addElement(d);
            output.addElement(e);
            output.addElement(f);
          }
        }
      }
    }

    output.addElement(c);
  }
};

}  // namespace

//-----------------------------------------------------------------------------

void polylineToQuadratics(const std::vector<TPointD> &polyline,
                          std::vector<TThickPoint> &cps, double adherenceTol,
                          double angleTol, double relativeTol,
                          double relativeDistTol, double mergeTol) {
  CpsReader cpsReader(cps);
  TripletsConverter op(polyline.begin(), polyline.end(), adherenceTol, angleTol,
                       relativeTol, relativeDistTol);
  tcg::polyline_ops::toQuadratics(polyline.begin(), polyline.end(), cpsReader,
                                  op, mergeTol);
}
