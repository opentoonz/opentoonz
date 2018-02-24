/*------------------------------------
Iwa_BarrelDistortFx
Generates the barrel/pincushion distort.
Based on an approximated model for radial distortion
by Fitzgibbon, 2001
------------------------------------*/

#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tparamuiconcept.h"

#include <QVector2D>
#include <QPointF>

namespace {
struct float4 {
  float x, y, z, w;
};
/*------------------------------------------------------------
read the source image, normalize to 0 - 1
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim) {
  float4 *chann_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, chann_p++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;
    }
  }
}

/*------------------------------------------------------------
convert the result to channel value and store to the output raster
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void setOutputRaster(float4 *srcMem, const RASTER dstRas) {
  typename PIXEL::Channel halfChan =
      (typename PIXEL::Channel)(PIXEL::maxChannelValue / 2);

  dstRas->fill(PIXEL::Transparent);

  float4 *chan_p = srcMem;
  for (int j = 0; j < dstRas->getLy(); j++) {
    PIXEL *pix = dstRas->pixels(j);
    for (int i = 0; i < dstRas->getLx(); i++, chan_p++, pix++) {
      float val;
      val    = (*chan_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
    }
  }
}

float4 getSource_CPU(float4 *source_host, TDimensionI &dim, int pos_x,
                     int pos_y) {
  if (pos_x < 0 || pos_x >= dim.lx || pos_y < 0 || pos_y >= dim.ly)
    return float4{0.0f, 0.0f, 0.0f, 0.0f};

  return source_host[pos_y * dim.lx + pos_x];
}

float4 interp_CPU(float4 val1, float4 val2, float ratio) {
  return float4{(1.0f - ratio) * val1.x + ratio * val2.x,
                (1.0f - ratio) * val1.y + ratio * val2.y,
                (1.0f - ratio) * val1.z + ratio * val2.z,
                (1.0f - ratio) * val1.w + ratio * val2.w};
}
float adjustExposure(float source, float distance, float amount, float gamma,
                     float midpoint) {
  float scale = (distance < midpoint)
                    ? 0.0f
                    : amount * (distance - midpoint) / (1.0f - midpoint);

  float ret = powf(10, (source - 0.5f) * gamma);

  ret *= powf(10, scale);

  ret = log10f(ret) / gamma + 0.5f;

  return (ret > 1.0f) ? 1.0f : ((ret < 0.0f) ? 0.0f : ret);
}
};

class Iwa_BarrelDistortFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_BarrelDistortFx)

  TRasterFxPort m_source;
  TPointParamP m_point;
  TDoubleParamP m_distortion;
  TDoubleParamP m_distortionAspect;
  TDoubleParamP m_precision;
  TDoubleParamP m_chromaticAberration;
  TDoubleParamP m_vignetteAmount;
  TDoubleParamP m_vignetteGamma;
  TDoubleParamP m_vignetteMidpoint;
  TDoubleParamP m_scale;

public:
  Iwa_BarrelDistortFx()
      : m_point(TPointD(0.0, 0.0))
      , m_distortion(0.0)
      , m_distortionAspect(1.0)
      , m_precision(1.0)
      , m_chromaticAberration(0.0)
      , m_vignetteAmount(0.0)
      , m_vignetteGamma(1.0)
      , m_vignetteMidpoint(0.5)
      , m_scale(1.0) {
    m_point->getX()->setMeasureName("fxLength");
    m_point->getY()->setMeasureName("fxLength");
    bindParam(this, "center", m_point);
    bindParam(this, "distortion", m_distortion);
    bindParam(this, "distortionAspect", m_distortionAspect);
    bindParam(this, "precision", m_precision);
    bindParam(this, "chromaticAberration", m_chromaticAberration);
    bindParam(this, "vignetteAmount", m_vignetteAmount);
    bindParam(this, "vignetteGamma", m_vignetteGamma);
    bindParam(this, "vignetteMidpoint", m_vignetteMidpoint);
    bindParam(this, "scale", m_scale);

    addInputPort("Source", m_source);
    m_distortion->setValueRange(-2.0, 2.0);
    m_distortionAspect->setValueRange(0.2, 5.0);
    m_precision->setValueRange(1.0, 3.0);
    m_chromaticAberration->setValueRange(-0.1, 0.1);
    m_vignetteAmount->setValueRange(-1.0, 1.0);
    m_vignetteGamma->setValueRange(0.05, 20.0);
    m_vignetteMidpoint->setValueRange(0.0, 1.0);
    m_scale->setValueRange(0.1, 2.0);
  }

  ~Iwa_BarrelDistortFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_source.isConnected()) {
      bool ret      = m_source->doGetBBox(frame, bBox, info);
      if (ret) bBox = TConsts::infiniteRectD;
      return ret;
    }
    return false;
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  void doCompute_CPU(TPointD &point, TPointD &sourcePoint, float dist,
                     float distAspect, float4 *source_host, float4 *result_host,
                     TDimensionI &outDim, TDimensionI &sourceDim,
                     TPointD &offset, float precision, double vignetteAmount,
                     double vignetteGamma, double vignetteMidpoint,
                     float scale);

  void doCompute_chroma_CPU(TPointD &point, TPointD &sourcePoint, float dist,
                            float distAspect, float chroma, float4 *source_host,
                            float4 *result_host, TDimensionI &outDim,
                            TDimensionI &sourceDim, TPointD &offset,
                            float precision, double vignetteAmount,
                            double vignetteGamma, double vignetteMidpoint,
                            float scale);

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Center";
    concepts[0].m_params.push_back(m_point);
  }
};

//------------------------------------------------------------------

void Iwa_BarrelDistortFx::doCompute(TTile &tile, double frame,
                                    const TRenderSettings &ri) {
  if (!m_source.isConnected()) return;

  TPointD point = m_point->getValue(frame);
  TAffine aff   = ri.m_affine;
  // convert the coordinate to with origin at bottom-left corner of the camera
  point = aff * point - (tile.m_pos + tile.getRaster()->getCenterD()) +
          TPointD(ri.m_cameraBox.getLx() / 2.0, ri.m_cameraBox.getLy() / 2.0);
  double dist       = m_distortion->getValue(frame);
  double distAspect = m_distortionAspect->getValue(frame);
  double scale      = m_scale->getValue(frame);

  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));
  TDimensionI outDim(rectOut.getLx(), rectOut.getLy());

  float precision = m_precision->getValue(frame);
  TRenderSettings source_ri(ri);
  TPointD sourcePoint(point);
  if (precision > 1.0) {
    source_ri.m_affine *= TScale(precision, precision);
    sourcePoint = source_ri.m_affine * m_point->getValue(frame) -
                  (tile.m_pos + tile.getRaster()->getCenterD()) +
                  TPointD(source_ri.m_cameraBox.getLx() / 2.0,
                          source_ri.m_cameraBox.getLy() / 2.0);
  }

  TRectD sourceBBox;
  m_source->getBBox(frame, sourceBBox, source_ri);
  TDimensionI sourceDim;
  if (sourceBBox == TConsts::infiniteRectD) {
    TPointD tileOffset = tile.m_pos + tile.getRaster()->getCenterD();
    sourceBBox         = TRectD(TPointD(source_ri.m_cameraBox.x0 + tileOffset.x,
                                source_ri.m_cameraBox.y0 + tileOffset.y),
                        TDimensionD(source_ri.m_cameraBox.getLx(),
                                    source_ri.m_cameraBox.getLy()));
  }
  sourceDim.lx = std::ceil(sourceBBox.getLx());
  sourceDim.ly = std::ceil(sourceBBox.getLy());

  TTile sourceTile;
  m_source->allocateAndCompute(sourceTile, sourceBBox.getP00(), sourceDim,
                               tile.getRaster(), frame, source_ri);

  float4 *source_host;
  TRasterGR8P source_host_ras(sourceDim.lx * sizeof(float4), sourceDim.ly);
  source_host_ras->lock();
  source_host = (float4 *)source_host_ras->getRawData();

  TRaster32P ras32 = (TRaster32P)sourceTile.getRaster();
  TRaster64P ras64 = (TRaster64P)sourceTile.getRaster();
  if (ras32)
    setSourceRaster<TRaster32P, TPixel32>(ras32, source_host, sourceDim);
  else if (ras64)
    setSourceRaster<TRaster64P, TPixel64>(ras64, source_host, sourceDim);

  TRasterGR8P result_host_ras(outDim.lx * sizeof(float4), outDim.ly);

  // memory to store the result
  float4 *result_host;
  result_host_ras->lock();
  result_host = (float4 *)result_host_ras->getRawData();

  TPointD offset = sourceTile.m_pos - tile.m_pos;

  double vignetteAmount   = m_vignetteAmount->getValue(frame);
  double vignetteGamma    = m_vignetteGamma->getValue(frame);
  double vignetteMidpoint = m_vignetteMidpoint->getValue(frame);

  double chroma = m_chromaticAberration->getValue(frame);
  if (areAlmostEqual(chroma, 0.0))
    doCompute_CPU(point, sourcePoint, dist, distAspect, source_host,
                  result_host, outDim, sourceDim, offset, precision,
                  vignetteAmount, vignetteGamma, vignetteMidpoint, scale);
  else
    doCompute_chroma_CPU(point, sourcePoint, dist, distAspect, chroma,
                         source_host, result_host, outDim, sourceDim, offset,
                         precision, vignetteAmount, vignetteGamma,
                         vignetteMidpoint, scale);

  source_host_ras->unlock();

  // convert the result to channel value and store to the output raster
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(result_host, outRas32);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(result_host, outRas64);

  result_host_ras->unlock();
}
//------------------------------------------------------------------

void Iwa_BarrelDistortFx::doCompute_CPU(
    TPointD &point, TPointD &sourcePoint, float dist, float distAspect,
    float4 *source_host, float4 *result_host, TDimensionI &outDim,
    TDimensionI &sourceDim, TPointD &offset, float precision,
    double vignetteAmount, double vignetteGamma, double vignetteMidpoint,
    float scale) {
  float4 *result_p  = result_host;
  float squaredSize = (float)(outDim.lx * outDim.lx) * 0.25f;
  QPointF offsetCenter(sourcePoint.x - offset.x, sourcePoint.y - offset.y);

  bool doVignette = !areAlmostEqual(vignetteAmount, 0.0);

  // Only the case of barrel distortion (dist < 0) , decrease the sample
  // position.
  // It will enlarge the result image.
  // Such adjustment can be seen in Lens Correction feature of PhotoShop.
  float sizeAdjust = (dist < 0.0f) ? (1.0f + dist) : 1.0f;

  QVector2D distAspectVec(1, 1);
  if (distAspect > 0.0f && distAspect != 1.0f) {
    float aspectSqrt = std::sqrt(distAspect);
    if (dist < 0.0f)
      distAspectVec = QVector2D(1.0 / aspectSqrt, aspectSqrt);
    else
      distAspectVec = QVector2D(aspectSqrt, 1.0 / aspectSqrt);
  }

  for (int j = 0; j < outDim.ly; j++) {
    for (int i = 0; i < outDim.lx; i++, result_p++) {
      QVector2D ru(QPointF((float)i, (float)j) - QPointF(point.x, point.y));
      // apply global scaling
      ru /= scale;
      float val = (ru * distAspectVec).lengthSquared() / squaredSize;
      if (dist > 0.0f && val > 1.0f / dist) {
        (*result_p) = {0.0f, 0.0f, 0.0f, 0.0f};
        continue;
      }
      float distortRatio = sizeAdjust / (1.0f + dist * val);
      QVector2D rd       = distortRatio * ru;
      QPointF samplePos  = offsetCenter + rd.toPointF() * precision;
      if (samplePos.x() <= -1.0f || samplePos.x() >= (float)(sourceDim.lx) ||
          samplePos.y() <= -1.0f || samplePos.y() >= (float)(sourceDim.ly)) {
        (*result_p) = {0.0f, 0.0f, 0.0f, 0.0f};
        continue;
      }
      QPoint pos(std::floor(samplePos.x()), std::floor(samplePos.y()));
      QPointF ratio(samplePos.x() - (float)pos.x(),
                    samplePos.y() - (float)pos.y());
      (*result_p) = interp_CPU(
          interp_CPU(
              getSource_CPU(source_host, sourceDim, pos.x(), pos.y()),
              getSource_CPU(source_host, sourceDim, pos.x() + 1, pos.y()),
              ratio.x()),
          interp_CPU(
              getSource_CPU(source_host, sourceDim, pos.x(), pos.y() + 1),
              getSource_CPU(source_host, sourceDim, pos.x() + 1, pos.y() + 1),
              ratio.x()),
          ratio.y());
      if (doVignette) {
        float distance = distortRatio * distortRatio * val;
        (*result_p).x  = adjustExposure((*result_p).x, distance, vignetteAmount,
                                       vignetteGamma, vignetteMidpoint);
        (*result_p).y = adjustExposure((*result_p).y, distance, vignetteAmount,
                                       vignetteGamma, vignetteMidpoint);
        (*result_p).z = adjustExposure((*result_p).z, distance, vignetteAmount,
                                       vignetteGamma, vignetteMidpoint);
      }
    }
  }
}

//------------------------------------------------------------------

void Iwa_BarrelDistortFx::doCompute_chroma_CPU(
    TPointD &point, TPointD &sourcePoint, float dist, float distAspect,
    float chroma, float4 *source_host, float4 *result_host, TDimensionI &outDim,
    TDimensionI &sourceDim, TPointD &offset, float precision,
    double vignetteAmount, double vignetteGamma, double vignetteMidpoint,
    float scale) {
  float4 *result_p  = result_host;
  float squaredSize = (float)(outDim.lx * outDim.lx) * 0.25f;
  QPointF offsetCenter(sourcePoint.x - offset.x, sourcePoint.y - offset.y);
  float dist_ch[3] = {dist + chroma, dist, dist - chroma};

  bool doVignette = !areAlmostEqual(vignetteAmount, 0.0);

  // Only the case of barrel distortion (dist < 0) , decrease the sample
  // position.
  // It will enlarge the result image.
  // Such adjustment can be seen in Lens Correction feature of PhotoShop.
  float sizeAdjust = (dist < 0.0f) ? (1.0f + dist) : 1.0f;

  QVector2D distAspectVec(1, 1);
  if (distAspect > 0.0f && distAspect != 1.0f) {
    float aspectSqrt = std::sqrt(distAspect);
    if (dist < 0.0f)
      distAspectVec = QVector2D(1.0 / aspectSqrt, aspectSqrt);
    else
      distAspectVec = QVector2D(aspectSqrt, 1.0 / aspectSqrt);
  }

  for (int j = 0; j < outDim.ly; j++) {
    for (int i = 0; i < outDim.lx; i++, result_p++) {
      for (int c = 0; c < 3; c++) {
        QVector2D ru(QPointF((float)i, (float)j) - QPointF(point.x, point.y));
        // apply global scaling
        ru /= scale;
        float val = (ru * distAspectVec).lengthSquared() / squaredSize;
        if (dist_ch[c] > 0.0f && val > 1.0f / dist_ch[c]) {
          (*result_p) = {0.0f, 0.0f, 0.0f, 0.0f};
          continue;
        }
        float distortRatio = sizeAdjust / (1.0f + dist_ch[c] * val);
        QVector2D rd       = distortRatio * ru;
        QPointF samplePos  = offsetCenter + rd.toPointF() * precision;
        if (samplePos.x() <= -1.0f || samplePos.x() >= (float)(sourceDim.lx) ||
            samplePos.y() <= -1.0f || samplePos.y() >= (float)(sourceDim.ly)) {
          (*result_p) = {0.0f, 0.0f, 0.0f, 0.0f};
          continue;
        }
        QPoint pos(std::floor(samplePos.x()), std::floor(samplePos.y()));
        QPointF ratio(samplePos.x() - (float)pos.x(),
                      samplePos.y() - (float)pos.y());
        float4 result_tmp = interp_CPU(
            interp_CPU(
                getSource_CPU(source_host, sourceDim, pos.x(), pos.y()),
                getSource_CPU(source_host, sourceDim, pos.x() + 1, pos.y()),
                ratio.x()),
            interp_CPU(
                getSource_CPU(source_host, sourceDim, pos.x(), pos.y() + 1),
                getSource_CPU(source_host, sourceDim, pos.x() + 1, pos.y() + 1),
                ratio.x()),
            ratio.y());
        switch (c) {
        case 0:
          (*result_p).x = result_tmp.x;
          if (doVignette)
            (*result_p).x =
                adjustExposure((*result_p).x, distortRatio * distortRatio * val,
                               vignetteAmount, vignetteGamma, vignetteMidpoint);
          break;
        case 1:
          (*result_p).y = result_tmp.y;
          (*result_p).w = result_tmp.w;
          if (doVignette)
            (*result_p).y =
                adjustExposure((*result_p).y, distortRatio * distortRatio * val,
                               vignetteAmount, vignetteGamma, vignetteMidpoint);
          break;
        case 2:
          (*result_p).z = result_tmp.z;
          if (doVignette)
            (*result_p).z =
                adjustExposure((*result_p).z, distortRatio * distortRatio * val,
                               vignetteAmount, vignetteGamma, vignetteMidpoint);
          break;
        default:
          break;
        }
      }
    }
  }
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_BarrelDistortFx, "iwa_BarrelDistortFx")