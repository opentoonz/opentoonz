#pragma once

#include "celsmoothing.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace CelSmoother2 {
using Pixel                 = CelSmoothing::Pixel;
constexpr int gammaCapacity = 5;

struct Settings {
  bool useKey    = false;
  bool invertKey = false;
  Pixel key{1, 1, 1, 1};
  double range    = 2;
  double strength = 100;
  double extra    = 0;
  int version     = 1;  // 0: encoded RGB, 1: sRGB linear light.
  int gammaMode   = 0;  // None, Gamma Colors, All Colors.
  double gamma    = 2.4;
  int gammaCount  = 1;
  std::array<Pixel, gammaCapacity> colors{};
};

inline double bounded(double value, double low, double high, double fallback) {
  return std::isfinite(value) ? std::max(low, std::min(high, value)) : fallback;
}

inline int choice(double value, int maximum) {
  return int(std::floor(bounded(value, 0, maximum, 0) + 0.5));
}

inline bool finite(const Pixel &p) {
  return std::isfinite(p.r) && std::isfinite(p.g) && std::isfinite(p.b) &&
         std::isfinite(p.a);
}

// Color selection is performed in straight, encoded RGB. Half an 8-bit step
// selects one sampled cel color without broadening with partial coverage.
inline bool matches(const Pixel &p, const Pixel &color) {
  constexpr double tolerance = 0.5 / 255.0;
  if (!(p.a > 0) || !finite(p) || !finite(color)) return false;
  return std::abs(p.r / p.a - color.r) < tolerance &&
         std::abs(p.g / p.a - color.g) < tolerance &&
         std::abs(p.b / p.a - color.b) < tolerance;
}

inline Pixel keyPixel(const Pixel &p, const Settings &settings) {
  if (!finite(p) || !(p.a > 0)) return Pixel{0, 0, 0, 0};
  if (settings.useKey && matches(p, settings.key) != settings.invertKey)
    return Pixel{0, 0, 0, 0};
  return p;
}

// Extend the sRGB transfer with odd symmetry to keep finite negative/HDR data.
// This transfer belongs to the effect and is separate from the host scene
// gamma.
inline double linearize(double value) {
  const double magnitude = std::abs(value);
  return std::copysign(magnitude <= 0.04045
                           ? magnitude / 12.92
                           : std::pow((magnitude + 0.055) / 1.055, 2.4),
                       value);
}
inline double encode(double value) {
  const double magnitude = std::abs(value);
  return std::copysign(magnitude <= 0.0031308
                           ? 12.92 * magnitude
                           : 1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055,
                       value);
}
inline double power(double value, double exponent) {
  return std::copysign(std::pow(std::abs(value), exponent), value);
}

inline bool selected(const Pixel &p, const Settings &settings) {
  for (int i = 0; i < settings.gammaCount; ++i)
    if (matches(p, settings.colors[i])) return true;
  return false;
}

// Apply the extra gamma to a blend when either contributing color is selected.
// Alpha is linear coverage; RGB is transformed before alpha-weighted mixing.
// Unselected edges still use the version's normal color space.
inline Pixel mix(const Pixel &a, const Pixel &b, double weight,
                 const Settings &settings) {
  if (weight <= 0) return a;
  if (weight >= 1) return b;
  const double alpha = a.a * (1 - weight) + b.a * weight;
  if (!(alpha > 0)) return Pixel{0, 0, 0, 0};
  const bool useGamma = settings.gammaMode == 2 ||
                        (settings.gammaMode == 1 &&
                         (selected(a, settings) || selected(b, settings)));
  const auto forward = [&](double channel, double coverage) {
    if (!(coverage > 0)) return 0.0;
    double value = channel / coverage;
    if (settings.version == 1) value = linearize(value);
    if (useGamma) value = power(value, 1.0 / settings.gamma);
    return value * coverage;
  };
  const auto blend = [&](double ac, double bc) {
    double value =
        (forward(ac, a.a) * (1 - weight) + forward(bc, b.a) * weight) / alpha;
    if (useGamma) value = power(value, settings.gamma);
    if (settings.version == 1) value = encode(value);
    return value * alpha;
  };
  return Pixel{blend(a.r, b.r), blend(a.g, b.g), blend(a.b, b.b), alpha};
}

inline bool process(int width, int height, const std::vector<Pixel> &input,
                    std::vector<Pixel> &output, const Settings &settings,
                    const int *canceled) {
  std::vector<Pixel> keyed(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if ((i & 4095) == 0 && canceled && *canceled) return false;
    keyed[i] = keyPixel(input[i], settings);
  }
  const auto blend = [&](const Pixel &a, const Pixel &b, double weight) {
    return mix(a, b, weight, settings);
  };
  if (!CelSmoothing::smooth(width, height, keyed, output, settings.range,
                            settings.strength, canceled, blend))
    return false;
  if (settings.extra > 0) {
    std::vector<Pixel> second;
    if (!CelSmoothing::smooth(width, height, output, second, settings.range,
                              settings.extra, canceled, blend))
      return false;
    output.swap(second);
  }
  return !(canceled && *canceled);
}
}  // namespace CelSmoother2
