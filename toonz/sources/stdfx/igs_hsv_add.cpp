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
    } else if (1.0 < sat) {
      sat = 1.0;
    }
  }
  if (0.0 != val_noise) {
    val += val_noise;
    if (val < 0.0) {
      val = 0.0;
    } else if (1.0 < val) {
      val = 1.0;
    }
  }
  if (0.0 != alp_noise) {
    alp += alp_noise;
    alp = (alp < 0.0) ? 0.0 : ((1.0 < alp) ? 1.0 : alp);
  }

  igs::color::hsv_to_rgb(hue, sat, val, red_out, gre_out, blu_out);
  alp_out = alp;
}
}
//------------------------------------------------------------
namespace {
class noise_ref_ {
public:
  noise_ref_(const unsigned char *array, const int height, const int width,
             const int channels, const int bits, const int xoffset,
             const int yoffset, const int zz);
  double noise(int xx  // 0...width-1...
               ,
               int yy    // 0...height-1...
               ) const;  // return range is 0...1

private:
  const unsigned char *array_;
  const int height_;
  const int width_;
  const int channels_;
  const int bits_;
  const double bmax_;
  const int xoffset_;
  const int yoffset_;
  const int zz_;

  /* copy constructorを無効化 */
  noise_ref_(const noise_ref_ &);

  /* 代入演算子を無効化 */
  noise_ref_ &operator=(const noise_ref_ &);
};
noise_ref_::noise_ref_(const unsigned char *array, const int height,
                       const int width, const int channels, const int bits,
                       const int xoffset, const int yoffset, const int zz)
    : array_(array)
    , height_(height)
    , width_(width)
    , channels_(channels)
    , bits_(bits)
    , bmax_(static_cast<double>((1 << bits) - 1))
    , xoffset_(xoffset)
    , yoffset_(yoffset)
    , zz_(zz) {
  if (0 == array) {
    throw std::domain_error("noise_ref_  no data");
  }
  if ((zz < 0) || (channels <= zz)) {
    throw std::domain_error("noise_ref_  bad zz");
  }
  if ((std::numeric_limits<unsigned char>::digits != this->bits_) &&
      (std::numeric_limits<unsigned short>::digits != this->bits_)) {
    throw std::domain_error("noise_ref_  bad bits");
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

  if (std::numeric_limits<unsigned char>::digits == this->bits_) {
    return (*(this->array_ + this->channels_ * this->width_ * yy +
              this->channels_ * xx + this->zz_)) /
           this->bmax_;
  }
  return (*(reinterpret_cast<const unsigned short *>(this->array_) +
            this->channels_ * this->width_ * yy + this->channels_ * xx +
            this->zz_)) /
         this->bmax_;
}
}
//------------------------------------------------------------
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_hsv_add.h"
namespace {
/* raster画像にノイズをのせるtemplate */
template <class IT, class RT>
void change_template_(
    IT *image_array, const int height, const int width, const int channels

    ,
    const noise_ref_ &noi

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double offset, const double hue_scale, const double sat_scale,
    const double val_scale, const double alp_scale

    ,
    const bool add_blend_sw) {
  const int t_max      = std::numeric_limits<IT>::max();
  const double div_val = static_cast<double>(t_max);
  const double mul_val = static_cast<double>(t_max) + 0.999999;
  const int r_max      = std::numeric_limits<RT>::max();
  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, image_array += channels) {
        /* 変化量初期値 */
        double refv = 1.0;

        /* 参照画像あればピクセル単位の画像変化量を得る */
        if (ref != 0) {
          refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
          ref += channels; /* continue;の前に行うこと */
        }

        /* 加算合成で、Alpha値ゼロならRGB値を計算する必要はない */
        if (add_blend_sw && (0 == image_array[alp])) {
          continue;
        }
        /* 加算合成でなくAlpha合成の時は、
Alpha値がゼロでもRGB値は存在する(してもよい) */

        /* HSVAそれぞれに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* マスクSWがON、なら変化をMask */
        if (add_blend_sw && (image_array[alp] < t_max)) {
          refv *= static_cast<double>(image_array[alp]) / div_val;
        }

        /* RGBAにHSVAノイズを加える */
        double rr, gg, bb, aa;
        pixel_rgba_(static_cast<double>(image_array[red]) / div_val,
                    static_cast<double>(image_array[gre]) / div_val,
                    static_cast<double>(image_array[blu]) / div_val,
                    static_cast<double>(image_array[alp]) / div_val,
                    refv * hue_scale, refv * sat_scale, refv * val_scale,
                    refv * alp_scale, rr, gg, bb, aa);

        /* 変化後の値を戻す */
        image_array[red] = static_cast<IT>(rr * mul_val);
        image_array[gre] = static_cast<IT>(gg * mul_val);
        image_array[blu] = static_cast<IT>(bb * mul_val);
        image_array[alp] = static_cast<IT>(aa * mul_val);
      }
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, image_array += channels) {
        /* 変化量初期値 */
        double refv = 1.0;

        /* 参照画像あればピクセル単位の画像変化量を得る */
        if (ref != 0) {
          refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
          ref += channels; /* continue;の前に行うこと */
        }

        /* HSVそれぞれに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* RGBにHSVノイズを加える */
        double rr, gg, bb, aa;
        pixel_rgba_(static_cast<double>(image_array[red]) / div_val,
                    static_cast<double>(image_array[gre]) / div_val,
                    static_cast<double>(image_array[blu]) / div_val, 1.0,
                    refv * hue_scale, refv * sat_scale, refv * val_scale, 0.0,
                    rr, gg, bb, aa);

        /* 変化後の値を戻す */
        image_array[red] = static_cast<IT>(rr * mul_val);
        image_array[gre] = static_cast<IT>(gg * mul_val);
        image_array[blu] = static_cast<IT>(bb * mul_val);
      }
    }
  } else if (1 == channels) { /* grayscale */
    for (int yy = 0; yy < height; ++yy) {
      for (int xx = 0; xx < width; ++xx, ++image_array) {
        /* 変化量初期値 */
        double refv = 1.0;

        /* 参照画像あればピクセル単位の画像変化量を得る */
        if (ref != 0) {
          refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
          ref += channels; /* continue;の前に行うこと */
        }

        /* Lに対するオフセット済ノイズ値 */
        refv *= (noi.noise(xx, yy) - offset);

        /* 変化なしなら次へ */
        if (0.0 == refv * val_scale) {
          continue;
        }

        /* GrayscaleにLノイズを加える */
        double val =
            static_cast<double>(image_array[0]) / div_val + refv * val_scale;
        val = (val < 0.0) ? 0.0 : ((1.0 < val) ? 1.0 : val);

        /* 変化後の値を戻す */
        image_array[0] = static_cast<IT>(val * mul_val);
      }
    }
  }
}
}

void igs::hsv_add::change(
    unsigned char *image_array, const int height, const int width,
    const int channels, const int bits

    ,
    const unsigned char *noi_image_array, const int noi_height,
    const int noi_width, const int noi_channels, const int noi_bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、channels数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const int xoffset, const int yoffset, const int from_rgba,
    const double offset, const double hue_scale, const double sat_scale,
    const double val_scale, const double alp_scale

    ,
    const bool add_blend_sw) {
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
  noise_ref_ noi(noi_image_array

                 ,
                 noi_height, noi_width, noi_channels, noi_bits

                 ,
                 xoffset, yoffset, from_rgba);

  /* rgb(a)画像にhsv(a)でドットノイズを加える */
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
}
