#include "tcolorfunctions.h"
#include "common/tvrender/patterncolorsignature.h"

#include <cmath>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

void checkOpacity() {
  const TPixel32 pixel(40, 80, 120, 200);
  for (double opacity : {0.0, 0.25, 0.5, 0.75, 1.0}) {
    TTranspFader fader(opacity);
    TColorFunction::Parameters parameters;
    require(fader.getParameters(parameters), "Opacity parameters unavailable");
    require(parameters.m_mM == opacity, "Opacity missing from parameters");
    const TPixel32 result = fader(pixel);
    require(result.m == int(opacity * pixel.m), "Incorrect faded alpha");
    require(result.r == pixel.r && result.g == pixel.g && result.b == pixel.b,
            "Opacity changed RGB");
  }
}

void checkFilters() {
  for (const TPixel32 &filter :
       {TPixel32(255, 0, 0, 64), TPixel32(0, 0, 255, 192),
        TPixel32(0, 0, 0, 255)}) {
    TColumnColorFilterFunction function(filter);
    TColorFunction::Parameters parameters;
    require(function.getParameters(parameters),
            "Filter parameters unavailable");
    require(parameters.m_cR == filter.r && parameters.m_cG == filter.g &&
                parameters.m_cB == filter.b,
            "Filter color missing from parameters");
    require(parameters.m_mM == filter.m / 255.0, "Filter opacity missing");
    for (int channel = 0; channel <= 255; ++channel) {
      const TPixel32 result =
          function(TPixel32(channel, channel, channel, channel));
      require(
          std::abs(result.r - (channel * parameters.m_mR + parameters.m_cR)) <
                  1.01 &&
              std::abs(result.g -
                       (channel * parameters.m_mG + parameters.m_cG)) < 1.01 &&
              std::abs(result.b -
                       (channel * parameters.m_mB + parameters.m_cB)) < 1.01,
          "Filter parameters do not describe RGB output");
      require(result.m == channel * filter.m / 255, "Incorrect filter alpha");
    }
  }
}

class Uncacheable final : public TColorFunction {
public:
  TPixel32 operator()(const TPixel32 &pixel) const override { return pixel; }
  TColorFunction *clone() const override { return new Uncacheable; }
  bool getParameters(Parameters &) const override { return false; }
};

void checkSignatures() {
  using PatternTextures::ColorSignature;
  TTranspFader low(0.25), high(0.75), same(0.25);
  require(!(ColorSignature(&low) == ColorSignature(&high)),
          "Opacity cache collision");
  require(ColorSignature(&low) == ColorSignature(&same),
          "Equal opacity cannot reuse texture");
  TColumnColorFilterFunction red(TPixel32(255, 0, 0, 255));
  TColumnColorFilterFunction blue(TPixel32(0, 0, 255, 255));
  require(!(ColorSignature(&red) == ColorSignature(&blue)),
          "Filter cache collision");
  const double multipliers[4] = {1, 1, 1, 0.25};
  const double constants[4]   = {0, 0, 0, 0};
  TGenericColorFunction generic(multipliers, constants);
  require(!(ColorSignature(&low) == ColorSignature(&generic)),
          "Function types share a key");
  Uncacheable unsupported;
  require(!ColorSignature(&unsupported).cacheable,
          "Unsupported function is cached");
  require(!(ColorSignature(&unsupported) == ColorSignature(&unsupported)),
          "Unsupported signatures compare equal");
  require(ColorSignature(nullptr) == ColorSignature(nullptr),
          "Untinted signature is unstable");
}

}  // namespace

int main() {
  checkOpacity();
  checkFilters();
  checkSignatures();
}
