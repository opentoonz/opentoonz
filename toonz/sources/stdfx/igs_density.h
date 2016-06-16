#pragma once

#ifndef igs_density_h
#define igs_density_h

#ifndef IGS_DENSITY_EXPORT
#define IGS_DENSITY_EXPORT
#endif

namespace igs {
namespace density {
IGS_DENSITY_EXPORT void change(
    unsigned char *image_array /* RGBAでなければならない */
    ,
    const int height, const int width,
    const int channels /* 4(=RGBAでなければならない) */
    ,
    const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double density = 1.0);
}
}

#endif /* !igs_density_h */
