

#include "stdfx.h"
//#include "tsystem.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "tparamset.h"
#include "trop.h"

//===================================================================

class AdjustLevelsFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(AdjustLevelsFx)

  TRasterFxPort m_input;
  TRangeParamP m_in_rgb;
  TRangeParamP m_in_r;
  TRangeParamP m_in_g;
  TRangeParamP m_in_b;
  TRangeParamP m_in_m;
  TRangeParamP m_out_rgb;
  TRangeParamP m_out_r;
  TRangeParamP m_out_g;
  TRangeParamP m_out_b;
  TRangeParamP m_out_m;
  TDoubleParamP m_gamma_rgb;
  TDoubleParamP m_gamma_r;
  TDoubleParamP m_gamma_g;
  TDoubleParamP m_gamma_b;
  TDoubleParamP m_gamma_m;

public:
  AdjustLevelsFx()

      : m_in_rgb(DoublePair(0.0, 255.0))
      , m_in_r(DoublePair(0.0, 255.0))
      , m_in_g(DoublePair(0.0, 255.0))
      , m_in_b(DoublePair(0.0, 255.0))
      , m_in_m(DoublePair(0.0, 255.0))
      , m_out_rgb(DoublePair(0.0, 255.0))
      , m_out_r(DoublePair(0.0, 255.0))
      , m_out_g(DoublePair(0.0, 255.0))
      , m_out_b(DoublePair(0.0, 255.0))
      , m_out_m(DoublePair(0.0, 255.0))
      , m_gamma_rgb(1.0)
      , m_gamma_r(1.0)
      , m_gamma_g(1.0)
      , m_gamma_b(1.0)
      , m_gamma_m(1.0) {
    bindParam(this, "in_rgb", m_in_rgb);
    bindParam(this, "in_r", m_in_r);
    bindParam(this, "in_g", m_in_g);
    bindParam(this, "in_b", m_in_b);
    bindParam(this, "in_m", m_in_m);
    bindParam(this, "out_rgb", m_out_rgb);
    bindParam(this, "out_r", m_out_r);
    bindParam(this, "out_g", m_out_g);
    bindParam(this, "out_b", m_out_b);
    bindParam(this, "out_m", m_out_m);
    bindParam(this, "gamma_rgb", m_gamma_rgb);
    bindParam(this, "gamma_r", m_gamma_r);
    bindParam(this, "gamma_g", m_gamma_g);
    bindParam(this, "gamma_b", m_gamma_b);
    bindParam(this, "gamma_m", m_gamma_m);
    addInputPort("Source", m_input);

    m_in_rgb->getMin()->setValueRange(0, 255);
    m_in_rgb->getMax()->setValueRange(0, 255);
    m_in_r->getMin()->setValueRange(0, 255);
    m_in_r->getMax()->setValueRange(0, 255);
    m_in_g->getMin()->setValueRange(0, 255);
    m_in_g->getMax()->setValueRange(0, 255);
    m_in_b->getMin()->setValueRange(0, 255);
    m_in_b->getMax()->setValueRange(0, 255);
    m_in_m->getMin()->setValueRange(0, 255);
    m_in_m->getMax()->setValueRange(0, 255);
    m_out_rgb->getMin()->setValueRange(0, 255);
    m_out_rgb->getMax()->setValueRange(0, 255);
    m_out_r->getMin()->setValueRange(0, 255);
    m_out_r->getMax()->setValueRange(0, 255);
    m_out_g->getMin()->setValueRange(0, 255);
    m_out_g->getMax()->setValueRange(0, 255);
    m_out_b->getMin()->setValueRange(0, 255);
    m_out_b->getMax()->setValueRange(0, 255);
    m_out_m->getMin()->setValueRange(0, 255);
    m_out_m->getMax()->setValueRange(0, 255);
    m_gamma_rgb->setValueRange(0.0, 200.0);
    m_gamma_r->setValueRange(0.0, 200.0);
    m_gamma_g->setValueRange(0.0, 200.0);
    m_gamma_b->setValueRange(0.0, 200.0);
    m_gamma_m->setValueRange(0.0, 200.0);
  }

  ~AdjustLevelsFx(){};

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

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
};

//------------------------------------------------------------------

void AdjustLevelsFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double gamma_rgb = m_gamma_rgb->getValue(frame);
  double gamma_r   = m_gamma_r->getValue(frame);
  double gamma_g   = m_gamma_g->getValue(frame);
  double gamma_b   = m_gamma_b->getValue(frame);
  double gamma_m   = m_gamma_m->getValue(frame);

  DoublePair vin_rgb  = m_in_rgb->getValue(frame);
  DoublePair vin_r    = m_in_r->getValue(frame);
  DoublePair vin_g    = m_in_g->getValue(frame);
  DoublePair vin_b    = m_in_b->getValue(frame);
  DoublePair vin_m    = m_in_m->getValue(frame);
  DoublePair vout_rgb = m_out_rgb->getValue(frame);
  DoublePair vout_r   = m_out_r->getValue(frame);
  DoublePair vout_g   = m_out_g->getValue(frame);
  DoublePair vout_b   = m_out_b->getValue(frame);
  DoublePair vout_m   = m_out_m->getValue(frame);

  int in0[5], in1[5], out0[5], out1[5];

  in0[0] = vin_rgb.first, in1[0] = vin_rgb.second;
  out0[0] = vout_rgb.first, out1[0] = vout_rgb.second;

  in0[1] = vin_r.first, in1[1] = vin_r.second;
  out0[1] = vout_r.first, out1[1] = vout_r.second;

  in0[2] = vin_g.first, in1[2] = vin_g.second;
  out0[2] = vout_g.first, out1[2] = vout_g.second;

  in0[3] = vin_b.first, in1[3] = vin_b.second;
  out0[3] = vout_b.first, out1[3] = vout_b.second;

  in0[4] = vin_m.first, in1[4] = vin_m.second;
  out0[4] = vout_m.first, out1[4] = vout_m.second;

  TRasterP ras = tile.getRaster();

  TRop::rgbmAdjust(ras, ras, in0, in1, out0, out1);
  TRop::gammaCorrect(ras, gamma_rgb);
  TRop::gammaCorrectRGBM(ras, gamma_r, gamma_g, gamma_b, gamma_m);
}

FX_PLUGIN_IDENTIFIER(AdjustLevelsFx, "adjustLevelsFx");
