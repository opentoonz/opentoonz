#pragma once

/*------------------------------------------------------------
 Iwa_PNPerspectiveFx
 PerlinNoise/SimplexNoiseパターンを生成、透視投影する
------------------------------------------------------------*/
#ifndef IWA_PN_PERSPECTIVE_H
#define IWA_PN_PERSPECTIVE_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

struct float2 {
  float x, y;
};
struct float3 {
  float x, y, z;
};
struct float4 {
  float x, y, z, w;
};

/*計算に用いるパラメータ群*/
struct PN_Params {
  int renderMode;
  int noiseType;     /* 0:Perlin, 1: Simplex */
  float size;        /* 第１世代のノイズサイズ */
  int octaves;       /* 世代数 */
  float2 offset;     /* 第１世代のオフセット */
  float p_intensity; /* 世代間の強度比 */
  float p_size;      /* 世代間のサイズ比 */
  float p_offset;    /* 世代間のオフセット比 */
  float2 eyeLevel;   /* 視点位置 */
  int drawLevel;     /* 描画範囲の下からの距離 描画を開始するスキャンライン位置 */
  bool alp_rend_sw;
  float waveHeight; /* warpHV / フレネル反射のときのみ使う */
  float fy_2;
  float A;
  float3 cam_pos;
  float base_fresnel_ref; /* フレネル反射のときのみ使う */
  float int_sum;
  float a11, a12, a13;
  float a21, a22, a23;
  float time;
  float p_evolution;
};

class Iwa_PNPerspectiveFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_PNPerspectiveFx)

  TIntEnumParamP
      m_renderMode; /* 描画モード
                                                                           -
                       強度
                                                                           -
                       強度（リサンプルなし）
                                                                           -
                       WarpHV用オフセット
                                                                           -
                       フレネル反射 */

  TIntEnumParamP
      m_noiseType; /* ノイズのタイプ
                                                                           -
                      Perlin Noise
                                                                           -
                      Simplex Noise */

  TDoubleParamP m_size;      /* ベースとなる大きさ */
  TDoubleParamP m_evolution; /* 展開 */
  TIntEnumParamP m_octaves;  /* 世代数 */
  TPointParamP m_offset;     /* ノイズのオフセット */

  TDoubleParamP m_persistance_intensity; /* 次の世代での振幅の倍率 */
  TDoubleParamP m_persistance_size;      /* 次の世代での波長の倍率 */
  TDoubleParamP m_persistance_evolution; /* 次の世代での展開周期の倍率 */
  TDoubleParamP m_persistance_offset; /* 次の世代でのオフセット距離の倍率 */

  TDoubleParamP m_fov;     /* カメラ画角 */
  TPointParamP m_eyeLevel; /* 消失点の位置 */

  TBoolParamP
      m_alpha_rendering; /* アルファチャンネルもノイズを与えるかどうか */

  TDoubleParamP m_waveHeight; /* 波の高さ */

  /* 出力結果をChannel値に変換して格納 */
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int drawLevel, const bool alp_rend_sw);

  /* PerlinNoiseのパラメータを取得 */
  void getPNParameters(TTile &tile, double frame,
                       const TRenderSettings &settings, PN_Params &params,
                       TDimensionI &dimOut);

  /* 通常のノイズのCPU計算 */
  void calcPerinNoise_CPU(float4 *out_host, TDimensionI &dimOut, PN_Params &p,
                          bool doResample);

  /* WarpHVモード、Fresnel反射モード */
  void calcPNNormal_CPU(float4 *out_host, TDimensionI &dimOut, PN_Params &p,
                        bool isSubWave = false);

public:
  Iwa_PNPerspectiveFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;

  void doCompute_CPU(TTile &tile, double frame, const TRenderSettings &settings,
                     float4 *out_host, TDimensionI &dimOut,
                     PN_Params &pnParams);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif