

#include "texception.h"
//#include "tfxparam.h"
#include "stdfx.h"

namespace {
TPixel32 unmultiply(const TPixel32 &pix) {
  if (!pix.m) return pix;
  double depremult = 255.0 / pix.m;
  return TPixel32((UCHAR)(pix.r * depremult), (UCHAR)(pix.g * depremult),
                  (UCHAR)(pix.b * depremult), pix.m);
}
}

//===================================================================

class UnmultiplyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(UnmultiplyFx)
  TRasterFxPort m_input;

public:
  UnmultiplyFx() { addInputPort("Source", m_input); }
  ~UnmultiplyFx(){};

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

//------------------------------------------------------------------------------

void UnmultiplyFx::doCompute(TTile &tile, double frame,
                             const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TRaster32P raster32 = tile.getRaster();
  assert(raster32);  // per ora gestisco solo i Raster32

  int j;
  raster32->lock();
  for (j = 0; j < raster32->getLy(); j++) {
    TPixel32 *pix    = raster32->pixels(j);
    TPixel32 *endPix = pix + raster32->getLx();
    while (pix < endPix) {
      if (pix->m) {
        double depremult = 255.0 / pix->m;
        pix->r           = (UCHAR)(pix->r * depremult);
        pix->g           = (UCHAR)(pix->g * depremult);
        pix->b           = (UCHAR)(pix->b * depremult);
      }

      //*pix=unmultiply(*pix);
      pix++;
    }
  }
  raster32->unlock();
}

FX_PLUGIN_IDENTIFIER(UnmultiplyFx, "unmultiplyFx");
