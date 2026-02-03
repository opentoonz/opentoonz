#pragma once

#if !defined(TOONZ_PLUGIN_UTILITIES_HPP__)
#define TOONZ_PLUGIN_UTILITIES_HPP__
#include "toonz_hostif.h"
#include <tgeometry.h>

namespace plugin {
namespace utils {

inline bool copy_rect(toonz_rect_t *dst, const TRectD &src) {
  dst->x0 = src.x0;
  dst->y0 = src.y0;
  dst->x1 = src.x1;
  dst->y1 = src.y1;
  return true;
}

inline TRectD restore_rect(const toonz_rect_t *src) {
  return TRectD(src->x0, src->y0, src->x1, src->y1);
}

inline bool copy_affine(toonz_affine_t *dst, const TAffine &src) {
  dst->a11 = src.a11;
  dst->a12 = src.a12;
  dst->a13 = src.a13;
  dst->a21 = src.a21;
  dst->a22 = src.a22;
  dst->a23 = src.a23;
  return true;
}

inline TAffine restore_affine(const toonz_affine_t *src) {
  return TAffine(src->a11, src->a12, src->a13, src->a21, src->a22, src->a23);
}
}
}
#endif
