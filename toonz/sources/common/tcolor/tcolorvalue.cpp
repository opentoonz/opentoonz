

#include "tcolorvalue.h"

//===================================================================

void TColorValue::getHsv(int &ih, int &is, int &iv) const {
  double max, min;
  double delta;
  double r, g, b;
  double v, s, h;
  r = m_r;
  g = m_g;
  b = m_b;
  assert(0 <= r && r <= 1);
  assert(0 <= g && g <= 1);
  assert(0 <= b && b <= 1);

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
    h   = h * 60;
    if (h < 0) h += 360;
  }
  assert(0 <= h && h <= 360);
  assert(0 <= s && s <= 1);
  assert(0 <= v && v <= 1);
  ih = (int)h;
  is = (int)(s * 100);
  iv = (int)(v * 100);
}

//===================================================================

void TColorValue::getHls(double &h, double &l, double &s) const {
  double max, min;
  double delta;

  max = std::max({m_r, m_g, m_b});
  min = std::min({m_r, m_g, m_b});

  l = (max + min) / 2;

  if (max == min) {
    s = 0;
    h = 0;
  } else {
    if (l <= 0.5)
      s = (max - min) / (max + min);
    else
      s = (max - min) / (2 - max - min);

    delta = max - min;
    if (m_r == max)
      h = (m_g - m_b) / delta;
    else if (m_g == max)
      h = 2 + (m_b - m_r) / delta;
    else if (m_b == max)
      h = 4 + (m_r - m_g) / delta;

    h = h * 60;
    if (h < 0) h += 360;
  }
}

//===================================================================

void TColorValue::setHsv(int h, int s, int v) {
  int i;
  double p, q, t, f;
  double hue, sat, value;

  hue   = h;
  sat   = 0.01 * s;
  value = 0.01 * v;

  assert(0 <= hue && hue <= 360);
  assert(0 <= sat && sat <= 1);
  assert(0 <= value && value <= 1);

  if (sat == 0) {
    m_r = m_g = m_b = value;
  } else {
    if (hue == 360) hue = 0;

    hue = hue / 60;
    i   = (int)hue;
    f   = hue - i;
    p   = tcrop(value * (1 - sat), 0., 1.);
    q   = tcrop(value * (1 - (sat * f)), 0., 1.);
    t   = tcrop(value * (1 - (sat * (1 - f))), 0., 1.);

    switch (i) {
    case 0:
      m_r = value;
      m_g = t;
      m_b = p;
      break;
    case 1:
      m_r = q;
      m_g = value;
      m_b = p;
      break;
    case 2:
      m_r = p;
      m_g = value;
      m_b = t;
      break;
    case 3:
      m_r = p;
      m_g = q;
      m_b = value;
      break;
    case 4:
      m_r = t;
      m_g = p;
      m_b = value;
      break;
    case 5:
      m_r = value;
      m_g = p;
      m_b = q;
      break;
    }
  }
}

//===================================================================

void TColorValue::getRgb(int &r, int &g, int &b) const {
  r = (int)(m_r * 255 + 0.5);
  g = (int)(m_g * 255 + 0.5);
  b = (int)(m_b * 255 + 0.5);
}

//===================================================================

TPixel32 TColorValue::getPixel() const {
  int r, g, b;
  getRgb(r, g, b);
  return TPixel32(r, g, b, (int)(m_m * 255.0 + 0.5));
}

//===================================================================

void TColorValue::setRgb(int r, int g, int b) {
  m_r = r / 255.0;
  m_g = g / 255.0;
  m_b = b / 255.0;
}

//===================================================================

void TColorValue::setPixel(const TPixel32 &src) {
  setRgb(src.r, src.g, src.b);
  m_m = src.m / 255.0;
}
