#include <string>
#include <vector>
#include <limits>     // std::numeric_limits
#include <stdexcept>  // std::domain_error()
#include <algorithm>  // std::max(),std::rotate()
#include "igs_resource_multithread.h"
#include "igs_ifx_common.h"
#include "igs_fog.h"
#include "igs_attenuation_distribution.cpp"

namespace {  //---------------------------------------------------------
void sl_track_render_dark_(
    double *in_out /* RorGorBorA元かつ結果値(0...1) */
    ,
    const double *alpha /* Alpha元値(0...1) */
    ,
    const int width /* 画像幅 */

    ,
    double **lens /* lensの外周の左開始位置ポインタ配列 */
    ,
    double **track /* 参照画外周の左開始位置ポインタ配列 */
    ,
    const int *sizes /* lens及び参照scanline毎の長さ配列 */
    ,
    const int size /* lens及び参照scanline数 */

    ,
    const double power = 1.0 /* 0無  0...1散暗  1強暗  1...2最強暗 */
    ) {
  for (int xx = 0; xx < width; ++xx) { /* 画像のScanlineループ */
    /* Alphaがないか、Alpha値ゼロより大きいと処理しRGB変化 */
    if ((0 == alpha) || (0.0 < alpha[xx])) {
      double total = 0.0;
      for (int y2 = 0; y2 < size; ++y2) {
        for (int x2 = 0; x2 < sizes[y2]; ++x2) {
          if (track[y2][x2] < in_out[xx]) {
            /* より暗いpixelにのみ暗さの影響を受ける */
            total += lens[y2][x2] * track[y2][x2];
          } else {
            /* より明るいpixelから影響を受けないようにする */
            total += lens[y2][x2] * in_out[xx];
          }
        }
      }
      /* !!!lensによる値の蓄積が0-1とならなければならない!!! */
      /* 変化を、強弱を付けて、AlphaでMaskし、元値に加える */
      total -= in_out[xx]; /* 変化を */
      total *= power;      /* 強弱を付けて */
      if ((0 != alpha) && (alpha[xx] < 1.0)) {
        total *= alpha[xx]; /* AlphaでMaskし */
      }
      total += in_out[xx]; /* 元値に加える */
      /* 0-1にclampし保存 */
      in_out[xx] = ((total < 0.0) ? 0.0 : ((1.0 < total) ? 1.0 : total));
    }
    for (int y2 = 0; y2 < size; ++y2) {
      if (0 < sizes[y2]) {
        ++track[y2];
      } /* 次位置へずらしとく */
    }
  }
}
void sl_track_render_light_(  // = fog = default
    double *in_out            /* RorGorBorA元かつ結果値(0...1) */
    ,
    const double *alpha /* Alpha元値(0...1) */
    ,
    const int width /* 画像幅 */

    ,
    double **lens /* lensの外周の左開始位置ポインタ配列 */
    ,
    double **track /* 参照画外周の左開始位置ポインタ配列 */
    ,
    const int *sizes /* lens及び参照scanline毎の長さ配列 */
    ,
    const int size /* lens及び参照scanline数 */

    ,
    const double power = 1.0 /* 0無  0...1散光  1強光  1...2最強光 */
    ) {
  for (int xx = 0; xx < width; ++xx) { /* 画像のScanlineループ */
    /* Alphaがないか、Alpha値ゼロより大きいと処理しRGB変化 */
    if ((0 == alpha) || (0.0 < alpha[xx])) {
      double total = 0.0;
      for (int y2 = 0; y2 < size; ++y2) {
        for (int x2 = 0; x2 < sizes[y2]; ++x2) {
          if (in_out[xx] < track[y2][x2]) {
            /* より明るいpixelにのみ明るさの影響を受ける */
            total += lens[y2][x2] * track[y2][x2];
          } else {
            /* より暗いpixelから影響を受けないようにする */
            total += lens[y2][x2] * in_out[xx];
          }
        }
      }
      /* !!!lensによる値の蓄積が0-1とならなければならない!!! */
      /* 変化を、強弱を付けて、AlphaでMaskし、元値に加える */
      total -= in_out[xx]; /* 変化を */
      total *= power;      /* 強弱を付けて */
      if ((0 != alpha) && (alpha[xx] < 1.0)) {
        total *= alpha[xx]; /* AlphaでMaskし */
      }
      total += in_out[xx]; /* 元値に加える */
      /* 0-1にclampし保存 */
      in_out[xx] = ((total < 0.0) ? 0.0 : ((1.0 < total) ? 1.0 : total));
    }
    for (int y2 = 0; y2 < size; ++y2) {
      if (0 < sizes[y2]) {
        ++track[y2];
      } /* 次位置へずらしとく */
    }
  }
}
void sl_track_render_threshold_(
    double *in_out /* RorGorBorA元かつ結果値(0...1) */
    ,
    const double *alpha /* Alpha元値(0...1) */
    ,
    const int width /* 画像幅 */

    ,
    double **lens /* lensの外周の左開始位置ポインタ配列 */
    ,
    double **track /* 参照画外周の左開始位置ポインタ配列 */
    ,
    double **thres /* 影響画外周の左開始位置ポインタ配列 */
    ,
    const int *sizes /* lens及び参照scanline毎の長さ配列 */
    ,
    const int size /* lens及び参照scanline数 */

    ,
    const double power = 1.0 /* 0無  0...1散光  1強光  1...2最強光 */
    ,
    const double threshold_min = 0.0 /* 0Cut無 0...1部分 1.01全Cut */
    ) {
  for (int xx = 0; xx < width; ++xx) { /* 画像のScanlineループ */
    /* Alphaがないか、Alpha値ゼロより大きいと処理しRGB変化 */
    if ((0 == alpha) || (0.0 < alpha[xx])) {
      double total = 0.0;
      for (int y2 = 0; y2 < size; ++y2) {
        for (int x2 = 0; x2 < sizes[y2]; ++x2) {
          if ((in_out[xx] < track[y2][x2]) &&
              (threshold_min <= thres[y2][x2])) {
            /* より明るいpixelにのみ明るさの影響を受ける */
            total += lens[y2][x2] * track[y2][x2];
          } else {
            /* より暗いpixelから影響を受けないようにする */
            total += lens[y2][x2] * in_out[xx];
          }
        }
      }
      /* !!!lensによる値の蓄積が0-1とならなければならない!!! */
      /* 変化を、強弱を付けて、AlphaでMaskし、元値に加える */
      total -= in_out[xx]; /* 変化を */
      total *= power;      /* 強弱を付けて */
      if ((0 != alpha) && (alpha[xx] < 1.0)) {
        total *= alpha[xx]; /* AlphaでMaskし */
      }
      total += in_out[xx]; /* 元値に加える */
      /* 0-1にclampし保存 */
      in_out[xx] = ((total < 0.0) ? 0.0 : ((1.0 < total) ? 1.0 : total));
    }
    for (int y2 = 0; y2 < size; ++y2) {
      if (0 < sizes[y2]) {
        ++track[y2]; /* 次の画像位置へずらしとく */
        ++thres[y2]; /* 次の画像位置へずらしとく */
      }
    }
  }
}
void sl_track_render_thrminmax_(
    double *in_out /* RorGorBorA元かつ結果値(0...1) */
    ,
    const double *alpha /* Alpha元値(0...1) */
    ,
    const int width /* 画像幅 */

    ,
    double **lens /* lensの外周の左開始位置ポインタ配列 */
    ,
    double **track /* 参照画外周の左開始位置ポインタ配列 */
    ,
    double **thres /* 影響画外周の左開始位置ポインタ配列 */
    ,
    const int *sizes /* lens及び参照scanline毎の長さ配列 */
    ,
    const int size /* lens及び参照scanline数 */

    ,
    const double power = 1.0 /* 0無  0...1散光  1強光  1...2最強光 */
    ,
    const double threshold_min = 0.0 /* 0Cut無 0...1部分 1.01全Cut */
    ,
    const double threshold_max = 0.0 /* 0Cut無 0...1部分 1.01全Cut */
    ) {
  const double thr_range = threshold_max - threshold_min;
  for (int xx = 0; xx < width; ++xx) { /* 画像のScanlineループ */
    /* Alphaがないか、Alpha値ゼロより大きいと処理しRGB変化 */
    if ((0 == alpha) || (0.0 < alpha[xx])) {
      double total = 0.0;
      for (int y2 = 0; y2 < size; ++y2) {
        for (int x2 = 0; x2 < sizes[y2]; ++x2) {
          if ((in_out[xx] < track[y2][x2]) &&
              (threshold_min <= thres[y2][x2])) {
            /* より明るくかつminより大きい値のpixelにのみ
明るさの影響を受ける */
            if (thres[y2][x2] < threshold_max) {
              /* maxよりは小さい値のpixelは比で影響する */
              total += lens[y2][x2] *
                       (in_out[xx] +
                        (track[y2][x2] - in_out[xx]) *
                            (thres[y2][x2] - threshold_min) / thr_range);
            } else {
              /* maxより大きい値のpixelは明るさ影響を受ける */
              total += lens[y2][x2] * track[y2][x2];
            }
          } else {
            /* より暗いか、minより小さい値のpixelから
影響を受けないようにする */
            total += lens[y2][x2] * in_out[xx];
          }
        }
      }
      /* !!!lensによる値の蓄積が0-1とならなければならない!!! */
      /* 変化を、強弱を付けて、AlphaでMaskし、元値に加える */
      total -= in_out[xx]; /* 変化を */
      total *= power;      /* 強弱を付けて */
      if ((0 != alpha) && (alpha[xx] < 1.0)) {
        total *= alpha[xx]; /* AlphaでMaskし */
      }
      total += in_out[xx]; /* 元値に加える */
      /* 0-1にclampし保存 */
      in_out[xx] = ((total < 0.0) ? 0.0 : ((1.0 < total) ? 1.0 : total));
    }
    for (int y2 = 0; y2 < size; ++y2) {
      if (0 < sizes[y2]) {
        ++track[y2]; /* 次の画像位置へずらしとく */
        ++thres[y2]; /* 次の画像位置へずらしとく */
      }
    }
  }
}
}
namespace {  //---------------------------------------------------------
void sl_track_resize_(const int odd_diameter, const int width,
                      std::vector<std::vector<double>> &pixe_tracks,
                      const bool thr_sw,
                      std::vector<std::vector<double>> &thre_tracks,
                      std::vector<double *> &pixe_starts,
                      std::vector<double *> &thre_starts,
                      std::vector<double> &result,
                      std::vector<double> &alpha_ref) {
  pixe_tracks.resize(odd_diameter);
  for (int yy = 0; yy < odd_diameter; ++yy) {
    pixe_tracks.at(yy).resize(width + odd_diameter - 1);
  }
  if (thr_sw) {
    thre_tracks.resize(odd_diameter);
    for (int yy = 0; yy < odd_diameter; ++yy) {
      thre_tracks.at(yy).resize(width + odd_diameter - 1);
    }
  }
  pixe_starts.resize(odd_diameter);
  thre_starts.resize(odd_diameter);
  result.resize(width);
  alpha_ref.clear(); /* 今はクリア!!!あとで使うとき確保!!! */
}
void sl_track_render_(
    // std::vector< std::vector<double> >&lens_matrix
    std::vector<int> &lens_offsets, std::vector<double *> &lens_starts,
    std::vector<int> &lens_sizes

    ,
    std::vector<std::vector<double>> &pixe_tracks,
    std::vector<std::vector<double>> &thre_tracks,
    const std::vector<double> &alpha_ref, std::vector<double> &result,
    std::vector<double *> &pixe_starts, std::vector<double *> &thre_starts

    ,
    const double power, const double threshold_min,
    const double threshold_max) {
  /*--- scanline初期位置 ---*/
  for (unsigned int yy = 0; yy < pixe_starts.size(); ++yy) {
    if (lens_offsets.at(yy) < 0) {
      pixe_starts.at(yy) = 0;
    } else {
      pixe_starts.at(yy) = &pixe_tracks.at(yy).at(lens_offsets.at(yy));
    }
  }
  if (0 < thre_tracks.size()) {
    for (unsigned int yy = 0; yy < thre_starts.size(); ++yy) {
      if (lens_offsets.at(yy) < 0) {
        thre_starts.at(yy) = 0;
      } else {
        thre_starts.at(yy) = &thre_tracks.at(yy).at(lens_offsets.at(yy));
      }
    }
  }
  /*--- 自分より暗いpixelから収光 ---*/
  if (power < 0.0) {
    sl_track_render_dark_(&result.at(0),
                          ((0 < alpha_ref.size()) ? &alpha_ref.at(0) : 0),
                          static_cast<int>(result.size())

                              ,
                          &lens_starts.at(0), &pixe_starts.at(0),
                          &lens_sizes.at(0), static_cast<int>(lens_sizes.size())

                                                 ,
                          -power);
    return;
  }
  /*--- 自分より明るいpixelから収光し光量によってfogが変化 ---*/
  if ((0.0 < threshold_min) || (0.0 < threshold_max)) {
    if (threshold_min < threshold_max) {
      sl_track_render_thrminmax_(
          &result.at(0), ((0 < alpha_ref.size()) ? &alpha_ref.at(0) : 0),
          static_cast<int>(result.size())

              ,
          &lens_starts.at(0), &pixe_starts.at(0), &thre_starts.at(0),
          &lens_sizes.at(0), static_cast<int>(lens_sizes.size())

                                 ,
          power, threshold_min, threshold_max);
      return;
    } else {
      /*--- 自分より明るいpixelから収光し、
ある光量以上からいきなりfogがかかる ---*/
      sl_track_render_threshold_(
          &result.at(0), ((0 < alpha_ref.size()) ? &alpha_ref.at(0) : 0),
          static_cast<int>(result.size())

              ,
          &lens_starts.at(0), &pixe_starts.at(0), &thre_starts.at(0),
          &lens_sizes.at(0), static_cast<int>(lens_sizes.size())

                                 ,
          power, threshold_min);
      return;
    }
  }
  /*--- 自分より明るいpixelから収光 ---*/
  sl_track_render_light_(&result.at(0),
                         ((0 < alpha_ref.size()) ? &alpha_ref.at(0) : 0),
                         static_cast<int>(result.size())

                             ,
                         &lens_starts.at(0), &pixe_starts.at(0),
                         &lens_sizes.at(0), static_cast<int>(lens_sizes.size())

                                                ,
                         power);
}
void sl_track_shift_(std::vector<std::vector<double>> &pixe_tracks,
                     std::vector<std::vector<double>> &thre_tracks) {
  std::rotate(pixe_tracks.begin(), pixe_tracks.end() - 1, pixe_tracks.end());
  if (0 < thre_tracks.size()) {
    std::rotate(thre_tracks.begin(), thre_tracks.end() - 1, thre_tracks.end());
  }
}
void sl_track_clear_(std::vector<std::vector<double>> &pixe_tracks,
                     std::vector<std::vector<double>> &thre_tracks,
                     std::vector<double *> &pixe_starts,
                     std::vector<double *> &thre_starts,
                     std::vector<double> &result,
                     std::vector<double> &alpha_ref) {
  if (!alpha_ref.empty()) {
    alpha_ref.clear();
  }
  if (!result.empty()) {
    result.clear();
  }
  if (!thre_starts.empty()) {
    thre_starts.clear();
  }
  if (!pixe_starts.empty()) {
    pixe_starts.clear();
  }
  if (!thre_tracks.empty()) {
    thre_tracks.clear();
  }
  if (!pixe_tracks.empty()) {
    pixe_tracks.clear();
  }
}
}
namespace {  //--------------------------------------------------------
template <class T>
void rgb_to_lightness_image_(const T *image, const int height, const int width,
                             const int channels, double *lightness) {
  const double val_max = static_cast<double>(std::numeric_limits<T>::max());
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx, image += channels, ++lightness) {
      if (1 == channels) {
        *lightness = static_cast<double>(image[0]) / val_max;
      } else {
        const double red = static_cast<double>(image[0]) / val_max;
        const double gre = static_cast<double>(image[1]) / val_max;
        const double blu = static_cast<double>(image[2]) / val_max;
        /* rgb --> hls のl(明度(lightness))のみの計算 */
        *lightness = (std::max(std::max(red, gre), blu) +
                      std::min(std::min(red, gre), blu)) /
                     2.0;
      }
    }
  }
}
}
namespace {  //--------------------------------------------------------
/*
        画像配列の高さ位置を、実際の範囲内にclampし、scanlineの先頭を返す
        TP is 'unsigned char *' or 'unsigned short *'
  */
template <class TP>
TP sl_top_clamped_in_h_(TP sl, const int height, const int width,
                        const int channels, const int yy) {
  if (height <= yy) {
    return sl + (channels * width * (height - 1));
  } else if (yy < 0) {
    return sl;
  }
  return sl + (channels * width * yy);
}
/*
        trackの左右の余白を画像の端の色で塗りつぶす
  */
template <class T>
void paint_margin_(const int width, const int margin, std::vector<T> &track) {
  if (width <= (margin * 2)) {
    return;
  } /* 余白太すぎ */

  for (int xx = 0; xx < margin; ++xx) {
    track.at(xx) = track.at(margin); /* 始点側の余白を塗る */
    track.at(track.size() - 1 - xx) =
        track.at(track.size() - margin - 1); /* 終端側の余白を塗る */
  }
}
}
namespace {  //--------------------------------------------------------
/*
        T is 'unsigned char' or 'unsigned short'
        alphaは先に処理して結果(out_image_array)に入れ、
                その後rgbのために参照する
  */
template <class T>
void get_first_sl_ch_(
    const T *in_image_array,
    const T *out_image_array /* 処理結果alpha値の参照のため必要 */
    ,
    const double *ref_thres_array /* threshold値の参照のため必要 */
    ,
    const int hh, const int ww, const int ch, const int yy, const int zz,
    const int margin, std::vector<std::vector<double>> &pixe_tracks,
    std::vector<std::vector<double>> &thre_tracks, std::vector<double> &result,
    std::vector<double> &alpha_ref) {
  const double val_max = static_cast<double>(std::numeric_limits<T>::max());
  /*--- 入力画像を(マージン含めた1scanline分)得る ---*/
  {
    int yp = -margin + yy;
    int ii = margin * 2;
    for (; yp <= margin + yy; ++yp, --ii) {
      const T *ss = sl_top_clamped_in_h_(in_image_array, hh, ww, ch, yp) + zz;
      std::vector<double> &track = pixe_tracks.at(ii);
      for (int xx = 0; xx < ww; ++xx) {
        track.at(margin + xx) = static_cast<double>(ss[xx * ch]) / val_max;
      }
      paint_margin_(ww, margin, track);
    }
  }
  /*--- (閾値として)参照する画像を(マージン含めた1scanline分)得る -*/
  if ((0 != ref_thres_array) && (0 < thre_tracks.size())) {
    int yp = -margin + yy;
    int ii = margin * 2;
    for (; yp <= margin + yy; ++yp, --ii) {
      const double *ss = sl_top_clamped_in_h_(ref_thres_array, hh, ww, 1, yp);
      std::vector<double> &track = thre_tracks.at(ii);
      for (int xx = 0; xx < ww; ++xx) {
        track.at(margin + xx) = ss[xx * 1];
      }
      paint_margin_(ww, margin, track);
    }
  }
  /*--- 入力画像を保存画像エリアに1scanline分入れる ---*/
  const T *ss = sl_top_clamped_in_h_(in_image_array, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    result.at(xx) = static_cast<double>(ss[xx * ch]) / val_max;
  }
  /*--- 処理結果としてのalpha画像エリアを1scanline分入れる ---*/
  if ((alpha_ref.size() <= 0) || (ch < 4)) {
    return;
  }
  const T *dd = sl_top_clamped_in_h_(out_image_array, hh, ww, ch, yy) + 3;
  for (int xx = 0; xx < ww; ++xx) {
    alpha_ref.at(xx) = static_cast<double>(dd[xx * ch]) / val_max;
  }
}
template <class T>
void get_next_sl_ch_(
    const T *in_image_array,
    const T *out_image_array /* 処理結果alpha値の参照のため必要 */
    ,
    const double *ref_thres_array /* threshold値の参照のため必要 */
    ,
    const int hh, const int ww, const int ch, const int yy, const int zz,
    const int margin, std::vector<std::vector<double>> &pixe_tracks,
    std::vector<std::vector<double>> &thre_tracks, std::vector<double> &result,
    std::vector<double> &alpha_ref) {
  const double val_max = static_cast<double>(std::numeric_limits<T>::max());
  /*--- 入力画像を(1scanline分)得る ---*/
  {
    const T *mm =
        sl_top_clamped_in_h_(in_image_array, hh, ww, ch, yy + margin) + zz;
    std::vector<double> &track = pixe_tracks.at(0);
    for (int xx = 0; xx < ww; ++xx) {
      track.at(margin + xx) = static_cast<double>(mm[xx * ch]) / val_max;
    }
    paint_margin_(ww, margin, track);
  }
  /*--- (閾値として)参照する画像を(1scanline分)得る --*/
  if ((0 != ref_thres_array) && (0 < thre_tracks.size())) {
    const double *mm =
        sl_top_clamped_in_h_(ref_thres_array, hh, ww, 1, yy + margin);
    std::vector<double> &track = thre_tracks.at(0);
    for (int xx = 0; xx < ww; ++xx) {
      track.at(margin + xx) = mm[xx * 1];
    }
    paint_margin_(ww, margin, track);
  }
  /*--- 入力画像を保存画像エリアに1scanline分入れる ---*/
  const T *ss = sl_top_clamped_in_h_(in_image_array, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    result.at(xx) = static_cast<double>(ss[xx * ch]) / val_max;
  }
  /*--- 処理結果としてのalpha画像エリアを1scanline分入れる ---*/
  if ((alpha_ref.size() <= 0) || (ch < 4)) {
    return;
  }
  const T *dd = sl_top_clamped_in_h_(out_image_array, hh, ww, ch, yy) + 3;
  for (int xx = 0; xx < ww; ++xx) {
    alpha_ref.at(xx) = static_cast<double>(dd[xx * ch]) / val_max;
  }
}
template <class TP>
void copy_sl_ch_(const TP in_image_array, TP out_image_array, const int hh,
                 const int ww, const int ch, const int yy, const int zz) {
  const TP ss = sl_top_clamped_in_h_(in_image_array, hh, ww, ch, yy) + zz;
  TP dd       = sl_top_clamped_in_h_(out_image_array, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    dd[ch * xx] = ss[ch * xx];
  }
}
template <class T>
void put_sl_ch_(std::vector<double> &result, const int hh, const int ww,
                const int ch, const int yy, const int zz, T *out_image_array) {
  const double val_max = static_cast<double>(std::numeric_limits<T>::max());
  T *dd = sl_top_clamped_in_h_(out_image_array, hh, ww, ch, yy) + zz;
  for (int xx = 0; xx < ww; ++xx) {
    dd[ch * xx] = static_cast<T>(result.at(xx) * (val_max + 0.999999));
  }
}
}
namespace {  //--------------------------------------------------------
template <class T>
class one_thread_ final
    : public igs::resource::thread_execute_interface { /* thread単位の実行設定
                                                    */
public:
  one_thread_() {}
  void setup(T in_image, T out_image, double *ref_thresh

             ,
             const int height, const int width, const int channels

             ,
             const int y_begin, const int y_end

             // , std::vector< std::vector<double> > *lens_matrix_p
             ,
             std::vector<int> *lens_offsets_p,
             std::vector<double *> *lens_starts_p,
             std::vector<int> *lens_sizes_p

             ,
             const double power, const double threshold_min,
             const double threshold_max, const bool alpha_rendering_sw) {
    this->in_image_   = in_image;
    this->out_image_  = out_image;
    this->ref_thresh_ = ref_thresh;

    this->height_   = height;
    this->width_    = width;
    this->channels_ = channels;

    this->y_begin_ = y_begin;
    this->y_end_   = y_end;

    // this->lens_matrix_p_ = lens_matrix_p;
    this->lens_offsets_p_ = lens_offsets_p;
    this->lens_starts_p_  = lens_starts_p;
    this->lens_sizes_p_   = lens_sizes_p;

    this->power_              = power;
    this->threshold_min_      = threshold_min;
    this->threshold_max_      = threshold_max;
    this->alpha_rendering_sw_ = alpha_rendering_sw;

    sl_track_resize_(static_cast<int>(this->lens_offsets_p_->size()),
                     this->width_, this->pixe_tracks_,
                     ((0 == ref_thresh) ? false : true), this->thre_tracks_,
                     this->pixe_starts_, this->thre_starts_, this->result_,
                     this->alpha_ref_);
  }
  void run(void) override {
    bool rgb_rendering_sw   = true;
    bool alpha_rendering_sw = this->alpha_rendering_sw_;
    if (this->pixe_tracks_.size() <= 1) {
      rgb_rendering_sw   = false;
      alpha_rendering_sw = false;
    }  // not render,then copy

    /* first scanline-->next scanlineで処理するので
かならすchannel毎にまとめてループする */

    int yy;
    if (igs::image::rgba::siz == this->channels_) {
      using namespace igs::image::rgba;
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, alp, alpha_rendering_sw);
      }
      /* alpha処理後にalpha参照用設定をする */
      this->alpha_ref_.resize(this->width_);

      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, blu, rgb_rendering_sw);
      }
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, gre, rgb_rendering_sw);
      }
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, red, rgb_rendering_sw);
      }
    } else if (igs::image::rgb::siz == this->channels_) {
      using namespace igs::image::rgb;
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, blu, rgb_rendering_sw);
      }
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, gre, rgb_rendering_sw);
      }
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, red, rgb_rendering_sw);
      }
    } else if (1 == this->channels_) { /* grayscale */
      for (yy = this->y_begin_; yy <= this->y_end_; ++yy) {
        this->rendering_sl_ch_(yy, 0, rgb_rendering_sw);
      }
    }
  }
  void clear(void) {
    sl_track_clear_(this->pixe_tracks_, this->thre_tracks_, this->pixe_starts_,
                    this->thre_starts_, this->result_, this->alpha_ref_);
  }

private:
  T in_image_;
  T out_image_;
  double *ref_thresh_;

  int height_;
  int width_;
  int channels_;

  int y_begin_;
  int y_end_;

  // std::vector< std::vector<double> > *lens_matrix_p_;
  std::vector<int> *lens_offsets_p_;
  std::vector<double *> *lens_starts_p_;
  std::vector<int> *lens_sizes_p_;

  double power_;
  double threshold_min_;
  double threshold_max_;
  bool alpha_rendering_sw_;

  std::vector<std::vector<double>> pixe_tracks_;
  std::vector<std::vector<double>> thre_tracks_;
  std::vector<double *> pixe_starts_;
  std::vector<double *> thre_starts_;
  std::vector<double> result_;
  std::vector<double> alpha_ref_;

  void rendering_sl_ch_(int yy, int zz, bool rendering_sw) {
    if (!rendering_sw) {
      copy_sl_ch_(this->in_image_, this->out_image_, this->height_,
                  this->width_, this->channels_, yy, zz);
      return;
    }
    if (yy == this->y_begin_) {
      get_first_sl_ch_(this->in_image_, this->out_image_, this->ref_thresh_,
                       this->height_, this->width_, this->channels_, yy, zz,
                       static_cast<int>(this->pixe_tracks_.size() / 2),
                       this->pixe_tracks_, this->thre_tracks_, this->result_,
                       this->alpha_ref_);
    } else {
      sl_track_shift_(this->pixe_tracks_, this->thre_tracks_);
      get_next_sl_ch_(this->in_image_, this->out_image_, this->ref_thresh_,
                      this->height_, this->width_, this->channels_, yy, zz,
                      static_cast<int>(this->pixe_tracks_.size() / 2),
                      this->pixe_tracks_, this->thre_tracks_, this->result_,
                      this->alpha_ref_);
    }
    sl_track_render_(
        //  *(this->lens_matrix_p_)
        *(this->lens_offsets_p_), *(this->lens_starts_p_),
        *(this->lens_sizes_p_)

            ,
        this->pixe_tracks_, this->thre_tracks_, this->alpha_ref_

        ,
        this->result_

        ,
        this->pixe_starts_, this->thre_starts_

        ,
        this->power_, this->threshold_min_, this->threshold_max_);
    put_sl_ch_(this->result_, this->height_, this->width_, this->channels_, yy,
               zz, this->out_image_);
  }
};
}
namespace {  //--------------------------------------------------------
template <class T>
class multi_thread_ {
public:
  multi_thread_(T in_image, T out_image, double *ref_thres, const int height,
                const int width, const int channels

                ,
                const int number_of_thread  // 1 ... INT_MAX

                ,
                const double radius  // 25.0(0...100(DOUBLE_MAX))
                ,
                const double curve  // 1.00(0.01 ... 100)
                ,
                const int polygon_number  // 2(2 ... 16(INT_MAX))
                ,
                const double degree  // 0(0 ... DOUBLE_MAX)

                ,
                const double power  // 1.00(-2.00 ... 2.00)
                ,
                const double threshold_min  // 0.00(0.00 ... 1.01)
                ,
                const double threshold_max  // 0.00(0.00 ... 1.01)
                ,
                const bool alpha_rendering_sw  // false(true,false)
                ) {
    /*--------------スレッド数の設定--------------------*/
    int thread_num = number_of_thread;
    if ((thread_num < 1) || (height < thread_num)) {
      thread_num = 1;
    } /* ゼロ以下か、高さより多い */
    /*--------------メモリ確保--------------------------*/
    int odd_diameter = 0;
    attenuation_distribution_(this->lens_matrix_, this->lens_offsets_,
                              this->lens_starts_, this->lens_sizes_,
                              odd_diameter,
                              radius /* 直径(radiusの2倍)1以下ならエラーthrow */
                              ,
                              curve /* curveがゼロならエラーthrow */
                              ,
                              polygon_number, degree);
    this->threads_.resize(thread_num);
    /*-------スレッド毎の処理指定-----------------------*/
    int h_sub = height / thread_num;
    if (0 != (height % thread_num)) {
      ++h_sub; /* 割り切れないときは一つ増やす */
    }
    int yy = 0;
    for (int ii = 0; ii < thread_num; ++ii, yy += h_sub) {
      /*
      5 h_scanline,4 thread --> h_count is 2
      thread	1	2	3	4
      h_count	2	2	1	0
      */
      if (height < (yy + h_sub)) {
        h_sub = height - yy;
      }
      this->threads_.at(ii).setup(
          in_image, out_image, ref_thres

          ,
          height, width, channels

          ,
          yy, yy + h_sub - 1

          // , &(this->lens_matrix_)
          ,
          &(this->lens_offsets_), &(this->lens_starts_), &(this->lens_sizes_)

                                                             ,
          power, threshold_min, threshold_max, alpha_rendering_sw);
    }
    /*------スレッド毎のスレッド指定------*/
    for (int ii = 0; ii < thread_num; ++ii) {
      this->mthread_.add(&(this->threads_.at(ii)));
    }
  }
  void run(void) { this->mthread_.run(); }
  void clear() {
    this->mthread_.clear();
    this->threads_.clear();
    this->lens_sizes_.clear();
    this->lens_starts_.clear();
    this->lens_offsets_.clear();
    this->lens_matrix_.clear();
  }

private:
  std::vector<std::vector<double>> lens_matrix_;
  std::vector<int> lens_offsets_;
  std::vector<double *> lens_starts_;
  std::vector<int> lens_sizes_;
  std::vector<one_thread_<T>> threads_;
  igs::resource::multithread mthread_;
};
}
//--------------------------------------------------------------------
/* パラメータの指定を見て、画像に変化の無い場合、
呼び出し側で、fog処理するか判断する。fog処理しないときの処理も忘れずに */
bool igs::fog::have_change(const double radius  // 25.0(0 ... 100(DOUBLE_MAX))
                           ,
                           const double power  // 1.00(-2.00 ... 2.00)
                           ,
                           const double threshold_min  // 0.00(0.00 ... 1.01)
                           ) {
  /* 収光(変化)しない
          場合1  直径が1以下
          場合2  powerがゼロ(マイナスは有効としている)
          場合3  powerがプラスで、threshold_minが1よりも大きい
  */
  if ((static_cast<int>(ceil(radius * 2.0)) <= 1) || (0.0 == power) ||
      ((0.0 < power) && (1.0 < threshold_min))) {
    return false;
  }
  return true;
}
/* fog処理する */
void igs::fog::convert(void *in  // no margin
                       ,
                       void *out  // no margin
                       ,
                       double *buffer  // no margin

                       ,
                       const int height, const int width, const int channels,
                       const int bits

                       ,
                       const int number_of_thread  // 1 ... INT_MAX

                       ,
                       const double radius  // 25.0(0 ... 100(DOUBLE_MAX))
                       ,
                       const double curve  // 1.00(0.01 ... 100)
                       ,
                       const int polygon_number  // 2(2 ... 16(INT_MAX))
                       ,
                       const double degree  // 0(0 ... DOUBLE_MAX)

                       ,
                       const double power  // 1.00(-2.00 ... 2.00)
                       ,
                       const double threshold_min  // 0.00(0.00 ... 1.01)
                       ,
                       const double threshold_max  // 0.00(0.00 ... 1.01)
                       ,
                       const bool alpha_rendering_sw  // false(true,false)
                       ) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  if (std::numeric_limits<unsigned char>::digits == bits) {
    if (0 != buffer) {
      rgb_to_lightness_image_(static_cast<const unsigned char *>(in), height,
                              width, channels, buffer);
    }
    multi_thread_<unsigned char *> mthread(
        static_cast<unsigned char *>(in), static_cast<unsigned char *>(out),
        buffer, height, width, channels, number_of_thread, radius, curve,
        polygon_number, degree, power, threshold_min, threshold_max,
        alpha_rendering_sw);
    mthread.run();
    mthread.clear();
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    if (0 != buffer) {
      rgb_to_lightness_image_(static_cast<const unsigned short *>(in), height,
                              width, channels, buffer);
    }
    multi_thread_<unsigned short *> mthread(
        static_cast<unsigned short *>(in), static_cast<unsigned short *>(out),
        buffer, height, width, channels, number_of_thread, radius, curve,
        polygon_number, degree, power, threshold_min, threshold_max,
        alpha_rendering_sw);
    mthread.run();
    mthread.clear();
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
