#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tfxattributes.h"
#include "celdistance.h"
#include "celfxsourcegrid.h"
#include "tpixelutils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
int gradientChoice(double value, int maximum) {
  if (!std::isfinite(value)) return 0;
  return int(std::floor(std::max(0.0, std::min(double(maximum), value)) + 0.5));
}
double gradientBound(double value, double maximum) {
  return std::isfinite(value) ? std::max(0.0, std::min(maximum, value)) : 0.0;
}
TPixelD gradientColor(TPixelD color) {
  if (!std::isfinite(color.r)) color.r = 0;
  if (!std::isfinite(color.g)) color.g = 0;
  if (!std::isfinite(color.b)) color.b = 0;
  color.m = 1;
  return color;
}
struct GradientSettings {
  int region, mode, interpolation, blur;
  bool background, invert;
  double inside, outside, power, blurSize;
  TPixelD color, bg;
};
bool gradientCanceled(const int *canceled) { return canceled && *canceled; }
bool gradientInvertible(const TAffine &affine) {
  return std::isfinite(affine.det()) && std::abs(affine.det()) >= 1e-12;
}
int gradientBlurSupport(const GradientSettings &p) {
  if (!p.blur || p.blurSize <= 0) return 0;
  const double radius = std::ceil(p.blurSize);
  if (!std::isfinite(radius) ||
      radius > (std::numeric_limits<int>::max() - 1) / 6)
    throw TException("Alpha Distance Gradient: blur exceeds raster dimensions");
  return int(radius);
}
int gradientMargin(const GradientSettings &p) {
  const double distance = p.region == 0 ? 0 : std::ceil(p.outside) + 1;
  const double margin   = distance + gradientBlurSupport(p);
  if (!std::isfinite(margin) ||
      margin > (std::numeric_limits<int>::max() - 1) / 2)
    throw TException(
        "Alpha Distance Gradient: support exceeds raster dimensions");
  return int(margin);
}
bool gradientFiniteBox(const TRectD &box) {
  return box != TConsts::infiniteRectD && std::isfinite(box.x0) &&
         std::isfinite(box.y0) && std::isfinite(box.x1) &&
         std::isfinite(box.y1);
}
TRectD gradientRasterBox(TRectD box) {
  // Canceling DPI/rotation can leave roundoff at an integer pixel boundary.
  auto snap = [](double value) {
    const double nearest   = std::round(value);
    const double tolerance = 64 * std::numeric_limits<double>::epsilon() *
                             std::max(1.0, std::abs(value));
    return std::abs(value - nearest) <= tolerance ? nearest : value;
  };
  return TRectD(std::floor(snap(box.x0)), std::floor(snap(box.y0)),
                std::ceil(snap(box.x1)), std::ceil(snap(box.y1)));
}
TDimension gradientSize(const TRectD &box, int pixelBytes = sizeof(TPixelF)) {
  const double w = box.getLx(), h = box.getLy();
  if (!gradientFiniteBox(box) || w < 1 || h < 1 ||
      w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max() || pixelBytes <= 0 ||
      w * h > std::numeric_limits<int>::max() / pixelBytes)
    throw TException("Alpha Distance Gradient: requested canvas is too large");
  return TDimension(int(w), int(h));
}
// Boundary-adjacent pixels start at zero; subsequent distance remains in
// pixels.
double gradientWeight(double distance, double threshold, int interpolation,
                      double power) {
  if (!std::isfinite(distance)) return 0;
  distance = std::max(0.0, distance - 1.0);
  if (threshold <= 0) return distance == 0 ? 1 : 0;
  if (distance > threshold) return 0;
  const double t = std::min(1.0, distance / threshold);
  switch (interpolation) {
  case 1:
    return std::sqrt(std::max(0.0, 1.0 - t * t));
  case 2:
    return std::pow(1.0 - t, power);
  case 3:
    return 1;
  default:
    return 1.0 - t;
  }
}
// Three separable box convolutions approximate a Gaussian in linear time.
// Zero padding is included in the divisor, preserving the falloff at
// boundaries.
bool gradientBoxPass(const std::vector<double> &input,
                     std::vector<double> &output, int w, int h, int radius,
                     bool vertical, const int *canceled) {
  output.resize(input.size());
  const int lines = vertical ? w : h, length = vertical ? h : w;
  const double divisor = 2.0 * radius + 1;
  for (int line = 0; line < lines; ++line) {
    if (gradientCanceled(canceled)) return false;
    auto index = [=](int position) -> size_t {
      return vertical ? size_t(position) * w + line
                      : size_t(line) * w + position;
    };
    double sum = 0;
    for (int p = 0; p <= std::min(radius, length - 1); ++p)
      sum += input[index(p)];
    for (int p = 0; p < length; ++p) {
      output[index(p)] = std::max(0.0, sum / divisor);
      if (p - radius >= 0) sum -= input[index(p - radius)];
      if (int64_t(p) + radius + 1 < length) sum += input[index(p + radius + 1)];
    }
  }
  return true;
}
bool gradientBlur(std::vector<double> &values, int w, int h, int support,
                  bool preservePeak, const int *canceled) {
  if (!support) return !gradientCanceled(canceled);
  const double originalPeak = *std::max_element(values.begin(), values.end());
  std::vector<double> temporary;
  for (int pass = 0; pass < 3; ++pass) {
    const int radius = (support + pass) / 3;
    if (!gradientBoxPass(values, temporary, w, h, radius, false, canceled) ||
        !gradientBoxPass(temporary, values, w, h, radius, true, canceled))
      return false;
  }
  if (preservePeak) {
    const double blurredPeak = *std::max_element(values.begin(), values.end());
    if (blurredPeak > 0) {
      const double factor = originalPeak / blurredPeak;
      for (size_t i = 0; i < values.size(); ++i) {
        if ((i & 4095) == 0 && gradientCanceled(canceled)) return false;
        values[i] = std::min(1.0, values[i] * factor);
      }
    }
  }
  return !gradientCanceled(canceled);
}
class GradientRasterLock {
  TRasterP m_raster;

public:
  explicit GradientRasterLock(const TRasterP &raster) : m_raster(raster) {
    m_raster->lock();
  }
  ~GradientRasterLock() { m_raster->unlock(); }
};
template <class Pixel>
bool gradientSeed(const Pixel &p) {
  return p.m > 0 && std::isfinite(double(p.m)) && std::isfinite(double(p.r)) &&
         std::isfinite(double(p.g)) && std::isfinite(double(p.b));
}
template <class Pixel>
TPixelD gradientStraight(const Pixel &p) {
  return gradientSeed(p) ? TPixelD(double(p.r) / p.m, double(p.g) / p.m,
                                   double(p.b) / p.m, 1)
                         : TPixelD(0, 0, 0, 1);
}
TPixelD gradientInteger(const TPixelD &p) {
  const double alpha = std::max(0.0, std::min(1.0, p.m));
  return TPixelD(std::max(0.0, std::min(alpha, p.r)),
                 std::max(0.0, std::min(alpha, p.g)),
                 std::max(0.0, std::min(alpha, p.b)), alpha);
}
TPixel32 gradientPixel(const TPixelD &p, TPixel32 *) {
  return toPixel32(gradientInteger(p));
}
TPixel64 gradientPixel(const TPixelD &p, TPixel64 *) {
  return toPixel64(gradientInteger(p));
}
float gradientFloat(double value) {
  const double limit = std::numeric_limits<float>::max();
  return std::isnan(value) ? 0.0f
                           : float(std::max(-limit, std::min(limit, value)));
}
TPixelF gradientPixel(const TPixelD &p, TPixelF *) {
  return TPixelF(gradientFloat(p.r), gradientFloat(p.g), gradientFloat(p.b),
                 float(p.m));
}
template <class Pixel>
void gradientRender(const TRasterPT<Pixel> &source,
                    const TRasterPT<Pixel> &output,
                    const TPointD &sourcePosition,
                    const TPointD &outputPosition, const GradientSettings &p,
                    const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  const size_t area = size_t(w) * h;
  GradientRasterLock sourceLock(source);
  std::vector<uint8_t> mask(area), inverse(area);
  for (int y = 0; y < h; ++y) {
    if (gradientCanceled(canceled)) return;
    const Pixel *row = source->pixels(y);
    for (int x = 0; x < w; ++x) {
      const size_t i = size_t(y) * w + x;
      mask[i]        = gradientSeed(row[x]);
      inverse[i]     = !mask[i];
    }
  }
  CelDistance::Field inside, outside;
  if (!CelDistance::transform(inverse, w, h, CelDistance::Metric::Euclidean,
                              inside, canceled, true) ||
      !CelDistance::transform(mask, w, h, CelDistance::Metric::Euclidean,
                              outside, canceled))
    return;
  std::vector<double> weights(area, 0);
  for (size_t i = 0; i < area; ++i) {
    if ((i & 4095) == 0 && gradientCanceled(canceled)) return;
    if (mask[i] && p.region != 1)
      weights[i] = gradientWeight(inside.distance[i], p.inside, p.interpolation,
                                  p.power);
    else if (!mask[i] && p.region != 0)
      weights[i] = gradientWeight(outside.distance[i], p.outside,
                                  p.interpolation, p.power);
  }
  if (!gradientBlur(weights, w, h, gradientBlurSupport(p), p.blur == 2,
                    canceled))
    return;
  GradientRasterLock outputLock(output);
  const int64_t offsetX =
      int64_t(std::llround(outputPosition.x - sourcePosition.x));
  const int64_t offsetY =
      int64_t(std::llround(outputPosition.y - sourcePosition.y));
  for (int y = 0; y < output->getLy(); ++y) {
    if (gradientCanceled(canceled)) {
      output->clear();
      return;
    }
    Pixel *row       = output->pixels(y);
    const int64_t sy = y + offsetY;
    if (sy < 0 || sy >= h) continue;
    for (int x = 0; x < output->getLx(); ++x) {
      const int64_t sx = x + offsetX;
      if (sx < 0 || sx >= w) continue;
      const size_t i = size_t(sy) * w + size_t(sx);
      double weight  = std::max(0.0, std::min(1.0, weights[i]));
      if (p.invert) weight = 1.0 - weight;
      TPixelD color = p.color;
      if (p.mode == 1) {
        const int nearest = outside.nearest[i];
        color =
            nearest < 0
                ? TPixelD(0, 0, 0, 1)
                : gradientStraight(source->pixels(nearest / w)[nearest % w]);
      }
      const double background = p.background ? 1 - weight : 0;
      const double alpha      = p.background ? 1 : weight;
      const TPixelD value(color.r * weight + p.bg.r * background,
                          color.g * weight + p.bg.g * background,
                          color.b * weight + p.bg.b * background, alpha);
      row[x] = gradientPixel(value, static_cast<Pixel *>(nullptr));
    }
  }
}
}  // namespace

class AlphaDistanceGradientFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(AlphaDistanceGradientFx)
  TRasterFxPort m_source;
  TDoubleParamP m_region, m_inside, m_outside, m_mode, m_background, m_invert;
  TDoubleParamP m_interpolation, m_power, m_blur, m_blurSize;
  TPixelParamP m_color, m_bg;
  GradientSettings parameters(double frame, double scale = 1) const {
    return {gradientChoice(m_region->getValue(frame), 2),
            gradientChoice(m_mode->getValue(frame), 1),
            gradientChoice(m_interpolation->getValue(frame), 3),
            gradientChoice(m_blur->getValue(frame), 2),
            gradientChoice(m_background->getValue(frame), 1) != 0,
            gradientChoice(m_invert->getValue(frame), 1) != 0,
            gradientBound(m_inside->getValue(frame), 1000) * scale,
            gradientBound(m_outside->getValue(frame), 1000) * scale,
            std::max(0.01, gradientBound(m_power->getValue(frame), 5)),
            gradientBound(m_blurSize->getValue(frame), 4096) * scale,
            gradientColor(m_color->getValueD(frame)),
            gradientColor(m_bg->getValueD(frame))};
  }
  bool usesCameraDomain(double frame, const TRenderSettings &settings) {
    if (!m_source.isConnected()) return false;
    TRenderSettings full(settings);
    full.m_getFullSizeBBox = true;
    TRectD box;
    m_source->doGetBBox(frame, box, full);
    return !gradientFiniteBox(box);
  }
  double pixelScale(double frame, const TRenderSettings &settings) {
    TAffine drawing = settings.m_affine;
    if (!usesCameraDomain(frame, settings))
      drawing =
          drawing * CelFxSourceGrid::nativeToWorld(m_source.getFx(), frame);
    const double scale = std::sqrt(std::abs(drawing.det()));
    return std::abs(scale - 1) < 1e-10 ? 1 : scale;
  }
  // The host moves integer affine translations and fractional tile positions
  // into the working tile origin. A camera rectangle must move with that grid.
  TPointD cameraCoordinates(const TPointD &position,
                            const TRenderSettings &settings,
                            TRenderSettings &normalized) const {
    const TPointD integral(std::floor(position.x), std::floor(position.y));
    const TPointD fraction = position - integral;
    const TPointD translation(std::floor(settings.m_affine.a13 - fraction.x),
                              std::floor(settings.m_affine.a23 - fraction.y));
    const TPointD origin = integral - translation;
    const TPointD delta  = position - origin;
    normalized           = settings;
    normalized.m_affine.a13 -= delta.x;
    normalized.m_affine.a23 -= delta.y;
    normalized.m_cameraBox -= delta;
    return origin;
  }
  bool sourceCanvas(double frame, const TRenderSettings &settings, TRectD &box,
                    bool transformed, bool *cameraDomain = nullptr) {
    if (cameraDomain) *cameraDomain = false;
    TRenderSettings full(settings);
    full.m_getFullSizeBBox = true;
    bool result            = transformed ? m_source->getBBox(frame, box, full)
                                         : m_source->doGetBBox(frame, box, full);
    if (!gradientFiniteBox(box)) {
      if (cameraDomain) *cameraDomain = true;
      box = settings.m_cameraBox;
      if (!gradientFiniteBox(box) || box.isEmpty())
        throw TException(
            "Alpha Distance Gradient: a finite source or camera canvas is "
            "required");
      result = true;
    } else if (result && !box.isEmpty()) {
      const auto grid = CelFxSourceGrid::discover(m_source.getFx(), frame);
      if (grid.hasCanvas)
        box = (transformed ? settings.m_affine * grid.placement
                           : grid.placement) *
              grid.canvas;
    }
    return result && !box.isEmpty();
  }
  TRectD analysisBox(double frame, const TRenderSettings &settings,
                     TRectD &canvas) {
    if (!sourceCanvas(frame, settings, canvas, true)) return TRectD();
    canvas = gradientRasterBox(canvas);
    return canvas.enlarge(
        gradientMargin(parameters(frame, pixelScale(frame, settings))));
  }

public:
  AlphaDistanceGradientFx()
      : m_region(0.0)
      , m_inside(128.0)
      , m_outside(128.0)
      , m_mode(0.0)
      , m_background(0.0)
      , m_invert(0.0)
      , m_interpolation(0.0)
      , m_power(1.0)
      , m_blur(0.0)
      , m_blurSize(0.0)
      , m_color(TPixel32::White)
      , m_bg(TPixel32::Black) {
    addInputPort("Source", m_source);
    bindParam(this, "region", m_region);
    bindParam(this, "insideThreshold", m_inside);
    bindParam(this, "outsideThreshold", m_outside);
    bindParam(this, "renderMode", m_mode);
    bindParam(this, "useBackground", m_background);
    bindParam(this, "gradationColor", m_color);
    bindParam(this, "backgroundColor", m_bg);
    bindParam(this, "invert", m_invert);
    bindParam(this, "interpolation", m_interpolation);
    bindParam(this, "power", m_power);
    bindParam(this, "blurMode", m_blur);
    bindParam(this, "blurSize", m_blurSize);
    m_region->setValueRange(0, 2);
    m_region->setUIChoices({"Inside", "Outside", "Both"});
    m_mode->setValueRange(0, 1);
    m_mode->setUIChoices({"RGB", "Layer"});
    m_background->setValueRange(0, 1);
    m_background->setUIChoices({"Off", "On"}, true);
    m_invert->setValueRange(0, 1);
    m_invert->setUIChoices({"Off", "On"}, true);
    m_interpolation->setValueRange(0, 3);
    m_interpolation->setUIChoices({"Linear", "Sphere", "Power", "Constant"});
    m_blur->setValueRange(0, 2);
    m_blur->setUIChoices({"No Blur", "Blur No Scale", "Blur"});
    m_inside->setValueRange(0, 1000);
    m_outside->setValueRange(0, 1000);
    m_power->setValueRange(0.01, 5);
    m_blurSize->setValueRange(0, 4096);
    // No unit: the thresholds and the blur are in the level's own pixels (not
    // the length unit of the user, which the fields would convert them to)
    m_color->enableMatte(false);
    m_bg->enableMatte(false);
    enableComputeInFloat(true);
  }
  bool usesCustomRenderGrid() const override { return true; }
  bool canHandle(const TRenderSettings &settings, double frame) override {
    return gradientInvertible(settings.m_affine) &&
           usesCameraDomain(frame, settings);
  }
  TAffine handledAffine(const TRenderSettings &settings,
                        double frame) override {
    // Finite sources use one native grid. Infinite sources use the camera's
    // render grid; its rectangle is already expressed in output coordinates.
    return gradientInvertible(settings.m_affine) &&
                   usesCameraDomain(frame, settings)
               ? settings.m_affine
               : CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  }
  void compute(TTile &tile, double frame,
               const TRenderSettings &settings) override {
    if (!getAttributes()->isEnabled() || !usesCameraDomain(frame, settings)) {
      TRasterFx::compute(tile, frame, settings);
      return;
    }
    TRenderSettings normalized;
    const TPointD original = tile.m_pos;
    tile.m_pos             = cameraCoordinates(original, settings, normalized);
    try {
      TRasterFx::compute(tile, frame, normalized);
    } catch (...) {
      tile.m_pos = original;
      throw;
    }
    tile.m_pos = original;
  }
  void dryCompute(TRectD &rect, double frame,
                  const TRenderSettings &settings) override {
    if (!getAttributes()->isEnabled() || !usesCameraDomain(frame, settings)) {
      TRasterFx::dryCompute(rect, frame, settings);
      return;
    }
    TRenderSettings normalized;
    const TPointD origin =
        cameraCoordinates(rect.getP00(), settings, normalized);
    TRectD normalizedRect(origin, rect.getSize());
    TRasterFx::dryCompute(normalizedRect, frame, normalized);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override {
    box               = TRectD();
    bool cameraDomain = false;
    if (!m_source.isConnected() ||
        !sourceCanvas(frame, settings, box, false, &cameraDomain))
      return false;
    if (cameraDomain) {
      if (!gradientInvertible(settings.m_affine)) {
        box = TRectD();
        return false;
      }
      box = TRectD(std::floor(box.x0), std::floor(box.y0), std::ceil(box.x1),
                   std::ceil(box.y1));
      box = box.enlarge(gradientMargin(
          parameters(frame, std::sqrt(std::abs(settings.m_affine.det())))));
      // getBBox applies the affine after this callback. Return native bounds
      // that contain the already-transformed camera, without scaling it twice.
      box = settings.m_affine.inv() * box;
    } else {
      const auto grid  = CelFxSourceGrid::discover(m_source.getFx(), frame);
      const int margin = gradientMargin(parameters(frame));
      box = grid.hasCanvas ? grid.placement * grid.canvas.enlarge(margin)
                           : box.enlarge(margin);
    }
    return true;
  }
  void transform(double frame, int, const TRectD &,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override {
    inputSettings = settings;
    input         = TRectD();
    if (m_source.isConnected()) sourceCanvas(frame, settings, input, true);
    if (!input.isEmpty()) input = gradientRasterBox(input);
  }
  void doDryCompute(TRectD &, double frame,
                    const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return;
    TRectD canvas;
    TRenderSettings inputSettings;
    transform(frame, 0, TRectD(), settings, canvas, inputSettings);
    if (!canvas.isEmpty()) m_source->dryCompute(canvas, frame, inputSettings);
  }
  int getMemoryRequirement(const TRectD &, double frame,
                           const TRenderSettings &settings) override {
    if (!m_source.isConnected()) return 0;
    TRectD canvas;
    const TRectD box   = analysisBox(frame, settings, canvas);
    const double bytes = box.getLx() * box.getLy() * 96;
    // In MB, as the tile subdivision compares it with its limit in MB
    const double megabytes = std::ceil(bytes / (1024.0 * 1024.0));
    return int(std::min(double(std::numeric_limits<int>::max()),
                        std::max(0.0, megabytes)));
  }
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override {
    const GradientSettings p = parameters(frame);
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << TRasterFx::getAlias(frame, settings) << "[alphaDistance:3:"
      << std::setprecision(std::numeric_limits<double>::max_digits10)
      << p.region << ',' << p.mode << ',' << p.interpolation << ',' << p.blur
      << ',' << p.background << ',' << p.invert << ',' << p.inside << ','
      << p.outside << ',' << p.power << ',' << p.blurSize << ',' << p.color.r
      << ',' << p.color.g << ',' << p.color.b << ',' << p.bg.r << ',' << p.bg.g
      << ',' << p.bg.b << ',' << settings.m_cameraBox.x0 << ','
      << settings.m_cameraBox.y0 << ',' << settings.m_cameraBox.x1 << ','
      << settings.m_cameraBox.y1;
    const auto grid = CelFxSourceGrid::discover(m_source.getFx(), frame);
    for (double value :
         {grid.placement.a11, grid.placement.a12, grid.placement.a13,
          grid.placement.a21, grid.placement.a22, grid.placement.a23,
          grid.phase.x, grid.phase.y, grid.canvas.x0, grid.canvas.y0,
          grid.canvas.x1, grid.canvas.y1})
      s << ',' << value;
    s << ',' << grid.hasCanvas << ']';
    return s.str();
  }
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override {
    tile.getRaster()->clear();
    if (!m_source.isConnected() || gradientCanceled(settings.m_isCanceled))
      return;
    TRectD canvas;
    const TRectD box = analysisBox(frame, settings, canvas);
    if (box.isEmpty()) return;
    const TRectD requested(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                                   tile.getRaster()->getLy()));
    if ((requested * box).isEmpty()) return;
    const int bytes             = tile.getRaster()->getPixelSize();
    const TDimension canvasSize = gradientSize(canvas, bytes),
                     size       = gradientSize(box, bytes);
    TTile source;
    source.setRaster(tile.getRaster()->create(canvasSize.lx, canvasSize.ly));
    source.m_pos = canvas.getP00();
    source.getRaster()->clear();
    m_source->compute(source, frame, settings);
    if (gradientCanceled(settings.m_isCanceled)) return;
    TRasterP padded = tile.getRaster()->create(size.lx, size.ly);
    padded->clear();
    padded->copy(source.getRaster(),
                 TPoint(int(canvas.x0 - box.x0), int(canvas.y0 - box.y0)));
    const GradientSettings p = parameters(frame, pixelScale(frame, settings));
    if (TRaster32P output = tile.getRaster())
      gradientRender<TPixel32>(padded, output, box.getP00(), tile.m_pos, p,
                               settings.m_isCanceled);
    else if (TRaster64P output = tile.getRaster())
      gradientRender<TPixel64>(padded, output, box.getP00(), tile.m_pos, p,
                               settings.m_isCanceled);
    else if (TRasterFP output = tile.getRaster())
      gradientRender<TPixelF>(padded, output, box.getP00(), tile.m_pos, p,
                              settings.m_isCanceled);
    else
      throw TException("Alpha Distance Gradient: unsupported pixel type");
  }
};
FX_PLUGIN_IDENTIFIER(AlphaDistanceGradientFx, "alphaDistanceGradientFx")
