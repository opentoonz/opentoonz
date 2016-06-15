#include <iostream>
#include <vector>
#include <limits>  // std::numeric_limits
#include "igs_color_rgb_hsv.h"
#include "igs_math_random.h"
#include "igs_ifx_common.h"  // igs::image::rgba
namespace {
/*------ バッファをまとめて確保(return or throwで自動解放) ------*/
class noise_reference_ {
public:
  noise_reference_(const int ww, const int hh, const double hue_range,
                   const double sat_range, const double val_range,
                   const double alp_range, const unsigned long random_seed,
                   const double near_blur, const int camera_x,
                   const int camera_y, const int camera_w, const int camera_h)
      : w_(ww), h_(hh), nblur_(near_blur) {
    if (0 == ww) {
      return;
    }
    if (0 == hh) {
      return;
    }

    /* 枚数ゼロ(rangeが全部ゼロ)なのでノイズかけない */
    if ((0.0 == hue_range) && (0.0 == sat_range) && (0.0 == val_range) &&
        (0.0 == alp_range)) {
      return;
    }

    /* memory確保 */
    const int sz = ww * hh;
    if (0.0 != hue_range) {
      this->hue_array_.resize(sz);
    }
    if (0.0 != sat_range) {
      this->sat_array_.resize(sz);
    }
    if (0.0 != val_range) {
      this->val_array_.resize(sz);
    }
    if (0.0 != alp_range) {
      this->alp_array_.resize(sz);
    }

    /* タネを設定 */
    igs::math::random hue_rand, sat_rand, val_rand, alp_rand;
    unsigned long step = 0;
    if (0.0 != hue_range) {
      hue_rand.seed(random_seed + step++);
    }
    if (0.0 != sat_range) {
      sat_rand.seed(random_seed + step++);
    }
    if (0.0 != val_range) {
      val_rand.seed(random_seed + step++);
    }
    if (0.0 != alp_range) {
      alp_rand.seed(random_seed + step++);
    }
    igs::math::random hue_rand_ext, sat_rand_ext, val_rand_ext, alp_rand_ext;
    if (0.0 != hue_range) {
      hue_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != sat_range) {
      sat_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != val_range) {
      val_rand_ext.seed(random_seed + step++);
    }
    if (0.0 != alp_range) {
      alp_rand_ext.seed(random_seed + step++);
    }

    /* ノイズを書く */
    /* -???_range/2〜???_range/2 */
    const int x1 = camera_x;
    const int y1 = camera_y;
    const int x2 = camera_x + camera_w - 1;
    const int y2 = camera_y + camera_h - 1;

    /* マージンあるかないかでノイズパターンが変わってしまうため、
near_blur用に広げてはいけない */
    // if (0.0 != near_blur) { x1-=1; x2+=1; y1-=1; y2+=1; }

    if (0.0 != hue_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            /* 計算用マージンがあったらそこは別のパターン */
            this->hue_array_[pos] = hue_range * (hue_rand_ext.next_d() - 0.5);
          } else {
            /* 計算用マージン幅が変化してもその中のノイズパターンは
変わらないようにマージン内は別計算 */
            this->hue_array_[pos] = hue_range * (hue_rand.next_d() - 0.5);
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
    if (0.0 != val_range) {
      int pos = 0;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, ++pos) {
          if ((xx < x1) || (x2 < xx) || (yy < y1) || (y2 < yy)) {
            this->val_array_[pos] = val_range * (val_rand_ext.next_d() - 0.5);
          } else {
            this->val_array_[pos] = val_range * (val_rand.next_d() - 0.5);
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
  double sat_value(const int xx, const int yy) {
    return this->noise_value_(this->sat_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  double val_value(const int xx, const int yy) {
    return this->noise_value_(this->val_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  double alp_value(const int xx, const int yy) {
    return this->noise_value_(this->alp_array_, this->w_, this->h_, xx, yy,
                              this->nblur_);
  }
  void clear() {
    this->alp_array_.clear();
    this->val_array_.clear();
    this->sat_array_.clear();
    this->hue_array_.clear();
  }
  ~noise_reference_() { this->clear(); }

private:
  const int w_;
  const int h_;
  const double nblur_;
  std::vector<double> hue_array_;
  std::vector<double> sat_array_;
  std::vector<double> val_array_;
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
    /*
ノイズをなじませるために、
8近傍ピクセルと対象ピクセルとの重み付けをする処理
+-----+-----+-----+
| p00 | p01 | p02 |
+-----+-----+-----+
| p10 | p11 | p12 |
+-----+-----+-----+
| p20 | p21 | p22 |
+-----+-----+-----+
対象ピクセル = p11
8近傍ピクセル = p00,p01,p02,p10,p12,p20,p21,p22
重み付け = near_blur = nb = 0...1
注意：対象ピクセルが画像のエッジにあると、8近傍ピクセルには、
    存在しないピクセルがあるのでそれは除いて計算する
p00+p01+p02+p10+p12+p20+p21+p22
------------------------------- x nb + p22 x (1 - nb)
            8
                                  nb
= (p00+p01+p02+p10+p12+p20+p21+p22) x -- + p22 x (1 - nb)
                                   8
nb == 0のとき
    --> 対象ピクセルの値
    = p22
nb == 0.5のとき
    --> 対象が半分、8近傍平均値が半分
    = (p00+p01+p02+p10+p12+p20+p21+p22) / 16 + p22 / 2
nb == 0.888...(8/9)のとき
    --> 対象と8近傍クセルの平均値
    = (p00+p01+p02+p10+p12+p20+p21+p22) x 8 / 72 + p22 / 9
nb == 1のとき
    --> 8近傍ピクセルのみの平均値
    = (p00+p01+p02+p10+p12+p20+p21+p22) / 8
*/
  }

  /* copy constructorを無効化 */
  noise_reference_(const noise_reference_ &);

  /* 代入演算子を無効化 */
  noise_reference_ &operator=(const noise_reference_ &);
};
/*------ 端値を適度に調整する ------*/
class control_term_within_limits_ {
public:
  control_term_within_limits_(const double effective_low /* = 0.0 */
                              ,
                              const double effective_high /* = 0.0 */
                              ,
                              const double center /* = 0.5 */
                              ,
                              const int type /* = 0   */
                              ,
                              const double noise_range /* = 0.0 */
                              )
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
  control_term_within_limits_(const control_term_within_limits_ &);

  /* 代入演算子を無効化 */
  control_term_within_limits_ &operator=(const control_term_within_limits_ &);
};
/*------ RGB値にノイズをのせる ------*/
void pixel_rgb_(const double red_in, const double gre_in, const double blu_in,
                const double alp_in, const double hue_noise,
                const double sat_noise, const double val_noise,
                control_term_within_limits_ &sat_term,
                control_term_within_limits_ &val_term, double &red_out,
                double &gre_out, double &blu_out) {
  if (0.0 == alp_in) {
    red_out = red_in;
    gre_out = gre_in;
    blu_out = blu_in;
    return;
  }
  double hue, sat, val;
  igs::color::rgb_to_hsv(red_in, gre_in, blu_in, hue, sat, val);
  if (0.0 != hue_noise) {
    hue += 360.0 * hue_noise * alp_in;
    while (hue < 0.0) {
      hue += 360.0;
    }
    while (360.0 <= hue) {
      hue -= 360.0;
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
    // if( 0.0 == sat ) hue = -1.0; // hsv_to_rgb(-)
  }
  if (0.0 != val_term.noise_range()) {
    double shift_value = 0;
    double valnoise    = val_noise;
    val_term.exec(val, valnoise, shift_value);
    val += shift_value * alp_in;
    val += valnoise * alp_in;
    if (val < 0.0) {
      val = 0.0;
    } else if (1.0 < val) {
      val = 1.0;
    }
  }
  igs::color::hsv_to_rgb(hue, sat, val, red_out, gre_out, blu_out);
}
/*------ Alpha値にノイズをのせる ------*/
void pixel_a_(const double alp_in, const double alp_noise,
              control_term_within_limits_ &alp_term, double &alp_out) {
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
/*------ raster画像にノイズをのせるtemplate ------*/
template <class IT, class RT>
void change_template_(
    IT *image_array, const int width, const int height, const int channels

    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    noise_reference_ &noise, const double hue_range,
    control_term_within_limits_ &sat_term,
    control_term_within_limits_ &val_term, control_term_within_limits_ &alp_term

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

        /* マスクSWがON、なら変化をMask */
        if (add_blend_sw && (image_array[alp] < t_max)) {
          refv *= static_cast<double>(image_array[alp]) / div_val;
        }

        if (((0.0 != hue_range) || (0.0 != val_term.noise_range()) ||
             (0.0 !=
              sat_term.noise_range())) /* ノイズがhsvのどれか一つはある */
            ) {
          double rr1 = static_cast<double>(image_array[red]) / div_val,
                 gg1 = static_cast<double>(image_array[gre]) / div_val,
                 bb1 = static_cast<double>(image_array[blu]) / div_val,
                 aa1 = static_cast<double>(image_array[alp]) / div_val;
          double rr2 = 0, gg2 = 0, bb2 = 0;
          pixel_rgb_(rr1, gg1, bb1, aa1, noise.hue_value(xx, yy),
                     noise.sat_value(xx, yy), noise.val_value(xx, yy), sat_term,
                     val_term, rr2, gg2, bb2);
          if (refv != 1.0) {
            rr2 = (rr2 - rr1) * refv + rr1;
            gg2 = (gg2 - gg1) * refv + gg1;
            bb2 = (bb2 - bb1) * refv + bb1;
          }
          image_array[red] = static_cast<IT>(rr2 * mul_val);
          image_array[gre] = static_cast<IT>(gg2 * mul_val);
          image_array[blu] = static_cast<IT>(bb2 * mul_val);
        }
        if (0.0 != alp_term.noise_range()) {
          double aa1 = static_cast<double>(image_array[alp]) / div_val;
          double aa2 = 0;
          pixel_a_(aa1, noise.alp_value(xx, yy), alp_term, aa2);
          if (refv != 1.0) {
            aa2 = (aa2 - aa1) * refv + aa1;
          }
          image_array[alp] = static_cast<IT>(aa2 * mul_val);
        }
      }
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    if (((0.0 != hue_range) || (0.0 != sat_term.noise_range()) ||
         (0.0 != val_term.noise_range())) /* ノイズがhsvのどれか一つはある */
        ) {
      for (int yy = 0; yy < height; ++yy) {
        for (int xx = 0; xx < width; ++xx, image_array += channels) {
          /* 変化量初期値 */
          double refv = 1.0;

          /* 参照画像あればピクセル単位の画像変化量を得る */
          if (ref != 0) {
            refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
            ref += channels; /* continue;の前に行うこと */
          }

          double rr1 = static_cast<double>(image_array[red]) / div_val,
                 gg1 = static_cast<double>(image_array[gre]) / div_val,
                 bb1 = static_cast<double>(image_array[blu]) / div_val;
          double rr2 = 0, gg2 = 0, bb2 = 0;
          pixel_rgb_(rr1, gg1, bb1, 1.0, noise.hue_value(xx, yy),
                     noise.sat_value(xx, yy), noise.val_value(xx, yy), sat_term,
                     val_term, rr2, gg2, bb2);
          if (refv != 1.0) {
            rr2 = (rr2 - rr1) * refv + rr1;
            gg2 = (gg2 - gg1) * refv + gg1;
            bb2 = (bb2 - bb1) * refv + bb1;
          }
          image_array[red] = static_cast<IT>(rr2 * mul_val);
          image_array[gre] = static_cast<IT>(gg2 * mul_val);
          image_array[blu] = static_cast<IT>(bb2 * mul_val);
        }
      }
    }
  } else if (1 == channels) { /* grayscale */
    if (0.0 != val_term.noise_range()) {
      for (int yy = 0; yy < height; ++yy) {
        for (int xx = 0; xx < width; ++xx, ++image_array) {
          /* 変化量初期値 */
          double refv = 1.0;

          /* 参照画像あればピクセル単位の画像変化量を得る */
          if (ref != 0) {
            refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
            ref += channels; /* continue;の前に行うこと */
          }

          double va1         = static_cast<double>(image_array[0]) / div_val;
          double shift_value = 0;
          double val_noise   = noise.val_value(xx, yy);
          val_term.exec(va1, val_noise, shift_value);

          double va2 = va1;
          va2 += shift_value;
          va2 += val_noise;
          va2 = (va2 < 0.0) ? 0.0 : ((1.0 < va2) ? 1.0 : va2);

          if (refv != 1.0) {
            va2 = va1 + (va2 - va1) * refv;
          }

          image_array[0] = static_cast<IT>(va2 * mul_val);
        }
      }
    }
  }
}
}
//--------------------------------------------------------------------

#include <stdexcept>  // std::domain_error
#include "igs_hsv_noise.h"
void igs::hsv_noise::change(
    unsigned char *image_array

    ,
    const int height, const int width, const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、channels数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    /* image_arrayに余白が変化してもノイズパターンが変わらない
            ようにするためにカメラエリアを指定する */
    ,
    const int camera_x, const int camera_y, const int camera_w,
    const int camera_h

    ,
    const double hue_range, const double sat_range, const double val_range,
    const double alp_range, const unsigned long random_seed,
    const double near_blur

    ,
    const double sat_effective, const double sat_center, const int sat_type,
    const double val_effective, const double val_center, const int val_type,
    const double alp_effective, const double alp_center, const int alp_type

    ,
    const bool add_blend_sw) {
  if ((0.0 == hue_range) && (0.0 == sat_range) && (0.0 == val_range) &&
      (0.0 == alp_range)) {
    return;
  }

  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  /* ノイズ参照画像を作成する */
  noise_reference_ noise(width, height, hue_range, sat_range, val_range,
                         alp_range, random_seed, near_blur, camera_x, camera_y,
                         camera_w, camera_h);

  /* 端値を適度に調整する設定 */
  control_term_within_limits_ sat_term(sat_effective, sat_effective, sat_center,
                                       sat_type, sat_range);
  control_term_within_limits_ val_term(val_effective, val_effective, val_center,
                                       val_type, val_range);
  control_term_within_limits_ alp_term(alp_effective, alp_effective, alp_center,
                                       alp_type, alp_range);

  /* rgb(a)画像にhsv(a)でドットノイズを加える */
  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    change_template_(image_array, width, height, channels, ref, ref_mode, noise,
                     hue_range, sat_term, val_term, alp_term, add_blend_sw);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), width,
                     height, channels, ref, ref_mode, noise, hue_range,
                     sat_term, val_term, alp_term, add_blend_sw);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(
        reinterpret_cast<unsigned short *>(image_array), width, height,
        channels, reinterpret_cast<const unsigned short *>(ref), ref_mode,
        noise, hue_range, sat_term, val_term, alp_term, add_blend_sw);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_template_(image_array, width, height, channels,
                     reinterpret_cast<const unsigned short *>(ref), ref_mode,
                     noise, hue_range, sat_term, val_term, alp_term,
                     add_blend_sw);
    noise.clear(); /* ノイズ画像メモリ解放 */
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
