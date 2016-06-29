

// TnzCore includes
#include "trop.h"

// TnzBase includes
#include "tfxparam.h"

// TnzStdfx includes
#include "stdfx.h"

//--------------------------------------------------------------------------

class DespeckleFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(DespeckleFx)

  TRasterFxPort m_input;
  TIntParamP m_size;
  TIntEnumParamP m_transparenceType;

public:
  DespeckleFx()
      : m_size(1), m_transparenceType(new TIntEnumParam(0, "Transparent Bg")) {
    bindParam(this, "size", m_size);
    bindParam(this, "detect_speckles_on", m_transparenceType);

    m_transparenceType->addItem(1, "White Bg");

    addInputPort("Source", m_input);
    m_size->setValueRange(1, 1000);
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }
};

//-------------------------------------------------------------------

bool DespeckleFx::doGetBBox(double frame, TRectD &bBox,
                            const TRenderSettings &info) {
  if (m_input.isConnected())
    return m_input->doGetBBox(frame, bBox, info);
  else {
    bBox = TRectD();
    return false;
  }
}

//-------------------------------------------------------------------

void DespeckleFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  int size = m_size->getValue();

  TRop::despeckle(tile.getRaster(), size, false,
                  m_transparenceType->getValue() == 1);
}

//------------------------------------------------------------------

void DespeckleFx::doDryCompute(TRectD &rect, double frame,
                               const TRenderSettings &info) {
  if (!m_input.isConnected()) return;

  m_input->dryCompute(rect, frame, info);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(DespeckleFx, "despeckleFx")
