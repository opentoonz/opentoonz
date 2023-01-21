#pragma once

#ifndef igs_level_auto_in_camera_h
#define igs_level_auto_in_camera_h

#ifndef IGS_LEVEL_AUTO_IN_CAMERA_EXPORT
#define IGS_LEVEL_AUTO_IN_CAMERA_EXPORT
#endif

namespace igs {
namespace level_auto_in_camera {
IGS_LEVEL_AUTO_IN_CAMERA_EXPORT void change(
    void *image_array, const int height, const int width, const int channels,
    const int bits,
    const bool *act_sw,          // true(false/true)
    const double *in_min_shift,  // 0(-1...1)
    const double *in_max_shift,  // 0(-1...1)
    const double *out_min,       // 0(0...1)
    const double *out_max,       // 1(0...1)
    const double *gamma,         // 1(0.01...100)
    const int camera_x, const int camera_y, const int camera_w,
    const int camera_h);
}
}  // namespace igs

#endif /* !igs_level_auto_in_camera_h */
