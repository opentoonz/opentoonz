#pragma once

#ifndef igs_hsv_adjust_h
#define igs_hsv_adjust_h

#ifndef IGS_HSV_ADJUST_EXPORT
#define IGS_HSV_ADJUST_EXPORT
#endif

namespace igs {
namespace hsv_adjust {
IGS_HSV_ADJUST_EXPORT void change(
    unsigned char *image_array, const int height, const int width,
    const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、channels数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double hue_pivot /* 0.0  ...0...360... */
    ,
    const double hue_scale /* 1.0  ...1...       */
    ,
    const double hue_shift /* 0.0  ...0...360... */
    ,
    const double sat_pivot /* 0.0  ...0...1...   */
    ,
    const double sat_scale /* 1.0  ...1...       */
    ,
    const double sat_shift /* 0.0  ...0...1...   */
    ,
    const double val_pivot /* 0.0  ...0...1...   */
    ,
    const double val_scale /* 1.0  ...1...       */
    ,
    const double val_shift /* 0.0  ...0...1...   */

    ,
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
}

#endif /* !igs_hsv_adjust_h */
