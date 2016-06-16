#pragma once

#ifndef igs_gaussian_blur_hv_h
#define igs_gaussian_blur_hv_h

#ifndef IGS_GAUSSIAN_BLUR_HV_EXPORT
#define IGS_GAUSSIAN_BLUR_HV_EXPORT
#endif

namespace igs {
namespace gaussian_blur_hv {
IGS_GAUSSIAN_BLUR_HV_EXPORT const int int_radius(const double real_radius);
IGS_GAUSSIAN_BLUR_HV_EXPORT const int buffer_bytes(const int height_with_margin,
                                                   const int width_with_margin,
                                                   const int int_radius);
IGS_GAUSSIAN_BLUR_HV_EXPORT void convert(
    /* 入出力画像 */
    const void *in_with_margin, void *out_no_margin

    ,
    const int height_with_margin, const int width_with_margin,
    const int channels, const int bits

    /* Pixel毎に効果の強弱 */
    ,
    const unsigned char *ref /* 求める画像(out)と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    /* 計算バッファ */
    ,
    void *buffer, int buffer_bytes
    // Must be igs::gaussian_blur_hv::buffer_bytes(-)

    /* Action Geometry */
    ,
    const int int_radius  // margin
    // Must be igs::gaussian_blur_hv::int_radius(real_radius)

    ,
    const double real_radius, const double sigma = 0.25);
}
}

#endif /* !igs_gaussian_blur_hv_h */
