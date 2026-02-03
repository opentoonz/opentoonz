// #include <iostream>
#include "igs_motion_wind_table.h"
#include "igs_motion_wind_pixel.h"

igs::motion_wind::pixel::pixel(
    const bool blow_dark_sw, const bool blow_alpha_sw

    ,
    const unsigned long length_random_seed, const double length_min,
    const double length_max, const double length_bias, const bool length_ref_sw

    ,
    const unsigned long force_random_seed, const double force_min,
    const double force_max, const double force_bias, const bool force_ref_sw

    ,
    const unsigned long density_random_seed, const double density_min,
    const double density_max, const double density_bias,
    const bool density_ref_sw)
    : blow_dark_sw_(blow_dark_sw)
    , blow_alpha_sw_(blow_alpha_sw)

    , length_min_(length_min)
    , length_max_(length_max)
    , length_bias_(length_bias)
    , length_ref_sw_(length_ref_sw)

    , force_min_(force_min)
    , force_max_(force_max)
    , force_bias_(force_bias)
    , force_ref_sw_(force_ref_sw)

    , density_min_(density_min)
    , density_max_(density_max)
    , density_bias_(density_bias)
    , density_ref_sw_(density_ref_sw)

    , key_lightness_(0)
    , table_len_(0)
    , table_pos_(0)
    , table_array_(0) {
  this->table_.resize(igs::motion_wind::table_size(length_min, length_max));
  this->length_random_.seed(length_random_seed);
  this->force_random_.seed(force_random_seed);
  this->density_random_.seed(density_random_seed);
}
void igs::motion_wind::pixel::clear(void) { this->table_.clear(); }
//----------------------------------------------------------------------
namespace {
void wind_rgba_(const double key, const double alp, const double ratio,
                bool &sw, double &tgt) {
  if (tgt < key) {
    tgt += ratio * (key - tgt) * alp;
    tgt = (tgt < 0.0) ? 0.0 : ((1.0 < tgt) ? 1.0 : tgt);  // limit
    sw  = true;
  }
}
void wind_rgb_(const double key, const double ratio, bool &sw, double &tgt) {
  if (tgt < key) {
    // std::cout << "(R" << ratio << ",K" << key << ",T" << tgt;
    tgt += ratio * (key - tgt);
    // std::cout << ">" << tgt << ")\n";
    tgt = (tgt < 0.0) ? 0.0 : ((1.0 < tgt) ? 1.0 : tgt);  // limit
    sw  = true;
  }
}
void wind_a_(const double key /* alpha値 */
             ,
             const double ratio, const bool sw, double &tgt /* alpha値 */
             ) {
  if ((tgt < key) || sw) {
    double val = tgt + (ratio * (key - tgt));
    if (tgt < val) { /* 元より明るいときのみ変化 */
      tgt = val;
      tgt = (tgt < 0.0) ? 0.0 : ((1.0 < tgt) ? 1.0 : tgt);  // limit
    }
  }
}
//-----------------------------------------------------------
void blow_wind_(/* 風の影響による値の変化 */
                const int channels, const double *key, const double *tbl_a,
                const int tbl_p, const bool blow_alpha_sw, double *tgt) {
  bool sw = false; /* rgb風を実行したか否かを示す */

  if (igs::image::rgba::siz == channels) { /* Alphaがある */
    using namespace igs::image::rgba;
    if (blow_alpha_sw) { /* Alpha風を実行する */
                         /* RGB風 */
      wind_rgb_(key[red], tbl_a[tbl_p], sw, tgt[red]);
      wind_rgb_(key[gre], tbl_a[tbl_p], sw, tgt[gre]);
      wind_rgb_(key[blu], tbl_a[tbl_p], sw, tgt[blu]);
      /* Alpha風 */
      /* Alpha独自の風と、RGB風の、両方の影響による値の変化 */
      wind_a_(key[alp], tbl_a[tbl_p], sw, tgt[alp]);
    } else { /* Alpha風を実行せず、マスク抜きのRGB風を生成 */
      wind_rgba_(key[red], tgt[alp], tbl_a[tbl_p], sw, tgt[red]);
      wind_rgba_(key[gre], tgt[alp], tbl_a[tbl_p], sw, tgt[gre]);
      wind_rgba_(key[blu], tgt[alp], tbl_a[tbl_p], sw, tgt[blu]);
    }
  } else { /* channel毎に風を生成する */
    for (int zz = 0; zz < channels; ++zz) {
      wind_rgb_(key[zz], tbl_a[tbl_p], sw, tgt[zz]);
    }
  }
}
void invert_pixel_(const int channels, double *pixel) {
  if (igs::image::rgba::siz == channels) { /* Alphaがある */
    using namespace igs::image::rgba;
    pixel[red] = (1.0 - pixel[red]) * pixel[alp];
    pixel[gre] = (1.0 - pixel[gre]) * pixel[alp];
    pixel[blu] = (1.0 - pixel[blu]) * pixel[alp];
  } else /* Alphaがないなら、ただ白黒反転 */
      if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    pixel[red] = 1.0 - pixel[red];
    pixel[gre] = 1.0 - pixel[gre];
    pixel[blu] = 1.0 - pixel[blu];
  } else {
    for (int zz = 0; zz < channels; ++zz) {
      pixel[zz] = 1.0 - pixel[zz];
    }
  }
}
void rgb_to_lightness_(const double re, const double gr, const double bl,
                       double &li) {
  li = ((re < gr) ? ((gr < bl) ? bl : gr)
                  : (((re < bl) ? bl : re) + ((gr < re) ? ((bl < gr) ? bl : gr)
                                                        : ((bl < re) ? bl : re)))) /
       2.0;
}
double get_lightness_(const int channels, const double *pixel
                      // , const bool blow_dark_sw
                      ) {
  double lightness;

  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    rgb_to_lightness_(pixel[red], pixel[gre], pixel[blu], lightness);
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    rgb_to_lightness_(pixel[red], pixel[gre], pixel[blu], lightness);
  } else {
    lightness = pixel[0];
  }
  /* 指定があるなら白黒反転 */
  // if (blow_dark_sw) { lightness = 1.0 - lightness; }
  return lightness;
}
}
//------------------------------------------------------------
int igs::motion_wind::pixel::change(
    const bool key_reset_sw

    /***, const int ref_channel
    , const double *ref_pixel***/

    ,
    const double ref_val /* ゼロ以上なら有効値、マイナスなら無効 */
    ,
    const int channels, double *pixel_tgt) {
  /* 指定があるなら白黒反転、
  このプログラム内ではすべて明るさで処理する */
  if (this->blow_dark_sw_) {
    invert_pixel_(channels, pixel_tgt);
  }

  /* 現位置の明るさを求める */
  const double crnt_lightness =
      get_lightness_(channels, pixel_tgt  // , this->blow_dark_sw_
                     );

  /* スキャンラインの始点のではkey pixel値のreset */
  if (key_reset_sw) {
    this->key_lightness_ = crnt_lightness;
    for (int ii = 0; ii < channels; ++ii) {
      this->pixel_key_[ii] = pixel_tgt[ii];
    }
  }

  /* 値がkeyより小さい(山の頂上より小さい)時 */
  if (crnt_lightness < this->key_lightness_) {
    /* 風テーブルがゼロなら
    風データを設定し、風テーブルを設定する */
    if (0 == this->table_array_) {
      double length_min   = this->length_min_;
      double length_max   = this->length_max_;
      double length_bias  = this->length_bias_;
      double force_min    = this->force_min_;
      double force_max    = this->force_max_;
      double force_bias   = this->force_bias_;
      double density_min  = this->density_min_;
      double density_max  = this->density_max_;
      double density_bias = this->density_bias_;
#if 0
			double ref_value;
			/* 画像から強弱を得る */
			if (0 != ref_pixel) {/* 参照画像のRGBAから */
				ref_value = ref_pixel[ref_channel];
			} else { /* 自画像から強弱を得る */
				ref_value = get_lightness_(
					channels, pixel_tgt);
			}
#endif
      double ref_value = ref_val;
      /* 参照画像から取らないときは自画像から強弱を得る */
      if (ref_value < 0.0) {
        ref_value = get_lightness_(channels, pixel_tgt);
      }

      if (this->length_ref_sw_) {
        length_min *= ref_value;
        length_max *= ref_value;
      }
      if (this->force_ref_sw_) {
        force_min *= ref_value;
        force_max *= ref_value;
      }
      if (this->density_ref_sw_) {
        density_min *= ref_value;
        density_max *= ref_value;
      }

      /* 風データを得る(流れのパターンの更新) */
      /***std::cout
<< "lmin " << length_min << "  lmax " << length_max
<< "\ncmin " << force_min  << "  cmax " << force_max
<< "\nhmin " << density_min << "  hmax " << density_max
<< "\n"
;***/
      /* 更新した、風データを、カレントにセットする */
      this->table_len_ = igs::motion_wind::make_table(
          this->table_, this->length_random_, this->force_random_,
          this->density_random_, length_min, length_max, length_bias, force_min,
          force_max, force_bias, density_min, density_max, density_bias);
      this->table_array_ = &this->table_.at(0);
      this->table_pos_   = 1;
      /***for (int ii=0;ii<this->table_len_;++ii) {
std::cout << this->table_array_[ii] << "\n";
}***/

      /* 風の中である。
      風の長さは１かもしれない... */
    }

    /* 風に影響された値を出力値にいれる */
    if (this->table_pos_ < this->table_len_) {
      /* 風の計算 */
      blow_wind_(channels, this->pixel_key_, this->table_array_,
                 this->table_pos_, this->blow_alpha_sw_, pixel_tgt);
      ++this->table_pos_;

      /* 保存すべき値となったので白黒反転を戻す */
      if (this->blow_dark_sw_) {
        invert_pixel_(channels, pixel_tgt);
      }

      return true; /* 風ん中 */
    }
  }

  /*	ここに来たということは、
          風の外である、
                  つまり風の影響範囲からはずれたとき、
                  つまりtable_pos_がtable_len_から外れた、
          あるいは、
          値が等しいかkeyより大きい値のとき、
                  つまりcrnt_lightnessがkey_lightness_以上、
          なので、現在値をメモリする(keyにいれる)
  */

  this->key_lightness_ = crnt_lightness;
  for (int ii = 0L; ii < channels; ++ii) {
    this->pixel_key_[ii] = pixel_tgt[ii];
  }

  /* 風が途切れたので風テーブルをゼロとする */
  this->table_array_ = 0;

  return false; /* 風の外 */
}
