#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"
#include "celedgelines.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>

// Cel Edge Lines: lines along the boundaries between flat colour regions,
// with rules choosing which pairs of regions get a line, and its weight and
// colour. The math is in celedgelines.h.
namespace {
constexpr int ruleCapacity = 8;

std::string ruleName(const char *prefix, int index) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << prefix << std::setw(3) << std::setfill('0') << index + 1;
  return out.str();
}
double bounded(double value, double minimum, double maximum) {
  return std::isfinite(value) ? std::max(minimum, std::min(maximum, value))
                              : minimum;
}
int discrete(double value, int maximum) {
  return int(std::floor(bounded(value, 0, maximum) + 0.5));
}
CelEdge::Kind kindOf(double value) {
  switch (discrete(value, 2)) {
  case 1:
    return CelEdge::Kind::Color;
  case 2:
    return CelEdge::Kind::Transparent;
  default:
    return CelEdge::Kind::Any;
  }
}
TDimension paddedSize(int width, int height, int halo, int pixelBytes) {
  const int64_t w = int64_t(width) + 2LL * halo;
  const int64_t h = int64_t(height) + 2LL * halo;
  if (w <= 0 || h <= 0 || w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max() || pixelBytes <= 0 ||
      w * h > std::numeric_limits<int>::max() / pixelBytes)
    throw TException("Cel Edge Lines: requested raster is too large");
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
void drawLines(const TRasterPT<Pixel> &source, const TRasterPT<Pixel> &output,
               int halo, const CelEdge::Settings &settings,
               const std::vector<CelEdge::Rule> &rules, const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  std::vector<CelEdge::Color> image(CelDistance::checkedArea(w, h));
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
  std::vector<CelEdge::Color> result;
  if (!CelEdge::render(image, w, h, settings, rules, result, canceled)) {
    output->clear();
    return;
  }
  RasterLock lock(output);
  for (int y = 0; y < output->getLy(); ++y) {
    Pixel *row = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      const CelEdge::Color &c = result[size_t(y + halo) * w + x + halo];
      row[x] = PixelConverter<Pixel>::from(TPixelD(c.r, c.g, c.b, c.a));
    }
  }
}
}  // namespace

class CelEdgeLinesFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelEdgeLinesFx)
  TRasterFxPort m_source;
  TDoubleParamP m_mode, m_tolerance, m_matchTolerance, m_gap, m_opacity,
      m_enabledRules;
  struct Rule {
    TDoubleParamP use, firstKind, secondKind, weight;
    TPixelParamP firstColor, secondColor, lineColor;
    Rule()
        : use(0.0)
        , firstKind(0.0)
        , secondKind(0.0)
        , weight(2.0)
        , firstColor(TPixel32::White)
        , secondColor(TPixel32::White)
        , lineColor(TPixel32::Black) {}
  };
  std::array<Rule, ruleCapacity> m_rules;

  // Line weights and the ramp gap are in the level's own pixels
  double pixelScale(double frame, const TRenderSettings &settings) const {
    const TAffine drawing = settings.m_affine * CelFxSourceGrid::nativeToWorld(
                                                    m_source.getFx(), frame);
    const double scale = std::sqrt(std::abs(drawing.det()));
    return std::isfinite(scale) && std::abs(scale - 1) >= 1e-10 && scale > 0
               ? scale
               : 1.0;
  }
  CelEdge::Settings settings(double frame,
                             const TRenderSettings &render) const {
    CelEdge::Settings s;
    s.tolerance      = bounded(m_tolerance->getValue(frame), 0, 1);
    s.matchTolerance = bounded(m_matchTolerance->getValue(frame), 0, 1);
    s.gap = bounded(m_gap->getValue(frame), 0, 64) * pixelScale(frame, render);
    s.opacity   = bounded(m_opacity->getValue(frame), 0, 1);
    s.linesOnly = discrete(m_mode->getValue(frame), 1) == 1;
    return s;
  }
  std::vector<CelEdge::Rule> rules(double frame,
                                   const TRenderSettings &render) const {
    const double scale = pixelScale(frame, render);
    const int count = discrete(m_enabledRules->getValue(frame), ruleCapacity);
    std::vector<CelEdge::Rule> out;
    for (int i = 0; i < count; ++i) {
      const Rule &r = m_rules[i];
      CelEdge::Rule rule;
      rule.enabled        = discrete(r.use->getValue(frame), 1) == 1;
      rule.weight         = bounded(r.weight->getValue(frame), 0, 1000) * scale;
      const TPixelD first = r.firstColor->getValueD(frame);
      const TPixelD second = r.secondColor->getValueD(frame);
      const TPixelD line   = r.lineColor->getValueD(frame);
      rule.first  = {kindOf(r.firstKind->getValue(frame)), first.r, first.g,
                     first.b};
      rule.second = {kindOf(r.secondKind->getValue(frame)), second.r, second.g,
                     second.b};
      rule.line   = {line.r, line.g, line.b, line.m};
      out.push_back(rule);
    }
    return out;
  }
  bool drawsNothing(double frame, const TRenderSettings &render) const {
    return CelEdge::maxWeight(rules(frame, render)) <= 0.0 ||
           bounded(m_opacity->getValue(frame), 0, 1) <= 0.0;
  }
  int halo(double frame, const TRenderSettings &render) const {
    return CelEdge::halo(settings(frame, render), rules(frame, render));
  }

public:
  CelEdgeLinesFx()
      : m_mode(0.0)
      , m_tolerance(0.03)
      , m_matchTolerance(0.1)
      , m_gap(2.0)
      , m_opacity(1.0)
      , m_enabledRules(2.0) {
    addInputPort("Source", m_source);
    auto flag = [](const TDoubleParamP &param) {
      param->setValueRange(0, 1);
      param->setUIChoices({"Off", "On"}, true);
    };
    auto choice = [](const TDoubleParamP &param,
                     const std::vector<std::string> &options) {
      param->setValueRange(0, double(options.size() - 1));
      param->setUIChoices(options);
    };
    choice(m_mode, {"Lines over Source", "Lines Only"});
    m_tolerance->setValueRange(0, 1);
    m_matchTolerance->setValueRange(0, 1);
    m_gap->setValueRange(0, 64);
    m_gap->setUISliderRange(0, 8);
    m_opacity->setValueRange(0, 1);
    m_enabledRules->setValueRange(0, ruleCapacity);
    bindParam(this, "mode", m_mode);
    bindParam(this, "tolerance", m_tolerance);
    bindParam(this, "matchTolerance", m_matchTolerance);
    bindParam(this, "gap", m_gap);
    bindParam(this, "opacity", m_opacity);
    bindParam(this, "enabledRules", m_enabledRules);
    for (int i = 0; i < ruleCapacity; ++i) {
      Rule &rule = m_rules[i];
      flag(rule.use);
      choice(rule.firstKind, {"Any Colour", "Colour", "Transparent"});
      choice(rule.secondKind, {"Any Colour", "Colour", "Transparent"});
      rule.weight->setValueRange(0, 1000);
      rule.weight->setUISliderRange(0, 20);
      rule.firstColor->enableMatte(false);
      rule.secondColor->enableMatte(false);
      bindParam(this, ruleName("use", i), rule.use);
      bindParam(this, ruleName("firstKind", i), rule.firstKind);
      bindParam(this, ruleName("firstColor", i), rule.firstColor);
      bindParam(this, ruleName("secondKind", i), rule.secondKind);
      bindParam(this, ruleName("secondColor", i), rule.secondColor);
      bindParam(this, ruleName("weight", i), rule.weight);
      bindParam(this, ruleName("lineColor", i), rule.lineColor);
    }
    // By default, a line between any two colours and around the drawing
    m_rules[0].use->setDefaultValue(1.0);
    m_rules[1].use->setDefaultValue(1.0);
    m_rules[1].secondKind->setDefaultValue(2.0);
    enableComputeInFloat(true);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &settings, double frame) override {
    return drawsNothing(frame, settings);
  }
  TAffine handledAffine(const TRenderSettings &settings,
                        double frame) override {
    return drawsNothing(frame, settings)
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
    // Lines around the drawing reach past it by half their weight
    if (!box.isEmpty() && box != TConsts::infiniteRectD) {
      const TAffine placement =
          CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
      double weight   = 0.0;  // in the level's pixels, which placement scales
      const int count = discrete(m_enabledRules->getValue(frame), ruleCapacity);
      for (int i = 0; i < count; ++i)
        if (discrete(m_rules[i].use->getValue(frame), 1) == 1)
          weight = std::max(
              weight, bounded(m_rules[i].weight->getValue(frame), 0, 1000));
      const double reach = weight / 2.0 + 1.0;
      const double dx    = reach * std::hypot(placement.a11, placement.a12);
      const double dy    = reach * std::hypot(placement.a21, placement.a22);
      box = TRectD(box.x0 - dx, box.y0 - dy, box.x1 + dx, box.y1 + dy);
    }
    return !box.isEmpty();
  }
  void transform(double frame, int, const TRectD &output,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override {
    inputSettings = settings;
    input         = drawsNothing(frame, settings)
                        ? output
                        : output.enlarge(halo(frame, settings));
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD input = drawsNothing(frame, settings)
                       ? rect
                       : rect.enlarge(halo(frame, settings));
    m_source->dryCompute(input, frame, settings);
  }
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override {
    if (drawsNothing(frame, settings)) return 0;
    const double h = halo(frame, settings);
    // Source and result colours, two masks, two distance fields and labels
    const double bytes = (rect.getLx() + 2 * h) * (rect.getLy() + 2 * h) * 160;
    // In MB, as the tile subdivision compares it with its limit in MB
    const double megabytes = std::ceil(bytes / (1024.0 * 1024.0));
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::max(0.0, megabytes)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    std::ostringstream alias;
    alias.imbue(std::locale::classic());
    alias << TRasterFx::getAlias(frame, settings) << "[celEdgeLines:1:"
          << std::setprecision(std::numeric_limits<double>::max_digits10);
    const CelEdge::Settings s = this->settings(frame, settings);
    alias << s.tolerance << ',' << s.matchTolerance << ',' << s.gap << ','
          << s.opacity << ',' << s.linesOnly;
    for (const CelEdge::Rule &r : rules(frame, settings))
      alias << ';' << r.enabled << ',' << int(r.first.kind) << ',' << r.first.r
            << ',' << r.first.g << ',' << r.first.b << ',' << int(r.second.kind)
            << ',' << r.second.r << ',' << r.second.g << ',' << r.second.b
            << ',' << r.weight << ',' << r.line.r << ',' << r.line.g << ','
            << r.line.b << ',' << r.line.a;
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
    const CelEdge::Settings s = this->settings(frame, settings);
    if (drawsNothing(frame, settings)) {
      if (!s.linesOnly) m_source->compute(tile, frame, settings);
      if (settings.m_isCanceled && *settings.m_isCanceled)
        tile.getRaster()->clear();
      return;
    }
    const std::vector<CelEdge::Rule> r = rules(frame, settings);
    const int h                        = CelEdge::halo(s, r);
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
      drawLines<TPixel32>(input.getRaster(), output, h, s, r,
                          settings.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      drawLines<TPixel64>(input.getRaster(), output, h, s, r,
                          settings.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      drawLines<TPixelF>(input.getRaster(), output, h, s, r,
                         settings.m_isCanceled);
    else
      throw TException("Cel Edge Lines: unsupported pixel type");
  }
};
FX_PLUGIN_IDENTIFIER(CelEdgeLinesFx, "celEdgeLinesFx")
