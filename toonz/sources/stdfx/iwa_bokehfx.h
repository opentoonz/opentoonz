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

#include "tools/kiss_fftnd.h"

const int LAYER_NUM = 5;

struct double2 {
  double x, y;
};
struct int2 {
  int x, y;
};

class MyThread : public QThread {
public:
  enum Channel { Red = 0, Green, Blue };

private:
  Channel m_channel;

  volatile bool m_finished;

  TRasterP m_layerTileRas;
  TRasterP m_outTileRas;
  TRasterP m_tmpAlphaRas;

  kiss_fft_cpx *m_kissfft_comp_iris;

  float m_filmGamma;  // keep the film gamma in each thread as it is refered so
                      // often

  TRasterGR8P m_kissfft_comp_in_ras, m_kissfft_comp_out_ras;
  kiss_fft_cpx *m_kissfft_comp_in, *m_kissfft_comp_out;
  kiss_fftnd_cfg m_kissfft_plan_fwd, m_kissfft_plan_bkwd;

  bool m_isTerminated;

  // not used for now
  bool m_doLightenComp;

public:
  MyThread(Channel channel, TRasterP layerTileRas, TRasterP outTileRas,
           TRasterP tmpAlphaRas, kiss_fft_cpx *kissfft_comp_iris,
           float m_filmGamma,
           bool doLightenComp = false);  // not used for now

  // Convert the pixels from RGB values to exposures and multiply it by alpha
  // channel value.
  // Store the results in the real part of kiss_fft_cpx.
  template <typename RASTER, typename PIXEL>
  void setLayerRaster(const RASTER srcRas, kiss_fft_cpx *dstMem,
                      TDimensionI dim);

  // Composite the bokeh layer to the result
  template <typename RASTER, typename PIXEL, typename A_RASTER,
            typename A_PIXEL>
  void compositLayerToTile(const RASTER layerRas, const RASTER outTileRas,
                           const A_RASTER alphaRas, TDimensionI dim,
                           int2 margin);
  void run();

  bool isFinished() { return m_finished; }

  // RGB value <--> Exposure
  float valueToExposure(float value);
  float exposureToValue(float exposure);

  // memory allocation
  bool init();

  void terminateThread() { m_isTerminated = true; }

  bool checkTerminationAndCleanupThread();
};

class Iwa_BokehFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_BokehFx)

protected:
  TRasterFxPort m_iris;
  TDoubleParamP m_onFocusDistance;  // Focus Distance (0-1)
  TDoubleParamP m_bokehAmount;  // The maximum bokeh size. The size of bokeh at
                                // the layer separated by 1.0 from the focal
                                // position
  TDoubleParamP m_hardness;     // Film gamma

  struct LAYERPARAM {
    TRasterFxPort m_source;
    TBoolParamP m_premultiply;
    TDoubleParamP m_distance;  // The layer distance from the camera (0-1)
    TDoubleParamP m_bokehAdjustment;  // Factor for adjusting distance (= focal
                                      // distance - layer distance) (0-2.0)
  } m_layerParams[LAYER_NUM];

  // Sort source layers by distance
  QList<int> getSortedSourceIndices(double frame);
  // Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
  float getBokehPixelAmount(const double frame, const TAffine affine);
  // Compute the bokeh size for each layer. The source tile will be enlarged by
  // the largest size of them.
  QVector<float> getIrisSizes(const double frame,
                              const QList<int> sourceIndices,
                              const float bokehPixelAmount, float &maxIrisSize);
  //"Over" composite the layer to the output raster.
  void compositLayerAsIs(TTile &tile, TTile &layerTile, const double frame,
                         const TRenderSettings &settings, const int index);
  // Resize / flip the iris image according to the size ratio.
  // Normalize the brightness of the iris image.
  // Enlarge the iris to the output size.
  void convertIris(const float irisSize, kiss_fft_cpx *kissfft_comp_iris_before,
                   const TDimensionI &dimOut, const TRectD &irisBBox,
                   const TTile &irisTile);

  // Do FFT the alpha channel.
  // Forward FFT -> Multiply by the iris data -> Backward FFT
  void calcAlfaChannelBokeh(kiss_fft_cpx *kissfft_comp_iris, TTile &layerTile,
                            TRasterP tmpAlphaRas);

public:
  Iwa_BokehFx();

  void doCompute(TTile &tile, double frame, const TRenderSettings &settings);

  bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info);

  bool canHandle(const TRenderSettings &info, double frame);
};

#endif