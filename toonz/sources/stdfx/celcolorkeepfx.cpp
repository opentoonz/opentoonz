#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"
#include "tstream.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
constexpr int colorCapacity = 100;

int activeColorCount(double value) {
  if (!std::isfinite(value)) return 0;
  return int(std::floor(std::max(0.0, std::min(100.0, value)) + 0.5));
}

std::string colorSlotName(int index) {
  std::ostringstream name;
  name.imbue(std::locale::classic());
  name << "color" << std::setw(3) << std::setfill('0') << index + 1;
  return name.str();
}

bool boundedColor(const TPixelD &color) {
  return std::isfinite(color.r) && std::isfinite(color.g) &&
         std::isfinite(color.b) && color.r >= 0 && color.r <= 1 &&
         color.g >= 0 && color.g <= 1 && color.b >= 0 && color.b <= 1;
}

// Quantize the selected straight RGB, then use the same coverage operation as
// the host. Native 8-bit premultiplication rounds; its 16-bit helper truncates.
bool matchesColor(const TPixel32 &pixel, const TPixelD &color) {
  if (!boundedColor(color)) return false;
  TPixel32 selected = toPixel32(TPixelD(color.r, color.g, color.b, 1));
  selected.m        = pixel.m;
  selected          = premultiply(selected);
  return pixel.r == selected.r && pixel.g == selected.g &&
         pixel.b == selected.b;
}

bool matchesColor(const TPixel64 &pixel, const TPixelD &color) {
  if (!boundedColor(color)) return false;
  TPixel64 selected = toPixel64(TPixelD(color.r, color.g, color.b, 1));
  selected.m        = pixel.m;
  selected          = premultiply(selected);
  return pixel.r == selected.r && pixel.g == selected.g &&
         pixel.b == selected.b;
}

bool matchesColor(const TPixelF &pixel, const TPixelD &color) {
  // Floating RGB remains unclamped, including finite HDR values. The tolerance
  // is fixed in straight RGB, so partial coverage does not broaden the key.
  const float tolerance = 0.0001f * pixel.m;
  return std::fabs(pixel.r - float(color.r) * pixel.m) <= tolerance &&
         std::fabs(pixel.g - float(color.g) * pixel.m) <= tolerance &&
         std::fabs(pixel.b - float(color.b) * pixel.m) <= tolerance;
}

template <class Pixel>
void keepColors(const TRasterPT<Pixel> &raster,
                const std::array<TPixelD, colorCapacity> &colors, int count,
                const int *isCanceled) {
  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    if (isCanceled && *isCanceled) {
      raster->unlock();
      raster->clear();
      return;
    }
    Pixel *row = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x) {
      Pixel &pixel = row[x];
      bool keep    = false;
      if (pixel.m > 0 && std::isfinite(double(pixel.m))) {
        for (int i = 0; i < count; ++i) {
          if (matchesColor(pixel, colors[i])) {
            keep = true;
            break;
          }
        }
      }
      // Zero RGB as well as alpha: downstream compositors expect premultiplied
      // pixels and would otherwise let rejected RGB leak into the composite.
      if (!keep) pixel = Pixel::Transparent;
    }
  }
  raster->unlock();
}
}  // namespace

class CelColorKeepFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelColorKeepFx)

  TRasterFxPort m_source;
  TDoubleParamP m_enabledColors;
  std::array<TPixelParamP, colorCapacity> m_colors;

public:
  CelColorKeepFx() : m_enabledColors(1.0) {
    addInputPort("Source", m_source);
    bindParam(this, "enabledColors", m_enabledColors);
    m_enabledColors->setValueRange(0.0, 100.0);
    for (int i = 0; i < colorCapacity; ++i) {
      m_colors[i] = TPixelParamP(TPixel32::Black);
      m_colors[i]->enableMatte(false);
      bindParam(this, colorSlotName(i), m_colors[i]);
    }
    enableComputeInFloat(true);
  }

  bool canHandle(const TRenderSettings &, double) override { return false; }

  bool requiresFullPrecisionSerialization() const override { return true; }

  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &settings) override {
    if (!m_source.isConnected()) {
      bbox = TRectD();
      return false;
    }
    return m_source->doGetBBox(frame, bbox, settings);
  }

  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    // The base alias rounds parameter values to three decimals. Append the
    // evaluated RGB at full precision to distinguish fine keys and HDR colors.
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    alias << TRasterFx::getAlias(frame, settings) << "[celKeep:"
          << std::setprecision(std::numeric_limits<double>::max_digits10);
    const int count = activeColorCount(m_enabledColors->getValue(frame));
    alias << count;
    for (int i = 0; i < count; ++i) {
      const TPixelD color = m_colors[i]->getValueD(frame);
      alias << ';' << color.r << ',' << color.g << ',' << color.b;
    }
    return alias.str() + ']';
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    if (!m_source.isConnected() ||
        (settings.m_isCanceled && *settings.m_isCanceled)) {
      tile.getRaster()->clear();
      return;
    }
    // An upstream effect may render only its bounding box, leaving the
    // remainder untouched. Start from transparent pixels in that case.
    tile.getRaster()->clear();
    m_source->compute(tile, frame, settings);
    const int count = activeColorCount(m_enabledColors->getValue(frame));
    if (count == 0) {
      tile.getRaster()->clear();
      return;
    }
    // Per-render immutable values: tiles and frames never share mutable state.
    std::array<TPixelD, colorCapacity> colors;
    for (int i = 0; i < count; ++i) colors[i] = m_colors[i]->getValueD(frame);
    if (TRaster32P raster = tile.getRaster())
      keepColors<TPixel32>(raster, colors, count, settings.m_isCanceled);
    else if (TRaster64P raster = tile.getRaster())
      keepColors<TPixel64>(raster, colors, count, settings.m_isCanceled);
    else if (TRasterFP raster = tile.getRaster())
      keepColors<TPixelF>(raster, colors, count, settings.m_isCanceled);
    else
      throw TException("Cel Color Keep: unsupported pixel type");
  }
};

FX_PLUGIN_IDENTIFIER(CelColorKeepFx, "celColorKeepFx")
