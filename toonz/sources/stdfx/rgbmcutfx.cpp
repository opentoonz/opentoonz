

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"

#include "tparamset.h"
#include "globalcontrollablefx.h"

class RGBMCutFx final : public GlobalControllableFx {
  FX_PLUGIN_DECLARATION(RGBMCutFx)

  TRasterFxPort m_input;
  TRangeParamP m_r_range;
  TRangeParamP m_g_range;
  TRangeParamP m_b_range;
  TRangeParamP m_m_range;

public:
  RGBMCutFx()
      : m_r_range(DoublePair(0., 255.))
      , m_g_range(DoublePair(0., 255.))
      , m_b_range(DoublePair(0., 255.))
      , m_m_range(DoublePair(0., 255.)) {
    bindParam(this, "r_range", m_r_range);
    bindParam(this, "g_range", m_g_range);
    bindParam(this, "b_range", m_b_range);
    bindParam(this, "m_range", m_m_range);
    m_r_range->getMin()->setValueRange(0., 255.);
    m_g_range->getMin()->setValueRange(0., 255.);
    m_b_range->getMin()->setValueRange(0., 255.);
    m_m_range->getMin()->setValueRange(0., 255.);
    m_r_range->getMax()->setValueRange(0., 255.);
    m_g_range->getMax()->setValueRange(0., 255.);
    m_b_range->getMax()->setValueRange(0., 255.);
    m_m_range->getMax()->setValueRange(0., 255.);

    addInputPort("Source", m_input);
  }

  ~RGBMCutFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    return m_input.getFx() && m_input->doGetBBox(frame, bBox, info);
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

namespace {
void update_param(double &param, TRaster32P ras) { return; }

void update_param(double &param, TRaster64P ras) {
  param = param * 257;
  return;
}
}  // namespace

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doRGBMCut(TRasterPT<PIXEL> ras, double hi_m, double hi_r, double hi_g,
               double hi_b, double lo_m, double lo_r, double lo_g,
               double lo_b) {
  update_param(hi_m, ras);
  update_param(hi_r, ras);
  update_param(hi_g, ras);
  update_param(hi_b, ras);
  update_param(lo_m, ras);
  update_param(lo_r, ras);
  update_param(lo_g, ras);
  update_param(lo_b, ras);

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m != 0) {
        *pix = depremultiply(*pix);
        if (lo_m == 0)
          pix->m = tcrop((int)pix->m, (int)lo_m, (int)hi_m);
        else
          pix->m = (pix->m) ? tcrop((int)pix->m, (int)lo_m, (int)hi_m) : 0;

        pix->r = tcrop((int)pix->r, (int)lo_r, (int)hi_r);
        pix->g = tcrop((int)pix->g, (int)lo_g, (int)hi_g);
        pix->b = tcrop((int)pix->b, (int)lo_b, (int)hi_b);
        *pix   = premultiply(*pix);
      }
      pix++;
    }
  }
  ras->unlock();
}

//==============================================================================

void RGBMCutFx::doCompute(TTile &tile, double frame,
                          const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double hi_m = m_m_range->getMax()->getValue(frame);
  double hi_r = m_r_range->getMax()->getValue(frame);
  double hi_g = m_g_range->getMax()->getValue(frame);
  double hi_b = m_b_range->getMax()->getValue(frame);
  double lo_m = m_m_range->getMin()->getValue(frame);
  double lo_r = m_r_range->getMin()->getValue(frame);
  double lo_g = m_g_range->getMin()->getValue(frame);
  double lo_b = m_b_range->getMin()->getValue(frame);

  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doRGBMCut<TPixel32, UCHAR>(raster32, hi_m, hi_r, hi_g, hi_b, lo_m, lo_r,
                               lo_g, lo_b);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doRGBMCut<TPixel64, USHORT>(raster64, hi_m, hi_r, hi_g, hi_b, lo_m, lo_r,
                                  lo_g, lo_b);
    else
      throw TException("RGBACutFx: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(RGBMCutFx, "rgbmCutFx")
