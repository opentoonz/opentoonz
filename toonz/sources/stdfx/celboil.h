#pragma once

#include "celdistance.h"
#include "celnoise.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// A hand-drawn "boil": the image, or only its lines, displaced by a smooth
// noise field that holds for a number of frames, then changes. The field is
// a function of the global position, so tiles and preview sizes agree.
namespace CelBoil {

// Premultiplied RGBA in [0, 1].
struct Color {
  double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};

enum class Target { Image, Lines };

struct Settings {
  double amplitude   = 1.5;   // largest displacement, in pixels
  double size        = 12.0;  // feature size of the wobble, in pixels (1..100)
  double detail      = 0.0;   // a finer second layer, as a share of amplitude
  int cycle          = 2;     // frames each wobble is held
  double blend       = 0.0;   // share of a cycle spent changing to the next
  std::uint32_t seed = 1;
  Target target      = Target::Image;
  double lineThreshold = 0.35;  // Lines: darker pixels are line
};

inline void validate(const Settings &s) {
  if (!std::isfinite(s.amplitude) || s.amplitude < 0.0 ||
      !std::isfinite(s.size) || s.size <= 0.0 || !std::isfinite(s.detail) ||
      s.detail < 0.0 || s.cycle < 1 || !std::isfinite(s.blend) ||
      s.blend < 0.0 || s.blend > 1.0 || !std::isfinite(s.lineThreshold))
    throw std::invalid_argument("Invalid boil settings");
}

// How far a pixel can move, plus the sampling footprint.
inline double reach(const Settings &s) {
  validate(s);
  const double value = s.amplitude * (1.0 + s.detail) + 2.0;
  if (value > 1 << 20)
    throw std::length_error("Boil amplitude exceeds raster dimensions");
  return value;
}

// The pixels around a tile that its result depends on: the reach, and for
// lines their antialiased edge.
inline int halo(const Settings &s) {
  return int(std::ceil(reach(s))) + (s.target == Target::Lines ? 2 : 0);
}

// The noise plane for a frame: whole while a wobble is held, fractional
// while blending into the next one.
inline double phase(const Settings &s, double frame) {
  validate(s);
  if (!std::isfinite(frame)) return 0.0;
  const double steps  = frame / s.cycle;
  const double plane  = std::floor(steps + 1e-9);
  const double within = std::max(0.0, steps - plane);  // [0, 1)
  if (s.blend <= 0.0 || within <= 1.0 - s.blend) return plane;
  const double t = (within - (1.0 - s.blend)) / s.blend;
  return plane + t * t * (3.0 - 2.0 * t);
}

// The displacement at a global position.
inline void displacement(const Settings &s, double offset, double x, double y,
                         double &dx, double &dy) {
  CelNoise::Settings noise;
  noise.type      = CelNoise::Smooth;
  noise.offset    = offset;
  noise.thickness = std::max(1.0, std::min(100.0, s.size));
  auto channel    = [&](std::uint32_t salt, double scale) {
    noise.seed      = s.seed * 2654435761u + salt;
    noise.thickness = std::max(1.0, std::min(100.0, s.size * scale));
    return 2.0 * CelNoise::sample(noise, x, y) - 1.0;
  };
  dx = s.amplitude * channel(0x68e31da4u, 1.0);
  dy = s.amplitude * channel(0xb5297a4du, 1.0);
  if (s.detail > 0.0) {
    dx += s.amplitude * s.detail * channel(0x1b56c4e9u, 1.0 / 3.0);
    dy += s.amplitude * s.detail * channel(0x7f4a7c15u, 1.0 / 3.0);
  }
}

namespace detail {

// Bilinear sample of premultiplied colours at pixel (x, y) moved by
// (ox, oy); outside the image is clear. The weights depend only on the
// offset, never on the pixel's position, so every tile computes the same.
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

inline bool isLine(const Color &c, double threshold) {
  if (!(c.a > 0.0)) return false;
  const double luma = (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) / c.a;
  return luma <= threshold;
}

}  // namespace detail

// Boils a w x h image whose pixel (0, 0) is at the global position
// (originX, originY). Returns false when canceled, with out cleared.
inline bool render(const std::vector<Color> &image, int w, int h,
                   double originX, double originY, const Settings &s,
                   double frame, std::vector<Color> &out,
                   const int *canceled = nullptr) {
  validate(s);
  const std::size_t area = CelDistance::checkedArea(w, h);
  if (image.size() != area) throw std::invalid_argument("Image size mismatch");
  const double offset = phase(s, frame);
  out.assign(area, Color());

  // What moves, and what stays under it
  const std::vector<Color> *moving = &image;
  std::vector<Color> lines, paint;
  if (s.target == Target::Lines) {
    std::vector<std::uint8_t> line(area);
    for (std::size_t i = 0; i < area; ++i)
      line[i] = detail::isLine(image[i], s.lineThreshold);
    // The antialiased edge of a line moves with it
    std::vector<std::uint8_t> withEdge;
    if (!CelDistance::thin(line, w, h, 1.0, CelDistance::Metric::Euclidean,
                           withEdge, canceled)) {
      out.clear();
      return false;
    }
    // The paint under a line is the nearest paint within the reach; deeper
    // in a thick line, which moves less than its width, the line itself
    const double fill = reach(s);
    std::vector<std::uint8_t> paintSeed(area);
    for (std::size_t i = 0; i < area; ++i) paintSeed[i] = !withEdge[i];
    CelDistance::Field field;
    if (!CelDistance::transform(paintSeed, w, h, CelDistance::Metric::Euclidean,
                                field, canceled)) {
      out.clear();
      return false;
    }
    lines.assign(area, Color());
    paint.assign(area, Color());
    for (std::size_t i = 0; i < area; ++i) {
      if (withEdge[i]) {
        lines[i] = image[i];
        paint[i] = field.nearest[i] >= 0 && field.distance[i] <= fill
                       ? image[std::size_t(field.nearest[i])]
                       : image[i];
      } else
        paint[i] = image[i];
    }
    moving = &lines;
  }

  for (int y = 0; y < h; ++y) {
    if (CelDistance::isCanceled(canceled)) {
      out.clear();
      return false;
    }
    for (int x = 0; x < w; ++x) {
      double dx, dy;
      displacement(s, offset, originX + x + 0.5, originY + y + 0.5, dx, dy);
      Color c = detail::sample(*moving, w, h, x, y, -dx, -dy);
      if (s.target == Target::Lines) {
        const Color &p    = paint[std::size_t(y) * w + x];
        const double keep = 1.0 - c.a;
        c = Color{c.r + p.r * keep, c.g + p.g * keep, c.b + p.b * keep,
                  c.a + p.a * keep};
      }
      out[std::size_t(y) * w + x] = c;
    }
  }
  return true;
}

}  // namespace CelBoil
