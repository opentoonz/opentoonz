#pragma once

/*------------------------------------
Iwa_BokehRefFx
Apply an off-focus effect to a single source image.
The amount of bokeh is specified by user input reference image,
which represents the depth by brightness of pixels.
It considers characteristics of films (which is known as Hurter–Driffield
curves) or human eye's perception (which is known as Weber–Fechner law).
For filtering process I used KissFFT, an FFT library by Mark Borgerding,
distributed with a 3-clause BSD-style license.
------------------------------------*/

#ifndef IWA_BOKEH_REF_H
#define IWA_BOKEH_REF_H

#include "stdfx.h"
#include "tfxparam.h"

#include <QVector>
#include <QThread>

#include "iwa_bokeh_util.h"

//------------------------------------

class Iwa_BokehRefFx : public Iwa_BokehCommonFx {
  FX_PLUGIN_DECLARATION(Iwa_BokehRefFx)

protected:
  TRasterFxPort m_source;  // source image
  TRasterFxPort m_depth;   // depth reference image

  TIntParamP m_distancePrecision;  // Separation of depth image

  TBoolParamP m_fillGap;  // Toggles whether to extend pixels behind the front
                          // layers in order to fill gap
  // It should be ON for normal backgrounds and should be OFF for the layer
  // which is
  // "floating" from the lower layers such as dust, snowflake or spider-web.
  TBoolParamP m_doMedian;  // (Effective only when the Fill Gap option is ON)
  // Toggles whether to use Median Filter for extending the pixels.

  // normalize brightness of the depth reference image to unsigned char
  // and store into dstMem
  template <typename RASTER, typename PIXEL>
  void setDepthRaster(const RASTER srcRas, unsigned char* dstMem,
                      TDimensionI dim);
  template <typename RASTER, typename PIXEL>
  void setDepthRasterGray(const RASTER srcRas, unsigned char* dstMem,
                          TDimensionI dim);

public:
  Iwa_BokehRefFx();

  void doCompute(TTile& tile, double frame,
                 const TRenderSettings& settings) override;

  void onFxVersionSet() final override;
};

#endif
