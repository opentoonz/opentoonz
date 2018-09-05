

#include "stdfx.h"
#include "tfxparam.h"
#include "tspectrumparam.h"

class MultiToneFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(MultiToneFx)

  TRasterFxPort m_input;
  TSpectrumParamP m_colors;

public:
  MultiToneFx() {
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.5, TPixel32::Yellow),
        TSpectrum::ColorKey(1, TPixel32::Red)};
    m_colors = TSpectrumParamP(colors);
    bool ret = m_colors->isKeyframe(0);
    bindParam(this, "colors", m_colors);

    ret = getParams()->getParam(0)->isKeyframe(0);
    addInputPort("Source", m_input);
  }

  ~MultiToneFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

template <typename PIXEL, typename PIXELGRAY, typename CHANNEL_TYPE>
void doMultiTone(const TRasterPT<PIXEL> &ras,
                 const TSpectrumT<PIXEL> &spectrum) {
  double aux = (double)PIXEL::maxChannelValue;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m) {
        CHANNEL_TYPE reference;
        double s;
        reference = PIXELGRAY::from(*pix).value;
        s         = reference / aux;
        if (pix->m == aux) {
          *pix = spectrum.getPremultipliedValue(s);
        } else {
          PIXEL tmp = spectrum.getPremultipliedValue(s);
          s         = pix->m / aux;
          tmp.r     = (CHANNEL_TYPE)(tmp.r * s);
          tmp.g     = (CHANNEL_TYPE)(tmp.g * s);
          tmp.b     = (CHANNEL_TYPE)(tmp.b * s);
          tmp.m     = (CHANNEL_TYPE)(tmp.m * s);
          *pix      = tmp;
        }
      }
      pix++;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

void MultiToneFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  if (TRaster32P raster32 = tile.getRaster())
    doMultiTone<TPixel32, TPixelGR8, UCHAR>(raster32,
                                            m_colors->getValue(frame));
  else if (TRaster64P raster64 = tile.getRaster())
    doMultiTone<TPixel64, TPixelGR16, USHORT>(raster64,
                                              m_colors->getValue64(frame));
  else
    throw TException("MultiToneFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(MultiToneFx, "multiToneFx")
