

// #include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"

#include "tparamset.h"
#include "globalcontrollablefx.h"
#include "tpixelutils.h"

class RGBKeyFx final : public GlobalControllableFx {
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
    enableComputeInFloat(true);
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
    return false;
  }
};

//-------------------------------------------------------------------

template <typename PIXEL>
void doRGBKey(TRasterPT<PIXEL> ras, PIXEL highColor, PIXEL lowColor,
              bool gender) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      bool condition = pix->r >= lowColor.r && pix->r <= highColor.r &&
                       pix->g >= lowColor.g && pix->g <= highColor.g &&
                       pix->b >= lowColor.b && pix->b <= highColor.b;
      if (condition != gender) *pix = PIXEL::Transparent;
      pix++;
    }
  }
  ras->unlock();
}

template <>
void doRGBKey(TRasterFP ras, TPixelF highColor, TPixelF lowColor, bool gender) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPixelF *pix    = ras->pixels(j);
    TPixelF *endPix = pix + ras->getLx();
    while (pix < endPix) {
      // clamp 0.f to 1.f in case computing HDR
      float clampedR = std::min(1.f, std::max(0.f, pix->r));
      float clampedG = std::min(1.f, std::max(0.f, pix->g));
      float clampedB = std::min(1.f, std::max(0.f, pix->b));

      bool condition = clampedR >= lowColor.r && clampedR <= highColor.r &&
                       clampedG >= lowColor.g && clampedG <= highColor.g &&
                       clampedB >= lowColor.b && clampedB <= highColor.b;
      if (condition != gender) *pix = TPixelF::Transparent;
      pix++;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

void RGBKeyFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double r_range = m_rrange->getValue(frame) / 255.0;
  double g_range = m_grange->getValue(frame) / 255.0;
  double b_range = m_brange->getValue(frame) / 255.0;
  bool gender    = m_gender->getValue();

  const TPixelF color = premultiply(toPixelF(m_color->getValueD(frame)));

  TPixelF lowColor(color.r - r_range, color.g - g_range, color.b - b_range);
  TPixelF highColor(color.r + r_range, color.g + g_range, color.b + b_range);

  // currently the tile should always be nonlinear
  assert(!tile.getRaster()->isLinear());
  if (tile.getRaster()->isLinear()) {
    lowColor  = toLinear(lowColor, ri.m_colorSpaceGamma);
    highColor = toLinear(highColor, ri.m_colorSpaceGamma);
  }

  TRaster32P raster32 = tile.getRaster();
  TRaster64P raster64 = tile.getRaster();
  TRasterFP rasterF   = tile.getRaster();
  if (raster32)
    doRGBKey<TPixel32>(raster32, toPixel32(highColor), toPixel32(lowColor),
                       gender);
  else if (raster64)
    doRGBKey<TPixel64>(raster64, toPixel64(highColor), toPixel64(lowColor),
                       gender);
  else if (rasterF)
    doRGBKey<TPixelF>(rasterF, highColor, lowColor, gender);
  else
    throw TException("RGBKeyFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(RGBKeyFx, "rgbKeyFx")
