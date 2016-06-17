#pragma once

#ifndef T_COLORVALUE_INCLUDED
#define T_COLORVALUE_INCLUDED

#include "tpixel.h"

#undef DVAPI
#undef DVVAR
#ifdef TCOLOR_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TColorValue {
  double m_r, m_g, m_b, m_m;

public:
  TColorValue() : m_r(0), m_g(0), m_b(0), m_m(1){};
  TColorValue(const TPixel32 &src) { setPixel(src); }

  void getHsv(int &h, int &s, int &v) const;
  void getHsv(int hsv[3]) const { getHsv(hsv[0], hsv[1], hsv[2]); }

  void getHls(double hls[3]) const { getHls(hls[0], hls[1], hls[2]); }
  void getHls(double &h, double &l, double &s) const;

  void setHsv(int h, int s, int v);
  void setHsv(int hsv[3]) { setHsv(hsv[0], hsv[1], hsv[2]); }

  void getRgb(int &r, int &g, int &b) const;
  /*Sposto in tcolorvalue.cpp
{
r = (int)(m_r*255+0.5);
g = (int)(m_g*255+0.5);
b = (int)(m_b*255+0.5);
}
*/

  void getRgb(int rgb[3]) const { getRgb(rgb[0], rgb[1], rgb[2]); }

  TPixel32 getPixel() const;
  /*Sposto in tcolorvalue.cpp
{
int r,g,b; getRgb(r,g,b);
return TPixel32(r,g,b,(int)(m_m*255.0+0.5));
}
*/

  void setRgb(int r, int g, int b);
  /*Sposto in tcolorvalue.cpp
{
m_r = r/255.0;
m_g = g/255.0;
m_b = b/255.0;
}
*/

  void setRgb(int rgb[3]) { setRgb(rgb[0], rgb[1], rgb[2]); }

  void setPixel(const TPixel32 &src);
  /*Sposto in tcolorvalue.cpp
{
setRgb(src.r, src.g,src.b);
m_m = src.m/255.0;
}
*/

  bool operator==(const TColorValue &cv) const {
    return cv.m_r == m_r && cv.m_g == m_g && cv.m_b == m_b && cv.m_m == m_m;
  }

  bool operator!=(const TColorValue &cv) const { return !operator==(cv); }
  bool operator<(const TColorValue &cv) const {
    if (m_r < cv.m_r)
      return true;
    else if (m_r > cv.m_r)
      return false;
    if (m_g < cv.m_g)
      return true;
    else if (m_g > cv.m_g)
      return false;
    if (m_b < cv.m_b)
      return true;
    else if (m_b > cv.m_b)
      return false;
    return (m_m < cv.m_m);
  }
  bool operator>=(const TColorValue &cv) const { return !(*this < cv); }
  inline bool operator>(const TColorValue &cv) const { return (cv < *this); }
  bool operator<=(const TColorValue &cv) const { return (cv >= *this); }
};

#endif
