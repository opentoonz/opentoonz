

//#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"
#include "hsvutil.h"

class HSVKeyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(HSVKeyFx)

  TRasterFxPort m_input;
  TDoubleParamP m_h;
  TDoubleParamP m_s;
  TDoubleParamP m_v;
  TDoubleParamP m_hrange;
  TDoubleParamP m_srange;
  TDoubleParamP m_vrange;
  TBoolParamP m_gender;

public:
  HSVKeyFx()
      : m_h(0.0)
      , m_s(0.0)
      , m_v(0.0)
      , m_hrange(0.0)
      , m_srange(0.0)
      , m_vrange(0.0)
      , m_gender(false) {
    bindParam(this, "h", m_h);
    bindParam(this, "s", m_s);
    bindParam(this, "v", m_v);
    bindParam(this, "h_range", m_hrange);
    bindParam(this, "s_range", m_srange);
    bindParam(this, "v_range", m_vrange);
    bindParam(this, "invert", m_gender);
    m_h->setValueRange(0.0, 360.0);
    m_s->setValueRange(0.0, 1.0);
    m_v->setValueRange(0.0, 1.0);
    m_hrange->setValueRange(0.0, 360.0);
    m_srange->setValueRange(0.0, 1.0);
    m_vrange->setValueRange(0.0, 1.0);
    addInputPort("Source", m_input);
  }

  ~HSVKeyFx(){};

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

//------------------------------------------------------------------------------

template <typename PIXEL>
void doHSVKey(const TRasterPT<PIXEL> &ras, double lowH, double highH,
              double lowS, double highS, double lowV, double highV,
              bool gender) {
  double aux = (double)PIXEL::maxChannelValue;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double h, s, v;
      OLDRGB2HSV(pix->r / aux, pix->g / aux, pix->b / aux, &h, &s, &v);
      bool condition = h >= lowH && h <= highH && s >= lowS && s <= highS &&
                       v >= lowV && v <= highV;
      if (condition != gender) *pix = PIXEL::Transparent;
      pix++;
    }
  }
  ras->unlock();
}
//------------------------------------------------------------------------------

void HSVKeyFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double h_ref = m_h->getValue(frame);
  double s_ref = m_s->getValue(frame);
  double v_ref = m_v->getValue(frame);

  double h_range = m_hrange->getValue(frame);
  double s_range = m_srange->getValue(frame);
  double v_range = m_vrange->getValue(frame);
  bool gender    = (int)m_gender->getValue();

  double lowH  = std::max(0.0, h_ref - h_range);
  double highH = std::min(360.0, h_ref + h_range);
  double lowS  = std::max(0.0, s_ref - s_range);
  double highS = std::min(1.0, s_ref + s_range);
  double lowV  = std::max(0.0, v_ref - v_range);
  double highV = std::min(1.0, v_ref + v_range);

  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doHSVKey<TPixel32>(raster32, lowH, highH, lowS, highS, lowV, highV, gender);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doHSVKey<TPixel64>(raster64, lowH, highH, lowS, highS, lowV, highV,
                         gender);
    else
      throw TException("HSVKey: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(HSVKeyFx, "hsvKeyFx")
