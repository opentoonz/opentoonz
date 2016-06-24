

//#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"

#include "tparamset.h"

class RGBKeyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RGBKeyFx)

  TRasterFxPort m_input;
  TPixelParamP m_color;
  TDoubleParamP m_rrange;
  TDoubleParamP m_grange;
  TDoubleParamP m_brange;
  TBoolParamP m_gender;

public:
  RGBKeyFx()
      : m_color(TPixel32::Black)
      , m_rrange(0.0)
      , m_grange(0.0)
      , m_brange(0.0)
      , m_gender(false) {
    bindParam(this, "color", m_color);
    bindParam(this, "r_range", m_rrange);
    bindParam(this, "g_range", m_grange);
    bindParam(this, "b_range", m_brange);
    bindParam(this, "invert", m_gender);
    m_rrange->setValueRange(0.0, 255.0);
    m_grange->setValueRange(0.0, 255.0);
    m_brange->setValueRange(0.0, 255.0);
    addInputPort("Source", m_input);
  }

  ~RGBKeyFx(){};

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

namespace {
void update_param(int &param, TRaster32P ras) { return; }

void update_param(int &param, TRaster64P ras) {
  param = param * 257;
  return;
}
}

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doRGBKey(TRasterPT<PIXEL> ras, int highR, int highG, int highB, int lowR,
              int lowG, int lowB, bool gender) {
  update_param(highR, ras);
  update_param(highG, ras);
  update_param(highB, ras);
  update_param(lowR, ras);
  update_param(lowG, ras);
  update_param(lowB, ras);

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      bool condition = pix->r >= lowR && pix->r <= highR && pix->g >= lowG &&
                       pix->g <= highG && pix->b >= lowB && pix->b <= highB;
      if (condition != gender) *pix = PIXEL::Transparent;
      pix++;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

void RGBKeyFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  int r_range          = (int)m_rrange->getValue(frame);
  int g_range          = (int)m_grange->getValue(frame);
  int b_range          = (int)m_brange->getValue(frame);
  bool gender          = (int)m_gender->getValue();
  const TPixel32 Color = m_color->getPremultipliedValue(frame);
  TRaster32P raster32  = tile.getRaster();

  int lowR  = std::max(0, Color.r - r_range);
  int highR = std::min(255, Color.r + r_range);
  int lowG  = std::max(0, Color.g - g_range);
  int highG = std::min(255, Color.g + g_range);
  int lowB  = std::max(0, Color.b - b_range);
  int highB = std::min(255, Color.b + b_range);

  if (raster32)
    doRGBKey<TPixel32, UCHAR>(raster32, highR, highG, highB, lowR, lowG, lowB,
                              gender);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doRGBKey<TPixel64, USHORT>(raster64, highR, highG, highB, lowR, lowG,
                                 lowB, gender);
    else
      throw TException("RGBKeyFx: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(RGBKeyFx, "rgbKeyFx")
