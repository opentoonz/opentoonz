#pragma once
#include <cstdint>

namespace CelNoise {
enum Type { Smooth = 0, Block = 1, Layer = 2 };
struct Settings {
  Type type          = Smooth;
  std::uint32_t seed = 0;
  double offset      = 0;
  double thickness   = 10;
};
// Coordinates are in the effect's unscaled global domain. Layer values are
// premultiplied luminance: transparent pixels are black. No frame/order state.
double sample(const Settings &settings, double x, double y,
              double layerLuminance = 0);
}  // namespace CelNoise
