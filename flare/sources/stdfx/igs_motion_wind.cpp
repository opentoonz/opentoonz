#include <limits>
#include <stdexcept> /* std::domain_error(-) */
#include "igs_motion_wind_pixel.h"

namespace {
template <class T>
const T *ref_pixel_(const T *ref, const int hh, const int ww, const int cc,
                    int xx, int yy) {
  while (xx < 0) {
    xx += ww;
  }
  while (ww <= xx) {
    xx -= ww;
  }
  while (yy < 0) {
    yy += hh;
  }
  while (hh <= yy) {
    yy -= hh;
  }
  return ref + cc * ww * yy + cc * xx;
}
template <class T>
void pixel_pop_(const T *in, const double div_val, const int cc, double *out) {
  /* 正規化し取り出し */
  for (int zz = 0; zz < cc; ++zz) {
    out[zz] = static_cast<double>(in[zz]) / div_val;
  }
}
template <class T>
void pixel_push_(const double *in, const double mul_val, const int cc, T *out) {
  /* 値が変化したら格納する */
  for (int zz = 0; zz < cc; ++zz) {
    out[zz] = static_cast<T>(in[zz] * mul_val);
  }
}
//-----------------------------------------------------------
template <class T, class R>
void change_template_vert_(
    T *in, const int ww, const int hh, const int cc, const R *ref, const int rw,
    const int rh, const int rc
    //, const int rz
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    igs::motion_wind::pixel &pix_rend, const bool reverse_sw) {
  const unsigned int max_val = std::numeric_limits<T>::max();
  const double div_val       = static_cast<double>(max_val);
  const double mul_val       = div_val + 0.999999;

  const unsigned int ref_max_val = std::numeric_limits<R>::max();
  const double ref_div_val       = static_cast<double>(ref_max_val);

  const int refprev = (reverse_sw) ? 1 : -1;

  for (int xx = 0; xx < ww; ++xx) {
    for (int yy = 0; yy < hh; ++yy) {
      T *next = in;
      if (reverse_sw) { /* 上から */
        next += (hh - 1 - yy) * ww * cc + xx * cc;
      } else { /* 下から */
        next += yy * ww * cc + xx * cc;
      }
      double pixel_in[igs::image::rgba::siz];
      pixel_pop_(next, div_val, cc, pixel_in);

      /****double pixel_ref[igs::image::rgba::siz];
double *rp_p = 0;
if (0 != ref) {
      pixel_pop_(
              ref_pixel_(ref,rh,rw,rc,xx,yy+refprev)
              , ref_div_val, rc, pixel_ref);
      rp_p = pixel_ref;
}
if (pix_rend.change(xx <= 0, rz, rp_p, cc, pixel_in)) {
      pixel_push_(pixel_in,mul_val,cc,next);
} ***/

      double ref_val = -1.0;
      if (0 != ref && 0 <= ref_mode && ref_mode <= 4) {
        ref_val =
            igs::color::ref_value(ref_pixel_(ref, rh, rw, rc, xx, yy + refprev),
                                  rc, ref_max_val, ref_mode);
      }

      if (pix_rend.change(xx <= 0, ref_val, cc, pixel_in)) {
        pixel_push_(pixel_in, mul_val, cc, next);
      }
    }
  }
}
template <class T, class R>
void change_template_hori_(
    T *in, const int ww, const int hh, const int cc, const R *ref, const int rw,
    const int rh, const int rc
    //, const int rz
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    igs::motion_wind::pixel &pix_rend, const bool reverse_sw) {
  const unsigned int max_val = std::numeric_limits<T>::max();
  const double div_val       = static_cast<double>(max_val);
  const double mul_val       = div_val + 0.999999;

  const unsigned int ref_max_val = std::numeric_limits<R>::max();
  const double ref_div_val       = static_cast<double>(ref_max_val);

  const int refprev = (reverse_sw) ? 1 : -1;

  for (int yy = 0; yy < hh; ++yy) {
    T *next = in; /* 左から */
    if (reverse_sw) {
      next += (ww - 1) * cc;
    } /* 右から */
    for (int xx = 0; xx < ww; ++xx) {
      double pixel_in[igs::image::rgba::siz];
      pixel_pop_(next, div_val, cc, pixel_in);

      /***double pixel_ref[igs::image::rgba::siz];
double *rp_p = 0;
if (0 != ref) {
      pixel_pop_(
              ref_pixel_(ref,rh,rw,rc,xx+refprev,yy)
              , ref_div_val, rc, pixel_ref);
      rp_p = pixel_ref;
}
if (pix_rend.change(xx <= 0, rz, rp_p, cc, pixel_in)) {
      pixel_push_(pixel_in,mul_val,cc,next);
}***/

      double ref_val = -1.0;
      if (0 != ref && 0 <= ref_mode && ref_mode <= 4) {
        ref_val =
            igs::color::ref_value(ref_pixel_(ref, rh, rw, rc, xx, yy + refprev),
                                  rc, ref_max_val, ref_mode);
      }

      if (pix_rend.change(xx <= 0, ref_val, cc, pixel_in)) {
        pixel_push_(pixel_in, mul_val, cc, next);
      }

      if (reverse_sw) {
        next -= cc;
      } /* 右から左 */
      else {
        next += cc;
      } /* 左から右 */
    }
    in += ww * cc; /* 下から上 */
  }
}
}
/*-------------------------------------------------------
        Parameter
        方向			left-bottom is origin.
                0	:   0 : left to right
                1	:  90 : bottom to top
                2	: 180 : right to left
                3	: 270 : top to bottom
        暗い風へ変更するスイッチ
                true	: blow dark
                false	: blow light
        アルファにも風吹くスイッチ
                true	: blow alpha
                false	: not blow alpha
        風長さ(ランダム初期種)		1(0...)
        風長さ(最小値)			0(0...)
        風長さ(最大値)			18(0...)
        風長さ(最大最小のかたより値)	0.0(-1.0...0.0...1.0)
        風長さ(参照スイッチ)		false(true/false)

        風曲線(ランダム初期種)		1(0...)
        風曲線(最小値)			0(0...)
        風曲線(最大値)			1(0...)
        風曲線(最大最小のかたより値)	0.0(-1.0...0.0...1.0)
        風曲線(参照スイッチ)		false(true/false)

        風高さ(ランダム初期種)		1(0...)
        風高さ(最小値)			0(0...)
        風高さ(最大値)			1(0...)
        風高さ(最大最小のかたより値)	0.0(-1.0...0.0...1.0)
        風高さ(参照スイッチ)		false(true/false)
-------------------------------------------------------*/
#include "igs_motion_wind.h"
void igs::motion_wind::change(
    unsigned char *in

    ,
    const int hh, const int ww, const int cc, const int bb

    ,
    const unsigned char *ref

    ,
    const int rh, const int rw, const int rc, const int rb

    //, const int rz
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const int direction  // 0(0(LtoR),1(BtoT),2(RtoL),3(TtoB))
    ,
    const bool blow_dark_sw  // false(false,true)
    ,
    const bool blow_alpha_sw  // true(false,true)

    ,
    const unsigned long length_random_seed  // 0(0...)
    ,
    const double length_min  // 0.0...
    ,
    const double length_max  // 18.0...
    ,
    const double length_bias  // 1.0(0.0<...1.0...)
    ,
    const bool length_ref_sw  // false(false,true)

    ,
    const unsigned long force_random_seed  // 0(0...)
    ,
    const double force_min  // 0.0...
    ,
    const double force_max  // 1.0...
    ,
    const double force_bias  // 1.0(0.0<...1.0...)
    ,
    const bool force_ref_sw  // false(false,true)

    ,
    const unsigned long density_random_seed  // 0(0...)
    ,
    const double density_min  // 0.0...
    ,
    const double density_max  // 1.0...
    ,
    const double density_bias  // 1.0(0.0<...1.0...)
    ,
    const bool density_ref_sw  // false(false,true)
    ) {
  /* ユーザーによるノーアクションの指定、つまりゼロ長
  の時は処理せず終る */
  if ((length_max <= 0.0) && (length_min <= 0.0)) {
    return;
  }
  //------------
  igs::motion_wind::pixel pix_rend(
      blow_dark_sw, blow_alpha_sw, length_random_seed, length_min, length_max,
      length_bias, length_ref_sw, force_random_seed, force_min, force_max,
      force_bias, force_ref_sw, density_random_seed, density_min, density_max,
      density_bias, density_ref_sw);
  //------------
  const int bi    = static_cast<int>(bb);
  const int refbi = static_cast<int>(rb);
  if (std::numeric_limits<unsigned char>::digits == bi) {
    if (std::numeric_limits<unsigned short>::digits == refbi) {
      if ((0 == direction) || (2 == direction)) {
        change_template_hori_(
            in, ww, hh, cc, reinterpret_cast<const unsigned short *>(ref), rw,
            rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (0 == direction) ? false : true);
      } else if ((1 == direction) || (3 == direction)) {
        change_template_vert_(
            in, ww, hh, cc, reinterpret_cast<const unsigned short *>(ref), rw,
            rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (1 == direction) ? false : true);
      }
    } else { /* refbi is 8(uchar) or other(0...) */
      if ((0 == direction) || (2 == direction)) {
        change_template_hori_(in, ww, hh, cc, ref, rw, rh, rc
                              //,rz
                              ,
                              ref_mode, pix_rend,
                              (0 == direction) ? false : true);
      } else if ((1 == direction) || (3 == direction)) {
        change_template_vert_(in, ww, hh, cc, ref, rw, rh, rc
                              //,rz
                              ,
                              ref_mode, pix_rend,
                              (1 == direction) ? false : true);
      }
    }
  } else if (std::numeric_limits<unsigned short>::digits == bi) {
    if (std::numeric_limits<unsigned short>::digits == refbi) {
      if ((0 == direction) || (2 == direction)) {
        change_template_hori_(
            reinterpret_cast<unsigned short *>(in), ww, hh, cc,
            reinterpret_cast<const unsigned short *>(ref), rw, rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (0 == direction) ? false : true);
      } else if ((1 == direction) || (3 == direction)) {
        change_template_vert_(
            reinterpret_cast<unsigned short *>(in), ww, hh, cc,
            reinterpret_cast<const unsigned short *>(ref), rw, rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (1 == direction) ? false : true);
      }
    } else { /* refbi is 8(uchar) or other(0...) */
      if ((0 == direction) || (2 == direction)) {
        change_template_hori_(
            reinterpret_cast<unsigned short *>(in), ww, hh, cc, ref, rw, rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (0 == direction) ? false : true);
      } else if ((1 == direction) || (3 == direction)) {
        change_template_vert_(
            reinterpret_cast<unsigned short *>(in), ww, hh, cc, ref, rw, rh, rc
            //,rz
            ,
            ref_mode, pix_rend, (1 == direction) ? false : true);
      }
    }
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }

  pix_rend.clear();
}
