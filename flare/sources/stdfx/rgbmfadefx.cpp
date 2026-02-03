

#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "tpixelutils.h"
#include "tparamset.h"

//===================================================================

class RGBMFadeFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RGBMFadeFx)
  TRasterFxPort m_input;
  TPixelParamP m_color;
  TDoubleParamP m_intensity;

public:
  RGBMFadeFx() : m_intensity(50.0), m_color(TPixel32::Black) {
    bindParam(this, "color", m_color);
    bindParam(this, "intensity", m_intensity);
    m_intensity->setValueRange(0, 100);
    addInputPort("Source", m_input);
    m_color->enableMatte(false);

    enableComputeInFloat(true);
  }
  ~RGBMFadeFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

template <typename PIXEL>
void doRGBMFade(TRasterPT<PIXEL> &ras, const PIXEL &col, double intensity) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      pix->m        = pix->m;
      double factor = pix->m / (double)PIXEL::maxChannelValue;
      int val;
      val    = troundp(pix->r + intensity * (col.r * factor - pix->r));
      pix->r = (val > PIXEL::maxChannelValue) ? PIXEL::maxChannelValue : val;
      val    = troundp(pix->g + intensity * (col.g * factor - pix->g));
      pix->g = (val > PIXEL::maxChannelValue) ? PIXEL::maxChannelValue : val;
      val    = troundp(pix->b + intensity * (col.b * factor - pix->b));
      pix->b = (val > PIXEL::maxChannelValue) ? PIXEL::maxChannelValue : val;
      /*  qui si faceva il fade anche del matte
pix->r=(UCHAR)(pix->r+intensity*(col.r-pix->r));
pix->g=(UCHAR)(pix->g+intensity*(col.g-pix->g));
pix->b=(UCHAR)(pix->b+intensity*(col.b-pix->b));
pix->m=(UCHAR)(pix->m+intensity*(col.m-pix->m));*/
      ++pix;
    }
  }
  ras->unlock();
}

template <>
void doRGBMFade(TRasterFP &ras, const TPixelF &col, double intensity) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPixelF *pix    = ras->pixels(j);
    TPixelF *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double factor = pix->m;
      pix->r        = pix->r + intensity * (col.r * factor - pix->r);
      pix->g        = pix->g + intensity * (col.g * factor - pix->g);
      pix->b        = pix->b + intensity * (col.b * factor - pix->b);
      ++pix;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

void RGBMFadeFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;
  m_input->compute(tile, frame, ri);

  TPixel32 col = m_color->getValue(frame);
  double min, max, step;
  m_intensity->getValueRange(min, max, step);
  double intensity = tcrop(m_intensity->getValue(frame), min, max) / 100;

  TRaster32P raster32 = tile.getRaster();
  TRaster64P raster64 = tile.getRaster();
  TRasterFP rasterF   = tile.getRaster();

  if (raster32)
    doRGBMFade<TPixel32>(raster32, col, intensity);
  else if (raster64)
    doRGBMFade<TPixel64>(raster64, toPixel64(col), intensity);
  else if (rasterF)
    doRGBMFade<TPixelF>(rasterF, toPixelF(col), intensity);
  else
    throw TException("RGBAFadeFx: unsupported Pixel Type");
}

FX_PLUGIN_IDENTIFIER(RGBMFadeFx, "rgbmFadeFx");
