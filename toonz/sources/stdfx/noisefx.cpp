

#include "trandom.h"
#include "tfxparam.h"
#include "stdfx.h"

class NoiseFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(NoiseFx)

  TRasterFxPort m_input;
  TDoubleParamP m_value;
  TBoolParamP m_Red;
  TBoolParamP m_Green;
  TBoolParamP m_Blue;
  TBoolParamP m_BW;
  TBoolParamP m_Animate;

public:
  NoiseFx()
      : m_value(100.0)
      , m_Red(1.0)
      , m_Green(1.0)
      , m_Blue(1.0)
      , m_BW(0.0)
      , m_Animate(0.0) {
    bindParam(this, "Intensity", m_value);
    bindParam(this, "Red", m_Red);
    bindParam(this, "Green", m_Green);
    bindParam(this, "Blue", m_Blue);
    bindParam(this, "Black_White", m_BW);
    bindParam(this, "Animate", m_Animate);
    addInputPort("Source", m_input);
    m_value->setValueRange(0, (std::numeric_limits<double>::max)());
  }

  ~NoiseFx(){};

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
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//------------------------------------------------------------------------------

namespace {
template <class PIXEL>
class RayleighNoise {
  std::vector<double> noise_buf;

public:
  RayleighNoise(double sigma, int seed = 0) {
    noise_buf.resize(PIXEL::maxChannelValue + 1);
    TRandom random(seed);
    sigma = log(sigma * 0.07 + 1);
    if (PIXEL::maxChannelValue == 255)
      sigma = 2.0 * sigma * sigma * sigma * sigma;
    else
      sigma = 257.0 * 2.0 * sigma * sigma * sigma * sigma;
    for (int i = 0; i < PIXEL::maxChannelValue + 1; i++) {
      noise_buf[i] = sigma * sqrt(-log(1.0 - random.getFloat())) *
                     cos(M_PI * (2.0 * random.getFloat() - 1.0));
    }
  }
  double get_value(int index) {
    tcrop(index, 0, PIXEL::maxChannelValue);
    return noise_buf[index];
  }
};
}

template <typename PIXEL, typename PIXELGRAY, typename CHANNEL_TYPE>
void doNoise(TRasterPT<PIXEL> &ras, double sigma, bool bw, bool red, bool green,
             bool blue, bool animate, double frame) {
  RayleighNoise<PIXEL> noise(sigma);

  TRandom random;
  if (animate) random.setSeed(frame);
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (bw) {
        double index;
        double value;
        index = random.getFloat() * pix->m;  // PIXEL::maxChannelValue;
        value = noise.get_value(tfloor(index));

        // value =
        // (value<pix->m)?((value>0)?(int)value:0):PIXEL::maxChannelValue;
        // if(value)
        //{
        int reference;
        reference = PIXELGRAY::from(*pix).value;
        pix->r    = tcrop<int>(value + reference, 0, pix->m);
        pix->g    = tcrop<int>(value + reference, 0, pix->m);
        pix->b    = tcrop<int>(value + reference, 0, pix->m);
        //}
      } else {
        double r, g, b, index;
        if (red) {
          index  = random.getFloat() * pix->m;  // PIXEL::maxChannelValue;
          r      = noise.get_value(tfloor(index)) + pix->r;
          pix->r = tcrop<int>(r, 0, pix->m);
        }
        if (green) {
          index  = random.getFloat() * pix->m;  // PIXEL::maxChannelValue;
          g      = noise.get_value(tfloor(index)) + pix->g;
          pix->g = tcrop<int>(g, 0, pix->m);
        }
        if (blue) {
          index  = random.getFloat() * pix->m;  // PIXEL::maxChannelValue;
          b      = noise.get_value(tfloor(index)) + pix->b;
          pix->b = tcrop<int>(b, 0, pix->m);
        }
      }
      pix++;
    }
  }

  ras->unlock();
}

//-------------------------------------------------------------------

void NoiseFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double sigma = m_value->getValue(frame);
  bool bw      = m_BW->getValue();
  bool red     = m_Red->getValue();
  bool green   = m_Green->getValue();
  bool blue    = m_Blue->getValue();
  bool animate = m_Animate->getValue();
  if (sigma == 0.0) return;

  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    doNoise<TPixel32, TPixelGR8, UCHAR>(raster32, sigma, bw, red, green, blue,
                                        animate, frame);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doNoise<TPixel64, TPixelGR16, USHORT>(raster64, sigma, bw, red, green,
                                            blue, animate, frame);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}
//------------------------------------------------------------------

std::string NoiseFx::getAlias(double frame, const TRenderSettings &info) const {
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

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(NoiseFx, "noiseFx")
