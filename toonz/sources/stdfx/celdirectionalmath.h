#pragma once
#include "celmotion.h"
#include "celnoise.h"

namespace CelDirectional {
struct Side {
  double strength  = 0;
  double edgeFade  = 0;
  double sharpTail = 0;
};
struct Settings {
  double angle          = 0;  // Radians, counterclockwise.
  double gain           = 1;
  double sizeVariation  = 0;
  double noiseVariation = 0;
  double scale          = 1;
  Side front, back;
  CelNoise::Settings noise;
};
// Output geometry is supplied by the caller; all analysis uses the complete
// canonical source. Samples are anchored to source pixels, independent of
// tiles.
bool render(const CelMotion::Image &source, const CelMotion::Shapes &shapes,
            const CelMotion::Image *noise, CelMotion::Image &output,
            const Settings &settings, const int *canceled = nullptr);
}  // namespace CelDirectional
