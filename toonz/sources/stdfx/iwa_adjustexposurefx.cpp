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

//------------------------------------------------

Iwa_AdjustExposureFx::Iwa_AdjustExposureFx()
    : m_hardness(3.3), m_scale(0.0), m_offset(0.0) {
  addInputPort("Source", m_source);
  bindParam(this, "hardness", m_hardness, false);
  bindParam(this, "scale", m_scale, false);
  bindParam(this, "offset", m_offset, false);

  m_hardness->setValueRange(0.05, 20.0);
  m_scale->setValueRange(-10.0, 10.0);
  m_offset->setValueRange(-0.5, 0.5);
}

//------------------------------------------------

void Iwa_AdjustExposureFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &settings) {
  /*- Sourceが無ければreturn -*/
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  m_source->compute(tile, frame, settings);

  float4 *tile_host;
  TDimensionI dim(tile.getRaster()->getSize());

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

  doCompute_CPU(tile, frame, settings, dim, tile_host);

  /*-  出力結果をチャンネル値に変換 -*/
  tile.getRaster()->clear();
  if (ras32)
    setOutputRaster<TRaster32P, TPixel32>(tile_host, ras32, dim);
  else if (ras64)
    setOutputRaster<TRaster64P, TPixel64>(tile_host, ras64, dim);

  tile_host_ras->unlock();
}

//------------------------------------------------

void Iwa_AdjustExposureFx::doCompute_CPU(TTile &tile, double frame,
                                         const TRenderSettings &settings,
                                         TDimensionI &dim, float4 *tile_host) {
  float hardness = (float)m_hardness->getValue(frame);
  float scale    = (float)m_scale->getValue(frame);
  float offset   = (float)m_offset->getValue(frame);

  float exposureOffset = (powf(10.0f, (float)(std::abs(offset) / hardness)) - 1.0f) *
                         ((offset < 0.0f) ? -1.0f : 1.0f);

  float4 *pix = tile_host;

  int size = dim.lx * dim.ly;
  for (int i = 0; i < size; i++, pix++) {
    /*- RGB->Exposure -*/
    pix->x = powf(10, (pix->x - 0.5f) * hardness);
    pix->y = powf(10, (pix->y - 0.5f) * hardness);
    pix->z = powf(10, (pix->z - 0.5f) * hardness);

    /*- スケール -*/
    pix->x *= powf(10, scale);
    pix->y *= powf(10, scale);
    pix->z *= powf(10, scale);

    /*- オフセット -*/
    pix->x += exposureOffset;
    pix->y += exposureOffset;
    pix->z += exposureOffset;

    /*- Exposure->RGB -*/
    pix->x = log10f(pix->x) / hardness + 0.5f;
    pix->y = log10f(pix->y) / hardness + 0.5f;
    pix->z = log10f(pix->z) / hardness + 0.5f;

    /*- クランプ -*/
    pix->x = (pix->x > 1.0f) ? 1.0f : ((pix->x < 0.0f) ? 0.0f : pix->x);
    pix->y = (pix->y > 1.0f) ? 1.0f : ((pix->y < 0.0f) ? 0.0f : pix->y);
    pix->z = (pix->z > 1.0f) ? 1.0f : ((pix->z < 0.0f) ? 0.0f : pix->z);
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

FX_PLUGIN_IDENTIFIER(Iwa_AdjustExposureFx, "iwa_AdjustExposureFx")
