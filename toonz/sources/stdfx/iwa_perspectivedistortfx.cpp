/*------------------------------------
Iwa_PerspectiveDistortFx
奥行き方向に台形歪みを行うエフェクト
ディティールを保持するため、引き伸ばす量に応じて素材の解像度を上げる
------------------------------------*/

#include "iwa_perspectivedistortfx.h"

#include "tparamuiconcept.h"
//#include "ino_common.h"

namespace {

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
}

/*------------------------------------------------------------
 出力結果をChannel値に変換して格納
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_PerspectiveDistortFx::setOutputRaster(float4 *srcMem,
                                               const RASTER dstRas,
                                               TDimensionI dim, int drawLevel) {
  typename PIXEL::Channel halfChan =
      (typename PIXEL::Channel)(PIXEL::maxChannelValue / 2);

  dstRas->fill(PIXEL::Transparent);

  float4 *chan_p = srcMem;
  for (int j = 0; j < drawLevel; j++) {
    if (j >= dstRas->getLy()) break;

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

/*------------------------------------------------------------
 ソース画像を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_PerspectiveDistortFx::setSourceRaster(const RASTER srcRas,
                                               float4 *dstMem,
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

//------------------------------------------------------------

Iwa_PerspectiveDistortFx::Iwa_PerspectiveDistortFx()
    : m_vanishingPoint(TPointD(0, 0))
    , m_anchorPoint(TPointD(0, -100.0))
    , m_precision(300.0 / 162.5) {
  /*- 共通パラメータのバインド -*/
  addInputPort("Source", m_source);

  bindParam(this, "vanishingPoint", m_vanishingPoint);
  bindParam(this, "anchorPoint", m_anchorPoint);
  bindParam(this, "precision", m_precision);

  m_vanishingPoint->getX()->setMeasureName("fxLength");
  m_vanishingPoint->getY()->setMeasureName("fxLength");
  m_anchorPoint->getX()->setMeasureName("fxLength");
  m_anchorPoint->getY()->setMeasureName("fxLength");

  m_precision->setValueRange(1.0, 2.0);
}

//------------------------------------

bool Iwa_PerspectiveDistortFx::doGetBBox(double frame, TRectD &bBox,
                                         const TRenderSettings &info) {
  if (m_source.isConnected()) {
    bool ret      = m_source->doGetBBox(frame, bBox, info);
    if (ret) bBox = TConsts::infiniteRectD;
    return ret;
  }
  return false;
}

//------------------------------------

bool Iwa_PerspectiveDistortFx::canHandle(const TRenderSettings &info,
                                         double frame) {
  return false;
}

//------------------------------------

void Iwa_PerspectiveDistortFx::doCompute(TTile &tile, double frame,
                                         const TRenderSettings &rend_sets) {
  /*- ソース画像が刺さっていなければreturn -*/
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  /*- パラメータの取得 -*/
  TPointD vp       = m_vanishingPoint->getValue(frame);
  TPointD ap       = m_anchorPoint->getValue(frame);
  double precision = m_precision->getValue(frame);

  /*- 基準点が消失点よりも上にあったらreturn -*/
  if (vp.y <= ap.y) {
    std::cout << "the anchor must be lower than the vanishing point"
              << std::endl;
    tile.getRaster()->clear();
    return;
  }

  TAffine aff = rend_sets.m_affine;
  /*- カメラ左下を中心としたピクセル座標に変換する -*/
  TPointD vanishingPoint = aff * vp -
                           (tile.m_pos + tile.getRaster()->getCenterD()) +
                           TPointD(rend_sets.m_cameraBox.getLx() / 2.0,
                                   rend_sets.m_cameraBox.getLy() / 2.0);
  TPointD anchorPoint = aff * ap -
                        (tile.m_pos + tile.getRaster()->getCenterD()) +
                        TPointD(rend_sets.m_cameraBox.getLx() / 2.0,
                                rend_sets.m_cameraBox.getLy() / 2.0);

  double offs = vanishingPoint.x - rend_sets.m_cameraBox.getLx() / 2.0;

  /*- 取り込む解像度の倍率 -*/

  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));
  TDimensionI outDim(rectOut.getLx(), rectOut.getLy());
  /*- ソース画像を正規化して格納 -*/
  /*- 素材を拡大させて持ち込む -*/
  TDimensionI sourceDim(rectOut.getLx() * (int)tceil(precision), anchorPoint.y);
  float4 *source_host;
  TRasterGR8P source_host_ras(sourceDim.lx * sizeof(float4), sourceDim.ly);
  source_host_ras->lock();
  source_host = (float4 *)source_host_ras->getRawData();
  {
    TRenderSettings new_sets(rend_sets);

    new_sets.m_affine *= TTranslation(vp.x, vp.y);
    new_sets.m_affine *= TScale(precision, 1.0);
    new_sets.m_affine *= TTranslation(-vp.x, -vp.y);

    double sourcePosX = offs + precision * (tile.m_pos.x - offs);

    TTile sourceTile;
    m_source->allocateAndCompute(sourceTile, TPointD(sourcePosX, tile.m_pos.y),
                                 sourceDim, tile.getRaster(), frame, new_sets);

    /*- タイルの画像を０〜１に正規化してホストメモリに読み込む -*/
    TRaster32P ras32 = (TRaster32P)sourceTile.getRaster();
    TRaster64P ras64 = (TRaster64P)sourceTile.getRaster();
    if (ras32)
      setSourceRaster<TRaster32P, TPixel32>(ras32, source_host, sourceDim);
    else if (ras64)
      setSourceRaster<TRaster64P, TPixel64>(ras64, source_host, sourceDim);
  }

  TDimensionI resultDim(rectOut.getLx(), anchorPoint.y);
  TRasterGR8P result_host_ras(resultDim.lx * sizeof(float4), resultDim.ly);
  /*- 結果を収めるメモリ -*/
  float4 *result_host;
  result_host_ras->lock();
  result_host = (float4 *)result_host_ras->getRawData();

  doCompute_CPU(tile, frame, rend_sets, vanishingPoint, anchorPoint,
                source_host, result_host, sourceDim, resultDim, precision,
                offs);

  source_host_ras->unlock();

  /*- 出力結果をChannel値に変換して格納 -*/
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(result_host, outRas32, outDim,
                                          resultDim.ly);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(result_host, outRas64, outDim,
                                          resultDim.ly);

  result_host_ras->unlock();
}

//------------------------------------

void Iwa_PerspectiveDistortFx::doCompute_CPU(
    TTile &tile, const double frame, const TRenderSettings &settings,
    TPointD &vp, TPointD &ap, float4 *source_host, float4 *result_host,
    TDimensionI &sourceDim, TDimensionI &resultDim, const double precision,
    const double offs) {
  /*- 結果ポインタ -*/
  float4 *result_p = result_host;
  /*- 結果画像内でループ -*/
  for (int j = 0; j < resultDim.ly; j++) {
    /*- サンプルスタート地点 -*/
    double sampleX =
        precision * (vp.x * (ap.y - (double)j) / (vp.y - (double)j));
    /*- 1ピクセル横に移動した場合のサンプリング位置の移動量 -*/
    double dx = precision * (vp.y - ap.y) / (vp.y - (double)j);
    for (int i = 0; i < resultDim.lx; i++, result_p++, sampleX += dx) {
      int index    = (int)tfloor(sampleX);
      double ratio = sampleX - (double)index;
      (*result_p)  = interp_CPU(
          getSource_CPU(source_host, sourceDim, index, j),
          getSource_CPU(source_host, sourceDim, index + 1, j), ratio);
    }
  }
}

//------------------------------------

void Iwa_PerspectiveDistortFx::getParamUIs(TParamUIConcept *&concepts,
                                           int &length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Vanishing Point";
  concepts[0].m_params.push_back(m_vanishingPoint);

  concepts[1].m_type  = TParamUIConcept::POINT;
  concepts[1].m_label = "Anchor Point";
  concepts[1].m_params.push_back(m_anchorPoint);
}

//------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_PerspectiveDistortFx, "iwa_PerspectiveDistortFx")
