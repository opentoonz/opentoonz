

#include "stdfx.h"
#include "tfxparam.h"

namespace {
template <typename T, typename PIXEL>
void prepare_lut(int levels, std::vector<T> &lut) {
  int i, j;
  int valuestep = PIXEL::maxChannelValue / (levels - 1);
  int step      = PIXEL::maxChannelValue / levels;
  for (j = 0; j < levels; j++)
    for (i = 0; i <= step; i++) lut[i + j * step] = j * valuestep;
}
}

//===================================================================

class PosterizeFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(PosterizeFx)

  TRasterFxPort m_input;
  TDoubleParamP m_levels;

public:
  PosterizeFx() : m_levels(7.0) {
    bindParam(this, "levels", m_levels);
    addInputPort("Source", m_input);
    m_levels->setValueRange(2.0, 10.0);
  }

  ~PosterizeFx(){};

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

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doPosterize(TRasterPT<PIXEL> ras, int levels) {
  std::vector<CHANNEL_TYPE> solarize_lut(PIXEL::maxChannelValue + 1);

  prepare_lut<CHANNEL_TYPE, PIXEL>(levels, solarize_lut);
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      pix->r = (CHANNEL_TYPE)(solarize_lut[(int)(pix->r)]);
      pix->g = (CHANNEL_TYPE)(solarize_lut[(int)(pix->g)]);
      pix->b = (CHANNEL_TYPE)(solarize_lut[(int)(pix->b)]);
      pix++;
    }
  }
  ras->unlock();
}
//-------------------------------------------------------------------

void PosterizeFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  int levels          = (int)m_levels->getValue(frame);
  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    doPosterize<TPixel32, UCHAR>(raster32, levels);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doPosterize<TPixel64, USHORT>(raster64, levels);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(PosterizeFx, "posterizeFx");
