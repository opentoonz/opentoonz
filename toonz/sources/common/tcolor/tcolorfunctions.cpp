
#include <cstring>
#include "tcolorfunctions.h"
#include "tpixelutils.h"

TPixel32 TColorFader::operator()(const TPixel32 &color) const {
  TPixel32 ret = blend(color, m_color, m_fade);
  return ret;
}

bool TColorFader::getParameters(Parameters &p) const {
  p.m_mR = p.m_mG = p.m_mB = p.m_mM = (1 - m_fade);
  p.m_cR                            = m_fade * m_color.r;
  p.m_cG                            = m_fade * m_color.g;
  p.m_cB                            = m_fade * m_color.b;
  p.m_cM                            = m_fade * m_color.m;
  return true;
}

//---------------------------------------

TGenericColorFunction::TGenericColorFunction(const double m[4],
                                             const double c[4]) {
  memcpy(m_m, m, 4 * sizeof(double));
  memcpy(m_c, c, 4 * sizeof(double));
}

//---------------------------------------

TPixel32 TGenericColorFunction::operator()(const TPixel32 &color) const {
  return TPixel32(tcrop(m_m[0] * color.r + m_c[0], 0.0, 255.0),
                  tcrop(m_m[1] * color.g + m_c[1], 0.0, 255.0),
                  tcrop(m_m[2] * color.b + m_c[2], 0.0, 255.0),
                  tcrop(m_m[3] * color.m + m_c[3], 0.0, 255.0));
}

//---------------------------------------

bool TGenericColorFunction::getParameters(Parameters &p) const {
  p.m_mR = m_m[0], p.m_mG = m_m[1], p.m_mB = m_m[2], p.m_mM = m_m[3];
  p.m_cR = m_c[0], p.m_cG = m_c[1], p.m_cB = m_c[2], p.m_cM = m_c[3];

  return true;
}

//---------------------------------------

TPixel32 TTranspFader::operator()(const TPixel32 &color) const {
  return TPixel32(color.r, color.g, color.b, m_transp * color.m);
}

//---------------------------------------

bool TTranspFader::getParameters(Parameters &p) const {
  assert(false);
  return true;
}

//----------------------------------

TPixel32 TOnionFader::operator()(const TPixel32 &color) const {
  if (color.m == 0) return color;

  TPixel32 blendColor = blend(color, m_color, m_fade);
  blendColor.m        = 180;
  return blendColor;
}

bool TOnionFader::getParameters(Parameters &p) const {
  p.m_mR = p.m_mG = p.m_mB = (1 - m_fade);
  p.m_mM                   = 1;
  p.m_cR                   = m_fade * m_color.r;
  p.m_cG                   = m_fade * m_color.g;
  p.m_cB                   = m_fade * m_color.b;
  p.m_cM                   = m_color.m;
  return true;
}

//---------------------------------------

TPixel32 TColumnColorFilterFunction::operator()(const TPixel32 &color) const {
  int r = 255 - (255 - color.r) * (255 - m_colorScale.r) / 255;
  int g = 255 - (255 - color.g) * (255 - m_colorScale.g) / 255;
  int b = 255 - (255 - color.b) * (255 - m_colorScale.b) / 255;
  return TPixel32(r, g, b, color.m * m_colorScale.m / 255);
}

bool TColumnColorFilterFunction::getParameters(Parameters &p) const {
  assert(false);
  return true;
}
