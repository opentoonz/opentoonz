#pragma once

#ifndef IWA_PAINTBRUSHFX_H
#define IWA_PAINTBRUSHFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"

#include <QVector>
#include <QMap>

struct double2 {
  double x, y;
};

struct colorRGBA {
  double r, g, b, a;
};

struct BrushStroke {
  QVector<TPointD> centerPos;
  TPointD originPos;
  colorRGBA color;
  double length;
  double widthHalf;
  double angle;
  int textureId;
  bool invert;
  double randomVal;
  double stack;  // 整列用の値
};

struct BrushVertex {
  double pos[2];
  double texCoord[2];
  BrushVertex(const TPointD _pos, double u, double v) {
    pos[0]      = _pos.x;
    pos[1]      = _pos.y;
    texCoord[0] = u;
    texCoord[1] = v;
  }
  BrushVertex() : BrushVertex(TPointD(), 0, 0) {}
};

struct FlowPaintBrushFxParam {
  TDimensionI dim;
  TPointD origin_pos;
  TPointD horiz_pos;
  TPointD vert_pos;
  TRectD bbox;
  int fill_gap_size;

  double h_density;
  double v_density;
  double pos_randomness;
  double pos_wobble;

  int random_seed;
  DoublePair tipLength;
  DoublePair tipWidth;
  DoublePair tipAlpha;
  double width_random;
  double length_random;
  double angle_random;

  int reso;
  bool anti_jaggy;

  TPointD hVec;
  TPointD vVec;
  double2 vVec_unit;

  TAffine brushAff;

  int lastFrame;
};

class Iwa_FlowPaintBrushFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_FlowPaintBrushFx)
public:
  enum StackMode {
    NoSort = 0,
    Smaller,
    Larger,
    Darker,
    Brighter,
    Random
    //,TestModeArea
  };

private:
  TRasterFxPort m_brush;
  TRasterFxPort m_flow;
  TRasterFxPort m_area;
  TRasterFxPort m_color;

  // 密度
  TDoubleParamP m_h_density;
  TDoubleParamP m_v_density;
  // 位置のランダムさ（0：格子状 1：一様にランダム <1: ばらつき増加）
  TDoubleParamP m_pos_randomness;
  TDoubleParamP m_pos_wobble;
  // タッチのサイズ（Areaの値によって変化）
  TRangeParamP m_tip_width;
  TRangeParamP m_tip_length;
  // タッチの不透明度
  TRangeParamP m_tip_alpha;
  TIntParamP m_tip_joints;
  TBoolParamP m_bidirectional;

  // ばらつき
  TDoubleParamP m_width_randomness;
  TDoubleParamP m_length_randomness;
  TDoubleParamP m_angle_randomness;  // degreeで

  TDoubleParamP m_sustain_width_to_skew;
  TBoolParamP m_anti_jaggy;

  // 生成範囲
  TPointParamP m_origin_pos;
  TPointParamP m_horizontal_pos;
  TPointParamP m_vertical_pos;
  TPointParamP m_curve_point;
  TDoubleParamP m_fill_gap_size;

  // 基準フレーム
  TDoubleParamP m_reference_frame;
  // 基準フレームを使って生成するタッチの割合
  TDoubleParamP m_reference_prevalence;

  // ランダムシード
  TIntParamP m_random_seed;
  // 並べ替え
  TIntEnumParamP m_sortBy;

  // ブラシタッチのラスターデータを取得
  void getBrushRasters(std::vector<TRasterP> &brushRasters, TDimension &b_size,
                       int &lastFrame, TTile &tile, const TRenderSettings &ri);

  template <typename RASTER, typename PIXEL>
  void setFlowTileToBuffer(const RASTER flowRas, double2 *buf);
  template <typename RASTER, typename PIXEL>
  void setAreaTileToBuffer(const RASTER areaRas, double *buf);
  template <typename RASTER, typename PIXEL>
  void setColorTileToBuffer(const RASTER colorRas, colorRGBA *buf);

  // ためしに
  template <typename RASTER, typename PIXEL>
  void setOutRaster(const RASTER outRas, double *buf);

  void fillGapByDilateAndErode(double *buf, const TDimension &dim,
                               const int fill_gap_size);

  void computeBrushVertices(QVector<BrushVertex> &brushVertices,
                            QList<BrushStroke> &brushStrokes,
                            FlowPaintBrushFxParam &p, TTile &tile, double frame,
                            const TRenderSettings &ri);

  double getSizePixelAmount(const double val, const TAffine affine);
  FlowPaintBrushFxParam getParam(TTile &tile, double frame,
                                 const TRenderSettings &ri);

public:
  Iwa_FlowPaintBrushFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
};

#endif