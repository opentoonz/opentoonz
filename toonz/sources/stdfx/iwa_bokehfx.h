#pragma once

/*------------------------------------
Iwa_BokehFx
Apply an off-focus effect to the source image, using user input iris image.
It considers characteristics of films (which is known as Hurter–Driffield
curves)
or human eye's perception (which is known as Weber–Fechner law).
For filtering process I used KissFFT, an FFT library by Mark Borgerding,
distributed with a 3-clause BSD-style license.
------------------------------------*/

#ifndef IWA_BOKEHFX_H
#define IWA_BOKEHFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include "traster.h"

#include <QList>
#include <QThread>

#include "iwa_bokeh_util.h"

const int LAYER_NUM = 5;

class Iwa_BokehFx : public Iwa_BokehCommonFx {
  FX_PLUGIN_DECLARATION(Iwa_BokehFx)

  struct LAYERPARAM {
    TRasterFxPort m_source;
    TBoolParamP m_premultiply;
    TDoubleParamP m_distance;  // The layer distance from the camera (0-1)
    TDoubleParamP m_bokehAdjustment;  // Factor for adjusting distance (= focal
                                      // distance - layer distance) (0-2.0)
  } m_layerParams[LAYER_NUM];

  // Sort source layers by distance
  QList<int> getSortedSourceIndices(double frame);
  // Compute the bokeh size for each layer. The source tile will be enlarged by
  // the largest size of them.
  QMap<int, double> getIrisSizes(const double frame,
                                 const QList<int> sourceIndices,
                                 const double bokehPixelAmount,
                                 double &maxIrisSize);

public:
  Iwa_BokehFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  void onFxVersionSet() final override;
};

#endif
