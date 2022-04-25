#pragma once

#ifndef igs_rotate_blur_h
#define igs_rotate_blur_h

#ifndef IGS_ROTATE_BLUR_EXPORT
#define IGS_ROTATE_BLUR_EXPORT
#endif

#include "tgeometry.h"

namespace igs {
namespace rotate_blur {
IGS_ROTATE_BLUR_EXPORT void convert(
    const float* in, float* out, const int margin,
    const TDimension out_dim,             /* 求める画像(out)のサイズ */
    const int channels, const float* ref, /* outと同じ高さ、幅 */
    const TPointD center, const double degree = 30.0, /* ぼかしの回転角度 */
    const double blur_radius = 0.0, /* ぼかしの始まる半径 */
    const double spin_radius = 0.0, /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    const int type = 0,  // 0: Accelerator, 1: Uniform Angle, 2: Uniform Length
    const bool antialias_sw =
        true, /* when true, sampled pixel will be bilinear-interpolated */
    const bool alpha_rendering_sw     = true,
    const double ellipse_aspect_ratio = 1.0, const double ellipse_angle = 0.0);
#if 0   //------------------- comment out start ------------------------
  IGS_ROTATE_BLUR_EXPORT int enlarge_margin(
	 const int height	/* 求める画像(out)の高さ */
	,const int width	/* 求める画像(out)の幅 */
	,const double xc
	,const double yc
	,const double degree	/* ぼかしの回転角度 */
	,const double blur_radius	/* ぼかしの始まる半径 */
	,const double spin_radius	/* ゼロ以上でspin指定となり、
				かつぼかし強弱の一定になる半径となる */
	,const int sub_div	/* 1ならJaggy、2以上はAntialias */
  );
#endif  //------------------- comment out end -------------------------
IGS_ROTATE_BLUR_EXPORT int reference_margin(
    const int height, /* 求める画像(out)の高さ */
    const int width,  /* 求める画像(out)の幅 */
    const TPointD center, const double degree, /* ぼかしの回転角度 */
    const double blur_radius,                  /* ぼかしの始まる半径 */
    const double spin_radius, /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    const int type  // 0: Accelerator, 1: Uniform Angle, 2: Uniform Length
);
}  // namespace rotate_blur
}  // namespace igs

#endif /* !igs_rotate_blur_h */
