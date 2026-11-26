#include "celdirectionalblurfx.cpp"

#include <QCoreApplication>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class DirectionalSource final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(DirectionalSource)

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
FX_PLUGIN_IDENTIFIER(DirectionalSource, "directionalRegressionSource")

struct Fixture {
  TRasterFxP sourceOwner = new DirectionalSource;
  TRasterFxP effectOwner = new CelDirectionalBlurFx;
  DirectionalSource& source =
      *static_cast<DirectionalSource*>(sourceOwner.getPointer());
  CelDirectionalBlurFx& effect =
      *static_cast<CelDirectionalBlurFx*>(effectOwner.getPointer());
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
    tile.m_pos = TPointD(x, -4);
    tile.setRaster(source.pixels->create(width, 16));
    effect.doCompute(tile, 0, settings);
    return tile.getRaster();
  }
};

template <class Pixel>
void tiles() {
  Fixture f;
  TRasterPT<Pixel> input(8, 8);
  input->clear();
  const double scale = directionalChannelScale<Pixel>();
  for (int y = 2; y < 6; ++y)
    for (int x = 2; x < 6; ++x)
      input->pixels(y)[x] =
          x < 4 ? Pixel(scale, 0, 0, scale) : Pixel(0, 0, scale / 2, scale / 2);
  f.source.pixels = input;
  f.set("frontStrength", 3.5);
  f.set("backStrength", 2.25);
  f.set("angle", 33);
  for (int noise : {0, 1, 2}) {
    f.set("noiseType", noise);
    f.set("noiseVariation", noise ? 50 : 0);
    if (noise == 2)
      require(f.effect.connect("Noise", &f.source), "connect Noise failed");
    TRasterPT<Pixel> whole = f.render(-4, 16);
    int extended           = 0;
    for (int x = -4; x < 12; x += 4) {
      TRasterPT<Pixel> strip = f.render(x, 4);
      for (int y = 0; y < 16; ++y)
        for (int i = 0; i < 4; ++i) {
          const Pixel a = whole->pixels(y)[x + 4 + i];
          const Pixel b = strip->pixels(y)[i];
          require(std::abs(double(a.r) - b.r) <= 1e-6 &&
                      std::abs(double(a.g) - b.g) <= 1e-6 &&
                      std::abs(double(a.b) - b.b) <= 1e-6 &&
                      std::abs(double(a.m) - b.m) <= 1e-6,
                  "split tiles differ from whole-frame directional blur");
          const int sx = x + i, sy = y - 4;
          if ((sx < 2 || sx >= 6 || sy < 2 || sy >= 6) && a.m > 0) ++extended;
        }
    }
    require(extended > 0, "active directional blur did not extend the source");
  }
  f.set("frontStrength", 0);
  f.set("backStrength", 0);
  TRasterPT<Pixel> bypass = f.render(0, 8);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      require(bypass->pixels(y + 4)[x] == input->pixels(y)[x],
              "zero strengths changed the source");
}

void gain() {
  Fixture f;
  TRasterFP input(2, 1);
  input->pixels(0)[0] = TPixelF(2, -0.25f, 0.125f, 0.5f);
  input->pixels(0)[1] = TPixelF(1, 1, 1, 1);
  f.source.pixels     = input;
  f.set("gain", 2);
  TRasterFP output = f.render(0, 2);
  require(output->pixels(4)[0] == TPixelF(4, -0.5f, 0.25f, 0.5f),
          "gain changed source alpha or clipped ordinary HDR RGB");
  f.set("frontStrength", 2);
  output              = f.render(0, 6);
  const auto extended = output->pixels(4)[2];
  require(
      extended.m > 0 && std::isfinite(extended.r) && std::isfinite(extended.g),
      "HDR streak produced invalid coverage or color");
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
    } else if (name == "gain")
      gain();
    else if (name == "canceled" || name == "missing")
      canceled(name == "missing");
    else
      throw std::runtime_error("unknown test case");
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Directional Blur checks passed\n";
  return 0;
}
