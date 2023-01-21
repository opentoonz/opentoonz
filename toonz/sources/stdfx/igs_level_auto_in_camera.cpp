#include <cmath>     // pow(-)
#include <stdexcept> /* std::domain_error(-) */
#include <limits>    // std::numeric_limits
#include <vector>
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_level_auto_in_camera.h"
//------------------------------------------------------------
namespace {
double level_value_(double value, double mul_max, bool act_sw, double in_min,
                    double in_max, double out_min, double out_max,
                    double gamma) {
  if (act_sw) {
    if (in_max == in_min) {
      value = in_max;
    } else {
      /* 制限(in_min〜in_max) */
      if (in_min < in_max) {
        if (value < in_min) {
          value = in_min;
        } else if (in_max < value) {
          value = in_max;
        }
      } else {
        if (value < in_max) {
          value = in_max;
        } else if (in_min < value) {
          value = in_min;
        }
      }
      /* 正規化(0〜1) */
      value = (value - in_min) / (in_max - in_min);

      /* Gamma変換 */
      if ((1.0 != gamma) && (0.0 != gamma)) {
        value = pow(value, 1.0 / gamma);
      }
    }
    /* Outputの範囲にスケール変換 */
    value = out_min + value * (out_max - out_min);
    /* 0...1の間で制限かける */
    if (value < 0.0) {
      value = 0.0;
    } else if (1.0 < value) {
      value = 1.0;
    }
  }
  /* 0〜1.0 --> 0〜mul_maxスケール変換し、整数値化 */
  return floor(value * mul_max);
}

//------------------------------------------------------------

struct LevelAutoValueF {
  double in_min[4];
  double out_min[4];
  double gamma[4];
  double in_max_minus_in_min[4];
  double out_max_minus_out_min[4];
  LevelAutoValueF(const int channels, const float *_in_min,
                  const float *_in_max, const double *_in_min_shift,
                  const double *_in_max_shift, const double *_out_min,
                  const double *_out_max, const double *_gamma) {
    for (int c = 0; c < channels; c++) {
      in_min[c]                = _in_min[c] + _in_min_shift[c];
      out_min[c]               = _out_min[c];
      gamma[c]                 = _gamma[c];
      in_max_minus_in_min[c]   = _in_max[c] + _in_max_shift[c] - in_min[c];
      out_max_minus_out_min[c] = _out_max[c] - _out_min[c];
    }
  }

  inline float convert(const int c, float value) {
    if (in_max_minus_in_min[c] == 0.0) {
      value = in_min[c];
    } else  // normalize
      value = (value - in_min[c]) / in_max_minus_in_min[c];
    // gamma transform
    if ((1.0 != gamma[c]) && (0.0 != gamma[c]) && value > 0.0)
      value = std::pow(value, 1.0 / gamma[c]);
    // normalize to the output range
    value = out_min[c] + value * out_max_minus_out_min[c];
    return value;
  }
};

//------------------------------------------------------------
void level_ctable_template_(const unsigned int channels,
                            const bool *act_sw,          // user setting
                            const int *in_min,           // image pixel value
                            const int *in_max,           // image pixel value
                            const double *in_min_shift,  // user setting
                            const double *in_max_shift,  // user setting
                            const double *out_min,       // user setting
                            const double *out_max,       // user setting
                            const double *gamma,         // user setting
                            const unsigned int div_num,
                            std::vector<std::vector<unsigned int>> &table_array
                            /*
                            std::vector< std::vector<T> > &table_array
                     とするとvc2005mdで他プログラムとのリンク時、
                            allocatorの２重定義でエラーとなる。
                            std::vector< std::vector<unsigned int> >&table_array
                     ならOK
                     2009-01-27
                    */
) {
  const double div_val = static_cast<double>(div_num);
  const double mul_val = div_val + 0.999999;
#if defined _WIN32  // vc compile_type
  double in_min_[4], in_max_[4];
#else
  double in_min_[channels], in_max_[channels];
#endif
  for (unsigned int cc = 0; cc < channels; ++cc) {
    in_min_[cc] = in_min_shift[cc] + in_min[cc] / div_val;
    in_max_[cc] = in_max_shift[cc] + in_max[cc] / div_val;
  }
  table_array.resize(channels);
  for (unsigned int cc = 0; cc < channels; ++cc) {
    table_array[cc].resize(div_num + 1);
    for (unsigned int yy = 0; yy <= div_num; ++yy) {
      table_array[cc][yy] = static_cast<unsigned int>(
          level_value_(yy / div_val, mul_val, act_sw[cc], in_min_[cc],
                       in_max_[cc], out_min[cc], out_max[cc], gamma[cc]));
    }
  }
}
//------------------------------------------------------------
template <class T>
void change_template_(T *image_array, const int height, const int width,
                      const int channels

                      ,
                      const bool *act_sw, const double *in_min_shift,
                      const double *in_max_shift, const double *out_min,
                      const double *out_max, const double *gamma

                      ,
                      const int camera_x, const int camera_y,
                      const int camera_w, const int camera_h) {
/* 1.まずcameraエリア内の最大値、最小値を求める */
#if defined _WIN32
  int in_min[4], in_max[4];
#else
  int in_min[channels], in_max[channels];
#endif
  T *image_crnt =
      image_array + camera_y * width * channels + camera_x * channels;
  for (int zz = 0; zz < channels; ++zz) {
    in_min[zz] = in_max[zz] = image_crnt[zz];
  }
  T *image_xx = 0;
  for (int yy = 0; yy < camera_h; ++yy) {
    image_xx = image_crnt;
    image_crnt += width * channels;
    for (int xx = 0; xx < camera_w; ++xx) {
      for (int zz = 0; zz < channels; ++zz) {
        if (image_xx[zz] < in_min[zz]) {
          in_min[zz] = image_xx[zz];
        } else if (in_max[zz] < image_xx[zz]) {
          in_max[zz] = image_xx[zz];
        }
      }
      image_xx += channels;
    }
  }

  /* 2.最大値、最小値から変換テーブルを求める */
  std::vector<std::vector<unsigned int>> table_array;

  level_ctable_template_(channels, act_sw, in_min, in_max, in_min_shift,
                         in_max_shift, out_min, out_max, gamma,
                         std::numeric_limits<T>::max(), table_array);

  /* 3.変換テーブルを使って画像全体をlevel変換する */
  image_crnt        = image_array;
  const int pixsize = height * width;

  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int ii = 0; ii < pixsize; ++ii, image_crnt += channels) {
      image_crnt[red] = static_cast<T>(table_array[0][image_crnt[red]]);
      image_crnt[gre] = static_cast<T>(table_array[1][image_crnt[gre]]);
      image_crnt[blu] = static_cast<T>(table_array[2][image_crnt[blu]]);
      image_crnt[alp] = static_cast<T>(table_array[3][image_crnt[alp]]);
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int ii = 0; ii < pixsize; ++ii, image_crnt += channels) {
      image_crnt[red] = static_cast<T>(table_array[0][image_crnt[red]]);
      image_crnt[gre] = static_cast<T>(table_array[1][image_crnt[gre]]);
      image_crnt[blu] = static_cast<T>(table_array[2][image_crnt[blu]]);
    }
  } else if (1 == channels) { /* grayscale */
    for (int ii = 0; ii < pixsize; ++ii, ++image_crnt) {
      image_crnt[0] = static_cast<T>(table_array[0][image_crnt[0]]);
    }
  }

  table_array.clear();
}

//------------------------------------------------------------

template <>
void change_template_<float>(float *image_array, const int height,
                             const int width, const int channels,
                             const bool *act_sw, const double *in_min_shift,
                             const double *in_max_shift, const double *out_min,
                             const double *out_max, const double *gamma,
                             const int camera_x, const int camera_y,
                             const int camera_w, const int camera_h) {
  /* 1.まずcameraエリア内の最大値、最小値を求める */
#if defined _WIN32
  float in_min[4], in_max[4];
#else
  float in_min[channels], in_max[channels];
#endif
  float *image_crnt =
      image_array + camera_y * width * channels + camera_x * channels;
  for (int zz = 0; zz < channels; ++zz) {
    in_min[zz] = in_max[zz] = image_crnt[zz];
  }
  float *image_xx = nullptr;
  for (int yy = 0; yy < camera_h; ++yy) {
    image_xx = image_crnt;
    image_crnt += width * channels;
    for (int xx = 0; xx < camera_w; ++xx) {
      for (int zz = 0; zz < channels; ++zz) {
        if (image_xx[zz] < in_min[zz]) {
          in_min[zz] = image_xx[zz];
        } else if (in_max[zz] < image_xx[zz]) {
          in_max[zz] = image_xx[zz];
        }
      }
      image_xx += channels;
    }
  }

  /* 2.最大値、最小値から変換テーブルを求める */
  // std::vector<std::vector<unsigned int>> table_array;
  //
  // level_ctable_template_(channels, act_sw, in_min, in_max, in_min_shift,
  //   in_max_shift, out_min, out_max, gamma,
  //   std::numeric_limits<T>::max(), table_array);

  LevelAutoValueF converter(channels, in_min, in_max, in_min_shift,
                            in_max_shift, out_min, out_max, gamma);

  /* 画像全体をlevel変換する */
  image_crnt        = image_array;
  const int pixsize = height * width;

  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int ii = 0; ii < pixsize; ++ii, image_crnt += channels) {
      image_crnt[red] = converter.convert(0, image_crnt[red]);
      image_crnt[gre] = converter.convert(1, image_crnt[gre]);
      image_crnt[blu] = converter.convert(2, image_crnt[blu]);
      image_crnt[alp] = converter.convert(3, image_crnt[alp]);
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int ii = 0; ii < pixsize; ++ii, image_crnt += channels) {
      image_crnt[red] = converter.convert(0, image_crnt[red]);
      image_crnt[gre] = converter.convert(1, image_crnt[gre]);
      image_crnt[blu] = converter.convert(2, image_crnt[blu]);
    }
  } else if (1 == channels) { /* grayscale */
    for (int ii = 0; ii < pixsize; ++ii, ++image_crnt) {
      image_crnt[0] = converter.convert(0, image_crnt[0]);
    }
  }
}
}  // namespace

void igs::level_auto_in_camera::change(
    void *image_array, const int height, const int width, const int channels,
    const int bits,
    const bool *act_sw,          // channels array
    const double *in_min_shift,  // channels array
    const double *in_max_shift,  // channels array
    const double *out_min,       // channels array
    const double *out_max,       // channels array
    const double *gamma,         // channels array
    const int camera_x, const int camera_y, const int camera_w,
    const int camera_h) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
  ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  if (std::numeric_limits<unsigned char>::digits == bits) {
    change_template_(static_cast<unsigned char *>(image_array), height, width,
                     channels, act_sw, in_min_shift, in_max_shift, out_min,
                     out_max, gamma, camera_x, camera_y, camera_w, camera_h);
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    change_template_(static_cast<unsigned short *>(image_array), height, width,
                     channels, act_sw, in_min_shift, in_max_shift, out_min,
                     out_max, gamma, camera_x, camera_y, camera_w, camera_h);
  } else if (std::numeric_limits<float>::digits == bits) {
    change_template_(static_cast<float *>(image_array), height, width, channels,
                     act_sw, in_min_shift, in_max_shift, out_min, out_max,
                     gamma, camera_x, camera_y, camera_w, camera_h);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
