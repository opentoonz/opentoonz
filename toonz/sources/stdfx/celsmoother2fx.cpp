#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "celsmoother2.h"
#include "celfxsourcegrid.h"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
using SmoothPixel = CelSmoothing::Pixel;

std::string gammaSlot(int index) {
  return "gammaColor" + std::to_string(index + 1);
}

bool canAllocateSource(int width, int height, int pixelSize) {
  if (width <= 0 || height <= 0 || pixelSize <= 0) return false;
  // The host raster manager multiplies these values in signed int.
  const size_t maximum = size_t(std::numeric_limits<int>::max());
  return size_t(width) <= maximum / size_t(pixelSize) / size_t(height);
}

template <class Pixel>
double pixelScale() {
  return Pixel::maxChannelValue;
}
template <>
double pixelScale<TPixelF>() {
  return 1;
}

template <class Pixel>
bool readPixels(const TRasterPT<Pixel> &raster,
                std::vector<SmoothPixel> &pixels, const int *canceled) {
  pixels.resize(CelSmoothing::pixelCount(raster->getLx(), raster->getLy()));
  const double scale = pixelScale<Pixel>();
  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    if (canceled && *canceled) {
      raster->unlock();
      return false;
    }
    const Pixel *row = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x)
      pixels[size_t(y) * raster->getLx() + x] = {
          row[x].r / scale, row[x].g / scale, row[x].b / scale,
          row[x].m / scale};
  }
  raster->unlock();
  return true;
}

template <class Pixel>
Pixel makePixel(const SmoothPixel &p) {
  const double scale = pixelScale<Pixel>();
  const auto channel = [scale](double v) {
    return int(std::floor(CelSmoother2::bounded(v, 0, 1, 0) * scale + 0.5));
  };
  return Pixel(channel(p.r), channel(p.g), channel(p.b), channel(p.a));
}
template <>
TPixelF makePixel<TPixelF>(const SmoothPixel &p) {
  if (!CelSmoother2::finite(p) || !(p.a > 0)) return TPixelF::Transparent;
  const double limit = std::numeric_limits<float>::max();
  return TPixelF(float(std::max(-limit, std::min(limit, p.r))),
                 float(std::max(-limit, std::min(limit, p.g))),
                 float(std::max(-limit, std::min(limit, p.b))), float(p.a));
}

template <class Pixel>
void writePixels(const TRasterPT<Pixel> &raster,
                 const std::vector<SmoothPixel> &pixels, int width, int height,
                 int offsetX, int offsetY, const int *canceled) {
  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    if (canceled && *canceled) {
      raster->unlock();
      raster->clear();
      return;
    }
    Pixel *row   = raster->pixels(y);
    const int sy = y + offsetY;
    for (int x = 0; x < raster->getLx(); ++x) {
      const int sx = x + offsetX;
      row[x]       = sx >= 0 && sx < width && sy >= 0 && sy < height
                         ? makePixel<Pixel>(pixels[size_t(sy) * width + sx])
                         : Pixel::Transparent;
    }
  }
  raster->unlock();
}
}  // namespace

class CelSmoother2Fx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelSmoother2Fx)
  TRasterFxPort m_source;
  TDoubleParamP m_useKey, m_invertKey, m_range, m_strength, m_extra;
  TDoubleParamP m_version, m_gammaMode, m_gamma, m_gammaCount;
  TPixelParamP m_key;
  std::array<TPixelParamP, CelSmoother2::gammaCapacity> m_colors;

  CelSmoother2::Settings snapshot(double frame) const {
    CelSmoother2::Settings result;
    result.useKey = CelSmoother2::choice(m_useKey->getValue(frame), 1) != 0;
    result.invertKey =
        CelSmoother2::choice(m_invertKey->getValue(frame), 1) != 0;
    const auto color = [](const TPixelD &p) {
      return SmoothPixel{p.r, p.g, p.b, 1};
    };
    result.key   = color(m_key->getValueD(frame));
    result.range = CelSmoother2::bounded(m_range->getValue(frame), 0, 255, 2);
    result.strength =
        CelSmoother2::bounded(m_strength->getValue(frame), 0, 100, 100);
    result.extra   = CelSmoother2::bounded(m_extra->getValue(frame), 0, 100, 0);
    result.version = CelSmoother2::choice(m_version->getValue(frame), 1);
    result.gammaMode = CelSmoother2::choice(m_gammaMode->getValue(frame), 2);
    result.gamma = CelSmoother2::bounded(m_gamma->getValue(frame), 1, 2.4, 2.4);
    result.gammaCount = CelSmoother2::choice(m_gammaCount->getValue(frame), 5);
    for (int i = 0; i < CelSmoother2::gammaCapacity; ++i)
      result.colors[i] = color(m_colors[i]->getValueD(frame));
    return result;
  }

  TRectD domain(double frame, const TRenderSettings &settings) const {
    if (!m_source.isConnected()) return TRectD();
    TRenderSettings full(settings);
    full.m_getFullSizeBBox = true;
    full.m_affine = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    TRectD box;
    auto *source = static_cast<TRasterFx *>(m_source.getFx());
    if (!source->getBBox(frame, box, full) || box.isEmpty()) return TRectD();
    if (box == TConsts::infiniteRectD) {
      box = CelFxSourceGrid::cameraBox(settings, full.m_affine);
      if (box.isEmpty())
        throw TException(
            "Cel Smoother v2: a finite camera is required for infinite "
            "sources");
    }
    if (box.isEmpty()) return TRectD();
    if (!std::isfinite(box.x0) || !std::isfinite(box.y0) ||
        !std::isfinite(box.x1) || !std::isfinite(box.y1) ||
        box == TConsts::infiniteRectD)
      throw TException(
          "Cel Smoother v2: a finite source or camera is required");
    // Every output tile uses the same source bounds and two transparent
    // borders.
    box = TRectD(std::floor(box.x0) - 2, std::floor(box.y0) - 2,
                 std::ceil(box.x1) + 2, std::ceil(box.y1) + 2);
    if (box.getLx() > std::numeric_limits<int>::max() ||
        box.getLy() > std::numeric_limits<int>::max() ||
        std::abs(box.x0) > std::numeric_limits<int>::max() / 2.0 ||
        std::abs(box.y0) > std::numeric_limits<int>::max() / 2.0)
      throw TException(
          "Cel Smoother v2: source dimensions exceed raster limits");
    CelSmoothing::pixelCount(int(box.getLx()), int(box.getLy()));
    return box;
  }

public:
  CelSmoother2Fx()
      : m_useKey(0.0)
      , m_invertKey(0.0)
      , m_range(2.0)
      , m_strength(100.0)
      , m_extra(0.0)
      , m_version(1.0)
      , m_gammaMode(0.0)
      , m_gamma(2.4)
      , m_gammaCount(1.0)
      , m_key(TPixel32::White) {
    addInputPort("Source", m_source);
    bindParam(this, "useColorKey", m_useKey);
    bindParam(this, "colorKey", m_key);
    bindParam(this, "invertColorKey", m_invertKey);
    bindParam(this, "smoothRange", m_range);
    bindParam(this, "smoothness", m_strength);
    bindParam(this, "extraSmooth", m_extra);
    bindParam(this, "smootherVersion", m_version);
    bindParam(this, "gammaCorrection", m_gammaMode);
    bindParam(this, "gammaValue", m_gamma);
    bindParam(this, "numberOfGammaColors", m_gammaCount);
    m_useKey->setUIChoices({"Off", "On"}, true);
    m_invertKey->setUIChoices({"Off", "On"}, true);
    m_version->setUIChoices({"v1", "v2"});
    m_gammaMode->setUIChoices({"None", "Gamma Colors", "All Colors"});
    m_useKey->setValueRange(0, 1);
    m_invertKey->setValueRange(0, 1);
    m_version->setValueRange(0, 1);
    m_gammaMode->setValueRange(0, 2);
    m_range->setValueRange(0, 255);
    m_strength->setValueRange(0, 100);
    m_extra->setValueRange(0, 100);
    m_gamma->setValueRange(1, 2.4);
    m_gammaCount->setValueRange(0, 5);
    m_key->enableMatte(false);
    for (int i = 0; i < CelSmoother2::gammaCapacity; ++i) {
      m_colors[i] = TPixelParamP(TPixel32::Black);
      m_colors[i]->enableMatte(false);
      bindParam(this, gammaSlot(i), m_colors[i]);
    }
    enableComputeInFloat(true);
  }

  bool requiresFullPrecisionSerialization() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return false; }
  bool usesCustomRenderGrid() const override { return true; }
  TAffine handledAffine(const TRenderSettings &, double frame) override {
    return CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  void compute(TTile &tile, double frame,
               const TRenderSettings &settings) override {
    if (CelFxSourceGrid::usesCameraDomain(this, m_source.getFx(), frame,
                                          settings))
      CelFxSourceGrid::computeInCamera(this, tile, frame, settings);
    else
      TRasterFx::compute(tile, frame, settings);
  }
  void dryCompute(TRectD &rect, double frame,
                  const TRenderSettings &settings) override {
    if (CelFxSourceGrid::usesCameraDomain(this, m_source.getFx(), frame,
                                          settings))
      CelFxSourceGrid::dryComputeInCamera(this, rect, frame, settings);
    else
      TRasterFx::dryCompute(rect, frame, settings);
  }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    if (!m_source.isConnected()) {
      box = TRectD();
      return false;
    }
    const bool result = m_source->doGetBBox(frame, box, settings);
    if (!box.isEmpty() && box != TConsts::infiniteRectD) {
      const TAffine placement =
          CelFxSourceGrid::renderAffine(m_source.getFx(), frame).inv();
      const double x = 2 * (std::abs(placement.a11) + std::abs(placement.a12));
      const double y = 2 * (std::abs(placement.a21) + std::abs(placement.a22));
      box            = TRectD(box.x0 - x, box.y0 - y, box.x1 + x, box.y1 + y);
    }
    return result;
  }
  void transform(double frame, int, const TRectD &,
                 const TRenderSettings &output, TRectD &inputBox,
                 TRenderSettings &input) override {
    input                   = output;
    input.m_getFullSizeBBox = true;
    input.m_affine = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    inputBox       = domain(frame, output);
  }
  void doDryCompute(TRectD &, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD box = domain(frame, settings);
    TRenderSettings input(settings);
    input.m_affine = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    if (!box.isEmpty()) m_source->dryCompute(box, frame, input);
  }
  int getMemoryRequirement(const TRectD &, double frame,
                           const TRenderSettings &settings) override {
    const TRectD box   = domain(frame, settings);
    const double bytes = box.getLx() * box.getLy() *
                         (4 * sizeof(SmoothPixel) + settings.m_bpp / 8.0);
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::ceil(std::max(0.0, bytes) / 1048576.0)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    const auto s       = snapshot(frame);
    const auto box     = domain(frame, settings);
    const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << TRasterFx::getAlias(frame, settings) << "[celSmoother2:2:"
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << s.useKey << ',' << s.invertKey << ',' << s.key.r << ',' << s.key.g
        << ',' << s.key.b << ',' << s.range << ',' << s.strength << ','
        << s.extra << ',' << s.version << ',' << s.gammaMode << ',' << s.gamma
        << ',' << s.gammaCount;
    for (const auto &p : s.colors)
      out << ';' << p.r << ',' << p.g << ',' << p.b;
    out << ';' << box.x0 << ',' << box.y0 << ',' << box.x1 << ',' << box.y1
        << ';' << grid.a11 << ',' << grid.a12 << ',' << grid.a13 << ','
        << grid.a21 << ',' << grid.a22 << ',' << grid.a23 << ']';
    return out.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() ||
        (settings.m_isCanceled && *settings.m_isCanceled))
      return;
    const TRectD box = domain(frame, settings);
    if (box.isEmpty()) return;
    const TRectD requested(tile.m_pos.x, tile.m_pos.y,
                           tile.m_pos.x + tile.getRaster()->getLx(),
                           tile.m_pos.y + tile.getRaster()->getLy());
    if ((requested * box).isEmpty()) return;
    const int width = int(box.getLx()), height = int(box.getLy());
    if (!canAllocateSource(width, height, tile.getRaster()->getPixelSize()))
      throw TException(
          "Cel Smoother v2: source raster exceeds allocation limits");
    TTile source;
    source.m_pos = box.getP00();
    source.setRaster(tile.getRaster()->create(width, height));
    source.getRaster()->setLinear(tile.getRaster()->isLinear());
    // Upstream nodes may only write their intersecting bounds. The two-pixel
    // smoothing border and unwritten source regions must start transparent.
    source.getRaster()->clear();
    TRenderSettings input(settings);
    input.m_affine = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    m_source->compute(source, frame, input);
    std::vector<SmoothPixel> pixels, result;
    bool read = false;
    if (TRaster32P raster = source.getRaster())
      read = readPixels<TPixel32>(raster, pixels, settings.m_isCanceled);
    else if (TRaster64P raster = source.getRaster())
      read = readPixels<TPixel64>(raster, pixels, settings.m_isCanceled);
    else if (TRasterFP raster = source.getRaster())
      read = readPixels<TPixelF>(raster, pixels, settings.m_isCanceled);
    else
      throw TException("Cel Smoother v2: unsupported pixel type");
    if (!read || !CelSmoother2::process(width, height, pixels, result,
                                        snapshot(frame), settings.m_isCanceled))
      return;
    const int x = int(std::floor(tile.m_pos.x - box.x0));
    const int y = int(std::floor(tile.m_pos.y - box.y0));
    if (TRaster32P raster = tile.getRaster())
      writePixels<TPixel32>(raster, result, width, height, x, y,
                            settings.m_isCanceled);
    else if (TRaster64P raster = tile.getRaster())
      writePixels<TPixel64>(raster, result, width, height, x, y,
                            settings.m_isCanceled);
    else if (TRasterFP raster = tile.getRaster())
      writePixels<TPixelF>(raster, result, width, height, x, y,
                           settings.m_isCanceled);
  }
};

FX_PLUGIN_IDENTIFIER(CelSmoother2Fx, "celSmoother2Fx")
