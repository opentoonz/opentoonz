#pragma once

#include "celdistance.h"
#include "celnoise.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Displacement driven by a second image: a vector field (red and green are
// x and y, 0.5 meaning no motion, as the motion flow FX write them) or a
// grey map pushing along a direction, weighted by a matte, plus a drifting
// noise for secondary motion. Every pixel depends only on its global
// position and on pixels within halo() of it, so tiles agree exactly.
namespace FlowDisplace {

// Premultiplied RGBA in [0, 1].
struct Color {
  double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};

enum class Mode { Flow, Map };

struct Settings {
  Mode mode             = Mode::Flow;
  double strength       = 10.0;  // pixels for a full-scale field
  double direction      = 0.0;   // Map: degrees
  bool invert           = false;
  double noiseAmplitude = 0.0;   // pixels
  double noiseSize      = 20.0;  // pixels (1..100)
  double noiseSpeed     = 0.1;   // noise planes per frame
  std::uint32_t seed    = 1;
  bool useMatte         = false;
};

inline void validate(const Settings &s) {
  if (!std::isfinite(s.strength) || !std::isfinite(s.direction) ||
      !std::isfinite(s.noiseAmplitude) || s.noiseAmplitude < 0.0 ||
      !std::isfinite(s.noiseSize) || s.noiseSize <= 0.0 ||
      !std::isfinite(s.noiseSpeed))
    throw std::invalid_argument("Invalid displacement settings");
}

// The pixels around a tile that its result depends on.
inline int halo(const Settings &s) {
  validate(s);
  const double reach = std::abs(s.strength) + s.noiseAmplitude * 1.5 + 2.0;
  if (reach > 1 << 20)
    throw std::length_error("Displacement exceeds raster dimensions");
  return int(std::ceil(reach));
}

namespace detail {

inline double unit(double v) { return std::max(0.0, std::min(1.0, v)); }

inline Color sample(const std::vector<Color> &image, int w, int h, int x, int y,
                    double ox, double oy) {
  const double fx = std::floor(ox), fy = std::floor(oy);
  const int x0 = x + int(fx), y0 = y + int(fy);
  const double tx = ox - fx, ty = oy - fy;
  Color out;
  for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
      const int px = x0 + i, py = y0 + j;
      if (px < 0 || py < 0 || px >= w || py >= h) continue;
      const double weight = (i ? tx : 1.0 - tx) * (j ? ty : 1.0 - ty);
      const Color &c      = image[std::size_t(py) * w + px];
      out.r += c.r * weight;
      out.g += c.g * weight;
      out.b += c.b * weight;
      out.a += c.a * weight;
    }
  return out;
}

}  // namespace detail

// The displacement at a pixel, from its field and matte colours, at a
// global position and frame.
inline void displacement(const Settings &s, const Color &field,
                         const Color *matte, double x, double y, double frame,
                         double &dx, double &dy) {
  dx = dy = 0.0;
  if (field.a > 1e-12) {
    // Straight colour: a half-covered field pixel is the same motion
    const double r = field.r / field.a, g = field.g / field.a,
                 b = field.b / field.a;
    if (s.mode == Mode::Flow) {
      dx = (2.0 * r - 1.0) * s.strength;
      dy = (2.0 * g - 1.0) * s.strength;
    } else {
      const double luma    = 0.299 * r + 0.587 * g + 0.114 * b;
      const double amount  = (2.0 * luma - 1.0) * s.strength;
      const double radians = s.direction * 3.14159265358979323846 / 180.0;
      dx                   = amount * std::cos(radians);
      dy                   = amount * std::sin(radians);
    }
    // The field's own coverage fades its motion
    dx *= detail::unit(field.a);
    dy *= detail::unit(field.a);
  }
  if (s.noiseAmplitude > 0.0) {
    CelNoise::Settings noise;
    noise.type      = CelNoise::Smooth;
    noise.thickness = std::max(1.0, std::min(100.0, s.noiseSize));
    noise.offset    = std::isfinite(frame) ? frame * s.noiseSpeed : 0.0;
    noise.seed      = s.seed * 2654435761u + 0x3c6ef372u;
    dx += s.noiseAmplitude * (2.0 * CelNoise::sample(noise, x, y) - 1.0);
    noise.seed = s.seed * 2654435761u + 0xa54ff53au;
    dy += s.noiseAmplitude * (2.0 * CelNoise::sample(noise, x, y) - 1.0);
  }
  if (s.invert) dx = -dx, dy = -dy;
  if (s.useMatte) {
    const double weight = matte ? detail::unit(matte->a) : 0.0;
    dx *= weight;
    dy *= weight;
  }
}

// Displaces a w x h image whose pixel (0, 0) is at the global position
// (originX, originY); field and matte cover the same pixels (empty when not
// connected). Returns false when canceled, with out cleared.
inline bool render(const std::vector<Color> &image,
                   const std::vector<Color> &field,
                   const std::vector<Color> &matte, int w, int h,
                   double originX, double originY, const Settings &s,
                   double frame, std::vector<Color> &out,
                   const int *canceled = nullptr) {
  validate(s);
  const std::size_t area = CelDistance::checkedArea(w, h);
  if (image.size() != area || (!field.empty() && field.size() != area) ||
      (!matte.empty() && matte.size() != area))
    throw std::invalid_argument("Image size mismatch");
  out.assign(area, Color());
  const Color none;
  for (int y = 0; y < h; ++y) {
    if (CelDistance::isCanceled(canceled)) {
      out.clear();
      return false;
    }
    for (int x = 0; x < w; ++x) {
      const std::size_t i = std::size_t(y) * w + x;
      double dx, dy;
      displacement(s, field.empty() ? none : field[i],
                   matte.empty() ? nullptr : &matte[i], originX + x + 0.5,
                   originY + y + 0.5, frame, dx, dy);
      // Each pixel shows the source from where the field pulls it
      out[i] = detail::sample(image, w, h, x, y, -dx, -dy);
    }
  }
  return true;
}

}  // namespace FlowDisplace
