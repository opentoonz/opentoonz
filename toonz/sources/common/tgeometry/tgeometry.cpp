

#include "tgeometry.h"


using namespace std;

const T3DPointD TConsts::nap3d((numeric_limits<double>::max)(),
                               (numeric_limits<double>::max)(),
                               (numeric_limits<double>::max)());

const TThickPoint TConsts::natp((numeric_limits<double>::max)(),
                                (numeric_limits<double>::max)(),
                                (numeric_limits<double>::max)());

const TPointD TConsts::napd((numeric_limits<double>::max)(),
                            (numeric_limits<double>::max)());

const TPointI TConsts::nap((numeric_limits<int>::max)(),
                           (numeric_limits<int>::max)());

const TRectD TConsts::infiniteRectD(-(numeric_limits<double>::max)(),
                                    -(numeric_limits<double>::max)(),
                                    (numeric_limits<double>::max)(),
                                    (numeric_limits<double>::max)());

const TRectI TConsts::infiniteRectI(-(numeric_limits<int>::max)(),
                                    -(numeric_limits<int>::max)(),
                                    (numeric_limits<int>::max)(),
                                    (numeric_limits<int>::max)());

//==================================================================================================

// operazioni fra affini
TAffine &TAffine::operator=(const TAffine &a) {
  a11 = a.a11;
  a12 = a.a12;
  a13 = a.a13;
  a21 = a.a21;
  a22 = a.a22;
  a23 = a.a23;
  return *this;
}
//--------------------------------------------------------------------------------------------------
TAffine TAffine::operator*(const TAffine &b) const {
  return TAffine(a11 * b.a11 + a12 * b.a21, a11 * b.a12 + a12 * b.a22,
                 a11 * b.a13 + a12 * b.a23 + a13,

                 a21 * b.a11 + a22 * b.a21, a21 * b.a12 + a22 * b.a22,
                 a21 * b.a13 + a22 * b.a23 + a23);
}
//--------------------------------------------------------------------------------------------------
TAffine TAffine::operator*=(const TAffine &b) { return *this = *this * b; }
//--------------------------------------------------------------------------------------------------
TAffine TAffine::inv() const {
  if (a12 == 0.0 && a21 == 0.0) {
    double inv_a11 =
        (a11 == 0.0 ? std::numeric_limits<double>::max() / (1 << 16)
                    : 1.0 / a11);
    double inv_a22 =
        (a22 == 0.0 ? std::numeric_limits<double>::max() / (1 << 16)
                    : 1.0 / a22);
    return TAffine(inv_a11, 0, -a13 * inv_a11, 0, inv_a22, -a23 * inv_a22);
  } else if (a11 == 0.0 && a22 == 0.0) {
    assert(a12 != 0.0);
    assert(a21 != 0.0);
    double inv_a21 =
        (a21 == 0.0 ? std::numeric_limits<double>::max() / (1 << 16)
                    : 1.0 / a21);
    double inv_a12 =
        (a12 == 0.0 ? std::numeric_limits<double>::max() / (1 << 16)
                    : 1.0 / a12);
    return TAffine(0, inv_a21, -a23 * inv_a21, inv_a12, 0, -a13 * inv_a12);
  } else {
    double d = 1. / det();
    return TAffine(a22 * d, -a12 * d, (a12 * a23 - a22 * a13) * d, -a21 * d,
                   a11 * d, (a21 * a13 - a11 * a23) * d);
  }
}
//--------------------------------------------------------------------------------------------------
double TAffine::det() const { return a11 * a22 - a12 * a21; }

//--------------------------------------------------------------------------------------------------

// Confronti tra affini
bool TAffine::operator==(const TAffine &a) const {
  return a11 == a.a11 && a12 == a.a12 && a13 == a.a13 && a21 == a.a21 &&
         a22 == a.a22 && a23 == a.a23;
}
//--------------------------------------------------------------------------------------------------
bool TAffine::operator!=(const TAffine &a) const {
  return a11 != a.a11 || a12 != a.a12 || a13 != a.a13 || a21 != a.a21 ||
         a22 != a.a22 || a23 != a.a23;
}
//--------------------------------------------------------------------------------------------------
bool TAffine::isIdentity(double err) const {
  return ((a11 - 1.0) * (a11 - 1.0) + (a22 - 1.0) * (a22 - 1.0) + a12 * a12 +
          a13 * a13 + a21 * a21 + a23 * a23) < err;
}
//--------------------------------------------------------------------------------------------------
bool TAffine::isTranslation(double err) const {
  return ((a11 - 1.0) * (a11 - 1.0) + (a22 - 1.0) * (a22 - 1.0) + a12 * a12 +
          a21 * a21) < err;
}
//--------------------------------------------------------------------------------------------------
bool TAffine::isIsotropic(double err) const {
  return areAlmostEqual(a11, a22, err) && areAlmostEqual(a12, -a21, err);
}

//--------------------------------------------------------------------------------------------------

// applicazione
TPointD TAffine::operator*(const TPointD &p) const {
  return TPointD(p.x * a11 + p.y * a12 + a13, p.x * a21 + p.y * a22 + a23);
}

//--------------------------------------------------------------------------------------------------

TRectD TAffine::operator*(const TRectD &rect) const {
  if (rect != TConsts::infiniteRectD) {
    TPointD p1 = *this * rect.getP00(), p2 = *this * rect.getP01(),
            p3 = *this * rect.getP10(), p4 = *this * rect.getP11();
    return TRectD(
        std::min({p1.x, p2.x, p3.x, p4.x}), std::min({p1.y, p2.y, p3.y, p4.y}),
        std::max({p1.x, p2.x, p3.x, p4.x}), std::max({p1.y, p2.y, p3.y, p4.y}));
  } else
    return TConsts::infiniteRectD;
}

//--------------------------------------------------------------------------------------------------

TAffine TAffine::place(double u, double v, double x, double y) const {
  return TAffine(a11, a12, x - (a11 * u + a12 * v), a21, a22,
                 y - (a21 * u + a22 * v));
}

//--------------------------------------------------------------------------------------------------

TAffine TAffine::place(const TPointD &pIn, const TPointD &pOut) const {
  return TAffine(a11, a12, pOut.x - (a11 * pIn.x + a12 * pIn.y), a21, a22,
                 pOut.y - (a21 * pIn.x + a22 * pIn.y));
}

//==================================================================================================

TRotation::TRotation(double degrees) {
  double rad, sn, cs;
  int idegrees = (int)degrees;
  if ((double)idegrees == degrees && idegrees % 90 == 0) {
    switch ((idegrees / 90) & 3) {
    case 0:
      sn = 0;
      cs = 1;
      break;
    case 1:
      sn = 1;
      cs = 0;
      break;
    case 2:
      sn = 0;
      cs = -1;
      break;
    case 3:
      sn = -1;
      cs = 0;
      break;
    default:
      sn = 0;
      cs = 0;
      break;
    }
  } else {
    rad                         = degrees * M_PI_180;
    sn                          = sin(rad);
    cs                          = cos(rad);
    if (sn == 1 || sn == -1) cs = 0;
    if (cs == 1 || cs == -1) sn = 0;
  }
  a11 = cs;
  a12 = -sn;
  a21 = -a12;
  a22 = a11;
}
//--------------------------------------------------------------------------------------------------
TRotation::TRotation(const TPointD &center, double degrees) {
  TAffine a = TTranslation(center) * TRotation(degrees) * TTranslation(-center);
  a11       = a.a11;
  a12       = a.a12;
  a13       = a.a13;
  a21       = a.a21;
  a22       = a.a22;
  a23       = a.a23;
}

//==================================================================================================

TScale::TScale(const TPointD &center, double sx, double sy) {
  TAffine a = TTranslation(center) * TScale(sx, sy) * TTranslation(-center);
  a11       = a.a11;
  a12       = a.a12;
  a13       = a.a13;
  a21       = a.a21;
  a22       = a.a22;
  a23       = a.a23;
}
//--------------------------------------------------------------------------------------------------
TScale::TScale(const TPointD &center, double s) {
  TAffine a = TTranslation(center) * TScale(s) * TTranslation(-center);
  a11       = a.a11;
  a12       = a.a12;
  a13       = a.a13;
  a21       = a.a21;
  a22       = a.a22;
  a23       = a.a23;
}
