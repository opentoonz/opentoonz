#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace CelMotion {
using Pixel = std::array<double, 4>;  // Premultiplied RGBA, finite HDR allowed.
struct Image {
  int width = 0, height = 0;
  double originX = 0, originY = 0;
  std::vector<Pixel> pixels;
};
struct Component {
  std::size_t count = 0;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  double centerX = 0, centerY = 0;  // Local pixel-center coordinates.
};
struct Shapes {
  std::vector<std::uint32_t>
      labels;  // Zero is empty; group index is label - 1.
  std::vector<Component> groups;
  double minSize = 0, maxSize = 0;  // Square root of covered-pixel count.
};
std::size_t checkedArea(int width, int height);
bool analyze(const Image &image, Shapes &shapes, const int *canceled = nullptr);
double sizeFactor(const Component &component, const Shapes &shapes,
                  double variation);  // Variation is in [0,1].
Pixel sample(const Image &image, double x,
             double y);  // Global bilinear lookup.
double luminance(const Image &image, double x,
                 double y);  // Premultiplied luma.
}  // namespace CelMotion
