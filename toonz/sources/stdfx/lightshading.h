#pragma once

#ifndef LIGHTSHADING_H
#define LIGHTSHADING_H

// Pure helpers of the Light Shading Fx (lightshadingfx.cpp). Header only and
// free of any toonz type.
//
// The light intensity I is in [0, 1]: 0.5 is neutral, below it the pixel is
// in shadow, above it in light. Quantised to N bands, the signed shading level
// is in [-1, 1] with N steps on each side of a neutral middle band.

#include <algorithm>
#include <cmath>
#include <vector>

namespace lightshading {

// Clamped to [0, 1]; NaN becomes 0
inline double clamp01(double v) {
  if (!(v > 0.0)) return 0.0;
  return v > 1.0 ? 1.0 : v;
}

// Radial light: 1 at the light point, 0 at the radius and beyond. A radius of
// 0 or less switches the light off (0, the full shadow side).
inline double radialIntensity(double distance, double radius) {
  if (!(radius > 0.0)) return 0.0;
  return clamp01(1.0 - distance / radius);
}

// Directional light on a 2D 'normal' (the outward pointing gradient of the
// shape): N.L mapped from [-1, 1] to [0, 1]. A normal longer than 1 is
// shortened, one of length 0 (the inside of a shape) is neutral, 0.5. L need
// not be normalised; no direction gives a neutral light.
inline double directionalIntensity(double nx, double ny, double lx, double ly) {
  double nLength = std::sqrt(nx * nx + ny * ny);
  double lLength = std::sqrt(lx * lx + ly * ly);
  if (!(lLength > 0.0) || !(nLength > 0.0)) return 0.5;
  // cos of the angle between N and L, scaled by the (limited) length of N
  double dot = (nx * lx + ny * ly) / (nLength * lLength);
  return clamp01(0.5 + 0.5 * dot * std::min(nLength, 1.0));
}

// Quantises the intensity to a signed level in [-1, 1]: negative is shadow,
// positive is light, 0 the neutral middle band. With N bands each side has N
// flat steps (k / N) and the neutral band takes the same width as a step, so
// 1 band gives one shadow shape (I < 0.25) and one light shape (I > 0.75).
inline double quantizeLevel(double intensity, int bands) {
  bands        = std::min(std::max(bands, 1), 64);
  double s     = (clamp01(intensity) - 0.5) * 2.0;
  double steps = std::floor(std::fabs(s) * (bands + 1));
  steps        = std::min(steps, (double)bands);
  double level = steps / bands;
  return s < 0.0 ? -level : level;
}

// Blends the colour channel of a straight (not premultiplied) pixel towards
// the shaded or lit version: multiply for shadow (level < 0), screen for light
// (level > 0). Returns the interpolation amount (0 = unchanged); the caller
// does the blend with the multiplied or screened value.
inline double shadingAmount(double level, double shadeAmount,
                            double highlightAmount) {
  return clamp01(std::fabs(level) *
                 (level < 0.0 ? shadeAmount : highlightAmount));
}

// A box blur of radius r (window 2r + 1) over a w x h float buffer, in place.
// Outside the buffer counts as 0. tmp is scratch space.
inline void boxBlur(std::vector<float> &buf, int w, int h, int r,
                    std::vector<float> &tmp) {
  if (r <= 0 || w <= 0 || h <= 0) return;
  tmp.resize(buf.size());
  const float inv = 1.0f / (2 * r + 1);

  // Rows
  for (int y = 0; y < h; ++y) {
    const float *src = &buf[(size_t)y * w];
    float *dst       = &tmp[(size_t)y * w];
    double sum       = 0.0;
    for (int x = 0; x <= std::min(r, w - 1); ++x) sum += src[x];
    for (int x = 0; x < w; ++x) {
      dst[x] = (float)(sum * inv);
      if (x + r + 1 < w) sum += src[x + r + 1];
      if (x - r >= 0) sum -= src[x - r];
    }
  }
  // Columns
  for (int x = 0; x < w; ++x) {
    double sum = 0.0;
    for (int y = 0; y <= std::min(r, h - 1); ++y) sum += tmp[(size_t)y * w + x];
    for (int y = 0; y < h; ++y) {
      buf[(size_t)y * w + x] = (float)(sum * inv);
      if (y + r + 1 < h) sum += tmp[(size_t)(y + r + 1) * w + x];
      if (y - r >= 0) sum -= tmp[(size_t)(y - r) * w + x];
    }
  }
}

}  // namespace lightshading

#endif
