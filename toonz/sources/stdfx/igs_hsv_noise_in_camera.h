#pragma once

#ifndef igs_hsv_noise_in_camera_h
#define igs_hsv_noise_in_camera_h

#ifndef IGS_HSV_NOISE_IN_CAMERA_EXPORT
#define IGS_HSV_NOISE_IN_CAMERA_EXPORT
#endif

namespace igs {
namespace hsv_noise_in_camera {
IGS_HSV_NOISE_IN_CAMERA_EXPORT void change(
    void *image_array

    ,
    const int height, const int width, const int channels, const int bits

    ,
    const int camera_x, const int camera_y, const int camera_w,
    const int camera_h

    ,
    const double hue_range = 0.025  // 0 ... 1.0
    ,
    const double sat_range = 0.0  // 0 ... 1.0
    ,
    const double val_range = 0.035  // 0 ... 1.0
    ,
    const double alp_range = 0.0  // 0 ... 1.0
    ,
    const unsigned long random_seed = 1  // 0 ... ULONG_MAX
    ,
    const double near_blur = 0.500  // 0 ... 0.5

    ,
    const double sat_effective = 0.0  // 0 ... 1.0
    ,
    const double sat_center = 0.5  // 0 ... 1.0
    ,
    const int sat_type = 0
    // 0(shift_whole),1(shift_term),2(decrease_whole),3(decrease_term)
    ,
    const double val_effective = 0.0  // 0 ... 1.0
    ,
    const double val_center = 0.5  // 0 ... 1.0
    ,
    const int val_type = 0
    // 0(shift_whole),1(shift_term),2(decrease_whole),3(decrease_term)
    ,
    const double alp_effective = 0.0  // 0 ... 1.0
    ,
    const double alp_center = 0.5  // 0 ... 1.0
    ,
    const int alp_type = 0
    // 0(shift_whole),1(shift_term),2(decrease_whole),3(decrease_term)
    );
}
}

#endif /* !igs_hsv_noise_in_camera_h */
