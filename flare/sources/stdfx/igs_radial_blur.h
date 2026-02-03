#pragma once

#ifndef igs_radial_blur_h
#define igs_radial_blur_h

#ifndef IGS_RADIAL_BLUR_EXPORT
#define IGS_RADIAL_BLUR_EXPORT
#endif

#include "tgeometry.h"

namespace igs {
namespace radial_blur {
IGS_RADIAL_BLUR_EXPORT void convert(
    const float* in, float* out, const int margin, /* 参照画像(in)がもつ余白 */
    const TDimension out_dim,             /* 求める画像(out)の大きさ*/
    const int channels, const float* ref, /* outと同じ高さ、幅 */
    const TPointD center, const double twist_radian = 0.0,
    const double twist_radius = 0.0,
    const double intensity    = 0.2, /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    const double radius = 0.0, /* 平均値ぼかしの始まる半径 */
    const int type = 0, const bool antialias_sw = true,
    const bool alpha_rendering_sw     = true,
    const double ellipse_aspect_ratio = 1.0, const double ellipse_angle = 0.0,
    const double intensity_correlation_with_ellipse = 0.0);
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
                 const int height, const int width, const TPointD center,
                 const double twist_radian, const double twist_radius,
                 const double intensity = 0.2, /* 強度。ゼロより大きく2以下 */
                 /* radius円境界での平均値ぼかしゼロとするためintensityは2以下
                  */
                 const double radius = 0.0, /* 平均値ぼかしの始まる半径 */
                 const int type = 0, const double ellipse_aspect_ratio = 1.0,
                 const double ellipse_angle                      = 0.0,
                 const double intensity_correlation_with_ellipse = 0.0);
}  // namespace radial_blur
}  // namespace igs

#endif /* !igs_radial_blur_h */
