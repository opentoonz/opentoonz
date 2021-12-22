#pragma once

#ifndef T_PIXEL_INCLUDED
#define T_PIXEL_INCLUDED

#include "tcommon.h"
#include "tmachine.h"

#include <math.h>

#undef DVAPI
#undef DVVAR
#ifdef TCOLOR_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! r,g,b,m; 4 byte/pixel
class TPixelRGBM32;
//! r,g,b,m; 8 byte/pixel
class TPixelRGBM64;
//! POSSIBLY UNUSED! r:5,g:5,b:5; 2 byte/pixel; 1 bit unused
class TPixelRGB555;
//! POSSIBLY UNUSED! r:5,g:6,b:5; 2 byte/pixel
class TPixelRGB565;
//! Double r,g,b,m ; 16 byte/pixel
class TPixelD;

//! Gray Scale 1 byte/pixel
class TPixelGR8;
//! Gray Scale 2 byte/pixel
class TPixelGR16;

//-----------------------------------------------------------------------------

/*! The standard pixel type: r,g,b,m; 1 byte/channel.
    A set of predefined colors are included as well.
    Note that channel ordering is platform depending. */

class DVAPI DV_ALIGNED(4) TPixelRGBM32 {
  TPixelRGBM32(TUINT32 mask) { *(TUINT32 *)this = mask; };

public:
  static const int maxChannelValue;
  typedef unsigned char Channel;

#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
  Channel b, g, r, m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
  Channel m, b, g, r;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
  unsigned char r, g, b, m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
  Channel m, r, g, b;
#else
#error "Undefined machine order !!!!"
#endif

#ifdef MACOSX

#ifdef powerpc
  TPixelRGBM32() : m(maxChannelValue), r(0), g(0), b(0){};
  TPixelRGBM32(const TPixelRGBM32 &pix)
      : m(pix.m), r(pix.r), g(pix.g), b(pix.b){};
  TPixelRGBM32(int rr, int gg, int bb, int mm = maxChannelValue)
      : m(mm), r(rr), g(gg), b(bb){};
#else
  TPixelRGBM32() : b(0), g(0), r(0), m(maxChannelValue){};
  TPixelRGBM32(const TPixelRGBM32 &pix)
      : b(pix.b), g(pix.g), r(pix.r), m(pix.m){};
  TPixelRGBM32(int rr, int gg, int bb, int mm = maxChannelValue)
      : b(bb), g(gg), r(rr), m(mm){};
#endif

#else

  TPixelRGBM32() : r(0), g(0), b(0), m(maxChannelValue){};
  TPixelRGBM32(int rr, int gg, int bb, int mm = maxChannelValue)
      : r(rr), g(gg), b(bb), m(mm){};

  // Copy constructor and operator=
  TPixelRGBM32(const TPixelRGBM32 &pix) {
    *(TUINT32 *)this = *(const TUINT32 *)&pix;
  }

  TPixelRGBM32 &operator=(const TPixelRGBM32 &pix) {
    *(TUINT32 *)this = *(const TUINT32 *)&pix;
    return *this;
  }

#endif

public:
  inline bool operator==(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this == *(const TUINT32 *)&p;
  }
  inline bool operator!=(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this != *(const TUINT32 *)&p;
  }

  inline bool operator<(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this < *(const TUINT32 *)&p;
  }
  inline bool operator>=(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this >= *(const TUINT32 *)&p;
  }

  inline bool operator>(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this > *(const TUINT32 *)&p;
  }
  inline bool operator<=(const TPixelRGBM32 &p) const {
    return *(const TUINT32 *)this <= *(const TUINT32 *)&p;
  }

  /*
//!Returns itself
static inline TPixelRGBM32 from(const TPixelRGBM32 &pix) {return pix;};
//!Converts TPixelRGBM64 into TPixelRGBM32
static inline TPixelRGBM32 from(const TPixelRGBM64 &pix);
//!Converts TPixelGR8 into TPixelRGBM32
static TPixelRGBM32 from(const TPixelGR8 &pix);
//!Converts TPixelGR16 into TPixelRGBM32
static TPixelRGBM32 from(const TPixelGR16 &pix);
//!In this conversion instead of truncating values from 64 to 32 a randomic
dithering is performed.
//!r is a unsigned int random value
static inline TPixelRGBM32 from(const TPixelRGBM64 &pix, TUINT32 r); // per il
dithering
// ecc..

//!Converts TPixelD into TPixelRGBM32
// static inline TPixelRGBM32 from(const TPixelD &pix);
*/
  static const TPixelRGBM32 Red;
  static const TPixelRGBM32 Green;
  static const TPixelRGBM32 Blue;
  static const TPixelRGBM32 Yellow;
  static const TPixelRGBM32 Cyan;
  static const TPixelRGBM32 Magenta;
  static const TPixelRGBM32 White;
  static const TPixelRGBM32 Black;
  static const TPixelRGBM32 Transparent;
};

//-----------------------------------------------------------------------------
/*!The standard pixel type: r,g,b,m; 2 byte/channel.
  A set of predefined colors are included as well.
  Note that channel ordering is platform depending. */
//  8 byte alignment cannot be specified for function parameters
//  in Visual Studio 32bit platform.
//  Since SSE2 mostly require 16 byte aligned, changing 8 byte align to 4 byte
//  align will not cause problems.
#if defined(_MSC_VER) && !defined(x64)
class DVAPI DV_ALIGNED(4) TPixelRGBM64 {
#else
class DVAPI DV_ALIGNED(8) TPixelRGBM64 {
#endif
public:
  static const int maxChannelValue;
  typedef unsigned short Channel;

#ifdef TNZ_MACHINE_CHANNEL_ORDER_BGRM
  Channel b, g, r, m;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
  Channel m, r, g, b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
Channel r, g, b, m;
#else
undefined machine order !!!!
#endif

#ifdef _WIN32
  TPixelRGBM64() : r(0), g(0), b(0), m(maxChannelValue){};
  TPixelRGBM64(int rr, int gg, int bb, int mm = maxChannelValue)
      : r(rr), g(gg), b(bb), m(mm){};
#else
#if defined(LINUX) || defined(FREEBSD) || defined(MACOSX)

#ifdef powerpc

  TPixelRGBM64() : m(maxChannelValue), b(0), g(0), r(0){};
  TPixelRGBM64(int rr, int gg, int bb, int mm = maxChannelValue)
      : m(mm), b(bb), g(gg), r(rr){};
#else

  TPixelRGBM64() : b(0), g(0), r(0), m(maxChannelValue){};
  TPixelRGBM64(int rr, int gg, int bb, int mm = maxChannelValue)
      : b(bb), g(gg), r(rr), m(mm){};

#endif
#endif
#endif

  // Copy constructor and operator=
  TPixelRGBM64(const TPixelRGBM64 &pix) {
    *(TUINT64 *)this = *(const TUINT64 *)&pix;
  }

  TPixelRGBM64 &operator=(const TPixelRGBM64 &pix) {
    *(TUINT64 *)this = *(const TUINT64 *)&pix;
    return *this;
  }

public:
  inline bool operator==(const TPixelRGBM64 &p) const {
    return *(const TUINT64 *)this == *(const TUINT64 *)&p;
  }
  inline bool operator!=(const TPixelRGBM64 &p) const {
    return *(const TUINT64 *)this != *(const TUINT64 *)&p;
  }

  inline bool operator<(const TPixelRGBM64 &p) const {
    return *(const TUINT64 *)this < *(const TUINT64 *)&p;
  }
  inline bool operator>=(const TPixelRGBM64 &p) const { return !operator<(p); }

  inline bool operator>(const TPixelRGBM64 &p) const {
    return *(const TUINT64 *)this > *(const TUINT64 *)&p;
  }
  inline bool operator<=(const TPixelRGBM64 &p) const { return !operator>(p); }

  /*
//!Converts TPixelRGBM32 into TPixelRGBM64
static inline TPixelRGBM64 from(const TPixelRGBM32 &pix);
//!Converts TPixelGR8 into TPixelRGBM64
static TPixelRGBM64 from(const TPixelGR8 &pix   );
//!Converts TPixelGR16 into TPixelRGBM64
static TPixelRGBM64 from(const TPixelGR16 &pix  );
//!Converts TPixelD into TPixelRGBM64
static inline TPixelRGBM64 from(const TPixelD &pix);
*/

  static const TPixelRGBM64 Red;
  static const TPixelRGBM64 Green;
  static const TPixelRGBM64 Blue;
  static const TPixelRGBM64 Yellow;
  static const TPixelRGBM64 Cyan;
  static const TPixelRGBM64 Magenta;
  static const TPixelRGBM64 White;
  static const TPixelRGBM64 Black;
  static const TPixelRGBM64 Transparent;
};

//-----------------------------------------------------------------------------

//! TPixel32 is a shortcut for TPixelRGBM32. Use it!
typedef TPixelRGBM32 TPixel32;
//! TPixel is a shortcut for TPixelRGBM32.
typedef TPixelRGBM32 TPixel;
//! TPixel64 is a shortcut for TPixelRGBM64. Use it!
typedef TPixelRGBM64 TPixel64;

//-----------------------------------------------------------------------------

class DVAPI TPixelD {
public:
  typedef double Channel;

  Channel r, g, b, m;

  TPixelD() : r(0), g(0), b(0), m(1){};
  TPixelD(const TPixelD &pix) : r(pix.r), g(pix.g), b(pix.b), m(pix.m){};
  TPixelD(double rr, double gg, double bb, double mm = 1)
      : r(rr), g(gg), b(bb), m(mm){};

  inline bool operator==(const TPixelD &p) const {
    return r == p.r && g == p.g && b == p.b && m == p.m;
  };
  inline bool operator<(const TPixelD &p) const {
    return r < p.r ||
           (r == p.r &&
            (g < p.g || (g == p.g && (b < p.b || (b == p.b && (m < p.m))))));
  };

  inline bool operator>=(const TPixelD &p) const { return !operator<(p); };
  inline bool operator!=(const TPixelD &p) const { return !operator==(p); };
  inline bool operator>(const TPixelD &p) const {
    return !operator<(p) && !operator==(p);
  };
  inline bool operator<=(const TPixelD &p) const { return !operator>(p); };

  inline TPixelD operator*=(const TPixelD &p) {
    r *= p.r;
    g *= p.g;
    b *= p.b;
    m *= p.m;
    return *this;
  }
  inline TPixelD operator*(const TPixelD &p) const {
    TPixelD ret(*this);
    return ret *= p;
  }

  /*
//!Returns TPixelRGBM32 into TPixelD
static inline TPixelD from(const TPixelRGBM32 &pix);
//!Converts TPixelRGBM64 into TPixelRGBM32
static inline TPixelD from(const TPixelRGBM64 &pix);
//!Converts TPixelGR8 into TPixelRGBM32
static TPixelD from(const TPixelGR8 &pix);
//!Converts TPixelGR16 into TPixelRGBM32
static TPixelD from(const TPixelGR16 &pix);
//!Returns itself
static inline TPixelD from(const TPixelD &pix) {return pix;};
*/

  static const TPixelD Red;
  static const TPixelD Green;
  static const TPixelD Blue;
  static const TPixelD Yellow;
  static const TPixelD Cyan;
  static const TPixelD Magenta;
  static const TPixelD White;
  static const TPixelD Black;
  static const TPixelD Transparent;
};

//-----------------------------------------------------------------------------

class DVAPI TPixelCY {
public:
  UCHAR c, y;
};

/*

TPixel64 DVAPI TPixel64::from(const TPixel32 &pix)
{
   return TPixel64(
         ushortFromByte(pix.r),
         ushortFromByte(pix.g),
         ushortFromByte(pix.b),
         ushortFromByte(pix.m));
}

//-----------------------------------------------------------------------------

TPixel32 DVAPI TPixel32::from(const TPixel64 &pix)
{
   return TPixel32(
         byteFromUshort(pix.r),
         byteFromUshort(pix.g),
         byteFromUshort(pix.b),
         byteFromUshort(pix.m));
}

//-----------------------------------------------------------------------------

TPixelD DVAPI TPixelD::from(const TPixel32 &pix)
{
  const double k = 1.0/255.0;
  return TPixelD(k*pix.r,k*pix.g,k*pix.b,k*pix.m);
}

//-----------------------------------------------------------------------------

TPixelD DVAPI TPixelD::from(const TPixel64 &pix)
{
  const double k = 1.0/65535.0;
  return TPixelD(k*pix.r,k*pix.g,k*pix.b,k*pix.m);
}

*/
//-----------------------------------------------------------------------------
/*
TPixel32 DVAPI TPixel32::from(const TPixelD &pix)
{
  const int max = 255;
  return TPixel32(
    tcrop((int)(pix.r*max), 0,max),
    tcrop((int)(pix.g*max), 0,max),
    tcrop((int)(pix.b*max), 0,max),
    tcrop((int)(pix.m*max), 0,max));
}
*/
//-----------------------------------------------------------------------------
/*
TPixel64 DVAPI TPixel64::from(const TPixelD &pix)
{
  const int max = 65535;
  return TPixel64(
    tcrop((int)(pix.r*max), 0,max),
    tcrop((int)(pix.g*max), 0,max),
    tcrop((int)(pix.b*max), 0,max),
    tcrop((int)(pix.m*max), 0,max));
}
*/
//-----------------------------------------------------------------------------

#endif  //__T_PIXEL_INCLUDED
