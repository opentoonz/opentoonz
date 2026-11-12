#include "celmotion.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace CelMotion {
std::size_t checkedArea(int width, int height) {
  constexpr std::size_t maxPixels = std::size_t(8) * 1024 * 1024 * 1024 / 160;
  if (width <= 0 || height <= 0) return 0;
  const auto allocatorPixels =
      std::size_t(std::numeric_limits<int>::max()) / 16;
  if (std::size_t(width) >
      std::min(maxPixels, allocatorPixels) / std::size_t(height))
    throw std::runtime_error(
        "Cel motion: canonical image exceeds memory limit");
  return std::size_t(width) * height;
}

bool analyze(const Image &image, Shapes &shapes, const int *canceled) {
  shapes           = Shapes();
  const auto count = checkedArea(image.width, image.height);
  if (image.pixels.size() != count)
    throw std::runtime_error("Cel motion: invalid canonical image");
  shapes.labels.assign(count, 0);
  std::vector<std::size_t> pending;
  for (std::size_t start = 0; start < count; ++start) {
    if (canceled && *canceled) {
      shapes = Shapes();
      return false;
    }
    if (shapes.labels[start] || image.pixels[start][3] <= 0 ||
        !std::isfinite(image.pixels[start][3]))
      continue;
    if (shapes.groups.size() >= std::numeric_limits<std::uint32_t>::max() - 1)
      throw std::runtime_error("Cel motion: too many connected shapes");
    const auto label = std::uint32_t(shapes.groups.size() + 1);
    Component group;
    group.x0 = group.x1 = int(start % image.width);
    group.y0 = group.y1 = int(start / image.width);
    pending.clear();
    pending.push_back(start);
    shapes.labels[start] = label;
    for (std::size_t next = 0; next < pending.size(); ++next) {
      if ((next & 4095) == 0 && canceled && *canceled) {
        shapes = Shapes();
        return false;
      }
      const std::size_t at = pending[next];
      const int x = int(at % image.width), y = int(at / image.width);
      ++group.count;
      group.centerX += x;
      group.centerY += y;
      group.x0 = std::min(group.x0, x);
      group.x1 = std::max(group.x1, x);
      group.y0 = std::min(group.y0, y);
      group.y1 = std::max(group.y1, y);
      for (int sy = std::max(0, y - 1); sy <= std::min(image.height - 1, y + 1);
           ++sy)
        for (int sx = std::max(0, x - 1);
             sx <= std::min(image.width - 1, x + 1); ++sx) {
          const auto neighbor = std::size_t(sy) * image.width + sx;
          if (!shapes.labels[neighbor] && image.pixels[neighbor][3] > 0 &&
              std::isfinite(image.pixels[neighbor][3])) {
            shapes.labels[neighbor] = label;
            pending.push_back(neighbor);
          }
        }
    }
    group.centerX /= group.count;
    group.centerY /= group.count;
    const double size = std::sqrt(double(group.count));
    if (shapes.groups.empty())
      shapes.minSize = shapes.maxSize = size;
    else {
      shapes.minSize = std::min(shapes.minSize, size);
      shapes.maxSize = std::max(shapes.maxSize, size);
    }
    shapes.groups.push_back(group);
  }
  return true;
}

double sizeFactor(const Component &component, const Shapes &shapes,
                  double variation) {
  const double span = shapes.maxSize - shapes.minSize;
  const double relative =
      span > 0 ? (std::sqrt(double(component.count)) - shapes.minSize) / span
               : 0;
  return 1 - std::max(0.0, std::min(1.0, variation)) * (1 - relative);
}

Pixel sample(const Image &image, double x, double y) {
  x -= image.originX;
  y -= image.originY;
  if (!std::isfinite(x) || !std::isfinite(y) || x < -1 || y < -1 ||
      x >= image.width || y >= image.height)
    return Pixel{};
  const int ix = int(std::floor(x)), iy = int(std::floor(y));
  const double tx = x - ix, ty = y - iy;
  Pixel result{};
  for (int dy = 0; dy < 2; ++dy)
    for (int dx = 0; dx < 2; ++dx) {
      const int sx = ix + dx, sy = iy + dy;
      if (sx < 0 || sy < 0 || sx >= image.width || sy >= image.height) continue;
      const double weight = (dx ? tx : 1 - tx) * (dy ? ty : 1 - ty);
      const auto &pixel   = image.pixels[std::size_t(sy) * image.width + sx];
      if (pixel[3] > 0)
        for (int c = 0; c < 4; ++c) result[c] += pixel[c] * weight;
    }
  return result;
}
double luminance(const Image &image, double x, double y) {
  const auto pixel = sample(image, x, y);
  if (pixel[3] <= 0) return 0;
  return std::max(0.0, std::min(1.0, 0.2126 * pixel[0] + 0.7152 * pixel[1] +
                                         0.0722 * pixel[2]));
}
}  // namespace CelMotion
