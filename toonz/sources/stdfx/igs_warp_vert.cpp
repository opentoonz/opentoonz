#include <cmath>
#include <vector>
#include <limits> /* std::numeric_limits */
#include "igs_ifx_common.h"
#include "igs_warp.h"

namespace {

void vert_change_(float* image, const int height, const int width,
                  const int channels,
                  const float* refer,  // same as height,width,channels
                  const int refchannels, const int refcc, const double maxlen,
                  const bool alpha_rendering_sw, const bool anti_aliasing_sw) {
  std::vector<std::vector<float>> buf_s1(channels);
  for (int zz = 0; zz < channels; ++zz) {
    buf_s1.at(zz).resize(height);
  }
  std::vector<float> buf_r(height);

  refer += refcc; /* 参照画像の参照色チャンネル */
  for (int xx = 0; xx < width; ++xx, image += channels, refer += refchannels) {
    for (int yy = 0; yy < height; ++yy) {
      for (int zz = 0; zz < channels; ++zz) {
        buf_s1.at(zz).at(yy) = image[yy * width * channels + zz];
      }
    }

    for (int yy = 0; yy < height; ++yy) {  // reference red of refer[]
      float pos    = refer[yy * width * refchannels];
      buf_r.at(yy) = (pos - 0.5f) * maxlen;
    }

    if (anti_aliasing_sw) {
      for (int yy = 0; yy < height; ++yy) {
        double pos = buf_r.at(yy);
        int fl_pos = yy + static_cast<int>(std::floor(pos));
        int ce_pos = yy + static_cast<int>(std::ceil(pos));
        float div  = pos - floor(pos);
        // clamp
        if (fl_pos < 0)
          fl_pos = 0;
        else if (height <= fl_pos)
          fl_pos = height - 1;
        if (ce_pos < 0)
          ce_pos = 0;
        else if (height <= ce_pos)
          ce_pos = height - 1;

        for (int zz = 0; zz < channels; ++zz) {
          if (!alpha_rendering_sw && (igs::image::rgba::alp == zz)) {
            image[yy * width * channels + zz] = buf_s1.at(zz).at(yy);
          } else {
            image[yy * width * channels + zz] =
                buf_s1.at(zz).at(fl_pos) * (1.0 - div) +
                buf_s1.at(zz).at(ce_pos) * div;
          }
        }
      }
    } else {
      for (int yy = 0; yy < height; ++yy) {
        int pos = yy + static_cast<int>(floor(buf_r.at(yy) + 0.5));
        // clamp
        if (pos < 0)
          pos = 0;
        else if (height <= pos)
          pos = height - 1;

        for (int zz = 0; zz < channels; ++zz) {
          if (!alpha_rendering_sw && (igs::image::rgba::alp == zz)) {
            image[yy * width * channels + zz] = buf_s1.at(zz).at(yy);
          } else {
            image[yy * width * channels + zz] = buf_s1.at(zz).at(pos);
          }
        }
      }
    }
  }
}

}  // namespace
//--------------------------------------------------------------------

void igs::warp::vert_change(float* image, const int height, const int width,
                            const int channels,
                            const float* refer,  // by height,width,channels
                            const int refchannels, const int refcc,
                            const double maxlen, const bool alpha_rendering_sw,
                            const bool anti_aliasing_sw) {
  vert_change_(image, height, width, channels, refer, refchannels, refcc,
               maxlen, alpha_rendering_sw, anti_aliasing_sw);
}