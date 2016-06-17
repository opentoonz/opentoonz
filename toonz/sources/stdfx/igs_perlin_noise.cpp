#include <cmath>  // pow()
#include "iwa_noise1234.h"

namespace {
double perlin_noise_3d_(const double x, const double y, const double z,
                        const int octaves_start  // 0<=
                        ,
                        const int octaves_end  // 0<=
                        ,
                        const double persistence  // Not 0
                        // 1/4 or 1/2 or 1/sqrt(3) or 1/sqrt(2) or 1 or ...
                        ) {
  double total = 0;
  Noise1234 pn;
  for (int ii = octaves_start; ii <= octaves_end; ++ii) {
    const double frequency = pow(2.0, ii);  // 1,2,4,8...
    const double amplitude = pow(persistence, ii);
    total += pn.noise(x * frequency, y * frequency, z * frequency) * amplitude;
  }
  return total;
}
double perlin_noise_minmax_(const int octaves_start  // 0<=
                            ,
                            const int octaves_end  // 0<=
                            ,
                            const double persistence  // Not 0
                            // 1/4 or 1/2 or 1/sqrt(3) or 1/sqrt(2) or 1 or ...
                            ) {
  double total = 0;
  for (int ii = octaves_start; ii <= octaves_end; ++ii) {
    total += pow(persistence, ii);
  }
  return total;
}
}
//--------------------------------------------------------------------
#include <stdexcept>        // std::domain_error(-)
#include <limits>           // std::numeric_limits
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_perlin_noise.h"
namespace {
template <class T>
void change_(T *image_array, const int height  // pixel
             ,
             const int width  // pixel
             ,
             const int channels, const bool alpha_rendering_sw,
             const double a11  // geometry of 2D affine transformation
             ,
             const double a12, const double a13, const double a21,
             const double a22, const double a23, const double zz,
             const int octaves_start  // 0<=
             ,
             const int octaves_end  // 0<=
             ,
             const double persistence  // Not 0
             ) {
  const int max_div   = std::numeric_limits<T>::max();
  const int max_div_2 = max_div / 2;
  // 255 / 2    --> 127
  // 65535 / 2  --> 32767
  // const double max_mul = static_cast<double>(max_div_2+0.999999);
  // const double max_off = static_cast<double>(max_div_2+1);
  const double max_mul = static_cast<double>(max_div_2 + 0.499999);
  const double max_off = static_cast<double>(max_div_2 + 1.5);
  /*
  -1 .............. 0 ......... 1
  x127+0.499999
  ------------------------------------------
  -127+0.499999 ... 0 ......... 127+0.499999
  +127+1.5      ... 127+1.5 ... 127+1.5
  ------------------------------------------
  1.000001 ........ 127+1.5 ... 255.999999
  integer
  ------------------------------------------
  1 ............... 128 ....... 255
*/

  const double maxi =
      perlin_noise_minmax_(octaves_start, octaves_end, persistence);

  using namespace igs::image::rgba;
  T *image_crnt = image_array;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx, image_crnt += channels) {
      const T val = static_cast<T>(
          perlin_noise_3d_(xx * a11 + yy * a12 + a13, xx * a21 + yy * a22 + a23,
                           zz, octaves_start, octaves_end, persistence) /
              maxi * max_mul +
          max_off);
      for (int zz = 0; zz < channels; ++zz) {
        if (!alpha_rendering_sw && (alp == zz)) {
          image_crnt[zz] = static_cast<T>(max_div);
        } else {
          image_crnt[zz] = val;
        }
      }
    }
  }
}
}
// #include "igs_geometry2d.h"
void igs::perlin_noise::change(
    unsigned char *image_array, const int height  // pixel
    ,
    const int width  // pixel
    ,
    const int channels, const int bits, const bool alpha_rendering_sw,
    const double a11  // geometry of 2D affine transformation
    ,
    const double a12, const double a13, const double a21, const double a22,
    const double a23, const double zz, const int octaves_start  // 0...
    ,
    const int octaves_end  // 0...
    ,
    const double persistence  // not 0
    ) {
  // igs::geometry2d::affine af(a11 , a12 , a13 , a21 , a22 , a23);
  // igs::geometry2d::translate();

  if (std::numeric_limits<unsigned char>::digits == bits) {
    change_(image_array, height, width, channels, alpha_rendering_sw, a11, a12,
            a13, a21, a22, a23, zz, octaves_start, octaves_end, persistence);
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    change_(reinterpret_cast<unsigned short *>(image_array), height, width,
            channels, alpha_rendering_sw, a11, a12, a13, a21, a22, a23, zz,
            octaves_start, octaves_end, persistence);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
