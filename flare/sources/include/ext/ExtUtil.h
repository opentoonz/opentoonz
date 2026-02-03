#pragma once

#ifndef TNZ_EXTUTIL_H
#define TNZ_EXTUTIL_H

/**
 * @author  Fabrizio Morciano <fabrizio.morciano@gmail.com>
 */

#include "tcommon.h"
#include <tvectorimage.h>

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "ext/Types.h"

namespace ToonzExt {
// Functions for straight corners management.
/**
   * @brief Like cornersDetector but for straight corners.
   */
DVAPI bool straightCornersDetector(const TStroke *stroke,
                                   std::vector<double> &corners);

/**
   * @brief Find intervals where stroke is a straight line.
   *
   * These intervals are stored in an @c vector od Interval.
   */
DVAPI bool detectStraightIntervals(const TStroke *stroke,
                                   ToonzExt::Intervals &intervals,
                                   double tolerance = TConsts::epsilon);

/**
   * @brief Retrieve parameters for straight intervals
   * near actualW parameter.
   *
   * @return If not found return [-1,-1] interval, else
   *         the correct interval.
   */
DVAPI bool findNearestStraightCorners(const TStroke *s, double actualW,
                                      Interval &out,
                                      const ToonzExt::Intervals *const cl = 0,
                                      double tolerance = TConsts::epsilon);

/**
   */
DVAPI bool isAStraightCorner(const TStroke *s, double actualW,
                             const ToonzExt::Intervals *const cl = 0,
                             double tolerance = TConsts::epsilon);

// Functions for spire corners management.

/**
   * @brief Enhanced version of Toonz detect corners.
   */
DVAPI bool cornersDetector(const TStroke *stroke, int minDegree,
                           std::vector<double> &corners);

/**
   * @brief Find intervals where stroke has limited from spire.
   *
   * These intervals are stored in an @c vector od Interval.
   */
DVAPI bool detectSpireIntervals(const TStroke *stroke,
                                ToonzExt::Intervals &intervals, int minDegree);

/**
   * @brief Retrieve parameters for corners near actualW parameter.
   */
DVAPI bool findNearestSpireCorners(const TStroke *s, double actualW,
                                   Interval &out, int cornerSize,
                                   const ToonzExt::Intervals *const cl = 0,
                                   double tolerance = TConsts::epsilon);

/**
   */
DVAPI bool isASpireCorner(const TStroke *s, double w, int cornerSize,
                          const ToonzExt::Intervals *const cl = 0,
                          double tolerance = TConsts::epsilon);

/**
   * @brief Retrieve corners of a TStroke.
   */
DVAPI void findCorners(const TStroke *stroke, Intervals &corners,
                       Intervals &intervals, int angle, double tolerance);

/**
   * @brief Retrieve corners of a TStroke.
   */
DVAPI bool findNearestCorners(const TStroke *stroke, double w, Interval &out,
                              const Intervals &values,
                              double tolerance = TConsts::epsilon);

//  Stroke related functions.

/**
   * @brief Rotate the control point at even position [0,2,4,...] in a
   * new position along the close path of a selfLoop stroke.
   *
   * The new position is: @b atLength.
   * @return 0 if some problem occurs, else the new stroke.
   * @todo Some refactoring required, code duplication.
   */
DVAPI TStroke *rotateControlPoint(const TStroke *stroke,
                                  const ToonzExt::EvenInt &even,
                                  double atLength);

/**
   * Retrieve all parameters where stroke has a minimum.
   */
DVAPI bool getAllW(const TStroke *stroke, const TPointD &pnt, double &dist2,
                   std::vector<double> &parameters);
/**
   * @brief Replace a stroke in a vector image.
   */
DVAPI bool replaceStroke(TStroke *old_stroke, TStroke *new_stroke,
                         unsigned int n_, TVectorImageP &vi);

/**
   * @brief Copy several parameters that are not
   *        copied with operator=
   */
void cloneStrokeStatus(const TStroke *from, TStroke *to);

//---------------------------------------------------------------------------

template <class T>
inline bool isValid(const T *s) {
  assert(0 != s);
  if (!s) return false;
  return true;
}

//---------------------------------------------------------------------------

inline bool isValid(double w) {
  assert(0.0 <= w && w <= 1.0);
  if (w < 0.0 || w > 1.0) return false;
  return true;
}

//---------------------------------------------------------------------------
}

#endif /* TNZ_EXTUTIL_H */
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
