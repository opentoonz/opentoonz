#pragma once

//******************************************************************
// Iwa FractalNoise Fx
// An Fx emulating Fractal Noise effect in Adobe AfterEffect
//******************************************************************

#ifndef IWA_FRACTALNOISEFX_H
#define IWA_FRACTALNOISEFX_H

#include "tfxparam.h"
#include "tparamset.h"
#include "stdfx.h"

class Iwa_FractalNoiseFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_FractalNoiseFx)

  enum FractalType {
    Basic = 0,
    TurbulentSmooth,
    TurbulentBasic,
    TurbulentSharp,
    Dynamic,
    DynamicTwist,
    Max,
    Rocky,
    FractalTypeCount
  };

  enum NoiseType { Block = 0, Smooth, NoiseTypeCount };

  struct FNParam {
    FractalType fractalType;
    NoiseType noiseType;
    bool invert;
    double rotation;
    TDimensionD scale;
    TPointD offsetTurbulence;
    bool perspectiveOffset;
    double complexity;
    double subInfluence;
    double subScaling;
    double subRotation;
    TPointD subOffset;
    double evolution;
    bool cycleEvolution;
    double cycleEvolutionRange;
    double dynamicIntensity;
    bool alphaRendering;
  };

protected:
  // Fractal Type フラクタルの種類
  TIntEnumParamP m_fractalType;
  // Noise Type ノイズの種類
  TIntEnumParamP m_noiseType;
  // Invert 反転
  TBoolParamP m_invert;
  /// Contrast コントラスト
  /// Brightness 明るさ
  /// Overflow オーバーフロー

  //- - - Transform トランスフォーム - - -
  // Rotation 回転
  TDoubleParamP m_rotation;
  // Uniform Scaling　縦横比を固定
  TBoolParamP m_uniformScaling;
  // Scale スケール
  TDoubleParamP m_scale;
  // Scale Width スケールの幅
  TDoubleParamP m_scaleW;
  // Scale Height スケールの高さ
  TDoubleParamP m_scaleH;
  // Offset Turbulence 乱気流のオフセット
  TPointParamP m_offsetTurbulence;
  // Perspective Offset 遠近オフセット
  TBoolParamP m_perspectiveOffset;

  // Complexity 複雑度
  TDoubleParamP m_complexity;

  //- - - Sub Settings サブ設定 - - -
  // Sub Influence サブ影響（％）
  TDoubleParamP m_subInfluence;
  // Sub Scaling　サブスケール
  TDoubleParamP m_subScaling;
  // Sub Rotation サブ回転
  TDoubleParamP m_subRotation;
  // Sub Offset サブのオフセット
  TPointParamP m_subOffset;
  // Center Subscale サブスケールを中心
  /// TBoolParamP m_centerSubscale;

  // Evolution 展開
  TDoubleParamP m_evolution;

  //- - - Evolution Options 展開のオプション - - -
  // Cycle Evolution サイクル展開
  TBoolParamP m_cycleEvolution;
  // Cycle (in Evolution) サイクル（周期）
  TDoubleParamP m_cycleEvolutionRange;
  /// Random Seed ランダムシード
  /// Opacity  不透明度
  /// Blending Mode 描画モード

  // ダイナミックの度合い
  TDoubleParamP m_dynamicIntensity;

  // - - - additional parameters - - -
  TBoolParamP m_alphaRendering;

public:
  Iwa_FractalNoiseFx();
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  void obtainParams(FNParam &param, const double frame, const TAffine &aff);

  template <typename RASTER, typename PIXEL>
  void outputRaster(const RASTER outRas, double *out_buf, const FNParam &param);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  // For Dynamic and Dynamic Twist patterns, the position offsets using gradient
  // / rotation of the parent pattern
  TPointD getSamplePos(int x, int y, const TDimension outDim,
                       const double *out_buf, const int gen, const double scale,
                       const FNParam &param);
  // convert the noise
  void convert(double *buf, const FNParam &param);
  // composite the base noise pattern
  void composite(double *out, double *buf, const double influence,
                 const FNParam &param);
  // finalize pattern (coverting the color space)
  void finalize(double *out, const FNParam &param);
};

#endif