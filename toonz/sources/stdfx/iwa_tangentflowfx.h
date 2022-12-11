#pragma once

/*--------------------------------
Iwa_TangentFlowFx
Computing a smooth, feature-preserving local edge ﬂow (edge tangent flow, ETF)
Implementation of "Coherent Line Drawing" by H.Kang et al, Proc. NPAR 2007.
----------------------------------*/

#ifndef IWA_TANGENT_FLOW_FX_H
#define IWA_TANGENT_FLOW_FX_H

#include "stdfx.h"
#include "tfxparam.h"

#include <QThread>

struct double2 {
  double x, y;
  double2(double _x = 0., double _y = 0.) {
    x = _x;
    y = _y;
  }
};
struct int2 {
  int x, y;

  int2 operator+(const int2& other) const {
    return int2{x + other.x, y + other.y};
  }
  int2 operator-(const int2& other) const {
    return int2{x - other.x, y - other.y};
  }
  int len2() const { return x * x + y * y; }
  int2(int _x = 0, int _y = 0) {
    x = _x;
    y = _y;
  }
};

class SobelFilterWorker : public QThread {
  double* m_source_buf;
  double2* m_flow_buf;
  double* m_grad_mag_buf;
  int2* m_offset_buf;
  double m_mag_threshold;
  TDimension m_dim;
  int m_yFrom, m_yTo;

  bool m_hasEmptyVector = false;

public:
  SobelFilterWorker(double* source_buf, double2* flow_buf, double* grad_mag_buf,
                    int2* offset_buf, double mag_threshold, TDimension dim,
                    int yFrom, int yTo)
      : m_source_buf(source_buf)
      , m_flow_buf(flow_buf)
      , m_grad_mag_buf(grad_mag_buf)
      , m_offset_buf(offset_buf)
      , m_mag_threshold(mag_threshold)
      , m_dim(dim)
      , m_yFrom(yFrom)
      , m_yTo(yTo) {}

  void run();

  bool hasEmptyVector() { return m_hasEmptyVector; }
};

class TangentFlowWorker : public QThread {
  double2* m_flow_cur_buf;
  double2* m_flow_new_buf;
  double* m_grad_mag_buf;
  TDimension m_dim;
  int m_kernelRadius;
  int m_yFrom, m_yTo;

public:
  TangentFlowWorker(double2* flow_cur_buf, double2* flow_new_buf,
                    double* grad_mag_buf, TDimension dim, int kernelRadius,
                    int yFrom, int yTo)
      : m_flow_cur_buf(flow_cur_buf)
      , m_flow_new_buf(flow_new_buf)
      , m_grad_mag_buf(grad_mag_buf)
      , m_dim(dim)
      , m_kernelRadius(kernelRadius)
      , m_yFrom(yFrom)
      , m_yTo(yTo) {}

  void run();
};

class Iwa_TangentFlowFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_TangentFlowFx)

protected:
  TRasterFxPort m_source;

  TIntParamP m_iteration;
  TDoubleParamP m_kernelRadius;

  TDoubleParamP m_threshold;

  TBoolParamP m_alignDirection;
  TDoubleParamP m_pivotAngle;

  // obtain source tile brightness, normalizing 0 to 1
  template <typename RASTER, typename PIXEL>
  void setSourceTileToBuffer(const RASTER srcRas, double* buf);

  // render vector field in red & green channels
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(double2* buf, double* grad_buf, const RASTER dstRas);

  // compute the initial vector field.
  // apply sobel filter, rotate by 90 degrees and normalize.
  // also obtaining gradient magnitude (length of the vector length)
  // empty regions will be filled by using Fast Sweeping Method
  void computeInitialFlow(double* source_buf, double2* flow_buf,
                          double* grad_mag_buf, const TDimension dim,
                          double mag_threshold);
  // 基準の角度に向きを合わせる
  void alignFlowDirection(double2* flow_buf, const TDimension dim,
                          const double2& pivotVec);

public:
  Iwa_TangentFlowFx();

  void doCompute(TTile& tile, double frame,
                 const TRenderSettings& settings) override;

  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& info) override;

  bool canHandle(const TRenderSettings& info, double frame) override;
};

#endif
