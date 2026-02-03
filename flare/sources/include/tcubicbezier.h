#pragma once

#ifndef TCUBICBEZIER_INCLUDED
#define TCUBICBEZIER_INCLUDED

//-----------------------------------------------------------------------------

#include "tmathutil.h"
//#include "tcommon.h"
#include "tgeometry.h"
//#include <algorithm>

using TConsts::epsilon;

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------------------------------------

/*!
  Find the value of the parameter u0 of the cubic Bezier curve C(u)
  for a specified value x, with C(u0) = (x,y).
  \par x the x-component of a point on the Bezier curve
  \par a first control point of the Bezier curve
  \par aSpeed the tangent vector in a (that is the first control point minus the
  second)
  \par b the third control point
  \par bSpeed the tangent vector in b (that is the third control point minus the
  fourth)
  \ret the parameter value u0 of the Bezier curve C(u) for x, that is C(u0) =
  (x,y)
*/
DVAPI double invCubicBezierX(double x, const TPointD &a, const TPointD &aSpeed,
                             const TPointD &bSpeed, const TPointD &b);

//-----------------------------------------------------------------------------

/*!
  for C(u) a cubic Bezier curve, C(u0) = (x,y), find y from x.
  \par x the x-component of a point on the Bezier curve
  \par a first control point of the Bezier curve
  \par aSpeed the tangent vector in a (that is the first control point minus the
  second)
  \par b the third control point
  \par bSpeed the tangent vector in b (that is the third control point minus the
  fourth)
  \ret the y-component of a point P(x,y) on the Bezier curve, with C(u0) = P
*/
DVAPI double getCubicBezierY(double x, const TPointD &a, const TPointD &aSpeed,
                             const TPointD &bSpeed, const TPointD &b);

//-----------------------------------------------------------------------------

/*!
  for C(u)=(x,y) a cubic Bezier curve, find minimum and maximum y-values of C(u)
  \par a first control point of the Bezier curve
  \par aSpeed the tangent vector in a (that is the first control point minus the
  second)
  \par b the third control point
  \par bSpeed the tangent vector in b (that is the third control point minus the
  fourth)
  \ret a pair of points which are the minimum and maximum of the cubic Bezier
*/
DVAPI std::pair<TPointD, TPointD> getMinMaxCubicBezierY(const TPointD &a,
                                                        const TPointD &aSpeed,
                                                        const TPointD &bSpeed,
                                                        const TPointD &b);

//-----------------------------------------------------------------------------

#endif  // TCUBICBEZIER_INCLUDED

//-----------------------------------------------------------------------------
