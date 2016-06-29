

#include "trop.h"
#include "tfxparam.h"
#include "texception.h"
#include "stdfx.h"

//===================================================================

class RGBMScaleFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RGBMScaleFx)
  TRasterFxPort m_input;
  TDoubleParamP m_red;
  TDoubleParamP m_green;
  TDoubleParamP m_blue;
  TDoubleParamP m_matte;

public:
  RGBMScaleFx() : m_red(100.0), m_green(100.0), m_blue(100.0), m_matte(100.0) {
    bindParam(this, "red", m_red);
    bindParam(this, "green", m_green);
    bindParam(this, "blue", m_blue);
    bindParam(this, "matte", m_matte);
    m_red->setValueRange(0, (std::numeric_limits<double>::max)());
    m_green->setValueRange(0, (std::numeric_limits<double>::max)());
    m_blue->setValueRange(0, (std::numeric_limits<double>::max)());
    m_matte->setValueRange(0, (std::numeric_limits<double>::max)());
    addInputPort("Source", m_input);
  }
  ~RGBMScaleFx(){};

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

void RGBMScaleFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;
  m_input->compute(tile, frame, ri);

  double red   = m_red->getValue(frame) / 100;
  double green = m_green->getValue(frame) / 100;
  double blue  = m_blue->getValue(frame) / 100;
  double matte = m_matte->getValue(frame) / 100;

  // TRaster32P raster32 = tile.getRaster();
  // assert(raster32); // per ora gestisco solo i Raster32

  TRop::rgbmScale(tile.getRaster(), tile.getRaster(), red, green, blue, matte);
}

FX_PLUGIN_IDENTIFIER(RGBMScaleFx, "rgbmScaleFx");
