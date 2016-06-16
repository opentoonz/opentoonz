#pragma once

#ifndef igs_level_auto_h
#define igs_level_auto_h

#ifndef IGS_LEVEL_AUTO_EXPORT
#define IGS_LEVEL_AUTO_EXPORT
#endif

namespace igs {
namespace level_auto {
IGS_LEVEL_AUTO_EXPORT void change(unsigned char *image_array,

                                  const int height, const int width,
                                  const int channels, const int bits,

                                  const bool *act_sw,  // true(false/true)
                                  const double *in_min_shift,  // 0(-1...1)
                                  const double *in_max_shift,  // 0(-1...1)
                                  const double *out_min,       // 0(0...1)
                                  const double *out_max,       // 1(0...1)
                                  const double *gamma          // 1(0.01...100)
                                  );
}
}

#endif /* !igs_level_auto_h */
