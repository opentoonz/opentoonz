#pragma once

#ifndef igs_motion_blur_h
#define igs_motion_blur_h

#ifndef IGS_MOTION_BLUR_EXPORT
#define IGS_MOTION_BLUR_EXPORT
#endif

namespace igs {
namespace motion_blur {
IGS_MOTION_BLUR_EXPORT void convert(
    const unsigned char *image_in, unsigned char *image_out,

    const int height, const int width, const int channels, const int bits,

    const double x_vector = 1.0, const double y_vector = 1.0,
    const double vector_scale = 1.0, const double curve = 1.0,
    const int zanzo_length = 0.0, const double zanzo_power = 1.0,
    const bool alpha_rend_sw = true);
}
}

#endif /* !igs_motion_blur_h */
