

#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"

#include "tparamset.h"

static void OLDRGB2HSV(double r, double g, double b, double *h, double *s,
                       double *v) {
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
    *h   = *h * 60;
    if (*h < 0) *h += 360;
  }
}

static void OLDHSV2RGB(double hue, double sat, double value, double *red,
                       double *green, double *blue) {
  int i;
  double p, q, t, f;

  //  if (hue > 360 || hue < 0)
  //    hue=0;
  if (hue > 360) hue -= 360;
  if (hue < 0) hue += 360;
  if (sat < 0) sat     = 0;
  if (sat > 1) sat     = 1;
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

class ChangeColorFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ChangeColorFx)
  TRasterFxPort m_input;
  TPixelParamP m_from_color;
  TPixelParamP m_to_color;
  TDoubleParamP m_range;
  TDoubleParamP m_falloff;

public:
  ChangeColorFx()
      : m_from_color(TPixel32::Red)
      , m_to_color(TPixel32::Blue)
      , m_range(0.0)
      , m_falloff(0.0) {
    addInputPort("Source", m_input);
    bindParam(this, "range", m_range);
    bindParam(this, "falloff", m_falloff);
    bindParam(this, "from_color", m_from_color);
    bindParam(this, "to_color", m_to_color);
    m_range->setValueRange(0, 100);
    m_falloff->setValueRange(0, 100);
  }
  ~ChangeColorFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) return m_input->doGetBBox(frame, bBox, info);
    {
      bBox = TRectD();
      return false;
    }
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};

static double normalize_h(double h) {
  if (h < 0) h += 360;
  if (h > 360) h -= 360;
  return h;
}
//------------------------------------------------------------------------------

void ChangeColorFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double from_h, from_s, from_v, to_h, to_s, to_v;
  bool swaprange = false, swapfallmin = false, swapfallmax;
  TPixel32 from_color = m_from_color->getPremultipliedValue(frame);
  TPixel32 to_color   = m_to_color->getPremultipliedValue(frame);
  double range        = m_range->getValue(frame) / 100;
  double falloff      = m_falloff->getValue(frame) / 100;

  OLDRGB2HSV(from_color.r / 255.0, from_color.g / 255.0, from_color.b / 255.0,
             &from_h, &from_s, &from_v);
  OLDRGB2HSV(to_color.r / 255.0, to_color.g / 255.0, to_color.b / 255.0, &to_h,
             &to_s, &to_v);
  /*
int from_hsv[3];
int to_hsv[3];
rgb2hsv(from_hsv, from_color);
rgb2hsv(to_hsv, to_color);

///////////////  verificare   ///////
from_h =((double)from_hsv[0]/255.)*360.;
from_s =from_hsv[1]/255.;
from_v =from_hsv[2]/255.;
to_h =((double)to_hsv[0]/255.)*360.;
to_s =to_hsv[1]/255.;
to_v =to_hsv[2]/255.;
*/
  ////////////////////////
  TRaster32P raster32 = tile.getRaster();
  assert(raster32);  // per ora gestisco solo i Raster32

  double hmin = from_h - range * 180;
  hmin        = normalize_h(hmin);
  double hmax = from_h + range * 180;
  hmax        = normalize_h(hmax);
  if (hmax <= hmin) {
    std::swap(hmin, hmax);
    swaprange = true;
  }
  double smin     = from_s - range;
  double smax     = from_s + range;
  double vmin     = from_v - range;
  double vmax     = from_v + range;
  double hfallmin = hmin - falloff * 180;
  double hfallmax = hmax + falloff * 180;
  hfallmin        = normalize_h(hfallmin);
  hfallmax        = normalize_h(hfallmax);
  if (hfallmin > hmin) {
    swapfallmin = true;
  }
  if (hfallmax < hmax) {
    swapfallmax = true;
  }
  double sfallmin = smin - falloff;
  double sfallmax = smax + falloff;
  double vfallmin = vmin - falloff;
  double vfallmax = vmax + falloff;

  int j;
  raster32->lock();
  for (j = 0; j < raster32->getLy(); j++) {
    TPixel32 *pix    = raster32->pixels(j);
    TPixel32 *endPix = pix + raster32->getLx();
    while (pix < endPix) {
      double r, g, b, h, s, v;
      r = pix->r / 255.0;
      g = pix->g / 255.0;
      b = pix->b / 255.0;
      // int hsv[3];
      OLDRGB2HSV(r, g, b, &h, &s, &v);
      int hflag            = (h <= hmax && h >= hmin);
      if (swaprange) hflag = !hflag;
      if (hflag && s <= smax && s >= smin && v <= vmax && v >= vmin)
        *pix = to_color;
      else {
        double hcorr            = 0;
        double scorr            = 0;
        double vcorr            = 0;
        int hfallminflag        = (h <= hmin && h >= hfallmin);
        if (hfallminflag) hcorr = h - hmin;
        int hfallmaxflag        = (h >= hmax && h <= hfallmax);
        if (hfallmaxflag) hcorr = h - hmax;
        if (hcorr) {
          if (s <= smin && s >= sfallmin) scorr = s - smin;
          if (s >= smax && s <= sfallmax) scorr = s - smax;
          if (v <= vmin && v >= vfallmin) vcorr = v - vmin;
          if (v >= vmax && v <= vfallmax) vcorr = v - vmax;
          if (s < smin && s > sfallmin) scorr   = s - smin;
          if (s > smax && s < sfallmax) scorr   = s - smax;
          h                                     = to_h + hcorr;
          if (h < 0) h += 360;
          if (h > 360) h -= 360;
          OLDHSV2RGB(h, s, v, &r, &g, &b);

          pix->r = (UCHAR)(r * 255);
          pix->g = (UCHAR)(g * 255);
          pix->b = (UCHAR)(b * 255);
        }
      }
      // premultiply(*pix);
      *pix++;
    }
  }
  raster32->unlock();
}

// FX_PLUGIN_IDENTIFIER(ChangeColorFx    , "changeColorFx")
