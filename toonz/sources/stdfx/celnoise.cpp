#include "celnoise.h"
#include <algorithm>
#include <cmath>

namespace CelNoise {
namespace {
std::uint64_t mix(std::uint64_t value) {
  value ^= value >> 30;
  value *= UINT64_C(0xbf58476d1ce4e5b9);
  value ^= value >> 27;
  value *= UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}
double lattice(std::int64_t x, std::int64_t y, std::int64_t z,
               std::uint32_t seed) {
  const auto value = mix(std::uint64_t(x) ^ mix(std::uint64_t(y)) ^
                         mix(std::uint64_t(z) ^ seed));
  return double(value >> 11) * (1.0 / 9007199254740991.0);
}
double fade(double t) { return t * t * (3 - 2 * t); }
double lerp(double a, double b, double t) { return a + (b - a) * t; }

double latticeCoordinate(double value, double thickness) {
  const double period = thickness * 1073741824.0;
  if (std::isfinite(period)) return std::fmod(value, period) / thickness;
  return value / thickness;
}
}  // namespace

double sample(const Settings &settings, double x, double y,
              double layerLuminance) {
  if (settings.type == Layer)
    return std::isfinite(layerLuminance)
               ? std::max(0.0, std::min(1.0, layerLuminance))
               : 0;
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(settings.offset) ||
      !std::isfinite(settings.thickness) || settings.thickness <= 0)
    return 0;
  // Callers may scale thickness outside the UI range for render resolution.
  // Reduce before division so extreme coordinates cannot overflow the lattice.
  x              = latticeCoordinate(x, settings.thickness);
  y              = latticeCoordinate(y, settings.thickness);
  const double z = std::fmod(settings.offset, 1073741824.0);
  const auto ix = std::int64_t(std::floor(x)), iy = std::int64_t(std::floor(y));
  const auto iz = std::int64_t(std::floor(z));
  auto plane    = [&](std::int64_t layer) {
    if (settings.type == Block) return lattice(ix, iy, layer, settings.seed);
    const double tx = fade(x - ix), ty = fade(y - iy);
    return lerp(lerp(lattice(ix, iy, layer, settings.seed),
                        lattice(ix + 1, iy, layer, settings.seed), tx),
                   lerp(lattice(ix, iy + 1, layer, settings.seed),
                        lattice(ix + 1, iy + 1, layer, settings.seed), tx),
                   ty);
  };
  return lerp(plane(iz), plane(iz + 1), fade(z - iz));
}
}  // namespace CelNoise
