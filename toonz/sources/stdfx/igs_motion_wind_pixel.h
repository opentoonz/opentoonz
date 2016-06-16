#pragma once

#ifndef igs_motion_wind_pixel_h
#define igs_motion_wind_pixel_h

#include <vector>
#include "igs_math_random.h"
#include "igs_ifx_common.h" /* igs::image::rgba::siz */

#ifndef IGS_MOTION_WIND_EXPORT
#define IGS_MOTION_WIND_EXPORT
#endif

namespace igs {
namespace motion_wind {
class IGS_MOTION_WIND_EXPORT pixel {
public:
  pixel(const bool blow_dark_sw, const bool blow_alpha_sw

        ,
        const unsigned long length_random_seed, const double length_min,
        const double length_max, const double length_bias,
        const bool length_ref_sw

        ,
        const unsigned long force_random_seed, const double force_min,
        const double force_max, const double force_bias, const bool force_ref_sw

        ,
        const unsigned long density_random_seed, const double density_min,
        const double density_max, const double density_bias,
        const bool density_ref_sw);

  int change(const bool key_reset_sw
             /***, const int ref_channel
	, const double *ref_pixel***/ /* ゼロなら参照しない */
             ,
             const double ref_val /* ゼロ以上なら有効値、マイナスなら無効 */
             ,
             const int channels, double *pixel_tgt);
  void clear(void);

private:
  std::vector<double> table_; /* 減少テーブル */

  igs::math::random length_random_;   // default seed is 1
  igs::math::random force_random_;    // default seed is 1
  igs::math::random density_random_;  // default seed is 1

  const bool blow_dark_sw_, blow_alpha_sw_;

  /* ref全体をセットするか否かは、参照画像があるかない(NULL)かで決める */
  const double length_min_, length_max_;
  const double length_bias_;
  const bool length_ref_sw_;

  const double force_min_, force_max_;
  const double force_bias_;
  const bool force_ref_sw_;

  const double density_min_, density_max_;
  const double density_bias_;
  const bool density_ref_sw_;

  double key_lightness_;
  double pixel_key_[igs::image::rgba::siz];
  long table_len_, table_pos_;
  double *table_array_;

  /* copy constructorを無効化 */
  pixel(const pixel &);

  /* 代入演算子を無効化 */
  pixel &operator=(const pixel &);
};
}
}

#endif /* !igs_motion_wind_pixel_h */
