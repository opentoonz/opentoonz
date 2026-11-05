#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "printprocess.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

// Print Process: offset printing with CMYK separation, rotated dot screens,
// misregistration, dot gain and paper. The math is in printprocess.h.
namespace {
const char *const plateNames[PrintProcess::plateCount] = {"Cyan", "Magenta",
                                                          "Yellow", "Black"};
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
    throw TException("Print Process: requested raster is too large");
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
void print(const TRasterPT<Pixel> &source, const TRasterPT<Pixel> &output,
           int halo, TPointD origin, const PrintProcess::Settings &settings,
           const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  std::vector<PrintProcess::Color> image(CelDistance::checkedArea(w, h));
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
  std::vector<PrintProcess::Color> result;
  if (!PrintProcess::render(image, w, h, origin.x, origin.y, settings, result,
                            canceled)) {
    output->clear();
    return;
  }
  RasterLock lock(output);
  for (int y = 0; y < output->getLy(); ++y) {
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const PrintProcess::Color &c = result[size_t(y + halo) * w + x + halo];
      row[x] = PixelConverter<Pixel>::from(TPixelD(c.r, c.g, c.b, c.a));
    }
  }
}
}  // namespace

class PrintProcessFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(PrintProcessFx)
  TRasterFxPort m_source;
  TDoubleParamP m_cellSize, m_shape, m_blackGeneration, m_dotGain, m_softness,
      m_grain, m_grainSize, m_seed, m_background, m_output;
  TPixelParamP m_paper;
  struct Plate {
    TDoubleParamP use, angle, offsetX, offsetY;
    TPixelParamP ink;
    Plate()
        : use(1.0)
        , angle(0.0)
        , offsetX(0.0)
        , offsetY(0.0)
        , ink(TPixel32::Black) {}
  };
  std::array<Plate, PrintProcess::plateCount> m_plates;

  // Screen cells, grain and misregistration are in the level's own pixels
  double pixelScale(double frame, const TRenderSettings &settings) const {
    const TAffine drawing = settings.m_affine * CelFxSourceGrid::nativeToWorld(
                                                    m_source.getFx(), frame);
    const double scale = std::sqrt(std::abs(drawing.det()));
    return std::isfinite(scale) && std::abs(scale - 1) >= 1e-10 && scale > 0
               ? scale
               : 1.0;
  }
  PrintProcess::Settings settings(double frame,
                                  const TRenderSettings &render) const {
    const double scale = pixelScale(frame, render);
    PrintProcess::Settings s;
    s.cellSize = bounded(m_cellSize->getValue(frame), 1, 500) * scale;
    s.shape    = PrintProcess::Shape(discrete(m_shape->getValue(frame), 2));
    s.blackGeneration = bounded(m_blackGeneration->getValue(frame), 0, 1);
    s.dotGain         = bounded(m_dotGain->getValue(frame), 0, 1);
    s.softness        = bounded(m_softness->getValue(frame), 0, 4);
    s.grain           = bounded(m_grain->getValue(frame), 0, 1);
    s.grainSize       = bounded(m_grainSize->getValue(frame), 1, 100) * scale;
    s.seed = std::uint32_t(discrete(m_seed->getValue(frame), 100000));
    s.transparentBackground = discrete(m_background->getValue(frame), 1) == 1;
    s.output = PrintProcess::Output(discrete(m_output->getValue(frame), 4));
    const TPixelD paper = m_paper->getValueD(frame);
    s.paper             = {paper.r, paper.g, paper.b};
    for (int k = 0; k < PrintProcess::plateCount; ++k) {
      const Plate &p         = m_plates[k];
      PrintProcess::Plate &q = s.plates[k];
      q.enabled              = discrete(p.use->getValue(frame), 1) == 1;
      q.angle                = bounded(p.angle->getValue(frame), -360, 360);
      q.offsetX = bounded(p.offsetX->getValue(frame), -1000, 1000) * scale;
      q.offsetY = bounded(p.offsetY->getValue(frame), -1000, 1000) * scale;
      const TPixelD ink = p.ink->getValueD(frame);
      q.ink             = {ink.r, ink.g, ink.b};
    }
    return s;
  }

public:
  PrintProcessFx()
      : m_cellSize(6.0)
      , m_shape(0.0)
      , m_blackGeneration(0.8)
      , m_dotGain(0.1)
      , m_softness(0.5)
      , m_grain(0.15)
      , m_grainSize(2.0)
      , m_seed(1.0)
      , m_background(0.0)
      , m_output(0.0)
      , m_paper(TPixel32(252, 249, 240)) {
    addInputPort("Source", m_source);
    m_cellSize->setValueRange(1, 500);
    m_cellSize->setUISliderRange(2, 40);
    m_shape->setValueRange(0, 2);
    m_shape->setUIChoices({"Round", "Diamond", "Line"});
    m_blackGeneration->setValueRange(0, 1);
    m_dotGain->setValueRange(0, 1);
    m_softness->setValueRange(0, 4);
    m_grain->setValueRange(0, 1);
    m_grainSize->setValueRange(1, 100);
    m_grainSize->setUISliderRange(1, 10);
    m_seed->setValueRange(0, 100000);
    m_seed->setUISliderRange(0, 1000);
    m_background->setValueRange(0, 1);
    m_background->setUIChoices({"Paper", "Transparent"});
    m_output->setValueRange(0, 4);
    m_output->setUIChoices({"Print", "Cyan Plate", "Magenta Plate",
                            "Yellow Plate", "Black Plate"});
    m_paper->enableMatte(false);
    bindParam(this, "cellSize", m_cellSize);
    bindParam(this, "shape", m_shape);
    bindParam(this, "blackGeneration", m_blackGeneration);
    bindParam(this, "dotGain", m_dotGain);
    bindParam(this, "softness", m_softness);
    bindParam(this, "paper", m_paper);
    bindParam(this, "grain", m_grain);
    bindParam(this, "grainSize", m_grainSize);
    bindParam(this, "seed", m_seed);
    bindParam(this, "background", m_background);
    bindParam(this, "output", m_output);
    const PrintProcess::Settings defaults;
    for (int k = 0; k < PrintProcess::plateCount; ++k) {
      Plate &p                     = m_plates[k];
      const std::string name       = plateNames[k];
      const PrintProcess::Plate &d = defaults.plates[k];
      p.use->setValueRange(0, 1);
      p.use->setUIChoices({"Off", "On"}, true);
      p.angle->setDefaultValue(d.angle);
      p.angle->setValueRange(-360, 360);
      p.angle->setMeasureName("angle");
      p.offsetX->setValueRange(-1000, 1000);
      p.offsetY->setValueRange(-1000, 1000);
      p.offsetX->setUISliderRange(-10, 10);
      p.offsetY->setUISliderRange(-10, 10);
      p.ink->setDefaultValue(TPixel32(int(std::round(d.ink.r * 255)),
                                      int(std::round(d.ink.g * 255)),
                                      int(std::round(d.ink.b * 255))));
      p.ink->enableMatte(false);
      bindParam(this, "use" + name, p.use);
      bindParam(this, "angle" + name, p.angle);
      bindParam(this, "offset" + name + "X", p.offsetX);
      bindParam(this, "offset" + name + "Y", p.offsetY);
      bindParam(this, "ink" + name, p.ink);
    }
    enableComputeInFloat(true);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return false; }
  TAffine handledAffine(const TRenderSettings &, double frame) override {
    return CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    box = TRectD();
    if (!m_source.isConnected()) return false;
    // Paper covers the whole frame; a transparent background keeps the
    // source's extent, widened by the misregistration
    if (discrete(m_background->getValue(frame), 1) == 0) {
      box = TConsts::infiniteRectD;
      return true;
    }
    if (!m_source->doGetBBox(frame, box, settings)) return false;
    if (!box.isEmpty() && box != TConsts::infiniteRectD) {
      double reach = 1.0;
      for (const Plate &p : m_plates)
        if (discrete(p.use->getValue(frame), 1) == 1)
          reach = std::max(
              reach, 1.0 + std::max(std::abs(bounded(p.offsetX->getValue(frame),
                                                     -1000, 1000)),
                                    std::abs(bounded(p.offsetY->getValue(frame),
                                                     -1000, 1000))));
      const TAffine placement =
          CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
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
    input = output.enlarge(PrintProcess::halo(this->settings(frame, settings)));
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD input =
        rect.enlarge(PrintProcess::halo(this->settings(frame, settings)));
    m_source->dryCompute(input, frame, settings);
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override {
    const double h = PrintProcess::halo(this->settings(frame, settings));
    // Source and result colours
    const double bytes = (rect.getLx() + 2 * h) * (rect.getLy() + 2 * h) * 64;
    const double megabytes = std::ceil(bytes / (1024.0 * 1024.0));
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::max(0.0, megabytes)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    const PrintProcess::Settings s = this->settings(frame, settings);
    alias << TRasterFx::getAlias(frame, settings) << "[printProcess:1:"
          << std::setprecision(std::numeric_limits<double>::max_digits10)
          << s.cellSize << ',' << int(s.shape) << ',' << s.blackGeneration
          << ',' << s.dotGain << ',' << s.softness << ',' << s.paper.r << ','
          << s.paper.g << ',' << s.paper.b << ',' << s.grain << ','
          << s.grainSize << ',' << s.seed << ',' << s.transparentBackground
          << ',' << int(s.output);
    for (const PrintProcess::Plate &p : s.plates)
      alias << ';' << p.enabled << ',' << p.angle << ',' << p.offsetX << ','
            << p.offsetY << ',' << p.ink.r << ',' << p.ink.g << ',' << p.ink.b;
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
    const PrintProcess::Settings s = this->settings(frame, settings);
    const int h                    = PrintProcess::halo(s);
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
      print<TPixel32>(input.getRaster(), output, h, input.m_pos, s,
                      settings.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      print<TPixel64>(input.getRaster(), output, h, input.m_pos, s,
                      settings.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      print<TPixelF>(input.getRaster(), output, h, input.m_pos, s,
                     settings.m_isCanceled);
    else
      throw TException("Print Process: unsupported pixel type");
  }
};
FX_PLUGIN_IDENTIFIER(PrintProcessFx, "printProcessFx")
