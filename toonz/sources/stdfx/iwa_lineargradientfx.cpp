#include "iwa_lineargradientfx.h"

#include "tspectrumparam.h"
#include "gradients.h"
#include "tparamuiconcept.h"
#include "traster.h"

//------------------------------------------------------------

Iwa_LinearGradientFx::Iwa_LinearGradientFx()
    : m_startPoint(TPointD(-50.0, 0.0))
    , m_endPoint(TPointD(50.0, 0.0))
    , m_startColor(TPixel32::Black)
    , m_endColor(TPixel32::White)
    , m_curveType(new TIntEnumParam(EaseInOut, "Ease In-Out"))
    , m_wave_amplitude(0.0)
    , m_wave_freq(0.0)
    , m_wave_phase(0.0) {
  m_startPoint->getX()->setMeasureName("fxLength");
  m_startPoint->getY()->setMeasureName("fxLength");
  m_endPoint->getX()->setMeasureName("fxLength");
  m_endPoint->getY()->setMeasureName("fxLength");
  bindParam(this, "startPoint", m_startPoint);
  bindParam(this, "endPoint", m_endPoint);

  m_curveType->addItem(Linear, "Linear");
  m_curveType->addItem(EaseIn, "Ease In");
  m_curveType->addItem(EaseOut, "Ease Out");
  bindParam(this, "curveType", m_curveType);

  m_wave_amplitude->setValueRange(0, std::numeric_limits<double>::max());
  m_wave_amplitude->setMeasureName("fxLength");
  bindParam(this, "wave_amplitude", m_wave_amplitude);
  bindParam(this, "wave_frequency", m_wave_freq);
  bindParam(this, "wave_phase", m_wave_phase);

  bindParam(this, "startColor", m_startColor);
  bindParam(this, "endColor", m_endColor);
}

//------------------------------------------------------------

bool Iwa_LinearGradientFx::doGetBBox(double frame, TRectD &bBox,
                                     const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------

namespace {
template <typename RASTER, typename PIXEL>
void doLinearGradientT(RASTER ras, TDimensionI dim, TPointD startPos,
                       TPointD endPos, const TSpectrumT<PIXEL> &spectrum,
                       GradientCurveType type, double w_amplitude,
                       double w_freq, double w_phase, const TAffine &affInv) {
  auto getFactor = [&](double t) {
    if (t > 1.0)
      t = 1.0;
    else if (t < 0.0)
      t = 0.0;

    double factor;
    switch (type) {
    case Linear:
      factor = t;
      break;
    case EaseIn:
      factor = t * t;
      break;
    case EaseOut:
      factor = 1.0 - (1.0 - t) * (1.0 - t);
      break;
    case EaseInOut:
    default:
      factor = (-2 * t + 3) * (t * t);
      break;
    }
    return factor;
  };
  startPos      = affInv * startPos;
  endPos        = affInv * endPos;
  TPointD seVec = endPos - startPos;

  if (seVec == TPointD()) {
    ras->fill(spectrum.getPremultipliedValue(0.0));
    return;
  }

  TPointD posTrasf = -startPos;
  double seVecLen2 = seVec.x * seVec.x + seVec.y * seVec.y;
  double seVecLen  = sqrt(seVecLen2);
  double amplitude = w_amplitude / seVecLen;
  TPointD auxVec(-seVec.y / seVecLen, seVec.x / seVecLen);

  ras->lock();
  for (int j = 0; j < ras->getLy(); j++) {
    TPointD posAux = posTrasf;

    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();

    while (pix < endPix) {
      double t = (seVec.x * posAux.x + seVec.y * posAux.y) / seVecLen2;
      if (amplitude) {
        double distance = posAux.x * auxVec.x + posAux.y * auxVec.y;
        t += amplitude * sin(w_freq * distance + w_phase);
      }
      double factor = getFactor(t);
      *pix++        = spectrum.getPremultipliedValue(factor);

      posAux.x += affInv.a11;
      posAux.y += affInv.a21;
    }
    posTrasf.x += affInv.a12;
    posTrasf.y += affInv.a22;
  }
  ras->unlock();
}
}  // namespace

//------------------------------------------------------------

void Iwa_LinearGradientFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &ri) {
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  // convert shape position to render region coordinate
  TAffine aff = ri.m_affine;
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TPointD dimOffset((float)dimOut.lx / 2.0f, (float)dimOut.ly / 2.0f);
  TPointD startPos = aff * m_startPoint->getValue(frame) -
                     (tile.m_pos + tile.getRaster()->getCenterD()) + dimOffset;
  TPointD endPos = aff * m_endPoint->getValue(frame) -
                   (tile.m_pos + tile.getRaster()->getCenterD()) + dimOffset;

  double w_amplitude = m_wave_amplitude->getValue(frame) / ri.m_shrinkX;
  double w_freq      = m_wave_freq->getValue(frame) * ri.m_shrinkX;
  double w_phase     = m_wave_phase->getValue(frame);
  w_freq *= 0.01 * M_PI_180;

  std::vector<TSpectrum::ColorKey> colors = {
      TSpectrum::ColorKey(0, m_startColor->getValue(frame)),
      TSpectrum::ColorKey(1, m_endColor->getValue(frame))};
  TSpectrumParamP m_colors = TSpectrumParamP(colors);

  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    doLinearGradientT<TRaster32P, TPixel32>(
        outRas32, dimOut, startPos, endPos, m_colors->getValue(frame),
        (GradientCurveType)m_curveType->getValue(), w_amplitude, w_freq,
        w_phase, aff.inv());
  else if (outRas64)
    doLinearGradientT<TRaster64P, TPixel64>(
        outRas64, dimOut, startPos, endPos, m_colors->getValue64(frame),
        (GradientCurveType)m_curveType->getValue(), w_amplitude, w_freq,
        w_phase, aff.inv());
}

//------------------------------------------------------------

void Iwa_LinearGradientFx::getParamUIs(TParamUIConcept *&concepts,
                                       int &length) {
  concepts = new TParamUIConcept[length = 1];

  concepts[0].m_type  = TParamUIConcept::LINEAR_RANGE;
  concepts[0].m_label = "";
  concepts[0].m_params.push_back(m_startPoint);
  concepts[0].m_params.push_back(m_endPoint);
}

//------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_LinearGradientFx, "iwa_LinearGradientFx");
