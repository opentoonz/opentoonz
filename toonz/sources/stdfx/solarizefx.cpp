

#include "texception.h"
#include "tfxparam.h"

#include "stdfx.h"

namespace {
template <typename T, typename PIXEL>
void prepare_lut(double max, int edge, std::vector<T> &lut) {
  double aux = (double)PIXEL::maxChannelValue;
  int i      = 0;
  for (i = 0; i <= edge; i++) {
    lut[i] = (int)((max / edge) * i);
  }
  for (i = edge + 1; i < PIXEL::maxChannelValue + 1; i++) {
    lut[i] = (int)((max / (edge - aux)) * (i - aux));
  }
}
}

//===================================================================

class SolarizeFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(SolarizeFx)

  TRasterFxPort m_input;
  TDoubleParamP m_maximum;
  TDoubleParamP m_edge;

public:
  SolarizeFx() : m_maximum(1.0), m_edge(128.0) {
    bindParam(this, "maximum", m_maximum);
    bindParam(this, "peak_edge", m_edge);
    addInputPort("Source", m_input);
    // m_value->setValueRange(0, std::numeric_limits<double>::max());
    m_maximum->setValueRange(0.0, 10.0);
    m_edge->setValueRange(0.0, 255.0);
  }

  ~SolarizeFx(){};

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
namespace {
template <typename T>
void update_param(T &param, TRaster32P ras) {
  return;
}

template <typename T>
void update_param(T &param, TRaster64P ras) {
  param = param * 257;
  return;
}
}

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doSolarize(TRasterPT<PIXEL> ras, double max, int edge) {
  std::vector<CHANNEL_TYPE> solarize_lut(PIXEL::maxChannelValue + 1);

  update_param(max, ras);
  update_param(edge, ras);

  prepare_lut<CHANNEL_TYPE, PIXEL>(max, edge, solarize_lut);
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

void SolarizeFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double min, max, step;
  m_maximum->getValueRange(min, max, step);
  double maxValue = 128 * tcrop(m_maximum->getValue(frame), min, max);
  m_edge->getValueRange(min, max, step);
  int edge = (int)tcrop(m_edge->getValue(frame), min, max);

  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    doSolarize<TPixel32, UCHAR>(raster32, maxValue, edge);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doSolarize<TPixel64, USHORT>(raster64, maxValue, edge);
    else
      throw TException("SolarizeFx: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(SolarizeFx, "solarizeFx");
