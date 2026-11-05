#pragma once

#include <array>
#include <cstddef>
#include <vector>

// Native, coverage-normalized cel filtering. Pixels contain premultiplied RGBA.
// Alpha is a fixed mask: filtering never introduces coverage outside that mask.
namespace CelBlur {
using Pixel = std::array<double, 4>;
struct Settings {
  double radius = 10;
  double blend  = 0.5;
  int repeats   = 1;
  bool vertical = true;   // The named axis is the last pass of each iteration.
  bool legacy   = false;  // Uniform straight-color weights for covered samples.
};

std::size_t checkedArea(int width, int height);
int support(const Settings &settings);
bool filter(std::vector<Pixel> &pixels, int width, int height,
            const Settings &settings, const int *canceled = nullptr);
}  // namespace CelBlur
