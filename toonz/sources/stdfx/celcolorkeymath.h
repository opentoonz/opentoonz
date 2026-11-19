#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace CelKey {

using RGB = std::array<double, 3>;
enum class ColorSpace { RGB, HSV, Lab76, Lab94, YUV, YCrCb };
enum class Precision { Full, Bit16, Bit8 };

struct Entry {
  bool enabled     = false;
  bool replace     = false;
  RGB color        = {{0.0, 0.0, 0.0}};
  RGB replacement  = {{0.0, 0.0, 0.0}};
  double threshold = 0.0;
  RGB components   = {{0.0, 0.0, 0.0}};
};

struct Settings {
  ColorSpace space    = ColorSpace::RGB;
  Precision precision = Precision::Full;
  bool premultiplied  = false;
  bool perColor       = false;
  bool perComponent   = false;
  double threshold    = 0.0;
  RGB components      = {{0.0, 0.0, 0.0}};
  int sourceBits      = 32;
};

inline bool finite(const RGB &color) {
  return std::isfinite(color[0]) && std::isfinite(color[1]) &&
         std::isfinite(color[2]);
}

inline bool validPixel(const RGB &color, double alpha) {
  return finite(color) && std::isfinite(alpha) && alpha > 0.0;
}

namespace detail {

inline double unit(double value) { return std::max(0.0, std::min(1.0, value)); }

inline bool validThreshold(double value) {
  return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

inline bool validThresholds(const RGB &value) {
  return validThreshold(value[0]) && validThreshold(value[1]) &&
         validThreshold(value[2]);
}

inline double linear(double value) {
  return value <= 0.04045 ? value / 12.92
                          : std::pow((value + 0.055) / 1.055, 2.4);
}

inline double labCurve(double value) {
  return value > 216.0 / 24389.0 ? std::cbrt(value)
                                 : ((24389.0 / 27.0) * value + 16.0) / 116.0;
}

inline bool validSpace(ColorSpace space) {
  return space == ColorSpace::RGB || space == ColorSpace::HSV ||
         space == ColorSpace::Lab76 || space == ColorSpace::Lab94 ||
         space == ColorSpace::YUV || space == ColorSpace::YCrCb;
}

}  // namespace detail

// Comparison quantization only. Integer comparison clamps to the unit range;
// Full precision on a float source preserves finite HDR values. The selected
// comparison precision never increases the source's native bit depth.
inline RGB quantize(RGB color, int bits) {
  if (bits != 8 && bits != 16 && bits != 32)
    throw std::invalid_argument("Invalid comparison precision");
  if (bits == 32 || !finite(color)) return color;
  const double maximum = bits == 8 ? 255.0 : 65535.0;
  for (double &component : color)
    component = std::floor(detail::unit(component) * maximum + 0.5) / maximum;
  return color;
}

// HSV uses turns for hue. Lab uses the sRGB transfer function, the D65 white
// point and the CIE 2-degree observer. YUV and full-range YCrCb use BT.601;
// chroma offsets are unnecessary for differences and are omitted.
inline RGB convert(const RGB &color, ColorSpace space) {
  switch (space) {
  case ColorSpace::RGB:
    return color;
  case ColorSpace::HSV: {
    const double maximum = std::max(color[0], std::max(color[1], color[2]));
    const double minimum = std::min(color[0], std::min(color[1], color[2]));
    const double delta   = maximum - minimum;
    double hue           = 0.0;
    if (delta > 0.0) {
      if (maximum == color[0])
        hue = (color[1] - color[2]) / delta;
      else if (maximum == color[1])
        hue = (color[2] - color[0]) / delta + 2.0;
      else
        hue = (color[0] - color[1]) / delta + 4.0;
      hue /= 6.0;
      hue -= std::floor(hue);
    }
    return {{hue, maximum > 0.0 ? delta / maximum : 0.0, maximum}};
  }
  case ColorSpace::Lab76:
  case ColorSpace::Lab94: {
    const double red   = detail::linear(color[0]);
    const double green = detail::linear(color[1]);
    const double blue  = detail::linear(color[2]);
    const double x     = detail::labCurve(
            (0.4124564 * red + 0.3575761 * green + 0.1804375 * blue) / 0.95047);
    const double y = detail::labCurve(0.2126729 * red + 0.7151522 * green +
                                      0.0721750 * blue);
    const double z = detail::labCurve(
        (0.0193339 * red + 0.1191920 * green + 0.9503041 * blue) / 1.08883);
    return {{116.0 * y - 16.0, 500.0 * (x - y), 200.0 * (y - z)}};
  }
  case ColorSpace::YUV:
    return {{0.299 * color[0] + 0.587 * color[1] + 0.114 * color[2],
             -0.14713 * color[0] - 0.28886 * color[1] + 0.436 * color[2],
             0.615 * color[0] - 0.51499 * color[1] - 0.10001 * color[2]}};
  case ColorSpace::YCrCb: {
    const double y = 0.299 * color[0] + 0.587 * color[1] + 0.114 * color[2];
    return {{y, (color[0] - y) / 1.402, (color[2] - y) / 1.772}};
  }
  }
  throw std::invalid_argument("Invalid color comparison space");
}

// Normalized absolute component differences between already-converted colors.
// RGB and HSV S/V use unit ranges; HSV hue is circular and a half-turn is 1.
// Hue has no effect when either color is achromatic. Lab76 uses L/100, a/255,
// b/255. Lab94 uses L/100, C/255, H/255, with the selected key as reference and
// graphics-arts weights (kL=kC=kH=1, K1=.045, K2=.015). YUV uses Y/1,
// U/.872, V/1.23; YCrCb uses unit Y/Cr/Cb ranges. Scalar comparisons use the
// root-mean-square of these three normalized components; component comparisons
// test each component separately. All public thresholds range from zero to one.
inline RGB differences(const RGB &sample, const RGB &key, ColorSpace space) {
  RGB delta = {{std::abs(sample[0] - key[0]), std::abs(sample[1] - key[1]),
                std::abs(sample[2] - key[2])}};
  switch (space) {
  case ColorSpace::RGB:
  case ColorSpace::YCrCb:
    return delta;
  case ColorSpace::HSV:
    delta[0] = sample[1] <= 0.0 || key[1] <= 0.0
                   ? 0.0
                   : 2.0 * std::min(delta[0], 1.0 - delta[0]);
    return delta;
  case ColorSpace::Lab76:
    return {{delta[0] / 100.0, delta[1] / 255.0, delta[2] / 255.0}};
  case ColorSpace::Lab94: {
    const double sampleChroma = std::hypot(sample[1], sample[2]);
    const double keyChroma    = std::hypot(key[1], key[2]);
    const double chromaDelta  = std::abs(sampleChroma - keyChroma);
    // Normalize before squaring to avoid overflow for large finite inputs.
    const double scale = std::max(delta[1], std::max(delta[2], chromaDelta));
    double hueDelta    = 0.0;
    if (scale > 0.0) {
      const double a = delta[1] / scale, b = delta[2] / scale;
      const double c = chromaDelta / scale;
      hueDelta       = scale * std::sqrt(std::max(0.0, a * a + b * b - c * c));
    }
    return {{delta[0] / 100.0, chromaDelta / (1.0 + 0.045 * keyChroma) / 255.0,
             hueDelta / (1.0 + 0.015 * keyChroma) / 255.0}};
  }
  case ColorSpace::YUV:
    return {{delta[0], delta[1] / 0.872, delta[2] / 1.23}};
  }
  throw std::invalid_argument("Invalid color comparison space");
}

// Immutable render snapshot. Key conversion occurs once rather than once per
// source pixel; firstMatch is allocation-free and safe to call concurrently.
class Matcher {
  struct Prepared {
    bool enabled;
    RGB color;
    double threshold;
    RGB components;
  };
  Settings m_settings;
  int m_bits;
  std::vector<Prepared> m_entries;

public:
  Matcher(const Entry *entries, std::size_t count, const Settings &settings)
      : m_settings(settings), m_bits(settings.sourceBits) {
    if ((!entries && count) ||
        count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !detail::validSpace(settings.space) ||
        (m_bits != 8 && m_bits != 16 && m_bits != 32) ||
        (settings.precision != Precision::Full &&
         settings.precision != Precision::Bit16 &&
         settings.precision != Precision::Bit8) ||
        !detail::validThreshold(settings.threshold) ||
        !detail::validThresholds(settings.components))
      throw std::invalid_argument("Invalid color-key settings");
    if (settings.precision == Precision::Bit16) m_bits = std::min(m_bits, 16);
    if (settings.precision == Precision::Bit8) m_bits = 8;
    m_entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      Prepared prepared;
      prepared.enabled = entries[i].enabled && finite(entries[i].color);
      prepared.color =
          prepared.enabled
              ? convert(quantize(entries[i].color, m_bits), settings.space)
              : RGB{{0.0, 0.0, 0.0}};
      prepared.enabled = prepared.enabled && finite(prepared.color);
      prepared.threshold =
          settings.perColor ? entries[i].threshold : settings.threshold;
      prepared.components =
          settings.perColor ? entries[i].components : settings.components;
      if (!detail::validThreshold(prepared.threshold) ||
          !detail::validThresholds(prepared.components))
        throw std::invalid_argument("Invalid color-key entry threshold");
      m_entries.push_back(prepared);
    }
  }

  // Input RGB is native premultiplied RGB. With premultiplied=false it is
  // unpremultiplied before comparison. With true it is compared directly with
  // the selected, unscaled key RGB. Transparent or nonfinite pixels never
  // match. Replacement and output precision are intentionally handled by the
  // renderer; matching never mutates its source or the selected colors.
  int firstMatch(RGB color, double alpha) const {
    if (!validPixel(color, alpha)) return -1;
    if (!m_settings.premultiplied)
      for (double &component : color) component /= alpha;
    if (!finite(color)) return -1;
    color = convert(quantize(color, m_bits), m_settings.space);
    if (!finite(color)) return -1;
    // Float rasters and sampled parameters reach this snapshot through
    // different native representations. Permit only their rounding noise.
    const double epsilon = m_bits == 32 ? 1e-6 : 1e-12;
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
      const Prepared &entry = m_entries[i];
      if (!entry.enabled) continue;
      const RGB delta = differences(color, entry.color, m_settings.space);
      if (!finite(delta)) continue;
      if (m_settings.perComponent) {
        if (delta[0] <= entry.components[0] + epsilon &&
            delta[1] <= entry.components[1] + epsilon &&
            delta[2] <= entry.components[2] + epsilon)
          return static_cast<int>(i);
      } else {
        const double distance =
            std::hypot(std::hypot(delta[0], delta[1]), delta[2]) /
            std::sqrt(3.0);
        if (std::isfinite(distance) && distance <= entry.threshold + epsilon)
          return static_cast<int>(i);
      }
    }
    return -1;
  }
};

inline int firstMatch(RGB color, double alpha, const Entry *entries,
                      std::size_t count, const Settings &settings) {
  return Matcher(entries, count, settings).firstMatch(color, alpha);
}

}  // namespace CelKey
