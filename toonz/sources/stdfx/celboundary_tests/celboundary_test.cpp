#include "celboundaryblurfx.cpp"
#include "trenderer.h"
#include "tthread.h"

#include <QCoreApplication>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class BoundarySource final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(BoundarySource)

public:
  TRasterP pixels;
  bool doGetBBox(double, TRectD& box, const TRenderSettings&) override {
    box = TRectD(0, 0, pixels->getLx(), pixels->getLy());
    return true;
  }
  bool canHandle(const TRenderSettings&, double) override { return true; }
  void compute(TTile& tile, double frame,
               const TRenderSettings& settings) override {
    doCompute(tile, frame, settings);
  }
  void doCompute(TTile& tile, double, const TRenderSettings&) override {
    tile.getRaster()->clear();
    tile.getRaster()->copy(pixels,
                           TPoint(-int(tile.m_pos.x), -int(tile.m_pos.y)));
  }
};
FX_PLUGIN_IDENTIFIER(BoundarySource, "boundaryRegressionSource")

struct Fixture {
  TRasterFxP sourceOwner = new BoundarySource;
  TRasterFxP effectOwner = new CelBoundaryBlurFx;
  BoundarySource& source =
      *static_cast<BoundarySource*>(sourceOwner.getPointer());
  CelBoundaryBlurFx& effect =
      *static_cast<CelBoundaryBlurFx*>(effectOwner.getPointer());
  TRenderSettings settings;

  Fixture() {
    require(effect.connect("Source", &source), "connect Source failed");
  }

  void set(const char* name, double value) {
    auto* param =
        dynamic_cast<TDoubleParam*>(effect.getParams()->getParam(name));
    require(param != nullptr, "missing effect parameter");
    param->setDefaultValue(value);
  }

  TRasterP render(int x, int width) {
    TTile tile;
    tile.m_pos = TPointD(x, -2);
    tile.setRaster(source.pixels->create(width, 10));
    effect.doCompute(tile, 0, settings);
    return tile.getRaster();
  }
};

template <class Pixel>
void tiles() {
  Fixture f;
  TRasterPT<Pixel> input(8, 6);
  input->clear();
  const double scale = blurChannelScale<Pixel>();
  for (int y = 1; y < 5; ++y)
    for (int x = 1; x < 7; ++x) {
      const double alpha  = y == 1 ? 0.5 : 1;
      input->pixels(y)[x] = Pixel(x < 4 ? scale * alpha : 0, 0,
                                  x < 4 ? 0 : scale * alpha, scale * alpha);
    }
  f.source.pixels = input;
  f.set("amount", 1.25);
  for (int legacy : {0, 1}) {
    f.set("legacy", legacy);
    for (int repeats : {0, 1, 3}) {
      f.set("repeats", repeats);
      TRasterPT<Pixel> whole = f.render(-2, 12);
      if (repeats > 0) {
        const Pixel boundary = whole->pixels(5)[5];
        require(boundary.r > 0 && boundary.b > 0,
                "active blur did not mix the neighboring covered colors");
      }
      for (int x = -2; x < 10; x += 3) {
        TRasterPT<Pixel> strip = f.render(x, 3);
        for (int y = 0; y < 10; ++y)
          for (int i = 0; i < 3; ++i) {
            const Pixel a = whole->pixels(y)[x + 2 + i];
            const Pixel b = strip->pixels(y)[i];
            require(std::abs(double(a.r) - b.r) <= 1e-6 &&
                        std::abs(double(a.g) - b.g) <= 1e-6 &&
                        std::abs(double(a.b) - b.b) <= 1e-6 && a.m == b.m,
                    "split tiles differ from whole-frame blur");
            const int sx = x + i, sy = y - 2;
            const Pixel original = sx >= 0 && sx < 8 && sy >= 0 && sy < 6
                                       ? input->pixels(sy)[sx]
                                       : Pixel::Transparent;
            require(a.m == original.m, "blur changed the source silhouette");
            if (repeats == 0)
              require(a == original, "zero repeats changed source pixels");
          }
      }
    }
  }
}

void hdr() {
  Fixture f;
  TRasterFP input(2, 1);
  input->pixels(0)[0] = TPixelF(1e30f, -1e30f, 1, 1e-20f);
  input->pixels(0)[1] = TPixelF(1, 1, 1, 1);
  f.source.pixels     = input;
  f.set("legacy", 1);
  f.set("repeats", 1);
  TRasterFP output = f.render(0, 2);
  const auto pixel = output->pixels(2)[1];
  require(std::isfinite(pixel.r) && std::isfinite(pixel.g) &&
              std::isfinite(pixel.b),
          "finite HDR source produced nonfinite float output");
  require(pixel.r > 1 && pixel.g < 0 && pixel.m == 1,
          "HDR saturation lost the channel signs or original alpha");
}

void canceled(bool missing) {
  Fixture f;
  TRaster32P input(2, 1);
  input->fill(TPixel32::Red);
  f.source.pixels = input;
  int cancel      = 1;
  if (missing)
    f.effect.disconnect("Source");
  else
    f.settings.m_isCanceled = &cancel;
  TRaster32P output = f.render(0, 2);
  for (int y = 0; y < output->getLy(); ++y)
    for (int x = 0; x < output->getLx(); ++x)
      require(output->pixels(y)[x] == TPixel32::Transparent,
              "cancellation or missing source left output pixels");
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  try {
    require(argc == 2, "expected test case");
    const std::string name(argv[1]);
    if (name == "tiles") {
      tiles<TPixel32>();
      tiles<TPixel64>();
      tiles<TPixelF>();
    } else if (name == "hdr")
      hdr();
    else if (name == "canceled" || name == "missing")
      canceled(name == "missing");
    else
      throw std::runtime_error("unknown test case");
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Boundary Blur checks passed\n";
  return 0;
}
