#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"
#include "tspectrumparam.h"

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
constexpr double pi = 3.14159265358979323846;
double limit(double value, double minimum, double maximum) {
  return std::isfinite(value) ? std::max(minimum, std::min(maximum, value))
                              : minimum;
}
int mode(double value, int maximum) {
  return int(std::floor(limit(value, 0, maximum) + 0.5));
}
bool rasterSizeSupported(int width, int height, int bytesPerPixel) {
  return width > 0 && height > 0 && bytesPerPixel > 0 &&
         static_cast<unsigned long long>(width) *
                 static_cast<unsigned long long>(height) <=
             static_cast<unsigned long long>(std::numeric_limits<int>::max()) /
                 bytesPerPixel;
}
struct Vec {
  double r = 0, g = 0, b = 0, a = 0;
  Vec &operator+=(const Vec &v) {
    r += v.r;
    g += v.g;
    b += v.b;
    a += v.a;
    return *this;
  }
  Vec operator*(double v) const { return {r * v, g * v, b * v, a * v}; }
};
struct Stop {
  double position;
  TPixelD color;
};
struct Direction {
  double length = 0;
  bool useRamp  = false;
  TPixelD color;
  std::vector<Stop> ramp;
};
struct Settings {
  int channel = 0, profile = 1, merge = 0;
  bool approximate = false;
  double gain = 1, threshold = 0, fade = 0, glowOpacity = 1, sourceOpacity = 1;
  double rotation = 0, highlightRadius = 0;
  TPixelD highlightColor;
  std::array<Direction, 3> directions;
};
TPixelD rampColor(const Direction &direction, double position) {
  if (!direction.useRamp || direction.ramp.empty()) return direction.color;
  const auto &stops = direction.ramp;
  if (position <= stops.front().position) return stops.front().color;
  for (size_t i = 1; i < stops.size(); ++i) {
    if (position <= stops[i].position) {
      const double span = stops[i].position - stops[i - 1].position;
      const double t = span > 0 ? (position - stops[i - 1].position) / span : 1;
      const TPixelD &a = stops[i - 1].color, &b = stops[i].color;
      return TPixelD(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                     a.b + (b.b - a.b) * t, a.m + (b.m - a.m) * t);
    }
  }
  return stops.back().color;
}
double profileWeight(int profile, double distance) {
  const double x = std::abs(distance);
  if (x > 1) return 0;
  switch (profile) {
  case 0:
    return 1;
  case 1: {  // Continuous quadratic B-spline: three equal box convolutions.
    const double t = 1.5 * x;
    return t < 0.5 ? 0.75 - t * t : 0.5 * (1.5 - t) * (1.5 - t);
  }
  case 2:
    return std::exp(-4.5 * x * x);
  default:
    return std::exp(-4 * x);
  }
}
Vec extract(const TPixelD &p, const Settings &s) {
  if (!std::isfinite(p.r) || !std::isfinite(p.g) || !std::isfinite(p.b) ||
      !std::isfinite(p.m) || p.m <= 0)
    return {};
  const double r = p.r / p.m, g = p.g / p.m, b = p.b / p.m;
  if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) return {};
  const double intensity =
      s.channel == 0   ? p.m
      : s.channel == 1 ? std::max(0.0, 0.2126 * r + 0.7152 * g + 0.0722 * b)
      : s.channel == 3 ? std::max({0.0, r, g, b})
                       : std::max({0.0, r, g, b});
  double strength = std::max(0.0, intensity - s.threshold);
  if ((s.channel == 1 || s.channel == 3) && s.fade > 0)
    strength *= std::min(1.0, strength / s.fade);
  strength *= s.gain;
  if (s.channel != 0) strength *= p.m;
  if (s.channel == 2) {
    const double divisor =
        std::max({std::abs(r), std::abs(g), std::abs(b), 1e-30});
    return {std::max(0.0, r) / divisor * strength,
            std::max(0.0, g) / divisor * strength,
            std::max(0.0, b) / divisor * strength, strength};
  }
  return {strength, strength, strength, strength};
}
Vec tinted(const Vec &pixel, const TPixelD &color) {
  if (!std::isfinite(color.r) || !std::isfinite(color.g) ||
      !std::isfinite(color.b) || !std::isfinite(color.m))
    return {};
  return {pixel.r * color.r * color.m, pixel.g * color.g * color.m,
          pixel.b * color.b * color.m, pixel.a * color.m};
}
struct RasterLock {
  TRasterP raster;
  explicit RasterLock(const TRasterP &r) : raster(r) { raster->lock(); }
  ~RasterLock() { raster->unlock(); }
};
}  // namespace

class StarGlowFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(StarGlowFx)
  TRasterFxPort m_source;
  TDoubleParamP m_channel, m_profile, m_merge, m_approximate, m_gain,
      m_threshold, m_fade;
  TDoubleParamP m_glowOpacity, m_sourceOpacity, m_rotation, m_highlightRadius;
  TPixelParamP m_highlightColor;
  struct DirectionParams {
    TDoubleParamP length, useRamp;
    TPixelParamP color;
    TSpectrumParamP ramp;
    DirectionParams(double initialLength)
        : length(initialLength)
        , useRamp(0.0)
        , color(TPixel32::White)
        , ramp(std::vector<TSpectrum::ColorKey>{{0, TPixel32::White},
                                                {1, TPixel32::White}}) {}
  };
  DirectionParams m_vertical, m_horizontal, m_diagonal;

public:
  StarGlowFx()
      : m_channel(0.0)
      , m_profile(1.0)
      , m_merge(0.0)
      , m_approximate(0.0)
      , m_gain(1.0)
      , m_threshold(0.0)
      , m_fade(0.0)
      , m_glowOpacity(100.0)
      , m_sourceOpacity(100.0)
      , m_rotation(0.0)
      , m_highlightRadius(0.0)
      , m_highlightColor(TPixel32::White)
      , m_vertical(50.0)
      , m_horizontal(50.0)
      , m_diagonal(0.0) {
    addInputPort("Source", m_source);
    auto choices = [](const TDoubleParamP &p, const std::vector<std::string> &v,
                      bool flag = false) {
      p->setValueRange(0, double(v.size() - 1));
      p->setUIChoices(v, flag);
    };
    choices(m_channel, {"Alpha", "Luminance", "RGB", "Brightness"});
    choices(m_profile,
            {"Box", "Approximated Gaussian", "Gaussian", "Exponential"});
    choices(m_merge, {"Add", "Premultiply"});
    choices(m_approximate, {"Off", "On"}, true);
    bindParam(this, "channel", m_channel);
    bindParam(this, "profile", m_profile);
    bindParam(this, "merge", m_merge);
    bindParam(this, "approximate", m_approximate);
    bindParam(this, "gain", m_gain);
    m_gain->setValueRange(0, 1000);
    bindParam(this, "threshold", m_threshold);
    m_threshold->setValueRange(0, 100);
    bindParam(this, "fade", m_fade);
    m_fade->setValueRange(0, 100);
    bindParam(this, "glowOpacity", m_glowOpacity);
    m_glowOpacity->setValueRange(0, 100);
    bindParam(this, "sourceOpacity", m_sourceOpacity);
    m_sourceOpacity->setValueRange(0, 100);
    bindParam(this, "rotation", m_rotation);
    bindParam(this, "highlightRadius", m_highlightRadius);
    m_highlightRadius->setValueRange(0, 1000);
    bindParam(this, "highlightColor", m_highlightColor);
    m_highlightColor->enableMatte(false);
    auto direction = [&](const char *prefix, DirectionParams &d) {
      d.length->setValueRange(0, 1000);
      d.color->enableMatte(false);
      choices(d.useRamp, {"Off", "On"}, true);
      bindParam(this, std::string(prefix) + "Length", d.length);
      bindParam(this, std::string(prefix) + "Color", d.color);
      bindParam(this, std::string(prefix) + "UseRamp", d.useRamp);
      bindParam(this, std::string(prefix) + "Ramp", d.ramp);
    };
    direction("vertical", m_vertical);
    direction("horizontal", m_horizontal);
    direction("diagonal", m_diagonal);
    enableComputeInFloat(true);
  }
  bool requiresFullPrecisionSerialization() const override { return true; }
  bool canHandle(const TRenderSettings &, double) override { return false; }
  TAffine handledAffine(const TRenderSettings &, double) override {
    return TAffine();
  }
  bool toBeComputedInLinearColorSpace(bool, bool) const override {
    return false;
  }
  Settings snapshot(double frame) const;
  double support(double frame) const;
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

Settings StarGlowFx::snapshot(double frame) const {
  Settings s;
  s.channel         = mode(m_channel->getValue(frame), 3);
  s.profile         = mode(m_profile->getValue(frame), 3);
  s.merge           = mode(m_merge->getValue(frame), 1);
  s.approximate     = mode(m_approximate->getValue(frame), 1) != 0;
  s.gain            = limit(m_gain->getValue(frame), 0, 1000);
  s.threshold       = limit(m_threshold->getValue(frame), 0, 100) / 100;
  s.fade            = limit(m_fade->getValue(frame), 0, 100) / 100;
  s.glowOpacity     = limit(m_glowOpacity->getValue(frame), 0, 100) / 100;
  s.sourceOpacity   = limit(m_sourceOpacity->getValue(frame), 0, 100) / 100;
  s.rotation        = std::isfinite(m_rotation->getValue(frame))
                          ? m_rotation->getValue(frame)
                          : 0;
  s.highlightRadius = limit(m_highlightRadius->getValue(frame), 0, 1000);
  s.highlightColor  = m_highlightColor->getValueD(frame);
  // Disabled matte is stored as 255 by TPixelParam, while getValueD exposes
  // its raw value. Such colors are opaque in the normalized renderer.
  if (!m_highlightColor->isMatteEnabled()) s.highlightColor.m = 1.0;
  const std::array<const DirectionParams *, 3> params{
      {&m_vertical, &m_horizontal, &m_diagonal}};
  for (int d = 0; d < 3; ++d) {
    Direction &direction     = s.directions[d];
    const DirectionParams &p = *params[d];
    direction.length         = limit(p.length->getValue(frame), 0, 1000);
    direction.useRamp        = mode(p.useRamp->getValue(frame), 1) != 0;
    direction.color          = p.color->getValueD(frame);
    if (!p.color->isMatteEnabled()) direction.color.m = 1.0;
    for (int i = 0; i < p.ramp->getKeyCount(); ++i) {
      const double pos    = p.ramp->getPosition(i)->getValue(frame);
      const TPixelD color = p.ramp->getColor(i)->getValueD(frame);
      if (std::isfinite(pos) && std::isfinite(color.r) &&
          std::isfinite(color.g) && std::isfinite(color.b) &&
          std::isfinite(color.m))
        direction.ramp.push_back({pos, color});
    }
    std::stable_sort(
        direction.ramp.begin(), direction.ramp.end(),
        [](const Stop &a, const Stop &b) { return a.position < b.position; });
  }
  return s;
}

double StarGlowFx::support(double frame) const {
  const double maximum =
      std::max({limit(m_vertical.length->getValue(frame), 0, 1000),
                limit(m_horizontal.length->getValue(frame), 0, 1000),
                limit(m_diagonal.length->getValue(frame), 0, 1000),
                limit(m_highlightRadius->getValue(frame), 0, 1000)});
  return maximum > 0 ? std::ceil(maximum) + 2 : 0;
}

bool StarGlowFx::doGetBBox(double frame, TRectD &box,
                           const TRenderSettings &settings) {
  if (!m_source.isConnected() || !m_source->doGetBBox(frame, box, settings)) {
    box = TRectD();
    return false;
  }
  box = box.enlarge(support(frame));
  return true;
}

void StarGlowFx::transform(double frame, int, const TRectD &output,
                           const TRenderSettings &settings, TRectD &input,
                           TRenderSettings &inputSettings) {
  inputSettings = settings;
  input         = TRectD();
  if (!m_source.isConnected() || output.isEmpty()) return;
  TRectD sourceBox;
  if (!m_source->getBBox(frame, sourceBox, settings)) return;
  const TRectD clipped = output.enlarge(support(frame)) * sourceBox.enlarge(2);
  if (clipped.isEmpty()) return;
  input = TRectD(output.x0 + std::floor(clipped.x0 - output.x0),
                 output.y0 + std::floor(clipped.y0 - output.y0),
                 output.x0 + std::ceil(clipped.x1 - output.x0),
                 output.y0 + std::ceil(clipped.y1 - output.y0));
}

int StarGlowFx::getMemoryRequirement(const TRectD &rect, double frame,
                                     const TRenderSettings &settings) {
  TRectD input;
  TRenderSettings inputSettings;
  transform(frame, 0, rect, settings, input, inputSettings);
  // Calculate in wide floating point: the host helper multiplies unsigned
  // long values, which can overflow at 32 bits on Windows.
  if (input.isEmpty()) return 0;
  const long double inputWidth  = static_cast<long double>(input.getLx()) + 1;
  const long double inputHeight = static_cast<long double>(input.getLy()) + 1;
  const long double outputWidth = static_cast<long double>(rect.getLx()) + 1;
  const long double bytes =
      inputWidth * inputHeight * 80 + outputWidth * inputHeight * 32;
  const long double megabytes = bytes / (1024 * 1024);
  if (!std::isfinite(megabytes) || megabytes >= std::numeric_limits<int>::max())
    return std::numeric_limits<int>::max();
  return int(std::ceil(megabytes));
}

std::string StarGlowFx::getAlias(double frame,
                                 const TRenderSettings &settings) const {
  const Settings s = snapshot(frame);
  std::ostringstream out;
  out.imbue(std::locale::classic());
  // Build the source portion directly: the generic spectrum alias converts
  // every stop to 8-bit before formatting, which can overflow for HDR colors.
  out << getFxType() << "[source:";
  TFxPort *sourcePort = getInputPort(0);
  if (sourcePort && sourcePort->isConnected()) {
    TRasterFxP source = sourcePort->getFx();
    if (source) out << source->getAlias(frame, settings);
  }
  out << "][starGlow:"
      << std::setprecision(std::numeric_limits<double>::max_digits10)
      << s.channel << ',' << s.profile << ',' << s.merge << ',' << s.approximate
      << ',' << s.gain << ',' << s.threshold << ',' << s.fade << ','
      << s.glowOpacity << ',' << s.sourceOpacity << ',' << s.rotation << ','
      << s.highlightRadius;
  auto color = [&](const TPixelD &c) {
    out << ';' << c.r << ',' << c.g << ',' << c.b << ',' << c.m;
  };
  color(s.highlightColor);
  for (const Direction &direction : s.directions) {
    out << ';' << direction.length << ',' << direction.useRamp;
    color(direction.color);
    for (const Stop &stop : direction.ramp) {
      out << ';' << stop.position;
      color(stop.color);
    }
  }
  return out.str() + ']';
}

namespace {
Vec get(const std::vector<Vec> &pixels, int width, int height, int x, int y) {
  if (x < 0 || y < 0 || x >= width || y >= height) return {};
  return pixels[size_t(y) * size_t(width) + size_t(x)];
}
Vec sample(const std::vector<Vec> &pixels, int width, int height, double x,
           double y) {
  const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
  const double dx = x - x0, dy = y - y0;
  Vec result = get(pixels, width, height, x0, y0) * ((1 - dx) * (1 - dy));
  result += get(pixels, width, height, x0 + 1, y0) * (dx * (1 - dy));
  result += get(pixels, width, height, x0, y0 + 1) * ((1 - dx) * dy);
  result += get(pixels, width, height, x0 + 1, y0 + 1) * (dx * dy);
  return result;
}
Vec streak(const std::vector<Vec> &pixels, int width, int height, double x,
           double y, double angle, const Direction &direction, int profile,
           bool approximate) {
  if (direction.length <= 0) return {};
  const int radius = int(std::ceil(direction.length));
  const double dx = std::cos(angle), dy = std::sin(angle);
  Vec sum;
  double weights = 0;
  const int step = approximate && radius >= 3 ? 2 : 1;
  for (int i = -radius + (step == 2 ? radius % 2 : 0); i <= radius; i += step) {
    const double distance = std::abs(double(i)) / direction.length;
    const double weight   = profileWeight(profile, distance);
    if (weight <= 0) continue;
    sum += tinted(sample(pixels, width, height, x + i * dx, y + i * dy),
                  rampColor(direction, distance)) *
           weight;
    weights += weight;
  }
  return weights > 0 ? sum * (1.0 / weights) : Vec{};
}

template <class Pixel>
bool render(const TRasterPT<Pixel> &source, const TRasterPT<Pixel> &output,
            int offsetX, int offsetY, int originParityX, int originParityY,
            const Settings &s, const int *canceled) {
  const int w = source->getLx(), h = source->getLy();
  if (w <= 0 || h <= 0 ||
      size_t(w) > std::numeric_limits<size_t>::max() / size_t(h) / sizeof(Vec))
    throw TException("Star Glow: invalid raster dimensions");
  std::vector<Vec> extracted(size_t(w) * size_t(h));
  RasterLock inputLock(source), outputLock(output);
  auto inputPixel = [&](int x, int y) -> Vec {
    if (x < 0 || y < 0 || x >= w || y >= h) return {};
    const TPixelD p = toPixelD(source->pixels(y)[x]);
    if (!std::isfinite(p.r) || !std::isfinite(p.g) || !std::isfinite(p.b) ||
        !std::isfinite(p.m) || p.m <= 0)
      return {};
    return {p.r, p.g, p.b, p.m};
  };
  for (int y = 0; y < h; ++y) {
    if (canceled && *canceled) return false;
    for (int x = 0; x < w; ++x) {
      if ((x & 31) == 0 && canceled && *canceled) return false;
      if (s.approximate) {
        const int gx = originParityX + x, gy = originParityY + y;
        const int bx = x - ((gx % 2 + 2) % 2), by = y - ((gy % 2 + 2) % 2);
        Vec average = inputPixel(bx, by);
        average += inputPixel(bx + 1, by);
        average += inputPixel(bx, by + 1);
        average += inputPixel(bx + 1, by + 1);
        average = average * 0.25;
        extracted[size_t(y) * size_t(w) + size_t(x)] =
            extract(TPixelD(average.r, average.g, average.b, average.a), s);
      } else {
        extracted[size_t(y) * size_t(w) + size_t(x)] =
            extract(toPixelD(source->pixels(y)[x]), s);
      }
    }
  }
  // The highlight is a separable two-axis blur, reusing the selected profile.
  std::vector<Vec> horizontal;
  int horizontalStart = 0, horizontalWidth = 0;
  if (s.highlightRadius > 0) {
    const long long radius =
        static_cast<long long>(std::ceil(s.highlightRadius));
    const long long start =
        std::max(0LL, -static_cast<long long>(offsetX) - radius - 1);
    const long long end =
        std::min(static_cast<long long>(output->getLx()),
                 static_cast<long long>(w) - offsetX + radius + 1);
    if (end > start) {
      horizontalStart = int(start);
      horizontalWidth = int(end - start);
      if (size_t(horizontalWidth) >
          std::numeric_limits<size_t>::max() / size_t(h) / sizeof(Vec))
        throw TException("Star Glow: invalid highlight dimensions");
      horizontal.resize(size_t(horizontalWidth) * size_t(h));
    }
    Direction white;
    white.length = s.highlightRadius;
    white.color  = TPixelD(1, 1, 1, 1);
    for (int y = 0; y < h; ++y) {
      if (canceled && *canceled) return false;
      for (int x = 0; x < horizontalWidth; ++x) {
        if ((x & 31) == 0 && canceled && *canceled) return false;
        horizontal[size_t(y) * size_t(horizontalWidth) + size_t(x)] =
            streak(extracted, w, h, x + horizontalStart + offsetX, y, 0, white,
                   s.profile, s.approximate);
      }
    }
  }
  const double rotation = std::remainder(s.rotation, 360.0) * pi / 180.0;
  for (int y = 0; y < output->getLy(); ++y) {
    if (canceled && *canceled) return false;
    Pixel *dest = output->pixels(y);
    for (int x = 0; x < output->getLx(); ++x) {
      if ((x & 31) == 0 && canceled && *canceled) return false;
      const int sx = x + offsetX, sy = y + offsetY;
      Vec glow;
      glow += streak(extracted, w, h, sx, sy, rotation + pi / 2,
                     s.directions[0], s.profile, s.approximate);
      glow += streak(extracted, w, h, sx, sy, rotation, s.directions[1],
                     s.profile, s.approximate);
      glow += streak(extracted, w, h, sx, sy, rotation + pi / 4,
                     s.directions[2], s.profile, s.approximate);
      glow += streak(extracted, w, h, sx, sy, rotation + 3 * pi / 4,
                     s.directions[2], s.profile, s.approximate);
      if (s.highlightRadius > 0 && x >= horizontalStart &&
          x < horizontalStart + horizontalWidth) {
        Direction highlight;
        highlight.length = s.highlightRadius;
        highlight.color  = s.highlightColor;
        glow += streak(horizontal, horizontalWidth, h, x - horizontalStart, sy,
                       pi / 2, highlight, s.profile, s.approximate);
      }
      TPixelD original(0, 0, 0, 0);
      if (sx >= 0 && sy >= 0 && sx < w && sy < h)
        original = toPixelD(source->pixels(sy)[sx]);
      if (!std::isfinite(original.r) || !std::isfinite(original.g) ||
          !std::isfinite(original.b) || !std::isfinite(original.m))
        original = TPixelD(0, 0, 0, 0);
      const double alpha = limit(glow.a * s.glowOpacity, 0, 1);
      const double underlying =
          s.merge == 0 ? s.sourceOpacity : s.sourceOpacity * (1 - alpha);
      const double maximum = std::is_same<Pixel, TPixelF>::value
                                 ? double(std::numeric_limits<float>::max())
                                 : 1.0;
      const double minimum =
          std::is_same<Pixel, TPixelF>::value ? -maximum : 0.0;
      auto safeChannel = [minimum, maximum](double value) {
        return std::isnan(value) ? 0.0
                                 : std::max(minimum, std::min(maximum, value));
      };
      const TPixelD composite(
          safeChannel(original.r * underlying + glow.r * s.glowOpacity),
          safeChannel(original.g * underlying + glow.g * s.glowOpacity),
          safeChannel(original.b * underlying + glow.b * s.glowOpacity),
          limit(original.m * underlying + alpha, 0, 1));
      dest[x] = PixelConverter<Pixel>::from(composite);
    }
  }
  return !(canceled && *canceled);
}
}  // namespace

void StarGlowFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &settings) {
  tile.getRaster()->clear();
  if (!m_source.isConnected() ||
      (settings.m_isCanceled && *settings.m_isCanceled))
    return;
  const TRectD output(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                              tile.getRaster()->getLy()));
  TRectD input;
  TRenderSettings inputSettings;
  transform(frame, 0, output, settings, input, inputSettings);
  if (input.isEmpty()) return;
  if (!std::isfinite(input.getLx()) || !std::isfinite(input.getLy()) ||
      input.getLx() <= 0 || input.getLy() <= 0 ||
      input.getLx() > std::numeric_limits<int>::max() ||
      input.getLy() > std::numeric_limits<int>::max())
    throw TException("Star Glow: invalid input dimensions");
  // Bounds were aligned to the output pixel grid. Round away tiny floating
  // point subtraction error at fractional tile origins rather than truncating.
  const int width         = int(std::llround(input.getLx()));
  const int height        = int(std::llround(input.getLy()));
  const int bytesPerPixel = tile.getRaster()->getPixelSize();
  if (!rasterSizeSupported(width, height, bytesPerPixel))
    throw TException(
        "Star Glow: input raster exceeds the supported allocation size");
  TRasterP sourceRaster;
  if (TRaster32P(tile.getRaster()))
    sourceRaster = TRaster32P(width, height);
  else if (TRaster64P(tile.getRaster()))
    sourceRaster = TRaster64P(width, height);
  else if (TRasterFP(tile.getRaster()))
    sourceRaster = TRasterFP(width, height);
  else
    throw TException("Star Glow: unsupported pixel type");
  sourceRaster->setLinear(tile.getRaster()->isLinear());
  sourceRaster->clear();
  TTile source;
  source.setRaster(sourceRaster);
  source.m_pos = input.getP00();
  m_source->compute(source, frame, settings);
  if (settings.m_isCanceled && *settings.m_isCanceled) return;
  const Settings values = snapshot(frame);
  const int dx          = int(std::llround(tile.m_pos.x - input.x0)),
            dy          = int(std::llround(tile.m_pos.y - input.y0));
  // Only grid parity is needed; avoid converting large world coordinates to
  // int.
  const int ox  = int(std::fmod(std::floor(input.x0), 2.0));
  const int oy  = int(std::fmod(std::floor(input.y0), 2.0));
  bool complete = false;
  if (TRaster32P out = tile.getRaster())
    complete = render<TPixel32>(source.getRaster(), out, dx, dy, ox, oy, values,
                                settings.m_isCanceled);
  else if (TRaster64P out = tile.getRaster())
    complete = render<TPixel64>(source.getRaster(), out, dx, dy, ox, oy, values,
                                settings.m_isCanceled);
  else if (TRasterFP out = tile.getRaster())
    complete = render<TPixelF>(source.getRaster(), out, dx, dy, ox, oy, values,
                               settings.m_isCanceled);
  else
    throw TException("Star Glow: unsupported pixel type");
  if (!complete) tile.getRaster()->clear();
}

FX_PLUGIN_IDENTIFIER(StarGlowFx, "starGlowFx")
