

#include "tmachine.h"
#include "tcurves.h"
#include "tcurveutil.h"
#include "tmathutil.h"
#include "tbezier.h"

using namespace std;

//=============================================================================

ostream &operator<<(ostream &out, const TSegment &segment) {
  return out << "S{" << segment.getP0() << ", " << segment.getP1() << "}";
}

//=============================================================================

void TCubic::split(double t, TCubic &first, TCubic &second) const {
  double s = 1.0 - t;

  TPointD H = s * m_p1 + t * m_p2;

  first.m_p0 = m_p0;
  first.m_p1 = s * m_p0 + t * m_p1;
  first.m_p2 = s * first.m_p1 + t * H;

  second.m_p3 = m_p3;
  second.m_p2 = s * m_p2 + t * m_p3;
  second.m_p1 = s * H + t * second.m_p2;

  first.m_p3  = s * first.m_p2 + t * second.m_p1;
  second.m_p0 = first.m_p3;
}

double TCubic::getLength(double t0, double t1) const { return -1; }

//=============================================================================

TPointD TQuadratic::getPoint(double t) const {
  double s = 1 - t;
  return m_p0 * s * s + 2 * t * s * m_p1 + t * t * m_p2;
}

//-----------------------------------------------------------------------------

double TQuadratic::getX(double t) const {
  double s = 1 - t;
  return m_p0.x * s * s + 2 * t * s * m_p1.x + t * t * m_p2.x;
}
//-----------------------------------------------------------------------------

double TQuadratic::getY(double t) const {
  double s = 1 - t;
  return m_p0.y * s * s + 2 * t * s * m_p1.y + t * t * m_p2.y;
}
//-----------------------------------------------------------------------------

double TQuadratic::getT(const TPointD &p) const {
  // risolvo l'equazione  min|| b(t) - p ||

  // esprimo b in forma di polinomio  ed ottengo
  //
  //    ||  2                ||
  // min || a t + b t + c - p ||
  //    ||                   ||
  //
  // il tutto si riconduce a cercare le radici
  //  di un'equazione del tipo
  //    2  3          2               2
  // 2·a ·t  + 3·a·b·t  + t·(2·a·v + b ) + b·v
  // dove v e' pari a c - p

  vector<TPointD> bez(3), poly(3);

  bez[0] = m_p0;
  bez[1] = m_p1;
  bez[2] = m_p2;

  bezier2poly(bez, poly);

  TPointD v = poly[0] - p;

  vector<double> toSolve(4);
  vector<double> sol;

  toSolve[3] = 2.0 * norm2(poly[2]);
  toSolve[2] = 3.0 * (poly[2].x * poly[1].x + poly[2].y * poly[1].y);
  toSolve[1] = 2.0 * (poly[2].x * v.x + poly[2].y * v.y) + norm2(poly[1]);
  toSolve[0] = (poly[1].x * v.x + poly[1].y * v.y);

  int nSol = rootFinding(toSolve, sol);

  if (-1 == nSol)  // infinite soluzioni
    return 0;

  int minParameter = -1;
  double minDist   = (std::numeric_limits<double>::max)();

  for (int i = 0; i < nSol; ++i) {
    if (sol[i] < 0.0)
      sol[i] = 0.0;
    else if (sol[i] > 1.0)
      sol[i] = 1.0;

    double tmpDist = tdistance2(p, getPoint(sol[i]));
    if (tmpDist < minDist) {
      minDist      = tmpDist;
      minParameter = i;
    }
  }

  if (minParameter != -1) return sol[minParameter];

  return tdistance2(m_p0, p) < tdistance2(m_p2, p) ? 0 : 1;
}

//-----------------------------------------------------------------------------

void TQuadratic::split(double t, TQuadratic &left, TQuadratic &right) const {
  double dt;
  TPointD p;
  dt = 1.0 - t;

  left.m_p0  = m_p0;
  right.m_p2 = m_p2;

  left.m_p1  = dt * m_p0 + t * m_p1;
  right.m_p1 = dt * m_p1 + t * m_p2;
  p          = dt * left.m_p1 + t * right.m_p1;

  left.m_p2 = right.m_p0 = p;
}

//-----------------------------------------------------------------------------

TRectD TQuadratic::getBBox() const {
  TRectD bBox;
  if (m_p0.x < m_p2.x)
    bBox.x0 = m_p0.x, bBox.x1 = m_p2.x;
  else
    bBox.x0 = m_p2.x, bBox.x1 = m_p0.x;

  if (m_p0.y < m_p2.y)
    bBox.y0 = m_p0.y, bBox.y1 = m_p2.y;
  else
    bBox.y0 = m_p2.y, bBox.y1 = m_p0.y;

  TPointD denom = 2 * m_p1 - m_p0 - m_p2;
  if (denom.x != 0) {
    double tx = (m_p1.x - m_p0.x) / denom.x;
    if (tx >= 0 && tx <= 1) {
      double x = getPoint(tx).x;
      if (x < bBox.x0)
        bBox.x0 = x;
      else if (x > bBox.x1)
        bBox.x1 = x;
    }
  }
  if (denom.y != 0) {
    double ty = (m_p1.y - m_p0.y) / denom.y;
    if (ty >= 0 && ty <= 1) {
      double y = getPoint(ty).y;
      if (y < bBox.y0)
        bBox.y0 = y;
      else if (y > bBox.y1)
        bBox.y1 = y;
    }
  }

  return bBox;
}

/*!
Calcolo della curvatura per una Quadratica.
Vedi Farin pag.176 per la spiegazione della formula
usata.
*/
double TQuadratic::getCurvature(double t) const {
  assert(0 <= t && t <= 1.0);

  TQuadratic q1, q2;

  split(t, q1, q2);

  double signum = 1.0;
  if (areAlmostEqual(t, 1.0)) {
    signum *= -1.0;
    std::swap(q1, q2);
    std::swap(q2.m_p0, q2.m_p2);
  }

  TPointD v_1_0(q2.m_p1 - q2.m_p0);

  double a = norm2(v_1_0);

  if (isAlmostZero(a)) return (std::numeric_limits<double>::max)();

  a = 1.0 / sqrt(a);

  double b = cross(v_1_0 * a, q2.m_p2 - q2.m_p0);

  return 0.5 * signum * b / a;
}

//-----------------------------------------------------------------------------

double TQuadratic::getLength(double t0, double t1) const {
  TQuadraticLengthEvaluator lengthEval(*this);

  t0 = min(max(0.0, t0), 1.0);  // backward compatibility
  t1 = min(max(0.0, t1), 1.0);  // backward compatibility
  if (t0 > t1) std::swap(t0, t1);

  if (t0 > 0.0) return lengthEval.getLengthAt(t1) - lengthEval.getLengthAt(t0);

  return lengthEval.getLengthAt(t1);
}

double TQuadratic::getApproximateLength(double t0, double t1,
                                        double error) const {
  if (t0 == t1) return 0;

  t0 = min(max(0.0, t0), 1.0);
  t1 = min(max(0.0, t1), 1.0);

  if (t0 > t1) std::swap(t0, t1);

  TQuadratic q;

  if (t0 == 0.0 && t1 == 1.0)
    q = *this;
  else {
    TQuadratic q1;
    split(t0, q, q1);

    assert(t0 != 1.0);

    double newPar = (t1 - t0) / (1.0 - t0);
    q1.split(newPar, q, q1);
  }

  double step = computeStep(q, error);

  double length = 0.0;

  TPointD p1 = q.getP0();
  TPointD p2;
  for (double t = step; t < 1.0; t += step) {
    p2 = q.getPoint(t);
    length += tdistance(p1, p2);
    p1 = p2;
  }
  length += tdistance(p1, q.getP2());

  return length;
}

//-----------------------------------------------------------------------------

int TQuadratic::getX(double y, double &x0, double &x1) const {
  int ret = 0;
  double t;

  if (y > getBBox().y1 || y < getBBox().y0) return 0;

  double a      = getP0().y - 2 * getP1().y + getP2().y;
  double half_b = getP1().y - getP0().y;
  double c      = getP0().y - y;

  if (a == 0)  // segment
  {
    if (half_b == 0)  // horizontal segment, or point
    {
      if (c == 0) {
        x0 = getP0().x;
        x1 = getP2().x;
        return 2;
      } else
        return 0;
    } else {
      t = -c / (2 * half_b);
      if (t >= 0 && t <= 1) {
        x0 = getPoint(t).x;
        return 1;
      }
    }
  }

  double discr = half_b * half_b - a * c;

  if (discr < 0) return 0;

  double coeff  = 1.0 / a;
  double coeff1 = -half_b * coeff;

  if (discr == 0) {
    t = coeff1;
    if (t >= 0 && t <= 1) {
      ret = 2;
      x0 = x1 = getPoint(t).x;
    }
  } else {
    discr = sqrt(discr) * coeff;
    t     = coeff1 + discr;
    if (t >= 0 && t <= 1) {
      ret++;
      x0 = getPoint(t).x;
    }

    t = coeff1 - discr;
    if (t >= 0 && t <= 1) {
      ret++;
      if (ret == 2)
        x1 = getPoint(t).x;
      else
        x0 = getPoint(t).x;
    }
  }
  return ret;
}

int TQuadratic::getY(double y, double &y0, double &y1) const {
  TQuadratic temp(*this);

  swap(temp.m_p0.x, temp.m_p0.y);
  swap(temp.m_p1.x, temp.m_p1.y);
  swap(temp.m_p2.x, temp.m_p2.y);

  return temp.getX(y, y0, y1);
}
//=============================================================================
TPointD TCubic::getPoint(double t) const {
  double s = 1 - t;
  return m_p0 * s * s * s + 3 * t * s * (s * m_p1 + t * m_p2) +
         t * t * t * m_p3;
}
//-----------------------------------------------------------------------------
TPointD TCubic::getSpeed(double t) const {
  double s = 1 - t;
  return 3.0 * ((m_p1 - m_p0) * s * s + 2 * (m_p2 - m_p0) * s * t +
                (m_p3 - m_p2) * t * t);
}
//=============================================================================

TThickQuadratic::TThickQuadratic()
    : TQuadratic(), m_thickP0(0), m_thickP1(0), m_thickP2(0) {}

//-----------------------------------------------------------------------------

TThickQuadratic::TThickQuadratic(const TQuadratic &q)
    : TQuadratic(q), m_thickP0(0.0), m_thickP1(0.0), m_thickP2(0.0) {}

//-----------------------------------------------------------------------------

TThickQuadratic::TThickQuadratic(const TPointD &p0, double thickP0,
                                 const TPointD &p1, double thickP1,
                                 const TPointD &p2, double thickP2)
    : TQuadratic(p0, p1, p2)
    , m_thickP0(thickP0)
    , m_thickP1(thickP1)
    , m_thickP2(thickP2) {}

//-----------------------------------------------------------------------------

TThickQuadratic::TThickQuadratic(const TThickPoint &p0, const TThickPoint &p1,
                                 const TThickPoint &p2)
    : TQuadratic(TPointD(p0.x, p0.y), TPointD(p1.x, p1.y), TPointD(p2.x, p2.y))
    , m_thickP0(p0.thick)
    , m_thickP1(p1.thick)
    , m_thickP2(p2.thick) {}

//-----------------------------------------------------------------------------

TThickQuadratic::TThickQuadratic(const TThickQuadratic &thickQuadratic)
    : TQuadratic(thickQuadratic)
    , m_thickP0(thickQuadratic.m_thickP0)
    , m_thickP1(thickQuadratic.m_thickP1)
    , m_thickP2(thickQuadratic.m_thickP2) {}

//-----------------------------------------------------------------------------
void TThickQuadratic::setThickP0(const TThickPoint &p) {
  m_p0      = p;
  m_thickP0 = p.thick;
}

//-----------------------------------------------------------------------------
void TThickQuadratic::setThickP1(const TThickPoint &p) {
  m_p1      = p;
  m_thickP1 = p.thick;
}

//-----------------------------------------------------------------------------
void TThickQuadratic::setThickP2(const TThickPoint &p) {
  m_p2      = p;
  m_thickP2 = p.thick;
}

//-----------------------------------------------------------------------------
TThickPoint TThickQuadratic::getThickPoint(double t) const {
  double s = 1 - t;
  return TThickPoint(
      m_p0 * s * s + 2 * t * s * m_p1 + t * t * m_p2,
      m_thickP0 * s * s + 2 * t * s * m_thickP1 + t * t * m_thickP2);
}

//-----------------------------------------------------------------------------
void TThickQuadratic::split(double t, TThickQuadratic &left,
                            TThickQuadratic &right) const {
  double dt;
  TPointD p;
  dt = 1.0 - t;

  // control points
  left.m_p0  = m_p0;
  right.m_p2 = m_p2;

  left.m_p1  = dt * m_p0 + t * m_p1;
  right.m_p1 = dt * m_p1 + t * m_p2;
  p          = dt * left.m_p1 + t * right.m_p1;

  left.m_p2 = right.m_p0 = p;

  // thick points
  left.m_thickP0  = m_thickP0;
  right.m_thickP2 = m_thickP2;

  left.m_thickP1  = dt * m_thickP0 + t * m_thickP1;
  right.m_thickP1 = dt * m_thickP1 + t * m_thickP2;

  // store thickness of intermediary point
  p.x = dt * left.m_thickP1 + t * right.m_thickP1;

  left.m_thickP2 = right.m_thickP0 = p.x;
}

//-----------------------------------------------------------------------------

TRectD TThickQuadratic::getBBox() const {
  TRectD bBox = TQuadratic::getBBox();

  double maxRadius = std::max({m_thickP0, m_thickP1, m_thickP2});
  if (maxRadius > 0) {
    //  bBox.enlarge(maxRadius) si comporta male nel caso bBox.isEmpty()
    bBox.x0 -= maxRadius;
    bBox.y0 -= maxRadius;

    bBox.x1 += maxRadius;
    bBox.y1 += maxRadius;
  }

  return bBox;
}

// ============================================================================
// Methods of the class TThickCubic
// ============================================================================

TThickCubic::TThickCubic()
    : TCubic(), m_thickP0(0), m_thickP1(0), m_thickP2(0), m_thickP3(0) {}

//-----------------------------------------------------------------------------

TThickCubic::TThickCubic(const TPointD &p0, double thickP0, const TPointD &p1,
                         double thickP1, const TPointD &p2, double thickP2,
                         const TPointD &p3, double thickP3)
    : TCubic(p0, p1, p2, p3)
    , m_thickP0(thickP0)
    , m_thickP1(thickP1)
    , m_thickP2(thickP2)
    , m_thickP3(thickP3) {}

//-----------------------------------------------------------------------------

TThickCubic::TThickCubic(const TThickPoint &p0, const TThickPoint &p1,
                         const TThickPoint &p2, const TThickPoint &p3)
    : TCubic(TPointD(p0.x, p0.y), TPointD(p1.x, p1.y), TPointD(p2.x, p2.y),
             TPointD(p3.x, p3.y))
    , m_thickP0(p0.thick)
    , m_thickP1(p1.thick)
    , m_thickP2(p2.thick)
    , m_thickP3(p3.thick) {}
//  tonino ***************************************************************

TThickCubic::TThickCubic(const T3DPointD &p0, const T3DPointD &p1,
                         const T3DPointD &p2, const T3DPointD &p3)
    : TCubic(TPointD(p0.x, p0.y), TPointD(p1.x, p1.y), TPointD(p2.x, p2.y),
             TPointD(p3.x, p3.y))
    , m_thickP0(p0.z)
    , m_thickP1(p1.z)
    , m_thickP2(p2.z)
    , m_thickP3(p3.z) {}

//  tonino ***************************************************************

//-----------------------------------------------------------------------------

TThickCubic::TThickCubic(const TThickCubic &thickCubic)
    : TCubic(thickCubic)
    , m_thickP0(thickCubic.m_thickP0)
    , m_thickP1(thickCubic.m_thickP1)
    , m_thickP2(thickCubic.m_thickP2)
    , m_thickP3(thickCubic.m_thickP3) {}

//-----------------------------------------------------------------------------

void TThickCubic::setThickP0(const TThickPoint &p) {
  m_p0.x    = p.x;
  m_p0.y    = p.y;
  m_thickP0 = p.thick;
}

//-----------------------------------------------------------------------------

void TThickCubic::setThickP1(const TThickPoint &p) {
  m_p1.x    = p.x;
  m_p1.y    = p.y;
  m_thickP1 = p.thick;
}

//-----------------------------------------------------------------------------

void TThickCubic::setThickP2(const TThickPoint &p) {
  m_p2.x    = p.x;
  m_p2.y    = p.y;
  m_thickP2 = p.thick;
}

//-----------------------------------------------------------------------------
void TThickCubic::setThickP3(const TThickPoint &p) {
  m_p3.x    = p.x;
  m_p3.y    = p.y;
  m_thickP3 = p.thick;
}

//-----------------------------------------------------------------------------

TThickPoint TThickCubic::getThickPoint(double t) const {
  double thick_l1, thick_h, thick_r3;

  double s = 1.0 - t;

  TPointD l1(m_p0 * s + m_p1 * t);
  thick_l1 = m_thickP0 * s + m_thickP1 * t;

  TPointD h(m_p1 * s + m_p2 * t);
  thick_h = m_thickP1 * s + m_thickP2 * t;

  TPointD r3(m_p2 * s + m_p3 * t);
  thick_r3 = m_thickP2 * s + m_thickP3 * t;

  // adesso riutilizzo le variabili gia' utilizzate

  // l2
  l1       = l1 * s + h * t;
  thick_l1 = thick_l1 * s + thick_h * t;

  // r1
  r3       = h * s + r3 * t;
  thick_r3 = thick_h * s + thick_r3 * t;

  // l3-r0
  h       = l1 * s + r3 * t;
  thick_h = thick_l1 * s + thick_r3 * t;

  return TThickPoint(h, thick_h);
}

//-----------------------------------------------------------------------------

void TThickCubic::split(double t, TThickCubic &first,
                        TThickCubic &second) const {
  double s = 1.0 - t;

  TPointD H(m_p1 * s + m_p2 * t);
  double thick_h = m_thickP1 * s + m_thickP2 * t;

  first.m_p0      = m_p0;
  first.m_thickP0 = m_thickP0;

  first.m_p1      = m_p0 * s + m_p1 * t;
  first.m_thickP1 = m_thickP0 * s + m_thickP1 * t;

  first.m_p2      = first.m_p1 * s + H * t;
  first.m_thickP2 = first.m_thickP1 * s + thick_h * t;

  second.m_p3      = m_p3;
  second.m_thickP3 = m_thickP3;

  second.m_p2      = m_p2 * s + m_p3 * t;
  second.m_thickP2 = m_thickP2 * s + m_thickP3 * t;

  second.m_p1      = H * s + second.m_p2 * t;
  second.m_thickP1 = thick_h * s + second.m_thickP2 * t;

  first.m_p3      = first.m_p2 * s + second.m_p1 * t;
  first.m_thickP3 = first.m_thickP2 * s + second.m_thickP1 * t;

  second.m_p0      = first.m_p3;
  second.m_thickP0 = first.m_thickP3;
}

//-----------------------------------------------------------------------------

ostream &operator<<(ostream &out, const TQuadratic &curve) {
  return out << "Q{" << curve.getP0() << ", " << curve.getP1() << ", "
             << curve.getP2() << "}";
}

ostream &operator<<(ostream &out, const TCubic &curve) {
  return out << "C{" << curve.getP0() << ", " << curve.getP1() << ", "
             << curve.getP2() << ", " << curve.getP3() << "}";
}

ostream &operator<<(ostream &out, const TThickSegment &segment) {
  return out << "TS{" << segment.getThickP0() << ", " << segment.getThickP1()
             << "}";
}

ostream &operator<<(ostream &out, const TThickQuadratic &tq) {
  return out << "TQ{" << tq.getThickP0() << ", " << tq.getThickP1() << ", "
             << tq.getThickP2() << "}";
}

ostream &operator<<(ostream &out, const TThickCubic &tc) {
  return out << "TC{" << tc.getThickP0() << ", " << tc.getThickP1() << ", "
             << tc.getThickP2() << ", " << tc.getThickP3() << "}";
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
