

#include "stdfx.h"
#include "tfxparam.h"
#include "trop.h"

class GammaFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(GammaFx)

  TRasterFxPort m_input;
  TDoubleParamP m_gamma;

public:
  GammaFx() : m_gamma(1.0) {
    bindParam(this, "value", m_gamma);
    addInputPort("Source", m_input);
    // m_gamma->setValueRange(0, std::numeric_limits<double>::max());
    m_gamma->setValueRange(0.0, 200.0);
  }

  ~GammaFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//-------------------------------------------------------------------

void GammaFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double gamma = m_gamma->getValue(frame);

  if (gamma == 0.0) gamma = 0.01;
  TRop::gammaCorrect(tile.getRaster(), gamma);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(GammaFx, "gammaFx")
