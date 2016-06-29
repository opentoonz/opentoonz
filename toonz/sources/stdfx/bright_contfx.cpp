

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"

class Bright_ContFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Bright_ContFx)

  TRasterFxPort m_input;
  TDoubleParamP m_bright;
  TDoubleParamP m_contrast;

public:
  Bright_ContFx() : m_bright(0.0), m_contrast(0.0) {
    bindParam(this, "brightness", m_bright);
    bindParam(this, "contrast", m_contrast);
    m_bright->setValueRange(-127, 127);
    m_contrast->setValueRange(-127, 127);
    addInputPort("Source", m_input);
  }

  ~Bright_ContFx(){};

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (!m_input.isConnected()) {
      bBox = TRectD();
      return false;
    }
    return m_input->doGetBBox(frame, bBox, info);
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;
};

//===================================================================

template <typename PIXEL, typename T>
void my_compute_lut(double contrast, double brightness, std::vector<T> &lut) {
  int i;
  double value, nvalue, power;

  int half_maxChannelValue = tfloor(PIXEL::maxChannelValue / 2.0);

  int lutSize = PIXEL::maxChannelValue + 1;
  for (i = 0; i < lutSize; i++) {
    value = i / double(PIXEL::maxChannelValue);
    /*brightness*/
    if (brightness < 0.0)
      value = value * (1.0 + brightness);
    else
      value = value + ((1.0 - value) * brightness);
    /*contrast*/
    if (contrast < 0.0) {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      nvalue = 0.5 * pow(nvalue * 2.0, (double)(1.0 + contrast));
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    } else {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      power = (contrast == 1.0) ? half_maxChannelValue : 1.0 / (1.0 - contrast);
      nvalue = 0.5 * pow(2.0 * nvalue, power);
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    }
    lut[i] = (int)(value * PIXEL::maxChannelValue);
  }
}

template <typename PIXEL, typename CHANNEL_TYPE>
void doBrightnessContrast(TRasterPT<PIXEL> ras, double contrast,
                          double brightness) {
  int lx = ras->getLx();
  int ly = ras->getLy();

  std::vector<CHANNEL_TYPE> lut(PIXEL::maxChannelValue + 1);
  my_compute_lut<PIXEL, CHANNEL_TYPE>(contrast, brightness, lut);

  int j;
  ras->lock();
  for (j = 0; j < ly; j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->m) {
        *pix   = depremultiply(*pix);
        pix->r = lut[pix->r];
        pix->g = lut[pix->g];
        pix->b = lut[pix->b];
        pix->m = pix->m;
        *pix   = premultiply(*pix);
      }
      pix++;
    }
  }
  ras->unlock();
}

void Bright_ContFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  double brightness           = m_bright->getValue(frame) / 127.0;
  double contrast             = m_contrast->getValue(frame) / 127.0;
  if (contrast > 1) contrast  = 1;
  if (contrast < -1) contrast = -1;
  TRaster32P raster32         = tile.getRaster();
  if (raster32)
    doBrightnessContrast<TPixel32, UCHAR>(raster32, contrast, brightness);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doBrightnessContrast<TPixel64, USHORT>(raster64, contrast, brightness);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(Bright_ContFx, "brightContFx")
