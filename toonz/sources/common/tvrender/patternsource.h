#pragma once

#include "traster.h"
#include "trop.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace PatternTextures {

constexpr int maxSourceSide     = 2048;
constexpr size_t maxSourceBytes = 128 * 1024 * 1024;

// All frames share a canvas and scale, preserving animated size and movement.
inline TDimension sourceSize(const TRect &content, int frameCount) {
  if (frameCount <= 0)
    throw std::invalid_argument("Trail source has no frames");
  const size_t pixelsPerFrame = maxSourceBytes / sizeof(TPixel32) / frameCount;
  const int maxSide =
      std::min(maxSourceSide, int(std::sqrt(double(pixelsPerFrame))));
  if (maxSide < 3)
    throw std::length_error(
        "Trail source has too many frames for its memory budget");
  if (content.isEmpty()) return TDimension(3, 3);
  const double scale = std::min({1.0, double(maxSide - 2) / content.getLx(),
                                 double(maxSide - 2) / content.getLy()});
  return TDimension(std::max(1, int(content.getLx() * scale)) + 2,
                    std::max(1, int(content.getLy() * scale)) + 2);
}

// Measure original pixels before this call. Resample only the artwork and keep
// one transparent destination pixel around it, even after a large reduction.
inline TRaster32P prepareSource(TRaster32P source, const TRect &content,
                                const TDimension &size) {
  TRaster32P result(size);
  result->clear();
  if (content.isEmpty()) return result;
  TRect intersection = content * source->getBounds();
  if (intersection.isEmpty()) return result;
  const TDimension inner(size.lx - 2, size.ly - 2);
  const TAffine affine =
      TTranslation(1, 1) *
      TScale(double(inner.lx) / content.getLx(),
             double(inner.ly) / content.getLy()) *
      TTranslation(intersection.x0 - content.x0, intersection.y0 - content.y0);
  TRop::resample(result, source->extract(intersection), affine);
  result->clearOutside(TRect(1, 1, inner.lx, inner.ly));
  return result;
}

}  // namespace PatternTextures
