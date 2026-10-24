#pragma once

#ifndef TRAILSCALE_H
#define TRAILSCALE_H

#include <algorithm>
#include <cmath>

namespace TrailScale {
constexpr double minimum = 0.25;
constexpr double maximum = 10.0;
constexpr double normal  = 1.0;

// Reject nonfinite metadata before it reaches transforms or drawing bounds.
inline double normalized(double value) {
  return std::isfinite(value) ? (std::max)(minimum, (std::min)(maximum, value))
                              : normal;
}

inline double apply(double scale, double multiplier) {
  return scale * normalized(multiplier);
}

// Circumscribed radius of a centered source rectangle, valid at any Rotation.
inline double radius(double halfWidth, double halfHeight, double scale) {
  return std::hypot(halfWidth, halfHeight) * std::abs(scale);
}
}  // namespace TrailScale

#endif  // TRAILSCALE_H
