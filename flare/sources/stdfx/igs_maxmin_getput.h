#pragma once

#ifndef igs_maxmin_getput_h
#define igs_maxmin_getput_h

#include <vector>
#include <limits>            /* std::numeric_limits<->::max()
		--> (std::numeric_limits<->::max)() */
#include "igs_ifx_common.h"  // igs::color::ref_value(-)

namespace {
/* 画像配列の高さ位置を、実際の範囲内にclampし、scanlineの先頭を返す */
template <class T>
const T *csl_top_clamped_in_h_(const T *top, const int height, const int width,
                               const int channels, const int yy) {
  if (height <= yy) {
    return top + (channels * width * (height - 1));
  } else if (yy < 0) {
    return top;
  }
  return top + (channels * width * yy);
}
template <class T>
T *sl_out_clamped_in_h_(T *out, const int height, const int width,
                        const int channels, const int yy) {
  if (height <= yy) {
    return out + (channels * width * (height - 1));
  } else if (yy < 0) {
    return out;
  }
  return out + (channels * width * yy);
}
/* trackの余白を画像の端の色で塗りつぶす(余白部分をtracksの前後にコピー) */
template <class T>
void paint_margin_(const int margin, std::vector<T> &track) {
  int xx;
  /* 始点側の余白を塗りつぶす */
  for (xx = 0; xx < margin; ++xx) {
    track.at(xx) = track.at(margin);
  }
  /* 終端側の余白を塗りつぶす */
  for (xx = 0; xx < margin; ++xx) {
    track.at(track.size() - 1 - xx) = track.at(track.size() - 1 - margin);
  }
}
/* 入力画像を計算用のtrackバッファーの真ん中に正規化して入れる */
template <class T>
void inn_to_track_(const T *sl, const int width, const int channels,
                   const double div_val, const int margin,
                   std::vector<double> &track) {
  for (int xx = 0; xx < width; ++xx) {
    track.at(margin + xx) = sl[xx * channels] / div_val;
  }
}
/* 入力画像を正規化して、結果を置く場所(result)に初期値として入れる */
template <class T>
void inn_to_result_(
    const T *inn, const int height, const int width, const int channels,
    const int yy, const int zz, const double div_val,
    std::vector<double> &result /* 元値をいれといて、結果を入れる */
) {
  const T *ss = csl_top_clamped_in_h_(inn, height, width, channels, yy) + zz;
  for (int xx = 0; xx < width; ++xx) {
    result.at(xx) = ss[xx * channels] / div_val;
  }
}
/* Scanlineで、効果を調節するデータ(alpha_ref)に、初期値を与える */
void alpha_ref_init_one_(const int width, std::vector<double> &alpha_ref) {
  for (int xx = 0; xx < width; ++xx) {
    alpha_ref.at(xx) = 1.0; /* 全面的に処理を加える値で埋める */
  }
}
/* Scanlineで、効果を調節するデータ(alpha_ref)に、画像の影響を与える */
template <class RT>
void alpha_ref_mul_ref_(const RT *ref, const int height, const int width,
                        const int channels, const int yy, const int ref_mode,
                        std::vector<double> &alpha_ref) {
  const int r_max = (std::numeric_limits<RT>::max)();
  const RT *rr    = csl_top_clamped_in_h_(ref, height, width, channels, yy);
  for (int xx = 0; xx < width; ++xx) {
    double refv =
        igs::color::ref_value(&rr[xx * channels], channels, r_max, ref_mode);
    // clamp 0 to 1 in case using HDR raster
    alpha_ref.at(xx) *= std::min(1., std::max(0., refv));
  }
}
/* Scanlineで、効果を調節するデータ(alpha_ref)に、
        計算済み画像(out)のalphaチャンネル値の影響を与える */
template <class T>
void alpha_ref_mul_alpha_(const T *out, const int height, const int width,
                          const int channels, const int yy,
                          const double div_val,
                          std::vector<double> &alpha_ref) {
  const T *dd = csl_top_clamped_in_h_(out, height, width, channels, yy) + 3;
  for (int xx = 0; xx < width; ++xx) {
    alpha_ref.at(xx) *= dd[xx * channels] / div_val;
  }
}
}  // namespace
//------------------------------------------------------------
namespace igs {
namespace maxmin {
namespace getput {
/* 1番初めのスキャンラインのセット
        * IT is 'unsigned char' or 'unsigned short'
        * alphaは先に処理して結果(out)に入れ、
                その後rgbのために参照する
   */
template <class IT, class RT>
void get_first(
    const IT *inn /* outと同じ高さ、幅、チャンネル数 */
    ,
    const IT *out /* outの処理結果alpha値をinとして使用 */
    ,
    const int hh, const int ww, const int ch,
    const RT *ref /* outと同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    const int yy, const int zz, const int margin, const bool add_blend_sw,
    std::vector<std::vector<double>> &tracks /* sl影響範囲のpixel値 */
    ,
    std::vector<double> &alpha_ref /* pixel毎の変化の割合 */
    ,
    std::vector<double> &result /* 元値をいれといて、結果を入れる */
) {
  const int t_max      = (std::numeric_limits<IT>::max)();
  const double div_val = static_cast<double>(t_max);

  /* 計算範囲の画像値を計算バッファ(tracks)に入れる */
  int ii = margin * 2;
  for (int yp = -margin + yy; yp <= margin + yy; ++yp, --ii) {
    const IT *sl = csl_top_clamped_in_h_(inn, hh, ww, ch, yp) + zz;
    inn_to_track_(sl, ww, ch, div_val, margin, tracks.at(ii));
    paint_margin_(margin, tracks.at(ii));
  }
  inn_to_result_(inn, hh, ww, ch, yy, zz, div_val, result);
  if (alpha_ref.size() <= 0) {
    return;
  } /* alphaチャンネルを計算する場合 */
  alpha_ref_init_one_(ww, alpha_ref);
  if (ref != 0) {
    alpha_ref_mul_ref_(ref, hh, ww, ch, yy, ref_mode, alpha_ref);
  }
  if (ch < 4) {
    return;
  } /* alphaチャンネルがない場合はここで終わり */
  if (add_blend_sw) {
    alpha_ref_mul_alpha_(out, hh, ww, ch, yy, div_val, alpha_ref);
  }
}

template <>
void get_first(
    const float *inn /* outと同じ高さ、幅、チャンネル数 */
    ,
    const float *out /* outの処理結果alpha値をinとして使用 */
    ,
    const int hh, const int ww, const int ch,
    const float *ref /* outと同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    const int yy, const int zz, const int margin, const bool add_blend_sw,
    std::vector<std::vector<double>> &tracks /* sl影響範囲のpixel値 */
    ,
    std::vector<double> &alpha_ref /* pixel毎の変化の割合 */
    ,
    std::vector<double> &result /* 元値をいれといて、結果を入れる */
) {
  const double div_val = 1.;

  /* 計算範囲の画像値を計算バッファ(tracks)に入れる */
  int ii = margin * 2;
  for (int yp = -margin + yy; yp <= margin + yy; ++yp, --ii) {
    const float *sl = csl_top_clamped_in_h_(inn, hh, ww, ch, yp) + zz;
    inn_to_track_(sl, ww, ch, div_val, margin, tracks.at(ii));
    paint_margin_(margin, tracks.at(ii));
  }
  inn_to_result_(inn, hh, ww, ch, yy, zz, div_val, result);
  if (alpha_ref.size() <= 0) {
    return;
  } /* alphaチャンネルを計算する場合 */
  alpha_ref_init_one_(ww, alpha_ref);
  if (ref != 0) {
    alpha_ref_mul_ref_(ref, hh, ww, ch, yy, ref_mode, alpha_ref);
  }
  if (ch < 4) {
    return;
  } /* alphaチャンネルがない場合はここで終わり */
  if (add_blend_sw) {
    alpha_ref_mul_alpha_(out, hh, ww, ch, yy, div_val, alpha_ref);
  }
}
/*--- 2番以後のスキャンラインのセット -------------------------------*/
template <class IT, class RT>
void get_next(const IT *inn /* outと同じ高さ、幅、チャンネル数 */
              ,
              const IT *out /* outの処理結果alpha値をinとして使用 */
              ,
              const int hh, const int ww, const int ch,
              const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
              ,
              const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
              ,
              const int yy, const int zz, const int margin,
              const bool add_blend_sw,
              std::vector<std::vector<double>> &tracks /* sl影響範囲のpixel値 */
              ,
              std::vector<double> &alpha_ref /* pixel毎の変化の割合 */
              ,
              std::vector<double> &result /* 元値をいれといて、結果を入れる */
) {
  const int t_max      = (std::numeric_limits<IT>::max)();
  const double div_val = static_cast<double>(t_max);

  const IT *sl = csl_top_clamped_in_h_(inn, hh, ww, ch, yy + margin) + zz;
  inn_to_track_(sl, ww, ch, div_val, margin, tracks.at(0));
  paint_margin_(margin, tracks.at(0));

  inn_to_result_(inn, hh, ww, ch, yy, zz, div_val, result);
  if (alpha_ref.size() <= 0) {
    return;
  } /* alphaチャンネルを計算する場合 */
  alpha_ref_init_one_(ww, alpha_ref);
  if (ref != 0) {
    alpha_ref_mul_ref_(ref, hh, ww, ch, yy, ref_mode, alpha_ref);
  }
  if (ch < 4) {
    return;
  } /* alphaチャンネルがない場合はここで終わり */
  if (add_blend_sw) {
    alpha_ref_mul_alpha_(out, hh, ww, ch, yy, div_val, alpha_ref);
  }
}
template <>
void get_next(const float *inn /* outと同じ高さ、幅、チャンネル数 */
              ,
              const float *out /* outの処理結果alpha値をinとして使用 */
              ,
              const int hh, const int ww, const int ch,
              const float *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
              ,
              const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
              ,
              const int yy, const int zz, const int margin,
              const bool add_blend_sw,
              std::vector<std::vector<double>> &tracks /* sl影響範囲のpixel値 */
              ,
              std::vector<double> &alpha_ref /* pixel毎の変化の割合 */
              ,
              std::vector<double> &result /* 元値をいれといて、結果を入れる */
) {
  const double div_val = 1.;

  const float *sl = csl_top_clamped_in_h_(inn, hh, ww, ch, yy + margin) + zz;
  inn_to_track_(sl, ww, ch, div_val, margin, tracks.at(0));
  paint_margin_(margin, tracks.at(0));

  inn_to_result_(inn, hh, ww, ch, yy, zz, div_val, result);
  if (alpha_ref.size() <= 0) {
    return;
  } /* alphaチャンネルを計算する場合 */
  alpha_ref_init_one_(ww, alpha_ref);
  if (ref != 0) {
    alpha_ref_mul_ref_(ref, hh, ww, ch, yy, ref_mode, alpha_ref);
  }
  if (ch < 4) {
    return;
  } /* alphaチャンネルがない場合はここで終わり */
  if (add_blend_sw) {
    alpha_ref_mul_alpha_(out, hh, ww, ch, yy, div_val, alpha_ref);
  }
}

template <class T>
void copy(const T *inn, const int hh, const int ww, const int ch, const int yy,
          const int zz, T *out) {
  const T *ss = csl_top_clamped_in_h_(inn, hh, ww, ch, yy) + zz;
  T *dd       = sl_out_clamped_in_h_(out, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    dd[ch * xx] = ss[ch * xx];
  }
}

template <class T>
void put(const std::vector<double> &result, const int hh, const int ww,
         const int ch, const int yy, const int zz, T *out) {
  const int t_max      = (std::numeric_limits<T>::max)();
  const double mul_val = static_cast<double>(t_max) + 0.999999;
  T *dd                = sl_out_clamped_in_h_(out, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    dd[ch * xx] = static_cast<T>(result.at(xx) * mul_val);
    // std::cout << " (" << result.at(xx) << ")(" << (int)dd[ch * xx] << ")";
  }
  // std::cout << std::endl;
}

template <>
void put(const std::vector<double> &result, const int hh, const int ww,
         const int ch, const int yy, const int zz, float *out) {
  float *dd = sl_out_clamped_in_h_(out, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    dd[ch * xx] = static_cast<float>(result.at(xx));
  }
}

}  // namespace getput
}  // namespace maxmin
}  // namespace igs
#endif /* !igs_maxmin_getput_h */
