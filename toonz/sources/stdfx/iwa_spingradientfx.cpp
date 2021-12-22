#include "iwa_spingradientfx.h"

#include "trop.h"
#include "tparamuiconcept.h"
#include "tspectrumparam.h"
#include "gradients.h"

#include <QPolygonF>

#include <array>
#include <algorithm>

//------------------------------------------------------------

Iwa_SpinGradientFx::Iwa_SpinGradientFx()
    : m_center(TPointD(0.0, 0.0))
    , m_startAngle(0.0)
    , m_endAngle(0.0)
    , m_startColor(TPixel32::Black)
    , m_endColor(TPixel32::White)
    , m_curveType(new TIntEnumParam()) {
  m_center->getX()->setMeasureName("fxLength");
  m_center->getY()->setMeasureName("fxLength");
  bindParam(this, "center", m_center);

  m_startAngle->setValueRange(-360, 720);
  m_endAngle->setValueRange(-360, 720);
  bindParam(this, "startAngle", m_startAngle);
  bindParam(this, "endAngle", m_endAngle);

  m_curveType->addItem(EaseInOut, "Ease In-Out");
  m_curveType->addItem(Linear, "Linear");
  m_curveType->addItem(EaseIn, "Ease In");
  m_curveType->addItem(EaseOut, "Ease Out");
  m_curveType->setDefaultValue(Linear);
  m_curveType->setValue(Linear);
  bindParam(this, "curveType", m_curveType);

  bindParam(this, "startColor", m_startColor);
  bindParam(this, "endColor", m_endColor);
}

//------------------------------------------------------------

bool Iwa_SpinGradientFx::doGetBBox(double frame, TRectD &bBox,
                                   const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------
namespace {
template <typename RASTER, typename PIXEL>
void doSpinGradientT(RASTER ras, TDimensionI dim, TPointD centerPos,
                     double startAngle, double endAngle,
                     const TSpectrumT<PIXEL> &spectrum,
                     GradientCurveType type) {
  auto getFactor = [&](double angle) {
    double p = angle - startAngle;
    if (p < 0) p += 2.0 * M_PI;
    double range = endAngle - startAngle;
    if (range <= 0) range += 2.0 * M_PI;

    double t;
    if (range >= p)
      t = p / range;
    else if (M_PI + range / 2.0 > p)
      t = 1.0;
    else
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

  ras->lock();
  for (int j = 0; j < ras->getLy(); j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    double dy     = (double)j - centerPos.y;
    double dx     = -centerPos.x;
    while (pix < endPix) {
      double angle  = std::atan2(dy, dx);
      double factor = getFactor(angle);
      *pix++        = spectrum.getPremultipliedValue(factor);
      dx += 1.0;
    }
  }
  ras->unlock();
}
}  // namespace

//------------------------------------------------------------

void Iwa_SpinGradientFx::doCompute(TTile &tile, double frame,
                                   const TRenderSettings &ri) {
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  // convert shape position to render region coordinate
  TAffine aff = ri.m_affine;
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TPointD dimOffset((float)dimOut.lx / 2.0f, (float)dimOut.ly / 2.0f);
  TPointD centerPos = aff * m_center->getValue(frame) -
                      (tile.m_pos + tile.getRaster()->getCenterD()) + dimOffset;

  std::vector<TSpectrum::ColorKey> colors = {
      TSpectrum::ColorKey(0, m_startColor->getValue(frame)),
      TSpectrum::ColorKey(1, m_endColor->getValue(frame))};
  TSpectrumParamP m_colors = TSpectrumParamP(colors);

  auto conv2RadianAndClamp = [](double angle) {
    double ret = angle * M_PI / 180.0;
    while (ret < -M_PI) {
      ret += 2.0 * M_PI;
    }
    while (ret >= M_PI) {
      ret -= 2.0 * M_PI;
    }
    return ret;
  };

  double startAngle = conv2RadianAndClamp(m_startAngle->getValue(frame));
  double endAngle   = conv2RadianAndClamp(m_endAngle->getValue(frame));

  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    doSpinGradientT<TRaster32P, TPixel32>(
        outRas32, dimOut, centerPos, startAngle, endAngle,
        m_colors->getValue(frame), (GradientCurveType)m_curveType->getValue());
  else if (outRas64)
    doSpinGradientT<TRaster64P, TPixel64>(
        outRas64, dimOut, centerPos, startAngle, endAngle,
        m_colors->getValue64(frame),
        (GradientCurveType)m_curveType->getValue());
}

//------------------------------------------------------------

void Iwa_SpinGradientFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::ANGLE_2;
  concepts[0].m_label = "Angle";
  concepts[0].m_params.push_back(m_startAngle);
  concepts[0].m_params.push_back(m_endAngle);
  concepts[0].m_params.push_back(m_center);

  concepts[1].m_type  = TParamUIConcept::POINT;
  concepts[1].m_label = "Center";
  concepts[1].m_params.push_back(m_center);
}

//------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_SpinGradientFx, "iwa_SpinGradientFx");
