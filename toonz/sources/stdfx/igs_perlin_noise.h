#pragma once

#ifndef igs_perlin_noise_h
#define igs_perlin_noise_h

#ifndef IGS_PERLIN_NOISE_EXPORT
#define IGS_PERLIN_NOISE_EXPORT
#endif

namespace igs {
namespace perlin_noise {
IGS_PERLIN_NOISE_EXPORT void change(
    unsigned char *image_array, const int height  // pixel
    ,
    const int width  // pixel
    ,
    const int channels, const int bits, const bool alpha_rendering_sw = true

    ,
    const double a11 = 0.01  // 2D affine transformation
    ,
    const double a12 = 0.0, const double a13 = 0.0, const double a21 = 0.0,
    const double a22 = 0.01, const double a23 = 0.0

    ,
    const double zz = 0.0, const int octaves_start = 3  // 0...
    ,
    const int octaves_end = 9  // 0...
    ,
    const double persistence = 1. / 1.7320508  // not 0
    );
}
}

#endif /* !igs_perlin_noise_h */
