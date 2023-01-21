#include "igs_color_rgb_hls.h"
namespace {
void pixel_rgba_(const double red_in, const double gre_in, const double blu_in,
                 double &red_out, double &gre_out, double &blu_out,
                 const double hue_pivot,  // 0.0  ...0...360...
                 const double hue_scale,  // 1.0  ...1...
                 const double hue_shift,  // 0.0  ...0...360...
                 const double lig_pivot,  // 0.0  ...0...1...
                 const double lig_scale,  // 1.0  ...1...
                 const double lig_shift,  // 0.0  ...0...1...
                 const double sat_pivot,  // 0.0  ...0...1...
                 const double sat_scale,  // 1.0  ...1...
                 const double sat_shift,  // 0.0  ...0...1...
                 const bool cylindrical = true) {
  double hue, lig, sat;
  igs::color::rgb_to_hls(red_in, gre_in, blu_in, hue, lig, sat, cylindrical);
  if ((1.0 != hue_scale) || (0.0 != hue_shift)) {
    hue -= hue_pivot;
    while (hue < -180.0) {
      hue += 360.0;
    }
    while (180.0 <= hue) {
      hue -= 360.0;
    }
    hue *= hue_scale;
    hue += hue_pivot;
    hue += hue_shift;
    while (hue < 0.0) {
      hue += 360.0;
    }
    while (360.0 <= hue) {
      hue -= 360.0;
    }
  }
  if ((1.0 != lig_scale) || (0.0 != lig_shift)) {
    lig -= lig_pivot;
    lig *= lig_scale;
    lig += lig_pivot;
    lig += lig_shift;
    // lig = (lig < 0.0) ? 0.0 : ((1.0 < lig) ? 1.0 : lig);
  }
  if ((1.0 != sat_scale) || (0.0 != sat_shift)) {
    sat -= sat_pivot;
    sat *= sat_scale;
    sat += sat_pivot;
    sat += sat_shift;
    sat = (sat < 0.0) ? 0.0 : sat;
    // sat = (sat < 0.0) ? 0.0 : ((1.0 < sat) ? 1.0 : sat);
  }
  igs::color::hls_to_rgb(hue, lig, sat, red_out, gre_out, blu_out, cylindrical);
}
}  // namespace
//------------------------------------------------------------
#include <limits>           /* std::numeric_limits */
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_hls_adjust.h"
namespace {
/* raster画像にノイズをのせる*/
void change_(float *image_array, const int height, const int width,
             const int channels,
             const float *ref, /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
             const double hue_pivot, const double hue_scale,
             const double hue_shift, const double lig_pivot,
             const double lig_scale, const double lig_shift,
             const double sat_pivot, const double sat_scale,
             const double sat_shift, const bool add_blend_sw,
             const bool cylindrical = true) {
  // const int t_max      = std::numeric_limits<IT>::max();
  // const double div_val = static_cast<double>(t_max);
  // const double mul_val = static_cast<double>(t_max) + 0.999999;
  const int pixsize = height * width;
  // const int r_max      = std::numeric_limits<RT>::max();
  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      /* 変化量初期値 */
      float refv = 1.f;

      /* 参照画像あればピクセル単位の画像変化量を得る */
      if (ref != nullptr) {
        refv *= (*ref);
        ref++; /* continue;の前に行うこと */
      }

      /* 加算合成で、Alpha値ゼロならRGB値を計算する必要はない */
      if (add_blend_sw && (0.f == image_array[alp])) {
        continue;
      }
      /* 加算合成でなくAlpha合成の時は、
      Alpha値がゼロでもRGB値は存在する(してもよい) */

      /* pivot,scale,shiftによってRGB値を変化する */
      double rr, gg, bb;
      pixel_rgba_(image_array[red], image_array[gre], image_array[blu], rr, gg,
                  bb, hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale,
                  lig_shift, sat_pivot, sat_scale, sat_shift, cylindrical);

      /* 加算合成で、その値がMaxでなければ変化量に乗算 */
      if (add_blend_sw && (image_array[alp] < 1.f)) {
        refv *= image_array[alp];
      }

      /* ピクセル単位の変化量があれば、RGBを調整する */
      if ((ref != nullptr) || (add_blend_sw && (image_array[alp] < 1.f))) {
        rr = image_array[red] + (rr - image_array[red]) * refv;
        gg = image_array[gre] + (gg - image_array[gre]) * refv;
        bb = image_array[blu] + (bb - image_array[blu]) * refv;
      }

      /* 結果をRGBに戻す */
      image_array[red] = rr;
      image_array[gre] = gg;
      image_array[blu] = bb;
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      float refv = 1.f;
      if (ref != nullptr) {
        refv *= (*ref);
        ref++;
      }

      // const IT *const ia = image_array;
      // const double rr1   = static_cast<double>(ia[red]) / div_val;
      // const double gg1   = static_cast<double>(ia[gre]) / div_val;
      // const double bb1   = static_cast<double>(ia[blu]) / div_val;
      double rr, gg, bb;
      pixel_rgba_(image_array[red], image_array[gre], image_array[blu], rr, gg,
                  bb, hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale,
                  lig_shift, sat_pivot, sat_scale, sat_shift, cylindrical);

      if (ref != nullptr) {
        rr = image_array[red] + (rr - image_array[red]) * refv;
        gg = image_array[gre] + (gg - image_array[gre]) * refv;
        bb = image_array[blu] + (bb - image_array[blu]) * refv;
      }

      image_array[red] = rr;
      image_array[gre] = gg;
      image_array[blu] = bb;
    }
  } else if (1 == channels) { /* grayscale */
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      double refv = 1.f;
      if (ref != nullptr) {
        refv *= (*ref);
        ref++;
      }

      double li =
          (*image_array - lig_pivot) * lig_scale + lig_pivot + lig_shift;
      // li        = (li < 0.0) ? 0.0 : ((1.0 < li) ? 1.0 : li);

      if (ref != nullptr) {
        li = *image_array + (li - *image_array) * refv;
      }

      *image_array = li;
    }
  }
}
}  // namespace

#include <stdexcept> /* std::domain_error  */
void igs::hls_adjust::change(
    float *image_array, const int height, const int width, const int channels,
    const float *ref, /* 求める画像と同じ高、幅、channels数 */
    const double hue_pivot, const double hue_scale, const double hue_shift,
    const double lig_pivot, const double lig_scale, const double lig_shift,
    const double sat_pivot, const double sat_scale, const double sat_shift,
    const bool add_blend_sw, const bool cylindrical) {
  if ((1.0 == hue_scale) && (0.0 == hue_shift) && (1.0 == lig_scale) &&
      (0.0 == lig_shift) && (1.0 == sat_scale) && (0.0 == sat_shift)) {
    return;
  }

  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
  ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }
  change_(image_array, height, width, channels, ref, hue_pivot, hue_scale,
          hue_shift, lig_pivot, lig_scale, lig_shift, sat_pivot, sat_scale,
          sat_shift, add_blend_sw, cylindrical);

  /* rgb(a)画像にhls(a)でドットノイズを加える */
  /*
  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    change_template_(image_array, height, width, channels, ref, ref_mode,
                     hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale,
                     lig_shift, sat_pivot, sat_scale, sat_shift, add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels, ref, ref_mode, hue_pivot, hue_scale,
                     hue_shift, lig_pivot, lig_scale, lig_shift, sat_pivot,
                     sat_scale, sat_shift, add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale,
                     lig_shift, sat_pivot, sat_scale, sat_shift, add_blend_sw);
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(image_array, height, width, channels,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     hue_pivot, hue_scale, hue_shift, lig_pivot, lig_scale,
                     lig_shift, sat_pivot, sat_scale, sat_shift, add_blend_sw);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
  */
}
