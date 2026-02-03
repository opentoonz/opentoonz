

#include "tcubicbezier.h"

//-----------------------------------------------------------------------------

DVAPI double invCubicBezierX(double x, const TPointD &a, const TPointD &aSpeed,
                             const TPointD &bSpeed, const TPointD &b) {
  double aSpeedX            = aSpeed.x;
  double bSpeedX            = bSpeed.x;
  if (aSpeedX == 0) aSpeedX = epsilon;
  if (bSpeedX == 0) bSpeedX = -epsilon;

  // a*u^3+b*u^2+c*u+d
  double x0 = a.x;
  double x1 = x0 + aSpeedX;
  double x3 = b.x;
  double x2 = x3 + bSpeedX;

  double aX = x3 + 3 * (x1 - x2) - x0;
  double bX = 3 * (x2 - 2 * x1 + x0);
  double cX = 3 * (x1 - x0);
  double dX = x0 - x;

  return cubicRoot(aX, bX, cX, dX);
}

//-----------------------------------------------------------------------------

DVAPI double getCubicBezierY(double x, const TPointD &a, const TPointD &aSpeed,
                             const TPointD &bSpeed, const TPointD &b) {
  double y0 = a.y;
  double y1 = y0 + aSpeed.y;
  double y3 = b.y;
  double y2 = y3 + bSpeed.y;

  double aY = y3 + 3 * (y1 - y2) - y0;
  double bY = 3 * (y2 - 2 * y1 + y0);
  double cY = 3 * (y1 - y0);
  double dY = y0;

  double u = invCubicBezierX(x, a, aSpeed, bSpeed, b);
  u        = tcrop(u, 0.0, 1.0);
  return aY * u * u * u + bY * u * u + cY * u + dY;
}

//-----------------------------------------------------------------------------

DVAPI std::pair<TPointD, TPointD> getMinMaxCubicBezierY(const TPointD &a,
                                                        const TPointD &aSpeed,
                                                        const TPointD &bSpeed,
                                                        const TPointD &b) {
  double y0 = a.y;
  double y1 = y0 + aSpeed.y;
  double y3 = b.y;
  double y2 = y3 + bSpeed.y;

  double aY = y3 + 3 * (y1 - y2) - y0;
  double bY = 3 * (y2 - 2 * y1 + y0);
  double cY = 3 * (y1 - y0);
  double dY = y0;

  double x0 = a.x;
  double x1 = x0 + aSpeed.x;
  double x3 = b.x;
  double x2 = x3 + bSpeed.x;

  double aX = x3 + 3 * (x1 - x2) - x0;
  double bX = 3 * (x2 - 2 * x1 + x0);
  double cX = 3 * (x1 - x0);
  double dX = x0;

  double aaY = 3 * (y1 - y2) - y0 + y3;
  double bbY = 2 * (y0 + y2 - 2 * y1);
  double ccY = y1 - y0;

  if (aaY == 0) {
    if (a.y < b.y)
      return std::pair<TPointD, TPointD>(TPointD(a.x, a.y), TPointD(b.x, b.y));
    else
      return std::pair<TPointD, TPointD>(TPointD(b.x, b.y), TPointD(a.x, a.y));
  } else {
    double discr = bbY * bbY - 4 * aaY * ccY;
    if (discr < 0) {
      if (a.y < b.y)
        return std::pair<TPointD, TPointD>(TPointD(a.x, a.y),
                                           TPointD(b.x, b.y));
      else
        return std::pair<TPointD, TPointD>(TPointD(b.x, b.y),
                                           TPointD(a.x, a.y));
    } else {
      double sqrt_discr = sqrt(discr);
      double inv_2aY    = 1.0 / (2.0 * aaY);
      double u0         = (-bbY + sqrt_discr) * inv_2aY;
      double u1         = (-bbY - sqrt_discr) * inv_2aY;
      if (u0 > 1.0) u0  = 1.0;
      if (u0 < 0.0) u0  = 0.0;
      if (u1 > 1.0) u1  = 1.0;
      if (u1 < 0.0) u1  = 0.0;
      double y_0        = aY * u0 * u0 * u0 + bY * u0 * u0 + cY * u0 + dY;
      double y_1        = aY * u1 * u1 * u1 + bY * u1 * u1 + cY * u1 + dY;
      double x_0        = aX * u0 * u0 * u0 + bX * u0 * u0 + cX * u0 + dX;
      double x_1        = aX * u1 * u1 * u1 + bX * u1 * u1 + cX * u1 + dX;
      if (y_0 < y_1)
        return std::pair<TPointD, TPointD>(TPointD(x_0, y_0),
                                           TPointD(x_1, y_1));
      else
        return std::pair<TPointD, TPointD>(TPointD(x_1, y_1),
                                           TPointD(x_0, y_0));
    }
  }
}

//-----------------------------------------------------------------------------
