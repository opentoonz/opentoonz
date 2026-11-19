#include "tonehighlightfx.cpp"
#include "trenderer.h"
#include "tthread.h"

#include <QCoreApplication>

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

class RenderContext {
  TRenderer m_renderer;
  unsigned long m_id;

public:
  RenderContext() : m_id(TRenderer::buildRenderId()) {
    m_renderer.install(m_id);
    m_renderer.declareRenderStart(m_id);
    m_renderer.declareFrameStart(0);
  }
  ~RenderContext() {
    m_renderer.declareFrameEnd(0);
    m_renderer.declareRenderEnd(m_id);
    m_renderer.uninstall();
  }
};

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void setDouble(ToneHighlightFx& fx, const char* name, double value) {
  auto* param = dynamic_cast<TDoubleParam*>(fx.getParams()->getParam(name));
  require(param != nullptr, "Missing numeric parameter");
  param->setDefaultValue(value);
}

void setBool(ToneHighlightFx& fx, const char* name, bool value) {
  auto* param = dynamic_cast<TBoolParam*>(fx.getParams()->getParam(name));
  require(param != nullptr, "Missing boolean parameter");
  param->setValue(value);
}

void setMode(ToneHighlightFx& fx, bool highlight) {
  auto* param = dynamic_cast<TIntEnumParam*>(fx.getParams()->getParam("mode"));
  require(param != nullptr, "Missing mode parameter");
  param->setValue(highlight ? ToneHighlightFx::HIGHLIGHT
                            : ToneHighlightFx::TONE);
}

template <class Pixel>
void fillPattern(const TRasterPT<Pixel>& raster, const TPointD& origin,
                 int shift) {
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const int u = int(origin.x) + x - shift, v = int(origin.y) + y;
      const bool covered = u >= 0 && u < 42 && v >= 0 && v < 36;
      const int alpha = covered ? Pixel::maxChannelValue * (3 + u % 3) / 5 : 0;
      raster->pixels(y)[x] =
          Pixel(alpha * (u % 4) / 4, alpha * (v % 3) / 3, alpha / 2, alpha);
    }
  }
}

class SampleSource final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(SampleSource)

public:
  int computeCalls = 0;
  int dryCalls     = 0;
  TRectD lastDryRect;
  TPixelF sample = TPixelF(2.25f, -0.125f, 0.375f, 0.33333334f);
  bool patterned = false;
  bool alphaRamp = false;
  int shift      = 0;

  SampleSource() { enableComputeInFloat(true); }
  bool doGetBBox(double, TRectD& rect, const TRenderSettings&) override {
    rect = TRectD(-100000, -100000, 100000, 100000);
    return true;
  }
  bool canHandle(const TRenderSettings&, double) override { return true; }
  std::string getAlias(double, const TRenderSettings&) const override {
    return "ToneHighlightTestSource" + std::to_string(patterned) +
           std::to_string(shift);
  }
  void doDryCompute(TRectD& rect, double, const TRenderSettings&) override {
    ++dryCalls;
    lastDryRect = rect;
  }
  void doCompute(TTile& tile, double, const TRenderSettings&) override {
    ++computeCalls;
    TRasterFP floating = tile.getRaster();
    if (floating) {
      if (alphaRamp) {
        for (int y = 0; y < floating->getLy(); ++y)
          for (int x = 0; x < floating->getLx(); ++x) {
            const float alpha = tcrop((tile.m_pos.x + x + 1) * 0.2, 0.0, 1.0);
            floating->pixels(y)[x] = TPixelF(alpha, alpha, alpha, alpha);
          }
      } else
        floating->fill(sample);
      return;
    }
    TRaster32P eight = tile.getRaster();
    if (eight) {
      if (patterned)
        fillPattern<TPixel32>(eight, tile.m_pos, shift);
      else
        eight->fill(TPixel32(100, 80, 40, 128));
      return;
    }
    TRaster64P sixteen = tile.getRaster();
    require(bool(sixteen), "Unexpected raster type in source");
    if (patterned)
      fillPattern<TPixel64>(sixteen, tile.m_pos, shift);
    else
      sixteen->fill(TPixel64(50000, 40000, 20000, 60000));
  }
};

FX_PLUGIN_IDENTIFIER(SampleSource, "toneHighlightTestSource")

struct Fixture {
  TFxP fxOwner         = new ToneHighlightFx;
  ToneHighlightFx& fx  = *dynamic_cast<ToneHighlightFx*>(fxOwner.getPointer());
  SampleSource* source = new SampleSource;
  SampleSource* matte  = new SampleSource;
  TFxP sourceOwner     = source;
  TFxP matteOwner      = matte;
  TRenderSettings settings;

  Fixture() {
    fx.connect("Source", source);
    fx.connect("Matte", matte);
    matte->sample  = TPixelF(1.0f, 1.0f, 1.0f, 1.0f);
    settings.m_bpp = 128;
  }
};

void expectRadiusRejected(Fixture& f) {
  require(f.fx.getMemoryRequirement(TRectD(0, 0, 64, 64), 0, f.settings) == 0,
          "An invalid radius should not schedule workspace allocation");
  require(f.source->computeCalls == 0 && f.matte->computeCalls == 0,
          "Radius validation unexpectedly computed input images");
  const int sourceDryCalls = f.source->dryCalls;
  const int matteDryCalls  = f.matte->dryCalls;
  TRectD rect(0, 0, 64, 64);
  f.fx.doDryCompute(rect, 0, f.settings);
  require(f.source->dryCalls == sourceDryCalls &&
              f.matte->dryCalls == matteDryCalls,
          "An invalid radius should not precompute input images");
  bool rejected = false;
  try {
    TRasterP output;
    if (f.settings.m_bpp == 32)
      output = TRaster32P(2, 2);
    else if (f.settings.m_bpp == 64)
      output = TRaster64P(2, 2);
    else
      output = TRasterFP(2, 2);
    TTile tile(output);
    f.fx.compute(tile, 0, f.settings);
  } catch (const TException& error) {
    rejected = !error.getMessage().empty();
  }
  require(rejected, "Render compute accepted an unsafe blur radius");
  require(f.source->computeCalls == 0 && f.matte->computeCalls == 0,
          "Unsafe radius allocated and computed input images");
}

void testExtremeRadius() {
  Fixture f;
  setDouble(f.fx, "softness", std::numeric_limits<double>::max());
  expectRadiusRejected(f);
}

void testWorkspaceRadius() {
  Fixture f;
  setDouble(f.fx, "softness", 8192.0);
  for (int bpp : {32, 64, 128}) {
    f.settings.m_bpp = bpp;
    expectRadiusRejected(f);
  }
  f.settings.m_maxTileSize = 1;
  setDouble(f.fx, "softness", 256.0);
  expectRadiusRejected(f);
}

void testNonfiniteRadius() {
  Fixture f;
  setDouble(f.fx, "softness", std::numeric_limits<double>::infinity());
  expectRadiusRejected(f);
  setDouble(f.fx, "softness", std::numeric_limits<double>::quiet_NaN());
  expectRadiusRejected(f);
}

void testRadiusScale() {
  Fixture f;
  setDouble(f.fx, "softness", 2.125);
  f.settings.m_affine = TScale(2.0);
  TRectD rect(0, 0, 20, 10);
  f.fx.doDryCompute(rect, 0, f.settings);
  require(f.source->lastDryRect == rect,
          "Source request was unexpectedly padded");
  require(f.matte->lastDryRect == rect.enlarge(5),
          "Ordinary fractional scaled blur padding changed");
  f.settings.m_affine = TScale(1e100);
  expectRadiusRejected(f);
}

void testMemory() {
  Fixture f;
  setDouble(f.fx, "softness", 16.0);
  const TRectD output(0, 0, 1024, 768);
  const double padded = 1056.0 * 800;
  for (int bpp : {32, 64, 128}) {
    f.settings.m_bpp    = bpp;
    const int actual    = f.fx.getMemoryRequirement(output, 0, f.settings);
    const int workspace = bpp == 32 ? 16 : 32;
    const double bytes =
        padded * (bpp / 8 + workspace) + 1024.0 * 768 * (bpp / 8);
    require(actual >= int(std::ceil(bytes / 1048576)),
            "Memory estimate omits a live matte or the blur workspace");
  }
  require(f.fx.getMemoryRequirement(TRectD(), 0, f.settings) == 0,
          "Empty output should not require a padded matte");
  require(f.fx.getMemoryRequirement(TRectD(0, 0, 1e10, 1e10), 0, f.settings) ==
              std::numeric_limits<int>::max(),
          "An oversized memory estimate did not saturate");
}

void requireUnchanged(const TRasterFP& output, const TPixelF& expected) {
  for (int y = 0; y < output->getLy(); ++y)
    for (int x = 0; x < output->getLx(); ++x)
      require(
          std::memcmp(&output->pixels(y)[x], &expected, sizeof(TPixelF)) == 0,
          "Source HDR RGB or alpha changed through TRasterFx::compute");
}

void checkInactive(bool zeroIntensity) {
  Fixture f;
  if (zeroIntensity)
    setDouble(f.fx, "intensity", 0.0);
  else
    f.fx.disconnect("Matte");
  TRasterFP output(3, 2);
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  requireUnchanged(output, f.source->sample);
  require(f.matte->computeCalls == 0, "Inactive shading computed Matte");
}

void testHdrZero() { checkInactive(true); }
void testHdrNoMatte() { checkInactive(false); }

void testNoOp() {
  for (bool zeroIntensity : {true, false}) {
    Fixture f;
    if (zeroIntensity)
      setDouble(f.fx, "intensity", 0.0);
    else
      f.fx.disconnect("Matte");
    setDouble(f.fx, "softness", std::numeric_limits<double>::max());
    f.settings.m_affine = TScale(2.0, 3.0);
    require(f.fx.canHandle(f.settings, 0),
            "Inactive shading imposed blur's affine restriction");
    TRectD rect(0, 0, 3, 2);
    f.fx.doDryCompute(rect, 0, f.settings);
    require(f.source->dryCalls == 1 && f.source->lastDryRect == rect,
            "Inactive shading did not request Source's exact rectangle");
    require(f.matte->dryCalls == 0, "Inactive shading dry-computed Matte");
    require(f.fx.getMemoryRequirement(rect, 0, f.settings) <= 1,
            "Inactive shading estimated a blur workspace");
    TRasterFP output(3, 2);
    TTile tile(output);
    f.fx.compute(tile, 0, f.settings);
    requireUnchanged(output, f.source->sample);
  }
}

void testActiveFloat() {
  for (bool highlight : {false, true}) {
    for (double softness : {0.0, std::numeric_limits<double>::denorm_min(),
                            1e-100, 1e-38, 1e-7, 2.125}) {
      Fixture f;
      setMode(f.fx, highlight);
      setDouble(f.fx, "softness", softness);
      setBool(f.fx, "protectLines", false);
      TRasterFP output(2, 2);
      TTile tile(output);
      f.fx.compute(tile, 0, f.settings);
      const TPixelF actual = output->pixels(0)[0];
      const TPixelF input  = f.source->sample;
      require(actual.m == input.m, "Active shading changed float alpha");
      const float channels[] = {input.r, input.g, input.b};
      const float observed[] = {actual.r, actual.g, actual.b};
      const double colors[]  = {90.0 / 255, 80.0 / 255, 140.0 / 255};
      for (int c = 0; c < 3; ++c) {
        const double expected =
            highlight ? channels[c] + 0.5 * colors[c] * (input.m - channels[c])
                      : channels[c] * (0.5 + 0.5 * colors[c]);
        const bool matches = std::abs(observed[c] - expected) < 1e-6;
        if (!matches)
          std::cerr << "Softness " << softness << ", channel " << c << ": "
                    << observed[c] << " != " << expected << '\n';
        require(matches,
                "Active float tone/highlight clipped or changed its blend");
      }
    }
  }
}

void testTinySoftness() {
  const double epsilon = std::numeric_limits<float>::epsilon();
  for (double softness :
       {0.0, 1e-100, 1e-20, 1e-10, std::nextafter(epsilon, 0.0), epsilon,
        std::nextafter(epsilon, 1.0), 0.01, 0.5, std::nextafter(1.0, 0.0), 1.0,
        std::nextafter(1.0, 2.0)}) {
    Fixture f;
    f.matte->alphaRamp = true;
    setDouble(f.fx, "softness", softness);
    setBool(f.fx, "protectLines", false);
    TRasterFP output(3, 1);
    TTile tile(output);
    f.fx.compute(tile, 0, f.settings);
    for (int x = 0; x < 3; ++x) {
      const double amount = 0.5 * (x + 1) * 0.2;
      const double expected =
          f.source->sample.r * (1.0 - amount + amount * 90.0 / 255);
      const TPixelF actual = output->pixels(0)[x];
      const bool matches   = std::abs(actual.r - expected) < 2e-6;
      if (!matches)
        std::cerr << "Softness " << softness << ", pixel " << x << ": "
                  << actual.r << " != " << expected << '\n';
      require(matches, "Subpixel Softness corrupted spatial matte alpha");
      require(actual.m == f.source->sample.m,
              "Subpixel Softness changed source alpha");
    }
  }
}

template <class Pixel>
uint64_t hashPixels(const TRasterPT<Pixel>& raster) {
  uint64_t hash = 14695981039346656037ULL;
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const Pixel& pixel = raster->pixels(y)[x];
      for (unsigned int channel : {pixel.b, pixel.g, pixel.r, pixel.m}) {
        for (size_t byte = 0; byte < sizeof(pixel.r); ++byte) {
          hash ^= (channel >> (8 * byte)) & 255;
          hash *= 1099511628211ULL;
        }
      }
    }
  }
  return hash;
}

struct IntegerCase {
  int bpp;
  bool highlight;
  bool protect;
  bool invert;
  double softness;
};

uint64_t renderInteger(const IntegerCase& settings, std::ostream* pixels) {
  Fixture f;
  f.settings.m_bpp    = settings.bpp;
  f.source->patterned = f.matte->patterned = true;
  f.matte->shift                           = 7;
  setMode(f.fx, settings.highlight);
  setBool(f.fx, "protectLines", settings.protect);
  setBool(f.fx, "invertMatte", settings.invert);
  setDouble(f.fx, "softness", settings.softness);
  TRasterP output = settings.bpp == 32 ? TRasterP(TRaster32P(55, 49))
                                       : TRasterP(TRaster64P(55, 49));
  TTile tile(output, TPointD(-7, -6));
  f.fx.compute(tile, 0, f.settings);
  if (pixels) {
    for (int y = 0; y < output->getLy(); ++y) {
      const unsigned char* row =
          output->getRawData() + y * output->getWrap() * (settings.bpp / 8);
      pixels->write(reinterpret_cast<const char*>(row),
                    output->getLx() * (settings.bpp / 8));
    }
  }
  return settings.bpp == 32 ? hashPixels<TPixel32>(TRaster32P(output))
                            : hashPixels<TPixel64>(TRaster64P(output));
}

void printIntegerMatrix(std::ostream* pixels) {
  const uint64_t expected[] = {
      0x6f7a2f60de300425ULL, 0x2fdd53bd98c7c4a6ULL, 0xbf17100747bd8815ULL,
      0xf32984c7d14b384bULL, 0xffae6196b30820a5ULL, 0x3b892804791fa98fULL,
      0x9120e82356851fc5ULL, 0xc5c9d6236b7337d1ULL, 0x226a4614dec36795ULL,
      0xf48880f6dc077091ULL, 0x351b408404c546b5ULL, 0xf551f0d446442863ULL,
      0x542ad273dfcf40adULL, 0xcb8f3da73815da6dULL, 0xb3a9d894a650c325ULL,
      0x434f5b0e54ee72e5ULL, 0x240b115cc0553ab5ULL, 0x0bd67c094cea2192ULL,
      0x1b219beb1e711205ULL, 0xc65321004605e9afULL, 0x55d13e89a8d9c185ULL,
      0x94ff892d20aad015ULL, 0x4120764d371a0955ULL, 0x497da64924039787ULL,
      0x35999c995224648dULL, 0xc1d1f3288b3be461ULL, 0xacb005b6775789f5ULL,
      0xd72260b26d8630cdULL, 0xde9e1b1b291f8935ULL, 0x7f05e4b4d9b543e9ULL,
      0x5751faa83a80c78dULL, 0x34bdb4cce8eafaacULL};
  size_t caseIndex = 0;
  for (int bpp : {32, 64}) {
    for (bool highlight : {false, true}) {
      for (bool protect : {false, true}) {
        for (bool invert : {false, true}) {
          for (double softness : {0.0, 2.125}) {
            const uint64_t hash = renderInteger(
                {bpp, highlight, protect, invert, softness}, pixels);
            std::cout << bpp << ',' << highlight << ',' << protect << ','
                      << invert << ',' << softness << ':' << std::hex << hash
                      << std::dec << '\n';
            require(hash == expected[caseIndex++],
                    "Integer render differs from the original PR");
          }
        }
      }
    }
  }
}

void printSubpixelMatrix(std::ostream* pixels) {
  bool unchanged = true;
  for (int bpp : {32, 64}) {
    for (bool highlight : {false, true}) {
      for (bool protect : {false, true}) {
        for (bool invert : {false, true}) {
          const uint64_t expected =
              renderInteger({bpp, highlight, protect, invert, 0.0}, nullptr);
          for (double softness : {0.01, 0.75, 1.0}) {
            const uint64_t hash = renderInteger(
                {bpp, highlight, protect, invert, softness}, pixels);
            std::cout << bpp << ',' << highlight << ',' << protect << ','
                      << invert << ',' << softness << ':' << std::hex << hash
                      << std::dec << ", identity=" << (hash == expected)
                      << '\n';
            unchanged = unchanged && hash == expected;
          }
        }
      }
    }
  }
  require(unchanged, "Center-only blur kernel changed integer rendering");
}

void testSourceMissing() {
  Fixture f;
  f.fx.disconnect("Source");
  setDouble(f.fx, "softness", std::numeric_limits<double>::max());
  TRectD rect(0, 0, 3, 2);
  f.fx.doDryCompute(rect, 0, f.settings);
  TRasterFP output(3, 2);
  output->clear();
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  require(f.source->computeCalls == 0 && f.matte->computeCalls == 0 &&
              f.matte->dryCalls == 0,
          "Missing Source evaluated an input");
  requireUnchanged(output, TPixelF(0, 0, 0, 0));
}

#ifdef TONE_HIGHLIGHT_TEST_PADDING
void testPadding() {
  require(checkedMatteSize(TDimension(8192, 16384), 0, 32) ==
              TDimension(8192, 16384),
          "Unblurred matte was rejected for an unused blur workspace");
  for (int bpp : {32, 64, 128}) {
    const int bytes = bpp == 32 ? 16 : 32;
    const int width = std::numeric_limits<int>::max() / (35 * bytes) - 34;
    require(checkedMatteSize(TDimension(width, 1), 17, bpp) ==
                TDimension(width + 34, 35),
            "Largest supported scratch-plane width was rejected");
    bool rejected = false;
    try {
      checkedMatteSize(TDimension(width + 1, 1), 17, bpp);
    } catch (const TException& error) {
      rejected = !error.getMessage().empty();
    }
    require(rejected, "Oversized blur workspace passed the raster limit");
    rejected = false;
    try {
      checkedMatteSize(TDimension(std::numeric_limits<int>::max(), 1), 17, bpp);
    } catch (const TException& error) {
      rejected = !error.getMessage().empty();
    }
    require(rejected, "Padded width overflowed instead of rejecting the tile");
  }
}
#endif

void runMatrix(int argc, char** argv) {
  std::ofstream pixels;
  if (argc == 3) {
    pixels.open(argv[2], std::ios::binary);
    require(bool(pixels), "Cannot open pixel comparison output");
  }
  std::ostream* output = argc == 3 ? &pixels : nullptr;
  if (std::string(argv[1]) == "integer-matrix")
    printIntegerMatrix(output);
  else
    printSubpixelMatrix(output);
  require(bool(pixels), "Cannot write pixel comparison output");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  RenderContext context;
  if (argc != 2 && argc != 3) return 2;
  const std::string name(argv[1]);
  const std::map<std::string, void (*)()> cases = {
      {"radius-extreme", testExtremeRadius},
      {"radius-workspace", testWorkspaceRadius},
      {"radius-nonfinite", testNonfiniteRadius},
      {"radius-scale", testRadiusScale},
      {"memory", testMemory},
      {"hdr-zero", testHdrZero},
      {"hdr-no-matte", testHdrNoMatte},
      {"no-op", testNoOp},
      {"active-float", testActiveFloat},
      {"tiny-softness", testTinySoftness},
      {"source-missing", testSourceMissing},
#ifdef TONE_HIGHLIGHT_TEST_PADDING
      {"padding", testPadding},
#endif
  };
  try {
    if (name == "integer-matrix" || name == "subpixel-matrix") {
      runMatrix(argc, argv);
    } else {
      const auto test = cases.find(name);
      if (test == cases.end()) return 2;
      test->second();
    }
  } catch (const std::exception& error) {
    std::cerr << name << ": " << error.what() << '\n';
    return 1;
  } catch (const TException& error) {
    std::wcerr << error.getMessage() << '\n';
    return 1;
  }
  std::cout << name << ": passed\n";
  return 0;
}
