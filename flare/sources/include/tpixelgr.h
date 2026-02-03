#pragma once

#ifndef _PIXEL_GR_H
#define _PIXEL_GR_H

#include "tcommon.h"

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
//! Double r,g,b,m ; 16 byte/pixel
class TPixelD;
//! Gray Scale 1 byte/pixel
class TPixelGR8;
//! Gray Scale 2 byte/pixel
class TPixelGR16;

class TPixelF;

//-----------------------------------------------------------------------------
/*! grey tones, 8 bits
   A set of predefined colors are included as well.
*/
class DVAPI TPixelGR8 {
public:
  static const int maxChannelValue;
  typedef UCHAR Channel;

  UCHAR value;
  TPixelGR8(int v = 0) : value(v){};
  TPixelGR8(const TPixelGR8 &pix) : value(pix.value){};

  inline bool operator==(const TPixelGR8 &p) const { return value == p.value; };
  inline bool operator!=(const TPixelGR8 &p) const { return value != p.value; };
  inline bool operator<(const TPixelGR8 &p) const { return value < p.value; };
  inline bool operator<=(const TPixelGR8 &p) const { return value <= p.value; };
  inline bool operator>(const TPixelGR8 &p) const { return value > p.value; };
  inline bool operator>=(const TPixelGR8 &p) const { return value >= p.value; };

  inline void setValue(int _value) { value = (UCHAR)_value; }

  //! In this conversion instead of truncating values from 16 to 8 a randomic
  //! dithering is performed.
  //    randomNumber is an unsigned int random value
  static inline TPixelGR8 from(const TPixelGR16 &pix,
                               TUINT32 randomNumber);  // per il dithering
  //! In this conversion instead of truncating values from 64 to 8 a randomic
  //! dithering is performed.
  //    randomNumber is an unsigned int random value

  static TPixelGR8 from(const TPixelRGBM64 &pix,
                        TUINT32 randomNumber);  // per il dithering
  //! Converts TPixelRGBM32 into TPixelGR8
  static TPixelGR8 from(const TPixelRGBM32 &pix);
  //! Converts TPixelGR16 into TPixelGR8
  static inline TPixelGR8 from(const TPixelGR16 &pix);
  //! Converts TPixelD into TPixelGR8
  static TPixelGR8 from(const TPixelD &pix);

  static const TPixelGR8 White;
  static const TPixelGR8 Black;
};

//-----------------------------------------------------------------------------

/*!grey tones, 16 bits */
class DVAPI TPixelGR16 {
public:
  static const int maxChannelValue;
  typedef USHORT Channel;

  USHORT value;
  TPixelGR16(int v = 0) : value(v){};
  TPixelGR16(const TPixelGR16 &pix) : value(pix.value){};

  inline bool operator==(const TPixelGR16 &p) const {
    return value == p.value;
  };
  inline bool operator!=(const TPixelGR16 &p) const {
    return value != p.value;
  };
  inline bool operator<(const TPixelGR16 &p) const { return value < p.value; };
  inline bool operator<=(const TPixelGR16 &p) const {
    return value <= p.value;
  };
  inline bool operator>(const TPixelGR16 &p) const { return value > p.value; };
  inline bool operator>=(const TPixelGR16 &p) const {
    return value >= p.value;
  };

  inline void setValue(int _value) { value = (USHORT)_value; }
  static TPixelGR16 from(const TPixelRGBM64 &pix);

  static TPixelGR16 from(const TPixelD &pix);

  static const TPixelGR16 White;
  static const TPixelGR16 Black;
};

//-----------------------------------------------------------------------------

class DVAPI TPixelGRF {
public:
  typedef float Channel;

  float value;
  TPixelGRF(float v = 0.f) : value(v){};
  TPixelGRF(const TPixelGRF &pix) : value(pix.value){};
  inline bool operator==(const TPixelGRF &p) const { return value == p.value; };
  inline bool operator<(const TPixelGRF &p) const { return value < p.value; };

  inline void setValue(float _value) { value = (float)_value; }
  static TPixelGRF from(const TPixelF &pix);
};

//-----------------------------------------------------------------------------

class DVAPI TPixelGRD {
public:
  typedef double Channel;

  double value;
  TPixelGRD(double v = 0.0) : value(v){};
  TPixelGRD(const TPixelGRD &pix) : value(pix.value){};
  inline bool operator==(const TPixelGRD &p) const { return value == p.value; };
  inline bool operator<(const TPixelGRD &p) const { return value < p.value; };

  inline void setValue(double _value) { value = (double)_value; }
  static TPixelGRD from(const TPixelGR8 &pix) {
    return TPixelGRD((double)(pix.value));
  }
};

#endif
