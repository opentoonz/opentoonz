#pragma once

#ifndef igs_radial_blur_h
#define igs_radial_blur_h

#ifndef IGS_RADIAL_BLUR_EXPORT
#define IGS_RADIAL_BLUR_EXPORT
#endif

namespace igs {
namespace radial_blur {
IGS_RADIAL_BLUR_EXPORT void convert(
    const unsigned char *in /* 余白付き */
    ,
    const int margin /* 参照画像(in)がもつ余白 */

    ,
    const unsigned char *ref /* outと同じ高さ、幅、チャンネル数 */
    ,
    const int ref_bits,
    const int ref_mode  // R,G,B,A,luminance or R,G,B,luminance

    ,
    unsigned char *out /* 余白なし */

    ,
    const int height /* 求める画像(out)の高さ */
    ,
    const int width /* 求める画像(out)の幅 */
    ,
    const int channels, const int bits

    ,
    const double xc, const double yc, const double twist_radian = 0.0,
    const double twist_radius = 0.0,
    const double intensity    = 0.2 /* 強度。ゼロより大きく2以下 */
    ,
    const double radius = 0.0 /* ぼかしの始まる半径 */
    ,
    const int sub_div = 4 /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw = true);
#if 0   //-------------------- comment out start ------------------------
  IGS_RADIAL_BLUR_EXPORT int enlarge_margin( /* Twist時正確でない... */
	 const int height
	,const int width
	,const double xc
	,const double yc
	,const double twist_radian
	,const double twist_radius
	,const double intensity=0.2/* 強度。ゼロより大きく2以下 */
	/* radius円境界での平均値ぼかしゼロとするためintensityは2以下 */
	,const double radius=0.0	/* 平均値ぼかしの始まる半径 */
	,const int sub_div=4	/* 1ならJaggy、2以上はAntialias */
  );
#endif  //-------------------- comment out end -------------------------
IGS_RADIAL_BLUR_EXPORT int
reference_margin(/* Twist時正確でない... */
                 const int height, const int width, const double xc,
                 const double yc, const double twist_radian,
                 const double twist_radius,
                 const double intensity = 0.2 /* 強度。ゼロより大きく2以下 */
                 /* radius円境界での平均値ぼかしゼロとするためintensityは2以下
                    */
                 ,
                 const double radius = 0.0 /* 平均値ぼかしの始まる半径 */
                 ,
                 const int sub_div = 4 /* 1ならJaggy、2以上はAntialias */
                 );
}
}

#endif /* !igs_radial_blur_h */
