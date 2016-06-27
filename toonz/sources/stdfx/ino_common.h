#pragma once

#ifndef ino_common_h
#define ino_common_h

#include "trop.h"
#include "trasterfx.h"
#include "stdfx.h"

namespace ino {
/* 一時バッファとの変換機能 */
void ras_to_arr(const TRasterP in_ras, const int channels,
                unsigned char *out_arr);
void arr_to_ras(const unsigned char *in_arr, const int channels,
                TRasterP out_ras, const int margin);
void ras_to_vec(const TRasterP ras, const int channels,
                std::vector<unsigned char> &vec);
void vec_to_ras(std::vector<unsigned char> &vec, const int channels,
                TRasterP ras, const int margin = 0);
// void Lx_to_wrap( TRasterP ras );

/* logのserverアクセスON/OFF,install時設定をするための機能 */
/* TEnv::getConfigDir() + "fx_ino_no_log.setup"
        が存在するとtrueを返す */
bool log_enable_sw(void);

/* toonz6.0.x専用の固定値を返すinline(埋め込み)関数 */
inline double param_range(void) { return 1.0; }  // 1 or 100%
inline int channels(void) { return 4; }          // RGBM is 4 channels
inline int bits(const TRasterP ras) {
  return ((TRaster64P)ras) ? (std::numeric_limits<unsigned short>::digits)
                           : (std::numeric_limits<unsigned char>::digits);
}
inline int pixel_bits(const TRasterP ras) {
  return ino::channels() * ino::bits(ras);
}
// inline double pixel_per_mm(void) { return 640. / 12. / 25.4; }
inline double pixel_per_mm(void) { return 1.; }
// inline double pixel_per_inch(void) { return 640. / 12.; }
}

class TBlendForeBackRasterFx : public TRasterFx {
public:
  /* FX nodeが無効のときの、表示port番号 */
  int getPreferredInputPort() override { return 1; }

  std::string getPluginId() const override { return PLUGIN_PREFIX; }
};

#endif /* !ino_common_h */
