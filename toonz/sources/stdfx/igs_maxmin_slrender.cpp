#include <iostream>
#include <iomanip>
#include <algorithm> /* std::copy() */
#include "igs_maxmin_slrender.h"
#include "igs_maxmin_lens_matrix.h"

void igs::maxmin::slrender::resize(const int odd_diameter, const int width,
                                   const bool alpha_ref_sw,
                                   std::vector<std::vector<double>> &tracks,
                                   std::vector<double> &alpha_ref,
                                   std::vector<double> &result) {
  tracks.resize(odd_diameter);
  for (int yy = 0; yy < odd_diameter; ++yy) {
    tracks.at(yy).resize(width + odd_diameter - 1);
  }
  if (alpha_ref_sw) {
    alpha_ref.resize(width);
  }
  result.resize(width);
}
void igs::maxmin::slrender::clear(std::vector<std::vector<double>> &tracks,
                                  std::vector<double> &alpha_ref,
                                  std::vector<double> &result) {
  result.clear();
  alpha_ref.clear();
  tracks.clear();
}
void igs::maxmin::slrender::shift(std::vector<std::vector<double>> &tracks) {
  /* 先頭からtracks.end()-1番目の要素が先頭にくるように回転 */
  std::rotate(tracks.begin(), tracks.end() - 1, tracks.end());
}

namespace {
double maxmin_(const double src, const bool min_sw,
               const std::vector<const double *> &begin_ptr,
               const std::vector<int> &lens_sizes,
               const std::vector<std::vector<double>> &lens_ratio) {
  if (min_sw) {
    /* 暗を広げる場合、反転して判断し、結果は反転して戻す */
    double val           = 1.0 - src; /* 反転して判断 */
    const double rev_src = 1.0 - src; /* 反転して判断 */
    for (unsigned yy = 0; yy < begin_ptr.size(); ++yy) {
      const int sz = lens_sizes.at(yy);
      if (sz <= 0) {
        continue;
      }

      const double *xptr = begin_ptr.at(yy);
      const double *rptr = &lens_ratio.at(yy).at(0);
      for (int xx = 0; xx < sz; ++xx, ++xptr, ++rptr) {
        double crnt = 1.0 - (*xptr); /* 反転して判断 */

        /* 元値と同じか(反転してるので)小さい値は不要 */
        if (crnt <= rev_src) {
          continue;
        }

        /* 元値との差に比率を掛けて結果値を出す */
        crnt = rev_src + (crnt - rev_src) * (*rptr);
        /* 今までの中で(反転してるので)より大きいなら代入 */
        if (val < crnt) {
          val = crnt;
        }
      }
    }
    return 1.0 - val; /* 結果は反転して戻す */
  }
  /* Max */
  double val = src;
  for (unsigned yy = 0; yy < begin_ptr.size(); ++yy) {
    const int sz = lens_sizes.at(yy);
    if (sz <= 0) {
      continue;
    }

    const double *xptr = begin_ptr.at(yy);
    const double *rptr = &lens_ratio.at(yy).at(0);
    for (int xx = 0; xx < sz; ++xx, ++xptr, ++rptr) {
      /* 元値と同じか小さい値は不要 */
      if ((*xptr) <= src) {
        continue;
      }

      /* 元値との差に比率を掛けて結果値を出す */
      const double crnt = src + ((*xptr) - src) * (*rptr);
      /* 今までの中でより大きいなら代入 */
      if (val < crnt) {
        val = crnt;
      }
    }
  }
  return val;
}
void set_begin_ptr_(const std::vector<std::vector<double>> &tracks,
                    const std::vector<int> &lens_offsets, const int offset,
                    std::vector<const double *> &begin_ptr) {
  for (unsigned ii = 0; ii < lens_offsets.size(); ++ii) {
    begin_ptr.at(ii) = (0 <= lens_offsets.at(ii))
                           ? &tracks.at(ii).at(offset + lens_offsets.at(ii))
                           : 0;
  }
}
}
/* --- tracksをレンダリングする --------------------------------------*/
void igs::maxmin::slrender::render(
    const double radius, const double smooth_outer_range,
    const int polygon_number, const double roll_degree

    ,
    const bool min_sw

    ,
    std::vector<int> &lens_offsets, std::vector<int> &lens_sizes,
    std::vector<std::vector<double>> &lens_ratio

    ,
    const std::vector<std::vector<double>> &tracks /* RGBのどれか */
    ,
    const std::vector<double> &alpha_ref /* alpha値で影響度合を決める */
    ,
    std::vector<double> &result /* 計算結果 */
    ) {
  /* 初期位置 */
  std::vector<const double *> begin_ptr(lens_offsets.size());
  set_begin_ptr_(tracks, lens_offsets, 0, begin_ptr);

  /* 効果半径に変化がある場合 */
  if (0 < alpha_ref.size()) {
    double before_radius = 0.0;
    for (unsigned xx = 0; xx < result.size(); ++xx) {
      /* 次の処理の半径 */
      const double radius2 = alpha_ref.at(xx) * radius;

      /* ゼロ以上なら処理する */
      if (0.0 < alpha_ref.at(xx)) {
        /* 前のPixelと違う大きさならreshapeする */
        if (radius2 != before_radius) {
          igs::maxmin::reshape_lens_matrix(
              radius2, igs::maxmin::outer_radius_from_radius(
                           radius2, smooth_outer_range),
              igs::maxmin::diameter_from_outer_radius(radius +
                                                      smooth_outer_range),
              polygon_number, roll_degree, lens_offsets, lens_sizes,
              lens_ratio);
          set_begin_ptr_(tracks, lens_offsets, xx, begin_ptr);
        }
        /* 各ピクセルの処理 */
        result.at(xx) =
            maxmin_(result.at(xx), min_sw, begin_ptr, lens_sizes, lens_ratio);
      } /* alpha_refがゼロなら変化なし */

      /* 次の位置へ移動 */
      for (unsigned ii = 0; ii < begin_ptr.size(); ++ii) {
        if (begin_ptr.at(ii) != 0) {
          ++begin_ptr.at(ii);
        }
      }
      if (radius2 != before_radius) {
        before_radius = radius2;
      }
    }
  }
  /* 効果半径が変わらない場合 */
  else {
    for (unsigned xx = 0; xx < result.size(); ++xx) {
      /* 各ピクセルの処理 */
      result.at(xx) =
          maxmin_(result.at(xx), min_sw, begin_ptr, lens_sizes, lens_ratio);

      /* 次の位置へ移動 */
      for (unsigned ii = 0; ii < begin_ptr.size(); ++ii) {
        if (begin_ptr.at(ii) != 0) {
          ++begin_ptr.at(ii);
        }
      }
    }
  }
}
