// Raster and blur helpers shared by Drop Shadow and Bevel FX.
// Box-blur sampling and pass rounding match Flash / Adobe Animate filters.
#pragma once

#include "stdfx.h"
#include "tpixelutils.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace shadowbevel {

// A premultiplied RGBA float copy of a raster region.
struct FloatImage {
  int lx = 0, ly = 0;
  std::vector<float> pixels;  // r, g, b, m per pixel
  float *at(int x, int y) { return &pixels[4 * (size_t(y) * lx + x)]; }
};

template <class PIXEL>
void readRaster(const TRasterPT<PIXEL> &ras, FloatImage &image) {
  image.lx = ras->getLx(), image.ly = ras->getLy();
  image.pixels.assign(4 * size_t(image.lx) * image.ly, 0.f);
  const float max = float(PIXEL::maxChannelValue);
  for (int y = 0; y < image.ly; ++y) {
    const PIXEL *pix = ras->pixels(y);
    for (int x = 0; x < image.lx; ++x, ++pix) {
      float *out = image.at(x, y);
      out[0] = pix->r / max, out[1] = pix->g / max, out[2] = pix->b / max;
      out[3] = pix->m / max;
    }
  }
}

template <>
inline void readRaster<TPixelF>(const TRasterFP &ras, FloatImage &image) {
  image.lx = ras->getLx(), image.ly = ras->getLy();
  image.pixels.assign(4 * size_t(image.lx) * image.ly, 0.f);
  for (int y = 0; y < image.ly; ++y) {
    const TPixelF *pix = ras->pixels(y);
    for (int x = 0; x < image.lx; ++x, ++pix) {
      float *out = image.at(x, y);
      out[0] = pix->r, out[1] = pix->g, out[2] = pix->b, out[3] = pix->m;
    }
  }
}

template <class PIXEL>
void writePixel(PIXEL &pix, const float *value) {
  const float max = float(PIXEL::maxChannelValue);
  auto channel    = [max](float v) {
    return (typename PIXEL::Channel)std::lround(
           std::max(0.f, std::min(1.f, v)) * max);
  };
  pix.r = channel(value[0]), pix.g = channel(value[1]);
  pix.b = channel(value[2]), pix.m = channel(value[3]);
}

template <>
inline void writePixel<TPixelF>(TPixelF &pix, const float *value) {
  pix.r = value[0], pix.g = value[1], pix.b = value[2];
  pix.m = std::max(0.f, std::min(1.f, value[3]));
}

// One 1D box pass of width `width` pixels on `values` (stride `step`), as the
// Flash Player blurs: taps -(m+1)..(m+1) around each pixel, the two outer
// ones weighted by the fractional part, divided by the width and rounded
// down to 8 bits. Samples outside are zero.
inline void boxPass(float *values, int count, int step, double width,
                    std::vector<double> &prefix, std::vector<float> &line) {
  if (width <= 1 || count <= 0) return;
  const double radius = (width - 1) / 2;
  const int m         = int(std::ceil(radius)) - 1;
  const double edge   = std::floor((radius - m) * 255) / 255;
  prefix.assign(size_t(count) + 1, 0.0);
  line.resize(count);
  for (int i = 0; i < count; ++i) {
    line[i]       = values[size_t(i) * step];
    prefix[i + 1] = prefix[i] + line[i];
  }
  auto sample = [&](int i) { return i < 0 || i >= count ? 0.f : line[i]; };
  for (int i = 0; i < count; ++i) {
    const int low = std::max(0, i - m), high = std::min(count, i + m + 1);
    double total = high > low ? prefix[high] - prefix[low] : 0.0;
    total += edge * (sample(i - m - 1) + sample(i + m + 1));
    const double v           = std::floor(total / width * 255 + 1e-6) / 255;
    values[size_t(i) * step] = float(std::max(0.0, std::min(1.0, v)));
  }
}

// Blurs the alpha plane `alpha` (lx * ly) in place.
inline void boxBlur(std::vector<float> &alpha, int lx, int ly, double widthX,
                    double widthY, int passes) {
  std::vector<double> prefix;
  std::vector<float> line;
  for (int pass = 0; pass < passes; ++pass) {
    if (widthX > 1)
      for (int y = 0; y < ly; ++y)
        boxPass(&alpha[size_t(y) * lx], lx, 1, widthX, prefix, line);
    if (widthY > 1)
      for (int x = 0; x < lx; ++x)
        boxPass(&alpha[x], ly, lx, widthY, prefix, line);
  }
}

// Pixels a blur of this width and passes reaches on each side.
inline int blurReach(double width, int passes) {
  return width > 1 ? passes * int(std::ceil((width - 1) / 2)) : 0;
}

inline void premultipliedColor(const TPixelD &color, float *out) {
  out[0] = float(color.r * color.m), out[1] = float(color.g * color.m);
  out[2] = float(color.b * color.m), out[3] = float(color.m);
}

// Shared geometry of the shadow FX: the blur in output pixels and the
// offset of the shadow (raster Y points up).
struct ShadowGeometry {
  double widthX = 0, widthY = 0;
  int passes = 1;
  int dx = 0, dy = 0;  // rounded to whole pixels, as Flash samples them
  int reachX = 0, reachY = 0;

  ShadowGeometry(const TRenderSettings &info, double blurX, double blurY,
                 int quality, double angle, double distance) {
    const double scale   = std::sqrt(std::abs(info.m_affine.det()));
    widthX               = std::max(0.0, blurX * scale);
    widthY               = std::max(0.0, blurY * scale);
    passes               = std::max(0, std::min(15, quality));
    const double radians = angle * std::acos(-1.0) / 180;
    dx     = int(std::lround(distance * scale * std::cos(radians)));
    dy     = int(std::lround(-distance * scale * std::sin(radians)));
    reachX = blurReach(widthX, passes) + 1;
    reachY = blurReach(widthY, passes) + 1;
  }
};

}  // namespace shadowbevel
