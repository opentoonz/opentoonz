#pragma once

#ifndef T_PIXELUTILS_INCLUDED
#define T_PIXELUTILS_INCLUDED

#include "tpixel.h"
#include "tpixelgr.h"

#undef DVAPI
#undef DVVAR
#ifdef TCOLOR_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------------------------------------
/*! this template function computes a linear interpolation between
    the color \b a and \b b according to the parameter \b t.
    If \b t = 0, it returns \b a;
    if \b t = 1, it returns \b b;
    No check is performed.
*/
template <class T>
inline T blend(const T &a, const T &b, double t) {
  return T(troundp((1 - t) * a.r + t * b.r), troundp((1 - t) * a.g + t * b.g),
           troundp((1 - t) * a.b + t * b.b), troundp((1 - t) * a.m + t * b.m));
}

//-----------------------------------------------------------------------------

/*! this template function computes a linear interpolation between
    the color \b a and \b b according to the ratio between \b num and \b den.
    If \b num / \b den = 0, it returns \b a;
    if \b num / \b den = 1, it returns \b b;
    No check is performed.
    \warning \b b MUST be not zero.
*/
template <class T>
inline T blend(const T &a, const T &b, int num, int den) {
  return T((int)(((den - num) * a.r + num * b.r) / den),
           (int)(((den - num) * a.g + num * b.g) / den),
           (int)(((den - num) * a.b + num * b.b) / den),
           (int)(((den - num) * a.m + num * b.m) / den));
}

template <class T>
inline T antialias(const T &a, int num) {
  return T((int)((num * a.r) / 255), (int)((num * a.g) / 255),
           (int)((num * a.b) / 255), (int)((num * a.m) / 255));
}

//-----------------------------------------------------------------------------

/*! this function combines two pixels according to their alpha channel.
    If the \b top pixel is completely opaque the function returns it.
    If the \b top pixel is completely transparent the function returns \b bot.
    In the other cases a blend is performed.
*/

template <class T, class Q>
inline T overPixT(const T &bot, const T &top) {
  UINT max = T::maxChannelValue;

  if (top.m == max) return top;

  if (top.m == 0) return bot;

  TUINT32 r = top.r + bot.r * (max - top.m) / max;
  TUINT32 g = top.g + bot.g * (max - top.m) / max;
  TUINT32 b = top.b + bot.b * (max - top.m) / max;
  return T((r < max) ? (Q)r : (Q)max, (g < max) ? (Q)g : (Q)max,
           (b < max) ? (Q)b : (Q)max,
           (bot.m == max) ? max : max - (max - bot.m) * (max - top.m) / max);
}

//-----------------------------------------------------------------------------
template <class T, class S, class Q>
inline T overPixGRT(const T &bot, const S &top) {
  UINT max = T::maxChannelValue;

  if (top.value == max) return T(top.value, top.value, top.value, top.value);

  if (top.value == 0) return bot;

  double aux = (max - top.value) / max;
  TUINT32 r  = (TUINT32)(top.value + bot.r * aux);
  TUINT32 g  = (TUINT32)(top.value + bot.g * aux);
  TUINT32 b  = (TUINT32)(top.value + bot.b * aux);
  return T((r < max) ? (Q)r : (Q)max, (g < max) ? (Q)g : (Q)max,
           (b < max) ? (Q)b : (Q)max,
           (bot.m == max) ? max : (TUINT32)(max - (max - bot.m) * aux));
}

//-----------------------------------------------------------------------------
// as the other, but without if's. it's quicker if you know for sure that top.m
// is not 0 or 255.
template <class T, class Q>
inline T quickOverPixT(const T &bot, const T &top) {
  UINT max = T::maxChannelValue;

  TUINT32 r = top.r + bot.r * (max - top.m) / max;
  TUINT32 g = top.g + bot.g * (max - top.m) / max;
  TUINT32 b = top.b + bot.b * (max - top.m) / max;
  return T((r < max) ? (Q)r : (Q)max, (g < max) ? (Q)g : (Q)max,
           (b < max) ? (Q)b : (Q)max,
           (bot.m == max) ? max : max - (max - bot.m) * (max - top.m) / max);
}

//------------------------------------------------------------------------------------

template <class T, class Q>
inline T quickOverPixPremultT(const T &bot, const T &top) {
  UINT max = T::maxChannelValue;

  TUINT32 r = (top.r * top.m + bot.r * (max - top.m)) / max;
  TUINT32 g = (top.g * top.m + bot.g * (max - top.m)) / max;
  TUINT32 b = (top.b * top.m + bot.b * (max - top.m)) / max;
  return T((r < max) ? (Q)r : (Q)max, (g < max) ? (Q)g : (Q)max,
           (b < max) ? (Q)b : (Q)max,
           (bot.m == max) ? max : max - (max - bot.m) * (max - top.m) / max);
}
//------------------------------------------------------------------------------------
/*-- Show raster images darken-blended on the viewer --*/
/* references from ino_blend_darken.cpp */
template <class T, class Q>
inline T quickOverPixDarkenBlendedT(const T &bot, const T &top) {
  struct locals {
    static inline double comp(const double ch_a, const double ch_b,
                              const double alpha) {
      return clamp(ch_b + ch_a * (1.0 - alpha));
    }
    static inline double darken_ch(const double dn, const double dn_a,
                                   const double up, const double up_a) {
      return (up / up_a < dn / dn_a) ? comp(dn, up, up_a) : comp(up, dn, dn_a);
    }
    static inline double clamp(double val) {
      return (val < 0.0) ? 0.0 : (val > 1.0) ? 1.0 : val;
    }
  };  // locals

  if (bot.m == 0) return top;

  if (top.m == T::maxChannelValue && bot.m == T::maxChannelValue) {
    TUINT32 r = (top.r < bot.r) ? top.r : bot.r;
    TUINT32 g = (top.g < bot.g) ? top.g : bot.g;
    TUINT32 b = (top.b < bot.b) ? top.b : bot.b;
    return T((Q)r, (Q)g, (Q)b, T::maxChannelValue);
  }

  double maxi = static_cast<double>(T::maxChannelValue);  // 255or65535

  double upr = static_cast<double>(top.r) / maxi;
  double upg = static_cast<double>(top.g) / maxi;
  double upb = static_cast<double>(top.b) / maxi;
  double upa = static_cast<double>(top.m) / maxi;
  double dnr = static_cast<double>(bot.r) / maxi;
  double dng = static_cast<double>(bot.g) / maxi;
  double dnb = static_cast<double>(bot.b) / maxi;
  double dna = static_cast<double>(bot.m) / maxi;
  dnr        = locals::darken_ch(dnr, dna, upr, upa);
  dng        = locals::darken_ch(dng, dna, upg, upa);
  dnb        = locals::darken_ch(dnb, dna, upb, upa);
  dna        = locals::comp(dna, upa, upa);
  T out;
  out.r = static_cast<Q>(dnr * (maxi + 0.999999));
  out.g = static_cast<Q>(dng * (maxi + 0.999999));
  out.b = static_cast<Q>(dnb * (maxi + 0.999999));
  out.m = static_cast<Q>(dna * (maxi + 0.999999));
  return out;
}

//-----------------------------------------------------------------------------
template <class T, class S, class Q>
inline T quickOverPixGRT(const T &bot, const S &top) {
  UINT max = T::maxChannelValue;

  double aux = (max - top.value) / max;
  TUINT32 r  = (TUINT32)(top.value + bot.r * aux);
  TUINT32 g  = (TUINT32)(top.value + bot.g * aux);
  TUINT32 b  = (TUINT32)(top.value + bot.b * aux);
  return T((r < max) ? (Q)r : (Q)max, (g < max) ? (Q)g : (Q)max,
           (b < max) ? (Q)b : (Q)max,
           (bot.m == max) ? max : (TUINT32)(max - (max - bot.m) * aux));
}

//-----------------------------------------------------------------------------

inline TPixel32 overPix(const TPixel32 &bot, const TPixelGR8 &top) {
  return overPixGRT<TPixel32, TPixelGR8, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel64 overPix(const TPixel64 &bot, const TPixelGR16 &top) {
  return overPixGRT<TPixel64, TPixelGR16, USHORT>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel32 overPix(const TPixel32 &bot, const TPixel32 &top) {
  return overPixT<TPixel32, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel64 overPix(const TPixel64 &bot, const TPixel64 &top) {
  return overPixT<TPixel64, USHORT>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel32 quickOverPix(const TPixel32 &bot, const TPixelGR8 &top) {
  return quickOverPixGRT<TPixel32, TPixelGR8, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel64 quickOverPix(const TPixel64 &bot, const TPixelGR16 &top) {
  return quickOverPixGRT<TPixel64, TPixelGR16, USHORT>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel32 quickOverPix(const TPixel32 &bot, const TPixel32 &top) {
  return quickOverPixT<TPixel32, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel32 quickOverPixPremult(const TPixel32 &bot, const TPixel32 &top) {
  return quickOverPixPremultT<TPixel32, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

inline TPixel64 quickOverPix(const TPixel64 &bot, const TPixel64 &top) {
  return quickOverPixT<TPixel64, USHORT>(bot, top);
}

//------------------------------------------------------------------------------------

inline TPixel32 quickOverPixDarkenBlended(const TPixel32 &bot,
                                          const TPixel32 &top) {
  return quickOverPixDarkenBlendedT<TPixel32, UCHAR>(bot, top);
}

//-----------------------------------------------------------------------------

template <class T, class Q>
inline void overPix(T &outPix, const T &bot, const T &top) {
  UINT max = T::maxChannelValue;

  if (top.m == max)
    outPix = top;
  else if (top.m == 0)
    outPix = bot;
  else {
    TUINT32 r = top.r + bot.r * (max - top.m) / max;
    TUINT32 g = top.g + bot.g * (max - top.m) / max;
    TUINT32 b = top.b + bot.b * (max - top.m) / max;
    outPix.r = (r < max) ? (Q)r : (Q)max, outPix.g = (g < max) ? (Q)g : (Q)max,
    outPix.b = (b < max) ? (Q)b : (Q)max,
    outPix.m = (bot.m == max) ? max : max - (max - bot.m) * (max - top.m) / max;
  }
}

//-----------------------------------------------------------------------------

inline TPixel32 overPixOnWhite(const TPixel32 &top) {
  UINT max = TPixel32::maxChannelValue;

  if (top.m == max)
    return top;
  else if (top.m == 0)
    return TPixel32::White;
  else
    return TPixel32(top.r + max - top.m, top.g + max - top.m,
                    top.b + max - top.m, max);
}

inline TPixel32 overPixOnBlack(const TPixel32 &top) {
  UINT max = TPixel32::maxChannelValue;

  if (top.m == max) return top;

  if (top.m == 0) return TPixel32::Black;

  return TPixel32(top.r, top.g, top.b, max);
}

//-----------------------------------------------------------------------------

/*! this function combines two GR8 pixels returning the darker.
*/

inline TPixelGR8 over(const TPixelGR8 &bot, const TPixelGR8 &top) {
  return TPixelGR8(std::min(bot.value, top.value));
}

//-----------------------------------------------------------------------------

/*! this function premultiply a not-premultiplied pixel.
    \note
     Premultiplied alpha is a term used to describe a source color,
     the components of which have already been multiplied by an alpha value.
     Premultiplied alpha is just a different way of representing alphified
   pixels.
     If the separate alpha pixel is (r, g, b, a), then the premultiplied alpha
   pixel is
     (ar, ag, ab, a).
     Premultiplying speeds up the rendering of the image by eliminating an extra
     multiplication operation per color component.
     For example, in an RGB color space, rendering the image with premultiplied
   alpha
     eliminates three multiplication operations (red times alpha, green times
   alpha,
     and blue times alpha) for each pixel in the image.

     (Without premultiplication, the calculation to composite an image w/alpha
   into a comp is:
         dest = pix1 * alpha1 + (1 - alpha1) * pix2 * alpha2

     If both images' alphas are premultiplied, this gets reduced to:
         dest = pix1 + (1 - alpha1) * pix2

*/

//-----------------------------------------------------------------------------

inline void premult(TPixel32 &pix) {
  const int MAGICFAC = (257U * 256U + 1U);
  UINT fac           = MAGICFAC * pix.m;

  pix.r = (UINT)(pix.r * fac + (1U << 23)) >> 24;
  pix.g = (UINT)(pix.g * fac + (1U << 23)) >> 24;
  pix.b = (UINT)(pix.b * fac + (1U << 23)) >> 24;
}

inline void premult(TPixel64 &pix) {
  pix.r = pix.r * pix.m / 65535.0;
  pix.g = pix.g * pix.m / 65535.0;
  pix.b = pix.b * pix.m / 65535.0;
}

inline void depremult(TPixel32 &pix) {
  float fac = 255.0f / pix.m;
  pix.r     = std::min(pix.r * fac, 255.0f);
  pix.g     = std::min(pix.g * fac, 255.0f);
  pix.b     = std::min(pix.b * fac, 255.0f);
}

inline void depremult(TPixel64 &pix) {
  double fac = 65535.0 / pix.m;
  pix.r      = std::min(pix.r * fac, 65535.0);
  pix.g      = std::min(pix.g * fac, 65535.0);
  pix.b      = std::min(pix.b * fac, 65535.0);
}

//-----------------------------------------------------------------------------

template <typename Chan>
const double *premultiplyTable();

template <typename Chan>
const double *depremultiplyTable();

//-----------------------------------------------------------------------------

inline TPixel32 premultiply(const TPixel32 &pix) {
  const int MAGICFAC = (257U * 256U + 1U);
  UINT fac           = MAGICFAC * pix.m;

  return TPixel32(((UINT)(pix.r * fac + (1U << 23)) >> 24),
                  ((UINT)(pix.g * fac + (1U << 23)) >> 24),
                  ((UINT)(pix.b * fac + (1U << 23)) >> 24), pix.m);
}

inline TPixel64 premultiply(const TPixel64 &pix) {
  return TPixel64(pix.r * pix.m / 65535.0, pix.g * pix.m / 65535.0,
                  pix.b * pix.m / 65535.0, pix.m);
}

inline TPixel32 depremultiply(const TPixel32 &pix) {
  return TPixel32(pix.r * 255.0 / pix.m, pix.g * 255.0 / pix.m,
                  pix.b * 255.0 / pix.m, pix.m);
}

inline TPixel64 depremultiply(const TPixel64 &pix) {
  return TPixel64(pix.r * 65535.0 / pix.m, pix.g * 65535.0 / pix.m,
                  pix.b * 65535.0 / pix.m, pix.m);
}

//-----------------------------------------------------------------------------

//! onversion between RGB and HSV colorspace
DVAPI void hsv2rgb(TPixel32 &dstRgb, int srcHsv[3], int maxHsv = 255);

//-------------------------------------------------------------------

/*!
  IN : h in [0..360], s and v in [0..1]
  OUT: r,g,b in [0..1]
*/
DVAPI void HSV2RGB(double hue, double sat, double value, double *red,
                   double *green, double *blue);

//-----------------------------------------------------------------------------

DVAPI void rgb2hsv(int dstHsv[3], const TPixel32 &srcRgb, int maxHsv = 255);

DVAPI void RGB2HSV(double r, double g, double b, double *h, double *s,
                   double *v);

//--------------------------------

/*!
  IN : h in [0..360], l and s in [0..1]
  OUT: r,g,b in [0..1]
*/

DVAPI void HLS2RGB(double h, double l, double s, double *r, double *g,
                   double *b);

//--------------------------------

/*!
  IN : r,g,b in [0..1]
  OUT: h in [0..360], l and s in [0..1]
*/

DVAPI void rgb2hls(double r, double g, double b, double *h, double *l,
                   double *s);

DVAPI TPixel32 toPixel32(const TPixel64 &);
DVAPI TPixel32 toPixel32(const TPixelD &);
DVAPI TPixel32 toPixel32(const TPixelGR8 &);

DVAPI TPixel64 toPixel64(const TPixel32 &);
DVAPI TPixel64 toPixel64(const TPixelD &);
DVAPI TPixel64 toPixel64(const TPixelGR8 &);

DVAPI TPixelD toPixelD(const TPixel32 &);
DVAPI TPixelD toPixelD(const TPixel64 &);
DVAPI TPixelD toPixelD(const TPixelGR8 &);

//
// nel caso in cui il tipo di destinazione sia il parametro di un template
// es. template<PIXEL> ....
// si fa cosi':
//
// PIXEL c = PixelConverter<PIXEL>::from(c1)
//

template <class T>
class PixelConverter {
public:
  inline static T from(const TPixel32 &pix);
  inline static T from(const TPixel64 &pix);
  inline static T from(const TPixelD &pix);
  inline static T from(const TPixelGR8 &pix);
};

template <>
class PixelConverter<TPixel32> {
public:
  inline static TPixel32 from(const TPixel32 &pix) { return pix; }
  inline static TPixel32 from(const TPixel64 &pix) { return toPixel32(pix); }
  inline static TPixel32 from(const TPixelD &pix) { return toPixel32(pix); }
  inline static TPixel32 from(const TPixelGR8 &pix) { return toPixel32(pix); }
};

template <>
class PixelConverter<TPixel64> {
public:
  inline static TPixel64 from(const TPixel32 &pix) { return toPixel64(pix); }
  inline static TPixel64 from(const TPixel64 &pix) { return pix; }
  inline static TPixel64 from(const TPixelD &pix) { return toPixel64(pix); }
  inline static TPixel64 from(const TPixelGR8 &pix) { return toPixel64(pix); }
};

template <>
class PixelConverter<TPixelD> {
public:
  inline static TPixelD from(const TPixel32 &pix) { return toPixelD(pix); }
  inline static TPixelD from(const TPixel64 &pix) { return toPixelD(pix); }
  inline static TPixelD from(const TPixelD &pix) { return pix; }
  inline static TPixelD from(const TPixelGR8 &pix) { return toPixelD(pix); }
};

//---------------------------------------------------------------------------------------

template <class T>
void add(T &pixout, const T &pixin, double v) {
  TINT32 r, g, b, m;
  r        = pixout.r + tround(pixin.r * v);
  g        = pixout.g + tround(pixin.g * v);
  b        = pixout.b + tround(pixin.b * v);
  m        = pixout.m + tround(pixin.m * v);
  pixout.r = tcrop<TINT32>(r, 0, T::maxChannelValue);
  pixout.g = tcrop<TINT32>(g, 0, T::maxChannelValue);
  pixout.b = tcrop<TINT32>(b, 0, T::maxChannelValue);
  pixout.m = tcrop<TINT32>(m, 0, T::maxChannelValue);
}

//---------------------------------------------------------------------------------------

template <class T>
void sub(T &pixout, const T &pixin, double v) {
  TINT32 r, g, b, m;
  r        = pixout.r - (pixin.r * v);
  g        = pixout.g - (pixin.g * v);
  b        = pixout.b - (pixin.b * v);
  m        = pixout.m - (pixin.m * v);
  pixout.r = tcrop<TINT32>(r, 0, T::maxChannelValue);
  pixout.g = tcrop<TINT32>(g, 0, T::maxChannelValue);
  pixout.b = tcrop<TINT32>(b, 0, T::maxChannelValue);
  pixout.m = tcrop<TINT32>(m, 0, T::maxChannelValue);
}

//---------------------------------------------------------------------------------------

//! Multiplies \b pixout by \b pixin. Passed parameter \b v stands for a further
//! additive
//! component on pixin.
template <class T>
void mult(T &pixout, const T &pixin, double v) {
  double r, g, b, m;
  r = pixin.r + v;
  g = pixin.g + v;
  b = pixin.b + v;
  m = pixin.m + v;
  pixout.r =
      (r < 0) ? 0 : ((r < T::maxChannelValue)
                         ? troundp(r * (pixout.r / (double)T::maxChannelValue))
                         : pixout.r);
  pixout.g =
      (g < 0) ? 0 : ((g < T::maxChannelValue)
                         ? troundp(g * (pixout.g / (double)T::maxChannelValue))
                         : pixout.g);
  pixout.b =
      (b < 0) ? 0 : ((b < T::maxChannelValue)
                         ? troundp(b * (pixout.b / (double)T::maxChannelValue))
                         : pixout.b);
  pixout.m =
      (m < 0) ? 0 : ((m < T::maxChannelValue)
                         ? troundp(m * (pixout.m / (double)T::maxChannelValue))
                         : pixout.m);
}

//---------------------------------------------------------------------------------------

//! Substitutes \b pixout components with those of \b pixin, when the latters
//! are greater.
template <class T>
void lighten(T &pixout, const T &pixin, double v) {
  pixout.r = pixin.r > pixout.r ? pixin.r : pixout.r;
  pixout.g = pixin.g > pixout.g ? pixin.g : pixout.g;
  pixout.b = pixin.b > pixout.b ? pixin.b : pixout.b;
  pixout.m = pixin.m > pixout.m ? pixin.m : pixout.m;
}

//---------------------------------------------------------------------------------------

//! Substitutes \b pixout components with those of \b pixin, when the latters
//! are smaller.
template <class T>
void darken(T &pixout, const T &pixin, double v) {
  pixout.r = pixin.r < pixout.r ? pixin.r : pixout.r;
  pixout.g = pixin.g < pixout.g ? pixin.g : pixout.g;
  pixout.b = pixin.b < pixout.b ? pixin.b : pixout.b;
  pixout.m = pixin.m < pixout.m ? pixin.m : pixout.m;
}

#endif
