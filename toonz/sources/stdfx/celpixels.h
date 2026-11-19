#pragma once

#include "traster.h"
#include "tpixelutils.h"
#include "texception.h"
#include "trasterfx.h"

#include <algorithm>
#include <cmath>

#include <cstdint>
#include <limits>
#include <vector>

// Rasters of any depth as premultiplied doubles and back, for FX whose math
// works on plain colour arrays.
namespace CelPixels {

template <class Color>
std::vector<Color> read(const TRasterP &raster) {
  std::vector<Color> out(std::size_t(raster->getLx()) * raster->getLy());
  raster->lock();
  auto copy = [&](auto typed) {
    for (int y = 0; y < typed->getLy(); ++y) {
      const auto *row = typed->pixels(y);
      for (int x = 0; x < typed->getLx(); ++x) {
        const TPixelD p = PixelConverter<TPixelD>::from(row[x]);
        out[std::size_t(y) * typed->getLx() + x] = Color{p.r, p.g, p.b, p.m};
      }
    }
  };
  if (TRaster32P r = raster)
    copy(r);
  else if (TRaster64P r = raster)
    copy(r);
  else if (TRasterFP r = raster)
    copy(r);
  else {
    raster->unlock();
    throw TException("Unsupported pixel type");
  }
  raster->unlock();
  return out;
}

// Writes the window of `colors` (width `stride`) starting at (x0, y0).
template <class Color>
void write(const std::vector<Color> &colors, int stride, int x0, int y0,
           const TRasterP &raster) {
  raster->lock();
  auto copy = [&](auto typed) {
    using Pixel =
        typename std::remove_reference<decltype(*typed->pixels(0))>::type;
    for (int y = 0; y < typed->getLy(); ++y) {
      Pixel *row = typed->pixels(y);
      for (int x = 0; x < typed->getLx(); ++x) {
        const Color &c = colors[std::size_t(y + y0) * stride + x + x0];
        row[x] = PixelConverter<Pixel>::from(TPixelD(c.r, c.g, c.b, c.a));
      }
    }
  };
  if (TRaster32P r = raster)
    copy(r);
  else if (TRaster64P r = raster)
    copy(r);
  else if (TRasterFP r = raster)
    copy(r);
  else {
    raster->unlock();
    throw TException("Unsupported pixel type");
  }
  raster->unlock();
}

// A raster of the tile's type, enlarged by halo on every side.
inline TRasterP padded(const TRasterP &tile, int halo) {
  const std::int64_t w = std::int64_t(tile->getLx()) + 2LL * halo;
  const std::int64_t h = std::int64_t(tile->getLy()) + 2LL * halo;
  const int bytes      = tile->getPixelSize();
  if (halo < 0 || w <= 0 || h <= 0 || w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max() || bytes <= 0 ||
      w * h > std::numeric_limits<int>::max() / bytes)
    throw TException("The requested raster is too large");
  TRasterP raster = tile->create(int(w), int(h));
  raster->clear();
  return raster;
}

inline double bounded(double value, double minimum, double maximum) {
  return std::isfinite(value) ? std::max(minimum, std::min(maximum, value))
                              : minimum;
}
inline int discrete(double value, int maximum) {
  return int(std::floor(bounded(value, 0, maximum) + 0.5));
}
// Radii in output pixels at 100%: the render's scale enlarges them
inline double renderScale(const TRenderSettings &settings) {
  const double scale = std::sqrt(std::abs(settings.m_affine.det()));
  return std::isfinite(scale) && scale > 0 ? scale : 1.0;
}
// The affines these FX handle themselves, as the standard blur does
inline bool isotropic(const TAffine &affine) {
  return affine.isIsotropic(0.001);
}
// A tile and its halo's memory, in MB as the tile subdivision expects
inline int megabytes(const TRectD &rect, int halo, double bytesPerPixel) {
  const double bytes =
      (rect.getLx() + 2.0 * halo) * (rect.getLy() + 2.0 * halo) * bytesPerPixel;
  return int(std::min(double(std::numeric_limits<int>::max()),
                      std::max(0.0, std::ceil(bytes / (1024.0 * 1024.0)))));
}

}  // namespace CelPixels
