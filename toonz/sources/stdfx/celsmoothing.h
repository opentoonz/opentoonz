#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

// Native contour-area smoothing. Classification always uses the original
// premultiplied image; the two directional passes accumulate coverage in a
// separate buffer. There is no quantization or hidden shared render state.
namespace CelSmoothing {
struct Pixel {
  double r = 0, g = 0, b = 0, a = 0;
};
using Mix = std::function<Pixel(const Pixel &, const Pixel &, double)>;

inline Pixel mix(const Pixel &a, const Pixel &b, double amount) {
  return {a.r * (1 - amount) + b.r * amount, a.g * (1 - amount) + b.g * amount,
          a.b * (1 - amount) + b.b * amount, a.a * (1 - amount) + b.a * amount};
}

inline std::size_t pixelCount(int width, int height) {
  if (width < 0 || height < 0 ||
      (height && std::size_t(width) > std::numeric_limits<std::size_t>::max() /
                                          std::size_t(height) / sizeof(Pixel)))
    throw std::length_error("Cel smoother: invalid image dimensions");
  return std::size_t(width) * std::size_t(height);
}

// Range is measured in 8-bit display-channel units. Zero groups exactly equal
// colors; 255 disables contour smoothing. Strength 100 uses half-pixel contour
// coverage. An optional blend policy supports transfer functions in later FX.
inline bool smooth(int width, int height, const std::vector<Pixel> &source,
                   std::vector<Pixel> &destination, double range,
                   double strength, const int *canceled = nullptr,
                   const Mix &blend = Mix()) {
  const std::size_t count = pixelCount(width, height);
  if (source.size() != count || &source == &destination)
    throw std::invalid_argument("Cel smoother: invalid image storage");
  const auto stopped = [canceled] { return canceled && *canceled; };
  if (stopped()) {
    destination.clear();
    return false;
  }
  destination = source;
  if (!count || range >= 255 || !(strength > 0)) return true;
  range    = std::isfinite(range) ? std::max(0.0, range) / 255.0 : 0;
  strength = std::isfinite(strength) ? std::min(100.0, strength) : 100;
  const double slope = 100.0 / strength;
  const auto equal   = [range](const Pixel &a, const Pixel &b) {
    // Normalizing integer channels can round a boundary subtraction a few
    // ulps upward. Preserve the inclusive integer range without broadening
    // zero-range exact matching.
    const double tolerance =
        range > 0 ? range + 4 * std::numeric_limits<double>::epsilon() : 0;
    return std::max({std::abs(a.r - b.r), std::abs(a.g - b.g),
                     std::abs(a.b - b.b), std::abs(a.a - b.a)}) <= tolerance;
  };
  const auto interpolate = [&blend](const Pixel &a, const Pixel &b, double w) {
    return blend ? blend(a, b, w) : mix(a, b, w);
  };
  for (int axis = 0; axis != 2; ++axis) {
    const int length = axis ? height : width;
    const int rows   = axis ? width : height;
    const auto index = [axis, width](int x, int y) {
      return axis ? std::size_t(x) * width + y : std::size_t(y) * width + x;
    };
    const auto at = [&](int x, int y) -> const Pixel & {
      return source[index(x, y)];
    };
    // At a four-color crossing, join the less frequent diagonal. All samples
    // are checked by coordinates, including single-row and narrow images.
    const auto minority = [&](int x, int y) {
      int first = 0, second = 0;
      const Pixel &a = at(x - 1, y), &b = at(x, y);
      const auto sample = [&](int sx, int sy) {
        first += equal(a, at(sx, sy));
        second += equal(b, at(sx, sy));
      };
      if (y > 1) {
        sample(x, y - 2);
        sample(x - 1, y - 2);
      }
      if (y < rows - 1) {
        sample(x, y + 1);
        sample(x - 1, y + 1);
      }
      if (x > 1) {
        sample(x - 2, y);
        sample(x - 2, y - 1);
      }
      if (x < length - 1) {
        sample(x + 1, y);
        sample(x + 1, y - 1);
      }
      return first > second;
    };
    for (int y = 0; y + 1 < rows; ++y) {
      if (stopped()) {
        destination.clear();
        return false;
      }
      int start = 0;
      while (start < length) {
        if ((start & 1023) == 0 && stopped()) {
          destination.clear();
          return false;
        }
        if (equal(at(start, y), at(start, y + 1))) {
          ++start;
          continue;
        }
        int end = start + 1;
        while (end < length && equal(at(start, y), at(end, y)) &&
               equal(at(start, y + 1), at(end, y + 1))) {
          if ((end & 1023) == 0 && stopped()) {
            destination.clear();
            return false;
          }
          ++end;
        }
        const int run = end - start;
        // Integrate the triangular contour area over each touched pixel.
        const auto filter = [&](bool fromLeft, bool lower) {
          const double localSlope =
              slope / (double(run) * ((start == 0 || end == length) ? 2 : 1));
          const double reach = std::min(double(run), .5 / localSlope);
          const int cells    = int(std::ceil(reach));
          for (int i = 0; i < cells; ++i) {
            if ((i & 1023) == 0 && stopped()) return;
            const double span = std::min(1.0, reach - i);
            const double h0   = std::max(0.0, .5 - localSlope * i);
            const double h1   = std::max(0.0, h0 - localSlope * span);
            const double area = .5 * (h0 + h1) * span;
            const int x       = fromLeft ? start + i : end - 1 - i;
            Pixel &out        = destination[index(x, lower ? y : y + 1)];
            out = interpolate(out, at(x, lower ? y + 1 : y), area);
          }
        };
        const auto validLength = [&](int left, int right, bool lower) {
          if (run > 1) return true;
          if (axis) return false;
          return (lower && y > 0 &&
                  !(equal(at(left, y), at(left, y - 1)) &&
                    equal(at(right, y), at(right, y - 1)))) ||
                 (y + 2 < rows && !(equal(at(left, y + 1), at(left, y + 2)) &&
                                    equal(at(right, y + 1), at(right, y + 2))));
        };
        if (start > 0) {
          bool lower       = equal(at(start, y + 1), at(start - 1, y));
          const bool upper = equal(at(start, y), at(start - 1, y + 1));
          if (lower || upper) {
            if (lower && upper) lower = minority(start, y + 1);
            if (validLength(start - 1, start, lower)) filter(true, lower);
          }
        }
        if (end < length) {
          bool lower       = equal(at(end - 1, y + 1), at(end, y));
          const bool upper = equal(at(end - 1, y), at(end, y + 1));
          if (lower || upper) {
            if (lower && upper) lower = !minority(end, y + 1);
            if (validLength(end - 1, end, lower)) filter(false, lower);
          }
        }
        start = end;
      }
    }
  }
  if (stopped()) {
    destination.clear();
    return false;
  }
  return true;
}
}  // namespace CelSmoothing
