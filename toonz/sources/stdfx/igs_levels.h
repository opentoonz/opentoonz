#pragma once

#ifndef igs_levels_h
#define igs_levels_h

#ifndef IGS_LEVELS_EXPORT
#define IGS_LEVELS_EXPORT
#endif

namespace igs {
namespace levels {
IGS_LEVELS_EXPORT void change(
    unsigned char *image_array, const int height, const int width,
    const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、channels数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double r_in_min, const double r_in_max  // 0...1
    ,
    const double g_in_min, const double g_in_max  // 0...1
    ,
    const double b_in_min, const double b_in_max  // 0...1
    ,
    const double a_in_min, const double a_in_max  // 0...1
    ,
    const double r_gamma  // 0.1 ... 10.0
    ,
    const double g_gamma  // 0.1 ... 10.0
    ,
    const double b_gamma  // 0.1 ... 10.0
    ,
    const double a_gamma  // 0.1 ... 10.0
    ,
    const double r_out_min, const double r_out_max  // 0...1
    ,
    const double g_out_min, const double g_out_max  // 0...1
    ,
    const double b_out_min, const double b_out_max  // 0...1
    ,
    const double a_out_min, const double a_out_max  // 0...1

    ,
    const bool clamp_sw,
    const bool alpha_sw /* Alphaチャンネルを処理するか否かのSW */
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

#endif /* !igs_levels_h */
