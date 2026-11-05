#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "celboil.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

// Cel Boil: a controlled hand-drawn boil. The image, or only its lines, is
// displaced by a smooth noise field that holds for Cycle frames and then
// changes, so a held drawing does not look frozen. The math is in celboil.h.
namespace {
double bounded(double value, double minimum, double maximum) {
  return std::isfinite(value) ? std::max(minimum, std::min(maximum, value))
                              : minimum;
}
int discrete(double value, int maximum) {
  return int(std::floor(bounded(value, 0, maximum) + 0.5));
}
TDimension paddedSize(int width, int height, int halo, int pixelBytes) {
  const int64_t w = int64_t(width) + 2LL * halo;
  const int64_t h = int64_t(height) + 2LL * halo;
  if (w <= 0 || h <= 0 || w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max() || pixelBytes <= 0 ||
      w * h > std::numeric_limits<int>::max() / pixelBytes)
    throw TException("Cel Boil: requested raster is too large");
  return TDimension(int(w), int(h));
}
class RasterLock {
  TRasterP m_raster;

public:
  explicit RasterLock(const TRasterP &raster) : m_raster(raster) {
    m_raster->lock();
  }
  ~RasterLock() { m_raster->unlock(); }
};

template <class Pixel>
void boil(const TRasterPT<Pixel> &source, const TRasterPT<Pixel> &output,
          int halo, TPointD origin, const CelBoil::Settings &settings,
          double frame, const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  std::vector<CelBoil::Color> image(CelDistance::checkedArea(w, h));
  {
    RasterLock lock(source);
    for (int y = 0; y < h; ++y) {
      const Pixel *row = source->pixels(y);
      for (int x = 0; x < w; ++x) {
        const TPixelD p          = PixelConverter<TPixelD>::from(row[x]);
        image[size_t(y) * w + x] = {p.r, p.g, p.b, p.m};
      }
    }
  }
  std::vector<CelBoil::Color> result;
  if (!CelBoil::render(image, w, h, origin.x, origin.y, settings, frame, result,
                       canceled)) {
    output->clear();
    return;
  }
  RasterLock lock(output);
  for (int y = 0; y < output->getLy(); ++y) {
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const CelBoil::Color &c = result[size_t(y + halo) * w + x + halo];
      row[x] = PixelConverter<Pixel>::from(TPixelD(c.r, c.g, c.b, c.a));
    }
  }
}
}  // namespace

class CelBoilFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelBoilFx)
  TRasterFxPort m_source;
  TDoubleParamP m_target, m_amplitude, m_size, m_detail, m_cycle, m_blend,
      m_seed, m_lineThreshold;

  // Amplitude and size are in the level's own pixels
  double pixelScale(double frame, const TRenderSettings &settings) const {
    const TAffine drawing = settings.m_affine * CelFxSourceGrid::nativeToWorld(
                                                    m_source.getFx(), frame);
    const double scale = std::sqrt(std::abs(drawing.det()));
    return std::isfinite(scale) && std::abs(scale - 1) >= 1e-10 && scale > 0
               ? scale
               : 1.0;
  }
  CelBoil::Settings settings(double frame,
                             const TRenderSettings &render) const {
    const double scale = pixelScale(frame, render);
    CelBoil::Settings s;
    s.amplitude     = bounded(m_amplitude->getValue(frame), 0, 1000) * scale;
    s.size          = bounded(m_size->getValue(frame), 1, 100) * scale;
    s.detail        = bounded(m_detail->getValue(frame), 0, 1);
    s.cycle         = std::max(1, discrete(m_cycle->getValue(frame), 1000));
    s.blend         = bounded(m_blend->getValue(frame), 0, 1);
    s.seed          = std::uint32_t(discrete(m_seed->getValue(frame), 100000));
    s.target        = discrete(m_target->getValue(frame), 1) == 1
                          ? CelBoil::Target::Lines
                          : CelBoil::Target::Image;
    s.lineThreshold = bounded(m_lineThreshold->getValue(frame), 0, 1);
    return s;
  }
  bool isStill(double frame) const {
    return bounded(m_amplitude->getValue(frame), 0, 1000) <= 0.0;
  }

public:
  CelBoilFx()
      : m_target(0.0)
      , m_amplitude(1.5)
      , m_size(12.0)
      , m_detail(0.0)
      , m_cycle(2.0)
      , m_blend(0.0)
      , m_seed(1.0)
      , m_lineThreshold(0.35) {
    addInputPort("Source", m_source);
    m_target->setValueRange(0, 1);
    m_target->setUIChoices({"Whole Image", "Lines Only"});
    m_amplitude->setValueRange(0, 1000);
    m_amplitude->setUISliderRange(0, 10);
    m_size->setValueRange(1, 100);
    m_detail->setValueRange(0, 1);
    m_cycle->setValueRange(1, 1000);
    m_cycle->setUISliderRange(1, 12);
    m_blend->setValueRange(0, 1);
    m_seed->setValueRange(0, 100000);
    m_seed->setUISliderRange(0, 1000);
    m_lineThreshold->setValueRange(0, 1);
    bindParam(this, "target", m_target);
    bindParam(this, "amplitude", m_amplitude);
    bindParam(this, "size", m_size);
    bindParam(this, "detail", m_detail);
    bindParam(this, "cycle", m_cycle);
    bindParam(this, "blend", m_blend);
    bindParam(this, "seed", m_seed);
    bindParam(this, "lineThreshold", m_lineThreshold);
    enableComputeInFloat(true);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &, double frame) override {
    return isStill(frame);
  }
  TAffine handledAffine(const TRenderSettings &settings,
                        double frame) override {
    return isStill(frame)
               ? settings.m_affine
               : CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    box = TRectD();
    if (!m_source.isConnected() || !m_source->doGetBBox(frame, box, settings))
      return false;
    if (!box.isEmpty() && box != TConsts::infiniteRectD) {
      const TAffine placement =
          CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
      const double reach =
          bounded(m_amplitude->getValue(frame), 0, 1000) *
              (1.0 + bounded(m_detail->getValue(frame), 0, 1)) +
          1.0;
      const double dx = reach * std::hypot(placement.a11, placement.a12);
      const double dy = reach * std::hypot(placement.a21, placement.a22);
      box = TRectD(box.x0 - dx, box.y0 - dy, box.x1 + dx, box.y1 + dy);
    }
    return !box.isEmpty();
  }
  void transform(double frame, int, const TRectD &output,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override {
    inputSettings = settings;
    input =
        isStill(frame)
            ? output
            : output.enlarge(CelBoil::halo(this->settings(frame, settings)));
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD input =
        isStill(frame)
            ? rect
            : rect.enlarge(CelBoil::halo(this->settings(frame, settings)));
    m_source->dryCompute(input, frame, settings);
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override {
    if (isStill(frame)) return 0;
    const double h = CelBoil::halo(this->settings(frame, settings));
    // Source, result, line and paint colours, masks and a distance field
    const double bytes = (rect.getLx() + 2 * h) * (rect.getLy() + 2 * h) * 160;
    const double megabytes = std::ceil(bytes / (1024.0 * 1024.0));
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::max(0.0, megabytes)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    const CelBoil::Settings s = this->settings(frame, settings);
    // The noise plane, not the frame: the frames of a held wobble share a
    // cache entry, and each new wobble gets its own
    alias << TRasterFx::getAlias(frame, settings) << "[celBoil:1:"
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << int(s.target) << ',' << s.amplitude << ',' << s.size << ','
          << s.detail << ',' << s.seed << ',' << s.lineThreshold << ','
          << CelBoil::phase(s, frame);
    const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
    for (double value :
         {grid.a11, grid.a12, grid.a13, grid.a21, grid.a22, grid.a23})
      alias << ',' << value;
    alias << ']';
    return alias.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() ||
        (settings.m_isCanceled && *settings.m_isCanceled))
      return;
    if (isStill(frame)) {
      m_source->compute(tile, frame, settings);
      if (settings.m_isCanceled && *settings.m_isCanceled)
        tile.getRaster()->clear();
      return;
    }
    const CelBoil::Settings s = this->settings(frame, settings);
    const int h               = CelBoil::halo(s);
    const TDimension size =
        paddedSize(tile.getRaster()->getLx(), tile.getRaster()->getLy(), h,
                   tile.getRaster()->getPixelSize());
    TTile input;
    input.setRaster(tile.getRaster()->create(size.lx, size.ly));
    input.m_pos = tile.m_pos - TPointD(h, h);
    input.getRaster()->clear();
    m_source->compute(input, frame, settings);
    if (settings.m_isCanceled && *settings.m_isCanceled) return;
    if (TRaster32P output = tile.getRaster())
      boil<TPixel32>(input.getRaster(), output, h, input.m_pos, s, frame,
                     settings.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      boil<TPixel64>(input.getRaster(), output, h, input.m_pos, s, frame,
                     settings.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      boil<TPixelF>(input.getRaster(), output, h, input.m_pos, s, frame,
                    settings.m_isCanceled);
    else
      throw TException("Cel Boil: unsupported pixel type");
  }
};
FX_PLUGIN_IDENTIFIER(CelBoilFx, "celBoilFx")
