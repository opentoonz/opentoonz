#include <cmath>
#include <vector>
#include <algorithm>  // std::sort()
#include <stdexcept>  /* std::domain_error(-) */
#include <limits>     /* std::numeric_limits */
#include "igs_ifx_common.h"

namespace igs {
namespace median_filter {
enum out_of_image {
  is_spread_edge,
  is_flip_repeat,
  is_black,  /* 必要か?????? */
  is_repeat, /* 必要か?????? */
};
class pixrender {
public:
  pixrender(double radius, igs::median_filter::out_of_image type);
  std::vector<int> xp;
  std::vector<int> yp;
  std::vector<int> around;
  void position(const int ww, const int hh, int &xx, int &yy);
  void clear(void);

private:
  pixrender();
  igs::median_filter::out_of_image type_;
};
}
}
//------------------------------------------------------------
igs::median_filter::pixrender::pixrender(
    const double radius, const igs::median_filter::out_of_image type)
    : type_(type) {
  const int radius_int = (int)ceil(radius);
  int size             = 0;

  for (int yy = -radius_int; yy <= radius_int; ++yy) {
    for (int xx = -radius_int; xx <= radius_int; ++xx) {
      /***
const double xr = xx / radius;
const double yr = yy / radius;
if ( sqrt(xr * xr + yr * yr) <= (1.0 + 1e-6) ){
++size;
}
***/
      const double xr = static_cast<double>(xx);
      const double yr = static_cast<double>(yy);
      if ((xr * xr + yr * yr) <= (radius * radius + 1e-6)) {
        ++size;
      }
    }
  }

  this->xp.resize(size);
  this->yp.resize(size);
  this->around.resize(size);

  int ii = 0;
  for (int yy = -radius_int; yy <= radius_int; ++yy) {
    for (int xx = -radius_int; xx <= radius_int; ++xx) {
      /***
const double xr = xx / radius;
const double yr = yy / radius;
if ( sqrt(xr * xr + yr * yr) <= (1.0 + 1e-6) ){
this->xp.at(ii) = xx;
this->yp.at(ii) = yy;
++ii;
}
***/
      const double xr = static_cast<double>(xx);
      const double yr = static_cast<double>(yy);
      if ((xr * xr + yr * yr) <= (radius * radius + 1e-6)) {
        this->xp.at(ii) = xx;
        this->yp.at(ii) = yy;
        ++ii;
      }
    }
  }
}
void igs::median_filter::pixrender::clear(void) {
  this->around.clear();
  this->yp.clear();
  this->xp.clear();
}

void igs::median_filter::pixrender::position(const int ww, const int hh,
                                             int &xx, int &yy) {
  switch (this->type_) {
  case igs::median_filter::is_spread_edge:
    xx = (xx < 0) ? 0 : ((ww <= xx) ? ww - 1 : xx);
    yy = (yy < 0) ? 0 : ((hh <= yy) ? hh - 1 : yy);
    break;
  case igs::median_filter::is_flip_repeat:
    if (xx < 0) {
      while (xx < 0) {
        xx += ww;
      }
      xx = ww - 1 - xx;
    }
    if (ww <= xx) {
      while (ww <= xx) {
        xx -= ww;
      }
      xx = ww - 1 - xx;
    }
    if (yy < 0) {
      while (yy < 0) {
        yy += hh;
      }
      yy = hh - 1 - yy;
    }
    if (hh <= yy) {
      while (hh <= yy) {
        yy -= hh;
      }
      yy = hh - 1 - yy;
    }
    break;
  case igs::median_filter::is_black: /* 必要か?????? */
    if (xx < 0 || ww <= xx) {
      xx = -1;
    }
    if (yy < 0 || hh <= yy) {
      yy = -1;
    }
    break;
  case igs::median_filter::is_repeat: /* 必要か?????? */
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
//------------------------------------------------------------
namespace {
template <class T>
T getter_(igs::median_filter::pixrender &pixr, const T *image, const int hh,
          const int ww, const int ch, int xx, int yy, const int zz) {
  pixr.position(ww, hh, xx, yy);
  if ((xx < 0) || (yy < 0)) {
    return 0;
  }
  return *(image + (ww * ch * yy + ch * xx + zz));
}
template <class T>
T median_filter_(igs::median_filter::pixrender &pixr, const T *image,
                 const int hh, const int ww, const int ch, const int xx,
                 const int yy, const int zz) {
  for (unsigned int ii = 0; ii < pixr.around.size(); ++ii) {
    pixr.around.at(ii) = static_cast<int>(getter_(
        pixr, image, hh, ww, ch, xx + pixr.xp.at(ii), yy + pixr.yp.at(ii), zz));
  }

  std::sort(pixr.around.begin(), pixr.around.end());

  /*	中央値(median)計算は、厳密な定義(wikipediaより)によると
                  奇数(odd)のときは中央値
                  偶数(even)のときは中央の二つの値の平均
          となるが、
          元のPixel値を変えないポリシーにより、
          偶数の場合も奇数の計算をそのまま流用する。
          よって偶数の場合は中央の二つの値の大きいほうとなる。
          2009-03-24
  */
  return static_cast<T>(pixr.around.at(pixr.around.size() / 2));
}
}
//------------------------------------------------------------
namespace {
double refchk_(const int src, const int tgt, const double refv) {
  return (src < tgt) ? (tgt - src + 0.999999) * refv + src
                     : (src - tgt + 0.999999) * (1.0 - refv) + tgt;
}
template <class IT, class RT>
void convert_each_to_all_channels_template_(
    const IT *in, IT *out, const int hh, const int ww, const int ch

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode  // R,G,B,A,luminance

    ,
    const int zz, const double radius,
    const igs::median_filter::out_of_image type) {
  igs::median_filter::pixrender pixr(radius, type);
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
      const IT v1 = median_filter_(pixr, in, hh, ww, ch, xx, yy, zz);
      const IT v2 = static_cast<IT>(refchk_(in_pix[zz], v1, refv));
      for (int zz = 0; zz < ch; ++zz) {
        out_pix[zz] = v2;
      }
    }
  }
  pixr.clear();
}
template <class IT, class RT>
void convert_each_to_each_channel_template_(
    const IT *in, IT *out, const int hh, const int ww, const int ch

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode  // R,G,B,A,luminance

    ,
    const double radius, const igs::median_filter::out_of_image type) {
  igs::median_filter::pixrender pixr(radius, type);
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
        const IT v1 = median_filter_(pixr, in, hh, ww, ch, xx, yy, zz);
        out_pix[zz] = static_cast<IT>(refchk_(in_pix[zz], v1, refv));
      }
    }
  }
  pixr.clear();
}
}
//------------------------------------------------------------
#include "igs_median_filter.h"
#include "igs_ifx_common.h" /* igs::image::rgba */
void igs::median_filter::convert(
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

  igs::median_filter::out_of_image type = igs::median_filter::is_spread_edge;
  switch (out_side_type) {
  case 0:
    type = igs::median_filter::is_spread_edge;
    break;
  case 1:
    type = igs::median_filter::is_flip_repeat;
    break;
  case 2:
    type = igs::median_filter::is_black;
    break; /* 必要か?????? */
  case 3:
    type = igs::median_filter::is_repeat;
    break; /* 必要か?????? */
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
