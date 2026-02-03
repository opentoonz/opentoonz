#pragma once
//----------------------------------------
// Iwa_BokehAdvancedFx
// Advanced version of Bokeh Fx Iwa
// - Enabled to set different hardness for each layer
// - Enabled to apply the depth reference image for each layer
//----------------------------------------

#ifndef IWA_BOKEH_ADVANCED_H
#define IWA_BOKEH_ADVANCED_H

#include "stdfx.h"
#include "tfxparam.h"
#include "traster.h"

#include "iwa_bokeh_util.h"

#include <array>
#include <QThread>

const int LAYER_NUM = 5;

//------------------------------------

class Iwa_BokehAdvancedFx : public Iwa_BokehCommonFx {
  FX_PLUGIN_DECLARATION(Iwa_BokehAdvancedFx)

protected:
  TFxPortDG m_control;

  TBoolParamP m_hardnessPerSource;  // switch between layer and master hardness

  struct LAYERPARAM {
    TRasterFxPort m_source;
    TDoubleParamP m_distance;  // The layer distance from the camera (0-1)
    TDoubleParamP m_bokehAdjustment;  // Factor for adjusting distance (= focal
                                      // distance - layer distance) (0-2.0)
    TDoubleParamP m_hardness;         // film gamma for each layer (Version1)
    TDoubleParamP m_gamma;            // film gamma for each layer (Version2)
    TDoubleParamP m_gammaAdjust;  // Gamma offset from the current color space
                                  // gamma (Version 3)

    TIntParamP m_depth_ref;      // port index of depth reference image
    TDoubleParamP m_depthRange;  // distance range varies depends on the
                                 // brightness of the reference image (0-1)
    TBoolParamP m_fillGap;
    TBoolParamP m_doMedian;
  };
  std::array<LAYERPARAM, LAYER_NUM> m_layerParams;

  // sort source images
  QList<int> getSortedSourceIndices(double frame);
  // Compute the bokeh size for each layer. The source tile will be enlarged by
  // the largest size of them.
  QMap<int, double> getIrisSizes(const double frame,
                                 const QList<int> sourceIndices,
                                 const double bokehPixelAmount,
                                 double& maxIrisSize);
  // return true if the control image is available and used
  bool portIsUsed(int portIndex);

public:
  Iwa_BokehAdvancedFx();

  void doCompute(TTile& tile, double frame,
                 const TRenderSettings& settings) override;

  int dynamicPortGroupsCount() const override { return 1; }
  const TFxPortDG* dynamicPortGroup(int g) const override {
    return (g == 0) ? &m_control : 0;
  }
  void onFxVersionSet() final override;
};

#endif