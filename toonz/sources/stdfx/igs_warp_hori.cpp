#include <cmath>
#include <vector>
#include <limits> /* std::numeric_limits */
#include "igs_ifx_common.h"
#include "igs_warp.h"

namespace {
void hori_change_(float* image, const int height, const int width,
                  const int channels,
                  const float* refer,  // same as height,width,channels
                  const int refchannels, const int refcc, const double maxlen,
                  const bool alpha_rendering_sw, const bool anti_aliasing_sw) {
  std::vector<std::vector<float>> buf_s(channels);
  for (int zz = 0; zz < channels; ++zz) {
    buf_s.at(zz).resize(width);
  }
  std::vector<double> buf_r(width);

  refer += refcc; /* 参照画像の参照色チャンネル */
  for (int yy = 0; yy < height;
       ++yy, image += channels * width, refer += refchannels * width) {
    // buf_sにSourceのスキャンラインのピクセル値をいれる
    for (int xx = 0; xx < width; ++xx) {
      for (int zz = 0; zz < channels; ++zz) {
        buf_s.at(zz).at(xx) = image[xx * channels + zz];
      }
    }
    // buf_rに参照画像の指定チャンネルの値を入れる
    for (int xx = 0; xx < width; ++xx) {  // reference red of refer[]
      float pos = refer[xx * refchannels];
      // clamp 0.f to 1.f in case using TPixelF
      pos          = std::min(1.f, std::max(0.f, pos));
      buf_r.at(xx) = (pos - 0.5f) * maxlen;
    }

    if (anti_aliasing_sw) {
      for (int xx = 0; xx < width; ++xx) {
        float pos  = buf_r.at(xx);
        int fl_pos = xx + static_cast<int>(std::floor(pos));
        int ce_pos = xx + static_cast<int>(std::ceil(pos));
        float div  = pos - floor(pos);

        // clamp
        if (fl_pos < 0)
          fl_pos = 0;
        else if (width <= fl_pos)
          fl_pos = width - 1;

        if (ce_pos < 0)
          ce_pos = 0;
        else if (width <= ce_pos)
          ce_pos = width - 1;

        for (int zz = 0; zz < channels; ++zz) {
          if (!alpha_rendering_sw && (igs::image::rgba::alp == zz)) {
            image[xx * channels + zz] = buf_s.at(zz).at(xx);
          } else {
            image[xx * channels + zz] = buf_s.at(zz).at(fl_pos) * (1.0 - div) +
                                        buf_s.at(zz).at(ce_pos) * div;
          }
        }
      }
    } else {
      for (int xx = 0; xx < width; ++xx) {
        int pos = xx + static_cast<int>(floor(buf_r.at(xx) + 0.5));
        // clamp
        if (pos < 0)
          pos = 0;
        else if (width <= pos)
          pos = width - 1;

        for (int zz = 0; zz < channels; ++zz) {
          if (!alpha_rendering_sw && (igs::image::rgba::alp == zz)) {
            image[xx * channels + zz] = buf_s.at(zz).at(xx);
          } else {
            image[xx * channels + zz] = buf_s.at(zz).at(pos);
          }
        }
      }
    }
  }
}

}  // namespace
//--------------------------------------------------------------------

void igs::warp::hori_change(float* image, const int height, const int width,
                            const int channels,
                            const float* refer,  // by height,width,channels
                            const int refchannels, const int refcc,
                            const double maxlen, const bool alpha_rendering_sw,
                            const bool anti_aliasing_sw) {
  hori_change_(image, height, width, channels, refer, refchannels, refcc,
               maxlen, alpha_rendering_sw, anti_aliasing_sw);
}