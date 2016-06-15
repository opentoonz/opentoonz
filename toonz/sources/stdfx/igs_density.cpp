#include "igs_density.h"

namespace {
double accum_by_trans_(
    const double src_value /* 元値(R,G,B) */
    ,
    const double transparent /* src_valueの透明度(0...1) */
    ,
    const int integer_part /* 濃度値の整数部分(0...) */
    ,
    const double fractional_part /* 濃度値の少数部分(0...1) */
    ) {
  double accumulation = src_value;
  if (1 <= integer_part) {
    for (int ii = 1; ii < integer_part; ++ii) {
      accumulation = accumulation * transparent + src_value;
    }
    if (0.0 < fractional_part) {
      accumulation +=
          ((accumulation * transparent + src_value) - accumulation) *
          fractional_part;
    }
  } else { /* 整数部分がゼロ以下のとき */
    if (0.0 < fractional_part) {
      accumulation *= fractional_part;
    } else { /* 少数部分もゼロ以下のとき */
      accumulation = 0.0;
    }
  }
  return (1.0 < accumulation) ? 1.0
                              : ((accumulation < 0.0) ? 0.0 : accumulation);
}
}
#include <limits>           /* std::numeric_limits */
#include "igs_ifx_common.h" /* igs::image::rgba */
namespace {
template <class IT, class RT>
void change_template_(
    IT *image_array, const int height, const int width, const int channels

    ,
    const RT *ref /* 求める画像と同じ高、幅、ch数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double density) {
  const int integer_part       = (int)density;
  const double fractional_part = density - (int)density;
  const double maxi            = std::numeric_limits<IT>::max();

  using namespace igs::image::rgba;
  const int pixsize = height * width;
  const int r_max   = std::numeric_limits<RT>::max();
  for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
    const double rr1 = (double)(image_array[red]) / maxi;
    const double gg1 = (double)(image_array[gre]) / maxi;
    const double bb1 = (double)(image_array[blu]) / maxi;
    const double aa1 = (double)(image_array[alp]) / maxi;

    double rr2 = accum_by_trans_(rr1, 1.0 - aa1, integer_part, fractional_part);
    double gg2 = accum_by_trans_(gg1, 1.0 - aa1, integer_part, fractional_part);
    double bb2 = accum_by_trans_(bb1, 1.0 - aa1, integer_part, fractional_part);
    double aa2 = accum_by_trans_(aa1, 1.0 - aa1, integer_part, fractional_part);

    /* 参照画像あればピクセル単位の画像変化 */
    if (ref != 0) {
      const double refv = igs::color::ref_value(ref, channels, r_max, ref_mode);

      ref += channels; /* continue;の前に行うこと */

      rr2 = (rr2 - rr1) * refv + rr1;
      gg2 = (gg2 - gg1) * refv + gg1;
      bb2 = (bb2 - bb1) * refv + bb1;
      aa2 = (aa2 - aa1) * refv + aa1;
    }

    image_array[red] = static_cast<IT>(rr2 * (maxi + 0.999999));
    image_array[gre] = static_cast<IT>(gg2 * (maxi + 0.999999));
    image_array[blu] = static_cast<IT>(bb2 * (maxi + 0.999999));
    image_array[alp] = static_cast<IT>(aa2 * (maxi + 0.999999));
  }
}
}
#include <stdexcept> /* std::domain_error(-) */
void igs::density::change(
    unsigned char *image_array /* RGBAでなければならない */
    ,
    const int height, const int width,
    const int channels /* 4(=RGBAでなければならない) */
    ,
    const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double density) {
  if (igs::image::rgba::siz != channels) {
    throw std::domain_error("Bad channels,Not rgba");
  }

  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    change_template_(image_array, height, width, channels, ref, ref_mode,
                     density);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels, ref, ref_mode, density);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     density);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(image_array, height, width, channels,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     density);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
