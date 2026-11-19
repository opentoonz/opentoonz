#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace CelRadial {
struct Point {
  double x = 0.0, y = 0.0;
};
// The host raster allocator multiplies dimensions and pixel size as signed int.
// Check using size_t before asking it to allocate, including the final product.
inline bool fitsRasterAllocation(int width, int height, int bytesPerPixel) {
  if (width <= 0 || height <= 0 || bytesPerPixel <= 0) return false;
  const std::size_t maximum = std::size_t(std::numeric_limits<int>::max());
  const std::size_t w = std::size_t(width), h = std::size_t(height),
                    b = std::size_t(bytesPerPixel);
  return w <= maximum / h && w * h <= maximum / b;
}

// A zero ratio is the limiting, very thin ellipse. Tangential motion uses arc
// distance bounded by the requested length, so every sample fits the halo.
inline Point trajectory(Point position, Point center, double ratio,
                        double angleDegrees, double length, bool rotation,
                        bool outer) {
  const double ratioSafe = std::max(0.001, std::min(5.0, ratio));
  const double angle     = angleDegrees * (3.14159265358979323846 / 180.0);
  const double cosine = std::cos(angle), sine = std::sin(angle);
  const double x = position.x - center.x, y = position.y - center.y;
  const double rotatedX = cosine * x + sine * y;
  const double rotatedY = -sine * x + cosine * y;
  const double qx = rotatedX / ratioSafe, qy = rotatedY;
  const double ellipticalRadius = std::hypot(qx, qy), radius = std::hypot(x, y);
  if (!std::isfinite(radius) || !std::isfinite(ellipticalRadius) ||
      !(radius > 1e-12) || !(ellipticalRadius > 1e-12) || !(length > 0.0))
    return position;
  if (!rotation) {
    const double variation =
        ratioSafe >= 1.0 ? std::min(1.0, ellipticalRadius / radius)
                         : std::min(1.0, ratioSafe * ellipticalRadius / radius);
    // The source sample for an outward trail is towards the center.
    const double signedLength = (outer ? -1.0 : 1.0) * length * variation;
    return {position.x + signedLength * x / radius,
            position.y + signedLength * y / radius};
  }
  // Image coordinates are Cartesian: reverse the source trajectory to produce
  // an outward clockwise or inward counterclockwise output trail.
  const double theta = (outer ? 1.0 : -1.0) * length /
                       (ellipticalRadius * std::max(1.0, ratioSafe));
  const double ct = std::cos(theta), st = std::sin(theta);
  const double sampleX = (ct * qx - st * qy) * ratioSafe;
  const double sampleY = st * qx + ct * qy;
  return {center.x + cosine * sampleX - sine * sampleY,
          center.y + sine * sampleX + cosine * sampleY};
}
}  // namespace CelRadial
