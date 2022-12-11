#pragma once

#ifndef IWA_MOTION_FLOW_FX_H
#define IWA_MOTION_FLOW_FX_H

#include "tfxparam.h"
#include "stdfx.h"

#include "motionawarebasefx.h"

struct double3 {
  double x, y, z;
};

class Iwa_MotionFlowFx final : public MotionAwareAffineFx {
  FX_PLUGIN_DECLARATION(Iwa_MotionFlowFx)

  TIntEnumParamP m_normalizeType;
  TDoubleParamP m_normalizeRange;

  enum NormalizeType { NORMALIZE_AUTO = 0, NORMALIZE_MANUAL };

public:
  Iwa_MotionFlowFx();

  void doCompute(TTile& tile, double frame,
                 const TRenderSettings& settings) override;

  template <typename RASTER, typename PIXEL>
  void setOutRas(RASTER ras, double3* outBuf, double norm_range);

  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& info) override;
  bool canHandle(const TRenderSettings& info, double frame) override;
  // Since there is a possibility that the reference object is moving,
  // Change the alias every frame
  std::string getAlias(double frame,
                       const TRenderSettings& info) const override;
};
#endif
