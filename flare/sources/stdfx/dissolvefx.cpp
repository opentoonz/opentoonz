

#include "texception.h"
#include "tfxparam.h"

#include "stdfx.h"
#include "trandom.h"

//===================================================================

class DissolveFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(DissolveFx)

  TRasterFxPort m_input;
  TDoubleParamP m_intensity;

public:
  DissolveFx() : m_intensity(30.0) {
    bindParam(this, "intensity", m_intensity);
    addInputPort("Source", m_input);
    m_intensity->setValueRange(0.0, 100.0);
  }

  ~DissolveFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      return ret;
    } else {
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
void doDissolve(const TRasterPT<PIXEL> &ras, const double intensity,
                TRandom &rnd) {
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m) {
        double data;
        data = rnd.getFloat();
        if (data < intensity) {
          pix->r = pix->g = pix->b = pix->m = 0;
        }
      }
      pix++;
    }
  }
  ras->unlock();
}
//-------------------------------------------------------------------

void DissolveFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TRandom rnd;
  double intensity = m_intensity->getValue(frame) / 100.0;
  ;

  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doDissolve<TPixel32>(raster32, intensity, rnd);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doDissolve<TPixel64>(raster64, intensity, rnd);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(DissolveFx, "dissolveFx");
