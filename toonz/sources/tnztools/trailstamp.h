#pragma once

#include "tgeometry.h"

#include <array>
#include <cmath>
#include <vector>

namespace TrailStamp {

// Two straight quadratics preserve the click anchor and style rotation.
// One unit survives PLI quantization; minimum stamp spacing is two.
inline bool makeClickCarrier(const std::vector<TThickPoint> &points,
                             std::array<TThickPoint, 5> &carrier) {
  if (points.empty()) return false;
  const TThickPoint &first = points.front();
  double thickness         = 0.0;
  for (const TThickPoint &point : points) {
    // Reject loops and drags, however small.
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        !std::isfinite(point.thick) || point.x != first.x || point.y != first.y)
      return false;
    // Keep peak tap pressure, not the zero-pressure release.
    thickness = std::max(thickness, point.thick);
  }
  if (thickness <= 0.0 || first.x + 0.25 == first.x) return false;

  for (size_t i = 0; i < carrier.size(); ++i)
    carrier[i] = TThickPoint(first.x + 0.25 * i, first.y, thickness);
  return true;
}

}  // namespace TrailStamp
