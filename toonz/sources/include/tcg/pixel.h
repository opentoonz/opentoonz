#pragma once

#ifndef TCG_PIXEL_H
#define TCG_PIXEL_H

// tcg includes
#include "pixel_ops.h"

// STD includes
#include <limits>

namespace tcg {

//***********************************************************
//    Tcg Pixel Types  definition
//***********************************************************

template <typename Chan, typename pixel_category>
struct Pixel;

template <typename Chan>
struct Pixel<Chan, grayscale_pixel_tag> {
  typedef grayscale_pixel_tag pixel_category;
  typedef Chan channel_type;

public:
  channel_type l;

public:
  Pixel() : l() {}
  Pixel(channel_type _l) : l(_l) {}

  bool operator==(const Pixel &other) const { return l == other.l; }
  bool operator!=(const Pixel &other) const { return l != other.l; }
};

template <typename Chan>
struct Pixel<Chan, rgb_pixel_tag> {
  typedef rgb_pixel_tag pixel_category;
  typedef Chan channel_type;

public:
  channel_type r, g, b;

public:
  Pixel() : r(), g(), b() {}
  Pixel(channel_type _r, channel_type _g, channel_type _b)
      : r(_r), g(_g), b(_b) {}

  bool operator==(const Pixel &other) const {
    return (r == other.r) && (g == other.g) && (g == other.g);
  }
  bool operator!=(const Pixel &other) const {
    return (r != other.r) || (g != other.g) || (g != other.g);
  }
};

template <typename Chan>
struct Pixel<Chan, rgbm_pixel_tag> {
  typedef rgbm_pixel_tag pixel_category;
  typedef Chan channel_type;

public:
  channel_type r, g, b, m;

public:
  Pixel() : r(), g(), b(), m() {}
  Pixel(channel_type _r, channel_type _g, channel_type _b, channel_type _m)
      : r(_r), g(_g), b(_b), m(_m) {}

  bool operator==(const Pixel &other) const {
    return (r == other.r) && (g == other.g) && (g == other.g) && (m == other.m);
  }
  bool operator!=(const Pixel &other) const {
    return (r != other.r) || (g != other.g) || (g != other.g) || (m != other.m);
  }
};

//***********************************************************
//    Tcg Pixel  traits
//***********************************************************

namespace pixel_ops {

template <typename Chan>
struct pixel_traits<Pixel<Chan, grayscale_pixel_tag>, grayscale_pixel_tag> {
  typedef Pixel<Chan, grayscale_pixel_tag> pixel_type;
  typedef grayscale_pixel_tag pixel_category;
  typedef Chan channel_type;

  enum { channels_count = 1 };

  static channel_type max_channel_value() {
    return (std::numeric_limits<channel_type>::max)();
  }

  static channel_type l(const pixel_type &pix) { return pix.l; }
  static channel_type &l(pixel_type &pix) { return pix.l; }
};

template <typename Chan>
struct pixel_traits<Pixel<Chan, rgb_pixel_tag>, rgb_pixel_tag> {
  typedef Pixel<Chan, rgb_pixel_tag> pixel_type;
  typedef rgb_pixel_tag pixel_category;
  typedef Chan channel_type;

  enum { channels_count = 3 };

  static channel_type max_channel_value() {
    return (std::numeric_limits<channel_type>::max)();
  }

  static channel_type r(const pixel_type &pix) { return pix.r; }
  static channel_type &r(pixel_type &pix) { return pix.r; }

  static channel_type g(const pixel_type &pix) { return pix.g; }
  static channel_type &g(pixel_type &pix) { return pix.g; }

  static channel_type b(const pixel_type &pix) { return pix.b; }
  static channel_type &b(pixel_type &pix) { return pix.b; }
};

template <typename Chan>
struct pixel_traits<Pixel<Chan, rgbm_pixel_tag>, rgbm_pixel_tag>
    : public pixel_traits<Pixel<Chan, rgb_pixel_tag>, rgb_pixel_tag> {
  typedef Pixel<Chan, rgbm_pixel_tag> pixel_type;
  typedef rgbm_pixel_tag pixel_category;
  typedef Chan channel_type;

  enum { channels_count = 4 };

  static channel_type m(const pixel_type &pix) { return pix.m; }
  static channel_type &m(pixel_type &pix) { return pix.m; }
};
}
}  // namespace tcg::pixel_ops

#endif  // TCG_PIXEL_H
