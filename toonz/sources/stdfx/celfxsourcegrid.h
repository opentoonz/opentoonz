#pragma once

#include "trasterfx.h"
#include "tfxattributes.h"
#include "toonz/tcolumnfx.h"

#include <cmath>
#include <set>

// Pixel-neighborhood effects use the raster level's sampling grid before its
// DPI and geometry placements. Ambiguous multi-source graphs retain the
// ordinary world grid instead of arbitrarily choosing one input's resolution.
namespace CelFxSourceGrid {

inline bool invertible(const TAffine &affine) {
  for (double value :
       {affine.a11, affine.a12, affine.a13, affine.a21, affine.a22, affine.a23})
    if (!std::isfinite(value)) return false;
  const double determinant = affine.det();
  if (!std::isfinite(determinant) || determinant == 0.0) return false;
  const TAffine inverse = affine.inv();
  for (double value : {inverse.a11, inverse.a12, inverse.a13, inverse.a21,
                       inverse.a22, inverse.a23})
    if (!std::isfinite(value)) return false;
  return true;
}

struct Grid {
  TAffine placement;
  TPointD phase;
  // Optional original raster canvas, before DPI and geometry placements.
  // Existing consumers only need placement/phase and retain their behavior.
  TRectD canvas;
  bool hasCanvas = false;
};

inline Grid discover(TFx *source, double frame) {
  TAffine placement;
  TRasterFx *canvas     = nullptr;
  bool markerNeedsInput = false;
  std::set<TFx *> visited;
  while (source) {
    if (!visited.insert(source).second) return Grid();
    auto *raster = dynamic_cast<TRasterFx *>(source);
    if (!raster) return Grid();
    const bool enabled = source->getAttributes()->isEnabled();
    // Identity DPI needs no affine node. The scene builder marks the original
    // raster source in that case, so keep placements accumulated above it.
    if (!canvas && enabled && source->getInputPortCount() == 0 &&
        source->getName() == L"LevelColumn AffineFx") {
      // Vector levels also carry the identity-DPI marker, but their artwork
      // bounds are continuous geometry, not the origin of a raster canvas.
      auto *level = dynamic_cast<TLevelColumnFx *>(source);
      if (level && level->canHandle(TRenderSettings(), frame)) return Grid();
      canvas = raster;
    }
    auto *geometry = dynamic_cast<TGeometryFx *>(source);
    if (!canvas && enabled && geometry &&
        geometry->getActiveTimeRegion().contains(frame)) {
      placement = placement * geometry->getPlacement(frame);
      if (!invertible(placement)) return Grid();
      auto *affine = dynamic_cast<NaAffineFx *>(geometry);
      if (affine && (affine->isDpiAffine() ||
                     affine->getName() == L"LevelColumn AffineFx"))
        markerNeedsInput = true;
    }
    TFx *next = nullptr;
    if (!enabled) {
      const int port = source->getPreferredInputPort();
      if (port >= 0 && port < source->getInputPortCount() &&
          source->getInputPort(port)->isConnected())
        next = source->getInputPort(port)->getFx();
    } else {
      for (int port = 0; port < source->getInputPortCount(); ++port) {
        TFxPort *input = source->getInputPort(port);
        if (!input->isConnected()) continue;
        if (next || !dynamic_cast<TRasterFx *>(input->getFx())) return Grid();
        next = input->getFx();
      }
    }
    if (markerNeedsInput) {
      canvas = dynamic_cast<TRasterFx *>(next);
      if (!canvas) return Grid();
      markerNeedsInput = false;
    }
    source = next;
  }
  if (!canvas) return Grid();
  // Raster levels are centered on the level canvas. An odd-sized canvas has a
  // half-pixel origin that must survive DPI cancellation. Query the unrounded
  // full canvas, not the rounded render bbox or the content's savebox.
  TRenderSettings settings;
  settings.m_getFullSizeBBox = true;
  TRectD bounds;
  if (!canvas->doGetBBox(frame, bounds, settings) || bounds.isEmpty() ||
      !std::isfinite(bounds.x0) || !std::isfinite(bounds.y0) ||
      !std::isfinite(bounds.x1) || !std::isfinite(bounds.y1))
    return Grid();
  Grid result;
  result.placement = placement;
  result.canvas    = bounds;
  result.hasCanvas = true;
  result.phase     = TPointD(std::floor(bounds.x0) - bounds.x0,
                             std::floor(bounds.y0) - bounds.y0);
  return result;
}

inline TAffine nativeToWorld(TFx *source, double frame) {
  return discover(source, frame).placement;
}

inline TAffine renderAffine(TFx *source, double frame) {
  const Grid sourceGrid = discover(source, frame);
  TAffine grid = TTranslation(sourceGrid.phase) * sourceGrid.placement.inv();
  // TRasterFx moves whole-pixel translations into the tile origin before
  // consulting handledAffine. Return the same canonical form to prevent a
  // repeated affine wrapper while retaining the native sampling phase.
  grid.a13 -= std::floor(grid.a13);
  grid.a23 -= std::floor(grid.a23);
  // x - floor(x) rounds to exactly 1 for a tiny negative x (residue of the
  // inverse): that is no canonical form, and TRasterFx would wrap the fx
  // again on each call, recursing without end
  if (grid.a13 >= 1.0) grid.a13 = 0.0;
  if (grid.a23 >= 1.0) grid.a23 = 0.0;
  return grid;
}

inline bool finiteBox(const TRectD &box) {
  return std::isfinite(box.x0) && std::isfinite(box.y0) &&
         std::isfinite(box.x1) && std::isfinite(box.y1) && !box.isEmpty() &&
         box != TConsts::infiniteRectD;
}

// True when an enabled fx covers an infinite source with the camera box.
inline bool usesCameraDomain(TRasterFx *fx, TFx *source, double frame,
                             const TRenderSettings &settings) {
  auto *raster = dynamic_cast<TRasterFx *>(source);
  if (!raster || !fx->getAttributes()->isEnabled()) return false;
  TRenderSettings full(settings);
  full.m_getFullSizeBBox = true;
  TRectD box;
  return raster->getBBox(frame, box, full) && !box.isEmpty() && !finiteBox(box);
}

// TRasterFx::compute moves tile fractions and whole-pixel translations into
// the affine, then wraps the fx in a TrFx that renders it on handledAffine.
// Neither step moves m_cameraBox, which stays in the caller's coordinates.
// Returns the tile origin the host would use, with settings whose camera box
// is in the coordinates the fx actually renders in.
inline TPointD cameraSettings(TRasterFx *fx, double frame,
                              const TPointD &position,
                              const TRenderSettings &settings,
                              TRenderSettings &normalized) {
  const TPointD integral(std::floor(position.x), std::floor(position.y));
  const TPointD fraction = position - integral;
  const TPointD translation(std::floor(settings.m_affine.a13 - fraction.x),
                            std::floor(settings.m_affine.a23 - fraction.y));
  const TPointD origin = integral - translation;
  const TPointD delta  = position - origin;
  normalized           = settings;
  normalized.m_affine.a13 -= delta.x;
  normalized.m_affine.a23 -= delta.y;
  if (!finiteBox(normalized.m_cameraBox)) return origin;
  normalized.m_cameraBox -= delta;
  const TAffine handled = fx->handledAffine(normalized, frame);
  if (handled != normalized.m_affine && invertible(handled) &&
      invertible(normalized.m_affine))
    normalized.m_cameraBox =
        handled * normalized.m_affine.inv() * normalized.m_cameraBox;
  return origin;
}

inline void computeInCamera(TRasterFx *fx, TTile &tile, double frame,
                            const TRenderSettings &settings) {
  TRenderSettings normalized;
  const TPointD original = tile.m_pos;
  tile.m_pos = cameraSettings(fx, frame, original, settings, normalized);
  try {
    fx->TRasterFx::compute(tile, frame, normalized);
  } catch (...) {
    tile.m_pos = original;
    throw;
  }
  tile.m_pos = original;
}

inline void dryComputeInCamera(TRasterFx *fx, TRectD &rect, double frame,
                               const TRenderSettings &settings) {
  TRenderSettings normalized;
  const TPointD origin =
      cameraSettings(fx, frame, rect.getP00(), settings, normalized);
  TRectD normalizedRect(origin, rect.getSize());
  fx->TRasterFx::dryCompute(normalizedRect, frame, normalized);
}

// The camera box of settings, in the coordinates of the given render grid.
inline TRectD cameraBox(const TRenderSettings &settings, const TAffine &grid) {
  if (!finiteBox(settings.m_cameraBox) || grid == settings.m_affine ||
      !invertible(grid) || !invertible(settings.m_affine))
    return settings.m_cameraBox;
  return grid * settings.m_affine.inv() * settings.m_cameraBox;
}

}  // namespace CelFxSourceGrid
