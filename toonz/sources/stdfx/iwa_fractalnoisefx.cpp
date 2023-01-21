#include "iwa_fractalnoisefx.h"
#include "iwa_noise1234.h"
#include "tparamuiconcept.h"

#include <QVector3D>

namespace {
// convert sRGB color space to power space
template <typename T = double>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  // return -std::log(T(1) - std::pow(nonlinear_color, gamma)) / exposure;
  if (nonlinear_color <= T(0)) return T(0);
  return std::pow(nonlinear_color, gamma) / exposure;
}
// convert power space to sRGB color space
template <typename T = double>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  // return std::pow(T(1) - std::exp(-exposure * linear_color), T(1) / gamma);
  if (linear_color <= T(0)) return T(0);
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
const double evolutionOffsetStep  = 19.82;
const double evolutionOffsetStepW = 31.1;
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
    , m_alphaRendering(false)
    , m_doConical(false)
    , m_conicalEvolution(0.0)
    , m_conicalAngle(60.0)
    , m_cameraFov(60.0)
    , m_zScale(2.0) {
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

  m_conicalEvolution->setValueRange(-100, 100.0);
  m_conicalAngle->setValueRange(0.0, 89.9);
  m_cameraFov->setValueRange(10.0, 170.0);
  m_zScale->setValueRange(0.0, 3.0);

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

  bindParam(this, "doConical", m_doConical);
  bindParam(this, "conicalEvolution", m_conicalEvolution);
  bindParam(this, "conicalAngle", m_conicalAngle);
  bindParam(this, "cameraFov", m_cameraFov);
  bindParam(this, "zScale", m_zScale);

  bindParam(this, "alphaRendering", m_alphaRendering);

  enableComputeInFloat(true);
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
  TAffine globalAff = TTranslation(-tile.m_pos) * ri.m_affine;
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

  if (!param.doConical) {
    TAffine parentOffsetAff = TTranslation(param.offsetTurbulence);
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
  }

  // conical noise
  else {
    // angle of slope of the cone
    double theta_n = param.conicalAngle * M_PI_180;
    // half of the vertical fov
    double phi_2       = param.cameraFov * 0.5 * M_PI_180;
    double z_scale     = std::pow(10.0, param.zScale);
    double evolution_w = param.conicalEvolution;

    // pixel distance between camera and the screen
    double D = ri.m_cameraBox.getLy() * 0.5 / std::tan(phi_2);
    // the line on the slope :  d = U * z + V
    double U = -1.0 / std::tan(theta_n);
    double V = ri.m_cameraBox.getLy() * 0.5;

    TPointD center = ri.m_affine * param.offsetTurbulence;

    // accumulate base noise pattern for each generation
    for (int gen = 0; gen < genCount; gen++) {
      // affine transformation for the current generation
      TAffine currentAff = (globalAff * parentAff * genAff).inv();
      // scale of the current pattern ( used for the Dynamic / Dynamic Twist
      // offset )
      double scale = sqrt(std::abs(currentAff.det()));

      // for each pixel
      double *buf_p = work_buf;
      for (int y = 0; y < outDim.ly; y++) {
        for (int x = 0; x < outDim.lx; x++, buf_p++) {
          double dx, dy, dz;
          if (theta_n == 0.0) {
            dx = x;
            dy = y;
            dz = 0.0;
          } else {
            // conical, without offset
            if (center == TPointD()) {
              TPointD p = TTranslation(tile.m_pos) * TPointD(x, y);
              // pixel distance from the screen center
              double d = tdistance(p, TPointD());
              // line of sight : d = S * z + T
              double S  = d / D;
              double T  = d;
              dz        = (V - T) / (S - U);
              double dp = S * dz + T;
              if (d != 0.0) {
                p.x *= dp / d;
                p.y *= dp / d;
              }
              p += center * (dp / V);
              p  = TTranslation(tile.m_pos).inv() * p;
              dx = p.x;
              dy = p.y;
              dz /= z_scale;
            }
            // conical, with offset
            else {
              // compute the intersecting point between the "noise cone" and the
              // line of sight
              TPointD _p = TTranslation(tile.m_pos) * TPointD(x, y);
              // offset by combination of offsets of A) projection position and
              // B) eye position.
              //
              // A) 0.5 * projection position offset
              QVector3D p(_p.x - center.x * 0.5, _p.y - center.y * 0.5, 0.0);
              QVector3D cone_C(0.0, 0.0, V * std::tan(theta_n));
              QVector3D cone_a(0, 0, -1);
              // B) 0.5 * eye position offset
              double offsetAdj = 0.5 * (D + cone_C.z()) / cone_C.z();
              QVector3D eye_O(center.x * offsetAdj, center.y * offsetAdj, -D);
              QVector3D eye_d  = (p - eye_O).normalized();
              double cos_ConeT = std::sin(theta_n);

              float d_a   = QVector3D::dotProduct(eye_d, cone_a);
              float Ca_Oa = QVector3D::dotProduct(cone_C, cone_a) -
                            QVector3D::dotProduct(eye_O, cone_a);

              // A * t^2 + B * t + C = 0
              float A =
                  d_a * d_a - eye_d.lengthSquared() * cos_ConeT * cos_ConeT;
              float B = 2.0 *
                            (QVector3D::dotProduct(eye_d, cone_C) -
                             QVector3D::dotProduct(eye_d, eye_O)) *
                            cos_ConeT * cos_ConeT -
                        2.0 * Ca_Oa * d_a;
              float C = Ca_Oa * Ca_Oa -
                        cos_ConeT * cos_ConeT *
                            (cone_C.lengthSquared() -
                             2.0 * QVector3D::dotProduct(eye_O, cone_C) +
                             eye_O.lengthSquared());

              // obtain t
              double t1 = (-B + std::sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
              double t2 = (-B - std::sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
              if (t1 < 0)
                t1 = t2;
              else if (t2 < 0)
                t2 = t1;
              double t = std::min(t1, t2);

              // intersecting point
              QVector3D sampleP = eye_O + eye_d * t;
              _p.x              = sampleP.x();
              _p.y              = sampleP.y();
              _p                = TTranslation(tile.m_pos).inv() * _p;
              dx                = _p.x;
              dy                = _p.y;
              dz                = sampleP.z() / z_scale;
            }
            if (param.cycleEvolution) {
              double cycle_theta =
                  2.0 * M_PI * (param.evolution + param.conicalEvolution + dz) /
                  param.cycleEvolutionRange;
              double cycle_d = param.cycleEvolutionRange / (2.0 * M_PI);
              evolution_zw.x = cycle_d * cos(cycle_theta);
              evolution_zw.y = cycle_d * sin(cycle_theta);
            }
          }
          // obtain sampling position
          // For Dynamic and Dynamic Twist patterns, the position offsets using
          // gradient / rotation of the parent pattern
          TPointD samplePosOffset =
              getSamplePos(x, y, outDim, out_buf, gen, scale, param) -
              TPointD(x, y);
          TPointD samplePos =
              TPointD(dx, dy) + samplePosOffset * (D / (D + dz));
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
            *buf_p = (pn.noise(samplePos.x, samplePos.y, evolution_z,
                               evolution_w + dz) +
                      1.0) *
                     0.5;

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

      if (param.cycleEvolution)
        evolution_zw.x += evolutionOffsetStep;
      else {
        evolution_z += evolutionOffsetStep;
        evolution_w += evolutionOffsetStepW;
      }
    }
  }

  work_buf_ras->unlock();

  // finalize pattern (converting the color space)
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
  TRasterFP rasF   = (TRasterFP)tile.getRaster();
  if (ras32)
    outputRaster<TRaster32P, TPixel32>(ras32, out_buf, param);
  else if (ras64)
    outputRaster<TRaster64P, TPixel64>(ras64, out_buf, param);
  else if (rasF)
    outputRaster<TRasterFP, TPixelF>(rasF, out_buf, param);

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

  param.doConical        = m_doConical->getValue();
  param.conicalEvolution = m_conicalEvolution->getValue(frame);
  param.conicalAngle     = m_conicalAngle->getValue(frame);
  param.cameraFov        = m_cameraFov->getValue(frame);
  param.zScale           = m_zScale->getValue(frame);

  param.alphaRendering = m_alphaRendering->getValue();
}

//------------------------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_FractalNoiseFx::outputRaster(const RASTER outRas, double *out_buf,
                                      const FNParam &param) {
  TDimension dim = outRas->getSize();
  double *buf_p  = out_buf;
  bool doClamp   = !(outRas->getPixelSize() == 16);
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = outRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, buf_p++) {
      double val = (param.invert) ? 1.0 - (*buf_p) : (*buf_p);
      if (doClamp) val = clamp(val, 0.0, 1.0);
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
TPointD Iwa_FractalNoiseFx::getSamplePos(double x, double y,
                                         const TDimension outDim,
                                         const double *out_buf, const int gen,
                                         const double scale,
                                         const FNParam &param) {
  // the position does not offset in the first generation
  if (gen == 0 || param.dynamicIntensity == 0.0 ||
      (param.fractalType != Dynamic && param.fractalType != DynamicTwist))
    return TPointD(x, y);

  auto clampPos = [&](double x, double y) {
    if (x < 0.0)
      x = 0.0;
    else if (x > (double)(outDim.lx - 1))
      x = (double)(outDim.lx - 1);
    if (y < 0.0)
      y = 0.0;
    else if (y > (double)(outDim.ly - 1))
      y = (double)(outDim.ly - 1);
    return TPointD(x, y);
  };

  auto val = [&](const TPoint &p) {
    return out_buf[std::min(p.y, outDim.ly - 1) * outDim.lx +
                   std::min(p.x, outDim.lx - 1)];
  };
  auto lerp = [](double v0, double v1, double ratio) {
    return (1.0 - ratio) * v0 + ratio * v1;
  };
  auto lerpVal = [&](const TPointD &p) {
    int id_x       = (int)std::floor(p.x);
    double ratio_x = p.x - (double)id_x;
    int id_y       = (int)std::floor(p.y);
    double ratio_y = p.y - (double)id_y;
    return lerp(
        lerp(val(TPoint(id_x, id_y)), val(TPoint(id_x + 1, id_y)), ratio_x),
        lerp(val(TPoint(id_x, id_y + 1)), val(TPoint(id_x + 1, id_y + 1)),
             ratio_x),
        ratio_y);
  };
  int range     = std::max(2, (int)(0.1 / scale));
  TPointD left  = clampPos(x - range, y);
  TPointD right = clampPos(x + range, y);
  TPointD down  = clampPos(x, y - range);
  TPointD up    = clampPos(x, y + range);

  double dif_x = param.dynamicIntensity * (1 / scale) *
                 (lerpVal(left) - lerpVal(right)) / (left.x - right.x);
  double dif_y = param.dynamicIntensity * (1 / scale) *
                 (lerpVal(up) - lerpVal(down)) / (up.y - down.y);

  if (param.fractalType == Dynamic)
    return TPointD(x + dif_x, y + dif_y);  // gradient
  else                                     // Dynamic_twist
    return TPointD(x + dif_y, y - dif_x);  // rotation
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
    // conversion LUT for the range from 0.43 to 0.57, every 0.01
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
// finalize pattern (converting the color space)
void Iwa_FractalNoiseFx::finalize(double *out, const FNParam &param) {
  assert(param.fractalType == TurbulentSmooth ||
         param.fractalType == TurbulentBasic ||
         param.fractalType == TurbulentSharp);

  // TurbulentSmooth / TurbulentBasic / TurbulentSharp
  *out = to_nonlinear_color_space(*out, 1.0, turbulentGamma);
}

FX_PLUGIN_IDENTIFIER(Iwa_FractalNoiseFx, "iwa_FractalNoiseFx");
