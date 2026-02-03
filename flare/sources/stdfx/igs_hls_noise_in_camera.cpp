#include <vector>
#include <stdexcept>  // std::domain_error
#include <limits>     // std::numeric_limits
#include "igs_color_rgb_hls.h"
#include "igs_math_random.h"
#include "igs_ifx_common.h"  // igs::image::rgba
#include "igs_hls_noise_in_camera.h"

namespace igs {
namespace hls_noise_in_camera {
/* バッファをまとめて確保(return or throwで自動解放) */
class noise_reference {
public:
  noise_reference(const int ww, const int hh, const double hue_range,
                  const double lig_range, const double sat_range,
                  const double alp_range, const unsigned long random_seed,
                  const double near_blur

                  ,
                  const int camera_x, const int camera_y, const int camera_w,
                  const int camera_h)
      : w_(ww), h_(hh), nblur_(near_blur) {
    if (0 == ww) {
      return;
    }
    if (0 == hh) {
      return;
    }

    /* 枚数ゼロ(rangeが全部ゼロ)なのでノイズかけない */
    if ((0.0 == hue_range) && (0.0 == lig_range) && (0.0 == sat_range) &&
        (0.0 == alp_range)) {
      return;
    }

    /* memory確保 */
    const int sz = ww * hh;
    if (0.0 != hue_range) {
      this->hue_array_.resize(sz);
    }
    if (0.0 != lig_range) {
      this->lig_array_.resize(sz);
    }
    if (0.0 != sat_range) {
      this->sat_array_.resize(sz);
    }
    if (0.0 != alp_range) {
      this->alp_array_.resize(sz);
    }

    /* タネを設定 */
    igs::math::random hue_rand, lig_rand, sat_rand, alp_rand;
    unsigned long step = 0;
    if (0.0 != hue_range) {
      hue_rand.seed(random_seed + step++);
    }
    if (0.0 != lig_range) {
      lig_rand.seed(random_seed + step++);
    }
    if (0.0 != sat_range) {
      sat_rand.seed(random_seed + step++);
    }
    if (0.0 != alp_range) {
      alp_rand.seed(random_seed + step++);
    }
    igs::math::random hue_rand_ext, lig_rand_ext, sat_rand_ext, alp_rand_ext;
    if (0.0 != hue_range) {
      hue_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != lig_range) {
      lig_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != sat_range) {
      sat_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != alp_range) {
      alp_rand_ext.seed(random_seed + step++);
    }

    /* ノイズを書く */
    /* -???_range/2〜???_range/2 */
    int x1 = camera_x;
    int y1 = camera_y;
    int x2 = camera_x + camera_w - 1;
    int y2 = camera_y + camera_h - 1;

    /* マージンあるかないかでノイズパターンが変わってしまうため、
near_blur用に広げてはいけない */
    // if (0.0 != near_blur) { x1-=1; x2+=1; y1-=1; y2+=1; }

    if (0.0 != hue_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            this->hue_array_[pos] = hue_range * (hue_rand_ext.next_d() - 0.5);
          } else {
            this->hue_array_[pos] = hue_range * (hue_rand.next_d() - 0.5);
          }
        }
      }
    }
    if (0.0 != lig_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            this->lig_array_[pos] = lig_range * (lig_rand_ext.next_d() - 0.5);
          } else {
            this->lig_array_[pos] = lig_range * (lig_rand.next_d() - 0.5);
          }
        }
      }
    }
    if (0.0 != sat_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            this->sat_array_[pos] = sat_range * (sat_rand_ext.next_d() - 0.5);
          } else {
            this->sat_array_[pos] = sat_range * (sat_rand.next_d() - 0.5);
          }
        }
      }
    }
    if (0.0 != alp_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            this->alp_array_[pos] = alp_range * (alp_rand_ext.next_d() - 0.5);
          } else {
            this->alp_array_[pos] = alp_range * (alp_rand.next_d() - 0.5);
          }
        }
      }
    }
  }
  double hue_value(const int xx, const int yy) {
    return this->noise_value_(this->hue_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  double lig_value(const int xx, const int yy) {
    return this->noise_value_(this->lig_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  double sat_value(const int xx, const int yy) {
    return this->noise_value_(this->sat_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  double alp_value(const int xx, const int yy) {
    return this->noise_value_(this->alp_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  void clear() {
    this->alp_array_.clear();
    this->sat_array_.clear();
    this->lig_array_.clear();
    this->hue_array_.clear();
  }
  ~noise_reference() { this->clear(); }

private:
  const int w_;
  const int h_;
  const double nblur_;
  std::vector<double> hue_array_;
  std::vector<double> lig_array_;
  std::vector<double> sat_array_;
  std::vector<double> alp_array_;
  void accum_in_(const double *noise_array, const int ww, const int hh,
                 const int xx, const int yy, double &accum_val,
                 int &accum_count) {
    if ((0 <= xx) && (xx < ww) && (0 <= yy) && (yy < hh)) {
      accum_val += noise_array[yy * ww + xx];
      ++accum_count;
    }
  }
  double noise_value_(const std::vector<double> &noise_vector, const int ww,
                      const int hh, const int xx, const int yy,
                      const double near_blur) {
    if (noise_vector.size() <= 0) {
      return 0.0;
    }
    const double *noise_array = &noise_vector.at(0);
    if (0.0 == near_blur) {
      return noise_array[yy * ww + xx];
    }

    double accum_val = 0.0;
    int accum_count  = 0;
    this->accum_in_(noise_array, ww, hh, xx - 1, yy - 1, accum_val,
                    accum_count);
    this->accum_in_(noise_array, ww, hh, xx, yy - 1, accum_val, accum_count);
    this->accum_in_(noise_array, ww, hh, xx + 1, yy - 1, accum_val,
                    accum_count);
    this->accum_in_(noise_array, ww, hh, xx - 1, yy, accum_val, accum_count);
    this->accum_in_(noise_array, ww, hh, xx + 1, yy, accum_val, accum_count);
    this->accum_in_(noise_array, ww, hh, xx - 1, yy + 1, accum_val,
                    accum_count);
    this->accum_in_(noise_array, ww, hh, xx, yy + 1, accum_val, accum_count);
    this->accum_in_(noise_array, ww, hh, xx + 1, yy + 1, accum_val,
                    accum_count);
    if (accum_count <= 0) {
      return noise_array[yy * ww + xx];
    }
    accum_val /= static_cast<double>(accum_count);

    /* 中心Pixelと廻り(1Pixel幅)の平均値とのバランスを返す */
    return (near_blur * accum_val) +
           ((1.0 - near_blur) * noise_array[yy * ww + xx]);
  }

  /* copy constructorを無効化 */
  noise_reference(const noise_reference &);

  /* 代入演算子を無効化 */
  noise_reference &operator=(const noise_reference &);
};
}
}
namespace igs {
namespace hls_noise_in_camera {
/* 端値を適度に調整する */
class control_term_within_limits {
public:
  control_term_within_limits(const double effective_low  = 0.0,
                             const double effective_high = 0.0,
                             const double center = 0.5, const int type = 0,
                             const double noise_range = 0.0)
      : effective_low_(effective_low)
      , effective_high_(effective_high)
      , center_(center)
      , type_(static_cast<term_type_>(type))
      , noise_range_(noise_range) {}
  void exec(const double current_value /* 0...1 */
            ,
            double &noise /* -noise_range/2...noise_range/2 */
            ,
            double &shift_value) {
    if ((0.0 < this->effective_low_) && (current_value < this->center_)) {
      const double cen = this->center_;
      const double val = current_value;
      const double ran = this->noise_range_;
      const double eff = this->effective_low_;
      switch (this->type_) {
      case shift_all_:
        shift_value = ((cen - val) / cen) * (ran / 2.0) * eff;
        break;
      case shift_term_:
        if (val < ran) {
          shift_value = (((cen < ran) ? cen : ran) - val) / 2.0 * eff;
        }
        break;
      case decrease_all_: {
        const double tmp = (cen - val) / cen * eff;
        if (0.0 < tmp) {
          noise *= 1.0 - tmp;
        }
      } break;
      case decrease_term_:
        if (val < (ran / 2.0)) {
          const double stop = (cen < (ran / 2.0)) ? cen : ran / 2.0;
          const double tmp  = (stop - val) / stop * eff;
          if (0.0 < tmp) {
            noise *= 1.0 - tmp;
          }
        }
        break;
      }
    }

    if ((0.0 < this->effective_high_) && (this->center_ < current_value)) {
      const double cen = this->center_;
      const double val = current_value;
      const double ran = this->noise_range_;
      const double eff = this->effective_high_;
      switch (this->type_) {
      case shift_all_:
        shift_value = ((cen - val) / (1.0 - cen)) * (ran / 2.0) * eff;
        break;
      case shift_term_:
        if ((1.0 - ran) < val) {
          const double ira = 1.0 - ran;
          shift_value      = (((cen < ira) ? ira : cen) - val) / 2.0 * eff;
        }
        break;
      case decrease_all_: {
        const double tmp = (val - cen) / (1.0 - cen) * eff;
        if (0.0 < tmp) {
          noise *= 1.0 - tmp;
        }
      } break;
      case decrease_term_:
        if ((1.0 - (ran / 2.0)) < val) {
          const double rpos = 1.0 - (ran / 2.0);
          const double stop = (cen < rpos) ? rpos : cen;
          const double tmp  = (val - stop) / (1.0 - stop) * eff;
          if (0.0 < tmp) {
            noise *= 1.0 - tmp;
          }
        }
        break;
      }
    }
  }
  double noise_range(void) const { return this->noise_range_; }

private:
  /* low,high両方ゼロ =exec()内処理せず =端値はカット =default */
  const double effective_low_;
  const double effective_high_;

  const double center_;

  enum term_type_ {                 /* 端値の調整方法 */
                    shift_all_ = 0, /* 0.全体的にノイズ位置がずれる */
                    shift_term_, /* 1.端のみでノイズ位置がずれる */
                    decrease_all_,  /* 2.全体的にノイズ幅が減る */
                    decrease_term_, /* 3.端のみでノイズ幅が減る */
  };
  const term_type_ type_;

  const double noise_range_;

  /* copy constructorを無効化 */
  control_term_within_limits(const control_term_within_limits &);

  /* 代入演算子を無効化 */
  control_term_within_limits &operator=(const control_term_within_limits &);
};
}
}
namespace igs {
namespace hls_noise_in_camera {
/* RGB値にノイズをのせる */
void pixel_rgb(const double red_in, const double gre_in, const double blu_in,
               const double alp_in, const double hue_noise,
               const double lig_noise, const double sat_noise,
               control_term_within_limits &lig_term,
               control_term_within_limits &sat_term, double &red_out,
               double &gre_out, double &blu_out);
/* Alpha値にノイズをのせる */
void pixel_a(const double alp_in, const double alp_noise,
             control_term_within_limits &alp_term, double &alp_out);
}
}
void igs::hls_noise_in_camera::pixel_rgb(
    const double red_in, const double gre_in, const double blu_in,
    const double alp_in, const double hue_noise, const double lig_noise,
    const double sat_noise, control_term_within_limits &lig_term,
    control_term_within_limits &sat_term, double &red_out, double &gre_out,
    double &blu_out) {
  if (0.0 == alp_in) {
    red_out = red_in;
    gre_out = gre_in;
    blu_out = blu_in;
    return;
  }
  double hue, lig, sat;
  igs::color::rgb_to_hls(red_in, gre_in, blu_in, hue, lig, sat);
  if (0.0 != hue_noise) {
    hue += 360.0 * hue_noise * alp_in;
    while (hue < 0.0) {
      hue += 360.0;
    }
    while (360.0 <= hue) {
      hue -= 360.0;
    }
  }
  if (0.0 != lig_term.noise_range()) {
    double shift_value = 0;
    double lignoise    = lig_noise;
    lig_term.exec(lig, lignoise, shift_value);
    lig += shift_value * alp_in;
    lig += lignoise * alp_in;
    if (lig < 0.0) {
      lig = 0.0;
    } else if (1.0 < lig) {
      lig = 1.0;
    }
  }
  if (0.0 != sat_term.noise_range()) {
    double shift_value = 0;
    double satnoise    = sat_noise;
    sat_term.exec(sat, satnoise, shift_value);
    sat += shift_value * alp_in;
    sat += satnoise * alp_in;
    if (sat < 0.0) {
      sat = 0.0;
    } else if (1.0 < sat) {
      sat = 1.0;
    }
    // if( 0.0 == sat ) hue = -1.0; // hls_to_rgb(-)
  }
  igs::color::hls_to_rgb(hue, lig, sat, red_out, gre_out, blu_out);
}
void igs::hls_noise_in_camera::pixel_a(const double alp_in,
                                       const double alp_noise,
                                       control_term_within_limits &alp_term,
                                       double &alp_out) {
  // if (0.0 == alp_in) { return; }
  double alpin = alp_in;
  if (0.0 != alp_term.noise_range()) {
    double shift_value = 0.0;
    double alpnoise    = alp_noise;
    alp_term.exec(alpin, alpnoise, shift_value);
    const double mask = alpin;
    alpin += shift_value * mask;
    alpin += alpnoise * mask;
    if (alpin < 0.0) {
      alpin = 0.0;
    } else if (1.0 < alpin) {
      alpin = 1.0;
    }
  }
  alp_out = alpin;
}
namespace igs {
namespace hls_noise_in_camera {
/* raster画像にノイズをのせるtemplate */
template <class T>
void change_template_(T *image_array, const int ww, const int hh, const int ch,
                      noise_reference &noise, const double hue_range,
                      control_term_within_limits &lig_term,
                      control_term_within_limits &sat_term,
                      control_term_within_limits &alp_term) {
  const double div_val = static_cast<double>(std::numeric_limits<T>::max());
  const double mul_val =
      static_cast<double>(std::numeric_limits<T>::max()) + 0.999999;
  double rr, gg, bb, aa;

  if (igs::image::rgba::siz == ch) {
    using namespace igs::image::rgba;
    for (int yy = 0; yy < hh; ++yy) {
      for (int xx = 0; xx < ww; ++xx) {
        if (((0.0 != hue_range) || (0.0 != lig_term.noise_range()) ||
             (0.0 !=
              sat_term.noise_range())) /* ノイズがhlsのどれか一つはある */
            ) {
          pixel_rgb(static_cast<double>(image_array[red]) / div_val,
                    static_cast<double>(image_array[gre]) / div_val,
                    static_cast<double>(image_array[blu]) / div_val,
                    static_cast<double>(image_array[alp]) / div_val,
                    noise.hue_value(xx, yy), noise.lig_value(xx, yy),
                    noise.sat_value(xx, yy), lig_term, sat_term, rr, gg, bb);
          image_array[red] = static_cast<T>(rr * mul_val);
          image_array[gre] = static_cast<T>(gg * mul_val);
          image_array[blu] = static_cast<T>(bb * mul_val);
        }
        if (0.0 != alp_term.noise_range()) {
          pixel_a(static_cast<double>(image_array[alp]) / div_val,
                  noise.alp_value(xx, yy), alp_term, aa);
          image_array[alp] = static_cast<T>(aa * mul_val);
        }
        image_array += ch;
      }
    }
  } else if (igs::image::rgb::siz == ch) {
    using namespace igs::image::rgb;
    if (((0.0 != hue_range) || (0.0 != lig_term.noise_range()) ||
         (0.0 != sat_term.noise_range())) /* ノイズがhlsのどれか一つはある */
        ) {
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, image_array += ch) {
          pixel_rgb(static_cast<double>(image_array[red]) / div_val,
                    static_cast<double>(image_array[gre]) / div_val,
                    static_cast<double>(image_array[blu]) / div_val, 1.0,
                    noise.hue_value(xx, yy), noise.lig_value(xx, yy),
                    noise.sat_value(xx, yy), lig_term, sat_term, rr, gg, bb);
          image_array[red] = static_cast<T>(rr * mul_val);
          image_array[gre] = static_cast<T>(gg * mul_val);
          image_array[blu] = static_cast<T>(bb * mul_val);
        }
      }
    }
  } else if (1 == ch) { /* grayscale */
    if (0.0 != lig_term.noise_range()) {
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++image_array) {
          double lig         = static_cast<double>(image_array[0]) / div_val;
          double shift_value = 0;
          double lig_noise   = noise.lig_value(xx, yy);
          lig_term.exec(lig, lig_noise, shift_value);
          lig += shift_value;
          lig += lig_noise;
          if (lig < 0.0) {
            lig = 0.0;
          } else if (1.0 < lig) {
            lig = 1.0;
          }
          image_array[0] = static_cast<T>(lig * mul_val);
        }
      }
    }
  }
}
}
}

void igs::hls_noise_in_camera::change(
    void *image_array

    ,
    const int height, const int width, const int channels, const int bits

    ,
    const int camera_x, const int camera_y, const int camera_w,
    const int camera_h

    ,
    const double hue_range, const double lig_range, const double sat_range,
    const double alp_range, const unsigned long random_seed,
    const double near_blur

    ,
    const double lig_effective, const double lig_center, const int lig_type,
    const double sat_effective, const double sat_center, const int sat_type,
    const double alp_effective, const double alp_center, const int alp_type) {
  if ((0.0 == hue_range) && (0.0 == lig_range) && (0.0 == sat_range) &&
      (0.0 == alp_range)) {
    return;
  }

  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  /* ノイズ参照画像を作成する */
  noise_reference noise(width, height, hue_range, lig_range, sat_range,
                        alp_range, random_seed, near_blur, camera_x, camera_y,
                        camera_w, camera_h);

  /* 端値を適度に調整する設定 */
  control_term_within_limits lig_term(lig_effective, lig_effective, lig_center,
                                      lig_type, lig_range);
  control_term_within_limits sat_term(sat_effective, sat_effective, sat_center,
                                      sat_type, sat_range);
  control_term_within_limits alp_term(alp_effective, alp_effective, alp_center,
                                      alp_type, alp_range);

  /* rgb(a)画像にhls(a)でドットノイズを加える */
  if (std::numeric_limits<unsigned char>::digits == bits) {
    change_template_(static_cast<unsigned char *>(image_array), width, height,
                     channels, noise, hue_range, lig_term, sat_term, alp_term);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    change_template_(static_cast<unsigned short *>(image_array), width, height,
                     channels, noise, hue_range, lig_term, sat_term, alp_term);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
