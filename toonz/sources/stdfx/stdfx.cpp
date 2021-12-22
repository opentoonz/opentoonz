

#include "trop.h"
#include "tfxparam.h"
#include "stdfx.h"
#include "tpluginmanager.h"
#include "tpixelutils.h"
#include "tspectrumparam.h"
#include "ttzpimagefx.h"
#include "gradients.h"
#include "tunit.h"
#include "tparamuiconcept.h"

#include <QCoreApplication>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
#undef max
//==================================================================

bool isAlmostIsotropic(const TAffine &aff) { return aff.isIsotropic(0.001); }

//==================================================================

class FadeFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(FadeFx)
  TRasterFxPort m_input;
  TDoubleParamP m_value;

public:
  FadeFx() : m_value(50) {
    m_value->setValueRange(0, 100);
    bindParam(this, "value", m_value);

    addInputPort("Source", m_input);
  };

  ~FadeFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      // devo scurire bgColor
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  };

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_input.isConnected()) return;

    m_input->compute(tile, frame, ri);

    double v = 1 - m_value->getValue(frame) / 100;
    TRop::rgbmScale(tile.getRaster(), tile.getRaster(), 1, 1, 1, v);
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//==================================================================

class SpiralFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(SpiralFx)
  TIntEnumParamP m_type;
  TDoubleParamP m_freq;
  TDoubleParamP m_phase;
  TSpectrumParamP m_spectrum;

  enum SpiralType { Archimedean, Logarithmic };

public:
  SpiralFx()
      : m_type(new TIntEnumParam(Archimedean, "Archimedean"))
      , m_freq(0.1)   // args, "Freq")
      , m_phase(0.0)  // args, "Phase")
  {
    // m_freq->setDefaultValue(0.1);
    // m_phase->setDefaultValue(0.0);
    const TPixel32 transparent(0, 0, 0, 0);
    /*
TPixel32 colors[] = {
            TPixel32::Magenta,
            TPixel32::Black,
            TPixel32::Red,
            TPixel32::Yellow,
            transparent};
*/
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::Magenta),
        TSpectrum::ColorKey(0.25, TPixel32::Black),
        TSpectrum::ColorKey(0.5, TPixel32::Red),
        TSpectrum::ColorKey(0.75, TPixel32::Yellow),
        TSpectrum::ColorKey(1, transparent)};
    m_spectrum = TSpectrumParamP(colors);

    m_type->addItem(Logarithmic, "Logarithmic");
    bindParam(this, "type", m_type);
    bindParam(this, "colors", m_spectrum);
    bindParam(this, "freq", m_freq);
    bindParam(this, "phase", m_phase);
    m_freq->setValueRange(0, 1);
    // m_spectrum->setDefaultValue(tArrayCount(colors), colors);
  };
  ~SpiralFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
};

//------------------------------------------------------------------

namespace {
template <class T>
void doComputeT(TRasterPT<T> raster, TPointD posTrasf, const TAffine &aff,
                const TSpectrumT<T> &spectrum, double freq, double phase,
                bool isLogarithmic) {
  raster->lock();
  for (int y = 0; y < raster->getLy(); y++) {
    TPointD posAux = posTrasf;
    T *pix         = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); x++) {
      double ang = 0.0;
      if (posAux.x != 0 || posAux.y != 0) ang = atan2(posAux.y, posAux.x);
      double r = sqrt(posAux.x * posAux.x + posAux.y * posAux.y);
      if (isLogarithmic) r = std::log(r) * 30.0;
      double v = 0.5 * (1 + sin(r * freq + ang + phase));
      *pix++   = spectrum.getPremultipliedValue(v);
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  raster->unlock();
}
}  // namespace

//==================================================================

void SpiralFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  double phase       = m_phase->getValue(frame);
  double freq        = m_freq->getValue(frame);
  bool isLogarithmic = SpiralType(m_type->getValue()) == Logarithmic;

  TAffine aff      = ri.m_affine.inv();
  TPointD posTrasf = aff * tile.m_pos;

  if (TRaster32P ras32 = tile.getRaster())
    doComputeT<TPixel32>(ras32, posTrasf, aff, m_spectrum->getValue(frame),
                         freq, phase, isLogarithmic);
  else if (TRaster64P ras64 = tile.getRaster())
    doComputeT<TPixel64>(ras64, posTrasf, aff, m_spectrum->getValue64(frame),
                         freq, phase, isLogarithmic);
  else
    throw TException("SpiralFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

class MultiLinearGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(MultiLinearGradientFx)
  TDoubleParamP m_period;
  TDoubleParamP m_count;
  TDoubleParamP m_cycle;
  TDoubleParamP m_wave_amplitude;
  TDoubleParamP m_wave_freq;
  TDoubleParamP m_wave_phase;
  TSpectrumParamP m_colors;

  TIntEnumParamP m_curveType;

public:
  MultiLinearGradientFx()
      : m_period(100)          // args, "Period")
      , m_count(2)             // args, "Count")
      , m_cycle(0.0)           // args, "Cycle")
      , m_wave_amplitude(0.0)  // args, "Cycle")
      , m_wave_freq(0.0)       // args, "Cycle")
      , m_wave_phase(0.0)      // args, "Cycle")
                               //    , m_colors (0) //args, "Colors")
      , m_curveType(new TIntEnumParam(EaseInOut, "Ease In-Out")) {
    m_curveType->addItem(Linear, "Linear");
    m_curveType->addItem(EaseIn, "Ease In");
    m_curveType->addItem(EaseOut, "Ease Out");

    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.33, TPixel32::Yellow),
        TSpectrum::ColorKey(0.66, TPixel32::Red),
        TSpectrum::ColorKey(1, TPixel32::White)};
    m_colors = TSpectrumParamP(colors);

    bindParam(this, "period", m_period);
    bindParam(this, "count", m_count);
    bindParam(this, "cycle", m_cycle);
    bindParam(this, "wave_amplitude", m_wave_amplitude);
    bindParam(this, "wave_frequency", m_wave_freq);
    bindParam(this, "wave_phase", m_wave_phase);
    bindParam(this, "colors", m_colors);
    bindParam(this, "curveType", m_curveType);

    m_period->setValueRange(0, (std::numeric_limits<double>::max)());
    m_cycle->setValueRange(0, (std::numeric_limits<double>::max)());
    m_wave_amplitude->setValueRange(0, (std::numeric_limits<double>::max)());
    m_count->setValueRange(0, (std::numeric_limits<double>::max)());
    m_period->setMeasureName("fxLength");
    m_wave_amplitude->setMeasureName("fxLength");
  }
  ~MultiLinearGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;

    return true;
    // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::WIDTH;
    concepts[0].m_label = "Size";
    concepts[0].m_params.push_back(m_period);
  }
};

// From V1.4 LinearGradientFx becomes obsolete and was replaced by
// Iwa_LinearGradientFx which has more flexibility. (iwa_lineargradientfx.cpp)
// This code is kept in order to load the fx made with older OT versions.
// Nov 14, 2019

class LinearGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(LinearGradientFx)
  TDoubleParamP m_period;

  TDoubleParamP m_wave_amplitude;
  TDoubleParamP m_wave_freq;
  TDoubleParamP m_wave_phase;
  TPixelParamP m_color1;
  TPixelParamP m_color2;

  TIntEnumParamP m_curveType;

public:
  LinearGradientFx()
      : m_period(100)          // args, "Period")
      , m_wave_amplitude(0.0)  // args, "Cycle")
      , m_wave_freq(0.0)       // args, "Cycle")
      , m_wave_phase(0.0)      // args, "Cycle")
      , m_color1(TPixel32::Black)
      , m_color2(TPixel32::White)
      //    , m_colors (0) //args, "Colors")
      , m_curveType(new TIntEnumParam(EaseInOut, "Ease In-Out")) {
    m_curveType->addItem(Linear, "Linear");
    m_curveType->addItem(EaseIn, "Ease In");
    m_curveType->addItem(EaseOut, "Ease Out");

    bindParam(this, "period", m_period);
    bindParam(this, "wave_amplitude", m_wave_amplitude);
    bindParam(this, "wave_frequency", m_wave_freq);
    bindParam(this, "wave_phase", m_wave_phase);
    bindParam(this, "color1", m_color1);
    bindParam(this, "color2", m_color2);
    bindParam(this, "curveType", m_curveType);

    m_period->setValueRange(0, std::numeric_limits<double>::max());
    m_wave_amplitude->setValueRange(0, std::numeric_limits<double>::max());
    m_period->setMeasureName("fxLength");
    m_wave_amplitude->setMeasureName("fxLength");
  }
  ~LinearGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;

    return true;
    // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::WIDTH;
    concepts[0].m_label = "Size";
    concepts[0].m_params.push_back(m_period);
  }
};

//==================================================================

namespace {
template <class T>
void doComputeT(TRasterPT<T> ras, TPointD posTrasf,
                const TSpectrumT<T> &spectrum, double period, double count,
                double w_amplitude, double w_freq, double w_phase, double cycle,
                const TAffine &aff) {
  double shift     = 0;
  double maxRadius = period * count / 2.;
  double freq      = 1.0 / period;
  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD posAux = posTrasf;

    // TPointD pos = tile.m_pos;
    // pos.y += j;
    T *pix    = ras->pixels(j);
    T *endPix = pix + ras->getLx();
    while (pix < endPix) {
      if (w_amplitude) shift = w_amplitude * sin(w_freq * posAux.y + w_phase);
      double radius = posAux.x + shift;
      double t      = 1;
      if (fabs(radius) < maxRadius) {
        t = (radius + maxRadius + cycle) * freq;
        t -= floor(t);
      } else if (radius < 0)
        t = 0;
      double polinomfactor = (-2 * t + 3) * (t * t);
      // pos.x += 1.0;
      *pix++ = spectrum.getPremultipliedValue(polinomfactor);
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  ras->unlock();
}
}  // namespace

//==================================================================

void LinearGradientFx::doCompute(TTile &tile, double frame,
                                 const TRenderSettings &ri) {
  assert((TRaster32P)tile.getRaster() || (TRaster64P)tile.getRaster());

  double period      = m_period->getValue(frame) / ri.m_shrinkX;
  double count       = 1.0;
  double cycle       = 0;
  double w_amplitude = m_wave_amplitude->getValue(frame) / ri.m_shrinkX;
  double w_freq      = m_wave_freq->getValue(frame) * ri.m_shrinkX;
  double w_phase     = m_wave_phase->getValue(frame);
  w_freq *= 0.01 * M_PI_180;

  std::vector<TSpectrum::ColorKey> colors = {
      TSpectrum::ColorKey(0, m_color1->getValue(frame)),
      TSpectrum::ColorKey(1, m_color2->getValue(frame))};
  TSpectrumParamP m_colors = TSpectrumParamP(colors);

  TAffine aff      = ri.m_affine.inv();
  TPointD posTrasf = aff * tile.m_pos;
  multiLinear(tile.getRaster(), posTrasf, m_colors, period, count, w_amplitude,
              w_freq, w_phase, cycle, aff, frame,
              (GradientCurveType)m_curveType->getValue());
  /*
  if (TRaster32P ras32 = tile.getRaster())
doComputeT<TPixel32>(
ras32, posTrasf,
m_colors->getValue(frame),
period, count, w_amplitude, w_freq, w_phase, cycle, aff);
else if (TRaster64P ras64 = tile.getRaster())
doComputeT<TPixel64>(
ras64, posTrasf,
m_colors->getValue64(frame),
period, count, w_amplitude, w_freq, w_phase, cycle, aff);
else
throw TException("MultiLinearGradientFx: unsupported Pixel Type");
*/
}

//==================================================================

void MultiLinearGradientFx::doCompute(TTile &tile, double frame,
                                      const TRenderSettings &ri) {
  assert((TRaster32P)tile.getRaster() || (TRaster64P)tile.getRaster());

  double period      = m_period->getValue(frame) / ri.m_shrinkX;
  double count       = m_count->getValue(frame);
  double cycle       = m_cycle->getValue(frame) / ri.m_shrinkX;
  double w_amplitude = m_wave_amplitude->getValue(frame) / ri.m_shrinkX;
  double w_freq      = m_wave_freq->getValue(frame) * ri.m_shrinkX;
  double w_phase     = m_wave_phase->getValue(frame);
  w_freq *= 0.01 * M_PI_180;

  TAffine aff      = ri.m_affine.inv();
  TPointD posTrasf = aff * tile.m_pos;
  multiLinear(tile.getRaster(), posTrasf, m_colors, period, count, w_amplitude,
              w_freq, w_phase, cycle, aff, frame,
              (GradientCurveType)m_curveType->getValue());
  /*
  if (TRaster32P ras32 = tile.getRaster())
doComputeT<TPixel32>(
ras32, posTrasf,
m_colors->getValue(frame),
period, count, w_amplitude, w_freq, w_phase, cycle, aff);
else if (TRaster64P ras64 = tile.getRaster())
doComputeT<TPixel64>(
ras64, posTrasf,
m_colors->getValue64(frame),
period, count, w_amplitude, w_freq, w_phase, cycle, aff);
else
throw TException("MultiLinearGradientFx: unsupported Pixel Type");
*/
}

//==================================================================

class RadialGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(RadialGradientFx)
  TDoubleParamP m_period;
  TDoubleParamP m_innerperiod;
  TPixelParamP m_color1;
  TPixelParamP m_color2;

  TIntEnumParamP m_curveType;

public:
  RadialGradientFx()
      : m_period(100.0)
      , m_innerperiod(0.0)  // args, "Period")
      , m_color1(TPixel32::White)
      , m_color2(TPixel32::Transparent)
      //    , m_colors (0) //args, "Colors")
      , m_curveType(new TIntEnumParam()) {
    m_curveType->addItem(EaseInOut, "Ease In-Out");
    m_curveType->addItem(Linear, "Linear");
    m_curveType->addItem(EaseIn, "Ease In");
    m_curveType->addItem(EaseOut, "Ease Out");
    m_curveType->setDefaultValue(Linear);
    m_curveType->setValue(Linear);

    m_period->setMeasureName("fxLength");
    m_innerperiod->setMeasureName("fxLength");
    bindParam(this, "period", m_period);
    bindParam(this, "innerperiod", m_innerperiod);
    bindParam(this, "color1", m_color1);
    bindParam(this, "color2", m_color2);
    bindParam(this, "curveType", m_curveType);
    m_period->setValueRange(0.0, std::numeric_limits<double>::max());
    m_innerperiod->setValueRange(0.0, std::numeric_limits<double>::max());
  }
  ~RadialGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
    // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 2];

    concepts[0].m_type  = TParamUIConcept::RADIUS;
    concepts[0].m_label = "Inner Size";
    concepts[0].m_params.push_back(m_innerperiod);

    concepts[1].m_type  = TParamUIConcept::RADIUS;
    concepts[1].m_label = "Outer Size";
    concepts[1].m_params.push_back(m_period);
  }
};

//==================================================================

class MultiRadialGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(MultiRadialGradientFx)
  TDoubleParamP m_period;
  TDoubleParamP m_count;
  TDoubleParamP m_cycle;
  TSpectrumParamP m_colors;

  TIntEnumParamP m_curveType;

public:
  MultiRadialGradientFx()
      : m_period(100)  // args, "Period")
      , m_count(2)     // args, "Count")
      , m_cycle(0.0)   // args, "Count")
                       //    , m_colors (0) //args, "Colors")
      , m_curveType(new TIntEnumParam()) {
    m_curveType->addItem(EaseInOut, "Ease In-Out");
    m_curveType->addItem(Linear, "Linear");
    m_curveType->addItem(EaseIn, "Ease In");
    m_curveType->addItem(EaseOut, "Ease Out");
    m_curveType->setDefaultValue(Linear);
    m_curveType->setValue(Linear);

    m_period->setMeasureName("fxLength");
    std::vector<TSpectrum::ColorKey> colors = {
        TSpectrum::ColorKey(0, TPixel32::White),
        TSpectrum::ColorKey(0.33, TPixel32::Yellow),
        TSpectrum::ColorKey(0.66, TPixel32::Red),
        TSpectrum::ColorKey(1, TPixel32::White)};
    m_colors = TSpectrumParamP(colors);

    bindParam(this, "period", m_period);
    bindParam(this, "count", m_count);
    bindParam(this, "cycle", m_cycle);
    bindParam(this, "colors", m_colors);
    bindParam(this, "curveType", m_curveType);
    m_period->setValueRange(0, (std::numeric_limits<double>::max)());
    m_cycle->setValueRange(0, (std::numeric_limits<double>::max)());
    m_count->setValueRange(0, (std::numeric_limits<double>::max)());
  }
  ~MultiRadialGradientFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
    // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::RADIUS;
    concepts[0].m_label = "Period";
    concepts[0].m_params.push_back(m_period);
  }
};

//------------------------------------------------------------------

//==================================================================

void MultiRadialGradientFx::doCompute(TTile &tile, double frame,
                                      const TRenderSettings &ri) {
  assert((TRaster32P)tile.getRaster() || (TRaster64P)tile.getRaster());
  double period = m_period->getValue(frame) / ri.m_shrinkX;
  double count  = m_count->getValue(frame);
  double cycle  = m_cycle->getValue(frame) / ri.m_shrinkX;

  TAffine aff      = ri.m_affine.inv();
  TPointD posTrasf = aff * tile.m_pos;
  multiRadial(tile.getRaster(), posTrasf, m_colors, period, count, cycle, aff,
              frame, 0.0, (GradientCurveType)m_curveType->getValue());
}

//==================================================================

void RadialGradientFx::doCompute(TTile &tile, double frame,
                                 const TRenderSettings &ri) {
  assert((TRaster32P)tile.getRaster() || (TRaster64P)tile.getRaster());
  double period      = m_period->getValue(frame) / ri.m_shrinkX;
  double innerperiod = m_innerperiod->getValue(frame) / ri.m_shrinkX;
  double count       = 1.0;
  double cycle       = 0.0;
  double inner       = 0.0;
  if (innerperiod < period)
    inner = innerperiod / period;
  else
    inner = 1 - TConsts::epsilon;
  std::vector<TSpectrum::ColorKey> colors = {
      TSpectrum::ColorKey(0, m_color1->getValue(frame)),
      TSpectrum::ColorKey(1, m_color2->getValue(frame))};
  TSpectrumParamP m_colors = TSpectrumParamP(colors);
  TAffine aff              = ri.m_affine.inv();
  TPointD posTrasf         = aff * tile.m_pos;
  multiRadial(tile.getRaster(), posTrasf, m_colors, period, count, cycle, aff,
              frame, inner, (GradientCurveType)m_curveType->getValue());
}

//------------------------------------------------------------------

class LightSpotFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(LightSpotFx)
  TDoubleParamP m_softness;
  TDoubleParamP m_a;
  TDoubleParamP m_b;
  TPixelParamP m_color;

public:
  LightSpotFx()
      : m_softness(0.2)           // args, "Softness")
      , m_a(200)                  // args, "A")
      , m_b(100)                  // args, "B")
      , m_color(TPixel::Magenta)  // args, "Color")
  {
    m_a->setMeasureName("fxLength");
    m_b->setMeasureName("fxLength");
    bindParam(this, "softness", m_softness);
    bindParam(this, "a", m_a);
    bindParam(this, "b", m_b);
    bindParam(this, "color", m_color);
    /*
m_a->setDefaultValue(200);
m_b->setDefaultValue(100);
m_color->setDefaultValue(TPixel::Magenta);
*/
  }
  ~LightSpotFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;
    // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type = TParamUIConcept::RECT;
    concepts[0].m_params.push_back(m_a);
    concepts[0].m_params.push_back(m_b);
  }
};

//------------------------------------------------------------------

namespace {
template <class T>
void doComputeT(TRasterPT<T> raster, TPointD posTrasf, const TAffine &aff,
                const T &pixelColor, double softness, double a, double b) {
  double aa     = a * a;
  double bb     = b * b;
  double invaa  = 1 / aa;
  double invbb  = 1 / bb;
  double num    = 2 * (aa + bb);
  double normax = num / (5 * aa + bb);
  int j;

  raster->lock();
  for (j = 0; j < raster->getLy(); j++) {
    TPointD posAux = posTrasf;
    T *pix         = raster->pixels(j);
    T *endPix      = pix + raster->getLx();
    while (pix < endPix) {
      double yyrot = (posAux.y) * (posAux.y);
      double yvar  = (yyrot)*invbb + 1;
      double result;
      double fact, xrot, tempvar, normtmp, outsideslope;
      // pos.x += 1.0;
      xrot    = (posAux.x);
      tempvar = xrot * xrot * invaa + yvar;
      fact    = tempvar * 0.5;
      if (fact < 1) {
        normtmp = num / (aa + bb + (xrot - a) * (xrot - a) + yyrot);
        result  = normtmp;
      } else {
        outsideslope = 1 / (1 + (fact - 1) * softness);
        result       = normax * outsideslope;
      }
      if (result > 1) result = 1;
      if (result < 0) result = 0;
      *pix++ = blend(T::Black, pixelColor, result);
      posAux.x += aff.a11;
      posAux.y += aff.a21;
    }
    posTrasf.x += aff.a12;
    posTrasf.y += aff.a22;
  }
  raster->unlock();
}
}  // namespace

//==================================================================

void LightSpotFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  double a = m_a->getValue(frame) / ri.m_shrinkX;
  double b = m_b->getValue(frame) / ri.m_shrinkX;

  if (a == 0.0 || b == 0.0) {
    if ((TRaster32P)tile.getRaster())
      ((TRaster32P)tile.getRaster())->fill(TPixel32::Black);
    else if ((TRaster64P)tile.getRaster())
      ((TRaster64P)tile.getRaster())->fill(TPixel64::Black);
    return;
  }

  TAffine aff               = ri.m_affine.inv();
  TPointD posTrasf          = aff * tile.m_pos;
  const TPixel32 pixelColor = m_color->getValue(frame);
  double softness           = m_softness->getValue(frame);
  if ((TRaster32P)tile.getRaster())
    doComputeT<TPixel32>(tile.getRaster(), posTrasf, aff, pixelColor, softness,
                         a, b);
  else if ((TRaster64P)tile.getRaster())
    doComputeT<TPixel64>(tile.getRaster(), posTrasf, aff, toPixel64(pixelColor),
                         softness, a, b);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(SpiralFx, "spiralFx")
FX_PLUGIN_IDENTIFIER(FadeFx, "fadeFx")
FX_PLUGIN_IDENTIFIER(RadialGradientFx, "radialGradientFx")
FX_PLUGIN_IDENTIFIER(MultiRadialGradientFx, "multiRadialGradientFx")
FX_PLUGIN_IDENTIFIER(LinearGradientFx, "linearGradientFx")
FX_PLUGIN_IDENTIFIER(MultiLinearGradientFx, "multiLinearGradientFx")
FX_PLUGIN_IDENTIFIER(LightSpotFx, "lightSpotFx")

/*
TLIBMAIN {
static TPluginInfo info("stdfx");
return &info;
};

*/

/* TODO, move to header */
DV_EXPORT_API void initStdFx();

DV_EXPORT_API void initStdFx() {}
