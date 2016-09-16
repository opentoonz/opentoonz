

#include "tcurveutil.h"
#include "tcurves.h"
#include "tmathutil.h"
#include "tbezier.h"

//=============================================================================

/*
This function returns a vector of
pairs of double (DoublePair) which identifies the parameters
of the points of intersection.

  The integer returned is the number of intersections which
  have been identified (for two segments).

    If the segments are parallel to the parameter it is set to -1.
*/

int intersect(const TSegment &first, const TSegment &second,
              std::vector<DoublePair> &intersections) {
  return intersect(first.getP0(), first.getP1(), second.getP0(), second.getP1(),
                   intersections);
}

int intersect(const TPointD &p1, const TPointD &p2, const TPointD &p3,
              const TPointD &p4, std::vector<DoublePair> &intersections) {
  // This algorithm is presented in Graphics Geems III pag 199

  static double Ax, Bx, Ay, By, Cx, Cy, d, f, e;
  static double x1lo, x1hi, y1lo, y1hi;

  Ax = p2.x - p1.x;
  Bx = p3.x - p4.x;

  // test delle BBox
  if (Ax < 0.0) {
    x1lo = p2.x;
    x1hi = p1.x;
  } else {
    x1lo = p1.x;
    x1hi = p2.x;
  }

  if (Bx > 0.0) {
    if (x1hi < p4.x || x1lo > p3.x) return 0;
  } else if (x1hi < p3.x || x1lo > p4.x)
    return 0;

  Ay = p2.y - p1.y;
  By = p3.y - p4.y;

  if (Ay < 0) {
    y1lo = p2.y;
    y1hi = p1.y;
  } else {
    y1lo = p1.y;
    y1hi = p2.y;
  }

  if (By > 0) {
    if (y1hi < p4.y || y1lo > p3.y) return 0;
  } else if (y1hi < p3.y || y1lo > p4.y)
    return 0;

  Cx = p1.x - p3.x;
  Cy = p1.y - p3.y;

  d = By * Cx - Bx * Cy;
  f = Ay * Bx - Ax * By;
  e = Ax * Cy - Ay * Cx;

  if (f > 0) {
    if (d < 0) return 0;

    if (!areAlmostEqual(d, f))
      if (d > f) return 0;

    if (e < 0) return 0;
    if (!areAlmostEqual(e, f))
      if (e > f) return 0;
  } else if (f < 0) {
    if (d > 0) return 0;

    if (!areAlmostEqual(d, f))
      if (d < f) return 0;

    if (e > 0) return 0;
    if (!areAlmostEqual(e, f))
      if (e < f) return 0;
  } else {
    if (d < 0 || d > 1 || e < 0 || e > 1) return 0;

    if (p1 == p2 && p3 == p4) {
      intersections.push_back(DoublePair(0, 0));
      return 1;
    }

    // Check that the segments are not on the same line.
    if (!cross(p2 - p1, p4 - p1)) {
      // Calculation of Barycentric combinations.
      double distp2p1 = norm2(p2 - p1);
      double distp3p4 = norm2(p3 - p4);

      double dist2_p3p1 = norm2(p3 - p1);
      double dist2_p4p1 = norm2(p4 - p1);
      double dist2_p3p2 = norm2(p3 - p2);
      double dist2_p4p2 = norm2(p4 - p2);

      int intersection = 0;

      // Calculation of the first two solutions.
      double vol1;

      if (distp3p4) {
        distp3p4 = sqrt(distp3p4);

        vol1 = (p1 - p3) * normalize(p4 - p3);

        if (vol1 >= 0 && vol1 <= distp3p4)  // Barycentric combinations valid
        {
          intersections.push_back(DoublePair(0.0, vol1 / distp3p4));
          ++intersection;
        }

        vol1 = (p2 - p3) * normalize(p4 - p3);

        if (vol1 >= 0 && vol1 <= distp3p4) {
          intersections.push_back(DoublePair(1.0, vol1 / distp3p4));
          ++intersection;
        }
      }

      if (distp2p1) {
        distp2p1 = sqrt(distp2p1);

        vol1 = (p3 - p1) * normalize(p2 - p1);

        if (dist2_p3p2 && dist2_p3p1)
          if (vol1 >= 0 && vol1 <= distp2p1) {
            intersections.push_back(DoublePair(vol1 / distp2p1, 0.0));
            ++intersection;
          }

        vol1 = (p4 - p1) * normalize(p2 - p1);

        if (dist2_p4p2 && dist2_p4p1)
          if (vol1 >= 0 && vol1 <= distp2p1) {
            intersections.push_back(DoublePair(vol1 / distp2p1, 1.0));
            ++intersection;
          }
      }
      return intersection;
    }
    return -1;
  }

  double par_s = d / f;
  double par_t = e / f;

  intersections.push_back(DoublePair(par_s, par_t));
  return 1;
}

//------------------------------------------------------------------------------------------------------------
int intersectCloseControlPoints(const TQuadratic &c0, const TQuadratic &c1,
                                std::vector<DoublePair> &intersections);

int intersect(const TQuadratic &c0, const TQuadratic &c1,
              std::vector<DoublePair> &intersections, bool checksegments) {
  int ret;

  // Works baddly, sometimes patch intersections...
  if (checksegments) {
    ret = intersectCloseControlPoints(c0, c1, intersections);
    if (ret != -2) return ret;
  }

  double a = c0.getP0().x - 2 * c0.getP1().x + c0.getP2().x;
  double b = 2 * (c0.getP1().x - c0.getP0().x);
  double d = c0.getP0().y - 2 * c0.getP1().y + c0.getP2().y;
  double e = 2 * (c0.getP1().y - c0.getP0().y);

  double coeff = b * d - a * e;
  int i        = 0;

  if (areAlmostEqual(coeff, 0.0))  // c0 is a Segment, or a single point!!!
  {
    TSegment s = TSegment(c0.getP0(), c0.getP2());
    ret        = intersect(s, c1, intersections);
    if (a == 0 && d == 0)  // values of t in s coincide with values of t in c0
      return ret;

    for (i = intersections.size() - ret; i < (int)intersections.size(); i++) {
      intersections[i].first = c0.getT(s.getPoint(intersections[i].first));
    }
    return ret;
  }

  double c = c0.getP0().x;
  double f = c0.getP0().y;

  double g = c1.getP0().x - 2 * c1.getP1().x + c1.getP2().x;
  double h = 2 * (c1.getP1().x - c1.getP0().x);
  double k = c1.getP0().x;

  double m = c1.getP0().y - 2 * c1.getP1().y + c1.getP2().y;
  double p = 2 * (c1.getP1().y - c1.getP0().y);
  double q = c1.getP0().y;

  if (areAlmostEqual(h * m - g * p,
                     0.0))  // c1 is a Segment, or a single point!!!
  {
    TSegment s = TSegment(c1.getP0(), c1.getP2());
    ret        = intersect(c0, s, intersections);
    if (g == 0 && m == 0)  // values of t in s coincide with values of t in c0
      return ret;

    for (i = intersections.size() - ret; i < (int)intersections.size(); i++) {
      intersections[i].second = c1.getT(s.getPoint(intersections[i].second));
    }
    return ret;
  }

  double a2 = (g * d - a * m);
  double b2 = (h * d - a * p);
  double c2 = ((k - c) * d + (f - q) * a);

  coeff = 1.0 / coeff;

  double A   = (a * a + d * d) * coeff * coeff;
  double aux = A * c2 + (a * b + d * e) * coeff;

  std::vector<double> t;
  std::vector<double> solutions;

  t.push_back(aux * c2 + a * c + d * f - k * a - d * q);
  aux += A * c2;
  t.push_back(aux * b2 - h * a - d * p);
  t.push_back(aux * a2 + A * b2 * b2 - g * a - d * m);
  t.push_back(2 * A * a2 * b2);
  t.push_back(A * a2 * a2);

  rootFinding(t, solutions);
  //  solutions.push_back(0.0); //per convenzione; un valore vale l'altro....

  for (i = 0; i < (int)solutions.size(); i++) {
    if (solutions[i] < 0) {
      if (areAlmostEqual(solutions[i], 0, 1e-6))
        solutions[i] = 0;
      else
        continue;
    } else if (solutions[i] > 1) {
      if (areAlmostEqual(solutions[i], 1, 1e-6))
        solutions[i] = 1;
      else
        continue;
    }

    DoublePair tt;
    tt.second = solutions[i];
    tt.first  = coeff * (tt.second * (a2 * tt.second + b2) + c2);
    if (tt.first < 0) {
      if (areAlmostEqual(tt.first, 0, 1e-6))
        tt.first = 0;
      else
        continue;
    } else if (tt.first > 1) {
      if (areAlmostEqual(tt.first, 1, 1e-6))
        tt.first = 1;
      else
        continue;
    }

    intersections.push_back(tt);

    assert(areAlmostEqual(c0.getPoint(tt.first), c1.getPoint(tt.second), 1e-1));
  }
  return intersections.size();
}

//=============================================================================
// This function checks whether the control point
// p1 is very close to p0 or p2.
// In this case, we are approximated to the quadratic p0-p2 segment.
// If p1 is near p0, the relationship between the original and the quadratic
// segment:
//     tq = sqrt(ts),
// If p1 is near p2, instead it's:
//     tq = 1-sqrt(1-ts).

int intersectCloseControlPoints(const TQuadratic &c0, const TQuadratic &c1,
                                std::vector<DoublePair> &intersections) {
  int ret = -2;

  double dist1          = tdistance2(c0.getP0(), c0.getP1());
  if (dist1 == 0) dist1 = 1e-20;
  double dist2          = tdistance2(c0.getP1(), c0.getP2());
  if (dist2 == 0) dist2 = 1e-20;
  double val0           = std::max(dist1, dist2) / std::min(dist1, dist2);
  double dist3          = tdistance2(c1.getP0(), c1.getP1());
  if (dist3 == 0) dist3 = 1e-20;
  double dist4          = tdistance2(c1.getP1(), c1.getP2());
  if (dist4 == 0) dist4 = 1e-20;
  double val1           = std::max(dist3, dist4) / std::min(dist3, dist4);

  if (val0 > 1000000 &&
      val1 > 1000000)  // both c0 and c1 approximated by segments
  {
    TSegment s0 = TSegment(c0.getP0(), c0.getP2());
    TSegment s1 = TSegment(c1.getP0(), c1.getP2());
    ret         = intersect(s0, s1, intersections);
    for (UINT i = intersections.size() - ret; i < (int)intersections.size();
         i++) {
      intersections[i].first = (dist1 < dist2)
                                   ? sqrt(intersections[i].first)
                                   : 1 - sqrt(1 - intersections[i].first);
      intersections[i].second = (dist3 < dist4)
                                    ? sqrt(intersections[i].second)
                                    : 1 - sqrt(1 - intersections[i].second);
    }
    // return ret;
  } else if (val0 > 1000000)  // c0 only approximated segment
  {
    TSegment s0 = TSegment(c0.getP0(), c0.getP2());
    ret         = intersect(s0, c1, intersections);
    for (UINT i = intersections.size() - ret; i < (int)intersections.size();
         i++)
      intersections[i].first = (dist1 < dist2)
                                   ? sqrt(intersections[i].first)
                                   : 1 - sqrt(1 - intersections[i].first);
    // return ret;
  } else if (val1 > 1000000)  // only c1 approximated segment
  {
    TSegment s1 = TSegment(c1.getP0(), c1.getP2());
    ret         = intersect(c0, s1, intersections);
    for (UINT i = intersections.size() - ret; i < (int)intersections.size();
         i++)
      intersections[i].second = (dist3 < dist4)
                                    ? sqrt(intersections[i].second)
                                    : 1 - sqrt(1 - intersections[i].second);
    // return ret;
  }

  /*
if (ret!=-2)
{
std::vector<DoublePair> intersections1;
int ret1 = intersect(c0, c1, intersections1, false);
if (ret1>ret)
{
intersections = intersections1;
return ret1;
}
}
*/

  return ret;
}

//=============================================================================

int intersect(const TQuadratic &q, const TSegment &s,
              std::vector<DoublePair> &intersections, bool firstIsQuad) {
  int solutionNumber = 0;

  // Note the line `a*x+b*y+c = 0` we search for solutions
  //  di a*x(t)+b*y(t)+c=0 in [0,1]
  double a = s.getP0().y - s.getP1().y, b = s.getP1().x - s.getP0().x,
         c = -(a * s.getP0().x + b * s.getP0().y);

  // se il segmento e' un punto
  if (0.0 == a && 0.0 == b) {
    double outParForQuad = q.getT(s.getP0());

    if (areAlmostEqual(q.getPoint(outParForQuad), s.getP0())) {
      if (firstIsQuad)
        intersections.push_back(DoublePair(outParForQuad, 0));
      else
        intersections.push_back(DoublePair(0, outParForQuad));
      return 1;
    }
    return 0;
  }

  if (q.getP2() - q.getP1() ==
      q.getP1() - q.getP0()) {  // the second is a segment....
    if (firstIsQuad)
      return intersect(TSegment(q.getP0(), q.getP2()), s, intersections);
    else
      return intersect(s, TSegment(q.getP0(), q.getP2()), intersections);
  }

  std::vector<TPointD> bez, pol;
  bez.push_back(q.getP0());
  bez.push_back(q.getP1());
  bez.push_back(q.getP2());

  bezier2poly(bez, pol);

  std::vector<double> poly_1(3, 0), sol;

  poly_1[0] = a * pol[0].x + b * pol[0].y + c;
  poly_1[1] = a * pol[1].x + b * pol[1].y;
  poly_1[2] = a * pol[2].x + b * pol[2].y;

  if (!(rootFinding(poly_1, sol))) return 0;

  double segmentPar, solution;

  TPointD v10(s.getP1() - s.getP0());
  for (UINT i = 0; i < sol.size(); ++i) {
    solution = sol[i];
    if ((0.0 <= solution && solution <= 1.0) ||
        areAlmostEqual(solution, 0.0, 1e-6) ||
        areAlmostEqual(solution, 1.0, 1e-6)) {
      segmentPar = (q.getPoint(solution) - s.getP0()) * v10 / (v10 * v10);
      if ((0.0 <= segmentPar && segmentPar <= 1.0) ||
          areAlmostEqual(segmentPar, 0.0, 1e-6) ||
          areAlmostEqual(segmentPar, 1.0, 1e-6)) {
        TPointD p1 = q.getPoint(solution);
        TPointD p2 = s.getPoint(segmentPar);
        assert(areAlmostEqual(p1, p2, 1e-1));

        if (firstIsQuad)
          intersections.push_back(DoublePair(solution, segmentPar));
        else
          intersections.push_back(DoublePair(segmentPar, solution));
        solutionNumber++;
      }
    }
  }

  return solutionNumber;
}

//=============================================================================

bool isCloseToSegment(const TPointD &point, const TSegment &segment,
                      double distance) {
  TPointD a      = segment.getP0();
  TPointD b      = segment.getP1();
  double length2 = tdistance2(a, b);
  if (length2 < tdistance2(a, point) || length2 < tdistance2(point, b))
    return false;
  if (a.x == b.x) return fabs(point.x - a.x) <= distance;
  if (a.y == b.y) return fabs(point.y - a.y) <= distance;

  // y=mx+q
  double m = (a.y - b.y) / (a.x - b.x);
  double q = a.y - (m * a.x);

  double d2 = pow(fabs(point.y - (m * point.x) - q), 2) / (1 + (m * m));
  return d2 <= distance * distance;
}

//=============================================================================

double tdistance(const TSegment &segment, const TPointD &point) {
  TPointD v1 = segment.getP1() - segment.getP0();
  TPointD v2 = point - segment.getP0();
  TPointD v3 = point - segment.getP1();

  if (v2 * v1 <= 0)
    return tdistance(point, segment.getP0());
  else if (v3 * v1 >= 0)
    return tdistance(point, segment.getP1());

  return fabs(v2 * rotate90(normalize(v1)));
}

//-----------------------------------------------------------------------------
/*
This formule is derived from Graphic Gems pag. 600

  e = h^2 |a|/8

    e = pixel size
    h = step
    a = acceleration of curve (for a quadratic is a costant value)
*/

double computeStep(const TQuadratic &quad, double pixelSize) {
  double step = 2;

  TPointD A = quad.getP0() - 2.0 * quad.getP1() +
              quad.getP2();  // 2*A is the acceleration of the curve

  double A_len = norm(A);

  /*
A_len is equal to 2*norm(a)
pixelSize will be 0.5*pixelSize
now h is equal to sqrt( 8 * 0.5 * pixelSize / (2*norm(a)) ) = sqrt(2) * sqrt(
pixelSize/A_len )
*/

  if (A_len > 0) step = sqrt(2 * pixelSize / A_len);

  return step;
}

//-----------------------------------------------------------------------------

double computeStep(const TThickQuadratic &quad, double pixelSize) {
  TThickPoint cp0 = quad.getThickP0(), cp1 = quad.getThickP1(),
              cp2 = quad.getThickP2();

  TQuadratic q1(TPointD(cp0.x, cp0.y), TPointD(cp1.x, cp1.y),
                TPointD(cp2.x, cp2.y)),
      q2(TPointD(cp0.y, cp0.thick), TPointD(cp1.y, cp1.thick),
         TPointD(cp2.y, cp2.thick)),
      q3(TPointD(cp0.x, cp0.thick), TPointD(cp1.x, cp1.thick),
         TPointD(cp2.x, cp2.thick));

  return std::min({computeStep(q1, pixelSize), computeStep(q2, pixelSize),
                   computeStep(q3, pixelSize)});
}

//=============================================================================

/*
  Explanation: The length of a Bezier quadratic can be calculated explicitly.

  Let Q be the quadratic. The tricks to explicitly integrate | Q'(t) | are:

    - The integrand can be reformulated as:  | Q'(t) | = sqrt(at^2 + bt + c);
    - Complete the square beneath the sqrt (add/subtract sq(b) / 4a)
      and perform a linear variable change. We reduce the integrand to:
  sqrt(kx^2 + k),
      where k can be taken outside => sqrt(x^2 + 1)
    - Use x = tan y. The integrand will yield sec^3 y.
    - Integrate by parts. In short, the resulting primitive of sqrt(x^2 + 1) is:

        F(x) = ( x * sqrt(x^2 + 1) + log(x + sqrt(x^2 + 1)) ) / 2;
*/

void TQuadraticLengthEvaluator::setQuad(const TQuadratic &quad) {
  const TPointD &p0 = quad.getP0();
  const TPointD &p1 = quad.getP1();
  const TPointD &p2 = quad.getP2();

  TPointD speed0(2.0 * (p1 - p0));
  TPointD accel(2.0 * (p2 - p1) - speed0);

  double a = accel * accel;
  double b = 2.0 * accel * speed0;
  m_c      = speed0 * speed0;

  m_constantSpeed = isAlmostZero(a);  // => b isAlmostZero, too
  if (m_constantSpeed) {
    m_c = sqrt(m_c);
    return;
  }

  m_sqrt_a_div_2 = 0.5 * sqrt(a);

  m_noSpeed0 = isAlmostZero(m_c);  // => b isAlmostZero, too
  if (m_noSpeed0) return;

  m_tRef   = 0.5 * b / a;
  double d = m_c - 0.5 * b * m_tRef;

  m_squareIntegrand = (d < TConsts::epsilon);
  if (m_squareIntegrand) {
    m_f = (b > 0) ? -sq(m_tRef) : sq(m_tRef);
    return;
  }

  m_e = d / a;

  double sqrt_part = sqrt(sq(m_tRef) + m_e);
  double log_arg   = m_tRef + sqrt_part;

  m_squareIntegrand = (log_arg < TConsts::epsilon);
  if (m_squareIntegrand) {
    m_f = (b > 0) ? -sq(m_tRef) : sq(m_tRef);
    return;
  }

  m_primitive_0 = m_sqrt_a_div_2 * (m_tRef * sqrt_part + m_e * log(log_arg));
}

//-----------------------------------------------------------------------------

double TQuadraticLengthEvaluator::getLengthAt(double t) const {
  if (m_constantSpeed) return m_c * t;

  if (m_noSpeed0) return m_sqrt_a_div_2 * sq(t);

  if (m_squareIntegrand) {
    double t_plus_tRef = t + m_tRef;
    return m_sqrt_a_div_2 *
           (m_f + ((t_plus_tRef > 0) ? sq(t_plus_tRef) : -sq(t_plus_tRef)));
  }

  double y         = t + m_tRef;
  double sqrt_part = sqrt(sq(y) + m_e);
  double log_arg =
      y + sqrt_part;  // NOTE: log_arg >= log_arg0 >= TConsts::epsilon

  return m_sqrt_a_div_2 * (y * sqrt_part + m_e * log(log_arg)) - m_primitive_0;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
