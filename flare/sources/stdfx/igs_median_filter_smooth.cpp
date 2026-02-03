#include <cmath> /* sqrt() */
#include <vector>
#include "igs_ifx_common.h"

namespace {
/* 画像の外への参照が必要なときどう拾うか */
enum outside_of_image_ {
  is_spread_edge_ /* 今はここしか使ってない2013-11-12 */
  ,
  is_flip_repeat_,
  is_black_,
  is_repeat_
};
/* 影響範囲(radius)の各ピクセルのgeometry情報 */
class pixel_geometry_ {
public:
  pixel_geometry_(const double radius, const outside_of_image_ type)
      : type_(type), ratio_total(0.0) {
    /* 辿るためピクセル整数位置 */
    const int radius_int = static_cast<int>(ceil(radius));

    /* 任意半径(double値)の中のピクセル数を数える */
    const double rxr = radius * radius + 1e-6;
    for (int yy = -radius_int; yy <= radius_int; ++yy) {
      const double yxy = static_cast<double>(yy) * yy;
      for (int xx = -radius_int; xx <= radius_int; ++xx) {
        const double xxx_plus_yxy = static_cast<double>(xx) * xx + yxy;
        if (xxx_plus_yxy <= rxr) { /* 円の内部なら */
          /* 円の中心位置を原点(0,0)とした座標 */
          this->xp.push_back(xx);
          this->yp.push_back(yy);

          /* 中心からpixelまでの距離(半径) */
          const double pixel_radius = sqrt(xxx_plus_yxy);

          /* 影響力を現す値
円縁から0...1の距離ならそのまま、
1以上離れて中心までは1とする */
          double ratio = radius - pixel_radius;
          if (1.0 < ratio) {
            ratio = 1.0;
          }
          this->in_out_ratio.push_back(ratio);
          this->ratio_total += ratio;
        }
      }
    }
  }
  void re_position(const int ww, const int hh, int &xx, int &yy) {
    switch (this->type_) {
    case is_spread_edge_: /* 外枠のピクセル値を広げる */
      xx = (xx < 0) ? 0 : ((ww <= xx) ? ww - 1 : xx);
      yy = (yy < 0) ? 0 : ((hh <= yy) ? hh - 1 : yy);
      break;
    case is_flip_repeat_: /* 同じ絵を上下左右に反転繰り返す */
    {
      int ii = 0;
      while (xx < 0) {
        xx += ww;
        ++ii;
      }
      if ((ii % 2) == 1) {
        xx = ww - 1 - xx;
      }
    }
      {
        int ii = 0;
        while (ww <= xx) {
          xx -= ww;
          ++ii;
        }
        if ((ii % 2) == 1) {
          xx = ww - 1 - xx;
        }
      }
      {
        int ii = 0;
        while (yy < 0) {
          yy += hh;
          ++ii;
        }
        if ((ii % 2) == 1) {
          yy = hh - 1 - yy;
        }
      }
      {
        int ii = 0;
        while (hh <= yy) {
          yy -= hh;
          ++ii;
        }
        if ((ii % 2) == 1) {
          yy = hh - 1 - yy;
        }
      }
      break;
    case is_black_: /* -1として外関数で黒塗り処理する */
      if (xx < 0 || ww <= xx) {
        xx = -1;
      }
      if (yy < 0 || hh <= yy) {
        yy = -1;
      }
      break;
    case is_repeat_: /* 同じ絵を上下左右繰り返す */
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
      break;
    }
  }
  void clear(void) {
    this->yp.clear();
    this->xp.clear();
    this->in_out_ratio.clear();
  }

  std::vector<int> xp;
  std::vector<int> yp;
  std::vector<double> in_out_ratio;
  double ratio_total;

private:
  pixel_geometry_();

  outside_of_image_ type_;
};
/* 指定位置のピクセルの値 */
template <class T>
T get_pixel_value_(pixel_geometry_ &pixg, const T *image_top, const int hh,
                   const int ww, const int ch, int xx, int yy, const int zz) {
  pixg.re_position(ww, hh, xx, yy);
  if ((xx < 0) || (yy < 0)) {
    return 0; /* 黒 */
  }
  return *(image_top + (ww * ch * yy + ch * xx + zz));
}
}
//------------------------------------------------------------
// #include <algorithm> // std::sort()
#include <stdexcept> /* std::domain_error(-) */
#include <limits>    /* std::numeric_limits */
#include <map>       /* std::multimap */
namespace {
/*
        Median Filter...
        +---+---+---+				+---+---+---+
        | 5 | 6 | 7 |				| 5 | 6 | 7 |
        +---+---+---+				+---+---+---+
        | 4 | 9 | 6 | --> 3,4,4,5,5,6,6,7,9 -->	| 4 | 5 | 6 |
        +---+---+---+		  ^		+---+---+---+
        | 3 | 4 | 5 |		  |		| 3 | 4 | 5 |
        +---+---+---+		median		+---+---+---+
        --> ...Smoothing...
*/
template <class T>
double median_filter_smooth_(pixel_geometry_ &pixg, const T *image_top,
                             const int hh, const int ww, const int ch,
                             const int xx, const int yy, const int zz) {
  /* ピクセル値と影響値のペアにして、ソートしたリストを作成 */
  std::multimap<double, double> pixels;
  for (unsigned int ii = 0; ii < pixg.xp.size(); ++ii) {
    const double value = static_cast<double>(
        get_pixel_value_(pixg, image_top, hh, ww, ch, xx + pixg.xp.at(ii),
                         yy + pixg.yp.at(ii), zz));
    pixels.insert(std::pair<double, double>(value, pixg.in_out_ratio.at(ii)));
  }

  /* 取り出しやすいようにvectorに移し変える */
  /*
          +---+---+---+---+---+
          | 0 | 1 | 2 | 3 | 4 |	<-- pixel value
          +---+---+---+---+---+
          ^-^---^---^---^---^-^
           0  1   2   3   4  5	<-- len_each
          +>0-->1-->2-->3-->4>5	<-- len_accum
  */
  /* 中央(median)位置の前後ピクセル位置を得
  中央値を求める */
  double len_median = pixg.ratio_total / 2.0; /* 並べたときの中間位置 */
  double accum      = 0.0;
  double before_value = 0.0;
  double before_ratio = 0.0;
  double before_accum = 0.0;
  for (std::multimap<double, double>::iterator it = pixels.begin();
       it != pixels.end();
       ++it, before_value = (*it).first, before_ratio = (*it).second,
                                               before_accum = accum) {
    /* ピクセル間の距離 */
    const double pixw = (*it).second / 2.0 + before_ratio / 2.0;

    /* 開始端からの(ピクセル単位の)距離 */
    accum += pixw; /* Pixelの中央の位置 */

    if (len_median <= accum) {
      /* 開始端から始めのピクセルの半分位置までの間 */
      if (it == pixels.begin()) {
        return (*it).first;
      }

      if (before_value < (*it).first) {
        return ((*it).first - before_value) * (len_median - before_accum) /
                   pixw +
               before_value;
      }
      return (before_value - (*it).first) * (accum - len_median) / pixw +
             (*it).first;
    }
  }
  return before_value;
}
}
//------------------------------------------------------------
namespace {
double refchk_(const double src, const double tgt, const double refv) {
  return ((src < tgt) ? (tgt - src) * refv + src
                      : (src - tgt) * (1.0 - refv) + tgt) +
         0.999999;
}
template <class IT, class RT>
void convert_each_to_all_channels_template_(
    const IT *in, IT *out, const int hh, const int ww, const int ch

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode  // R,G,B,A,luminance

    ,
    const int zz, const double radius, const outside_of_image_ type) {
  pixel_geometry_ pixg(radius, type);
  const IT *in_pix = in;
  IT *out_pix      = out;
  const int r_max  = std::numeric_limits<RT>::max();
  for (int yy = 0; yy < hh; ++yy) {
    for (int xx = 0; xx < ww; ++xx, in_pix += ch, out_pix += ch) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, ch, r_max, ref_mode);
        ref += ch;
      }
      const double v1 = median_filter_smooth_(pixg, in, hh, ww, ch, xx, yy, zz);
      const IT v2     = static_cast<IT>(refchk_(in_pix[zz], v1, refv));
      for (int zz = 0; zz < ch; ++zz) {
        out_pix[zz] = v2;
      }
    }
  }
  pixg.clear();
}
template <class IT, class RT>
void convert_each_to_each_channel_template_(
    const IT *in, IT *out, const int hh, const int ww, const int ch

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode  // R,G,B,A,luminance

    ,
    const double radius, const outside_of_image_ type) {
  pixel_geometry_ pixg(radius, type);
  const IT *in_pix = in;
  IT *out_pix      = out;
  const int r_max  = std::numeric_limits<RT>::max();
  for (int yy = 0; yy < hh; ++yy) {
    for (int xx = 0; xx < ww; ++xx, in_pix += ch, out_pix += ch) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, ch, r_max, ref_mode);
        ref += ch;
      }
      for (int zz = 0; zz < ch; ++zz) {
        const double v1 =
            median_filter_smooth_(pixg, in, hh, ww, ch, xx, yy, zz);
        out_pix[zz] = static_cast<IT>(refchk_(in_pix[zz], v1, refv));
      }
    }
  }
  pixg.clear();
}
}
//------------------------------------------------------------
#include "igs_median_filter_smooth.h"
#include "igs_ifx_common.h" /* igs::image::rgba */
void igs::median_filter_smooth::convert(
    const unsigned char *in_image, unsigned char *out_image

    ,
    const int height, const int width, const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const int zz  // 0(R),1(G),2(B),3(A),4(EachCh)
    ,
    const double radius  // 0...
    ,
    const int out_side_type  // 0(Spread),1(Flip),2(bk),3(Repeat)
    ) {
  /*--- 指定(zz)から、実際に処理すべき色チャンネル(z2)を得る ---*/
  int z2 = zz;
  if (igs::image::rgba::siz == channels) {
    switch (zz) {
    case 0:
      z2 = igs::image::rgba::red;
      break;
    case 1:
      z2 = igs::image::rgba::gre;
      break;
    case 2:
      z2 = igs::image::rgba::blu;
      break;
    case 3:
      z2 = igs::image::rgba::alp;
      break;
    default:
      z2 = igs::image::rgba::siz;
      break;
    }
  } else if (igs::image::rgb::siz == channels) {
    switch (zz) {
    case 0:
      z2 = igs::image::rgb::red;
      break;
    case 1:
      z2 = igs::image::rgb::gre;
      break;
    case 2:
      z2 = igs::image::rgb::blu;
      break;
    default:
      z2 = igs::image::rgb::siz;
      break;
    }
  } else if (1 == channels) {
    ;
  } else {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  /*--- 指定(out_side_type)から、実際の処理方法(type)を設定 ---*/
  outside_of_image_ type = is_spread_edge_;
  switch (out_side_type) {
  case 0:
    type = is_spread_edge_;
    break;
  case 1:
    type = is_flip_repeat_;
    break;
  case 2:
    type = is_black_;
    break;
  case 3:
    type = is_repeat_;
    break;
  }

  /* 処理 */
  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    if ((0 <= z2) && (z2 < channels)) {
      convert_each_to_all_channels_template_(in_image, out_image, height, width,
                                             channels, ref, ref_mode, z2,
                                             radius, type);
    } else {
      convert_each_to_each_channel_template_(in_image, out_image, height, width,
                                             channels, ref, ref_mode, radius,
                                             type);
    }
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    if ((0 <= z2) && (z2 < channels)) {
      convert_each_to_all_channels_template_(
          reinterpret_cast<const unsigned short *>(in_image),
          reinterpret_cast<unsigned short *>(out_image), height, width,
          channels, ref, ref_mode, z2, radius, type);
    } else {
      convert_each_to_each_channel_template_(
          reinterpret_cast<const unsigned short *>(in_image),
          reinterpret_cast<unsigned short *>(out_image), height, width,
          channels, ref, ref_mode, radius, type);
    }
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    if ((0 <= z2) && (z2 < channels)) {
      convert_each_to_all_channels_template_(
          reinterpret_cast<const unsigned short *>(in_image),
          reinterpret_cast<unsigned short *>(out_image), height, width,
          channels, reinterpret_cast<const unsigned short *>(ref), ref_mode, z2,
          radius, type);
    } else {
      convert_each_to_each_channel_template_(
          reinterpret_cast<const unsigned short *>(in_image),
          reinterpret_cast<unsigned short *>(out_image), height, width,
          channels, reinterpret_cast<const unsigned short *>(ref), ref_mode,
          radius, type);
    }
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    if ((0 <= z2) && (z2 < channels)) {
      convert_each_to_all_channels_template_(
          in_image, out_image, height, width, channels,
          reinterpret_cast<const unsigned short *>(ref), ref_mode, z2, radius,
          type);
    } else {
      convert_each_to_each_channel_template_(
          in_image, out_image, height, width, channels,
          reinterpret_cast<const unsigned short *>(ref), ref_mode, radius,
          type);
    }
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
