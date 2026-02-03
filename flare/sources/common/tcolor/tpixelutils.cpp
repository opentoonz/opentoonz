

#include "tpixelutils.h"

// /*!This method is used to produce current palette colors for cm24 Toonz
// images.*/
//  static inline TPixelRGBM32 combine(const TPixelRGBM32 &a, const TPixelRGBM32
//  &b) {
//    return (*(TUINT32*)&a + *(TUINT32*)&b);
// }

//-----------------------------------------------------------------------------

namespace {
int byteCrop(int v) { return (unsigned int)v <= 255 ? v : v > 255 ? 255 : 0; }
int wordCrop(int v) {
  return (unsigned int)v <= 65535 ? v : v > 65535 ? 65535 : 0;
}
}  // namespace

//-----------------------------------------------------------------------------

void hsv2rgb(TPixel32 &dstRgb, int srcHsv[3], int maxHsv) {
  int i;
  double p, q, t, f;
  double hue, sat, value;
  assert(maxHsv);
  hue   = ((double)srcHsv[0] / maxHsv) * 360.;
  sat   = (double)srcHsv[1] / maxHsv;
  value = (double)srcHsv[2] / maxHsv;

  if (hue > 360) hue -= 360;
  if (hue < 0) hue += 360;
  if (sat < 0) sat = 0;
  if (sat > 1) sat = 1;
  if (value < 0) value = 0;
  if (value > 1) value = 1;
  if (sat == 0) {
    dstRgb.r = dstRgb.g = dstRgb.b = tcrop((int)(value * 255), 0, 255);
  } else {
    if (hue == 360) hue = 0;

    hue = hue / 60;
    i   = (int)hue;
    f   = hue - i;
    p   = value * (1 - sat);
    q   = value * (1 - (sat * f));
    t   = value * (1 - (sat * (1 - f)));

    if (i == 0) {
      dstRgb.r = tcrop((int)(value * 255), 0, 255);
      dstRgb.g = tcrop((int)(t * 255), 0, 255);
      dstRgb.b = tcrop((int)(p * 255), 0, 255);
    } else if (i == 1) {
      dstRgb.r = tcrop((int)(q * 255), 0, 255);
      dstRgb.g = tcrop((int)(value * 255), 0, 255);
      dstRgb.b = tcrop((int)(p * 255), 0, 255);
    } else if (i == 2) {
      dstRgb.r = tcrop((int)(p * 255), 0, 255);
      dstRgb.g = tcrop((int)(value * 255), 0, 255);
      dstRgb.b = tcrop((int)(t * 255), 0, 255);
    } else if (i == 3) {
      dstRgb.r = tcrop((int)(p * 255), 0, 255);
      dstRgb.g = tcrop((int)(q * 255), 0, 255);
      dstRgb.b = tcrop((int)(value * 255), 0, 255);
    } else if (i == 4) {
      dstRgb.r = tcrop((int)(t * 255), 0, 255);
      dstRgb.g = tcrop((int)(p * 255), 0, 255);
      dstRgb.b = tcrop((int)(value * 255), 0, 255);
    } else if (i == 5) {
      dstRgb.r = tcrop((int)(value * 255), 0, 255);
      dstRgb.g = tcrop((int)(p * 255), 0, 255);
      dstRgb.b = tcrop((int)(q * 255), 0, 255);
    }
  }
  dstRgb.m = 255;  // matte
}
//-----------------------------------------------------------------------------

void HSV2RGB(double hue, double sat, double value, double *red, double *green,
             double *blue) {
  int i;
  double p, q, t, f;

  //  if (hue > 360 || hue < 0)
  //    hue=0;
  if (hue > 360) hue -= 360;
  if (hue < 0) hue += 360;
  if (sat < 0) sat = 0;
  if (sat > 1) sat = 1;
  if (value < 0) value = 0;
  if (value > 1) value = 1;
  if (sat == 0) {
    *red   = value;
    *green = value;
    *blue  = value;
  } else {
    if (hue == 360) hue = 0;

    hue = hue / 60;
    i   = (int)hue;
    f   = hue - i;
    p   = value * (1 - sat);
    q   = value * (1 - (sat * f));
    t   = value * (1 - (sat * (1 - f)));

    switch (i) {
    case 0:
      *red   = value;
      *green = t;
      *blue  = p;
      break;
    case 1:
      *red   = q;
      *green = value;
      *blue  = p;
      break;
    case 2:
      *red   = p;
      *green = value;
      *blue  = t;
      break;
    case 3:
      *red   = p;
      *green = q;
      *blue  = value;
      break;
    case 4:
      *red   = t;
      *green = p;
      *blue  = value;
      break;
    case 5:
      *red   = value;
      *green = p;
      *blue  = q;
      break;
    }
  }
}

void RGB2HSV(double r, double g, double b, double *h, double *s, double *v) {
  double max, min;
  double delta;

  max = std::max({r, g, b});
  min = std::min({r, g, b});

  *v = max;

  if (max != 0)
    *s = (max - min) / max;
  else
    *s = 0;

  if (*s == 0)
    *h = 0;
  else {
    delta = max - min;

    if (r == max)
      *h = (g - b) / delta;
    else if (g == max)
      *h = 2 + (b - r) / delta;
    else if (b == max)
      *h = 4 + (r - g) / delta;
    *h = *h * 60;
    if (*h < 0) *h += 360;
  }
}
void rgb2hsv(int dstHsv[3], const TPixel32 &srcRgb, int maxHsv) {
  double max, min;
  double delta;
  double r, g, b;
  double v, s, h = 0.;
  r = srcRgb.r / 255.;
  g = srcRgb.g / 255.;
  b = srcRgb.b / 255.;

  max = std::max({r, g, b});
  min = std::min({r, g, b});

  v = max;

  if (max != 0)
    s = (max - min) / max;
  else
    s = 0;

  if (s == 0)
    h = 0;
  else {
    delta = max - min;

    if (r == max)
      h = (g - b) / delta;
    else if (g == max)
      h = 2 + (b - r) / delta;
    else if (b == max)
      h = 4 + (r - g) / delta;
    h = h * 60;
    if (h < 0) h += 360;
  }

  dstHsv[0] = tcrop((int)((h / 360.) * maxHsv), 0, maxHsv);
  dstHsv[1] = tcrop((int)(s * maxHsv), 0, maxHsv);
  dstHsv[2] = tcrop((int)(v * maxHsv), 0, maxHsv);
}

/*!
  Conversion between RGB and HLS colorspace
*/

namespace {
// helper function
inline double HLSValue(double n1, double n2, double h) {
  if (h > 360)
    h -= 360;
  else if (h < 0)
    h += 360;
  if (h < 60)
    return n1 + (n2 - n1) * h / 60;
  else if (h < 180)
    return n2;
  else if (h < 240)
    return n1 + (n2 - n1) * (240 - h) / 60;
  else
    return n1;
}
}  // namespace
void HLS2RGB(double h, double l, double s, double *r, double *g, double *b) {
  if (s == 0) {
    *r = *g = *b = l;
    return;
  }

  double m1, m2;

  if (l < 0.5)
    m2 = l * (1 + s);
  else
    m2 = l + s + l * s;
  m1 = 2 * l - m2;

  *r = HLSValue(m1, m2, h + 120);
  *g = HLSValue(m1, m2, h);
  *b = HLSValue(m1, m2, h - 120);
}

void rgb2hls(double r, double g, double b, double *h, double *l, double *s)

{
  double max, min;
  double delta;

  max = std::max({r, g, b});
  min = std::min({r, g, b});

  *l = (max + min) / 2;

  if (max == min) {
    *s = 0;
    *h = 0;
  } else {
    if (*l <= 0.5)
      *s = (max - min) / (max + min);
    else
      *s = (max - min) / (2 - max - min);

    delta = max - min;
    if (r == max)
      *h = (g - b) / delta;
    else if (g == max)
      *h = 2 + (b - r) / delta;
    else if (b == max)
      *h = 4 + (r - g) / delta;

    *h = *h * 60;
    if (*h < 0) *h += 360;
  }
}

//-----------------------------------------------------------------------------

TPixel32 toPixel32(const TPixel64 &src) {
  return TPixelRGBM32(byteFromUshort(src.r), byteFromUshort(src.g),
                      byteFromUshort(src.b), byteFromUshort(src.m));
}

//-----------------------------------------------------------------------------

TPixel32 toPixel32(const TPixelD &src) {
  const double factor = 255.0;
  return TPixel32(
      byteCrop(tround(src.r * factor)), byteCrop(tround(src.g * factor)),
      byteCrop(tround(src.b * factor)), byteCrop(tround(src.m * factor)));
}

//-----------------------------------------------------------------------------

TPixel32 toPixel32(const TPixelGR8 &src) {
  return TPixel32(src.value, src.value, src.value);
}

//-----------------------------------------------------------------------------

TPixel32 toPixel32(const TPixelF &src) {
  const double factor = 255.0f;
  return TPixel32(
      byteCrop(tround(src.r * factor)), byteCrop(tround(src.g * factor)),
      byteCrop(tround(src.b * factor)), byteCrop(tround(src.m * factor)));
}

//-----------------------------------------------------------------------------

TPixel64 toPixel64(const TPixel32 &src) {
  return TPixelRGBM64(ushortFromByte(src.r), ushortFromByte(src.g),
                      ushortFromByte(src.b), ushortFromByte(src.m));
}

//-----------------------------------------------------------------------------

TPixel64 toPixel64(const TPixelD &src) {
  const double factor = 65535.0;
  return TPixel64(
      wordCrop(tround(src.r * factor)), wordCrop(tround(src.g * factor)),
      wordCrop(tround(src.b * factor)), wordCrop(tround(src.m * factor)));
}

//-----------------------------------------------------------------------------

TPixel64 toPixel64(const TPixelGR8 &src) {
  int v = ushortFromByte(src.value);
  return TPixel64(v, v, v);
}

//-----------------------------------------------------------------------------

TPixel64 toPixel64(const TPixelF &src) {
  const double factor = 65535.0;
  return TPixel64(wordCrop(tround((double)src.r * factor)),
                  wordCrop(tround((double)src.g * factor)),
                  wordCrop(tround((double)src.b * factor)),
                  wordCrop(tround((double)src.m * factor)));
}

//-----------------------------------------------------------------------------

TPixelD toPixelD(const TPixel32 &src) {
  const double factor = 1.0 / 255.0;
  return TPixelD(factor * src.r, factor * src.g, factor * src.b,
                 factor * src.m);
}

//-----------------------------------------------------------------------------

TPixelD toPixelD(const TPixel64 &src) {
  const double factor = 1.0 / 65535.0;
  return TPixelD(factor * src.r, factor * src.g, factor * src.b,
                 factor * src.m);
}

//-----------------------------------------------------------------------------

TPixelD toPixelD(const TPixelGR8 &src) {
  const double v = (double)src.value / 255.0;
  return TPixelD(v, v, v);
}

//-----------------------------------------------------------------------------

TPixelD toPixelD(const TPixelF &src) {
  return TPixelD(src.r, src.g, src.b, src.m);
}

//-----------------------------------------------------------------------------

TPixelF toPixelF(const TPixel32 &src) {
  const float factor = 1.f / 255.f;
  return TPixelF(factor * src.r, factor * src.g, factor * src.b,
                 factor * src.m);
}

//-----------------------------------------------------------------------------

TPixelF toPixelF(const TPixelD &src) {
  return TPixelF((float)src.r, (float)src.g, (float)src.b, (float)src.m);
}

//-----------------------------------------------------------------------------

TPixelF toPixelF(const TPixel64 &src) {
  const float factor = 1.f / 65535.f;
  return TPixelF(factor * src.r, factor * src.g, factor * src.b,
                 factor * src.m);
}

//-----------------------------------------------------------------------------

TPixelF toPixelF(const TPixelGR8 &src) {
  const float v = (float)src.value / 255.f;
  return TPixelF(v, v, v);
}

//-----------------------------------------------------------------------------
namespace {
template <class T, class Q>
Q toLin(Q val, double gamma) {
  return (Q)((T::maxChannelValue)*std::pow(
                 (double)val / (double)(T::maxChannelValue), gamma) +
             0.5);
}
template <>
float toLin<TPixelF, float>(float val, double gamma) {
  return (val < 0.f) ? val : std::pow(val, (float)gamma);
}
template <>
double toLin<TPixelD, double>(double val, double gamma) {
  return std::pow(val, gamma);
}
}  // namespace

//-----------------------------------------------------------------------------

TPixel32 toLinear(const TPixel32 &pix, const double gamma) {
  return TPixel32(toLin<TPixel32, unsigned char>(pix.r, gamma),
                  toLin<TPixel32, unsigned char>(pix.g, gamma),
                  toLin<TPixel32, unsigned char>(pix.b, gamma), pix.m);
}
TPixel64 toLinear(const TPixel64 &pix, const double gamma) {
  return TPixel64(toLin<TPixel64, unsigned short>(pix.r, gamma),
                  toLin<TPixel64, unsigned short>(pix.g, gamma),
                  toLin<TPixel64, unsigned short>(pix.b, gamma), pix.m);
}
TPixelD toLinear(const TPixelD &pix, const double gamma) {
  return TPixelD(toLin<TPixelD, double>(pix.r, gamma),
                 toLin<TPixelD, double>(pix.g, gamma),
                 toLin<TPixelD, double>(pix.b, gamma), pix.m);
}
TPixelF toLinear(const TPixelF &pix, const double gamma) {
  return TPixelF(toLin<TPixelF, float>(pix.r, gamma),
                 toLin<TPixelF, float>(pix.g, gamma),
                 toLin<TPixelF, float>(pix.b, gamma), pix.m);
}
TPixelGR8 toLinear(const TPixelGR8 &pix, const double gamma) {
  return TPixelGR8(toLin<TPixelGR8, unsigned char>(pix.value, gamma));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifdef CICCIO

//-----------------------------------------------------------------------------
/*!toonz color-map, 16 bits 7+5+4 bits (paint, ink, tone)*/
class DVAPI TPixelCM16 {  // 754
  static const int maxChannelValue;

public:
  USHORT value;

  TPixelCM16(int v = 0) : value(v){};
  TPixelCM16(int ink, int paint, int tone)
      : value(tone | ink << 9 | paint << 4){};
  TPixelCM16(const TPixelCM16 &pix) : value(pix.value){};

  inline bool operator==(const TPixelCM16 &p) const {
    return value == p.value;
  };

  int getInkId() { return value >> 9; };
  int getPaintId() { return (value >> 4) & 0x1F; };
  int getToneId() { return value & 0xF; };

  static TPixelCM16 pureInk(int ink) { return TPixelCM16(ink, 0, 0); };
  static TPixelCM16 purePaint(int paint) { return TPixelCM16(0, paint, 255); };
};

//-----------------------------------------------------------------------------
/*!cmapped, 16 bits, standard SGI 256-color colormap */
class DVAPI TPixelCM16S8 {
  static const int maxChannelValue;

public:
  USHORT value;
  TPixelCM16S8(int v = 0) : value(v){};
};

//-----------------------------------------------------------------------------
/*cmapped, 16 bits, standard SGI+Toonz 4096-color colormap */
class DVAPI TPixelCM16S12 {
  static const int maxChannelValue;

public:
  USHORT value;
  TPixelCM16S12(int v = 0) : value(v){};
};

//-----------------------------------------------------------------------------
/*!Toonz color-map, 8+8+8 bits (col, pen, tone) + 8 msb bits extra*/
class DVAPI TPixelCM24 {
  static const int maxChannelValue;

public:
  /* serve m_value come membro? vincenzo */
  TUINT32 m_value;
  UCHAR m_ink, m_paint, m_tone;

  TPixelCM24(int v = 0)
      : m_ink((v >> 8) & 0xff), m_paint((v >> 16) & 0xff), m_tone(v & 0xff){};
  TPixelCM24(int ink, int paint, int tone)
      : m_value(0), m_ink(ink), m_paint(paint), m_tone(tone){};
  TPixelCM24(const TPixelCM24 &pix)
      : m_value(pix.m_value)
      , m_ink(pix.m_ink)
      , m_paint(pix.m_paint)
      , m_tone(pix.m_tone){};

  inline bool operator==(const TPixelCM24 &p) const {
    return m_paint == p.m_paint && m_ink == p.m_ink && m_tone == p.m_tone;
  };
  // rivedere
  int getPaintIdx() { return m_paint << 16 | m_tone; };
  int getInkIdx() { return m_ink << 16 | m_tone; };
  // int getTone    () {return m_tone;}; m_tone e' pubblico!

  // static inline TPixelRGBM from(const TPixelCM24 &pix);

  static TPixelCM24 pureInk(int ink) { return TPixelCM24(ink, 0, 0); };
  static TPixelCM24 purePaint(int paint) { return TPixelCM24(0, paint, 255); };
};
//-----------------------------------------------------------------------------

//! Toonz5.0 color-map, 12+12+8 bits (ink,paint,tone)
class DVAPI TPixelCM32 {
public:
  TUINT32 m_value;

  TPixelCM32(TUINT32 v = 0) : m_value{};
  TPixelCM32(int ink, int paint, int tone)
      : m_value(ink << 20 | paint << 8 | tone) {
    assert(0 <= ink && ink < 4096);
    assert(0 <= paint && paint < 4096);
    assert(0 <= tone && tone < 256);
  };
  TPixelCM32(const TPixelCM32 &pix) : m_value(pix.m_value){};

  inline bool operator==(const TPixelCM32 &p) const {
    return m_value == p.m_value;
  };

  inline bool operator<(const TPixelCM32 &p) const {
    return m_value < p.m_value;
  };

  int getPaint() const { return m_paint >> 16 | m_tone; };
  int getInk() const { return m_ink << 16 | m_tone; };
  int getTone() const { return m_tone; };

  // static inline TPixelRGBM from(const TPixelCM24 &pix);

  static TPixelCM24 pureInk(int ink) { return TPixelCM24(ink, 0, 0); };
  static TPixelCM24 purePaint(int paint) { return TPixelCM24(0, paint, 255); };
};

//-----------------------------------------------------------------------------
/*!RGB 5+6+5 bits, red most significant */
class DVAPI TPixelRGB565 {
public:
  TUINT32 r : 5;
  TUINT32 g : 6;
  TUINT32 b : 5;
  TPixelRGB565(int rr, int gg, int bb) : r(rr), g(gg), b(bb) {
    assert(0 <= rr && rr < (1 << 5));
    assert(0 <= gg && gg < (1 << 6));
    assert(0 <= bb && bb < (1 << 5));
  };
  inline bool operator==(const TPixelRGB565 &p) const {
    return r == p.r && g == p.g && b == p.b;
  };
  inline bool operator<(const TPixelRGB565 &p) const {
    return r < p.r || r == p.r && (g < p.g || g == p.g && (b < p.b));
  };

  //! Converts TPixelCM8 into TPixelRGB565
  static inline TPixelRGB565 from(const TPixelCM8 &pix);
  //! Returns itself
  static inline TPixelRGB565 from(const TPixelRGB565 &pix) { return pix; };
  //! Converts TPixelRGBM32 into TPixelRGB565
  static inline TPixelRGB565 from(const TPixelRGBM32 &pix);
  //! Converts TPixelRGBM64 into TPixelRGB565
  static inline TPixelRGB565 from(const TPixelRGBM64 &pix);
  //! Converts TPixelGR8 into TPixelRGB565
  static inline TPixelRGB565 from(const TPixelGR8 &pix);
  //! Converts TPixelGR16 into TPixelRGB565
  static inline TPixelRGB565 from(const TPixelGR16 &pix);
  /*
static const TPixelRGB565 Red;
static const TPixelRGB565 Green;
static const TPixelRGB565 Blue;
static const TPixelRGB565 White;
static const TPixelRGB565 Black;
*/
};

//-----------------------------------------------------------------------------

TPixelRGBM32 DVAPI TPixelRGBM32::from(const TPixelCM8 &pix) {
  return TPixelRGBM32(pix.value, pix.value, pix.value);
}

//-----------------------------------------------------------------------------

TPixelRGBM64 DVAPI TPixelRGBM64::from(const TPixelRGBM32 &pix) {
  return TPixelRGBM64(ushortFromByte(pix.r), ushortFromByte(pix.g),
                      ushortFromByte(pix.b), ushortFromByte(pix.m));
}

//-----------------------------------------------------------------------------

TPixelRGBM32 DVAPI TPixelRGBM32::from(const TPixelRGBM64 &pix) {
  return TPixelRGBM32(byteFromUshort(pix.r), byteFromUshort(pix.g),
                      byteFromUshort(pix.b), byteFromUshort(pix.m));
}

//-----------------------------------------------------------------------------

TPixelRGBM32 DVAPI TPixelRGBM32::from(const TPixelGR8 &pix) {
  return TPixelRGBM32(pix.value, pix.value, pix.value, maxChannelValue);
}

//-----------------------------------------------------------------------------

TPixelRGBM32 DVAPI TPixelRGBM32::from(const TPixelGR16 &pix) {
  UCHAR value = byteFromUshort(pix.value);
  return TPixelRGBM32(value, value, value, maxChannelValue);
}

TPixelD DVAPI TPixelD::from(const TPixelCM8 &pix) {
  double v = (double)pix.value * (1.0 / 255.0);
  return TPixelD(v, v, v, v);
}

TPixelD DVAPI TPixelD::from(const TPixelGR8 &pix) {
  double v = (double)pix.value * (1.0 / 255.0);
  return TPixelD(v, v, v, v);
}

TPixelD DVAPI TPixelD::from(const TPixelRGBM32 &pix) {
  const double k = 1.0 / 255.0;
  return TPixelD(k * pix.r, k * pix.g, k * pix.b, k * pix.m);
}

TPixelD DVAPI TPixelD::from(const TPixelRGBM64 &pix) {
  const double k = 1.0 / 65535.0;
  return TPixelD(k * pix.r, k * pix.g, k * pix.b, k * pix.m);
}

TPixelD DVAPI TPixelD::from(const TPixelGR16 &pix) {
  double v = (double)pix.value * (1.0 / 65535.0);
  return TPixelD(v, v, v, v);
}

TPixel32 DVAPI TPixel32::from(const TPixelD &pix) {
  const int max = 255;
  return TPixel32(tcrop((int)(pix.r * max + .5), 0, max),
                  tcrop((int)(pix.g * max + .5), 0, max),
                  tcrop((int)(pix.b * max + .5), 0, max),
                  tcrop((int)(pix.m * max + .5), 0, max));
}

TPixel64 DVAPI TPixel64::from(const TPixelD &pix) {
  const int max = 65535;
  return TPixel64(tcrop((int)(pix.r * max + .5), 0, max),
                  tcrop((int)(pix.g * max + .5), 0, max),
                  tcrop((int)(pix.b * max + .5), 0, max),
                  tcrop((int)(pix.m * max + .5), 0, max));
}

// TPixelGR8 TPixelGR8::from(const TPixelD &pix)
//{
//  return from(TPixel32::from(pix));
//}

TPixelGR8 DVAPI TPixelGR8::from(const TPixel32 &pix) {
  return TPixelGR8((((UINT)(pix.r) * 19594 + (UINT)(pix.g) * 38472 +
                     (UINT)(pix.b) * 7470 + (UINT)(1 << 15)) >>
                    16));
}

//-----------------------------------------------------------------------------

TPixelGR16 DVAPI TPixelGR16::from(const TPixel64 &pix) {
  return TPixelGR16((((UINT)(pix.r) * 19594 + (UINT)(pix.g) * 38472 +
                      (UINT)(pix.b) * 7470 + (UINT)(1 << 15)) >>
                     16));
}

//-----------------------------------------------------------------------------
// class ostream;

DVAPI ostream &operator<<(ostream &out, const TPixel32 &pixel);
DVAPI ostream &operator<<(ostream &out, const TPixel64 &pixel);
DVAPI ostream &operator<<(ostream &out, const TPixelD &pixel);
DVAPI ostream &operator<<(ostream &out, const TPixelCM8 &pixel);

/*

TPixel64 inline TPixel32::to64() const
{
   return TPixel64(ushortFromByte(r), ushortFromByte(g),
ushortFromByte(b),ushortFromByte(m));
};

TPixel32 inline TPixel64::to32() const
{
   return TPixel32(byteFromUshort(r), byteFromUshort(g), byteFromUshort(b),
byteFromUshort(m));
};
*/

//-----------------------------------------------------------------------------
#endif
