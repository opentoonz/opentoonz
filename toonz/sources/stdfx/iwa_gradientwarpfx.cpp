/*------------------------------------
 Iwa_GradientWarpFx iwasawa
 参照画像の勾配方向にWarpするエフェクト
------------------------------------*/

#include "iwa_gradientwarpfx.h"

/*------------------------------------------------------------
 ソース画像を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_GradientWarpFx::setSourceRaster(const RASTER srcRas, float4 *dstMem,
                                         TDimensionI dim) {
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
 参照画像の輝度値を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_GradientWarpFx::setWarperRaster(const RASTER warperRas, float *dstMem,
                                         TDimensionI dim) {
  float *chann_p = dstMem;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = warperRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, chann_p++) {
      float r = (float)pix->r / (float)PIXEL::maxChannelValue;
      float g = (float)pix->g / (float)PIXEL::maxChannelValue;
      float b = (float)pix->b / (float)PIXEL::maxChannelValue;

      (*chann_p) = 0.298912f * r + 0.586611f * g + 0.114478f * b;
    }
  }
}

/*------------------------------------------------------------
 出力結果をChannel値に変換してタイルに格納
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_GradientWarpFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                         TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    PIXEL *pix     = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++, pix++, chan_p++) {
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

//------------------------------------

Iwa_GradientWarpFx::Iwa_GradientWarpFx()
    : m_h_maxlen(0.0), m_v_maxlen(0.0), m_scale(1.0) {
  /*- 共通パラメータのバインド -*/
  addInputPort("Source", m_source);
  addInputPort("Warper", m_warper);
  bindParam(this, "h_maxlen", m_h_maxlen);
  bindParam(this, "v_maxlen", m_v_maxlen);
  bindParam(this, "scale", m_scale);

  m_h_maxlen->setMeasureName("fxLength");
  m_v_maxlen->setMeasureName("fxLength");
  m_h_maxlen->setValueRange(-100.0, 100.0);
  m_v_maxlen->setValueRange(-100.0, 100.0);
  m_scale->setValueRange(1.0, 100.0);
}

//------------------------------------

void Iwa_GradientWarpFx::doCompute(TTile &tile, double frame,
                                   const TRenderSettings &settings) {
  /*- ソース画像が刺さっていなければreturn -*/
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  /*- 参照画像が刺さっていなければ、ソース画像をそのまま返す -*/
  if (!m_warper.isConnected()) {
    m_source->compute(tile, frame, settings);
    return;
  }

  /*-  計算パラメータを得る -*/
  /*- 移動距離のピクセルサイズ -*/
  /*--- 拡大縮小(移動回転しないで)のGeometryを反映させる ---*/
  double k       = sqrt(fabs(settings.m_affine.det()));
  double hLength = m_h_maxlen->getValue(frame) * k;
  double vLength = m_v_maxlen->getValue(frame) * k;

  double scale = m_scale->getValue(frame);

  /*- ワープ距離が０なら、ソース画像をそのまま返す -*/
  if (hLength == 0.0 && vLength == 0.0) {
    m_source->compute(tile, frame, settings);
    return;
  }

  int margin = static_cast<int>(
      ceil((std::abs(hLength) < std::abs(vLength)) ? std::abs(vLength) : std::abs(hLength)));

  /*- 素材計算範囲を計算 -*/
  /*- 出力範囲 -*/
  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));
  TRectD enlargedRect = rectOut.enlarge((double)margin);
  TDimensionI enlargedDim((int)enlargedRect.getLx(), (int)enlargedRect.getLy());

  /*- ソース画像を正規化して格納 -*/
  float4 *source_host;
  TRasterGR8P source_host_ras(enlargedDim.lx * sizeof(float4), enlargedDim.ly);
  source_host_ras->lock();
  source_host = (float4 *)source_host_ras->getRawData();
  {
    /*-
     * タイルはこのフォーカス内だけ使用。正規化してsource_hostに取り込んだらもう使わない。
     * -*/
    TTile sourceTile;
    m_source->allocateAndCompute(sourceTile, enlargedRect.getP00(), enlargedDim,
                                 tile.getRaster(), frame, settings);
    /*- タイルの画像を０〜１に正規化してホストメモリに読み込む -*/
    TRaster32P ras32 = (TRaster32P)sourceTile.getRaster();
    TRaster64P ras64 = (TRaster64P)sourceTile.getRaster();
    if (ras32)
      setSourceRaster<TRaster32P, TPixel32>(ras32, source_host, enlargedDim);
    else if (ras64)
      setSourceRaster<TRaster64P, TPixel64>(ras64, source_host, enlargedDim);
  }

  /*- 参照画像を正規化して格納 -*/
  float *warper_host;
  TRasterGR8P warper_host_ras(enlargedDim.lx * sizeof(float), enlargedDim.ly);
  warper_host_ras->lock();
  warper_host = (float *)warper_host_ras->getRawData();
  {
    /*-
     * タイルはこのフォーカス内だけ使用。正規化してwarper_hostに取り込んだらもう使わない
     * -*/
    TTile warperTile;
    m_warper->allocateAndCompute(warperTile, enlargedRect.getP00(), enlargedDim,
                                 tile.getRaster(), frame, settings);
    /*- タイルの画像の輝度値を０〜１に正規化してホストメモリに読み込む -*/
    TRaster32P ras32 = (TRaster32P)warperTile.getRaster();
    TRaster64P ras64 = (TRaster64P)warperTile.getRaster();
    if (ras32)
      setWarperRaster<TRaster32P, TPixel32>(ras32, warper_host, enlargedDim);
    else if (ras64)
      setWarperRaster<TRaster64P, TPixel64>(ras64, warper_host, enlargedDim);
  }

  /*- 変位値をScale倍して増やす -*/
  hLength *= scale;
  vLength *= scale;

  TRasterGR8P result_host_ras;

  result_host_ras =
      TRasterGR8P(enlargedDim.lx * sizeof(float4), enlargedDim.ly);
  /*- 結果を収めるメモリ -*/
  float4 *result_host;
  result_host_ras->lock();
  result_host = (float4 *)result_host_ras->getRawData();
  doCompute_CPU(tile, frame, settings, hLength, vLength, margin, enlargedDim,
                source_host, warper_host, result_host);
  /*- ポインタ入れ替え -*/
  source_host = result_host;

  int2 yohaku = {(enlargedDim.lx - tile.getRaster()->getSize().lx) / 2,
                 (enlargedDim.ly - tile.getRaster()->getSize().ly) / 2};
  /*- ラスタのクリア -*/
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(source_host, outRas32, enlargedDim,
                                          yohaku);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(source_host, outRas64, enlargedDim,
                                          yohaku);

  /*- ソース画像のメモリ解放 -*/
  source_host_ras->unlock();
  /*- 参照画像のメモリ解放 -*/
  warper_host_ras->unlock();
  result_host_ras->unlock();
}

/*------------------------------------
 Fx計算
------------------------------------*/

void Iwa_GradientWarpFx::doCompute_CPU(TTile &tile, const double frame,
                                       const TRenderSettings &settings,
                                       double hLength, double vLength,
                                       int margin, TDimensionI &enlargedDim,
                                       float4 *source_host, float *warper_host,
                                       float4 *result_host) {
  float4 *res_p = result_host + margin * enlargedDim.lx + margin;

  float *warp_up    = warper_host + (margin + 1) * enlargedDim.lx + margin;
  float *warp_down  = warper_host + (margin - 1) * enlargedDim.lx + margin;
  float *warp_right = warper_host + margin * enlargedDim.lx + 1 + margin;
  float *warp_left  = warper_host + margin * enlargedDim.lx - 1 + margin;

  for (int j = margin; j < enlargedDim.ly - margin; j++) {
    for (int i = margin; i < enlargedDim.lx - margin;
         i++, res_p++, warp_up++, warp_down++, warp_right++, warp_left++) {
      /*- 勾配を得る -*/
      float2 grad = {static_cast<float>((*warp_right) - (*warp_left)),
                     static_cast<float>((*warp_up) - (*warp_down))};
      /*- 参照点 -*/
      float2 samplePos = {static_cast<float>(i + grad.x * hLength),
                          static_cast<float>(j + grad.y * vLength)};

      int2 index = {
          (int)(samplePos.x + (float)enlargedDim.lx) - enlargedDim.lx,
          (int)(samplePos.y + (float)enlargedDim.ly) - enlargedDim.ly};
      float2 ratio = {samplePos.x - (float)index.x,
                      samplePos.y - (float)index.y};

      /*- サンプリング点回りのピクセルを線形補間 -*/
      float4 col1 = interp_CPU(
          getSourceVal_CPU(source_host, enlargedDim, index.x, index.y),
          getSourceVal_CPU(source_host, enlargedDim, index.x + 1, index.y),
          ratio.x);
      float4 col2 = interp_CPU(
          getSourceVal_CPU(source_host, enlargedDim, index.x, index.y + 1),
          getSourceVal_CPU(source_host, enlargedDim, index.x + 1, index.y + 1),
          ratio.x);
      *res_p = interp_CPU(col1, col2, ratio.y);
    }

    res_p += 2 * margin;
    warp_up += 2 * margin;
    warp_down += 2 * margin;
    warp_right += 2 * margin;
    warp_left += 2 * margin;
  }
}

float4 Iwa_GradientWarpFx::getSourceVal_CPU(float4 *source_host,
                                            TDimensionI &enlargedDim, int pos_x,
                                            int pos_y) {
  if (pos_x < 0 || pos_x >= enlargedDim.lx || pos_y < 0 ||
      pos_y >= enlargedDim.ly)
    return float4{0.0f, 0.0f, 0.0f, 0.0f};

  return source_host[pos_y * enlargedDim.lx + pos_x];
}

float4 Iwa_GradientWarpFx::interp_CPU(float4 val1, float4 val2, float ratio) {
  return float4{(1.0f - ratio) * val1.x + ratio * val2.x,
                (1.0f - ratio) * val1.y + ratio * val2.y,
                (1.0f - ratio) * val1.z + ratio * val2.z,
                (1.0f - ratio) * val1.w + ratio * val2.w};
}

//------------------------------------

bool Iwa_GradientWarpFx::doGetBBox(double frame, TRectD &bBox,
                                   const TRenderSettings &info) {
  if (!m_source.isConnected()) {
    bBox = TRectD();
    return false;
  }

  const bool ret = m_source->doGetBBox(frame, bBox, info);
  get_render_enlarge(frame, info.m_affine, bBox);
  return ret;
}

//------------------------------------

bool Iwa_GradientWarpFx::canHandle(const TRenderSettings &info, double frame) {
  return false;
}

//------------------------------------

void Iwa_GradientWarpFx::get_render_real_hv(const double frame,
                                            const TAffine affine,
                                            double &h_maxlen,
                                            double &v_maxlen) {
  /*--- ベクトルにする(プラス値) --- */
  TPointD rend_vect;
  rend_vect.x = std::abs(m_h_maxlen->getValue(frame));
  rend_vect.y = std::abs(m_v_maxlen->getValue(frame));
  /*--- 拡大縮小(移動回転しないで)のGeometryを反映させる ---*/
  rend_vect = rend_vect * sqrt(fabs(affine.det()));
  /*--- 方向は無視して長さを返す(プラス値) ---*/
  h_maxlen = rend_vect.x;
  v_maxlen = rend_vect.y;
}

//------------------------------------

void Iwa_GradientWarpFx::get_render_enlarge(const double frame,
                                            const TAffine affine,
                                            TRectD &bBox) {
  double h_maxlen = 0.0;
  double v_maxlen = 0.0;
  this->get_render_real_hv(frame, affine, h_maxlen, v_maxlen);
  const int margin =
      static_cast<int>(ceil((h_maxlen < v_maxlen) ? v_maxlen : h_maxlen));
  if (0 < margin) {
    bBox = bBox.enlarge(static_cast<double>(margin));
  }
}

FX_PLUGIN_IDENTIFIER(Iwa_GradientWarpFx, "iwa_GradientWarpFx")
