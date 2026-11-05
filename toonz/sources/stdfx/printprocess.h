#pragma once

#include "celdistance.h"
#include "celnoise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Offset printing: the image is separated into four plates (cyan, magenta,
// yellow and black), each printed through its own rotated dot screen, then
// recombined as inks on paper. Every pixel depends only on its global
// position and on source pixels at fixed offsets from it, so any tiling of
// the frame gives the same image.
namespace PrintProcess {

// Premultiplied RGBA in [0, 1].
struct Color {
  double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};
// Straight RGB in [0, 1].
struct Rgb {
  double r = 1.0, g = 1.0, b = 1.0;
};

enum class Shape { Round, Diamond, Line };
enum class Output { Print, Cyan, Magenta, Yellow, Black };
constexpr int plateCount = 4;

struct Plate {
  bool enabled   = true;
  double angle   = 0.0;                 // degrees
  double offsetX = 0.0, offsetY = 0.0;  // misregistration, in pixels
  Rgb ink;
};

struct Settings {
  double cellSize        = 6.0;  // screen cell, in pixels
  Shape shape            = Shape::Round;
  double blackGeneration = 0.8;  // share of the common grey printed in black
  double dotGain         = 0.1;  // ink spread, raising middle tones
  double softness        = 0.5;  // dot edge width, in pixels
  Rgb paper              = {0.988, 0.976, 0.941};
  double grain           = 0.15;  // paper texture, in paper and dot edges
  double grainSize       = 2.0;   // in pixels
  std::uint32_t seed     = 1;
  bool transparentBackground = false;  // keep the source's alpha
  Output output              = Output::Print;
  std::array<Plate, plateCount> plates;
  Settings() {
    const double angles[plateCount] = {15.0, 75.0, 0.0, 45.0};
    const Rgb inks[plateCount]      = {{0.0, 0.68, 0.94},
                                       {0.93, 0.0, 0.55},
                                       {1.0, 0.95, 0.0},
                                       {0.14, 0.12, 0.13}};
    for (int k = 0; k < plateCount; ++k) {
      plates[k].angle = angles[k];
      plates[k].ink   = inks[k];
    }
  }
};

inline void validate(const Settings &s) {
  auto finite = [](double v) { return std::isfinite(v); };
  bool ok     = finite(s.cellSize) && s.cellSize >= 1.0 &&
            finite(s.blackGeneration) && finite(s.dotGain) &&
            finite(s.softness) && s.softness >= 0.0 && finite(s.grain) &&
            finite(s.grainSize) && s.grainSize > 0.0;
  for (const Plate &p : s.plates)
    ok = ok && finite(p.angle) && finite(p.offsetX) && finite(p.offsetY) &&
         finite(p.ink.r) && finite(p.ink.g) && finite(p.ink.b);
  if (!ok) throw std::invalid_argument("Invalid print settings");
}

// The pixels around a tile that its result depends on: the largest
// misregistration and the sampling footprint.
inline int halo(const Settings &s) {
  validate(s);
  double reach = 0.0;
  for (const Plate &p : s.plates)
    if (p.enabled)
      reach =
          std::max(reach, std::max(std::abs(p.offsetX), std::abs(p.offsetY)));
  if (reach > 1 << 20)
    throw std::length_error("Misregistration exceeds raster dimensions");
  return int(std::ceil(reach)) + 2;
}

namespace detail {

inline double unit(double v) { return std::max(0.0, std::min(1.0, v)); }

inline double spot(Shape shape, double cx, double cy) {
  switch (shape) {
  case Shape::Diamond:
    return std::abs(cx) + std::abs(cy);
  case Shape::Line:
    return std::abs(cy);
  case Shape::Round:
  default:
    return cx * cx + cy * cy;
  }
}

// A cell position's threshold: the share of the cell whose spot value is
// lower. Inking where the tone exceeds it covers exactly the tone's share of
// the cell, for any dot shape.
class Screen {
  std::vector<double> m_sorted;
  Shape m_shape;

public:
  explicit Screen(Shape shape) : m_shape(shape) {
    const int n = 96;
    m_sorted.reserve(n * n);
    for (int j = 0; j < n; ++j)
      for (int i = 0; i < n; ++i)
        m_sorted.push_back(
            spot(shape, (i + 0.5) / n - 0.5, (j + 0.5) / n - 0.5));
    std::sort(m_sorted.begin(), m_sorted.end());
  }
  double threshold(double cx, double cy) const {
    const double value = spot(m_shape, cx, cy);
    const auto rank =
        std::lower_bound(m_sorted.begin(), m_sorted.end(), value) -
        m_sorted.begin();
    return (double(rank) + 0.5) / double(m_sorted.size());
  }
};

inline const Screen &screen(Shape shape) {
  static const Screen round(Shape::Round), diamond(Shape::Diamond),
      line(Shape::Line);
  return shape == Shape::Diamond ? diamond
         : shape == Shape::Line  ? line
                                 : round;
}

// Bilinear sample at pixel (x, y) moved by (ox, oy): the weights depend only
// on the offset, so every tile computes the same. Outside is clear.
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

// A colour's plate tones. Transparent is no ink.
inline std::array<double, plateCount> separate(const Color &c,
                                               double blackGeneration) {
  std::array<double, plateCount> tone = {0.0, 0.0, 0.0, 0.0};
  if (!(c.a > 1e-12)) return tone;
  const double cyan = unit(1.0 - c.r / c.a), magenta = unit(1.0 - c.g / c.a),
               yellow = unit(1.0 - c.b / c.a);
  const double black =
      unit(blackGeneration) * std::min(cyan, std::min(magenta, yellow));
  const double rest = 1.0 - black;
  tone[0]           = rest > 1e-12 ? (cyan - black) / rest : 0.0;
  tone[1]           = rest > 1e-12 ? (magenta - black) / rest : 0.0;
  tone[2]           = rest > 1e-12 ? (yellow - black) / rest : 0.0;
  tone[3]           = black;
  // Ink on part of a pixel is that much less ink
  for (double &t : tone) t = unit(t) * unit(c.a);
  return tone;
}

inline double grainAt(const Settings &s, std::uint32_t salt, double x,
                      double y) {
  if (s.grain <= 0.0) return 0.0;
  CelNoise::Settings noise;
  noise.type      = CelNoise::Smooth;
  noise.seed      = s.seed * 2654435761u + salt;
  noise.thickness = std::max(1.0, std::min(100.0, s.grainSize));
  return CelNoise::sample(noise, x, y) - 0.5;
}

}  // namespace detail

// Plate coverage at a global position, for a plate tone already sampled.
inline double coverage(const Settings &s, int plate, double tone, double x,
                       double y) {
  const Plate &p      = s.plates[std::size_t(plate)];
  const double gained = detail::unit(tone + s.dotGain * tone * (1.0 - tone));
  // Soft edges average to the tone's share of a cell, but not at the ends:
  // no ink is none and solid ink covers everything
  if (gained <= 0.0) return 0.0;
  if (gained >= 1.0) return 1.0;
  const double radians = p.angle * 3.14159265358979323846 / 180.0;
  const double c = std::cos(radians), sn = std::sin(radians);
  const double u  = (c * x + sn * y) / s.cellSize,
               v  = (-sn * x + c * y) / s.cellSize;
  const double cx = u - std::floor(u) - 0.5, cy = v - std::floor(v) - 0.5;
  double threshold = detail::screen(s.shape).threshold(cx, cy);
  // Paper fibres roughen the dot edges
  threshold += 0.25 * s.grain * detail::grainAt(s, 0x2545f491u + plate, x, y);
  // The threshold changes by about two over a cell: a pixel is 2/cellSize
  const double edge = std::max(1e-6, s.softness * 2.0 / s.cellSize);
  return detail::unit((gained - threshold) / edge + 0.5);
}

// Prints a w x h image whose pixel (0, 0) is at the global position
// (originX, originY). Returns false when canceled, with out cleared.
inline bool render(const std::vector<Color> &image, int w, int h,
                   double originX, double originY, const Settings &s,
                   std::vector<Color> &out, const int *canceled = nullptr) {
  validate(s);
  const std::size_t area = CelDistance::checkedArea(w, h);
  if (image.size() != area) throw std::invalid_argument("Image size mismatch");
  out.assign(area, Color());
  const int samples = 3;  // per axis, for antialiased dots
  for (int y = 0; y < h; ++y) {
    if (CelDistance::isCanceled(canceled)) {
      out.clear();
      return false;
    }
    for (int x = 0; x < w; ++x) {
      std::array<double, plateCount> cover = {0.0, 0.0, 0.0, 0.0};
      for (int k = 0; k < plateCount; ++k) {
        const Plate &plate = s.plates[std::size_t(k)];
        if (!plate.enabled) continue;
        double sum = 0.0;
        for (int j = 0; j < samples; ++j)
          for (int i = 0; i < samples; ++i) {
            // A sub-pixel position, and the plate moved by its offset
            const double sx = (i + 0.5) / samples - 0.5,
                         sy = (j + 0.5) / samples - 0.5;
            const double ox = sx - plate.offsetX, oy = sy - plate.offsetY;
            const Color c     = detail::sample(image, w, h, x, y, ox, oy);
            const double tone = detail::separate(c, s.blackGeneration)[k];
            sum += coverage(s, k, tone, originX + x + 0.5 + ox,
                            originY + y + 0.5 + oy);
          }
        cover[std::size_t(k)] = sum / (samples * samples);
      }
      Color result;
      if (s.output == Output::Print) {
        const double paperShade =
            1.0 + 0.3 * s.grain *
                      detail::grainAt(s, 0x9e3779b9u, originX + x + 0.5,
                                      originY + y + 0.5);
        double r = detail::unit(s.paper.r * paperShade),
               g = detail::unit(s.paper.g * paperShade),
               b = detail::unit(s.paper.b * paperShade);
        // Inks filter the light: each multiplies what is under it
        for (int k = 0; k < plateCount; ++k) {
          const double t = cover[std::size_t(k)];
          const Rgb &ink = s.plates[std::size_t(k)].ink;
          r *= 1.0 - t * (1.0 - ink.r);
          g *= 1.0 - t * (1.0 - ink.g);
          b *= 1.0 - t * (1.0 - ink.b);
        }
        result = Color{r, g, b, 1.0};
      } else {
        const double v = 1.0 - cover[std::size_t(int(s.output) - 1)];
        result         = Color{v, v, v, 1.0};
      }
      if (s.transparentBackground) {
        const double alpha = detail::unit(image[std::size_t(y) * w + x].a);
        result =
            Color{result.r * alpha, result.g * alpha, result.b * alpha, alpha};
      }
      out[std::size_t(y) * w + x] = result;
    }
  }
  return true;
}

}  // namespace PrintProcess
