#include "iwa_fractalnoisefx.h"
#include "iwa_noise1234.h"
#include "tparamuiconcept.h"

namespace {
// convert sRGB color space to power space
template <typename T = double>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  // return -std::log(T(1) - std::pow(nonlinear_color, gamma)) / exposure;
  return std::pow(nonlinear_color, gamma) / exposure;
}
// convert power space to sRGB color space
template <typename T = double>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  // return std::pow(T(1) - std::exp(-exposure * linear_color), T(1) / gamma);
  return std::pow(exposure * linear_color, T(1) / gamma);
}

inline double hardlight(const double *dn, const double *up) {
  if ((*up) < 0.5)
    return (*up) * (*dn) * 2.0;
  else
    return 1.0 - 2.0 * (1.0 - (*up)) * (1.0 - (*dn));
}

template <class T = double>
inline const T &clamp(const T &v, const T &lo, const T &hi) {
  assert(!(hi < lo));
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

const double turbulentGamma = 2.2;
// magic number to offset evolution between generations
const double evolutionOffsetStep = 19.82;
}  // namespace
//------------------------------------------------------------------

Iwa_FractalNoiseFx::Iwa_FractalNoiseFx()
    : m_fractalType(new TIntEnumParam(Basic, "Basic"))
    , m_noiseType(new TIntEnumParam(Block, "Block"))
    , m_invert(false)
    , m_rotation(0.0)
    , m_uniformScaling(true)
    , m_scale(100.0)
    , m_scaleW(100.0)
    , m_scaleH(100.0)
    , m_offsetTurbulence(TPointD(0.0, 0.0))
    , m_perspectiveOffset(false)
    , m_complexity(6.0)
    , m_subInfluence(70.0)
    , m_subScaling(56.0)
    , m_subRotation(0.0)
    , m_subOffset(TPointD(0.0, 0.0))
    ///, m_centerSubscale(false)
    , m_evolution(0.0)
    , m_cycleEvolution(false)
    , m_cycleEvolutionRange(1.0)
    ///, m_randomSeed(0)
    , m_dynamicIntensity(1.0)
    , m_alphaRendering(false) {
  m_fractalType->addItem(TurbulentSmooth, "Turbulent Smooth");
  m_fractalType->addItem(TurbulentBasic, "Turbulent Basic");
  m_fractalType->addItem(TurbulentSharp, "Turbulent Sharp");
  m_fractalType->addItem(Dynamic, "Dynamic");
  m_fractalType->addItem(DynamicTwist, "Dynamic Twist");
  m_fractalType->addItem(Max, "Max");
  m_fractalType->addItem(Rocky, "Rocky");

  m_noiseType->addItem(Smooth, "Smooth");
  m_noiseType->setValue(Smooth);

  m_rotation->setMeasureName("angle");
  m_rotation->setValueRange(-360.0, 360.0);
  m_scale->setMeasureName("fxLength");
  m_scale->setValueRange(20.0, 600.0);
  m_scaleW->setMeasureName("fxLength");
  m_scaleW->setValueRange(20.0, 600.0);
  m_scaleH->setMeasureName("fxLength");
  m_scaleH->setValueRange(20.0, 600.0);
  m_offsetTurbulence->getX()->setMeasureName("fxLength");
  m_offsetTurbulence->getY()->setMeasureName("fxLength");

  m_complexity->setValueRange(1.0, 10.0);
  m_subInfluence->setValueRange(25.0, 100.0);
  m_subScaling->setValueRange(25.0, 100.0);
  m_subRotation->setMeasureName("angle");
  m_subRotation->setValueRange(-360.0, 360.0);
  m_subOffset->getX()->setMeasureName("fxLength");
  m_subOffset->getY()->setMeasureName("fxLength");

  m_evolution->setValueRange(-100.0, 100.0);
  m_cycleEvolutionRange->setValueRange(0.1, 30.0);
  m_dynamicIntensity->setValueRange(-10.0, 10.0);

  bindParam(this, "fractalType", m_fractalType);
  bindParam(this, "noiseType", m_noiseType);
  bindParam(this, "invert", m_invert);
  bindParam(this, "rotation", m_rotation);
  bindParam(this, "uniformScaling", m_uniformScaling);
  bindParam(this, "scale", m_scale);
  bindParam(this, "scaleW", m_scaleW);
  bindParam(this, "scaleH", m_scaleH);
  bindParam(this, "offsetTurbulence", m_offsetTurbulence);
  bindParam(this, "perspectiveOffset", m_perspectiveOffset);
  bindParam(this, "complexity", m_complexity);
  bindParam(this, "subInfluence", m_subInfluence);
  bindParam(this, "subScaling", m_subScaling);
  bindParam(this, "subRotation", m_subRotation);
  bindParam(this, "subOffset", m_subOffset);
  /// bindParam(this, "centerSubscale", m_centerSubscale);
  bindParam(this, "evolution", m_evolution);
  bindParam(this, "cycleEvolution", m_cycleEvolution);
  bindParam(this, "cycleEvolutionRange", m_cycleEvolutionRange);
  /// bindParam(this, "randomSeed", m_randomSeed);
  bindParam(this, "dynamicIntensity", m_dynamicIntensity);

  bindParam(this, "alphaRendering", m_alphaRendering);
}

//------------------------------------------------------------------

bool Iwa_FractalNoiseFx::doGetBBox(double frame, TRectD &bBox,
                                   const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------------

void Iwa_FractalNoiseFx::doCompute(TTile &tile, double frame,
                                   const TRenderSettings &ri) {
  // obtain current parameters
  FNParam param;
  obtainParams(param, frame, ri.m_affine);

  Noise1234 pn;

  TDimension outDim = tile.getRaster()->getSize();
  // allocate buffer for accumulating the noise patterns
  TRasterGR8P out_buf_ras = TRasterGR8P(outDim.lx * sizeof(double), outDim.ly);
  out_buf_ras->clear();
  out_buf_ras->lock();
  double *out_buf = (double *)out_buf_ras->getRawData();

  // allocate buffer for storing the noise pattern of each generation
  TRasterGR8P work_buf_ras = TRasterGR8P(outDim.lx * sizeof(double), outDim.ly);
  work_buf_ras->lock();
  double *work_buf = (double *)work_buf_ras->getRawData();

  // affine transformations
  TAffine globalAff       = TTranslation(-tile.m_pos) * ri.m_affine;
  TAffine parentOffsetAff = TTranslation(param.offsetTurbulence);
  TAffine parentAff =
      TScale(param.scale.lx, param.scale.ly) * TRotation(-param.rotation);
  TAffine subAff = TTranslation(param.subOffset) * TScale(param.subScaling) *
                   TRotation(-param.subRotation);

  TAffine genAff;

  // for cyclic evolution, rotate the sample position in ZW space instead of
  // using the periodic noise in Z space so that it can cycle in arbitral
  // period.
  double evolution_z = param.evolution;
  TPointD evolution_zw;
  if (param.cycleEvolution) {
    double theta   = 2.0 * M_PI * param.evolution / param.cycleEvolutionRange;
    double d       = param.cycleEvolutionRange / (2.0 * M_PI);
    evolution_zw.x = d * cos(theta);
    evolution_zw.y = d * sin(theta);
  }

  int genCount = (int)std::ceil(param.complexity);

  // accumulate base noise pattern for each generation
  for (int gen = 0; gen < genCount; gen++) {
    // affine transformation for the current generation
    TAffine currentAff =
        (globalAff * parentOffsetAff * parentAff * genAff).inv();
    // scale of the current pattern ( used for the Dynamic / Dynamic Twist
    // offset )
    double scale = sqrt(std::abs(currentAff.det()));

    // for each pixel
    double *buf_p = work_buf;
    for (int y = 0; y < outDim.ly; y++) {
      for (int x = 0; x < outDim.lx; x++, buf_p++) {
        // obtain sampling position
        // For Dynamic and Dynamic Twist patterns, the position offsets using
        // gradient / rotation of the parent pattern
        TPointD samplePos =
            getSamplePos(x, y, outDim, out_buf, gen, scale, param);
        // multiply affine transformation
        samplePos = currentAff * samplePos;
        // adjust position for the block pattern
        if (param.noiseType == Block)
          samplePos = TPointD(std::floor(samplePos.x) + 0.5,
                              std::floor(samplePos.y) + 0.5);
        // calculate the base noise
        if (param.cycleEvolution)
          *buf_p = (pn.noise(samplePos.x, samplePos.y, evolution_zw.x,
                             evolution_zw.y) +
                    1.0) *
                   0.5;
        else
          *buf_p =
              (pn.noise(samplePos.x, samplePos.y, evolution_z) + 1.0) * 0.5;

        // convert the noise
        convert(buf_p, param);
      }
    }

    // just copy the values for the first generation
    if (gen == 0) {
      memcpy(out_buf, work_buf, outDim.lx * outDim.ly * sizeof(double));
    } else {
      // intensity of the last generation will take the fraction part of
      // complexity
      double genIntensity = std::min(1.0, param.complexity - (double)gen);
      // influence of the current generation
      double influence =
          genIntensity * std::pow(param.subInfluence, (double)gen);
      // composite the base noise pattern
      buf_p         = work_buf;
      double *out_p = out_buf;
      for (int i = 0; i < outDim.lx * outDim.ly; i++, buf_p++, out_p++)
        composite(out_p, buf_p, influence, param);
    }

    // update affine transformations (for the next generation loop)
    genAff *= subAff;
    // When the "Perspective Offset" option is ON, reduce the offset amount
    // according to the sub scale
    if (param.perspectiveOffset)
      parentOffsetAff = TScale(param.subScaling) *
                        TRotation(-param.subRotation) * parentOffsetAff *
                        TRotation(param.subRotation) *
                        TScale(1 / param.subScaling);

    if (param.cycleEvolution)
      evolution_zw.x += evolutionOffsetStep;
    else
      evolution_z += evolutionOffsetStep;
  }

  work_buf_ras->unlock();

  // finalize pattern (coverting the color space)
  if (param.fractalType == TurbulentSmooth ||
      param.fractalType == TurbulentBasic ||
      param.fractalType == TurbulentSharp) {
    double *out_p = out_buf;
    for (int i = 0; i < outDim.lx * outDim.ly; i++, out_p++)
      finalize(out_p, param);
  }

  tile.getRaster()->clear();

  // convert to RGB channel values
  TRaster32P ras32 = (TRaster32P)tile.getRaster();
  TRaster64P ras64 = (TRaster64P)tile.getRaster();
  if (ras32)
    outputRaster<TRaster32P, TPixel32>(ras32, out_buf, param);
  else if (ras64)
    outputRaster<TRaster64P, TPixel64>(ras64, out_buf, param);

  out_buf_ras->unlock();
}

//------------------------------------------------------------------
// obtain current parameters
void Iwa_FractalNoiseFx::obtainParams(FNParam &param, const double frame,
                                      const TAffine &aff) {
  param.fractalType = (FractalType)m_fractalType->getValue();
  param.noiseType   = (NoiseType)m_noiseType->getValue();
  param.invert      = m_invert->getValue();
  param.rotation    = m_rotation->getValue(frame);  // in degree, not radian
  if (m_uniformScaling->getValue()) {               // uniform case
    double s    = m_scale->getValue(frame);
    param.scale = TDimensionD(s, s);
  } else {  // non-uniform case
    param.scale.lx = m_scaleW->getValue(frame);
    param.scale.ly = m_scaleH->getValue(frame);
  }
  assert(param.scale.lx != 0.0 && param.scale.ly != 0.0);
  if (param.scale.lx == 0.0) param.scale.lx = 1e-8;
  if (param.scale.ly == 0.0) param.scale.ly = 1e-8;

  param.offsetTurbulence  = m_offsetTurbulence->getValue(frame);
  param.perspectiveOffset = m_perspectiveOffset->getValue();
  param.complexity        = m_complexity->getValue(frame);
  if (param.complexity < 1.0)
    param.complexity =
        1.0;  // at least the first generation is rendered in full opacity
  param.subInfluence =
      m_subInfluence->getValue(frame) / 100.0;  // normalize to 0 - 1
  param.subScaling =
      m_subScaling->getValue(frame) / 100.0;           // normalize to 0 - 1
  param.subRotation = m_subRotation->getValue(frame);  // in degree, not radian
  param.subOffset   = m_subOffset->getValue(frame);
  param.evolution   = m_evolution->getValue(frame);
  param.cycleEvolution      = m_cycleEvolution->getValue();
  param.cycleEvolutionRange = m_cycleEvolutionRange->getValue(frame);
  param.dynamicIntensity    = m_dynamicIntensity->getValue(frame) * 10.0;
  param.alphaRendering      = m_alphaRendering->getValue();
}

//------------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_FractalNoiseFx::outputRaster(const RASTER outRas, double *out_buf,
                                      const FNParam &param) {
  TDimension dim = outRas->getSize();
  double *buf_p  = out_buf;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = outRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, buf_p++) {
      double val                   = (param.invert) ? 1.0 - (*buf_p) : (*buf_p);
      val                          = clamp(val, 0.0, 1.0);
      typename PIXEL::Channel chan = static_cast<typename PIXEL::Channel>(
          val * (double)PIXEL::maxChannelValue);
      pix->r = chan;
      pix->g = chan;
      pix->b = chan;
      pix->m = (param.alphaRendering) ? chan : PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------------------------

void Iwa_FractalNoiseFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Offset Turbulence";
  concepts[0].m_params.push_back(m_offsetTurbulence);

  concepts[1].m_type  = TParamUIConcept::POINT;
  concepts[1].m_label = "Sub Offset";
  concepts[1].m_params.push_back(m_subOffset);
}
//------------------------------------------------------------------
// For Dynamic and Dynamic Twist patterns, the position offsets using gradient /
// rotation of the parent pattern
TPointD Iwa_FractalNoiseFx::getSamplePos(int x, int y, const TDimension outDim,
                                         const double *out_buf, const int gen,
                                         const double scale,
                                         const FNParam &param) {
  // the position does not offset in the first generation
  if (gen == 0 || param.dynamicIntensity == 0.0 ||
      (param.fractalType != Dynamic && param.fractalType != DynamicTwist))
    return TPointD((double)x, (double)y);

  auto clampPos = [&](int x, int y) {
    if (x < 0)
      x = 0;
    else if (x >= outDim.lx)
      x = outDim.lx - 1;
    if (y < 0)
      y = 0;
    else if (y >= outDim.ly)
      y = outDim.ly - 1;
    return TPoint(x, y);
  };

  auto val    = [&](const TPoint &p) { return out_buf[p.y * outDim.lx + p.x]; };
  int range   = std::max(2, (int)(0.1 / scale));
  TPoint left = clampPos(x - range, y);
  TPoint right = clampPos(x + range, y);
  TPoint down  = clampPos(x, y - range);
  TPoint up    = clampPos(x, y + range);

  double dif_x = param.dynamicIntensity * (1 / scale) *
                 (val(left) - val(right)) / (left.x - right.x);
  double dif_y = param.dynamicIntensity * (1 / scale) * (val(up) - val(down)) /
                 (up.y - down.y);

  if (param.fractalType == Dynamic)
    return TPointD((double)x + dif_x, (double)y + dif_y);  // gradient
  else                                                     // Dynamic_twist
    return TPointD((double)x + dif_y, (double)y - dif_x);  // rotation
}

//------------------------------------------------------------------
// convert the noise
void Iwa_FractalNoiseFx::convert(double *buf, const FNParam &param) {
  if (param.fractalType == Basic || param.fractalType == Dynamic ||
      param.fractalType == DynamicTwist)
    return;

  switch (param.fractalType) {
  case TurbulentSmooth:
    *buf = std::pow(std::abs(*buf - 0.5), 2.0) * 3.75;
    *buf = to_linear_color_space(*buf, 1.0, turbulentGamma);
    break;
  case TurbulentBasic:
    *buf = std::pow(std::abs(*buf - 0.5), 1.62) * 4.454;
    *buf = to_linear_color_space(*buf, 1.0, turbulentGamma);
    break;
  case TurbulentSharp:
    *buf = std::pow(std::abs(*buf - 0.5), 0.725) * 1.77;
    *buf = to_linear_color_space(*buf, 1.0, turbulentGamma);
    break;
  case Max:
    *buf = std::abs(*buf - 0.5) * 1.96;
    break;
  case Rocky:
    // convertion LUT for the range from 0.43 to 0.57, every 0.01
    static double table[15] = {
        0.25,        0.256658635, 0.275550218, 0.30569519,  0.345275591,
        0.392513494, 0.440512,    0.5,         0.555085147, 0.607486506,
        0.654724409, 0.69430481,  0.724449782, 0.743341365, 0.75};
    if (*buf <= 0.43)
      *buf = 0.25;
    else if (*buf >= 0.57)
      *buf = 0.75;
    else {
      int id   = (int)std::floor(*buf * 100.0) - 43;
      double t = *buf * 100.0 - (double)(id + 43);
      // linear interpolation the LUT values
      *buf = (1 - t) * table[id] + t * table[id + 1];
    }
    break;
  }
}

//------------------------------------------------------------------
// composite the base noise pattern
void Iwa_FractalNoiseFx::composite(double *out, double *buf,
                                   const double influence,
                                   const FNParam &param) {
  switch (param.fractalType) {
  case Basic:
  case Dynamic:
  case DynamicTwist:
  case Rocky: {
    // hard light composition
    double val = hardlight(out, buf);
    *out       = (1.0 - influence) * (*out) + influence * val;
    break;
  }
  case TurbulentSmooth:
  case TurbulentBasic:
  case TurbulentSharp:
    // add composition in the linear color space
    *out += (*buf) * influence;
    break;
  case Max:
    // max composition
    *out = std::max(*out, influence * (*buf));
    break;
  default: {
    double val = hardlight(out, buf);
    *out       = (1.0 - influence) * (*out) + influence * val;
    break;
  }
  }
}

//------------------------------------------------------------------
// finalize pattern (coverting the color space)
void Iwa_FractalNoiseFx::finalize(double *out, const FNParam &param) {
  assert(param.fractalType == TurbulentSmooth ||
         param.fractalType == TurbulentBasic ||
         param.fractalType == TurbulentSharp);

  // TurbulentSmooth / TurbulentBasic / TurbulentSharp
  *out = to_nonlinear_color_space(*out, 1.0, turbulentGamma);
}

FX_PLUGIN_IDENTIFIER(Iwa_FractalNoiseFx, "iwa_FractalNoiseFx");
