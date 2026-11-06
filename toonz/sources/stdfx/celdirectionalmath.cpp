#include "celdirectionalmath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace CelDirectional {
namespace {
double clamp(double value) { return std::max(0.0, std::min(1.0, value)); }
double smooth(double value) {
  value = clamp(value);
  return value * value * (3 - 2 * value);
}
struct Extent {
  double alongMin  = std::numeric_limits<double>::infinity();
  double alongMax  = -std::numeric_limits<double>::infinity();
  double acrossMin = std::numeric_limits<double>::infinity();
  double acrossMax = -std::numeric_limits<double>::infinity();
};
bool clipAxis(double origin, double velocity, double low, double high,
              double &first, double &last) {
  if (std::abs(velocity) < 1e-12) return origin >= low && origin <= high;
  double a = (low - origin) / velocity, b = (high - origin) / velocity;
  if (a > b) std::swap(a, b);
  first = std::max(first, a);
  last  = std::min(last, b);
  return first <= last;
}
void splat(CelMotion::Image &out, double x, double y,
           const CelMotion::Pixel &pixel, double weight) {
  x -= out.originX;
  y -= out.originY;
  if (x < -1 || y < -1 || x >= out.width || y >= out.height) return;
  const int ix = int(std::floor(x)), iy = int(std::floor(y));
  const double tx = x - ix, ty = y - iy;
  for (int dy = 0; dy < 2; ++dy)
    for (int dx = 0; dx < 2; ++dx) {
      const int sx = ix + dx, sy = iy + dy;
      if (sx < 0 || sy < 0 || sx >= out.width || sy >= out.height) continue;
      const double coverage = weight * (dx ? tx : 1 - tx) * (dy ? ty : 1 - ty);
      auto &destination     = out.pixels[std::size_t(sy) * out.width + sx];
      for (int c = 0; c < 4; ++c) destination[c] += pixel[c] * coverage;
    }
}
}  // namespace

bool render(const CelMotion::Image &source, const CelMotion::Shapes &shapes,
            const CelMotion::Image *noise, CelMotion::Image &output,
            const Settings &settings, const int *canceled) {
  const auto area = CelMotion::checkedArea(source.width, source.height);
  if (source.pixels.size() != area || shapes.labels.size() != area)
    throw std::runtime_error("Cel Directional Blur: incomplete shape analysis");
  output.pixels.assign(CelMotion::checkedArea(output.width, output.height),
                       CelMotion::Pixel{});
  const double ux = std::cos(settings.angle), uy = std::sin(settings.angle);
  std::vector<Extent> extents(shapes.groups.size());
  for (int y = 0; y < source.height; ++y) {
    if (canceled && *canceled) return false;
    for (int x = 0; x < source.width; ++x) {
      if ((x & 4095) == 0 && canceled && *canceled) return false;
      const auto label = shapes.labels[std::size_t(y) * source.width + x];
      if (!label) continue;
      auto &extent       = extents[label - 1];
      const double along = x * ux + y * uy, across = -x * uy + y * ux;
      extent.alongMin  = std::min(extent.alongMin, along);
      extent.alongMax  = std::max(extent.alongMax, along);
      extent.acrossMin = std::min(extent.acrossMin, across);
      extent.acrossMax = std::max(extent.acrossMax, across);
    }
  }
  for (int y = 0; y < source.height; ++y) {
    if (canceled && *canceled) return false;
    for (int x = 0; x < source.width; ++x) {
      if ((x & 4095) == 0 && canceled && *canceled) return false;
      const auto at    = std::size_t(y) * source.width + x;
      const auto label = shapes.labels[at];
      if (!label) continue;
      const auto &pixel       = source.pixels[at];
      const auto &extent      = extents[label - 1];
      const double shapeScale = CelMotion::sizeFactor(
          shapes.groups[label - 1], shapes, settings.sizeVariation);
      const double px = x + source.originX, py = y + source.originY;
      const double noiseValue =
          settings.noiseVariation == 0
              ? 1
              : CelNoise::sample(
                    settings.noise, px / settings.scale, py / settings.scale,
                    noise ? CelMotion::luminance(*noise, px, py) : 0);
      const double noiseScale = 1 - settings.noiseVariation * (1 - noiseValue);
      const double along = x * ux + y * uy, across = -x * uy + y * ux;
      const double halfWidth = (extent.acrossMax - extent.acrossMin) * 0.5;
      const double center    = (extent.acrossMax + extent.acrossMin) * 0.5;
      const double rim =
          halfWidth > 0 ? clamp(std::abs(across - center) / halfWidth) : 0;
      const double fadeWidth =
          std::max(1.0, (extent.alongMax - extent.alongMin + 1) *
                            (0.1 + 0.9 * settings.sizeVariation));
      double centerWeight = 0;
      bool activeSide     = false;
      for (int side = 0; side < 2; ++side) {
        const Side &parameters = side == 0 ? settings.front : settings.back;
        const double direction = side == 0 ? 1 : -1;
        const double length    = parameters.strength * settings.scale *
                              shapeScale * noiseScale *
                              (1 - parameters.sharpTail * rim);
        if (length <= 0) continue;
        activeSide = true;
        const double distanceFromBack =
            side == 0 ? along - extent.alongMin : extent.alongMax - along;
        const double emission =
            1 - parameters.edgeFade *
                    (1 - smooth((distanceFromBack + 0.5) / fadeWidth));
        centerWeight = std::max(centerWeight, emission);
        double first = 1, last = std::ceil(length);
        if (!std::isfinite(last) || last > std::numeric_limits<int>::max() / 4)
          throw std::runtime_error(
              "Cel Directional Blur: motion support is too large");
        // Clip the trajectory to this output tile before stepping. A long
        // streak therefore never scans its off-tile portion.
        if (!clipAxis(px, ux * direction, output.originX - 1,
                      output.originX + output.width, first, last) ||
            !clipAxis(py, uy * direction, output.originY - 1,
                      output.originY + output.height, first, last))
          continue;
        const int start = std::max(1, int(std::ceil(first)));
        const int end   = int(std::floor(last));
        for (int step = start; step <= end; ++step) {
          if ((step & 255) == 0 && canceled && *canceled) return false;
          const double weight =
              emission * std::max(0.0, 1 - step / (length + 1));
          splat(output, px + step * ux * direction, py + step * uy * direction,
                pixel, weight);
        }
      }
      splat(output, px, py, pixel, activeSide ? centerWeight : 1);
    }
  }
  for (std::size_t index = 0; index < output.pixels.size(); ++index) {
    if ((index & 4095) == 0 && canceled && *canceled) return false;
    auto &pixel = output.pixels[index];
    if (pixel[3] <= 0) {
      pixel = CelMotion::Pixel{};
      continue;
    }
    const double alpha  = clamp(pixel[3]);
    const double factor = settings.gain * alpha / pixel[3];
    for (int c = 0; c < 3; ++c) pixel[c] *= factor;
    pixel[3] = alpha;
  }
  return !(canceled && *canceled);
}
}  // namespace CelDirectional
