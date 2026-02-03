

//#include "trop.h"
#include "tfxparam.h"
#include "perlinnoise.h"
#include "stdfx.h"
#include "tspectrumparam.h"

class CloudsFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(CloudsFx)
  TIntEnumParamP m_type;
  TDoubleParamP m_size;
  TDoubleParamP m_min;
  TDoubleParamP m_max;
  TDoubleParamP m_evol;
  TSpectrumParamP m_colors;

public:
  CloudsFx()
      : m_type(new TIntEnumParam(PNOISE_CLOUDS, "Clouds"))
      , m_size(100.0)
      , m_min(0.0)
      , m_max(1.0)
      , m_evol(0.0) {
    bindParam(this, "type", m_type);
    m_type->addItem(PNOISE_WOODS, "Marble/Wood");
    bindParam(this, "size", m_size);
    bindParam(this, "min", m_min);
    bindParam(this, "max", m_max);
    bindParam(this, "evolution", m_evol);
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(1, TPixel32::Transparent)};
    m_colors = TSpectrumParamP(colors);
    bindParam(this, "colors", m_colors);
    m_size->setValueRange(0, 200);
    m_min->setValueRange(0, 1.0);
    m_max->setValueRange(0, 1.0);
  }

  ~CloudsFx(){};

  bool doGetBBox(double, TRectD &bbox, const TRenderSettings &info) override {
    bbox = TConsts::infiniteRectD;
    return true;
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }
  // TAffine handledAffine(const TRenderSettings& info, double frame) {return
  // TAffine();}
};

//==================================================================

void CloudsFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  double scale     = sqrt(fabs(ri.m_affine.det()));
  int type         = m_type->getValue();
  double min       = m_min->getValue(frame);
  double max       = m_max->getValue(frame);
  double evolution = m_evol->getValue(frame);
  double size      = m_size->getValue(frame) / ri.m_shrinkX;
  size             = fabs(size);
  if (size < 0.01) size = 0.01;
  TPointD pos = tile.m_pos;

  doClouds(tile.getRaster(), m_colors, pos, evolution, size, min, max, type,
           scale, frame);
}

FX_PLUGIN_IDENTIFIER(CloudsFx, "cloudsFx");
