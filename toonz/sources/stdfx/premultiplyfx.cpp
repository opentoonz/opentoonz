

#include "stdfx.h"
//#include "tfxparam.h"
#include "trop.h"
//===================================================================

class PremultiplyFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(PremultiplyFx)
  TRasterFxPort m_input;

public:
  PremultiplyFx() { addInputPort("Source", m_input); }
  ~PremultiplyFx(){};

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

void PremultiplyFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TRop::premultiply(tile.getRaster());
}

FX_PLUGIN_IDENTIFIER(PremultiplyFx, "premultiplyFx");
