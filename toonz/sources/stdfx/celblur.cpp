#include "celblur.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace CelBlur {
std::size_t checkedArea(int width, int height) {
  // Account for both RGBA double buffers, input pixels and prefix work space.
  constexpr std::size_t maxPixels = std::size_t(8) * 1024 * 1024 * 1024 / 160;
  if (width <= 0 || height <= 0) return 0;
  // The host raster allocator multiplies dimensions and bytes in signed int.
  const auto allocatorPixels =
      std::size_t(std::numeric_limits<int>::max()) / 16;
  if (std::size_t(width) >
      std::min(maxPixels, allocatorPixels) / std::size_t(height))
    throw std::runtime_error("Cel Blur: working image exceeds memory limit");
  return std::size_t(width) * std::size_t(height);
}

int support(const Settings &settings) {
  const double margin = std::ceil(settings.radius) * settings.repeats;
  if (!std::isfinite(margin) || margin < 0 ||
      margin > std::numeric_limits<int>::max() / 4)
    throw std::runtime_error("Cel Blur: filter support is too large");
  return int(margin);
}

namespace {
bool pass(const std::vector<Pixel> &input, std::vector<Pixel> &output,
          int width, int height, double radius, double blend, bool vertical,
          bool legacy, const int *canceled) {
  const int length = vertical ? height : width;
  const int lines  = vertical ? width : height;
  const int reach  = int(std::ceil(radius));
  std::vector<Pixel> sums(length + 1), moments(length + 1);
  for (int line = 0; line < lines; ++line) {
    if (canceled && *canceled) return false;
    auto index = [=](int position) -> std::size_t {
      return vertical ? std::size_t(position) * width + line
                      : std::size_t(line) * width + position;
    };
    sums[0] = moments[0] = Pixel{};
    for (int i = 0; i < length; ++i) {
      if ((i & 4095) == 0 && canceled && *canceled) return false;
      Pixel pixel = input[index(i)];
      if (legacy && pixel[3] > 0) {
        for (int channel = 0; channel < 3; ++channel)
          pixel[channel] /= pixel[3];
        pixel[3] = 1;
      }
      for (int channel = 0; channel < 4; ++channel) {
        sums[i + 1][channel]    = sums[i][channel] + pixel[channel];
        moments[i + 1][channel] = moments[i][channel] + i * pixel[channel];
      }
    }
    for (int i = 0; i < length; ++i) {
      if ((i & 4095) == 0 && canceled && *canceled) return false;
      const std::size_t at = index(i);
      const Pixel &pixel   = input[at];
      output[at]           = pixel;
      if (pixel[3] <= 0) continue;
      const int left  = std::max(0, i - reach);
      const int right = std::min(length, i + reach + 1);
      Pixel weighted{};
      for (int channel = 0; channel < 4; ++channel) {
        // The tent is split at its center. Prefix moments make every sample
        // constant cost even at the maximum amount and repeat count.
        const double low  = sums[i + 1][channel] - sums[left][channel];
        const double high = sums[right][channel] - sums[i + 1][channel];
        weighted[channel] =
            legacy ? sums[right][channel] - sums[left][channel]
                   : (radius + 1 - i) * low + moments[i + 1][channel] -
                         moments[left][channel] + (radius + 1 + i) * high -
                         moments[right][channel] + moments[i + 1][channel];
      }
      if (weighted[3] > 0) {
        for (int channel = 0; channel < 3; ++channel)
          output[at][channel] =
              pixel[channel] * (1 - blend) +
              weighted[channel] * (pixel[3] / weighted[3]) * blend;
      }
    }
  }
  return true;
}
}  // namespace

bool filter(std::vector<Pixel> &pixels, int width, int height,
            const Settings &settings, const int *canceled) {
  if (canceled && *canceled) return false;
  const std::size_t area = checkedArea(width, height);
  if (pixels.size() != area)
    throw std::runtime_error("Cel Blur: invalid working image");
  support(settings);
  if (area == 0 || settings.repeats == 0 || settings.radius <= 0) return true;
  std::vector<Pixel> scratch(area);
  for (int repeat = 0; repeat < settings.repeats; ++repeat) {
    if (!pass(pixels, scratch, width, height, settings.radius, settings.blend,
              !settings.vertical, settings.legacy, canceled) ||
        !pass(scratch, pixels, width, height, settings.radius, settings.blend,
              settings.vertical, settings.legacy, canceled))
      return false;
  }
  return !(canceled && *canceled);
}
}  // namespace CelBlur
