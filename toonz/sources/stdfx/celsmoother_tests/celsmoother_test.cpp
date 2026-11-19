#include "celsmoother2fx.cpp"

#include <QCoreApplication>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class SmootherSource final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(SmootherSource)

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
FX_PLUGIN_IDENTIFIER(SmootherSource, "smootherRegressionSource")

struct Fixture {
  TRasterFxP sourceOwner = new SmootherSource;
  TRasterFxP effectOwner = new CelSmoother2Fx;
  SmootherSource& source =
      *static_cast<SmootherSource*>(sourceOwner.getPointer());
  CelSmoother2Fx& effect =
      *static_cast<CelSmoother2Fx*>(effectOwner.getPointer());
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
    tile.setRaster(source.pixels->create(width, 12));
    effect.doCompute(tile, 0, settings);
    return tile.getRaster();
  }
};

template <class Pixel>
void tiles() {
  Fixture f;
  TRasterPT<Pixel> input(8, 8);
  const double scale = pixelScale<Pixel>();
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      input->pixels(y)[x] =
          x < y ? Pixel(scale, 0, 0, scale) : Pixel(0, 0, scale, scale);
  f.source.pixels = input;
  for (int version : {0, 1}) {
    f.set("smootherVersion", version);
    for (int strength : {0, 100}) {
      f.set("smoothness", strength);
      TRasterPT<Pixel> whole = f.render(-2, 12);
      int mixed              = 0;
      for (int x = -2; x < 10; x += 3) {
        TRasterPT<Pixel> strip = f.render(x, 3);
        for (int y = 0; y < 12; ++y)
          for (int i = 0; i < 3; ++i) {
            const Pixel a = whole->pixels(y)[x + 2 + i];
            const Pixel b = strip->pixels(y)[i];
            require(a == b, "split tiles differ from whole-frame smoothing");
            if (a.r > 0 && a.b > 0) ++mixed;
            if (strength != 0) continue;
            const int sx = x + i, sy = y - 2;
            const Pixel original = sx >= 0 && sx < 8 && sy >= 0 && sy < 8
                                       ? input->pixels(sy)[sx]
                                       : Pixel::Transparent;
            require(a == original, "zero smoothing changed input pixels");
          }
      }
      if (strength > 0)
        require(mixed > 0, "active smoothing did not mix staircase colors");
    }
  }
}

void keying() {
  Fixture f;
  TRasterFP input(2, 1);
  input->pixels(0)[0] = TPixelF(0.5f, 0.5f, 0.5f, 0.5f);
  input->pixels(0)[1] = TPixelF(2, -0.25f, 0.125f, 1);
  f.source.pixels     = input;
  f.set("smoothness", 0);
  f.set("useColorKey", 1);
  TRasterFP keyed = f.render(0, 2);
  require(keyed->pixels(2)[0] == TPixelF::Transparent,
          "white key did not match partial coverage in straight RGB");
  require(keyed->pixels(2)[1] == input->pixels(0)[1],
          "keying changed unmatched HDR color");
  f.set("invertColorKey", 1);
  TRasterFP inverted = f.render(0, 2);
  require(inverted->pixels(2)[0] == input->pixels(0)[0],
          "inverted key removed matched color");
  require(inverted->pixels(2)[1] == TPixelF::Transparent,
          "inverted key retained unmatched color");
}

void gamma() {
  CelSmoother2::Settings settings;
  const SmoothPixel black{0, 0, 0, 1}, white{1, 1, 1, 1};
  settings.version   = 0;
  const auto encoded = CelSmoother2::mix(black, white, 0.5, settings);
  require(encoded.r == 0.5 && encoded.a == 1, "v1 blend is not encoded RGB");
  settings.version  = 1;
  const auto linear = CelSmoother2::mix(black, white, 0.5, settings);
  require(linear.r > encoded.r && linear.r < 1 && linear.a == 1,
          "v2 blend did not use linear light");
  settings.gammaMode  = 1;
  settings.gammaCount = 0;
  require(CelSmoother2::mix(black, white, 0.5, settings).r == linear.r,
          "zero gamma colors changed normal blend");
  settings.gammaCount = 1;
  settings.colors[0]  = black;
  require(CelSmoother2::mix(black, white, 0.5, settings).r < linear.r,
          "selected gamma color did not affect edge blend");
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
  try {
    require(argc == 2, "expected test case");
    const std::string name(argv[1]);
    if (name == "tiles") {
      tiles<TPixel32>();
      tiles<TPixel64>();
      tiles<TPixelF>();
    } else if (name == "keying")
      keying();
    else if (name == "gamma")
      gamma();
    else if (name == "canceled" || name == "missing")
      canceled(name == "missing");
    else
      throw std::runtime_error("unknown test case");
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Smoother v2 checks passed\n";
  return 0;
}
