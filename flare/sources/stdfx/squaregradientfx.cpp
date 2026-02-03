

#include "texception.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "tspectrumparam.h"
#include "tparamuiconcept.h"

#undef max
//===================================================================

class SquareGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(SquareGradientFx)
  TSpectrumParamP m_colors;
  TDoubleParamP m_size;

public:
  SquareGradientFx() : m_size(200.0) {
    m_size->setMeasureName("fxLength");
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        // TSpectrum::ColorKey(0.5,TPixel32::Yellow),
        TSpectrum::ColorKey(1, TPixel32::Red)};
    m_colors = TSpectrumParamP(colors);
    bindParam(this, "colors", m_colors);
    bindParam(this, "size", m_size);
    m_size->setValueRange(0, std::numeric_limits<double>::max());
  }
  ~SquareGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  };
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::DIAMOND;
    concepts[0].m_label = "Size";
    concepts[0].m_params.push_back(m_size);
  }
};

template <typename PIXEL>
void doSquareGradient(const TRasterPT<PIXEL> &ras,
                      const TSpectrumT<PIXEL> &spectrum, TPointD &posTrasf,
                      TAffine aff, double size) {
  PIXEL outpixel = spectrum.getPremultipliedValue(1.);
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD posAux = posTrasf;
    TPointD fp;
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double s;
      fp.x = fabs(posAux.x) / size;
      fp.y = fabs(posAux.y) / size;
      if ((fp.x + fp.y) < 1) {
        s      = fp.x + fp.y;
        *pix++ = spectrum.getPremultipliedValue(s);
      } else
        *pix++ = outpixel;
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

void SquareGradientFx::doCompute(TTile &tile, double frame,
                                 const TRenderSettings &ri) {
  double size = m_size->getValue(frame) / ri.m_shrinkX;

  ;
  TAffine aff      = ri.m_affine.inv();
  TPointD posTrasf = aff * tile.m_pos;

  if (TRaster32P raster32 = tile.getRaster())
    doSquareGradient<TPixel32>(raster32, m_colors->getValue(frame), posTrasf,
                               aff, size);
  else if (TRaster64P raster64 = tile.getRaster())
    doSquareGradient<TPixel64>(raster64, m_colors->getValue64(frame), posTrasf,
                               aff, size);
  else
    throw TException("SquareGradientFx: unsupported Pixel Type");
}

FX_PLUGIN_IDENTIFIER(SquareGradientFx, "squareGradientFx");
