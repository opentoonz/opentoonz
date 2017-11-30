#include "texception.h"
#include "tfxparam.h"
#include "trop.h"
#include "stdfx.h"
#include "trasterfx.h"

//-------------------------------------------------------------------

class NothingFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(NothingFx)

  TRasterFxPort m_input;

public:
  NothingFx() {
    addInputPort("Source", m_input);
  }

  ~NothingFx(){};

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

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
	  return true;
  }
};

FX_PLUGIN_IDENTIFIER(NothingFx, "nothingFx")

//-------------------------------------------------------------------

void NothingFx::transform(double frame, int port, const TRectD &rectOnOutput,
                       const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                       TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;
  rectOnInput = rectOnOutput;
  return;
}

//-------------------------------------------------------------------

int NothingFx::getMemoryRequirement(const TRectD &rect, double frame,
                                 const TRenderSettings &info) {
	return 0;
}

//-------------------------------------------------------------------

void NothingFx::doCompute(TTile &tile, double frame,
                       const TRenderSettings &renderSettings) {
  if (!m_input.isConnected()) return;
  m_input->compute(tile, frame, renderSettings);
}
