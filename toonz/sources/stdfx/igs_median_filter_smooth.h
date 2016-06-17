#pragma once

#ifndef igs_median_filter_smooth_h
#define igs_median_filter_smooth_h

#ifndef IGS_MEDIAN_FILTER_SMOOTH_EXPORT
#define IGS_MEDIAN_FILTER_SMOOTH_EXPORT
#endif

namespace igs {
namespace median_filter_smooth {
IGS_MEDIAN_FILTER_SMOOTH_EXPORT void convert(
    const unsigned char *in_image, unsigned char *out_image

    ,
    const int height, const int width, const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、channels数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const int zz /* =0	0(R),1(G),2(B),3(A),4(EachCh) */
    ,
    const double radius /* =1.7	0... */
    ,
    const int out_side_type /* =0	0(Spread),1(Flip),2(bk),3(Repeat) */
    /* 2013-11-11現在0(Spread)のみ使用 */
    );
/*
        2013-11-13 igs::median_filter::convert(-)に比べて11倍遅い
  */
}
}

#endif /* !igs_median_filter_smooth_h */
