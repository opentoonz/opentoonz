#pragma once

#include "celdistance.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Lines along the boundaries between flat colour regions. Everything is
// local: a pixel's result depends only on the pixels within halo() of it, so
// any tiling of the frame gives the same image.
//
// 1. A pixel is transparent when its alpha is at most transparentAlpha.
//    Another pixel is flat when at least flatNeighbours of its 8 neighbours
//    have its colour, within tolerance. Antialiased ramps and thin details
//    are not flat.
// 2. A pixel that is neither takes the colour of the nearest flat or
//    transparent pixel within gap; farther from any, it has no region.
// 3. Two 4-neighbours whose region colours differ by more than tolerance
//    share a boundary. The first enabled rule whose two ends match the two
//    regions decides the line's weight and colour; no match, no line.
// 4. A line covers the pixels around its boundary: a pixel at distance d
//    from the nearest boundary pixel gets coverage weight/2 - d, clamped to
//    [0, 1], so a line of weight w is w pixels wide in total.
namespace CelEdge {

// Premultiplied RGBA in [0, 1].
struct Color {
  double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};

enum class Kind { Any, Color, Transparent };

struct End {
  Kind kind = Kind::Any;
  // Kind::Color: the region colour, straight (not premultiplied)
  double r = 0.0, g = 0.0, b = 0.0;
};

struct Rule {
  bool enabled = false;
  End first, second;
  double weight = 2.0;                   // in pixels
  Color line    = {0.0, 0.0, 0.0, 1.0};  // straight RGB and its opacity
};

struct Settings {
  double tolerance        = 0.03;  // region colours, per channel
  double matchTolerance   = 0.1;   // rule colours against region colours
  double gap              = 2.0;   // antialiased ramps, in pixels
  double transparentAlpha = 1.0 / 512.0;
  int flatNeighbours      = 4;
  double opacity          = 1.0;
  bool linesOnly          = false;
};

inline bool validRule(const Rule &rule) {
  return std::isfinite(rule.weight) && rule.weight >= 0.0 &&
         std::isfinite(rule.line.r) && std::isfinite(rule.line.g) &&
         std::isfinite(rule.line.b) && std::isfinite(rule.line.a);
}

inline void validate(const Settings &settings, const std::vector<Rule> &rules) {
  if (!std::isfinite(settings.tolerance) || settings.tolerance < 0.0 ||
      !std::isfinite(settings.matchTolerance) ||
      settings.matchTolerance < 0.0 || !std::isfinite(settings.gap) ||
      settings.gap < 0.0 || !std::isfinite(settings.transparentAlpha) ||
      settings.flatNeighbours < 0 || settings.flatNeighbours > 8 ||
      !std::isfinite(settings.opacity))
    throw std::invalid_argument("Invalid edge line settings");
  for (const Rule &rule : rules)
    if (rule.enabled && !validRule(rule))
      throw std::invalid_argument("Invalid edge line rule");
}

inline double maxWeight(const std::vector<Rule> &rules) {
  double weight = 0.0;
  for (const Rule &rule : rules)
    if (rule.enabled && validRule(rule)) weight = std::max(weight, rule.weight);
  return weight;
}

// The pixels around a tile that its result depends on: the line reach, the
// boundary test, the ramp search and the flat test.
inline int halo(const Settings &settings, const std::vector<Rule> &rules) {
  const double reach = maxWeight(rules) / 2.0 + 1.0 + settings.gap + 1.0;
  if (!std::isfinite(reach) || reach > 1 << 20)
    throw std::length_error("Edge line weight exceeds raster dimensions");
  return int(std::ceil(reach)) + 1;
}

namespace detail {

inline bool transparent(const Color &c, const Settings &settings) {
  return !(c.a > settings.transparentAlpha);
}

inline bool similar(const Color &a, const Color &b, double tolerance) {
  return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance &&
         std::abs(a.b - b.b) <= tolerance && std::abs(a.a - b.a) <= tolerance;
}

inline bool matches(const End &end, const Color &region, bool isTransparent,
                    double tolerance) {
  switch (end.kind) {
  case Kind::Any:
    return !isTransparent;
  case Kind::Transparent:
    return isTransparent;
  case Kind::Color:
    if (isTransparent) return false;
    return std::abs(region.r / region.a - end.r) <= tolerance &&
           std::abs(region.g / region.a - end.g) <= tolerance &&
           std::abs(region.b / region.a - end.b) <= tolerance;
  }
  return false;
}

}  // namespace detail

// Each region's colour, for the pixels of a w x h image: the pixel's own
// colour, or the nearest flat or transparent one; region[i].a < 0 means no
// region. Returns false when canceled.
inline bool regions(const std::vector<Color> &image, int w, int h,
                    const Settings &settings, std::vector<Color> &region,
                    std::vector<std::uint8_t> &isTransparent,
                    const int *canceled = nullptr) {
  const std::size_t area = CelDistance::checkedArea(w, h);
  if (image.size() != area) throw std::invalid_argument("Image size mismatch");
  std::vector<std::uint8_t> seed(area);
  isTransparent.assign(area, 0);
  for (int y = 0; y < h; ++y) {
    if (CelDistance::isCanceled(canceled)) return false;
    for (int x = 0; x < w; ++x) {
      const std::size_t i = std::size_t(y) * w + x;
      const Color &c      = image[i];
      if (detail::transparent(c, settings)) {
        seed[i] = isTransparent[i] = 1;
        continue;
      }
      int same = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          if (!dx && !dy) continue;
          const int nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
          if (detail::similar(c, image[std::size_t(ny) * w + nx],
                              settings.tolerance))
            ++same;
        }
      seed[i] = same >= settings.flatNeighbours;
    }
  }
  CelDistance::Field field;
  if (!CelDistance::transform(seed, w, h, CelDistance::Metric::Euclidean, field,
                              canceled))
    return false;
  region.assign(area, Color{0.0, 0.0, 0.0, -1.0});
  for (std::size_t i = 0; i < area; ++i) {
    if ((i & 1023) == 0 && CelDistance::isCanceled(canceled)) return false;
    const int nearest = field.nearest[i];
    if (nearest < 0 || field.distance[i] > settings.gap) continue;
    isTransparent[i] = isTransparent[std::size_t(nearest)];
    region[i]        = isTransparent[i] ? Color{0.0, 0.0, 0.0, 0.0}
                                        : image[std::size_t(nearest)];
  }
  return true;
}

// The index of the first enabled rule matching the two regions, or -1.
inline int ruleFor(const Color &a, bool aTransparent, const Color &b,
                   bool bTransparent, const std::vector<Rule> &rules,
                   double tolerance) {
  for (std::size_t r = 0; r < rules.size(); ++r) {
    const Rule &rule = rules[r];
    if (!rule.enabled || !validRule(rule) || rule.weight <= 0.0) continue;
    if ((detail::matches(rule.first, a, aTransparent, tolerance) &&
         detail::matches(rule.second, b, bTransparent, tolerance)) ||
        (detail::matches(rule.first, b, bTransparent, tolerance) &&
         detail::matches(rule.second, a, aTransparent, tolerance)))
      return int(r);
  }
  return -1;
}

// Line colour and coverage, premultiplied, for every pixel of the image.
// Returns false when canceled, with lines cleared.
inline bool lines(const std::vector<Color> &image, int w, int h,
                  const Settings &settings, const std::vector<Rule> &rules,
                  std::vector<Color> &out, const int *canceled = nullptr) {
  validate(settings, rules);
  const std::size_t area = CelDistance::checkedArea(w, h);
  out.assign(area, Color());
  std::vector<Color> region;
  std::vector<std::uint8_t> isTransparent;
  if (!regions(image, w, h, settings, region, isTransparent, canceled)) {
    out.clear();
    return false;
  }
  // Boundary pixels, with the rule of their boundary (the first rule, when
  // a pixel is on several)
  std::vector<std::uint8_t> seed(area);
  std::vector<int> seedRule(area, -1);
  auto mark = [&](std::size_t i, int rule) {
    seed[i] = 1;
    if (seedRule[i] < 0 || rule < seedRule[i]) seedRule[i] = rule;
  };
  for (int y = 0; y < h; ++y) {
    if (CelDistance::isCanceled(canceled)) {
      out.clear();
      return false;
    }
    for (int x = 0; x < w; ++x) {
      const std::size_t i = std::size_t(y) * w + x;
      if (region[i].a < 0.0) continue;
      const std::size_t neighbours[2] = {x + 1 < w ? i + 1 : i,
                                         y + 1 < h ? i + w : i};
      for (std::size_t j : neighbours) {
        if (j == i || region[j].a < 0.0) continue;
        if (isTransparent[i] == isTransparent[j] &&
            (isTransparent[i] ||
             detail::similar(region[i], region[j], settings.tolerance)))
          continue;
        const int rule =
            ruleFor(region[i], isTransparent[i], region[j], isTransparent[j],
                    rules, settings.matchTolerance);
        if (rule < 0) continue;
        mark(i, rule);
        mark(j, rule);
      }
    }
  }
  CelDistance::Field field;
  if (!CelDistance::transform(seed, w, h, CelDistance::Metric::Euclidean, field,
                              canceled)) {
    out.clear();
    return false;
  }
  const double opacity = std::max(0.0, std::min(1.0, settings.opacity));
  for (std::size_t i = 0; i < area; ++i) {
    if ((i & 1023) == 0 && CelDistance::isCanceled(canceled)) {
      out.clear();
      return false;
    }
    const int nearest = field.nearest[i];
    if (nearest < 0) continue;
    const Rule &rule = rules[std::size_t(seedRule[std::size_t(nearest)])];
    const double coverage =
        std::max(0.0, std::min(1.0, rule.weight / 2.0 - field.distance[i]));
    const double alpha =
        coverage * opacity * std::max(0.0, std::min(1.0, rule.line.a));
    if (alpha <= 0.0) continue;
    out[i] = Color{rule.line.r * alpha, rule.line.g * alpha,
                   rule.line.b * alpha, alpha};
  }
  return true;
}

// The image with its lines: over the image, or the lines alone.
inline bool render(const std::vector<Color> &image, int w, int h,
                   const Settings &settings, const std::vector<Rule> &rules,
                   std::vector<Color> &out, const int *canceled = nullptr) {
  std::vector<Color> drawn;
  if (!lines(image, w, h, settings, rules, drawn, canceled)) {
    out.clear();
    return false;
  }
  if (!settings.linesOnly)
    for (std::size_t i = 0; i < drawn.size(); ++i) {
      const Color &s    = image[i];
      Color &l          = drawn[i];
      const double keep = 1.0 - l.a;
      l = Color{l.r + s.r * keep, l.g + s.g * keep, l.b + s.b * keep,
                l.a + s.a * keep};
    }
  out.swap(drawn);
  return true;
}

}  // namespace CelEdge
