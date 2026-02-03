/*------------------------------------------------------------
Iwa_PNPerspectiveFx
 render perspective noise pattern.
------------------------------------------------------------*/

#include "iwa_pnperspectivefx.h"

#include "trop.h"
#include "tparamuiconcept.h"

#include "iwa_fresnel.h"
#include "iwa_simplexnoise.h"
#include "iwa_noise1234.h"

#include <vector>

namespace {
#ifndef M_PI
const double M_PI = 3.1415926535897932384626433832795;
#endif

inline double dot(double3 a, double3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline double3 cross(double3 a, double3 b) {
  double3 ret = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x};
  return ret;
}
inline double3 normalize(double3 v) {
  double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  double3 ret   = {v.x / length, v.y / length, v.z / length};
  return ret;
}

double getFresnel(double deg) {
  if (deg < 0.0) return 0.0;
  if (deg >= 90.0) return 1.0;
  int index    = (int)std::floor(deg);
  double ratio = deg - (double)index;
  return fresnel[index] * (1.0 - ratio) + fresnel[index + 1] * ratio;
}

}  // namespace

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_PNPerspectiveFx::setOutputRaster(double4 *srcMem, const RASTER dstRas,
                                          TDimensionI dim, int drawLevel,
                                          const bool alp_rend_sw) {
  typename PIXEL::Channel halfChan =
      (typename PIXEL::Channel)(PIXEL::maxChannelValue / 2);

  if (alp_rend_sw)
    dstRas->fill(PIXEL(halfChan, halfChan, halfChan, halfChan));
  else
    dstRas->fill(PIXEL(halfChan, halfChan, halfChan));
  double4 *chan_p = srcMem;
  for (int j = 0; j < drawLevel; j++) {
    PIXEL *pix = dstRas->pixels(j);
    for (int i = 0; i < dstRas->getLx(); i++, chan_p++, pix++) {
      double val;
      val    = (*chan_p).x * (double)PIXEL::maxChannelValue + 0.5;
      pix->r = (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                             ? (double)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (double)PIXEL::maxChannelValue + 0.5;
      pix->g = (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                             ? (double)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (double)PIXEL::maxChannelValue + 0.5;
      pix->b = (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                             ? (double)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (double)PIXEL::maxChannelValue + 0.5;
      pix->m = (typename PIXEL::Channel)((val > (double)PIXEL::maxChannelValue)
                                             ? (double)PIXEL::maxChannelValue
                                             : val);
    }
  }
}

template <>
void Iwa_PNPerspectiveFx::setOutputRaster<TRasterFP, TPixelF>(
    double4 *srcMem, const TRasterFP dstRas, TDimensionI dim, int drawLevel,
    const bool alp_rend_sw) {
  if (alp_rend_sw)
    dstRas->fill(TPixelF(0.5f, 0.5f, 0.5f, 0.5f));
  else
    dstRas->fill(TPixelF(0.5f, 0.5f, 0.5f));
  double4 *chan_p = srcMem;
  for (int j = 0; j < drawLevel; j++) {
    TPixelF *pix = dstRas->pixels(j);
    for (int i = 0; i < dstRas->getLx(); i++, chan_p++, pix++) {
      pix->r = (float)(*chan_p).x;
      pix->g = (float)(*chan_p).y;
      pix->b = (float)(*chan_p).z;
      pix->m = std::min((float)(*chan_p).w, 1.f);
    }
  }
}

//------------------------------------------------------------
// obtain parameters
void Iwa_PNPerspectiveFx::getPNParameters(TTile &tile, double frame,
                                          const TRenderSettings &settings,
                                          PN_Params &params,
                                          TDimensionI &dimOut) {
  params.renderMode = (PN_Params::RenderMode)m_renderMode->getValue();
  params.noiseType  = (PN_Params::NoiseType)m_noiseType->getValue();
  params.size       = m_size->getValue(frame);
  // adjust size of Simplex Noise to be with the same density as Perlin Noise
  if (params.noiseType == PN_Params::Simplex) params.size *= std::sqrt(2.0);
  params.octaves     = m_octaves->getValue() + 1;
  params.offset      = m_offset->getValue(frame);
  params.p_intensity = m_persistance_intensity->getValue(frame);
  params.p_size      = m_persistance_size->getValue(frame);
  params.p_offset    = m_persistance_offset->getValue(frame);
  params.eyeLevel    = m_eyeLevel->getValue(frame);
  params.alp_rend_sw = m_alpha_rendering->getValue();
  params.waveHeight  = m_waveHeight->getValue(frame);

  double fov = m_fov->getValue(frame);

  TAffine aff  = settings.m_affine;
  double scale = 1.0 / std::sqrt(std::abs(aff.det()));
  params.aff   = TScale(scale) * TTranslation(tile.m_pos);

  params.time        = m_evolution->getValue(frame) * 0.05;
  params.p_evolution = m_persistance_evolution->getValue(frame);

  TPointD eyePoint =
      aff * params.eyeLevel - (tile.m_pos + tile.getRaster()->getCenterD());
  double eyeHeight = eyePoint.y;
  // distance from the bottom of the render region to the eye level
  params.drawLevel = (int)((double)dimOut.ly / 2.0 + eyeHeight);
  if (params.drawLevel > dimOut.ly) params.drawLevel = dimOut.ly;

  //------------------------------------------------------------

  int camHeight = settings.m_cameraBox.getLy();
  TPointD vec_p0p1((double)camHeight * params.aff.a12,
                   (double)camHeight * params.aff.a22);
  params.fy_2 =
      std::sqrt(vec_p0p1.x * vec_p0p1.x + vec_p0p1.y * vec_p0p1.y) / 2.0;

  double fov_radian_2 = (fov / 2.0) * M_PI_180;

  // distance from the camera to the center of projection plane
  double D = params.fy_2 / std::tan(fov_radian_2);
  // distance from the camera to the projected point of the horizon
  params.A = std::sqrt(params.eyeLevel.y * params.eyeLevel.y + D * D);

  // angle between horizon and the vector [ camera center - bottom of the camera
  // frame ]
  double theta = fov_radian_2 + std::asin(params.eyeLevel.y / params.A);

  double M = params.fy_2 / std::sin(fov_radian_2);

  params.cam_pos = double3{0.0, -M * std::cos(theta), M * std::sin(theta)};

  // compute normalize range
  params.base_fresnel_ref = 0.0;
  params.top_fresnel_ref  = 1.0;
  if (params.renderMode == PN_Params::Fresnel &&
      m_normalize_fresnel->getValue()) {
    double phi              = 90.0 - theta * M_180_PI;
    params.base_fresnel_ref = getFresnel(phi);

    // frsnel value at the upper-side corner of the camera frame
    double fx_2 =
        params.fy_2 * (double)settings.m_cameraBox.getLx() / (double)camHeight;
    double side_A    = std::sqrt(fx_2 * fx_2 + params.A * params.A);
    double top_theta = -fov_radian_2 + std::asin(params.eyeLevel.y / side_A);
    phi              = 90.0 - top_theta * M_180_PI;
    params.top_fresnel_ref = getFresnel(phi);

    double marginRatio = m_normalize_margin->getValue(frame);

    // adding margin
    double margin =
        (params.top_fresnel_ref - params.base_fresnel_ref) * marginRatio;
    params.base_fresnel_ref = std::max(0.0, params.base_fresnel_ref - margin);
    params.top_fresnel_ref  = std::min(1.0, params.top_fresnel_ref + margin);
  }

  // for normalizing intensity
  double intensity = 2.0;  // from -1 to 1
  params.int_sum   = 0.0;
  for (int o = 0; o < params.octaves; o++) {
    params.int_sum += intensity;
    intensity *= params.p_intensity;
  }
}

//------------------------------------------------------------

Iwa_PNPerspectiveFx::Iwa_PNPerspectiveFx()
    : m_renderMode(new TIntEnumParam(PN_Params::Noise, "Noise"))
    , m_noiseType(new TIntEnumParam(PN_Params::Perlin, "Perlin Noise"))
    , m_size(10.0)
    , m_evolution(0.0)
    , m_octaves(new TIntEnumParam(0, "1"))
    , m_offset(TPointD(0, 0))
    , m_persistance_intensity(0.5)
    , m_persistance_size(0.5)
    , m_persistance_evolution(0.5)
    , m_persistance_offset(0.5)
    , m_fov(30)
    , m_eyeLevel(TPointD(0, 0))
    , m_alpha_rendering(true)
    , m_waveHeight(10.0)
    , m_normalize_fresnel(false)
    , m_normalize_margin(0.1) {
  bindParam(this, "renderMode", m_renderMode);
  bindParam(this, "noiseType", m_noiseType);
  bindParam(this, "size", m_size);
  bindParam(this, "evolution", m_evolution);
  bindParam(this, "octaves", m_octaves);
  bindParam(this, "offset", m_offset);
  bindParam(this, "persistance_intensity", m_persistance_intensity);
  bindParam(this, "persistance_size", m_persistance_size);
  bindParam(this, "persistance_evolution", m_persistance_evolution);
  bindParam(this, "persistance_offset", m_persistance_offset);
  bindParam(this, "fov", m_fov);
  bindParam(this, "eyeLevel", m_eyeLevel);
  bindParam(this, "alpha_rendering", m_alpha_rendering);
  bindParam(this, "waveHeight", m_waveHeight);
  bindParam(this, "normalize_fresnel", m_normalize_fresnel);
  bindParam(this, "normalize_margin", m_normalize_margin);

  m_noiseType->addItem(PN_Params::Simplex, "Simplex Noise");

  m_renderMode->addItem(PN_Params::Noise_NoResample, "Noise (no resampled)");
  m_renderMode->addItem(PN_Params::WarpHV, "Warp HV offset");
  m_renderMode->addItem(PN_Params::WarpHV2, "Warp HV offset 2");
  m_renderMode->addItem(PN_Params::Fresnel, "Fresnel reflectivity");

  m_size->setMeasureName("fxLength");
  m_size->setValueRange(0.0, 1000.0);

  m_octaves->addItem(1, "2");
  m_octaves->addItem(2, "3");
  m_octaves->addItem(3, "4");
  m_octaves->addItem(4, "5");
  m_octaves->addItem(5, "6");
  m_octaves->addItem(6, "7");
  m_octaves->addItem(7, "8");
  m_octaves->addItem(8, "9");
  m_octaves->addItem(9, "10");

  m_persistance_intensity->setValueRange(0.1, 2.0);
  m_persistance_size->setValueRange(0.1, 2.0);
  m_persistance_evolution->setValueRange(0.1, 2.0);
  m_persistance_offset->setValueRange(0.1, 2.0);

  m_fov->setValueRange(10, 90);

  m_eyeLevel->getX()->setMeasureName("fxLength");
  m_eyeLevel->getY()->setMeasureName("fxLength");

  m_waveHeight->setMeasureName("fxLength");
  m_waveHeight->setValueRange(1.0, 100.0);
  m_normalize_margin->setValueRange(0.0, 3.0);

  enableComputeInFloat(true);
}

//------------------------------------------------------------

bool Iwa_PNPerspectiveFx::doGetBBox(double frame, TRectD &bBox,
                                    const TRenderSettings &info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------

bool Iwa_PNPerspectiveFx::canHandle(const TRenderSettings &info, double frame) {
  return false;
}

//------------------------------------------------------------

void Iwa_PNPerspectiveFx::doCompute(TTile &tile, double frame,
                                    const TRenderSettings &settings) {
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster()) &&
      !((TRasterFP)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());

  // obtain parameters
  PN_Params pnParams;
  getPNParameters(tile, frame, settings, pnParams, dimOut);

  // return if the horizon is below the rendering area
  if (pnParams.drawLevel < 0) {
    tile.getRaster()->clear();
    return;
  }

  double evolution   = m_evolution->getValue(frame);
  double p_evolution = m_persistance_evolution->getValue(frame);

  double4 *out_host;
  // allocate buffer
  TRasterGR8P out_host_ras(sizeof(double4) * dimOut.lx, pnParams.drawLevel);
  out_host_ras->lock();
  out_host = (double4 *)out_host_ras->getRawData();

  doCompute_CPU(frame, settings, out_host, dimOut, pnParams);

  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  TRasterFP outRasF   = (TRasterFP)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(
        out_host, outRas32, dimOut, pnParams.drawLevel, pnParams.alp_rend_sw);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(
        out_host, outRas64, dimOut, pnParams.drawLevel, pnParams.alp_rend_sw);
  else if (outRasF)
    setOutputRaster<TRasterFP, TPixelF>(
        out_host, outRasF, dimOut, pnParams.drawLevel, pnParams.alp_rend_sw);

  out_host_ras->unlock();
}

//------------------------------------------------------------
void Iwa_PNPerspectiveFx::doCompute_CPU(double frame,
                                        const TRenderSettings &settings,
                                        double4 *out_host, TDimensionI &dimOut,
                                        PN_Params &pnParams) {
  if (pnParams.renderMode == PN_Params::Noise ||
      pnParams.renderMode == PN_Params::Noise_NoResample) {
    calcPerinNoise_CPU(out_host, dimOut, pnParams,
                       pnParams.renderMode == PN_Params::Noise);
  } else if (pnParams.renderMode == PN_Params::WarpHV ||
             pnParams.renderMode == PN_Params::Fresnel ||
             pnParams.renderMode == PN_Params::WarpHV2) {
    calcPNNormal_CPU(out_host, dimOut, pnParams);
    if (pnParams.renderMode == PN_Params::WarpHV2) {
      calcPNNormal_CPU(out_host, dimOut, pnParams, true);
    }
  }
}

//------------------------------------------------------------
// render for 2 Noise modes

void Iwa_PNPerspectiveFx::calcPerinNoise_CPU(double4 *out_host,
                                             TDimensionI &dimOut, PN_Params &p,
                                             bool doResample) {
  int reso       = (doResample) ? 10 : 1;
  double4 *out_p = out_host;
  // compute for each pixel
  for (int yy = 0; yy < p.drawLevel; yy++) {
    for (int xx = 0; xx < dimOut.lx; xx++, out_p++) {
      double val_sum = 0.0;
      int count      = 0;
      // for each sampling points
      for (int tt = 0; tt < reso; tt++) {
        for (int ss = 0; ss < reso; ss++) {
          TPointD tmpPixPos(
              (double)xx - 0.5 + ((double)ss + 0.5) / (double)reso,
              (double)yy - 0.5 + ((double)tt + 0.5) / (double)reso);
          TPointD screenPos = p.aff * tmpPixPos;
          // compute coordinate on the noise plane
          TPointD noisePos;
          noisePos.x = -(p.eyeLevel.y + p.fy_2) * (screenPos.x - p.eyeLevel.x) /
                           (screenPos.y - p.eyeLevel.y) +
                       p.eyeLevel.x;
          noisePos.y =
              (p.fy_2 + screenPos.y) * p.A / (p.eyeLevel.y - screenPos.y);
          double tmpVal           = 0.5;
          double currentSize      = p.size;
          TPointD currentOffset   = p.offset;
          double currentIntensity = 1.0;

          double currentEvolution = p.time;

          // sum noise values
          for (int o = 0; o < p.octaves; o++) {
            TPointD currentNoisePos =
                (noisePos - currentOffset) * (1.0 / currentSize);

            if (p.noiseType == PN_Params::Perlin) {
              tmpVal += currentIntensity *
                        Noise1234::noise(currentNoisePos.x, currentNoisePos.y,
                                         currentEvolution) /
                        p.int_sum;
            } else {  // Simplex case
              tmpVal +=
                  currentIntensity *
                  SimplexNoise::noise(currentNoisePos.x, currentNoisePos.y,
                                      currentEvolution) /
                  p.int_sum;
            }

            currentSize *= p.p_size;
            currentOffset.x *= p.p_offset;
            currentOffset.y *= p.p_offset;
            currentIntensity *= p.p_intensity;
            currentEvolution *= p.p_evolution;
          }
          val_sum += tmpVal;
          count += 1;
        }
      }

      double val = val_sum / (double)count;

      (*out_p).x = val;
      (*out_p).y = val;
      (*out_p).z = val;
      if (p.alp_rend_sw)  // clamp
        (*out_p).w = (val < 0.0) ? 0.0 : ((val > 1.0) ? 1.0 : val);
      else
        (*out_p).w = 1.0;
    }
  }
}

//------------------------------------------------------------
// render WarpHV / Fresnel modes

void Iwa_PNPerspectiveFx::calcPNNormal_CPU(double4 *out_host,
                                           TDimensionI &dimOut, PN_Params &p,
                                           bool isSubWave) {
  double4 *out_p = out_host;
  // compute for each pixel
  for (int yy = 0; yy < p.drawLevel; yy++) {
    for (int xx = 0; xx < dimOut.lx; xx++, out_p++) {
      TPointD screenPos = p.aff * TPointD((double)xx, (double)yy);

      // compute coordinate on the noise plane
      TPointD noisePos;
      noisePos.x = -(p.eyeLevel.y + p.fy_2) * (screenPos.x - p.eyeLevel.x) /
                       (screenPos.y - p.eyeLevel.y) +
                   p.eyeLevel.x;
      noisePos.y = (p.fy_2 + screenPos.y) * p.A / (p.eyeLevel.y - screenPos.y);

      double gradient[2];  // 0 : horizontal 1 : vertical
      double delta = 0.001;

      for (int hv = 0; hv < 2; hv++) {
        // initialize gradient
        gradient[hv] = 0.0;

        // compute offset of sampling position
        TPointD neighborNoisePos[2] = {
            TPointD(noisePos.x - ((hv == 0) ? delta : 0.0),
                    noisePos.y - ((hv == 0) ? 0.0 : delta)),
            TPointD(noisePos.x + ((hv == 0) ? delta : 0.0),
                    noisePos.y + ((hv == 0) ? 0.0 : delta))};
        double currentSize      = p.size;
        TPointD currentOffset   = p.offset;
        double currentIntensity = 1.0;
        double currentEvolution = (isSubWave) ? p.time + 100.0 : p.time;
        // for each generation
        for (int o = 0; o < p.octaves; o++, currentSize *= p.p_size,
                 currentOffset.x *= p.p_offset, currentOffset.y *= p.p_offset,
                 currentIntensity *= p.p_intensity) {
          // compute offset noise position
          TPointD currentOffsetNoisePos[2];
          for (int mp = 0; mp < 2; mp++)
            currentOffsetNoisePos[mp] =
                (neighborNoisePos[mp] - currentOffset) * (1.0 / currentSize);

          // sum noise values differences
          double noiseDiff;
          // Perlin Noise
          if (p.noiseType == PN_Params::Perlin) {
            noiseDiff =
                Noise1234::noise(currentOffsetNoisePos[1].x,
                                 currentOffsetNoisePos[1].y, currentEvolution) -
                Noise1234::noise(currentOffsetNoisePos[0].x,
                                 currentOffsetNoisePos[0].y, currentEvolution);
          } else {  // Simplex
            // compute cell indexes
            CellIds neighborIds[2] = {
                SimplexNoise::getCellIds(currentOffsetNoisePos[0].x,
                                         currentOffsetNoisePos[0].y,
                                         currentEvolution),
                SimplexNoise::getCellIds(currentOffsetNoisePos[1].x,
                                         currentOffsetNoisePos[1].y,
                                         currentEvolution)};
            // simply compute difference if points are in the same cell
            if (neighborIds[0] == neighborIds[1]) {
              noiseDiff = SimplexNoise::noise(currentOffsetNoisePos[1].x,
                                              currentOffsetNoisePos[1].y,
                                              currentEvolution) -
                          SimplexNoise::noise(currentOffsetNoisePos[0].x,
                                              currentOffsetNoisePos[0].y,
                                              currentEvolution);
            }
            // use center cell id if points are in the different cells
            else {
              TPointD currentCenterNoisePos =
                  (noisePos - currentOffset) * (1.0 / currentSize);
              CellIds centerIds = SimplexNoise::getCellIds(
                  currentCenterNoisePos.x, currentCenterNoisePos.y,
                  currentEvolution);
              if (neighborIds[0] == centerIds) {
                noiseDiff = SimplexNoise::noise(currentCenterNoisePos.x,
                                                currentCenterNoisePos.y,
                                                currentEvolution) -
                            SimplexNoise::noise(currentOffsetNoisePos[0].x,
                                                currentOffsetNoisePos[0].y,
                                                currentEvolution);
              } else  // if(neighborIds[1] == centerIds)
              {
                noiseDiff = SimplexNoise::noise(currentOffsetNoisePos[1].x,
                                                currentOffsetNoisePos[1].y,
                                                currentEvolution) -
                            SimplexNoise::noise(currentCenterNoisePos.x,
                                                currentCenterNoisePos.y,
                                                currentEvolution);
              }
              // multiply the difference
              // 片端→中心の変位を使っているので、片端→片端に合わせて変位を2倍する
              noiseDiff *= 2.0;
            }
          }
          // sum gradient
          gradient[hv] += currentIntensity * noiseDiff / p.int_sum;

          currentEvolution *= p.p_evolution;
        }
      }

      // compute neighbor vectors
      double3 vec_x  = {delta * 2, 0.0, gradient[0] * p.waveHeight};
      double3 vec_y  = {0.0, delta * 2, gradient[1] * p.waveHeight};
      double3 normal = normalize(cross(vec_x, vec_y));

      // camera to the center of projection plane
      double3 cam_vec = {noisePos.x - p.cam_pos.x, noisePos.y - p.cam_pos.y,
                         -p.cam_pos.z};
      cam_vec         = normalize(cam_vec);
      if (p.renderMode == PN_Params::WarpHV ||
          p.renderMode == PN_Params::WarpHV2) {
        // reflected vector from the plane
        double alpha        = dot(normal, cam_vec);
        double3 reflect_cam = {
            2.0 * alpha * normal.x - cam_vec.x,
            2.0 * alpha * normal.y - cam_vec.y,
            2.0 * alpha * normal.z -
                cam_vec.z};  // the length of this vector should be 1
        // reflection vector if the plane is flat
        double3 reflect_cam_mirror = {cam_vec.x, cam_vec.y, -cam_vec.z};
        // compute the angle difference
        // between the range from -PI/2 to PI/2
        double angle_h = std::atan(reflect_cam.x / reflect_cam.y) -
                         std::atan(reflect_cam_mirror.x / reflect_cam_mirror.y);
        double angle_v = std::atan(reflect_cam.z / reflect_cam.y) -
                         std::atan(reflect_cam_mirror.z / reflect_cam_mirror.y);

        // maximum 30 degrees
        angle_h = 0.5 + angle_h / 0.5236f;
        angle_v = 0.5 - angle_v / 0.5236f;

        // clamp
        angle_h = (angle_h < 0.0) ? 0.0 : ((angle_h > 1.0) ? 1.0 : angle_h);
        angle_v = (angle_v < 0.0) ? 0.0 : ((angle_v > 1.0) ? 1.0 : angle_v);

        if (p.renderMode == PN_Params::WarpHV) {
          (*out_p).x = angle_h;
          (*out_p).y = angle_v;
          (*out_p).z = 0.0;
          (*out_p).w = 1.0;
        } else  //  WarpHV2 case
        {
          if (!isSubWave) {
            (*out_p).y = angle_v;
            (*out_p).z = 0.0;
            (*out_p).w = 1.0;
          } else
            (*out_p).x = angle_v;
        }
      }
      // fresnel refrection mode
      else if (p.renderMode == PN_Params::Fresnel) {
        cam_vec.x *= -1.0;
        cam_vec.y *= -1.0;
        cam_vec.z *= -1.0;
        double diffuse_angle = std::acos(dot(normal, cam_vec)) * M_180_PI;
        double fresnel_ref   = getFresnel(diffuse_angle);
        double ref           = (fresnel_ref - p.base_fresnel_ref) /
                     (p.top_fresnel_ref - p.base_fresnel_ref);

        // clamp
        ref        = (ref < 0.0) ? 0.0 : ((ref > 1.0) ? 1.0 : ref);
        (*out_p).x = ref;
        (*out_p).y = ref;
        (*out_p).z = ref;
        (*out_p).w = (p.alp_rend_sw) ? ref : 1.0;
      }
    }
  }
}

//------------------------------------------------------------

void Iwa_PNPerspectiveFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 1];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Eye Level";
  concepts[0].m_params.push_back(m_eyeLevel);
}

FX_PLUGIN_IDENTIFIER(Iwa_PNPerspectiveFx, "iwa_PNPerspectiveFx");
