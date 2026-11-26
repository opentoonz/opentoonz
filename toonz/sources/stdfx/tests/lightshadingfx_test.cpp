#include "lightshadingfx.cpp"
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

void setDouble(LightShadingFx& fx, const char* name, double value) {
  auto* param = dynamic_cast<TDoubleParam*>(fx.getParams()->getParam(name));
  require(param != nullptr, "Missing numeric parameter");
  param->setDefaultValue(value);
}

void setDirectional(LightShadingFx& fx) {
  auto* mode = dynamic_cast<TIntEnumParam*>(fx.getParams()->getParam("mode"));
  require(mode != nullptr, "Missing mode parameter");
  mode->setValue(LightShadingFx::DIRECTIONAL);
}

void setShadingOnly(LightShadingFx& fx) {
  setDouble(fx, "lightRadius", 0.0);
  setDouble(fx, "shadeAmount", 0.5);
  setDouble(fx, "highlightAmount", 0.0);
  auto* protect =
      dynamic_cast<TBoolParam*>(fx.getParams()->getParam("protectLines"));
  require(protect != nullptr, "Missing protectLines parameter");
  protect->setValue(false);
}

template <class Pixel>
void fillPattern(const TRasterPT<Pixel>& raster, const TPointD& origin,
                 int shift) {
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const int u = int(origin.x) + x - shift, v = int(origin.y) + y;
      const bool covered = u >= 0 && u < 42 && v >= 0 && v < 36;
      const int alpha = covered ? Pixel::maxChannelValue * (3 + u % 3) / 5 : 0;
      raster->pixels(y)[x] = Pixel(alpha * (2 + u % 4) / 6,
                                   alpha * (2 + v % 3) / 5, alpha / 2, alpha);
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
  int shift      = 0;

  SampleSource() { enableComputeInFloat(true); }

  bool doGetBBox(double, TRectD& rect, const TRenderSettings&) override {
    rect = TRectD(-100000, -100000, 100000, 100000);
    return true;
  }

  bool canHandle(const TRenderSettings&, double) override { return true; }

  std::string getAlias(double, const TRenderSettings&) const override {
    return "LightShadingTestSource" + std::to_string(patterned) +
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

FX_PLUGIN_IDENTIFIER(SampleSource, "lightShadingTestSource")

struct Fixture {
  TFxP fxOwner         = new LightShadingFx;
  LightShadingFx& fx   = *dynamic_cast<LightShadingFx*>(fxOwner.getPointer());
  SampleSource* source = new SampleSource;
  SampleSource* matte  = new SampleSource;
  TFxP sourceOwner     = source;
  TFxP matteOwner      = matte;
  TRenderSettings settings;

  Fixture() {
    fx.connect("Source", source);
    settings.m_bpp = 128;
  }
};

void expectRejected(Fixture& f) {
  require(f.fx.getMemoryRequirement(TRectD(0, 0, 64, 64), 0, f.settings) == 0,
          "An invalid radius should not schedule workspace allocation");
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
  require(rejected, "Unsupported radius did not produce an actionable error");
  require(f.source->computeCalls == 0 && f.matte->computeCalls == 0,
          "Unsafe radius allocated and computed input images");
}

void testRadiusOverflow() {
  Fixture f;
  setDirectional(f.fx);
  setDouble(f.fx, "softness", std::numeric_limits<double>::max());
  expectRejected(f);
}

void testRadiusBoundary() {
  Fixture f;
  setDouble(f.fx, "softness", 64.0);
  TRectD request(0, 0, 8, 8);
  f.fx.doDryCompute(request, 0, f.settings);
  require(f.source->lastDryRect == request.enlarge(128),
          "Ordinary radial radius changed");
  setDirectional(f.fx);
  f.fx.doDryCompute(request, 0, f.settings);
  require(f.source->lastDryRect == request.enlarge(273),
          "Ordinary directional radius changed");
  f.settings.m_maxTileSize = 2;
  expectRejected(f);
  f.settings.m_maxTileSize = std::numeric_limits<int>::max();
  setDouble(f.fx, "softness", 1000.0);
  expectRejected(f);
  setDouble(f.fx, "softness", std::numeric_limits<double>::infinity());
  expectRejected(f);
  setDouble(f.fx, "softness", std::numeric_limits<double>::quiet_NaN());
  expectRejected(f);
}

void testMemoryEstimate() {
  Fixture f;
  setDirectional(f.fx);
  setDouble(f.fx, "softness", 16.0);
  const TRectD output(0, 0, 1024, 768);
  TRectD request = output;
  f.fx.doDryCompute(request, 0, f.settings);
  const double area =
      f.source->lastDryRect.getLx() * f.source->lastDryRect.getLy();
  for (int bpp : {32, 64, 128}) {
    f.settings.m_bpp = bpp;
    f.fx.disconnect("Matte");
    const int single = f.fx.getMemoryRequirement(output, 0, f.settings);
    const int minimumSingle =
        int(std::ceil(area * (bpp / 8 + 4 * sizeof(float)) / 1048576));
    require(single >= minimumSingle,
            "Source plus live float planes are underestimated");
    f.fx.connect("Matte", f.matte);
    const int both = f.fx.getMemoryRequirement(output, 0, f.settings);
    const int minimumBoth =
        int(std::ceil(area * (2 * (bpp / 8) + 4 * sizeof(float)) / 1048576));
    require(both >= minimumBoth,
            "Source, matte and live float planes are underestimated");
    require(both > single,
            "Connecting the matte does not increase the memory estimate");
  }
  require(f.fx.getMemoryRequirement(TRectD(), 0, f.settings) == 0,
          "An empty request should not allocate shading buffers");
  setDouble(f.fx, "softness", 64.0);
  const int tiny = f.fx.getMemoryRequirement(TRectD(0, 0, 1, 1), 0, f.settings);
  require(tiny > 0 && tiny <= 256,
          "Small-tile halo exceeds its supported memory budget");
}

void testHdrPassThrough() {
  Fixture f;
  setDouble(f.fx, "shadeAmount", 0.0);
  setDouble(f.fx, "highlightAmount", 0.0);
  f.fx.connect("Matte", f.matte);
  TRasterFP output(3, 2);
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  for (int y = 0; y < output->getLy(); ++y)
    for (int x = 0; x < output->getLx(); ++x)
      require(
          std::memcmp(&output->pixels(y)[x], &f.source->sample,
                      sizeof(TPixelF)) == 0,
          "Zero shading changed HDR RGB or alpha through TRasterFx::compute");
  require(f.matte->computeCalls == 0,
          "Pass-through unnecessarily computed the matte");
}

void testNoOpExtremeRadius() {
  Fixture f;
  setDouble(f.fx, "shadeAmount", 0.0);
  setDouble(f.fx, "highlightAmount", 0.0);
  setDouble(f.fx, "softness", std::numeric_limits<double>::max());
  f.fx.connect("Matte", f.matte);
  TRectD request(0, 0, 1, 1);
  f.fx.doDryCompute(request, 0, f.settings);
  require(f.source->dryCalls == 1 && f.source->lastDryRect == request,
          "Disabled shading enlarged or skipped the source dry request");
  require(f.matte->dryCalls == 0, "Disabled shading dry-computed the matte");
  TRasterFP output(1, 1);
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  require(
      std::memcmp(output->pixels(0), &f.source->sample, sizeof(TPixelF)) == 0,
      "Disabled shading did not bypass an extreme radius");
  require(f.matte->computeCalls == 0, "Disabled shading computed the matte");
  const int memory =
      f.fx.getMemoryRequirement(TRectD(0, 0, 1, 1), 0, f.settings);
  require(memory <= 1, "Disabled shading estimated a padded tile");
}

void testActiveFloat() {
  Fixture f;
  setShadingOnly(f.fx);
  TRasterFP output(2, 2);
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  const TPixelF actual = output->pixels(0)[0];
  const TPixelF input  = f.source->sample;
  require(actual.m == input.m, "Active shading changed float alpha");
  require(std::abs(actual.r - input.r * (0.5 + 0.5 * 90.0 / 255)) < 1e-6,
          "HDR red shading differs from the multiply operation");
  require(std::abs(actual.g - input.g * (0.5 + 0.5 * 80.0 / 255)) < 1e-6,
          "Negative green shading was clipped");
  require(std::abs(actual.b - input.b * (0.5 + 0.5 * 140.0 / 255)) < 1e-6,
          "Float blue shading differs from the multiply operation");
}

void testIntegerShading() {
  Fixture f;
  setShadingOnly(f.fx);
  f.settings.m_bpp = 32;
  TRaster32P output(2, 2);
  TTile tile(output);
  f.fx.compute(tile, 0, f.settings);
  const TPixel32 actual = output->pixels(0)[0];
  require(actual == TPixel32(68, 53, 31, 128), "8-bit shading output changed");
  f.settings.m_bpp = 64;
  TRaster64P output64(2, 2);
  TTile tile64(output64);
  f.fx.compute(tile64, 0, f.settings);
  const TPixel64 actual64 = output64->pixels(0)[0];
  require(actual64 == TPixel64(33824, 26275, 15490, 60000),
          "16-bit shading output changed");
}

template <class Pixel>
uint64_t hashPixels(const TRasterPT<Pixel>& raster) {
  uint64_t hash = 14695981039346656037ULL;
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const Pixel& pixel = raster->pixels(y)[x];
      // Canonical BGRM bytes keep the original goldens independent of pixel
      // ABI.
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

uint64_t renderIntegerCase(int bpp, bool directional, bool matte, int softness,
                           std::ostream* pixels) {
  Fixture f;
  f.settings.m_bpp    = bpp;
  f.source->patterned = f.matte->patterned = true;
  f.matte->shift                           = 7;
  if (directional) setDirectional(f.fx);
  if (matte) f.fx.connect("Matte", f.matte);
  setDouble(f.fx, "softness", softness);
  setDouble(f.fx, "lightRadius", 35.0);
  TRasterP output =
      bpp == 32 ? TRasterP(TRaster32P(55, 49)) : TRasterP(TRaster64P(55, 49));
  TTile tile(output, TPointD(-7, -6));
  f.fx.compute(tile, 0, f.settings);
  if (pixels) {
    for (int y = 0; y < output->getLy(); ++y) {
      const unsigned char* row =
          output->getRawData() + y * output->getWrap() * (bpp / 8);
      pixels->write(reinterpret_cast<const char*>(row),
                    output->getLx() * (bpp / 8));
    }
  }
  return bpp == 32 ? hashPixels<TPixel32>(TRaster32P(output))
                   : hashPixels<TPixel64>(TRaster64P(output));
}

void printIntegerMatrix(std::ostream* pixels) {
  const uint64_t expected[] = {
      0xa3816935a446bbc2ULL, 0x9d7c8edb05424396ULL, 0x988624e83bb74f91ULL,
      0x758de92f0e3f44eaULL, 0x7a64a53e2e71c039ULL, 0x9869fde70f33daeaULL,
      0x2c2ea5f6f01e6976ULL, 0xd3950f45ab595db1ULL, 0xffd3c15a69c1c44eULL,
      0x71ff6dc0e8e3fb06ULL, 0xc748885ce8c8f65fULL, 0xa6ee4c091b8738a9ULL,
      0xf4236c85562f15e4ULL, 0xdcd1f9de8e579ee4ULL, 0xcb06fd9473c7de7fULL,
      0x34007116be78c9ffULL};
  size_t caseIndex = 0;
  for (int bpp : {32, 64}) {
    for (bool directional : {false, true}) {
      for (bool matte : {false, true}) {
        for (int softness : {0, 3}) {
          const uint64_t hash =
              renderIntegerCase(bpp, directional, matte, softness, pixels);
          std::cout << bpp << ',' << directional << ',' << matte << ','
                    << softness << ':' << std::hex << hash << std::dec << '\n';
          require(hash == expected[caseIndex++],
                  "Integer rendering differs from original PR");
        }
      }
    }
  }
}

#ifdef LIGHT_SHADING_TEST_PADDING
void testPaddedDimensions() {
  const int maximum = std::numeric_limits<int>::max();
  require(paddedTileSize(TDimension(maximum - 34, 1), 17) ==
              TDimension(maximum, 35),
          "The largest representable padded width was rejected");
  require(paddedTileSize(TDimension(1, maximum - 34), 17) ==
              TDimension(35, maximum),
          "The largest representable padded height was rejected");
  for (const TDimension& size :
       {TDimension(maximum - 33, 1), TDimension(1, maximum - 33)}) {
    bool rejected = false;
    try {
      paddedTileSize(size, 17);
    } catch (const TException&) {
      rejected = true;
    }
    require(rejected,
            "Padded dimensions overflowed instead of rejecting the request");
  }
  bool rejected = false;
  try {
    paddedTileSize(TDimension(1, 1), maximum);
  } catch (const TException&) {
    rejected = true;
  }
  require(rejected, "Extreme padding overflowed before the dimension check");
}
#endif

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  RenderContext context;
  if (argc != 2 && argc != 3) return 2;
  const std::string name(argv[1]);
  const std::map<std::string, void (*)()> cases = {
      {"radius-overflow", testRadiusOverflow},
      {"radius-boundary", testRadiusBoundary},
      {"memory", testMemoryEstimate},
      {"hdr", testHdrPassThrough},
      {"no-op", testNoOpExtremeRadius},
      {"active-float", testActiveFloat},
      {"integer", testIntegerShading},
#ifdef LIGHT_SHADING_TEST_PADDING
      {"padding", testPaddedDimensions},
#endif
  };
  try {
    if (name == "integer-matrix") {
      std::ofstream pixels;
      if (argc == 3) {
        pixels.open(argv[2], std::ios::binary);
        require(bool(pixels), "Cannot open pixel comparison output");
      }
      printIntegerMatrix(argc == 3 ? &pixels : nullptr);
      require(bool(pixels), "Cannot write pixel comparison output");
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
