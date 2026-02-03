#include "iwa_adjustexposurefx.h"

#include "tfxparam.h"

/*------------------------------------------------------------
 ソース画像を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_AdjustExposureFx::setSourceRaster(const RASTER srcRas, float4 *dstMem,
                                           TDimensionI dim) {
  float4 *chann_p = dstMem;

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;

      pix++;
      chann_p++;
    }
  }
}

void Iwa_AdjustExposureFx::setSourceRasterF(const TRasterFP srcRas,
                                            float4 *dstMem, TDimensionI dim) {
  float4 *chann_p = dstMem;

  for (int j = 0; j < dim.ly; j++) {
    TPixelF *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      (*chann_p).x = pix->r;
      (*chann_p).y = pix->g;
      (*chann_p).z = pix->b;
      (*chann_p).w = pix->m;

      pix++;
      chann_p++;
    }
  }
}
/*------------------------------------------------------------
 出力結果をChannel値に変換してタイルに格納
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_AdjustExposureFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                           TDimensionI dim) {
  float4 *chan_p = srcMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = dstRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
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

      pix++;
      chan_p++;
    }
  }
}

void Iwa_AdjustExposureFx::setOutputRasterF(float4 *srcMem,
                                            const TRasterFP dstRas,
                                            TDimensionI dim) {
  float4 *chan_p = srcMem;
  for (int j = 0; j < dim.ly; j++) {
    TPixelF *pix = dstRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      pix->r = (*chan_p).x;
      pix->g = (*chan_p).y;
      pix->b = (*chan_p).z;
      pix->m = (*chan_p).w;
      pix++;
      chan_p++;
    }
  }
}

//------------------------------------------------

Iwa_AdjustExposureFx::Iwa_AdjustExposureFx()
    : m_hardness(3.3)
    , m_scale(0.0)
    , m_offset(0.0)
    , m_gamma(2.2)
    , m_gammaAdjust(0.) {
  addInputPort("Source", m_source);
  bindParam(this, "hardness", m_hardness, false);
  bindParam(this, "gamma", m_gamma);
  bindParam(this, "gammaAdjust", m_gammaAdjust);
  bindParam(this, "scale", m_scale, false);
  bindParam(this, "offset", m_offset, false);

  m_hardness->setValueRange(0.05, 20.0);
  m_gamma->setValueRange(1.0, 10.0);
  m_gammaAdjust->setValueRange(-5., 5.);
  m_scale->setValueRange(-10.0, 10.0);
  m_offset->setValueRange(-0.5, 0.5);
  enableComputeInFloat(true);

  // Version 1: Exposure is computed by using Hardness
  //            E = std::pow(10.0, (value - 0.5) / hardness)
  // Version 2: Exposure is computed by using Gamma, for easier combination with
  // the linear color space
  //            E = std::pow(value, gamma)
  // Version 3: Gamma is computed by rs.m_colorSpaceGamma + gammaAdjust
  // this must be called after binding the parameters (see onFxVersionSet())
  setFxVersion(3);
}

//--------------------------------------------

void Iwa_AdjustExposureFx::onFxVersionSet() {
  if (getFxVersion() == 1) {  // use hardness
    getParams()->getParamVar("hardness")->setIsHidden(false);
    getParams()->getParamVar("gamma")->setIsHidden(true);
    getParams()->getParamVar("gammaAdjust")->setIsHidden(true);
    return;
  }
  getParams()->getParamVar("hardness")->setIsHidden(true);

  bool useGamma = getFxVersion() == 2;
  if (useGamma) {
    // Automatically update version
    if (m_gamma->getKeyframeCount() == 0 &&
        areAlmostEqual(m_gamma->getDefaultValue(), 2.2)) {
      useGamma = false;
      setFxVersion(3);
    }
  }
  getParams()->getParamVar("gamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("gammaAdjust")->setIsHidden(useGamma);
}

//------------------------------------------------

void Iwa_AdjustExposureFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &settings) {
  /*- Sourceが無ければreturn -*/
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  double gamma;
  if (getFxVersion() == 1)  // hardness
    gamma = m_hardness->getValue(frame);
  else {  // gamma
    if (getFxVersion() == 2)
      gamma = m_gamma->getValue(frame);
    else
      gamma = std::max(
          1., settings.m_colorSpaceGamma + m_gammaAdjust->getValue(frame));
    if (tile.getRaster()->isLinear()) gamma /= settings.m_colorSpaceGamma;
  }

  m_source->compute(tile, frame, settings);

  TDimensionI dim(tile.getRaster()->getSize());

  if (TRasterFP rasF = tile.getRaster()) {
    if (getFxVersion() == 1)
      doFloatCompute(rasF, frame, dim,
                     HardnessBasedConverter(gamma, settings.m_colorSpaceGamma,
                                            tile.getRaster()->isLinear()));
    else
      doFloatCompute(rasF, frame, dim, GammaBasedConverter(gamma));
    return;
  }

  float4 *tile_host;

  /*- ホストメモリ確保 -*/
  TRasterGR8P tile_host_ras(sizeof(float4) * dim.lx, dim.ly);
  tile_host_ras->lock();
  tile_host = (float4 *)tile_host_ras->getRawData();

  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();

  if (ras32)
    setSourceRaster<TRaster32P, TPixel32>(ras32, tile_host, dim);
  else if (ras64)
    setSourceRaster<TRaster64P, TPixel64>(ras64, tile_host, dim);

  if (getFxVersion() == 1)
    doCompute_CPU(frame, dim, tile_host,
                  HardnessBasedConverter(gamma, settings.m_colorSpaceGamma,
                                         tile.getRaster()->isLinear()));
  else
    doCompute_CPU(frame, dim, tile_host, GammaBasedConverter(gamma));

  /*-  出力結果をチャンネル値に変換 -*/
  tile.getRaster()->clear();
  if (ras32)
    setOutputRaster<TRaster32P, TPixel32>(tile_host, ras32, dim);
  else if (ras64)
    setOutputRaster<TRaster64P, TPixel64>(tile_host, ras64, dim);

  tile_host_ras->unlock();
}

//------------------------------------------------

void Iwa_AdjustExposureFx::doCompute_CPU(double frame, TDimensionI &dim,
                                         float4 *tile_host,
                                         const ExposureConverter &conv) {
  float scale  = (float)m_scale->getValue(frame);
  float offset = (float)m_offset->getValue(frame);

  float exposureOffset = (conv.valueToExposure(std::abs(offset) + 0.5) -
                          conv.valueToExposure(0.5)) *
                         ((offset < 0.0f) ? -1.0f : 1.0f);

  float4 *pix = tile_host;

  int size = dim.lx * dim.ly;
  for (int i = 0; i < size; i++, pix++) {
    /*- RGB->Exposure -*/
    pix->x = conv.valueToExposure(pix->x);
    pix->y = conv.valueToExposure(pix->y);
    pix->z = conv.valueToExposure(pix->z);

    /*- スケール -*/
    pix->x *= powf(10, scale);
    pix->y *= powf(10, scale);
    pix->z *= powf(10, scale);

    /*- オフセット -*/
    pix->x += exposureOffset;
    pix->y += exposureOffset;
    pix->z += exposureOffset;

    /*- Exposure->RGB -*/
    pix->x = (pix->x < 0.0f) ? 0.0f : conv.exposureToValue(pix->x);
    pix->y = (pix->y < 0.0f) ? 0.0f : conv.exposureToValue(pix->y);
    pix->z = (pix->z < 0.0f) ? 0.0f : conv.exposureToValue(pix->z);
  }
}

//------------------------------------------------

void Iwa_AdjustExposureFx::doFloatCompute(const TRasterFP rasD, double frame,
                                          TDimensionI &dim,
                                          const ExposureConverter &conv) {
  double scale          = m_scale->getValue(frame);
  double offset         = m_offset->getValue(frame);
  double exposureOffset = (conv.valueToExposure(std::abs(offset) + 0.5) -
                           conv.valueToExposure(0.5)) *
                          ((offset < 0.) ? -1. : 1.);

  for (int j = 0; j < dim.ly; j++) {
    TPixelF *pix = rasD->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      for (int c = 0; c < 3; c++) {
        float *val = (c == 0) ? &pix->r : (c == 1) ? &pix->g : &pix->b;
        /*- RGB->Exposure -*/
        *val = conv.valueToExposure(*val);
        /*- スケール -*/
        *val *= pow(10.0, scale);
        /*- オフセット -*/
        *val += exposureOffset;
        /*- Exposure->RGB -*/
        *val = (*val < 0.f) ? 0.f : conv.exposureToValue(*val);
      }
      pix++;
    }
  }
}

//------------------------------------------------

bool Iwa_AdjustExposureFx::doGetBBox(double frame, TRectD &bBox,
                                     const TRenderSettings &info) {
  if (m_source.isConnected()) {
    bool ret = m_source->doGetBBox(frame, bBox, info);
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
}

//------------------------------------------------

bool Iwa_AdjustExposureFx::canHandle(const TRenderSettings &info,
                                     double frame) {
  return true;
}

//------------------------------------------------

bool Iwa_AdjustExposureFx::toBeComputedInLinearColorSpace(
    bool settingsIsLinear, bool tileIsLinear) const {
  // 下流がリニア計算するかどうかで判断。これで変換を最小限にできるか…？
  return tileIsLinear;
}

FX_PLUGIN_IDENTIFIER(Iwa_AdjustExposureFx, "iwa_AdjustExposureFx")
