

//-----------------------------------------------------------------------------
//  tstrokedeformations.cpp
//
//  Revision History:
//  16/07/2002  m_lengthOfDeformation is almost TConsts::epsilon
//-----------------------------------------------------------------------------

#include "tstrokedeformations.h"
#include "tcurveutil.h"
#include "tmathutil.h"
#include "tstroke.h"

using namespace std;

//=============================================================================

namespace {
const double nine_inv = 1.0 / 9.0;

/*!
           _____|  r
          /     |\  inner
      __/          \___|
                       | r
                          outer

                    {  1  if  r <= r
                    {              inner
                    {
 bowl potential(r)= {  cos( (r - r ) / (r  - r ) * pi_2 )  if   r < r  <= r
                    {             i      o    i                  i         o
                    {
                    {  0  if  r > r
                                   o

   */
struct bowlPotential {
  double m_radiusInner;
  double m_radiusOuter;

  bowlPotential(double radiusInner, double radiusOuter)
      : m_radiusInner(radiusInner), m_radiusOuter(radiusOuter) {
    assert(m_radiusInner > 0);
    assert(m_radiusOuter >= m_radiusInner);
  }

  virtual double value(double radiusToTest) {
    assert(radiusToTest >= 0);
    if (radiusToTest <= m_radiusInner) return 1.0;
    if (radiusToTest > m_radiusOuter) return 0.0;

    return 0.5 * (1.0 + cos((radiusToTest - m_radiusInner) /
                            (m_radiusOuter - m_radiusInner) * M_PI));
  }

  virtual double gradient(double radiusToTest) {
    assert(radiusToTest >= 0);
    if (radiusToTest <= m_radiusInner || radiusToTest > m_radiusOuter)
      return 0.0;

    double den = M_PI / (m_radiusOuter - m_radiusInner);

    return -0.5 * den * sin(den * (radiusToTest - m_radiusInner));
  }

  virtual ~bowlPotential() {}
};

double wyvillPotential(double r, double R) {
  if (0.0 == R) return 0.0;

  if (0 > r || r >= R) return 0.0;

  double ratio2 = sq(r / R);
  double ratio4 = sq(ratio2);
  double ratio6 = ratio2 * ratio4;

  return 1.0 + (17.0 * ratio4 - (4.0 * ratio6 + 22.0 * ratio2)) * nine_inv;
}

/*
  */
double derivateOfWyvillPotential(double r, double R) {
  if (0.0 == R) return 0.0;
  if (0 > r || r > R) return 0.0;

  double inv_of_R = 1.0 / R;
  double ratio    = r / R;
  double ratio2   = sq(ratio);
  double ratio3   = ratio * ratio2;
  double ratio5   = ratio3 * ratio2;

  return inv_of_R * nine_inv * (68.0 * ratio3 - (24.0 * ratio5 + 66.0 * ratio));
}

const double c_maxLengthOfGaussian = 3.0;

/*
  */
double gaussianPotential(double x) { return exp(-sq(x)); }

/*
  */
double derivateOfGaussianPotential(double x) { return -2 * x * exp(-sq(x)); }

/*
  Check if vector distance is in segment.
  bool  pointProjectionIsInSegment( const TPointD& p, const TSegment& seg )
  {
    TPointD
      a ( p - seg.getP0()),
      b ( seg.getP1() - seg.getP0());

    double  b2 = b*b;

    if( ! isAlmostZero(b2) )
    {
      b2 = a * b / b2;
      if (  0 <= b2 && b2 <= 1.0 ) return true;
    }

    return false;
  }
        */

double normOfGradientOfWyvillPotential(double r, double R) {
  TPointD grad;

  grad.x = 1.0;
  grad.y = derivateOfWyvillPotential(r, R);

  return norm(grad);
}

}  // end of unnamed namespace

//=============================================================================

struct TStrokePointDeformation::Imp {
  TPointD m_circleCenter;
  double m_circleRadius;
  TPointD *m_vect;

  bowlPotential *m_potential;

  Imp(const TPointD &center, double radius)
      : m_circleCenter(center), m_circleRadius(radius), m_vect(0) {
    m_potential = new bowlPotential(0.3 * m_circleRadius, m_circleRadius);
  }

  Imp(const TPointD &vect, const TPointD &center, double radius)
      : m_circleCenter(center)
      , m_circleRadius(radius)
      , m_vect(new TPointD(vect)) {
    m_potential = new bowlPotential(0.3 * m_circleRadius, m_circleRadius);
  }

  ~Imp() {
    delete m_vect;
    delete m_potential;
  }
};

TStrokePointDeformation::TStrokePointDeformation(const TPointD &center,
                                                 double radius)
    : m_imp(new Imp(center, radius)) {}

//-----------------------------------------------------------------------------

TStrokePointDeformation::TStrokePointDeformation(const TPointD &vect,
                                                 const TPointD &center,
                                                 double radius)
    : m_imp(new Imp(vect, center, radius)) {}

//-----------------------------------------------------------------------------

TStrokePointDeformation::~TStrokePointDeformation() {}

//-----------------------------------------------------------------------------

TThickPoint TStrokePointDeformation::getDisplacementForControlPoint(
    const TStroke &stroke, UINT n) const {
  // riferimento ad un punto ciccione della stroke
  TPointD pntOfStroke(convert(stroke.getControlPoint(n)));

  double d = tdistance(pntOfStroke, m_imp->m_circleCenter);

  if (m_imp->m_vect)
    return m_imp->m_potential->value(d) * TThickPoint(*m_imp->m_vect, 0);
  else {
    double outVal = m_imp->m_potential->value(d);

    return TThickPoint(outVal, outVal, 0);
  }
}

TThickPoint TStrokePointDeformation::getDisplacementForControlPointLen(
    const TStroke &stroke, double cpLen) const {
  assert(0);
  return TThickPoint();
}

//-----------------------------------------------------------------------------

TThickPoint TStrokePointDeformation::getDisplacement(const TStroke &stroke,
                                                     double w) const {
  // riferimento ad un punto ciccione della stroke
  TThickPoint thickPnt = m_imp->m_vect ? stroke.getControlPointAtParameter(w)
                                       : stroke.getThickPoint(w);

  assert(thickPnt != TConsts::natp);

  TPointD pntOfStroke(convert(thickPnt));

  double d = tdistance(pntOfStroke, m_imp->m_circleCenter);

  if (m_imp->m_vect)
    return m_imp->m_potential->value(d) * TThickPoint(*m_imp->m_vect, 0);
  else {
    double outVal = m_imp->m_potential->value(d);

    return TThickPoint(outVal, outVal, 0);
  }
}

//-----------------------------------------------------------------------------

double TStrokePointDeformation::getDelta(const TStroke &stroke,
                                         double w) const {
  // reference to a thickpoint
  TThickPoint thickPnt = m_imp->m_vect ? stroke.getControlPointAtParameter(w)
                                       : stroke.getThickPoint(w);

  assert(thickPnt != TConsts::natp);

  TPointD pntOfStroke = convert(thickPnt);

  double d = tdistance(pntOfStroke, m_imp->m_circleCenter);

  return m_imp->m_potential->gradient(d);
}

//-----------------------------------------------------------------------------

double TStrokePointDeformation::getMaxDiff() const { return 0.005; }

//=============================================================================

TStrokeParamDeformation::TStrokeParamDeformation(const TStroke *ref,
                                                 const TPointD &vect, double s,
                                                 double l)
    : m_pRef(ref)
    , m_startParameter(s)
    , m_lengthOfDeformation(l)
    , m_vect(new TPointD(vect)) {
  assert(m_lengthOfDeformation >= 0);
  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------

TStrokeParamDeformation::TStrokeParamDeformation(const TStroke *ref, double s,
                                                 double l)
    : m_pRef(ref), m_startParameter(s), m_lengthOfDeformation(l), m_vect(0) {
  assert(m_lengthOfDeformation >= 0);
  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------
TThickPoint TStrokeParamDeformation::getDisplacementForControlPoint(
    const TStroke &stroke, UINT n) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLengthAtControlPoint(n);

  diff = diff - m_startParameter;
  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    if (m_vect)
      return gaussianPotential(diff) * TThickPoint(*m_vect, 0);
    else {
      double outVal = gaussianPotential(diff);
      return TThickPoint(outVal, outVal, 0);
    }
  }

  return TThickPoint();
}

TThickPoint TStrokeParamDeformation::getDisplacementForControlPointLen(
    const TStroke &stroke, double cpLenDiff) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  // double  diff =  stroke.getLengthAtControlPoint(n);
  // double  diff =  cpLen;

  double diff = cpLenDiff;
  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    if (m_vect)
      return gaussianPotential(diff) * TThickPoint(*m_vect, 0);
    else {
      double outVal = gaussianPotential(diff);
      return TThickPoint(outVal, outVal, 0);
    }
  }

  return TThickPoint();
}

//-----------------------------------------------------------------------------

TThickPoint TStrokeParamDeformation::getDisplacement(const TStroke &stroke,
                                                     double w) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLength(w);

  diff = diff - m_startParameter;
  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    if (m_vect)
      return gaussianPotential(diff) * TThickPoint(*m_vect, 0);
    else {
      double outVal = gaussianPotential(diff);
      return TThickPoint(outVal, outVal, 0);
    }
  }

  return TThickPoint();
}

//-----------------------------------------------------------------------------

double TStrokeParamDeformation::getDelta(const TStroke &stroke,
                                         double w) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLength(w);

  diff = diff - m_startParameter;
  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    return derivateOfGaussianPotential(diff);
  }

  return 0;
}

//-----------------------------------------------------------------------------

double TStrokeParamDeformation::getMaxDiff() const { return 0.09; }

//-----------------------------------------------------------------------------

TStrokeParamDeformation::~TStrokeParamDeformation() { delete m_vect; }

//=============================================================================

//=============================================================================
/*
*/
TStrokeBenderDeformation::TStrokeBenderDeformation(const TStroke *ref, double s,
                                                   double l)
    : m_pRef(ref)
    , m_startLength(s)
    , m_lengthOfDeformation(l)
    , m_vect(0)
    , m_versus(INNER) {
  assert(m_lengthOfDeformation >= 0);

  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------

TStrokeBenderDeformation::TStrokeBenderDeformation(const TStroke *ref,
                                                   const TPointD &vect,
                                                   double angle, double s,
                                                   int innerOrOuter, double l)
    : m_pRef(ref)
    , m_startLength(s)
    , m_lengthOfDeformation(l)
    , m_vect(new TPointD(vect))
    , m_versus(innerOrOuter)
    , m_angle(angle) {
  assert(m_lengthOfDeformation >= 0);
  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------

TStrokeBenderDeformation::~TStrokeBenderDeformation() { delete m_vect; }

//-----------------------------------------------------------------------------

double TStrokeBenderDeformation::getMaxDiff() const {
  //  return  0.09; OK per gaussiana
  return 0.4;
}

//-----------------------------------------------------------------------------

TThickPoint TStrokeBenderDeformation::getDisplacementForControlPoint(
    const TStroke &s, UINT n) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double strokeLengthAtParameter = s.getLengthAtControlPoint(n);
  double diff                    = strokeLengthAtParameter - m_startLength;

  if (m_vect) {
    double outVal = 0;

    if (fabs(diff) <= m_lengthOfDeformation && m_versus == INNER) {
      diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;
      outVal = gaussianPotential(diff);
    } else if (m_versus == OUTER) {
      double valForGaussian = -c_maxLengthOfGaussian +
                              2 * c_maxLengthOfGaussian /
                                  m_lengthOfDeformation *
                                  strokeLengthAtParameter;
      outVal = 1.0 - gaussianPotential(valForGaussian);
    }

    TPointD cp = convert(s.getControlPoint(n));
    TPointD p  = cp;

    TRotation rot(*m_vect, outVal * rad2degree(m_angle));

    p = rot * p;

    return TThickPoint(p - cp, 0.0);
  }

  return TThickPoint();
}

TThickPoint TStrokeBenderDeformation::getDisplacementForControlPointLen(
    const TStroke &stroke, double cpLen) const {
  assert(0);
  return TThickPoint();
}

//-----------------------------------------------------------------------------

TThickPoint TStrokeBenderDeformation::getDisplacement(const TStroke &s,
                                                      double w) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double strokeLengthAtParameter = s.getLength(w);
  double diff                    = strokeLengthAtParameter - m_startLength;

  if (m_vect) {
    double outVal = 0.0;

    if (fabs(diff) <= m_lengthOfDeformation) {
      if (m_versus == INNER) {
        diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;
        outVal = gaussianPotential(diff);
      } else if (m_versus == OUTER) {
        double valForGaussian = -c_maxLengthOfGaussian +
                                2 * c_maxLengthOfGaussian /
                                    m_lengthOfDeformation *
                                    strokeLengthAtParameter;
        outVal = 1.0 - gaussianPotential(valForGaussian);
      }
    }

    TPointD cp = convert(s.getControlPointAtParameter(w));
    TPointD p  = cp;

    TRotation rot(*m_vect, outVal * rad2degree(m_angle));

    p = rot * p;

    return TThickPoint(p - cp, 0.0);
  }

  return TThickPoint();
}

//-----------------------------------------------------------------------------

double TStrokeBenderDeformation::getDelta(const TStroke &s, double w) const {
  double totalLength = s.getLength();

  if (totalLength != 0) {
    double val = s.getLength(w) / totalLength * (M_PI * 10.0);

    return sin(val);
  }
  return 0;
}

//=============================================================================

TStrokeTwirlDeformation::TStrokeTwirlDeformation(const TPointD &center,
                                                 double radius)
    : m_center(center)
    , m_innerRadius2(sq(radius))
    , m_vectorOfMovement(TPointD()) {
  m_outerRadius = 1.25 * radius;
}

//-----------------------------------------------------------------------------

TStrokeTwirlDeformation::TStrokeTwirlDeformation(const TPointD &center,
                                                 double radius,
                                                 const TPointD &v)
    : m_center(center), m_innerRadius2(sq(radius)), m_vectorOfMovement(v) {
  m_outerRadius = 1.25 * radius;
}

//-----------------------------------------------------------------------------

TStrokeTwirlDeformation::~TStrokeTwirlDeformation() {}

//-----------------------------------------------------------------------------

TThickPoint TStrokeTwirlDeformation::getDisplacement(const TStroke &stroke,
                                                     double s) const {
  double outVal = 0;

  double distance2 =
      tdistance2(convert(stroke.getControlPointAtParameter(s)), m_center);

  if (distance2 <= m_innerRadius2)
    outVal = wyvillPotential(distance2, m_innerRadius2);

  return TThickPoint(m_vectorOfMovement * outVal, 0);
}

//-----------------------------------------------------------------------------

double TStrokeTwirlDeformation::getDelta(const TStroke &stroke,
                                         double s) const {
  /*
vector<DoublePair>  vres;

if(intersect( stroke, m_center, m_outerRadius, vres))
{
double totalLength = stroke.getLength();

if(totalLength != 0)
{
double val =  stroke.getLength(s)/totalLength * (M_PI * 11.0);

return sin(val);
}
}
*/
  return 0;
}

//-----------------------------------------------------------------------------

double TStrokeTwirlDeformation::getMaxDiff() const { return 0.4; }

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------

//=============================================================================

TStrokeThicknessDeformation::TStrokeThicknessDeformation(const TStroke *ref,
                                                         const TPointD &vect,
                                                         double s, double l,
                                                         double versus)
    : m_lengthOfDeformation(l)
    , m_startParameter(s)
    , m_versus(versus)
    , m_vect(new TPointD(vect))
    , m_pRef(ref) {
  assert(m_lengthOfDeformation >= 0);
  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------

TStrokeThicknessDeformation::TStrokeThicknessDeformation(const TStroke *ref,
                                                         double s, double l)
    : m_lengthOfDeformation(l), m_startParameter(s), m_vect(0), m_pRef(ref) {
  assert(m_lengthOfDeformation >= 0);
  if (isAlmostZero(m_lengthOfDeformation))
    m_lengthOfDeformation = TConsts::epsilon;
}

//-----------------------------------------------------------------------------
TThickPoint TStrokeThicknessDeformation::getDisplacementForControlPoint(
    const TStroke &stroke, UINT n) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLengthAtControlPoint(n);

  diff = diff - m_startParameter;

  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    TThickPoint delta;

    if (m_vect) {
      // tsign(m_vect->y) * 0.1
      delta =
          TThickPoint(0, 0, m_versus * norm(*m_vect) * gaussianPotential(diff));
    } else {
      double outVal = gaussianPotential(diff);
      delta         = TThickPoint(0, 0, outVal);
    }
    // TThickPoint cp = stroke.getControlPoint(n);
    // if(cp.thick + delta.thick<0.001) delta.thick = 0.001-cp.thick;
    return delta;
  }
  return TThickPoint();
}

TThickPoint TStrokeThicknessDeformation::getDisplacementForControlPointLen(
    const TStroke &stroke, double diff) const {
  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    TThickPoint delta;

    if (m_vect) {
      // tsign(m_vect->y) * 0.1
      delta =
          TThickPoint(0, 0, m_versus * norm(*m_vect) * gaussianPotential(diff));
    } else {
      double outVal = gaussianPotential(diff);
      delta         = TThickPoint(0, 0, outVal);
    }
    // TThickPoint cp = stroke.getControlPoint(n);
    // if(cp.thick + delta.thick<0.001) delta.thick = 0.001-cp.thick;
    return delta;
  }
  return TThickPoint();
}

TThickPoint TStrokeThicknessDeformation::getDisplacement(const TStroke &stroke,
                                                         double w) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLength(w);

  diff = diff - m_startParameter;

  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    if (m_vect)
      return gaussianPotential(diff) * TThickPoint(*m_vect, 0);
    else {
      double outVal = gaussianPotential(diff);
      return TThickPoint(0, 0, outVal);
    }
  }

  return TThickPoint();
}

//-----------------------------------------------------------------------------

double TStrokeThicknessDeformation::getDelta(const TStroke &stroke,
                                             double w) const {
  // potenziale exp^(-x^2) limitato tra
  // [-c_maxLengthOfGaussian,c_maxLengthOfGaussian]
  double diff = stroke.getLength(w);

  diff = diff - m_startParameter;

  if (fabs(diff) <= m_lengthOfDeformation) {
    // modulo il vettore spostamento in funzione  del potenziale
    //  il termine (1.0/m_lengthOfDeformation) * 3 scala
    //  il punto in diff su un sistema di coordinate
    //  normalizzato, associato con la curva exp^(-x^2)
    diff *= (1.0 / m_lengthOfDeformation) * c_maxLengthOfGaussian;

    return derivateOfGaussianPotential(diff);
  }

  return 0;
}

//-----------------------------------------------------------------------------

double TStrokeThicknessDeformation::getMaxDiff() const { return 0.09; }

//-----------------------------------------------------------------------------

TStrokeThicknessDeformation::~TStrokeThicknessDeformation() { delete m_vect; }

//=============================================================================

TPointDeformation::TPointDeformation(const TStroke *stroke,
                                     const TPointD &center, double radius)
    : m_strokeRef(stroke), m_center(center), m_radius(radius) {
  assert(m_strokeRef);
}

//-----------------------------------------------------------------------------

TPointDeformation::TPointDeformation() {}

//-----------------------------------------------------------------------------

TPointDeformation::~TPointDeformation() {}

//-----------------------------------------------------------------------------

TThickPoint TPointDeformation::getDisplacement(double s) const {
  // riferimento ad un punto ciccione della stroke
  TThickPoint thickPnt = m_strokeRef->getPointAtLength(s);

  assert(thickPnt != TConsts::natp);

  TPointD pntOfStroke(convert(thickPnt));

  double d = tdistance(pntOfStroke, m_center);

  double outVal = wyvillPotential(d, m_radius);

  return TThickPoint(outVal, outVal, 0);
}

//-----------------------------------------------------------------------------

// return ratio: (number of Control Point)/length
double TPointDeformation::getCPDensity(double s) const {
  // reference to a thickpoint
  TThickPoint thickPnt = m_strokeRef->getThickPointAtLength(s);

  assert(thickPnt != TConsts::natp);

  TPointD pntOfStroke = convert(thickPnt);

  double d = tdistance(pntOfStroke, m_center);

  return normOfGradientOfWyvillPotential(d, m_radius);
}

//-----------------------------------------------------------------------------

double TPointDeformation::getCPCountInRange(double s0, double s1) const {
  if (s1 < s0) swap(s1, s0);

  double step = (s1 - s0) * 0.1;

  double cpCount = 0.0;

  for (double s = s0; s < s1; s += step) cpCount += getCPDensity(s);

  cpCount += getCPDensity(s1);

  return cpCount;
}

//-----------------------------------------------------------------------------

double TPointDeformation::getMinCPDensity() const { return 0.3; }

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
