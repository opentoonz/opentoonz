

//#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"
#include "hsvutil.h"

class HSVScaleFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(HSVScaleFx)

  TRasterFxPort m_input;
  TDoubleParamP m_hue;
  TDoubleParamP m_sat;
  TDoubleParamP m_value;
  TDoubleParamP m_hueScale;
  TDoubleParamP m_satScale;
  TDoubleParamP m_valueScale;

public:
  HSVScaleFx()
      : m_hue(0.0)
      , m_sat(0.0)
      , m_value(0.0)
      , m_hueScale(100.0)
      , m_satScale(100.0)
      , m_valueScale(100.0)

  {
    bindParam(this, "hue", m_hue);
    bindParam(this, "saturation", m_sat);
    bindParam(this, "value", m_value);
    bindParam(this, "hue_scale", m_hueScale);
    bindParam(this, "saturation_scale", m_satScale);
    bindParam(this, "value_scale", m_valueScale);
    m_hue->setValueRange(-180, 180);
    m_sat->setValueRange(-1, 1);
    m_value->setValueRange(-1, 1);
    m_hueScale->setValueRange(0, (std::numeric_limits<double>::max)());
    m_satScale->setValueRange(0, (std::numeric_limits<double>::max)());
    m_valueScale->setValueRange(0, (std::numeric_limits<double>::max)());
    addInputPort("Source", m_input);
  }

  ~HSVScaleFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      m_input->doGetBBox(frame, bBox, info);
      return true;
    }

    return false;
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

template <typename PIXEL, typename CHANNEL_TYPE>
void doHSVScale(const TRasterPT<PIXEL> &ras, double hue, double sat,
                double value, double hueScale, double satScale,
                double valueScale) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();

    for (; pix < endPix; ++pix) {
      if (pix->m == 0) continue;

      double m = pix->m;

      double r, g, b, h, s, v;
      // depremult(pix);
      r = pix->r / m;
      g = pix->g / m;
      b = pix->b / m;
      OLDRGB2HSV(r, g, b, &h, &s, &v);
      /*     int hsv[3];
rgb2hsv(hsv, *pix);
h =((double)hsv[0]/255.)*360.;
s =hsv[1]/255.;
v =hsv[2]/255.;*/
      h += hue;
      s += sat;
      v += value;
      h *= hueScale;
      s *= satScale;
      v *= valueScale;
      /*     hsv[0]=tcrop((int)((h/360.)*255.),0,255);
hsv[1]=tcrop((int)s*255, 0,255);
hsv[2]=tcrop((int)v*255, 0,255);
hsv2rgb(*pix,hsv);*/

      OLDHSV2RGB(h, s, v, &r, &g, &b);
      pix->r = (CHANNEL_TYPE)(r * m);
      pix->g = (CHANNEL_TYPE)(g * m);
      pix->b = (CHANNEL_TYPE)(b * m);
      // premultiply(*pix);
    }
  }
  ras->unlock();
}
//------------------------------------------------------------------------------

void HSVScaleFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double hue        = m_hue->getValue(frame);
  double sat        = m_sat->getValue(frame);
  double value      = m_value->getValue(frame);
  double hueScale   = m_hueScale->getValue(frame) / 100.0;
  double satScale   = m_satScale->getValue(frame) / 100.0;
  double valueScale = m_valueScale->getValue(frame) / 100.0;

  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doHSVScale<TPixel32, UCHAR>(raster32, hue, sat, value, hueScale, satScale,
                                valueScale);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doHSVScale<TPixel64, USHORT>(raster64, hue, sat, value, hueScale,
                                   satScale, valueScale);
    else
      throw TException("HSVScale: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(HSVScaleFx, "hsvScaleFx")
