#pragma once

#ifndef igs_ifx_common_h
#define igs_ifx_common_h

#ifndef IGS_IFX_COMMON_EXPORT
#define IGS_IFX_COMMON_EXPORT
#endif

#define RGBA_ORDER_OF_TOONZ6

#include <cassert>

namespace igs {
namespace image {
#if defined RGBA_ORDER_OF_TOONZ6
// Toonz6, or GDI, TGA order
/*	Exsample:
        igs::image::rgba::num order = igs::image::rgba::red;
        igs::image::rgb::num order = igs::image::rgba::red;
  */
namespace rgba {
enum num { blu = 0, gre, red, alp, siz };
}
namespace rgb {
enum num { blu = 0, gre, red, siz };
}
#elif defined RGBA_ORDER_OF_OPENGL
// OpenGL order
namespace rgba {
enum num { red = 0, gre, blu, alp, siz };
}
namespace rgb {
enum num { red = 0, gre, blu, siz };
}
#else
Must be define /DRGBA_ORDER_OF_TOONZ6 or /DRGBA_ORDER_OF_OPENGL
#endif

// Direct 3D, Apple Packed Pixel order
// namespace rgba {enum num { alp=0, red, gre, blu, siz }; }

// Direct 3D 10
// namespace rgba {enum num { alp=0, blu, gre, red, siz }; }

template <class T>
void copy_except_margin(const T *in, const int margin, T *out, const int hh,
                        const int ww, const int cc) {
  const T *p_in = in + margin * (ww + margin * 2) * cc + margin * cc;
  T *pout       = out;
  for (int yy = margin; yy < hh + margin; ++yy, p_in += 2 * margin * cc) {
    for (int xx = margin; xx < ww + margin; ++xx, p_in += cc, pout += cc) {
      for (int zz = 0; zz < cc; ++zz) {
        pout[zz] = p_in[zz];
      }
    }
  }
}
}  // namespace image

namespace color {
template <class T>
double ref_value(const T *ref, const int cc, const int ref_max, const int ref_mode) {
  // Prevent divide-by-zero by checking ref_max
  assert(ref_max != 0 && "ref_max must be non-zero to avoid division by zero");
  if (ref_max == 0) {
    return 0.0; // Safe fallback value
  }

  if (igs::image::rgba::siz == cc) {
    using namespace igs::image::rgba;
    switch (ref_mode) {
    case 0:
      return static_cast<double>(ref[red]) / static_cast<double>(ref_max);
    case 1:
      return static_cast<double>(ref[gre]) / static_cast<double>(ref_max);
    case 2:
      return static_cast<double>(ref[blu]) / static_cast<double>(ref_max);
    case 3:
      return static_cast<double>(ref[alp]) / static_cast<double>(ref_max);
    case 4:
      // Luminance (CCIR Rec.601)
      return 0.298912 * static_cast<double>(ref[red]) / static_cast<double>(ref_max) +
             0.586611 * static_cast<double>(ref[gre]) / static_cast<double>(ref_max) +
             0.114478 * static_cast<double>(ref[blu]) / static_cast<double>(ref_max);
    }
  } else if (igs::image::rgb::siz == cc) {
    using namespace igs::image::rgb;
    switch (ref_mode) {
    case 0:
      return static_cast<double>(ref[red]) / static_cast<double>(ref_max);
    case 1:
      return static_cast<double>(ref[gre]) / static_cast<double>(ref_max);
    case 2:
      return static_cast<double>(ref[blu]) / static_cast<double>(ref_max);
    case 3:
      // Luminance (CCIR Rec.601)
      return 0.298912 * static_cast<double>(ref[red]) / static_cast<double>(ref_max) +
             0.586611 * static_cast<double>(ref[gre]) / static_cast<double>(ref_max) +
             0.114478 * static_cast<double>(ref[blu]) / static_cast<double>(ref_max);
    }
  } else if (1 == cc) {
    return static_cast<double>(ref[0]) / static_cast<double>(ref_max);
  }
  return 1.0; // Default return if no conditions match
}
}  // namespace color
}  // namespace igs

#endif /* !igs_ifx_common_h */