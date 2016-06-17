#include <iostream>   // std::cout
#include <vector>     // std::vector
#include <cmath>      // cos(),sin(),sqrt()
#include <limits>     // std::numeric_limits<T>
#include <stdexcept>  // std::domain_error()
#include "igs_ifx_common.h"
#include "igs_rotate_blur.h"
namespace {
//------------------------------------------------------------------
template <class T>
class rotate_ {
public:
  rotate_(const T *in_top, const int height, const int width,
          const int channels, const double xc, const double yc,
          const double sub_size, const int imax, const double dmax,
          const double radian, const double blur_radius,
          const double spin_radius)
      : in_top_(in_top)
      , hh_(height)
      , ww_(width)
      , cc_(channels)
      , xc_(xc)
      , yc_(yc)
      , sub_size_(sub_size)
      , imax_(imax)
      , dmax_(dmax)
      , radian_(radian)
      , blur_radius_(blur_radius)
      , spin_radius_(spin_radius) {}
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
    if (dist <= this->blur_radius_) {
      for (int zz = z1; zz <= z2; ++zz) {
        result_pixel[zz] = in_current_pixel[zz];
      }
      return;
    }

    /* 中心からPixel位置への単位ベクトル */
    const double cosval = xv / dist;
    const double sinval = yv / dist;

    /* 円周接線方向Sampling１つずらした位置への回転角度 */
    const double xw = xv - this->sub_size_ * sinval;
    const double yw = yv + this->sub_size_ * cosval;
    const double sub_radian =
        acos((xv * xw + yv * yw) /
             (sqrt(xv * xv + yv * yv) * sqrt(xw * xw + yw * yw)));

    /* 積算値と積算回数 */
    std::vector<double> accum_val(this->cc_);
    int accum_counter = 0;

    /* 参照画像による強弱付加 */
    double scale = this->radian_;
    if (0.0 <= each_pixel_blur_ratio) {
      scale *= each_pixel_blur_ratio;
    }

    /* Radial方向Sampling */
    for (double ss = this->sub_size_ / 2.0 - 0.5; ss < 0.5;
         ss += this->sub_size_) {
      /* Radial方向位置 */
      const double xp2 = xp + ss * cosval;
      const double yp2 = yp + ss * sinval;

      /* ぼかす回転角度範囲range */
      double radian = scale;

      if (0.0 < this->spin_radius_) { /* 外への強調 */
        const double xv2   = xp2 - this->xc_;
        const double yv2   = yp2 - this->yc_;
        const double dist2 = sqrt(xv2 * xv2 + yv2 * yv2);
        radian *=
            /**(dist2              - this->blur_radius_) /
(this->spin_radius_ - this->blur_radius_);**/
            (dist2 - this->blur_radius_) / this->spin_radius_;
      }

      /* Sampling回数 */
      int sub_count = static_cast<int>(radian / sub_radian);

      /* Sampling開始角度 */
      const double sub_start =
          (radian - sub_count * sub_radian) / 2.0 - radian / 2.0;

      /* 円周方向Sampling */
      for (double rr = sub_start; 0 < sub_count--; rr += sub_radian) {
        /* Radial位置(xp2,yp2)から円周位置(xx,yy)へ */
        const double cosval2 = cos(rr);
        const double sinval2 = sin(rr);

        const double xd = xp2 - this->xc_;
        const double yd = yp2 - this->yc_;

        int xx = static_cast<int>(xd * cosval2 - yd * sinval2 + this->xc_);
        int yy = static_cast<int>(xd * sinval2 + yd * cosval2 + this->yc_);

        /* clamp */
        xx = (xx < 0) ? 0 : ((this->ww_ <= xx) ? this->ww_ - 1 : xx);
        yy = (yy < 0) ? 0 : ((this->hh_ <= yy) ? this->hh_ - 1 : yy);

        /* 画像のPixel位置 */
        const T *in_current =
            this->in_top_ + this->cc_ * yy * this->ww_ + this->cc_ * xx;

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
  rotate_() {}

  const T *in_top_;
  const int hh_;
  const int ww_;
  const int cc_;
  const double xc_;
  const double yc_;
  const double sub_size_;
  const int imax_;
  const double dmax_;
  const double radian_;
  const double blur_radius_;
  const double spin_radius_;

  /* copy constructorを無効化 */
  rotate_(const rotate_ &);

  /* 代入演算子を無効化 */
  rotate_ &operator=(const rotate_ &);
};
//------------------------------------------------------------------
const double pi = 3.14159265358979;
template <class IT, class RT>
void rotate_convert_template_(
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
    const int cc, const double xc, const double yc, const double degree,
    const double blur_radius /* ぼかしの始まる半径 */
    ,
    const double spin_radius /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
    ,
    const int sub_div /* 1ならJaggy、2以上はAntialias */
    ,
    const bool alpha_rendering_sw) {
  ref_bits;  // for warning

  /* 強度のないとき、または、サブ分割がないとき、なにもしない */
  if (degree <= 0.0 || sub_div <= 0
      // || spin_radius<=blur_radius
      ) {
    return;
  }

  rotate_<IT> cl_rota(in, hh + margin * 2, ww + margin * 2, cc, xc + margin,
                      yc + margin, 1.0 / sub_div,
                      std::numeric_limits<IT>::max(),
                      static_cast<double>(std::numeric_limits<IT>::max()),
                      degree * pi / 180.0, blur_radius, spin_radius);

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
            cl_rota.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, -1.0, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_rota.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, -1.0, pout);
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
            cl_rota.pixel_value(p_in, xx, yy, z1, z2,
                                static_cast<double>(p_in[alp]) / val_max, -1.0,
                                -1.0, pout);
          }
        }
      }
    } else { /* Alphaがない, RGB/Grayscale... */
      for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
        for (int xx = margin; xx < ww + margin; ++xx, p_in += cc, pout += cc) {
          cl_rota.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, -1.0, pout);
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

            cl_rota.pixel_value(p_in, xx, yy, alp, alp, -1.0, -1.0, refv, pout);
            if (0 == pout[alp]) {
              pout[red] = p_in[red];
              pout[gre] = p_in[gre];
              pout[blu] = p_in[blu];
              continue;
            }
            cl_rota.pixel_value(p_in, xx, yy, z1, z2, -1.0, -1.0, refv, pout);
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
            cl_rota.pixel_value(p_in, xx, yy, z1, z2,
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

          cl_rota.pixel_value(p_in, xx, yy, 0, cc - 1, -1.0, -1.0, refv, pout);
        }
      }
    }
  }
}
//------------------------------------------------------------------
}

void igs::rotate_blur::convert(
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
    const double xc, const double yc, const double degree /* ぼかしの回転角度 */
    ,
    const double blur_radius /* ぼかしの始まる半径 */
    ,
    const double spin_radius /* ゼロ以上でspin指定となり、
                            かつぼかし強弱の一定になる半径となる */
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
  if (degree <= 0.0 || sub_div <= 0
      // || spin_radius<=blur_radius
      ) {
    if (std::numeric_limits<unsigned char>::digits == bits) {
      igs::image::copy_except_margin(in, margin, out, height, width, channels);
    } else if (std::numeric_limits<unsigned short>::digits == bits) {
      igs::image::copy_except_margin(
          reinterpret_cast<const unsigned short *>(in), margin,
          reinterpret_cast<unsigned short *>(out), height, width, channels);
    }
    return;
  }

  if (std::numeric_limits<unsigned char>::digits == ref_bits) {
    if (std::numeric_limits<unsigned char>::digits == bits) {
      rotate_convert_template_(in, margin, ref, ref_bits, ref_mode, out, height,
                               width, channels, xc, yc, degree, blur_radius,
                               spin_radius, sub_div, alpha_rendering_sw);
    } else if (std::numeric_limits<unsigned short>::digits == bits) {
      rotate_convert_template_(reinterpret_cast<const unsigned short *>(in),
                               margin, ref, ref_bits, ref_mode,
                               reinterpret_cast<unsigned short *>(out), height,
                               width, channels, xc, yc, degree, blur_radius,
                               spin_radius, sub_div, alpha_rendering_sw);
    }
  } else { /* ref_bitsがゼロでも(refがなくても)ここにくる */
    if (std::numeric_limits<unsigned char>::digits == bits) {
      rotate_convert_template_(
          in, margin, reinterpret_cast<const unsigned short *>(ref), ref_bits,
          ref_mode, out, height, width, channels, xc, yc, degree, blur_radius,
          spin_radius, sub_div, alpha_rendering_sw);
    } else if (std::numeric_limits<unsigned short>::digits == bits) {
      rotate_convert_template_(
          reinterpret_cast<const unsigned short *>(in), margin,
          reinterpret_cast<const unsigned short *>(ref), ref_bits, ref_mode,
          reinterpret_cast<unsigned short *>(out), height, width, channels, xc,
          yc, degree, blur_radius, spin_radius, sub_div, alpha_rendering_sw);
    }
  }
}
//--------------------------------------------------------------------
namespace {
double reference_margin_length_(const double xc, const double yc,
                                const double xp, const double yp, double radian,
                                const double blur_radius,
                                const double spin_radius,
                                const double sub_size) {
  sub_size;  // for warning

  const double xv = xp - xc;
  const double yv = yp - yc;

  if (0.0 < spin_radius) { /* 外への強調 */
    const double dist = sqrt(xv * xv + yv * yv);
    // radian *= (dist-blur_radius)/(spin_radius-blur_radius);
    radian *= (dist - blur_radius) / spin_radius;
  }

  double cosval      = cos(radian / 2.0);
  double sinval      = sin(radian / 2.0);
  const double x1    = xv * cosval - yv * sinval + xc;
  const double y1    = xv * sinval + yv * cosval + yc;
  const double xv1   = x1 - xp;
  const double yv1   = y1 - yp;
  const double dist1 = sqrt(xv1 * xv1 + yv1 * yv1); /* 必ずプラス値 */

  cosval             = cos(-radian / 2.0);
  sinval             = sin(-radian / 2.0);
  const double xp2   = xv * cosval - yv * sinval + xc;
  const double yp2   = xv * sinval + yv * cosval + yc;
  const double xv2   = xp2 - xp;
  const double yv2   = yp2 - yp;
  const double dist2 = sqrt(xv2 * xv2 + yv2 * yv2); /* 必ずプラス値 */

  return (dist1 < dist2) ? dist2 : dist1;
}
}
int igs::rotate_blur::reference_margin(
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
    ) {
  /* 強度のないとき、または、サブ分割がないとき、なにもしない */
  if (degree <= 0.0 || sub_div <= 0
      // || spin_radius<=blur_radius
      ) {
    return 0;
  }

  double margin1 = 0;
  double margin2 = 0;

  double deg = degree;
  if (180.0 < deg) {
    deg = 180.0;
  }

  margin1 = reference_margin_length_(xc, yc, -width / 2.0, -height / 2.0,
                                     deg * pi / 180.0, blur_radius, spin_radius,
                                     1.0 / sub_div);

  margin2 = reference_margin_length_(xc, yc, -width / 2.0, height / 2.0,
                                     deg * pi / 180.0, blur_radius, spin_radius,
                                     1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(xc, yc, width / 2.0, -height / 2.0,
                                     deg * pi / 180.0, blur_radius, spin_radius,
                                     1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  margin2 = reference_margin_length_(xc, yc, width / 2.0, height / 2.0,
                                     deg * pi / 180.0, blur_radius, spin_radius,
                                     1.0 / sub_div);
  if (margin1 < margin2) {
    margin1 = margin2;
  }

  return static_cast<int>(ceil(margin1));
}
