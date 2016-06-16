#include <stdexcept>        /* std::domain_error(-) */
#include "igs_ifx_common.h" /* igs::image::rgba */
//------------------------------------------------------------
namespace {
template <class T>
void change_channel_(T *ima, const int pix_size, const int channels) {
  for (int ii = 0; ii < pix_size; ++ii, ima += channels) {
    ima[0] = ~ima[0];
  }
}
template <class T>
void change_multiplied_rgb(T *ima, T *alp, const int pix_size,
                           const int channels) {
  for (int ii = 0; ii < pix_size; ++ii, ima += channels, alp += channels) {
    if (alp[0] < ima[0]) {
      ima[0] = 0;
    } else {
      ima[0] = alp[0] - ima[0];
    }
  }
}
template <class T>
void change_template_(T *ima, const int hh, const int ww, const int ch,
                      const bool *sw /* each channels switch */
                      ) {
  const int sz = hh * ww;

  if (igs::image::rgba::siz == ch) {
    using namespace igs::image::rgba;
    if (sw[0]) {
      change_multiplied_rgb(&ima[red], &ima[alp], sz, ch);
    }
    if (sw[1]) {
      change_multiplied_rgb(&ima[gre], &ima[alp], sz, ch);
    }
    if (sw[2]) {
      change_multiplied_rgb(&ima[blu], &ima[alp], sz, ch);
    }
    if (sw[3]) {
      change_channel_(&ima[alp], sz, ch);
    }
  } else if (igs::image::rgb::siz == ch) {
    using namespace igs::image::rgb;
    if (sw[0]) {
      change_channel_(&ima[red], sz, ch);
    }
    if (sw[1]) {
      change_channel_(&ima[gre], sz, ch);
    }
    if (sw[2]) {
      change_channel_(&ima[blu], sz, ch);
    }
  } else if (1 == ch) { /* Grayscale */
    if (sw[0]) {
      change_channel_(&ima[0], sz, ch);
    }
  }
}
}
//------------------------------------------------------------
#include <limits>
#include "igs_negate.h"
void igs::negate::change(unsigned char *image_array, const int height,
                         const int width, const int channels, const int bits,
                         const bool *sw_array /* each channel switch  */
                         ) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* bit(monoBW) */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }
  if (std::numeric_limits<unsigned char>::digits == bits) {
    change_template_(image_array, height, width, channels, sw_array);
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    change_template_(reinterpret_cast<unsigned short *>(image_array), height,
                     width, channels, sw_array);
  } else if (1 == bits) {
    const int bi         = width * channels * bits;
    const int dg         = std::numeric_limits<unsigned char>::digits;
    const int sl_bytes   = bi / dg + ((0 != (bi % dg)) ? 1 : 0);
    const int image_size = height * sl_bytes;
    for (int ii = 0; ii < image_size; ++ii) {
      image_array[ii] = ~image_array[ii];
    }
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort/bit");
  }
}
