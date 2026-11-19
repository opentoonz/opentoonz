#include "celradialblurfx.cpp"
#include "trenderer.h"
#include "tthread.h"

#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class RenderContext {
  TRenderer m_renderer;
  unsigned long m_id = TRenderer::buildRenderId();

public:
  RenderContext() {
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

template <class Pixel>
void fillSource(const TRasterPT<Pixel>& raster, const TPointD& origin) {
  raster->clear();
  for (int y = 0; y < raster->getLy(); ++y) {
    for (int x = 0; x < raster->getLx(); ++x) {
      const double u = origin.x + x, v = origin.y + y;
      if ((u - 8) * (u - 8) + (v - 8) * (v - 8) > 36) continue;
      raster->pixels(y)[x] = PixelConverter<Pixel>::from(
          TPixelD(0.25 + u / 50, 0.125 + v / 50, 0.1, 0.75));
    }
  }
}

class Source final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Source)

public:
  Source() { enableComputeInFloat(true); }
  bool canHandle(const TRenderSettings&, double) override { return true; }
  bool doGetBBox(double, TRectD& box, const TRenderSettings&) override {
    box = TRectD(0, 0, 16, 16);
    return true;
  }
  void doCompute(TTile& tile, double, const TRenderSettings&) override {
    if (TRaster32P raster = tile.getRaster())
      fillSource<TPixel32>(raster, tile.m_pos);
    else if (TRaster64P raster = tile.getRaster())
      fillSource<TPixel64>(raster, tile.m_pos);
    else if (TRasterFP raster = tile.getRaster())
      fillSource<TPixelF>(raster, tile.m_pos);
  }
};
FX_PLUGIN_IDENTIFIER(Source, "celRadialTestSource")

void setNumber(CelRadialBlurFx& fx, const char* name, double value) {
  auto* param = dynamic_cast<TDoubleParam*>(fx.getParams()->getParam(name));
  require(param != nullptr, "missing radial parameter");
  param->setDefaultValue(value);
}

template <class Pixel>
TRasterPT<Pixel> render(CelRadialBlurFx& fx, const TRectD& area, int bpp) {
  TRasterPT<Pixel> raster(int(area.getLx()), int(area.getLy()));
  TTile tile(raster, area.getP00());
  TRenderSettings settings;
  settings.m_bpp = bpp;
  fx.doCompute(tile, 0, settings);
  return raster;
}

template <class Pixel>
void tileTest(int bpp, int rotation) {
  RenderContext context;
  TFxP source = new Source;
  TFxP owner  = new CelRadialBlurFx;
  auto& fx    = *static_cast<CelRadialBlurFx*>(owner.getPointer());
  require(fx.connect("Source", source.getPointer()),
          "source connection failed");
  setNumber(fx, "outerStrength", 4.25);
  setNumber(fx, "innerStrength", 2.5);
  setNumber(fx, "ellipseRatio", 1.75);
  setNumber(fx, "blurType", rotation);
  setNumber(fx, "outerFade", 45);
  setNumber(fx, "innerFade", 65);
  setNumber(fx, "noiseVariation", 30);
  const auto whole = render<Pixel>(fx, TRectD(-4, -4, 20, 20), bpp);
  bool changed     = false;
  TRasterPT<Pixel> original(24, 24);
  fillSource<Pixel>(original, TPointD(-4, -4));
  for (int y = 0; y < 24; ++y)
    for (int x = 0; x < 24; ++x)
      changed = changed || whole->pixels(y)[x] != original->pixels(y)[x];
  require(changed, "active radial blur did not change the drawing");
  for (int top : {0, 12}) {
    for (int left : {0, 12}) {
      const auto tile =
          render<Pixel>(fx, TRectD(left - 4, top - 4, left + 8, top + 8), bpp);
      for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x)
          require(whole->pixels(top + y)[left + x] == tile->pixels(y)[x],
                  "radial blur changed when the frame was split into tiles");
    }
  }
  setNumber(fx, "outerStrength", 0);
  setNumber(fx, "innerStrength", 0);
  const auto unchanged = render<Pixel>(fx, TRectD(-4, -4, 20, 20), bpp);
  for (int y = 0; y < 24; ++y)
    for (int x = 0; x < 24; ++x)
      require(unchanged->pixels(y)[x] == original->pixels(y)[x],
              "zero blur changed the drawing");
}

void allocationTest() {
  require(CelRadial::fitsRasterAllocation(1, 1, 16), "valid pixel rejected");
  require(!CelRadial::fitsRasterAllocation(0, 1, 16), "zero width accepted");
  require(!CelRadial::fitsRasterAllocation(100000, 100000, 16),
          "overflowing raster accepted");
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  try {
    for (int rotation : {0, 1}) {
      tileTest<TPixel32>(32, rotation);
      tileTest<TPixel64>(64, rotation);
      tileTest<TPixelF>(128, rotation);
    }
    allocationTest();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Radial Blur native raster and allocation checks passed\n";
}
