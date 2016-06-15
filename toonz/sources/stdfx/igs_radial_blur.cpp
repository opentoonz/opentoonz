#include <vector>     // std::vector
#include <cmath>      // cos(),sin(),sqrt()
#include <limits>     // std::numeric_limits<T>
#include <stdexcept>  // std::domain_error()
#include "igs_ifx_common.h"
#include "igs_radial_blur.h"
namespace {
//------------------------------------------------------------------
template <class T>
class radial_ {
public:
  radial_(const T *in_top, const int height, const int width,
          const int channels, const double xc, const double yc,
          const double sub_size, const int imax, const double dmax,
          const double intensity /* 平均値ぼかし強度 */
          ,
          const double radius /* 平均値ぼかしの始まる半径 */
          )
      : in_top_(in_top)
      , hh_(height)
      , ww_(width)
      , cc_(channels)
      , xc_(xc)
      , yc_(yc)
      , sub_size_(sub_size)
      , imax_(imax)
      , dmax_(dmax)
      , intensity_(intensity)
      , radius_(radius) {}
  void pixel_value(const T *in_current_pixel, const int xx, const int yy,
                   const int z1, const int z2, const double ref_increase_val,
                   const double ref_decrease_val,
                   const double each_pixel_blur_ratio, T *result_pixel) {
    /* Pixel位置(0.5 1.5 2.5 ...) */
    const double xp = static_cast<double>(xx) + 0.5;
    const double yp = static_cast<double>(yy) + 0.5;

    /* 中心からPixel位置へのベクトルと長さ */
    const double xv   = xp - this->xc_;
    const double yv   = yp - this->yc_;
    const double dist = sqrt(xv * xv + yv * yv);

    /* 指定半径の範囲内なら何もしない */
    if (dist <= this->radius_) {
      for (int zz = z1; zz <= z2; ++zz) {
        result_pixel[zz] = in_current_pixel[zz];
      }
      return;
    }

    /* 中心からPixel位置への単位ベクトル */
    const double cosval = xv / dist;
    const double sinval = yv / dist;

    /* Radial方向のSamplingの開始位置と終了位置 */
    double scale = this->intensity_;
    if (0.0 <= each_pixel_blur_ratio) {
      scale *= each_pixel_blur_ratio;
    }
    const double length     = (dist - this->radius_) * scale;
    const double count_half = floor(length / 2.0 / this->sub_size_);
    const double sub_sta    = -this->sub_size_ * count_half;
    const double sub_end    = this->sub_size_ * count_half;

    /* 積算値と積算回数 */
    std::vector<double> accum_val(this->cc_);
    int accum_counter = 0;

    /* 円周接線方向Samplingの相対位置 */
    for (double ss = this->sub_size_ / 2.0 - 0.5; ss < 0.5;
         ss += this->sub_size_) {
      /* 円周接線方向Sampling位置 */
      const double xps = xp + ss * sinval;
      const double yps = yp + ss * cosval;

      /* 中心からSampling位置へのベクトルと長さ */
      const double xvs   = xps - this->xc_;
      const double yvs   = yps - this->yc_;
      const double dists = sqrt(xvs * xvs + yvs * yvs);

      /* 中心からSampling位置への単位ベクトル */
      const double cosvals = xvs / dists;
      const double sinvals = yvs / dists;

      /* Radial方向のSampling */
      for (double tt = sub_sta; tt <= sub_end; tt += this->sub_size_) {
        /* Sampling位置からPixel位置を得る */
        int xi = static_cast<int>(xps + tt * cosvals);
        int yi = static_cast<int>(yps + tt * sinvals);

        /* clamp */
        xi = (xi < 0) ? 0 : ((this->ww_ <= xi) ? this->ww_ - 1 : xi);
        yi = (yi < 0) ? 0 : ((this->hh_ <= yi) ? this->hh_ - 1 : yi);

        /* 画像のPixel位置 */
        const T *in_current =
            this->in_top_ + this->cc_ * this->ww_ * yi + this->cc_ * xi;

        /* 積算 */
        for (int zz = z1; zz <= z2; ++zz) {
          accum_val[zz] += static_cast<double>(in_current[zz]);
        }
        ++accum_counter;
      }
    }
    /* 積算しなかったとき(念のためのCheck) */
    if (accum_counter <= 0) {
      for (int zz = z1; zz <= z2; ++zz) {
        result_pixel[zz] = in_current_pixel[zz];
      }
      return;
    }
    /* ここで画像Pixelに保存 */
    for (int zz = z1; zz <= z2; ++zz) {
      accum_val[zz] /= static_cast<double>(accum_counter);

      if ((0 <= ref_increase_val) && (in_current_pixel[zz] < accum_val[zz])) {
        /* 増分のみMask! */
        accum_val[zz] =
            static_cast<double>(in_current_pixel[zz]) +
            (accum_val[zz] - in_current_pixel[zz]) * ref_increase_val;
      } else if ((0 <= ref_decrease_val) &&
                 (accum_val[zz] < in_current_pixel[zz])) {
        /* 減分のみMask! */
        accum_val[zz] =
            static_cast<double>(in_current_pixel[zz]) +
            (accum_val[zz] - in_current_pixel[zz]) * ref_decrease_val;
      }

      accum_val[zz] += 0.5; /* 誤差対策 */
      if (this->dmax_ < accum_val[zz]) {
        result_pixel[zz] = static_cast<T>(this->imax_);
      } else if (accum_val[zz] < 0) {
        result_pixel[zz] = 0;
      } else {
        result_pixel[zz] = static_cast<T>(accum_val[zz]);
      }
    }
  }

private:
  radial_() {}

  const T *in_top_;
  const int hh_;
  const int ww_;
  const int cc_;
  const double xc_;
  const double yc_;
  const double sub_size_;
  const int imax_;
  const double dmax_;
  const double intensity_;
  const double radius_;

  /* copy constructorを無効化 */
  radial_(const radial_ &);

  /* 代入演算子を無効化 */
  radial_ &operator=(const radial_ &);
};
template <class IT, class RT>
void radial_convert_template_(
    const IT *in, const int margin /* 参照画像(in)がもつ余白 */

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_bits, const int ref_mode  // R,G,B,A,luminance

    ,
    IT *out, const int hh /* 求める画像(out)の高さ */
    ,
    const int ww /* 求める画像(out)の幅 */
    ,
    const int cc, const double xc, const double yc,
    const double intensity /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    ,
    const double radius /* 平均値ぼかしの始まる半径 */
    ,
    const int sub_div /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw) {
  ref_bits;  // for warning

  /* 強度のないとき、または、サブ分割がないとき、なにもしない */
  if (intensity <= 0.0 || sub_div <= 0) {
    return;
  }

  radial_<IT> cl_ra(
      in, hh + margin * 2, ww + margin * 2, cc, xc + margin, yc + margin,
      1.0 / sub_div, std::numeric_limits<IT>::max() /* サンプリング値 */
      ,
      static_cast<double>(std::numeric_limits<IT>::max()), intensity, radius);

#if defined RGBA_ORDER_OF_TOONZ6
  const int z1 = igs::image::rgba::blu;
  const int z2 = igs::image::rgba::red;
#elif defined RGBA_ORDER_OF_OPENGL
  const int z1 = igs::image::rgba::red;
  const int z2 = igs::image::rgba::blu;
#else
  Must be define / DRGBA_ORDER_OF_TOONZ6 or
      / DRGBA_ORDER_OF_OPENGL
#endif
  const IT *p_in = in + margin * (ww + margin * 2) * cc + margin * cc;
  IT *pout       = out;

  if (0 == ref) { /* 参照なし */
    if (igs::image::rgba::siz == cc) {
      using namespace igs::image::rgba;
      if (alpha_rendering_sw) { /* Alphaも処理する */
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc) {
            cl_ra.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, -1.0, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ra.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, -1.0, pout);
          }
        }
      } else { /* Alpha処理しない、RGB増分をAlphaでMaskする */
        const unsigned int val_max = std::numeric_limits<IT>::max();
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc) {
            pout[alp] = p_in[alp];
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            /* Alpha値あるならRGB増分のみAlphaでMaskする */
            cl_ra.pixel_value(p_in, xx, yy, z1, z2,
                              static_cast<double>(p_in[alp]) / val_max, -1.0,
                              -1.0, pout);
          }
        }
      }
    } else { /* Alphaがない, RGB/Grayscale... */
      for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
        for (int xx = margin; xx < ww + margin; ++xx, p_in += cc, pout += cc) {
          cl_ra.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, -1.0, pout);
        }
      }
    }
  } else { /* 参照あり */
    const RT *refe  = ref;
    const int r_max = std::numeric_limits<RT>::max();
    if (igs::image::rgba::siz == cc) {
      using namespace igs::image::rgba;
      if (alpha_rendering_sw) { /* Alpha処理する */
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc, refe += cc) {
            const double refv =
                igs::color::ref_value(refe, cc, r_max, ref_mode);
            if (0 == refv) {
              for (int zz = 0; zz < cc; ++zz) {
                pout[zz] = p_in[zz];
              }
              continue;
            }

            cl_ra.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, refv, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ra.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, refv, pout);
          }
        }
      } else { /* Alpha処理しない、RGB増分をAlphaでMaskする */
        const unsigned int val_max = std::numeric_limits<IT>::max();
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc, refe += cc) {
            const double refv =
                igs::color::ref_value(refe, cc, r_max, ref_mode);
            if (0 == refv) {
              for (int zz = 0; zz < cc; ++zz) {
                pout[zz] = p_in[zz];
              }
              continue;
            }

            pout[alp] = p_in[alp];
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ra.pixel_value(p_in, xx, yy, z1, z2,
                              static_cast<double>(p_in[alp]) / val_max, -1.0,
                              refv, pout);
          }
        }
      }
    } else { /* Alphaがない, RGB/Grayscale... */
      for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
        for (int xx = margin; xx < ww + margin;
             ++xx, p_in += cc, pout += cc, refe += cc) {
          const double refv = igs::color::ref_value(refe, cc, r_max, ref_mode);
          if (0 == refv) {
            for (int zz = 0; zz < cc; ++zz) {
              pout[zz] = p_in[zz];
            }
            continue;
          }

          cl_ra.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, refv, pout);
        }
      }
    }
  }
}
}
//--------------------------------------------------------------------
namespace {
class twist_ {
public:
  twist_(const int height, const double xc, const double yc,
         const double twist_radian, const double twist_radius)
      : xc_(xc)
      , yc_(yc)
      , twist_xp_(0)
      , twist_yp_(0)
      , twist_radian_(twist_radian)
      , twist_radius_(twist_radius)
      , current_xp_(0)
      , current_yp_(0)
      , current_radian_(0)
      , current_cos_(0)
      , current_sin_(0)
      , current_radius_(0) {
    /* twist_radiusは、Twist曲線上半径1のPixel画像上半径 */
    /* twist_radiusはゼロのとき、高さの半分を設定する */
    if (this->twist_radius_ <= 0.0) {
      this->twist_radius_ = height / 2.0;
    }
    /* ゼロだと計算不能になる */
    if (this->twist_radius_ <= 0.0) {
      throw std::domain_error("twist_radius is equal less than zero");
    }
  }
  void current_pos(const double xp, const double yp) {
    /* 現在位置 */
    this->current_xp_ = xp;
    this->current_yp_ = yp;

    /* 中心からの距離 */
    const double xv       = xp - this->xc_;
    const double yv       = yp - this->yc_;
    this->current_radius_ = sqrt(xv * xv + yv * yv);

    /* 基準からの距離比 */
    const double tt = this->current_radius_ / this->twist_radius_;

    /* Twist曲線上の位置 */
    const double xx = tt * cos(tt * this->twist_radian_);
    const double yy = tt * sin(tt * this->twist_radian_);

    /* Twist曲線の回転角度 */
    this->current_radian_ = atan2(yv, xv) - atan2(yy, xx);

    /* Twist曲線上の位置を回転角度に傾けた位置 */
    this->current_cos_ = cos(this->current_radian_);
    this->current_sin_ = sin(this->current_radian_);
    this->twist_xp_    = xx * this->current_cos_ - yy * this->current_sin_;
    this->twist_yp_    = xx * this->current_sin_ + yy * this->current_cos_;
  }
  void twist_pos(const double pixel_pos, int &xi, int &yi) {
    /* 基準半径からの比 */
    const double tt = (this->current_radius_ + pixel_pos) / this->twist_radius_;

    /* Twist曲線上の比による位置 */
    const double xx = tt * cos(tt * this->twist_radian_);
    const double yy = tt * sin(tt * this->twist_radian_);

    /* その位置の角度に傾ける */
    double x2 = xx * this->current_cos_ - yy * this->current_sin_;
    double y2 = xx * this->current_sin_ + yy * this->current_cos_;

    /* Twist曲線上の位置の差 */
    x2 = (x2 - this->twist_xp_);
    y2 = (y2 - this->twist_yp_);

    /* Pixel画像上の位置の差 */
    x2 *= this->twist_radius_;
    y2 *= this->twist_radius_;

    /* Pixel画像上の位置 */
    x2 += this->current_xp_;
    y2 += this->current_yp_;

    xi = static_cast<int>(x2);
    yi = static_cast<int>(y2);
  }

private:
  twist_();

  const double xc_;
  const double yc_;

  double twist_xp_;
  double twist_yp_;
  double twist_radian_;
  double twist_radius_;

  double current_xp_;
  double current_yp_;
  double current_radian_;
  double current_cos_;
  double current_sin_;
  double current_radius_;

  /* copy constructorを無効化 */
  twist_(const twist_ &);

  /* 代入演算子を無効化 */
  twist_ &operator=(const twist_ &);
};
//--------------------------------
template <class T>
class radial_twist_ {
public:
  radial_twist_(const T *in_top, const int height /* 求める画像(out)の高さ */
                ,
                const int width /* 求める画像(out)の幅 */
                ,
                const int channels, const double xc, const double yc,
                const double sub_size, const int imax, const double dmax,
                const double intensity /* 平均値ぼかし強度 */
                ,
                const double radius /* 平均値ぼかしの始まる半径 */
                ,
                const double twist_radian, const double twist_radius)
      : in_top_(in_top)
      , hh_(height)
      , ww_(width)
      , cc_(channels)
      , xc_(xc)
      , yc_(yc)
      , sub_size_(sub_size)
      , imax_(imax)
      , dmax_(dmax)
      , intensity_(intensity)
      , radius_(radius)
      , cl_tw_(height, xc, yc, twist_radian, twist_radius) {}
  void pixel_value(const T *in_current_pixel, const int xx, const int yy,
                   const int z1, const int z2, const double ref_increase_val,
                   const double ref_decrease_val,
                   const double each_pixel_blur_ratio, T *result_pixel) {
    /* Pixel位置(0.5 1.5 2.5 ...) */
    const double xp = static_cast<double>(xx) + 0.5;
    const double yp = static_cast<double>(yy) + 0.5;

    /* 中心からPixel位置へのベクトルと長さ */
    const double xv   = xp - this->xc_;
    const double yv   = yp - this->yc_;
    const double dist = sqrt(xv * xv + yv * yv);

    /* 指定半径の範囲内なら何もしない */
    if (dist <= this->radius_) {
      for (int zz = z1; zz <= z2; ++zz) {
        result_pixel[zz] = in_current_pixel[zz];
      }
      return;
    }

    /* Radial方向のSamplingの開始位置と終了位置 */
    double scale = this->intensity_;
    if (0.0 <= each_pixel_blur_ratio) {
      scale *= each_pixel_blur_ratio;
    }
    const double length     = (dist - this->radius_) * scale;
    const double count_half = floor(length / 2.0 / this->sub_size_);
    const double sub_sta    = -this->sub_size_ * count_half;
    const double sub_end    = this->sub_size_ * count_half;

    /* 積算値と積算回数 */
    std::vector<double> accum_val(this->cc_);
    int accum_counter = 0;

    /* SubPixelによるSamplingの相対位置 */
    for (double xsub = this->sub_size_ / 2.0 - 0.5; xsub < 0.5;
         xsub += this->sub_size_) {
      for (double ysub = this->sub_size_ / 2.0 - 0.5; ysub < 0.5;
           ysub += this->sub_size_) {
        /* Sampling位置をセット */
        this->cl_tw_.current_pos(xp + xsub, yp + ysub);

        /* Radial&Twist方向のSampling */
        for (double tt = sub_sta; tt <= sub_end; tt += this->sub_size_) {
          /* Sampling位置からPixel位置を得る */
          int xi = 0;
          int yi = 0;
          this->cl_tw_.twist_pos(tt, xi, yi);

          /* clamp */
          xi = (xi < 0) ? 0 : ((this->ww_ <= xi) ? this->ww_ - 1 : xi);
          yi = (yi < 0) ? 0 : ((this->hh_ <= yi) ? this->hh_ - 1 : yi);

          /* 画像のPixel位置 */
          const T *in_current =
              this->in_top_ + this->cc_ * this->ww_ * yi + this->cc_ * xi;

          /* 積算 */
          for (int zz = z1; zz <= z2; ++zz) {
            accum_val[zz] += static_cast<double>(in_current[zz]);
          }
          ++accum_counter;
        }
      }
    }
    /* 積算しなかったとき(念のためのCheck) */
    if (accum_counter <= 0) {
      for (int zz = z1; zz <= z2; ++zz) {
        result_pixel[zz] = in_current_pixel[zz];
      }
      return;
    }
    /* ここで画像Pixelに保存 */
    for (int zz = z1; zz <= z2; ++zz) {
      accum_val[zz] /= static_cast<double>(accum_counter);

      if ((0 <= ref_increase_val) && (in_current_pixel[zz] < accum_val[zz])) {
        /* 増分のみMask! */
        accum_val[zz] =
            static_cast<double>(in_current_pixel[zz]) +
            (accum_val[zz] - in_current_pixel[zz]) * ref_increase_val;
      } else if ((0 <= ref_decrease_val) &&
                 (accum_val[zz] < in_current_pixel[zz])) {
        /* 減分のみMask! */
        accum_val[zz] =
            static_cast<double>(in_current_pixel[zz]) +
            (accum_val[zz] - in_current_pixel[zz]) * ref_decrease_val;
      }

      accum_val[zz] += 0.5; /* 誤差対策 */
      if (this->dmax_ < accum_val[zz]) {
        result_pixel[zz] = static_cast<T>(this->imax_);
      } else if (accum_val[zz] < 0) {
        result_pixel[zz] = 0;
      } else {
        result_pixel[zz] = static_cast<T>(accum_val[zz]);
      }
    }
  }

private:
  radial_twist_() {}

  const T *in_top_;
  const int hh_;
  const int ww_;
  const int cc_;
  const double xc_;
  const double yc_;
  const double sub_size_;
  const int imax_;
  const double dmax_;
  const double intensity_;
  const double radius_;
  twist_ cl_tw_;

  /* copy constructorを無効化 */
  radial_twist_(const radial_twist_ &);

  /* 代入演算子を無効化 */
  radial_twist_ &operator=(const radial_twist_ &);
};
//--------------------------------
template <class IT, class RT>
void twist_convert_template_(
    const IT *in, const int margin /* 参照画像(in)がもつ余白 */

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_bits, const int ref_mode  // R,G,B,A,luminance

    ,
    IT *out, const int hh /* 求める画像(out)の高さ */
    ,
    const int ww /* 求める画像(out)の幅 */
    ,
    const int cc, const double xc, const double yc, const double twist_radian,
    const double twist_radius,
    const double intensity /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    ,
    const double radius /* 平均値ぼかしの始まる半径 */
    ,
    const int sub_div /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw) {
  ref_bits;  // for warning

  /* 強度のないとき、または、サブ分割がないとき、なにもしない */
  if (intensity <= 0.0 || sub_div <= 0) {
    return;
  }

  radial_twist_<IT> cl_ratw(in, hh + margin * 2, ww + margin * 2, cc,
                            xc + margin, yc + margin, 1.0 / sub_div,
                            std::numeric_limits<IT>::max(),
                            static_cast<double>(std::numeric_limits<IT>::max()),
                            intensity, radius, twist_radian, twist_radius);

#if defined RGBA_ORDER_OF_TOONZ6
  const int z1 = igs::image::rgba::blu;
  const int z2 = igs::image::rgba::red;
#elif defined RGBA_ORDER_OF_OPENGL
  const int z1 = igs::image::rgba::red;
  const int z2 = igs::image::rgba::blu;
#else
  Must be define / DRGBA_ORDER_OF_TOONZ6 or
      / DRGBA_ORDER_OF_OPENGL
#endif

  const IT *p_in = in + margin * (ww + margin * 2) * cc + margin * cc;
  IT *pout       = out;

  if (0 == ref) { /* 参照なし */
    if (igs::image::rgba::siz == cc) {
      using namespace igs::image::rgba;
      if (alpha_rendering_sw) { /* Alphaも処理する */
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc) {
            cl_ratw.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, -1.0, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ratw.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, -1.0, pout);
          }
        }
      } else { /* Alpha処理しない、RGB増分をAlphaでMaskする */
        const unsigned int val_max = std::numeric_limits<IT>::max();
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc) {
            pout[alp] = p_in[alp];
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ratw.pixel_value(p_in, xx, yy, z1, z2,
                                static_cast<double>(p_in[alp]) / val_max, -1.0,
                                -1.0, pout);
          }
        }
      }
    } else { /* Alphaがない, RGB/Grayscale... */
      for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
        for (int xx = margin; xx < ww + margin; ++xx, p_in += cc, pout += cc) {
          cl_ratw.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, -1.0, pout);
        }
      }
    }
  } else { /* 参照あり */
    const RT *refe  = ref;
    const int r_max = std::numeric_limits<RT>::max();
    if (igs::image::rgba::siz == cc) {
      using namespace igs::image::rgba;
      if (alpha_rendering_sw) { /* Alphaも処理する */
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc, refe += cc) {
            const double refv =
                igs::color::ref_value(refe, cc, r_max, ref_mode);
            if (0 == refv) {
              for (int zz = 0; zz < cc; ++zz) {
                pout[zz] = p_in[zz];
              }
              continue;
            }

            cl_ratw.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, refv, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ratw.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, refv, pout);
          }
        }
      } else { /* Alpha処理しない、RGB増分をAlphaでMaskする */
        const unsigned int val_max = std::numeric_limits<IT>::max();
        for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
          for (int xx = margin; xx < ww + margin;
               ++xx, p_in += cc, pout += cc, refe += cc) {
            const double refv =
                igs::color::ref_value(refe, cc, r_max, ref_mode);
            if (0 == refv) {
              for (int zz = 0; zz < cc; ++zz) {
                pout[zz] = p_in[zz];
              }
              continue;
            }

            pout[alp] = p_in[alp];
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_ratw.pixel_value(p_in, xx, yy, z1, z2,
                                static_cast<double>(p_in[alp]) / val_max, -1.0,
                                refv, pout);
          }
        }
      }
    } else { /* Alphaがない, RGB/Grayscale... */
      for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
        for (int xx = margin; xx < ww + margin;
             ++xx, p_in += cc, pout += cc, refe += cc) {
          const double refv = igs::color::ref_value(refe, cc, r_max, ref_mode);
          if (0 == refv) {
            for (int zz = 0; zz < cc; ++zz) {
              pout[zz] = p_in[zz];
            }
            continue;
          }

          cl_ratw.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, refv, pout);
        }
      }
    }
  }
}
}
//--------------------------------------------------------------------
void igs::radial_blur::convert(
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
    const double xc, const double yc, const double twist_radian,
    const double twist_radius
    /*
Twist Radius
    ひねりの基準半径を指定します。
    単位はミリメートルです。
    最小は0。最大1000mm(1m)です。
    初期値は0でひねりはありません。
*/
    ,
    const double intensity /* 強度。ゼロより大きく2以下 */
    /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
    ,
    const double radius /* 平均値ぼかしの始まる半径 */
    ,
    const int sub_div /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* bit(monoBW) */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  if ((std::numeric_limits<unsigned char>::digits != bits) &&
      (std::numeric_limits<unsigned short>::digits != bits)) {
    throw std::domain_error("Bad in bits,Not uchar/ushort");
  }
  if ((0 != ref) && (std::numeric_limits<unsigned char>::digits != ref_bits) &&
      (std::numeric_limits<unsigned short>::digits != ref_bits)) {
    throw std::domain_error("Bad ref bits,Not uchar/ushort");
  }

  /* 強度のないとき、または、サブ分割がないとき */
  if (intensity <= 0.0 || sub_div <= 0) {
    if (std::numeric_limits<unsigned char>::digits == bits) {
      igs::image::copy_except_margin(in, margin, out, height, width, channels);
    } else if (std::numeric_limits<unsigned short>::digits == bits) {
      igs::image::copy_except_margin(
          reinterpret_cast<const unsigned short *>(in), margin,
          reinterpret_cast<unsigned short *>(out), height, width, channels);
    }
    return;
  }

  if (0.0 == twist_radian) {
    if (std::numeric_limits<unsigned char>::digits == ref_bits) {
      if (std::numeric_limits<unsigned char>::digits == bits) {
        radial_convert_template_(in, margin, ref, ref_bits, ref_mode, out,
                                 height, width, channels, xc, yc, intensity,
                                 radius, sub_div, alpha_rendering_sw);
      } else if (std::numeric_limits<unsigned short>::digits == bits) {
        radial_convert_template_(
            reinterpret_cast<const unsigned short *>(in), margin, ref, ref_bits,
            ref_mode, reinterpret_cast<unsigned short *>(out), height, width,
            channels, xc, yc, intensity, radius, sub_div, alpha_rendering_sw);
      }
    } else { /* ref_bitsがゼロでも(refがなくても)ここにくる */
      if (std::numeric_limits<unsigned char>::digits == bits) {
        radial_convert_template_(
            in, margin, reinterpret_cast<const unsigned short *>(ref), ref_bits,
            ref_mode, out, height, width, channels, xc, yc, intensity, radius,
            sub_div, alpha_rendering_sw);
      } else if (std::numeric_limits<unsigned short>::digits == bits) {
        radial_convert_template_(
            reinterpret_cast<const unsigned short *>(in), margin,
            reinterpret_cast<const unsigned short *>(ref), ref_bits, ref_mode,
            reinterpret_cast<unsigned short *>(out), height, width, channels,
            xc, yc, intensity, radius, sub_div, alpha_rendering_sw);
      }
    }
  } else {
    if (std::numeric_limits<unsigned char>::digits == ref_bits) {
      if (std::numeric_limits<unsigned char>::digits == bits) {
        twist_convert_template_(in, margin, ref, ref_bits, ref_mode, out,
                                height, width, channels, xc, yc, twist_radian,
                                twist_radius, intensity, radius, sub_div,
                                alpha_rendering_sw);
      } else if (std::numeric_limits<unsigned short>::digits == bits) {
        twist_convert_template_(
            reinterpret_cast<const unsigned short *>(in), margin, ref, ref_bits,
            ref_mode, reinterpret_cast<unsigned short *>(out), height, width,
            channels, xc, yc, twist_radian, twist_radius, intensity, radius,
            sub_div, alpha_rendering_sw);
      }
    } else { /* ref_bitsがゼロでも(refがなくても)ここにくる */
      if (std::numeric_limits<unsigned char>::digits == bits) {
        twist_convert_template_(
            in, margin, reinterpret_cast<const unsigned short *>(ref), ref_bits,
            ref_mode, out, height, width, channels, xc, yc, twist_radian,
            twist_radius, intensity, radius, sub_div, alpha_rendering_sw);
      } else if (std::numeric_limits<unsigned short>::digits == bits) {
        twist_convert_template_(
            reinterpret_cast<const unsigned short *>(in), margin,
            reinterpret_cast<const unsigned short *>(ref), ref_bits, ref_mode,
            reinterpret_cast<unsigned short *>(out), height, width, channels,
            xc, yc, twist_radian, twist_radius, intensity, radius, sub_div,
            alpha_rendering_sw);
      }
    }
  }
}
#if 0   //-------------------- comment out start ------------------------
namespace {
 double enlarge_margin_length_(
	const double xc
	,const double yc
	,const double xp
	,const double yp
	,const double intensity
	,const double radius
	,const double sub_size
 ) {
	const double xv = xp - xc;
	const double yv = yp - yc;
	const double dist = sqrt(xv*xv + yv*yv);

	/* "dist = len - (len - radius) * intensity / 2.0;"より */
	const double len = (2.0 * dist - radius * intensity) /
				(2.0-intensity);

	/***const double length = (len - radius) * intensity;
	const double count_half = floor(length / sub_size);
	return sub_size * count_half;***/

	if (len <= radius) { return 0; }
	return (len - radius) * intensity / 2.0;
 }
}
int igs::radial_blur::enlarge_margin( /* Twist時は正確ではない... */
	const int height
	,const int width
	,const double xc
	,const double yc
	,const double twist_radian
	,const double twist_radius
	,const double intensity/* 強度。ゼロより大きく2以下 */
 /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい */
	,const double radius	/* 平均値ぼかしの始まる半径 */
	,const int sub_div	/* 1ならJaggy、2以上はAntialias */
) {
	/* 強度のないとき、2以上のとき、または、サブ分割がないとき、
	なにもしない */
	if (intensity <= 0.0 || 2.0 <= intensity || sub_div <= 0) {
		return 0;
	}

	double margin1 = 0;
	double margin2 = 0;

	margin1 = enlarge_margin_length_(
	  xc,yc,-width/2.0,-height/2.0,intensity,radius,1.0/sub_div);

	margin2 = enlarge_margin_length_(
	  xc,yc,-width/2.0, height/2.0,intensity,radius,1.0/sub_div);
	if (margin1 < margin2) { margin1 = margin2; }

	margin2 = enlarge_margin_length_(
	  xc,yc, width/2.0,-height/2.0,intensity,radius,1.0/sub_div);
	if (margin1 < margin2) { margin1 = margin2; }

	margin2 = enlarge_margin_length_(
	  xc,yc, width/2.0, height/2.0,intensity,radius,1.0/sub_div);
	if (margin1 < margin2) { margin1 = margin2; }

	return static_cast<int>(ceil(margin1));
}
#endif  //-------------------- comment out end -------------------------
namespace {
double reference_margin_length_(const double xc, const double yc,
                                const double xp, const double yp,
                                const double intensity, const double radius,
                                const double sub_size) {
  const double xv   = xp - xc;
  const double yv   = yp - yc;
  const double dist = sqrt(xv * xv + yv * yv);
  if (dist <= radius) {
    return 0;
  }
  const double half_length = (dist - radius) * intensity / 2.0;
  const double count_half  = floor(half_length / sub_size);
  return sub_size * count_half;
}
}
int igs::radial_blur::
    reference_margin(/* Twist時は正確ではない... */
                     const int height, const int width, const double xc,
                     const double yc, const double twist_radian,
                     const double twist_radius,
                     const double intensity /* 強度。ゼロより大きく2以下 */
                     /* radius円境界での平均値ぼかしゼロとするためintensityは2より小さい
                        */
                     ,
                     const double radius /* 平均値ぼかしの始まる半径 */
                     ,
                     const int sub_div /* 1ならJaggy、2以上はAntialias */
                     ) {
  twist_radius;  // for warning
  twist_radian;  // for warning

  /* 強度のないとき、2以上のとき、または、サブ分割がないとき、
  なにもしない */
  if (intensity <= 0.0 || 2.0 <= intensity || sub_div <= 0) {
    return 0;
  }

  double margin1 = 0;
  double margin2 = 0;

  /*
  四隅から参照する外部への最大マージンを計算する
  */
  margin1 = reference_margin_length_(xc, yc, -width / 2.0, -height / 2.0,
                                     intensity, radius, 1.0 / sub_div);

  margin2 = reference_margin_length_(xc, yc, -width / 2.0, height / 2.0,
                                     intensity, radius, 1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(xc, yc, width / 2.0, -height / 2.0,
                                     intensity, radius, 1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(xc, yc, width / 2.0, height / 2.0,
                                     intensity, radius, 1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  return static_cast<int>(ceil(margin1));
}
