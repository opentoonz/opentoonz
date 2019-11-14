

#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "tspectrumparam.h"
#include "tparamuiconcept.h"

class DiamondGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(DiamondGradientFx)
  TSpectrumParamP m_colors;
  TDoubleParamP m_size;

public:
  DiamondGradientFx() : m_size(100.0) {
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.2, TPixel32::Yellow),
        TSpectrum::ColorKey(0.4, TPixel32::Blue),
        TSpectrum::ColorKey(0.6, TPixel32::Green),
        TSpectrum::ColorKey(0.8, TPixel32::Magenta),
        TSpectrum::ColorKey(1, TPixel32::Red)};
    m_colors = TSpectrumParamP(colors);
    m_size->setMeasureName("fxLength");
    bindParam(this, "colors", m_colors);
    bindParam(this, "size", m_size);
  }
  ~DiamondGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  };
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::RADIUS;
    concepts[0].m_label = "Size";
    concepts[0].m_params.push_back(m_size);
  }
};

template <typename PIXEL>
void doDiamondGradient(const TRasterPT<PIXEL> &ras,
                       const TSpectrumT<PIXEL> &spectrum, TPointD &tilepos,
                       double size) {
  PIXEL outpixel = spectrum.getPremultipliedValue(1.);
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD pos = tilepos;
    pos.y += j;
    double fy     = fabs(pos.y) / size;
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double s;
      double fx = fabs(pos.x) / size;
      if ((fx * fy) < 1) {
        s      = fx * fy;
        *pix++ = spectrum.getPremultipliedValue(s);
      } else
        *pix++ = outpixel;
      pos.x += 1.0;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------

void DiamondGradientFx::doCompute(TTile &tile, double frame,
                                  const TRenderSettings &ri) {
  double size = m_size->getValue(frame) * ri.m_affine.a11 / ri.m_shrinkX;
  TPointD pos = tile.m_pos;
  if (TRaster32P raster32 = tile.getRaster())
    doDiamondGradient<TPixel32>(raster32, m_colors->getValue(frame), pos, size);
  else if (TRaster64P raster64 = tile.getRaster())
    doDiamondGradient<TPixel64>(raster64, m_colors->getValue64(frame), pos,
                                size);
  else
    throw TException("DiamondGradientFx: unsupported Pixel Type");
}

//==============================================================================

FX_PLUGIN_IDENTIFIER(DiamondGradientFx, "diamondGradientFx");
