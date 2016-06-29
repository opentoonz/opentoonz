

#include "stdfx.h"
#include "tfxparam.h"
//#include "trop.h"
#include <math.h>
#include "tpixelutils.h"

//==================================================================

class ChannelMixerFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ChannelMixerFx)

  TRasterFxPort m_input;
  TDoubleParamP m_r_r;
  TDoubleParamP m_g_r;
  TDoubleParamP m_b_r;
  TDoubleParamP m_m_r;
  TDoubleParamP m_r_g;
  TDoubleParamP m_g_g;
  TDoubleParamP m_b_g;
  TDoubleParamP m_m_g;
  TDoubleParamP m_r_b;
  TDoubleParamP m_g_b;
  TDoubleParamP m_b_b;
  TDoubleParamP m_m_b;
  TDoubleParamP m_r_m;
  TDoubleParamP m_g_m;
  TDoubleParamP m_b_m;
  TDoubleParamP m_m_m;

public:
  ChannelMixerFx()
      : m_r_r(1.0)
      , m_g_r(0.0)
      , m_b_r(0.0)
      , m_m_r(0.0)
      , m_r_g(0.0)
      , m_g_g(1.0)
      , m_b_g(0.0)
      , m_m_g(0.0)
      , m_r_b(0.0)
      , m_g_b(0.0)
      , m_b_b(1.0)
      , m_m_b(0.0)
      , m_r_m(0.0)
      , m_g_m(0.0)
      , m_b_m(0.0)
      , m_m_m(1.0)

  {
    addInputPort("Source", m_input);
    bindParam(this, "red_to_red", m_r_r);
    bindParam(this, "green_to_red", m_g_r);
    bindParam(this, "blue_to_red", m_b_r);
    bindParam(this, "matte_to_red", m_m_r);
    bindParam(this, "red_to_green", m_r_g);
    bindParam(this, "green_to_green", m_g_g);
    bindParam(this, "blue_to_green", m_b_g);
    bindParam(this, "matte_to_green", m_m_g);
    bindParam(this, "red_to_blue", m_r_b);
    bindParam(this, "green_to_blue", m_g_b);
    bindParam(this, "blue_to_blue", m_b_b);
    bindParam(this, "matte_to_blue", m_m_b);
    bindParam(this, "red_to_matte", m_r_m);
    bindParam(this, "green_to_matte", m_g_m);
    bindParam(this, "blue_to_matte", m_b_m);
    bindParam(this, "matte_to_matte", m_m_m);
    m_r_r->setValueRange(0, 1);
    m_r_g->setValueRange(0, 1);
    m_r_b->setValueRange(0, 1);
    m_r_m->setValueRange(0, 1);
    m_g_r->setValueRange(0, 1);
    m_g_g->setValueRange(0, 1);
    m_g_b->setValueRange(0, 1);
    m_g_m->setValueRange(0, 1);
    m_b_r->setValueRange(0, 1);
    m_b_g->setValueRange(0, 1);
    m_b_b->setValueRange(0, 1);
    m_b_m->setValueRange(0, 1);
    m_m_r->setValueRange(0, 1);
    m_m_g->setValueRange(0, 1);
    m_m_b->setValueRange(0, 1);
    m_m_m->setValueRange(0, 1);
  }
  ~ChannelMixerFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  };
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

namespace {
template <typename PIXEL, typename CHANNEL_TYPE>
void depremult(PIXEL *pix) {
  if (!pix->m) return;
  double depremult = (double)PIXEL::maxChannelValue / pix->m;
  pix->r           = (CHANNEL_TYPE)(pix->r * depremult);
  pix->g           = (CHANNEL_TYPE)(pix->g * depremult);
  pix->b           = (CHANNEL_TYPE)(pix->b * depremult);
}
}

template <typename PIXEL, typename CHANNEL_TYPE>
void doChannelMixer(TRasterPT<PIXEL> ras, double r_r, double r_g, double r_b,
                    double r_m, double g_r, double g_g, double g_b, double g_m,
                    double b_r, double b_g, double b_b, double b_m, double m_r,
                    double m_g, double m_b, double m_m) {
  double aux = (double)PIXEL::maxChannelValue;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      depremult<PIXEL, CHANNEL_TYPE>(pix);  // if removed, a black line appears
                                            // in the edge of a level (default
                                            // values, not black lines)
      double red   = pix->r * r_r + pix->g * g_r + pix->b * b_r + pix->m * m_r;
      double green = pix->r * r_g + pix->g * g_g + pix->b * b_g + pix->m * m_g;
      double blue  = pix->r * r_b + pix->g * g_b + pix->b * b_b + pix->m * m_b;
      double matte = pix->r * r_m + pix->g * g_m + pix->b * b_m + pix->m * m_m;
      red          = tcrop(red, 0.0, aux);
      green        = tcrop(green, 0.0, aux);
      blue         = tcrop(blue, 0.0, aux);
      matte        = tcrop(matte, 0.0, aux);
      pix->r       = (CHANNEL_TYPE)red;
      pix->g       = (CHANNEL_TYPE)green;
      pix->b       = (CHANNEL_TYPE)blue;
      pix->m       = (CHANNEL_TYPE)matte;
      *pix = premultiply(*pix);  // if removed, a white edged line appears in
                                 // the edge of a level (if m>r, g, b)
      pix++;
    }
  }
  ras->unlock();
}
//------------------------------------------------------------------------------

void ChannelMixerFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double r_r = m_r_r->getValue(frame);
  double r_g = m_r_g->getValue(frame);
  double r_b = m_r_b->getValue(frame);
  double r_m = m_r_m->getValue(frame);
  double g_r = m_g_r->getValue(frame);
  double g_g = m_g_g->getValue(frame);
  double g_b = m_g_b->getValue(frame);
  double g_m = m_g_m->getValue(frame);
  double b_r = m_b_r->getValue(frame);
  double b_g = m_b_g->getValue(frame);
  double b_b = m_b_b->getValue(frame);
  double b_m = m_b_m->getValue(frame);
  double m_r = m_m_r->getValue(frame);
  double m_g = m_m_g->getValue(frame);
  double m_b = m_m_b->getValue(frame);
  double m_m = m_m_m->getValue(frame);

  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    doChannelMixer<TPixel32, UCHAR>(raster32, r_r, r_g, r_b, r_m, g_r, g_g, g_b,
                                    g_m, b_r, b_g, b_b, b_m, m_r, m_g, m_b,
                                    m_m);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doChannelMixer<TPixel64, USHORT>(raster64, r_r, r_g, r_b, r_m, g_r, g_g,
                                       g_b, g_m, b_r, b_g, b_b, b_m, m_r, m_g,
                                       m_b, m_m);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(ChannelMixerFx, "channelMixerFx")
