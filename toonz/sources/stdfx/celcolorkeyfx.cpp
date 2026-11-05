#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"
#include "celcolorkeymath.h"
#include "celdistance.h"
#include "celfxsourcegrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <type_traits>
#include <vector>

namespace {
constexpr int colorCapacity = 25;
std::string slotName(const char *prefix, int index) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << prefix << std::setw(3) << std::setfill('0') << index + 1;
  return out.str();
}
int discrete(double value, int maximum) {
  return std::isfinite(value)
             ? int(std::floor(std::max(0.0, std::min(double(maximum), value)) +
                              0.5))
             : 0;
}
double bounded(double value, double minimum, double maximum) {
  return std::isfinite(value) ? std::max(minimum, std::min(maximum, value))
                              : 0.0;
}
void checkInputSize(int width, int height, int pixelSize) {
  const size_t area = CelDistance::checkedArea(width, height);
  // The host raster allocator multiplies dimensions in signed int.
  if (pixelSize <= 0 ||
      area > size_t(std::numeric_limits<int>::max()) / size_t(pixelSize))
    throw TException(
        "Cel Color Key: source tile exceeds the raster size limit");
}
}  // namespace

class CelColorKeyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(CelColorKeyFx)
  TRasterFxPort m_source;
  TDoubleParamP m_keep, m_threshold, m_premultiplied, m_colorSpace, m_precision;
  TDoubleParamP m_perColor, m_perComponent, m_component1, m_component2,
      m_component3;
  TDoubleParamP m_thin, m_thinMetric, m_blur, m_blurMetric, m_blurDirection;
  TDoubleParamP m_enabledColors, m_enableReplace;
  struct Slot {
    TDoubleParamP use, replace, threshold, component1, component2, component3;
    TPixelParamP key, replacement;
    Slot()
        : use(0.0)
        , replace(0.0)
        , threshold(0.0)
        , component1(0.0)
        , component2(0.0)
        , component3(0.0)
        , key(TPixel32::Black)
        , replacement(TPixel32::Black) {}
  };
  std::array<Slot, colorCapacity> m_slots;

public:
  CelColorKeyFx()
      : m_keep(0.0)
      , m_threshold(0.0)
      , m_premultiplied(0.0)
      , m_colorSpace(0.0)
      , m_precision(0.0)
      , m_perColor(0.0)
      , m_perComponent(0.0)
      , m_component1(0.0)
      , m_component2(0.0)
      , m_component3(0.0)
      , m_thin(0.0)
      , m_thinMetric(0.0)
      , m_blur(0.0)
      , m_blurMetric(0.0)
      , m_blurDirection(1.0)
      , m_enabledColors(1.0)
      , m_enableReplace(0.0) {
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
    auto threshold = [](const TDoubleParamP &param) {
      param->setValueRange(0, 1);
    };
    flag(m_keep);
    flag(m_premultiplied);
    flag(m_perColor);
    flag(m_perComponent);
    flag(m_enableReplace);
    choice(m_colorSpace, {"RGB", "HSV", "Lab76", "Lab94", "YUV", "YCrCb"});
    choice(m_precision, {"Full", "16 bit", "8 bit"});
    choice(m_thinMetric, {"Box", "Approximate", "Euclidean"});
    choice(m_blurMetric, {"Box", "Approximate", "Euclidean"});
    choice(m_blurDirection, {"Inside", "Around", "Outside"});
    threshold(m_threshold);
    threshold(m_component1);
    threshold(m_component2);
    threshold(m_component3);
    m_thin->setValueRange(-4000, 4000);
    m_blur->setValueRange(0, 4000);
    m_enabledColors->setValueRange(0, colorCapacity);
    m_thin->setUISliderRange(-100, 100);
    m_blur->setUISliderRange(0, 100);
    bindParam(this, "keep", m_keep);
    bindParam(this, "threshold", m_threshold);
    bindParam(this, "premultiplied", m_premultiplied);
    bindParam(this, "colorSpace", m_colorSpace);
    bindParam(this, "precision", m_precision);
    bindParam(this, "perColor", m_perColor);
    bindParam(this, "perComponent", m_perComponent);
    bindParam(this, "component1", m_component1);
    bindParam(this, "component2", m_component2);
    bindParam(this, "component3", m_component3);
    bindParam(this, "thin", m_thin);
    bindParam(this, "thinMetric", m_thinMetric);
    bindParam(this, "blur", m_blur);
    bindParam(this, "blurMetric", m_blurMetric);
    bindParam(this, "blurDirection", m_blurDirection);
    bindParam(this, "enabledColors", m_enabledColors);
    bindParam(this, "enableReplace", m_enableReplace);
    for (int i = 0; i < colorCapacity; ++i) {
      Slot &slot = m_slots[i];
      flag(slot.use);
      flag(slot.replace);
      threshold(slot.threshold);
      threshold(slot.component1);
      threshold(slot.component2);
      threshold(slot.component3);
      slot.key->enableMatte(false);
      slot.replacement->enableMatte(false);
      bindParam(this, slotName("use", i), slot.use);
      bindParam(this, slotName("key", i), slot.key);
      bindParam(this, slotName("replace", i), slot.replace);
      bindParam(this, slotName("replacement", i), slot.replacement);
      bindParam(this, slotName("threshold", i), slot.threshold);
      bindParam(this, slotName("component1", i), slot.component1);
      bindParam(this, slotName("component2", i), slot.component2);
      bindParam(this, slotName("component3", i), slot.component3);
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
  TRectD inputRect(double frame, const TRectD &output,
                   const TRenderSettings &settings);
  bool doGetBBox(double frame, TRectD &box,
                 const TRenderSettings &settings) override;
  void transform(double frame, int port, const TRectD &output,
                 const TRenderSettings &settings, TRectD &input,
                 TRenderSettings &inputSettings) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &settings) override;
  std::string getAlias(double frame,
                       const TRenderSettings &settings) const override;
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;
};

TRectD CelColorKeyFx::inputRect(double frame, const TRectD &output,
                                const TRenderSettings &settings) {
  if (!m_source.isConnected() || output.isEmpty()) return TRectD();
  TRectD sourceBox;
  if (!m_source->getBBox(frame, sourceBox, settings)) return TRectD();
  const double radius =
      std::ceil(std::abs(bounded(m_thin->getValue(frame), -4000, 4000)) +
                bounded(m_blur->getValue(frame), 0, 4000)) +
      2;
  const TRectD clipped = output.enlarge(radius) * sourceBox.enlarge(1.0);
  if (clipped.isEmpty()) return TRectD();
  // Align requests with the output's pixel grid, including fractional origins.
  return TRectD(output.x0 + std::floor(clipped.x0 - output.x0),
                output.y0 + std::floor(clipped.y0 - output.y0),
                output.x0 + std::ceil(clipped.x1 - output.x0),
                output.y0 + std::ceil(clipped.y1 - output.y0));
}

bool CelColorKeyFx::doGetBBox(double frame, TRectD &box,
                              const TRenderSettings &settings) {
  if (!m_source.isConnected()) {
    box = TRectD();
    return false;
  }
  return m_source->doGetBBox(frame, box, settings);
}

void CelColorKeyFx::transform(double frame, int, const TRectD &output,
                              const TRenderSettings &settings, TRectD &input,
                              TRenderSettings &inputSettings) {
  inputSettings = settings;
  input         = inputRect(frame, output, settings);
}

int CelColorKeyFx::getMemoryRequirement(const TRectD &rect, double frame,
                                        const TRenderSettings &settings) {
  // Source, masks, distance fields, nearest labels and row scratch. Compute
  // in wide floating point because unsigned long is only 32 bits on Windows.
  const TRectD input = inputRect(frame, rect, settings);
  if (input.isEmpty()) return 0;
  const long double megabytes = (static_cast<long double>(input.getLx()) + 1) *
                                (static_cast<long double>(input.getLy()) + 1) *
                                96 / (1024 * 1024);
  if (!std::isfinite(megabytes) || megabytes >= std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  return int(std::ceil(megabytes));
}

std::string CelColorKeyFx::getAlias(double frame,
                                    const TRenderSettings &settings) const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << TRasterFx::getAlias(frame, settings) << "[celKey:"
      << std::setprecision(std::numeric_limits<double>::max_digits10);
  const TAffine grid = CelFxSourceGrid::renderAffine(m_source.getFx(), frame);
  for (double component :
       {grid.a11, grid.a12, grid.a13, grid.a21, grid.a22, grid.a23})
    out << ';' << component;
  auto value = [&](const TDoubleParamP &p) {
    out << ';' << p->getValue(frame);
  };
  for (const TDoubleParamP &p :
       {m_keep, m_threshold, m_premultiplied, m_colorSpace, m_precision,
        m_perColor, m_perComponent, m_component1, m_component2, m_component3,
        m_thin, m_thinMetric, m_blur, m_blurMetric, m_blurDirection,
        m_enabledColors, m_enableReplace})
    value(p);
  for (const Slot &slot : m_slots) {
    for (const TDoubleParamP &p :
         {slot.use, slot.replace, slot.threshold, slot.component1,
          slot.component2, slot.component3})
      value(p);
    for (const TPixelParamP &p : {slot.key, slot.replacement}) {
      const TPixelD c = p->getValueD(frame);
      out << ';' << c.r << ',' << c.g << ',' << c.b;
    }
  }
  return out.str() + ']';
}

namespace {
struct RasterLock {
  TRasterP raster;
  explicit RasterLock(const TRasterP &value) : raster(value) { raster->lock(); }
  ~RasterLock() { raster->unlock(); }
};

template <class Pixel>
bool applyKey(const TRasterPT<Pixel> &source, const TRasterPT<Pixel> &output,
              int offsetX, int offsetY,
              const std::array<CelKey::Entry, colorCapacity> &entries,
              int count, CelKey::Settings settings, bool keep, bool replace,
              double thinAmount, CelDistance::Metric thinMetric,
              double blurAmount, CelDistance::Metric blurMetric,
              CelDistance::Direction direction, const int *canceled) {
  settings.sourceBits = std::is_same<Pixel, TPixel32>::value   ? 8
                        : std::is_same<Pixel, TPixel64>::value ? 16
                                                               : 32;
  const CelKey::Matcher matcher(entries.data(), size_t(count), settings);
  const int width = source->getLx(), height = source->getLy();
  const size_t area = CelDistance::checkedArea(width, height);
  std::vector<std::uint8_t> mask(area, 0), adjusted;
  std::vector<int> match(area, -1);
  RasterLock sourceLock(source), outputLock(output);
  for (int y = 0; y < height; ++y) {
    if (canceled && *canceled) return false;
    const Pixel *row = source->pixels(y);
    for (int x = 0; x < width; ++x) {
      const TPixelD pixel = toPixelD(row[x]);
      const int found =
          matcher.firstMatch({{pixel.r, pixel.g, pixel.b}}, pixel.m);
      const size_t index = size_t(y) * size_t(width) + size_t(x);
      match[index]       = found;
      mask[index]        = found >= 0;
    }
  }
  if (!CelDistance::thin(mask, width, height, thinAmount, thinMetric, adjusted,
                         canceled))
    return false;
  std::vector<double> coverage;
  if (!CelDistance::feather(adjusted, width, height, blurAmount, blurMetric,
                            direction, coverage, canceled))
    return false;
  for (int y = 0; y < output->getLy(); ++y) {
    if (canceled && *canceled) return false;
    Pixel *dest  = output->pixels(y);
    const int sy = y + offsetY;
    if (sy < 0 || sy >= height) continue;
    const Pixel *src = source->pixels(sy);
    for (int x = 0; x < output->getLx(); ++x) {
      const int sx = x + offsetX;
      if (sx < 0 || sx >= width) continue;
      const size_t index       = size_t(sy) * size_t(width) + size_t(sx);
      const double selected    = std::max(0.0, std::min(1.0, coverage[index]));
      const double alphaFactor = keep ? selected : 1.0 - selected;
      if (alphaFactor <= 0) {
        dest[x] = Pixel::Transparent;
        continue;
      }
      const int selectedIndex = match[index];
      TPixelD pixel           = toPixelD(src[sx]);
      if (!CelKey::validPixel({{pixel.r, pixel.g, pixel.b}}, pixel.m)) {
        dest[x] = Pixel::Transparent;
        continue;
      }
      if (alphaFactor == 1.0 && !(keep && replace && selectedIndex >= 0 &&
                                  entries[selectedIndex].replace)) {
        dest[x] = src[sx];
        continue;
      }
      pixel.m *= alphaFactor;
      if (keep && replace && selectedIndex >= 0 &&
          entries[selectedIndex].replace) {
        const CelKey::RGB &color = entries[selectedIndex].replacement;
        if (!CelKey::finite(color)) {
          dest[x] = Pixel::Transparent;
          continue;
        }
        // Integer rasters require RGB to stay within the alpha coverage.
        // Float rasters retain finite HDR replacement colors.
        const bool isFloat = std::is_same<Pixel, TPixelF>::value;
        pixel.r = (isFloat ? color[0] : bounded(color[0], 0, 1)) * pixel.m;
        pixel.g = (isFloat ? color[1] : bounded(color[1], 0, 1)) * pixel.m;
        pixel.b = (isFloat ? color[2] : bounded(color[2], 0, 1)) * pixel.m;
      } else {
        pixel.r *= alphaFactor;
        pixel.g *= alphaFactor;
        pixel.b *= alphaFactor;
      }
      dest[x] = PixelConverter<Pixel>::from(pixel);
    }
  }
  return !(canceled && *canceled);
}
}  // namespace

void CelColorKeyFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &settings) {
  tile.getRaster()->clear();
  if (!m_source.isConnected() ||
      (settings.m_isCanceled && *settings.m_isCanceled))
    return;
  const TRectD output(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                              tile.getRaster()->getLy()));
  const TRectD input = inputRect(frame, output, settings);
  if (input.isEmpty()) return;
  if (!std::isfinite(input.getLx()) || !std::isfinite(input.getLy()) ||
      input.getLx() <= 0 || input.getLy() <= 0 ||
      input.getLx() > std::numeric_limits<int>::max() ||
      input.getLy() > std::numeric_limits<int>::max())
    throw TException("Cel Color Key: invalid input dimensions");
  checkInputSize(int(input.getLx()), int(input.getLy()),
                 tile.getRaster()->getPixelSize());
  const TDimension size(int(input.getLx()), int(input.getLy()));
  TRasterP sourceRaster;
  if (TRaster32P(tile.getRaster()))
    sourceRaster = TRaster32P(size);
  else if (TRaster64P(tile.getRaster()))
    sourceRaster = TRaster64P(size);
  else if (TRasterFP(tile.getRaster()))
    sourceRaster = TRasterFP(size);
  else
    throw TException("Cel Color Key: unsupported pixel type");
  sourceRaster->setLinear(tile.getRaster()->isLinear());
  sourceRaster->clear();
  TTile source;
  source.m_pos = input.getP00();
  source.setRaster(sourceRaster);
  m_source->compute(source, frame, settings);
  if (settings.m_isCanceled && *settings.m_isCanceled) return;
  std::array<CelKey::Entry, colorCapacity> entries;
  const int count = discrete(m_enabledColors->getValue(frame), colorCapacity);
  for (int i = 0; i < count; ++i) {
    const Slot &slot          = m_slots[i];
    CelKey::Entry &entry      = entries[i];
    entry.enabled             = discrete(slot.use->getValue(frame), 1) != 0;
    entry.replace             = discrete(slot.replace->getValue(frame), 1) != 0;
    const TPixelD key         = slot.key->getValueD(frame),
                  replacement = slot.replacement->getValueD(frame);
    entry.color               = {{key.r, key.g, key.b}};
    entry.replacement         = {{replacement.r, replacement.g, replacement.b}};
    entry.threshold           = bounded(slot.threshold->getValue(frame), 0, 1);
    entry.components = {{bounded(slot.component1->getValue(frame), 0, 1),
                         bounded(slot.component2->getValue(frame), 0, 1),
                         bounded(slot.component3->getValue(frame), 0, 1)}};
  }
  CelKey::Settings key;
  key.space = static_cast<CelKey::ColorSpace>(
      discrete(m_colorSpace->getValue(frame), 5));
  key.precision =
      static_cast<CelKey::Precision>(discrete(m_precision->getValue(frame), 2));
  key.premultiplied     = discrete(m_premultiplied->getValue(frame), 1) != 0;
  key.perColor          = discrete(m_perColor->getValue(frame), 1) != 0;
  key.perComponent      = discrete(m_perComponent->getValue(frame), 1) != 0;
  key.threshold         = bounded(m_threshold->getValue(frame), 0, 1);
  key.components        = {{bounded(m_component1->getValue(frame), 0, 1),
                            bounded(m_component2->getValue(frame), 0, 1),
                            bounded(m_component3->getValue(frame), 0, 1)}};
  const bool keep       = discrete(m_keep->getValue(frame), 1) != 0;
  const bool replace    = discrete(m_enableReplace->getValue(frame), 1) != 0;
  const double thin     = bounded(m_thin->getValue(frame), -4000, 4000),
               blur     = bounded(m_blur->getValue(frame), 0, 4000);
  const auto thinMetric = static_cast<CelDistance::Metric>(
      discrete(m_thinMetric->getValue(frame), 2));
  const auto blurMetric = static_cast<CelDistance::Metric>(
      discrete(m_blurMetric->getValue(frame), 2));
  const auto direction = static_cast<CelDistance::Direction>(
      discrete(m_blurDirection->getValue(frame), 2));
  const int offsetX = int(std::llround(tile.m_pos.x - input.x0)),
            offsetY = int(std::llround(tile.m_pos.y - input.y0));
  bool completed    = false;
  if (TRaster32P out = tile.getRaster())
    completed =
        applyKey<TPixel32>(source.getRaster(), out, offsetX, offsetY, entries,
                           count, key, keep, replace, thin, thinMetric, blur,
                           blurMetric, direction, settings.m_isCanceled);
  else if (TRaster64P out = tile.getRaster())
    completed =
        applyKey<TPixel64>(source.getRaster(), out, offsetX, offsetY, entries,
                           count, key, keep, replace, thin, thinMetric, blur,
                           blurMetric, direction, settings.m_isCanceled);
  else if (TRasterFP out = tile.getRaster())
    completed =
        applyKey<TPixelF>(source.getRaster(), out, offsetX, offsetY, entries,
                          count, key, keep, replace, thin, thinMetric, blur,
                          blurMetric, direction, settings.m_isCanceled);
  else
    throw TException("Cel Color Key: unsupported pixel type");
  if (!completed) tile.getRaster()->clear();
}

FX_PLUGIN_IDENTIFIER(CelColorKeyFx, "celColorKeyFx")
