#pragma once

#ifndef igs_rotate_blur_h
#define igs_rotate_blur_h

#ifndef IGS_ROTATE_BLUR_EXPORT
#define IGS_ROTATE_BLUR_EXPORT
#endif

namespace igs {
namespace rotate_blur {
IGS_ROTATE_BLUR_EXPORT void convert(
    const unsigned char *in, const int margin /* 参照画像(in)がもつ余白 */

    ,
    const unsigned char *ref /* outと同じ高さ、幅、チャンネル数 */
    ,
    const int ref_bits, const int ref_mode  // R,G,B,A,luminance

    ,
    unsigned char *out

    ,
    const int height /* 求める画像(out)の高さ */
    ,
    const int width /* 求める画像(out)の幅 */
    ,
    const int channels, const int bits

    ,
    const double cx, const double cy,
    const double degree = 30.0 /* ぼかしの回転角度 */
    ,
    const double blur_radius = 0.0 /* ぼかしの始まる半径 */
    ,
    const double spin_radius = 0.0 /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    ,
    const int sub_div = 4 /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw = true);
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
    const int height /* 求める画像(out)の高さ */
    ,
    const int width /* 求める画像(out)の幅 */
    ,
    const double xc, const double yc, const double degree /* ぼかしの回転角度 */
    ,
    const double blur_radius /* ぼかしの始まる半径 */
    ,
    const double spin_radius /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    ,
    const int sub_div /* 1ならJaggy、2以上はAntialias */
    );
}
}

#endif /* !igs_rotate_blur_h */
