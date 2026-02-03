#pragma once

#ifndef igs_hls_adjust_h
#define igs_hls_adjust_h

#ifndef IGS_HLS_ADJUST_EXPORT
#define IGS_HLS_ADJUST_EXPORT
#endif

namespace igs {
namespace hls_adjust {
IGS_HLS_ADJUST_EXPORT void change(
    float *image_array, const int height, const int width, const int channels,
    const float *ref, /* 求める画像と同じ高、幅、channels数 */
    const double hue_pivot, /* 0.0  ...0...360... */
    const double hue_scale, /* 1.0  ...1...       */
    const double hue_shift, /* 0.0  ...0...360... */
    const double lig_pivot, /* 0.0  ...0...1...   */
    const double lig_scale, /* 1.0  ...1...       */
    const double lig_shift, /* 0.0  ...0...1...   */
    const double sat_pivot, /* 0.0  ...0...1...   */
    const double sat_scale, /* 1.0  ...1...       */
    const double sat_shift, /* 0.0  ...0...1...   */
    const bool add_blend_sw,
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
    const bool cylindrical = true  // colorspace shape: cylindrical or bicone
);
}
}  // namespace igs

#endif /* !igs_hls_adjust_h */
