/*------------------------------------------------------------
Iwa_PNPerspectiveFx
PerlinNoise/SimplexNoiseパターンを生成、透視投影する
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

/* 内積を返す */
inline float dot(float3 a, float3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* 外積を返す */
inline float3 cross(float3 a, float3 b) {
  float3 ret = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
  return ret;
}

/* 正規化する */
inline float3 normalize(float3 v) {
  float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  float3 ret   = {v.x / length, v.y / length, v.z / length};
  return ret;
}
}

/*------------------------------------------------------------
 出力結果をChannel値に変換して格納
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_PNPerspectiveFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                          TDimensionI dim, int drawLevel,
                                          const bool alp_rend_sw) {
  typename PIXEL::Channel halfChan =
      (typename PIXEL::Channel)(PIXEL::maxChannelValue / 2);

  if (alp_rend_sw)
    dstRas->fill(PIXEL(halfChan, halfChan, halfChan, halfChan));
  else
    dstRas->fill(PIXEL(halfChan, halfChan, halfChan));
  float4 *chan_p = srcMem;
  for (int j = 0; j < drawLevel; j++) {
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
 PerlinNoiseのパラメータを取得
------------------------------------------------------------*/
void Iwa_PNPerspectiveFx::getPNParameters(TTile &tile, double frame,
                                          const TRenderSettings &settings,
                                          PN_Params &params,
                                          TDimensionI &dimOut) {
  /*  動作パラメータを得る */
  params.renderMode = m_renderMode->getValue();
  params.noiseType  = m_noiseType->getValue();
  params.size       = (float)m_size->getValue(frame);
  /* SimplexNoiseの密度感をそろえるための係数をかける */
  if (params.noiseType == 1) params.size *= 1.41421356f;
  params.octaves = m_octaves->getValue() + 1;
  params.offset  = float2{(float)m_offset->getValue(frame).x,
                         (float)m_offset->getValue(frame).y};
  params.p_intensity = (float)m_persistance_intensity->getValue(frame);
  params.p_size      = (float)m_persistance_size->getValue(frame);
  params.p_offset    = (float)m_persistance_offset->getValue(frame);
  TPointD _eyeLevel  = m_eyeLevel->getValue(frame);
  params.eyeLevel    = float2{(float)_eyeLevel.x, (float)_eyeLevel.y};
  params.alp_rend_sw = m_alpha_rendering->getValue();
  params.waveHeight  = (float)m_waveHeight->getValue(frame);

  const float fov = (float)m_fov->getValue(frame);

  TAffine aff        = settings.m_affine;
  const double scale = 1.0 / sqrt(fabs(aff.det()));
  TAffine aff_pn     = TScale(scale) * TTranslation(tile.m_pos);

  params.a11 = aff_pn.a11;
  params.a12 = aff_pn.a12;
  params.a13 = aff_pn.a13;
  params.a21 = aff_pn.a21;
  params.a22 = aff_pn.a22;
  params.a23 = aff_pn.a23;

  params.time        = (float)m_evolution->getValue(frame) * 0.05;
  params.p_evolution = (float)m_persistance_evolution->getValue(frame);

  TPointD eyePoint =
      aff * _eyeLevel - (tile.m_pos + tile.getRaster()->getCenterD());
  const float eyeHeight = (float)eyePoint.y;
  /* 描画範囲の下からの距離 */
  params.drawLevel = (int)((float)dimOut.ly / 2.0f + eyeHeight);
  if (params.drawLevel > dimOut.ly) params.drawLevel = dimOut.ly;

  //------------------------------------------------------------

  /* カメラたて方向のmmサイズの半分の寸法 */
  int camHeight = settings.m_cameraBox.getLy();
  TPointD vec_p0p1((double)camHeight * aff_pn.a12,
                   (double)camHeight * aff_pn.a22);
  params.fy_2 = sqrtf(vec_p0p1.x * vec_p0p1.x + vec_p0p1.y * vec_p0p1.y) / 2.0f;

  float fov_radian_2 = (fov / 2.0f) * float(M_PI_180);

  /* カメラから投影面への距離 */
  float D = params.fy_2 / tanf(fov_radian_2);
  /* カメラから、投影面上の水平線への距離 */
  params.A = sqrtf(params.eyeLevel.y * params.eyeLevel.y + D * D);

  /* カメラ位置から下枠へのベクトルと、水平線のなす角度 */
  float theta = fov_radian_2 + asinf(params.eyeLevel.y / params.A);

  float M = params.fy_2 / sinf(fov_radian_2);

  params.cam_pos = float3{0.0f, -M * cosf(theta), M * sinf(theta)};

  /*ベースとなるフレネル反射率を求める*/
  params.base_fresnel_ref = 0.0f;
  float phi               = 90.0f - theta * 180.0f / M_PI;
  if (phi >= 0.0f && phi < 90.0f) {
    int index   = (int)phi;
    float ratio = phi - (float)index;
    params.base_fresnel_ref =
        fresnel[index] * (1.0f - ratio) + fresnel[index + 1] * ratio;
  }

  /*強度の正規化のため、合計値を算出*/
  float intensity = 2.0f; /* -1 ～ 1 */
  params.int_sum  = 0.0f;
  for (int o = 0; o < params.octaves; o++) {
    params.int_sum += intensity;
    intensity *= params.p_intensity;
  }
}

//------------------------------------------------------------

Iwa_PNPerspectiveFx::Iwa_PNPerspectiveFx()
    : m_renderMode(new TIntEnumParam(0, "Noise"))
    , m_noiseType(new TIntEnumParam(0, "Perlin Noise"))
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
    , m_waveHeight(10.0) {
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

  m_noiseType->addItem(1, "Simplex Noise");

  m_renderMode->addItem(1, "Noise (no resampled)");
  m_renderMode->addItem(2, "Warp HV offset");
  m_renderMode->addItem(4, "Warp HV offset 2");
  m_renderMode->addItem(3, "Fresnel reflectivity");

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
  /* サポートしていないPixelタイプはエラーを投げる */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }

  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());

  /* PerinNoiseのパラメータ */
  PN_Params pnParams;
  getPNParameters(tile, frame, settings, pnParams, dimOut);

  /* 水平線が画面より下のときreturn */
  if (pnParams.drawLevel < 0) {
    tile.getRaster()->clear();
    return;
  }

  const float evolution   = (float)m_evolution->getValue(frame);
  const float p_evolution = (float)m_persistance_evolution->getValue(frame);

  float4 *out_host;
  /* ホストのメモリ確保 */
  TRasterGR8P out_host_ras(sizeof(float4) * dimOut.lx, pnParams.drawLevel);
  out_host_ras->lock();
  out_host = (float4 *)out_host_ras->getRawData();

  doCompute_CPU(tile, frame, settings, out_host, dimOut, pnParams);

  /* 出力結果をChannel値に変換して格納 */
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(
        out_host, outRas32, dimOut, pnParams.drawLevel, pnParams.alp_rend_sw);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(
        out_host, outRas64, dimOut, pnParams.drawLevel, pnParams.alp_rend_sw);

  out_host_ras->unlock();
}

//------------------------------------------------------------
void Iwa_PNPerspectiveFx::doCompute_CPU(TTile &tile, double frame,
                                        const TRenderSettings &settings,
                                        float4 *out_host, TDimensionI &dimOut,
                                        PN_Params &pnParams) {
  /* モードで分ける */
  if (pnParams.renderMode == 0 || pnParams.renderMode == 1) {
    calcPerinNoise_CPU(out_host, dimOut, pnParams,
                       (bool)(pnParams.renderMode == 0));
  } else if (pnParams.renderMode == 2 || pnParams.renderMode == 3 ||
             pnParams.renderMode == 4) {
    calcPNNormal_CPU(out_host, dimOut, pnParams);
    if (pnParams.renderMode == 4) {
      calcPNNormal_CPU(out_host, dimOut, pnParams, true);
    }
  }
}

/*------------------------------------------------------------
 通常のノイズのCPU計算
------------------------------------------------------------*/
void Iwa_PNPerspectiveFx::calcPerinNoise_CPU(float4 *out_host,
                                             TDimensionI &dimOut, PN_Params &p,
                                             bool doResample) {
  int reso = (doResample) ? 10 : 1;
  /* 結果を収めるイテレータ */
  float4 *out_p = out_host;
  /* 各ピクセルについて */
  for (int yy = 0; yy < p.drawLevel; yy++) {
    for (int xx = 0; xx < dimOut.lx; xx++, out_p++) {
      float val_sum = 0.0f;
      int count     = 0;
      /* 各リサンプル点について */
      for (int tt = 0; tt < reso; tt++) {
        for (int ss = 0; ss < reso; ss++) {
          float2 tmpPixPos = {
              (float)xx - 0.5f + ((float)ss + 0.5f) / (float)reso,
              (float)yy - 0.5f + ((float)tt + 0.5f) / (float)reso};
          float2 screenPos = {
              tmpPixPos.x * p.a11 + tmpPixPos.y * p.a12 + p.a13,
              tmpPixPos.x * p.a21 + tmpPixPos.y * p.a22 + p.a23};
          /* ② Perlin Noise 平面上の座標を計算する */
          float2 noisePos;
          noisePos.x = -(p.eyeLevel.y + p.fy_2) * (screenPos.x - p.eyeLevel.x) /
                           (screenPos.y - p.eyeLevel.y) +
                       p.eyeLevel.x;
          noisePos.y =
              (p.fy_2 + screenPos.y) * p.A / (p.eyeLevel.y - screenPos.y);
          float tmpVal           = 0.5f;
          float currentSize      = p.size;
          float2 currentOffset   = p.offset;
          float currentIntensity = 1.0f;
          // float2* basis_p = basis;

          float currentEvolution = p.time;

          /* ノイズを各世代足しこむ */
          for (int o = 0; o < p.octaves; o++) {
            float2 currentNoisePos = {
                (noisePos.x - currentOffset.x) / currentSize,
                (noisePos.y - currentOffset.y) / currentSize};

            if (p.noiseType == 0) {
              tmpVal += currentIntensity *
                        Noise1234::noise(currentNoisePos.x, currentNoisePos.y,
                                         currentEvolution) /
                        p.int_sum;
            } else {
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

      float val = val_sum / (float)count;

      /* クランプ */
      val = (val < 0.0f) ? 0.0f : ((val > 1.0f) ? 1.0f : val);

      (*out_p).x = val;
      (*out_p).y = val;
      (*out_p).z = val;
      (*out_p).w = (p.alp_rend_sw) ? val : 1.0f;
    }
  }
}

/*------------------------------------------------------------
 WarpHVモード、Fresnel反射モード
------------------------------------------------------------*/
void Iwa_PNPerspectiveFx::calcPNNormal_CPU(float4 *out_host,
                                           TDimensionI &dimOut, PN_Params &p,
                                           bool isSubWave) {
  /* 結果を収めるイテレータ */
  float4 *out_p = out_host;
  /* 各ピクセルについて */
  for (int yy = 0; yy < p.drawLevel; yy++) {
    for (int xx = 0; xx < dimOut.lx; xx++, out_p++) {
      float2 screenPos = {(float)xx * p.a11 + (float)yy * p.a12 + p.a13,
                          (float)xx * p.a21 + (float)yy * p.a22 + p.a23};

      /*  ② Perlin Noise 平面上の座標を計算する */
      float2 noisePos;

      noisePos.x = -(p.eyeLevel.y + p.fy_2) * (screenPos.x - p.eyeLevel.x) /
                       (screenPos.y - p.eyeLevel.y) +
                   p.eyeLevel.x;

      noisePos.y = (p.fy_2 + screenPos.y) * p.A / (p.eyeLevel.y - screenPos.y);

      float gradient[2]; /* 0 : よこ差分、1 : たて差分 */

      float delta = 0.001f;

      /* 横、縦差分それぞれについて */
      for (int yokoTate = 0; yokoTate < 2; yokoTate++) {
        /* 勾配の初期化 */
        gradient[yokoTate] = 0.0f;

        /* サンプリング位置のオフセットを求める */
        float2 kinbouNoisePos[2] = {
            float2{noisePos.x - ((yokoTate == 0) ? delta : 0.0f),
                   noisePos.y - ((yokoTate == 0) ? 0.0f : delta)},
            float2{noisePos.x + ((yokoTate == 0) ? delta : 0.0f),
                   noisePos.y + ((yokoTate == 0) ? 0.0f : delta)}};
        float currentSize      = p.size;
        float2 currentOffset   = p.offset;
        float currentIntensity = 1.0f;
        // float2* basis_p = basis;
        float currentEvolution = (isSubWave) ? p.time + 100.0f : p.time;
        /* 各世代について */
        for (int o = 0; o < p.octaves; o++, currentSize *= p.p_size,
                 currentOffset.x *= p.p_offset, currentOffset.y *= p.p_offset,
                 currentIntensity *= p.p_intensity) {
          /* プラス方向、マイナス方向それぞれオフセットしたノイズ座標を求める */
          float2 currentOffsetNoisePos[2];
          for (int mp = 0; mp < 2; mp++)
            currentOffsetNoisePos[mp] =
                float2{(kinbouNoisePos[mp].x - currentOffset.x) / currentSize,
                       (kinbouNoisePos[mp].y - currentOffset.y) / currentSize};

          /* ノイズの差分を積算していく */
          float noiseDiff;
          // Perlin Noise
          if (p.noiseType == 0) {
            noiseDiff =
                Noise1234::noise(currentOffsetNoisePos[1].x,
                                 currentOffsetNoisePos[1].y, currentEvolution) -
                Noise1234::noise(currentOffsetNoisePos[0].x,
                                 currentOffsetNoisePos[0].y, currentEvolution);
          } else {
            /* インデックスをチェック */
            /* まず、前後 */
            CellIds kinbouIds[2] = {
                SimplexNoise::getCellIds(currentOffsetNoisePos[0].x,
                                         currentOffsetNoisePos[0].y,
                                         currentEvolution),
                SimplexNoise::getCellIds(currentOffsetNoisePos[1].x,
                                         currentOffsetNoisePos[1].y,
                                         currentEvolution)};
            /* 同じセルに入っていたら、普通に差分を計算 */
            if (kinbouIds[0] == kinbouIds[1]) {
              noiseDiff = SimplexNoise::noise(currentOffsetNoisePos[1].x,
                                              currentOffsetNoisePos[1].y,
                                              currentEvolution) -
                          SimplexNoise::noise(currentOffsetNoisePos[0].x,
                                              currentOffsetNoisePos[0].y,
                                              currentEvolution);
            }
            /* 違うセルの場合、中心位置を用いる */
            else {
              float2 currentCenterNoisePos = {
                  (noisePos.x - currentOffset.x) / currentSize,
                  (noisePos.y - currentOffset.y) / currentSize};
              CellIds centerIds = SimplexNoise::getCellIds(
                  currentCenterNoisePos.x, currentCenterNoisePos.y,
                  currentEvolution);
              if (kinbouIds[0] == centerIds) {
                noiseDiff = SimplexNoise::noise(currentCenterNoisePos.x,
                                                currentCenterNoisePos.y,
                                                currentEvolution) -
                            SimplexNoise::noise(currentOffsetNoisePos[0].x,
                                                currentOffsetNoisePos[0].y,
                                                currentEvolution);
              } else  // if(kinbouIds[1] == centerIds)
              {
                noiseDiff = SimplexNoise::noise(currentOffsetNoisePos[1].x,
                                                currentOffsetNoisePos[1].y,
                                                currentEvolution) -
                            SimplexNoise::noise(currentCenterNoisePos.x,
                                                currentCenterNoisePos.y,
                                                currentEvolution);
              }
              /* 片端→中心の変位を使っているので、片端→片端に合わせて変位を2倍する
               */
              noiseDiff *= 2.0f;
            }
          }
          /* 差分に強度を乗算して足しこむ */
          gradient[yokoTate] += currentIntensity * noiseDiff / p.int_sum;

          currentEvolution *= p.p_evolution;
        }
      }

      /* X方向、Y方向の近傍ベクトルを計算する */
      float3 vec_x  = {delta * 2, 0.0f, gradient[0] * p.waveHeight};
      float3 vec_y  = {0.0f, delta * 2, gradient[1] * p.waveHeight};
      float3 normal = normalize(cross(vec_x, vec_y));

      /* カメラから平面へのベクトル */
      float3 cam_vec = {noisePos.x - p.cam_pos.x, noisePos.y - p.cam_pos.y,
                        -p.cam_pos.z};
      cam_vec = normalize(cam_vec);
      /* WarpHVの参照画像モード */
      if (p.renderMode == 2 || p.renderMode == 4) {
        /* 平面からの反射ベクトル */
        float alpha        = dot(normal, cam_vec);
        float3 reflect_cam = {
            2.0f * alpha * normal.x - cam_vec.x,
            2.0f * alpha * normal.y - cam_vec.y,
            2.0f * alpha * normal.z - cam_vec.z}; /* これの長さは１ */
        /* 完全に水平な面で反射した場合の反射ベクトル */
        float3 reflect_cam_mirror = {cam_vec.x, cam_vec.y, -cam_vec.z};
        /* 角度のずれを格納する */
        /*  -PI/2 ～ PI/2 */
        float angle_h = atanf(reflect_cam.x / reflect_cam.y) -
                        atanf(reflect_cam_mirror.x / reflect_cam_mirror.y);
        float angle_v = atanf(reflect_cam.z / reflect_cam.y) -
                        atanf(reflect_cam_mirror.z / reflect_cam_mirror.y);

        /* 30°を最大とする */
        angle_h = 0.5f + angle_h / 0.5236f;
        angle_v = 0.5f - angle_v / 0.5236f;

        /* クランプ */
        angle_h = (angle_h < 0.0f) ? 0.0f : ((angle_h > 1.0f) ? 1.0f : angle_h);
        angle_v = (angle_v < 0.0f) ? 0.0f : ((angle_v > 1.0f) ? 1.0f : angle_v);

        if (p.renderMode == 2) {
          (*out_p).x = angle_h;
          (*out_p).y = angle_v;
          (*out_p).z = 0.0f;
          (*out_p).w = 1.0f;
        } else  //  p.renderMode == 4
        {
          if (!isSubWave) {
            (*out_p).y = angle_v;
            (*out_p).z = 0.0f;
            (*out_p).w = 1.0f;
          } else
            (*out_p).x = angle_v;
        }
      }
      /* フレネル反射モード */
      else if (p.renderMode == 3) {
        cam_vec.x *= -1;
        cam_vec.y *= -1;
        cam_vec.z *= -1;
        float diffuse_angle = acosf(dot(normal, cam_vec)) * 180.0f / 3.14159f;
        float ref           = 0.0f;
        if (diffuse_angle >= 0.0f && diffuse_angle < 90.0f) {
          int index   = (int)diffuse_angle;
          float ratio = diffuse_angle - (float)index;
          float fresnel_ref =
              fresnel[index] * (1.0f - ratio) + fresnel[index + 1] * ratio;
          ref =
              (fresnel_ref - p.base_fresnel_ref) / (1.0f - p.base_fresnel_ref);
        } else if (diffuse_angle >= 90.0f)
          ref = 1.0f;

        /* クランプ */
        ref        = (ref < 0.0f) ? 0.0f : ((ref > 1.0f) ? 1.0f : ref);
        (*out_p).x = ref;
        (*out_p).y = ref;
        (*out_p).z = ref;
        (*out_p).w = (p.alp_rend_sw) ? ref : 1.0f;
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
