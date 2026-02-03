

#include "texception.h"
#include "tfxparam.h"

#include "stdfx.h"
#include "trandom.h"

//===================================================================

class SaltPepperNoiseFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(SaltPepperNoiseFx)

  TRasterFxPort m_input;
  TDoubleParamP m_Intensity;
  TBoolParamP m_Animate;

public:
  SaltPepperNoiseFx() : m_Intensity(30.0), m_Animate(0.0) {
    bindParam(this, "Intensity", m_Intensity);
    bindParam(this, "Animate", m_Animate);
    addInputPort("Source", m_input);
    m_Intensity->setValueRange(0.0, 100.0);
  }

  ~SaltPepperNoiseFx(){};

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
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

template <typename PIXEL>
void doSaltPepperNoise(const TRasterPT<PIXEL> &ras, const double intensity,
                       TRandom &rnd, bool animate, double frame) {
  if (animate) rnd.setSeed(frame);
  double data  = 0.5 * intensity;
  double data1 = data + 0.5;
  double data2 = 0.5 - data;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (pix->m) {
        data = rnd.getFloat();
        if (data >= 0.5 && data < data1) {
          pix->r = pix->g = pix->b = 0;
        } else if (data >= data2 && data < 0.5)
          pix->r = pix->g = pix->b = pix->m;
      }
      pix++;
    }
  }
  ras->unlock();
}
//-------------------------------------------------------------------

void SaltPepperNoiseFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TRandom rnd;
  double intensity = m_Intensity->getValue(frame) / 100.0;
  ;
  bool animate        = m_Animate->getValue();
  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doSaltPepperNoise<TPixel32>(raster32, intensity, rnd, animate, frame);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doSaltPepperNoise<TPixel64>(raster64, intensity, rnd, animate, frame);
    else
      throw TException("SaltPepperNoiseFx: unsupported Pixel Type");
  }
}
//------------------------------------------------------------------

std::string SaltPepperNoiseFx::getAlias(double frame,
                                        const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  int i = 0;

  for (i = 0; i < getInputPortCount(); i++) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  bool addframe = 0;
  std::string paramalias("");
  for (i = 0; i < getParams()->getParamCount(); i++) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
    if (param->getName() == "Animate" && param->getValueAlias(frame, 0) == "1")
      addframe = true;
  }

  if (addframe) alias += std::to_string(frame) + ",";
  alias += paramalias + "]";

  return alias;
}

FX_PLUGIN_IDENTIFIER(SaltPepperNoiseFx, "saltpepperNoiseFx");
