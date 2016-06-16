#pragma once

#ifndef TCG_PIXEL_OPS_H
#define TCG_PIXEL_OPS_H

// tcg includes
#include "base.h"

namespace tcg {

//**************************************************************************
//    Pixel Categories
//**************************************************************************

struct grayscale_pixel_tag {};
struct rgb_pixel_tag {};
struct rgbm_pixel_tag : public rgb_pixel_tag {};
struct indexed_pixel_tag {};

//**************************************************************************
//    Pixel Traits Types
//**************************************************************************

template <typename Pix, typename pixel_category = typename Pix::pixel_category>
struct pixel_traits_types;

template <typename Pix>
struct pixel_traits_types<Pix, grayscale_pixel_tag> {
  typedef Pix pixel_type;
  typedef grayscale_pixel_tag pixel_category;
  typedef typename Pix::channel_type channel_type;

  enum { channels_count = 1 };
};

template <typename Pix>
struct pixel_traits_types<Pix, rgb_pixel_tag> {
  typedef Pix pixel_type;
  typedef rgb_pixel_tag pixel_category;
  typedef typename Pix::channel_type channel_type;

  enum { channels_count = 3 };
};

template <typename Pix>
struct pixel_traits_types<Pix, rgbm_pixel_tag>
    : public pixel_traits_types<Pix, rgb_pixel_tag> {
  typedef Pix pixel_type;
  typedef rgbm_pixel_tag pixel_category;
  typedef typename Pix::channel_type channel_type;

  enum { channels_count = 4 };
};

template <typename Pix>
struct pixel_traits_types<Pix, indexed_pixel_tag> {
  typedef Pix pixel_type;
  typedef indexed_pixel_tag pixel_category;
  typedef typename Pix::index_type index_type;
};

//**************************************************************************
//    Pixel Traits
//**************************************************************************

template <typename Pix, typename pixel_category = typename Pix::pixel_category>
struct pixel_traits;

template <typename Pix>
struct pixel_traits<Pix, grayscale_pixel_tag>
    : public pixel_traits_types<Pix, grayscale_pixel_tag> {
  typedef pixel_traits_types<Pix, grayscale_pixel_tag> tr;

public:
  static typename tr::channel_type max_channel_value();

  static typename tr::channel_type l(const typename tr::pixel_type &pix);
  static typename tr::channel_type &l(typename tr::pixel_type &pix);
};

template <typename Pix>
struct pixel_traits<Pix, rgb_pixel_tag> {
  typedef pixel_traits_types<Pix, rgb_pixel_tag> tr;

public:
  static typename tr::channel_type max_channel_value();

  static typename tr::channel_type r(const typename tr::pixel_type &pix);
  static typename tr::channel_type &r(typename tr::pixel_type &pix);

  static typename tr::channel_type g(const typename tr::pixel_type &pix);
  static typename tr::channel_type &g(typename tr::pixel_type &pix);

  static typename tr::channel_type b(const typename tr::pixel_type &pix);
  static typename tr::channel_type &b(typename tr::pixel_type &pix);
};

template <typename Pix>
struct pixel_traits<Pix, rgbm_pixel_tag>
    : public pixel_traits<Pix, rgb_pixel_tag> {
  typedef pixel_traits_types<Pix, rgbm_pixel_tag> tr;

public:
  static typename tr::channel_type m(const typename tr::pixel_type &pix);
  static typename tr::channel_type &m(typename tr::pixel_type &pix);
};

template <typename Pix>
struct pixel_traits<Pix, indexed_pixel_tag> {
  typedef pixel_traits_types<Pix, indexed_pixel_tag> tr;
  typedef typename tr::pixel_type pixel_type;
  typedef typename tr::index_type index_type;

public:
  static typename tr::index_type index(const typename tr::pixel_type &pix);
  static typename tr::index_type &index(typename tr::pixel_type &pix);
};

namespace pixel_ops {

//**************************************************************************
//    Pixel Functions
//**************************************************************************

template <typename PixIn, typename PixOut>
inline PixOut _cast(const PixIn &pix, grayscale_pixel_tag) {
  return PixOut(pixel_traits<PixIn>::l(pix));
}

template <typename PixIn, typename PixOut>
inline PixOut _cast(const PixIn &pix, rgb_pixel_tag) {
  return PixOut(pixel_traits<PixIn>::r(pix), pixel_traits<PixIn>::g(pix),
                pixel_traits<PixIn>::b(pix));
}

template <typename PixIn, typename PixOut>
inline PixOut _cast(const PixIn &pix, rgbm_pixel_tag) {
  return PixOut(pixel_traits<PixIn>::r(pix), pixel_traits<PixIn>::g(pix),
                pixel_traits<PixIn>::b(pix), pixel_traits<PixIn>::m(pix));
}

template <typename PixIn, typename PixOut>
inline PixOut cast(const PixIn &pix) {
  return _cast<PixIn, PixOut>(pix,
                              typename pixel_traits<PixIn>::pixel_category());
}

//------------------------------------------------------------------

template <typename PixIn, typename PixOut>
inline void _assign(PixOut &pixout, const PixIn &pixin, grayscale_pixel_tag) {
  pixel_traits<PixOut>::l(pixout) = pixel_traits<PixIn>::l(pixin);
}

template <typename PixIn, typename PixOut>
inline void _assign(PixOut &pixout, const PixIn &pixin, rgb_pixel_tag) {
  pixel_traits<PixOut>::r(pixout) = pixel_traits<PixIn>::r(pixin);
  pixel_traits<PixOut>::g(pixout) = pixel_traits<PixIn>::g(pixin);
  pixel_traits<PixOut>::b(pixout) = pixel_traits<PixIn>::b(pixin);
}

template <typename PixIn, typename PixOut>
inline void _assign(PixOut &pixout, const PixIn &pixin, rgbm_pixel_tag) {
  pixel_traits<PixOut>::r(pixout) = pixel_traits<PixIn>::r(pixin);
  pixel_traits<PixOut>::g(pixout) = pixel_traits<PixIn>::g(pixin);
  pixel_traits<PixOut>::b(pixout) = pixel_traits<PixIn>::b(pixin);
  pixel_traits<PixOut>::m(pixout) = pixel_traits<PixIn>::m(pixin);
}

template <typename PixIn, typename PixOut>
inline void assign(PixOut &pixout, const PixIn &pixin) {
  _assign<PixIn, PixOut>(pixout, pixin,
                         typename pixel_traits<PixIn>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix1, typename Pix2>
inline Pix1 _sum(const Pix1 &a, const Pix2 &b, grayscale_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::l(a) + pixel_traits<Pix2>::l(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 _sum(const Pix1 &a, const Pix2 &b, rgb_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::r(a) + pixel_traits<Pix2>::r(b),
              pixel_traits<Pix1>::g(a) + pixel_traits<Pix2>::g(b),
              pixel_traits<Pix1>::b(a) + pixel_traits<Pix2>::b(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 _sum(const Pix1 &a, const Pix2 &b, rgbm_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::r(a) + pixel_traits<Pix2>::r(b),
              pixel_traits<Pix1>::g(a) + pixel_traits<Pix2>::g(b),
              pixel_traits<Pix1>::b(a) + pixel_traits<Pix2>::b(b),
              pixel_traits<Pix1>::m(a) + pixel_traits<Pix2>::m(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 operator+(const Pix1 &a, const Pix2 &b) {
  return _sum(a, b, typename pixel_traits<Pix2>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix1, typename Pix2>
inline Pix1 _sub(const Pix1 &a, const Pix2 &b, grayscale_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::l(a) - pixel_traits<Pix2>::l(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 _sub(const Pix1 &a, const Pix2 &b, rgb_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::r(a) - pixel_traits<Pix2>::r(b),
              pixel_traits<Pix1>::g(a) - pixel_traits<Pix2>::g(b),
              pixel_traits<Pix1>::b(a) - pixel_traits<Pix2>::b(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 _sub(const Pix1 &a, const Pix2 &b, rgbm_pixel_tag) {
  return Pix1(pixel_traits<Pix1>::r(a) - pixel_traits<Pix2>::r(b),
              pixel_traits<Pix1>::g(a) - pixel_traits<Pix2>::g(b),
              pixel_traits<Pix1>::b(a) - pixel_traits<Pix2>::b(b),
              pixel_traits<Pix1>::m(a) - pixel_traits<Pix2>::m(b));
}

template <typename Pix1, typename Pix2>
inline Pix1 operator-(const Pix1 &a, const Pix2 &b) {
  return _sub(a, b, typename pixel_traits<Pix2>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix _mult(const Pix &a, Scalar k, grayscale_pixel_tag) {
  return Pix(pixel_traits<Pix>::l(a) * k);
}

template <typename Pix, typename Scalar>
inline Pix _mult(const Pix &a, Scalar k, rgb_pixel_tag) {
  return Pix(pixel_traits<Pix>::r(a) * k, pixel_traits<Pix>::g(a) * k,
             pixel_traits<Pix>::b(a) * k);
}

template <typename Pix, typename Scalar>
inline Pix _mult(const Pix &a, Scalar k, rgbm_pixel_tag) {
  return Pix(pixel_traits<Pix>::r(a) * k, pixel_traits<Pix>::g(a) * k,
             pixel_traits<Pix>::b(a) * k, pixel_traits<Pix>::m(a) * k);
}

template <typename Pix, typename Scalar>
inline Pix operator*(Scalar k, const Pix &pix) {
  return _mult(pix, k, typename pixel_traits<Pix>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix _div(const Pix &a, Scalar k, grayscale_pixel_tag) {
  return Pix(pixel_traits<Pix>::l(a) / k);
}

template <typename Pix, typename Scalar>
inline Pix _div(const Pix &a, Scalar k, rgb_pixel_tag) {
  return Pix(pixel_traits<Pix>::r(a) / k, pixel_traits<Pix>::g(a) / k,
             pixel_traits<Pix>::b(a) / k);
}

template <typename Pix, typename Scalar>
inline Pix _div(const Pix &a, Scalar k, rgbm_pixel_tag) {
  return Pix(pixel_traits<Pix>::r(a) / k, pixel_traits<Pix>::g(a) / k,
             pixel_traits<Pix>::b(a) / k, pixel_traits<Pix>::m(a) / k);
}

template <typename Pix, typename Scalar>
inline Pix operator/(const Pix &pix, Scalar k) {
  return _div(pix, k, typename pixel_traits<Pix>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline void premultiply(Pix &pix, Scalar = 0) {
  Scalar factor =
      pixel_traits<Pix>::m(pix) / Scalar(pixel_traits<Pix>::max_channel_value);

  pixel_traits<Pix>::r(pix) = pixel_traits<Pix>::r(pix) * factor;
  pixel_traits<Pix>::g(pix) = pixel_traits<Pix>::g(pix) * factor;
  pixel_traits<Pix>::b(pix) = pixel_traits<Pix>::b(pix) * factor;
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline void depremultiply(Pix &pix, Scalar = 0) {
  if (pixel_traits<Pix>::channel_type m = pixel_traits<Pix>::m(pix)) {
    Scalar factor = pixel_traits<Pix>::max_channel_value / Scalar(m);

    pixel_traits<Pix>::r(pix) = pixel_traits<Pix>::r(pix) * factor;
    pixel_traits<Pix>::g(pix) = pixel_traits<Pix>::g(pix) * factor;
    pixel_traits<Pix>::b(pix) = pixel_traits<Pix>::b(pix) * factor;
  }
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix _blend(const Pix &p0, const Pix &p1, Scalar t, grayscale_pixel_tag) {
  return Pix((1 - t) * pixel_traits<Pix>::l(p0) + t * pixel_traits<Pix>::l(p1));
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix _blend(const Pix &p0, const Pix &p1, Scalar t, rgb_pixel_tag) {
  Scalar one_t = 1 - t;
  return Pix(one_t * pixel_traits<Pix>::r(p0) + t * pixel_traits<Pix>::r(p1),
             one_t * pixel_traits<Pix>::g(p0) + t * pixel_traits<Pix>::g(p1),
             one_t * pixel_traits<Pix>::m(p0) + t * pixel_traits<Pix>::b(p1));
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix _blend(const Pix &p0, const Pix &p1, Scalar t, rgbm_pixel_tag) {
  Scalar one_t = 1 - t;
  return Pix(one_t * pixel_traits<Pix>::r(p0) + t * pixel_traits<Pix>::r(p1),
             one_t * pixel_traits<Pix>::g(p0) + t * pixel_traits<Pix>::g(p1),
             one_t * pixel_traits<Pix>::b(p0) + t * pixel_traits<Pix>::b(p1),
             one_t * pixel_traits<Pix>::m(p0) + t * pixel_traits<Pix>::m(p1));
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline Pix blend(const Pix &p0, const Pix &p1, Scalar t) {
  return _blend(p0, p1, t, pixel_traits<Pix>::pixel_category());
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline void over_premult(Pix &down, const Pix &up, Scalar = 0) {
  Scalar t =
      (1 -
       pixel_traits<Pix>::m(up) / Scalar(pixel_traits<Pix>::max_channel_value));

  pixel_traits<Pix>::r(down) =
      pixel_traits<Pix>::r(up) + t * pixel_traits<Pix>::r(down);
  pixel_traits<Pix>::g(down) =
      pixel_traits<Pix>::g(up) + t * pixel_traits<Pix>::g(down);
  pixel_traits<Pix>::b(down) =
      pixel_traits<Pix>::b(up) + t * pixel_traits<Pix>::b(down);
  pixel_traits<Pix>::m(down) =
      pixel_traits<Pix>::m(up) + t * pixel_traits<Pix>::m(down);
}

//------------------------------------------------------------------

template <typename Pix, typename Scalar>
inline void over(Pix &down, const Pix &up, Scalar = 0) {
  Scalar t = (1 -
              pixel_traits<Pix>::m(up) /
                  Scalar(pixel_traits<Pix>::max_channel_value)) *
             pixel_traits<Pix>::m(down);
  Scalar m = pixel_traits<Pix>::m(up) + t;

  Scalar up_fac = pixel_traits<Pix>::m(up) / m;
  Scalar dn_fac = t / m;

  pixel_traits<Pix>::r(down) =
      up_fac * pixel_traits<Pix>::r(up) + dn_fac * pixel_traits<Pix>::r(down);
  pixel_traits<Pix>::g(down) =
      up_fac * pixel_traits<Pix>::g(up) + dn_fac * pixel_traits<Pix>::g(down);
  pixel_traits<Pix>::b(down) =
      up_fac * pixel_traits<Pix>::b(up) + dn_fac * pixel_traits<Pix>::b(down);
  pixel_traits<Pix>::m(down) = m;
}
}
}  // namespace tcg::pixel_ops

#endif  // TCG_PIXEL_OPS_H
