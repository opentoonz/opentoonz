#pragma once

#ifndef igs_hsv_add_h
#define igs_hsv_add_h

#ifndef IGS_HSV_ADD_EXPORT
#define IGS_HSV_ADD_EXPORT
#endif

namespace igs {
namespace hsv_add {
IGS_HSV_ADD_EXPORT void change(
    float *image_array, const int height, const int width, const int channels,
    const float *noi_image_array,
    const float *ref,    /* 求める画像と同じ高、幅、channels数 */
    const int xoffset,   /* 0	INT_MIN ... INT_MAX	*/
    const int yoffset,   /* 0	INT_MIN ... INT_MAX	*/
    const int from_rgba, /* 0	0(R),1(G),2(B),3(A)	*/
    const double offset, /* 0.5	-1.0 ... 1.0	*/
    const double hue_scale, /* 0.0	-1.0 ... 1.0	*/
    const double sat_scale, /* 0.0	-1.0 ... 1.0	*/
    const double val_scale, /* 1.0	-1.0 ... 1.0	*/
    const double alp_scale, /* 0.0	-1.0 ... 1.0	*/
    const bool add_blend_sw
    /* 効果(変化量)をアルファブレンドするか否かのスイッチ
                add_blend_sw == true
                    アルファ値によりRGBの変化量を調整する
                    合成方法が
                            合成画 = 下絵 * (1 - alpha) + 上絵
                    の場合こちらを使う
                add_blend_sw == false
                    アルファ値に関係なくRGBが変化する
                    合成方法が
                            合成画 = 下絵 * (1 - alpha) + 上絵 * alpha
                    の場合こちらを使う
            */
);
}
}  // namespace igs
#endif /* !igs_hsv_add_h */
