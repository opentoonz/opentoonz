#include <stdexcept> /* std::domain_error  */
#include <limits>    /* std::numeric_limits */
#include "igs_color_rgb_hsv.h"
namespace {
void pixel_rgba_(const double red_in, const double gre_in, const double blu_in,
                 const double alp_in, const double hue_noise  // -0.5...0.5
                 ,
                 const double sat_noise, const double val_noise  // -0.5...0.5
                 ,
                 const double alp_noise, double &red_out, double &gre_out,
                 double &blu_out, double &alp_out) {
  double hue, sat, val, alp;
  igs::color::rgb_to_hsv(red_in, gre_in, blu_in, hue, sat, val);
  alp = alp_in;

  if (0.0 != hue_noise) {
    hue += 360.0 * hue_noise;
    while (hue < 0.0) {
      hue += 360.0;
    }
    while (360.0 <= hue) {
      hue -= 360.0;
    }
  }
  if (0.0 != sat_noise) {
    sat += sat_noise;
    if (sat < 0.0) {
      sat = 0.0;
    }
    // else if (1.0 < sat) {
    //   sat = 1.0;
    // }
  }
  if (0.0 != val_noise) {
    val += val_noise;
    // if (val < 0.0) {
    //   val = 0.0;
    // } else if (1.0 < val) {
    //   val = 1.0;
    // }
  }
  if (0.0 != alp_noise) {
    alp += alp_noise;
    alp = (alp < 0.0) ? 0.0 : ((1.0 < alp) ? 1.0 : alp);
  }

  igs::color::hsv_to_rgb(hue, sat, val, red_out, gre_out, blu_out);
  alp_out = alp;
}
}  // namespace
//------------------------------------------------------------
namespace {
class noise_ref_ {
public:
  noise_ref_(const float *array, const int height, const int width,
             const int xoffset, const int yoffset, const int zz);
  double noise(int xx  // 0...width-1...
               ,
               int yy  // 0...height-1...
  ) const;             // return range is 0...1

private:
  const float *array_;
  const int height_;
  const int width_;
  const int xoffset_;
  const int yoffset_;
  const int zz_;

  /* copy constructorを無効化 */
  noise_ref_(const noise_ref_ &);

  /* 代入演算子を無効化 */
  noise_ref_ &operator=(const noise_ref_ &);
};
noise_ref_::noise_ref_(const float *array, const int height, const int width,
                       const int xoffset, const int yoffset, const int zz)
    : array_(array)
    , height_(height)
    , width_(width)
    , xoffset_(xoffset)
    , yoffset_(yoffset)
    , zz_(zz) {
  if (0 == array) {
    throw std::domain_error("noise_ref_  no data");
  }
  if ((zz < 0) || (4 <= zz)) {
    throw std::domain_error("noise_ref_  bad zz");
  }
}
double noise_ref_::noise(int xx, int yy) const {
  xx -= this->xoffset_;
  yy -= this->yoffset_;
  while (xx < 0) {
    xx += this->width_;
  }
  while (this->width_ <= xx) {
    xx -= this->width_;
  }
  while (yy < 0) {
    yy += this->height_;
  }
  while (this->height_ <= yy) {
    yy -= this->height_;
  }

  return (*(this->array_ + 4 * this->width_ * yy + 4 * xx + this->zz_));
}
}  // namespace
//------------------------------------------------------------
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_hsv_add.h"
namespace {
/* raster画像にノイズをのせる */
void change_(float *image_array, const int height, const int width,
             const int channels, const noise_ref_ &noi,
             const float *ref, /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
             const double offset, const double hue_scale,
             const double sat_scale, const double val_scale,
             const double alp_scale, const bool add_blend_sw) {
  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, image_array += channels) {
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

        /* HSVAそれぞれに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* マスクSWがON、なら変化をMask */
        if (add_blend_sw && (image_array[alp] < 1.f)) {
          refv *= image_array[alp];
        }

        /* RGBAにHSVAノイズを加える */
        double rr, gg, bb, aa;
        pixel_rgba_(image_array[red], image_array[gre], image_array[blu],
                    image_array[alp], refv * hue_scale, refv * sat_scale,
                    refv * val_scale, refv * alp_scale, rr, gg, bb, aa);

        /* 変化後の値を戻す */
        image_array[red] = (float)rr;
        image_array[gre] = (float)gg;
        image_array[blu] = (float)bb;
        image_array[alp] = (float)aa;
      }
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, image_array += channels) {
        /* 変化量初期値 */
        float refv = 1.f;

        /* 参照画像あればピクセル単位の画像変化量を得る */
        if (ref != nullptr) {
          refv *= (*ref);
          ref++; /* continue;の前に行うこと */
        }

        /* HSVそれぞれに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* RGBにHSVノイズを加える */
        double rr, gg, bb, aa;
        pixel_rgba_(image_array[red], image_array[gre], image_array[blu], 1.0,
                    refv * hue_scale, refv * sat_scale, refv * val_scale, 0.0,
                    rr, gg, bb, aa);

        /* 変化後の値を戻す */
        image_array[red] = (float)rr;
        image_array[gre] = (float)gg;
        image_array[blu] = (float)bb;
      }
    }
  } else if (1 == channels) { /* grayscale */
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, ++image_array) {
        /* 変化量初期値 */
        float refv = 1.f;

        /* 参照画像あればピクセル単位の画像変化量を得る */
        if (ref != nullptr) {
          refv *= (*ref);
          ref++; /* continue;の前に行うこと */
        }

        /* Lに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* 変化なしなら次へ */
        if (0.0 == refv * val_scale) {
          continue;
        }

        /* GrayscaleにLノイズを加える */
        float val = image_array[0] + refv * val_scale;
        // val = (val < 0.0) ? 0.0 : ((1.0 < val) ? 1.0 : val);

        /* 変化後の値を戻す */
        image_array[0] = val;
      }
    }
  }
}
}  // namespace

void igs::hsv_add::change(
    float *image_array, const int height, const int width, const int channels,
    const float *noi_image_array,
    const float *ref, /* 求める画像と同じ高、幅、channels数 */
    const int xoffset, const int yoffset, const int from_rgba,
    const double offset, const double hue_scale, const double sat_scale,
    const double val_scale, const double alp_scale, const bool add_blend_sw) {
  if ((0.0 == hue_scale) && (0.0 == sat_scale) && (0.0 == val_scale) &&
      (0.0 == alp_scale)) {
    return;
  }

  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
  ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  /* ノイズ参照画像を作成する */
  noise_ref_ noi(noi_image_array, height, width, xoffset, yoffset, from_rgba);

  change_(image_array, height, width, channels, noi, ref, offset, hue_scale,
          sat_scale, val_scale, alp_scale, add_blend_sw);

  /* rgb(a)画像にhsv(a)でドットノイズを加える */
  /*
  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    change_template_(image_array, height, width, channels, noi, ref, ref_mode,
                     offset, hue_scale, sat_scale, val_scale, alp_scale,
                     add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels, noi, ref, ref_mode, offset, hue_scale,
                     sat_scale, val_scale, alp_scale, add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(
        reinterpret_cast<unsigned short *>(image_array), height, width,
        channels, noi, reinterpret_cast<const unsigned short *>(ref), ref_mode,
        offset, hue_scale, sat_scale, val_scale, alp_scale, add_blend_sw);
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(image_array, height, width, channels, noi,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     offset, hue_scale, sat_scale, val_scale, alp_scale,
                     add_blend_sw);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
  */
}
