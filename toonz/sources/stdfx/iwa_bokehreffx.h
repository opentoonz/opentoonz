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

#include "tools/kiss_fftnd.h"

struct float4 {
  float x, y, z, w;
};

//------------------------------------

class BokehRefThread : public QThread {
  int m_channel;
  volatile bool m_finished;

  kiss_fft_cpx* m_fftcpx_channel_before;
  kiss_fft_cpx* m_fftcpx_channel;
  kiss_fft_cpx* m_fftcpx_alpha;
  kiss_fft_cpx* m_fftcpx_iris;
  float4* m_result_buff;

  kiss_fftnd_cfg m_kissfft_plan_fwd, m_kissfft_plan_bkwd;

  TDimensionI m_dim;
  bool m_isTerminated;

public:
  BokehRefThread(int channel, kiss_fft_cpx* fftcpx_channel_before,
                 kiss_fft_cpx* fftcpx_channel, kiss_fft_cpx* fftcpx_alpha,
                 kiss_fft_cpx* fftcpx_iris, float4* result_buff,
                 kiss_fftnd_cfg kissfft_plan_fwd,
                 kiss_fftnd_cfg kissfft_plan_bkwd, TDimensionI& dim);

  void run() override;

  bool isFinished() { return m_finished; }
  void terminateThread() { m_isTerminated = true; }
};

//------------------------------------

class Iwa_BokehRefFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_BokehRefFx)

protected:
  TRasterFxPort m_iris;    // iris image
  TRasterFxPort m_source;  // source image
  TRasterFxPort m_depth;   // depth reference image

  TDoubleParamP m_onFocusDistance;  // Focus Distance (0-1)
  TDoubleParamP m_bokehAmount;  // The maximum bokeh size. The size of bokeh at
                                // the layer separated by 1.0 from the focal
                                // position
  TDoubleParamP m_hardness;     // Film gamma
  TIntParamP m_distancePrecision;  // Separation of depth image

  TBoolParamP m_fillGap;  // Toggles whether to extend pixels behind the front
                          // layers in order to fill gap
  // It should be ON for normal backgrounds and should be OFF for the layer
  // which is
  // "floating" from the lower layers such as dust, snowflake or spider-web.
  TBoolParamP m_doMedian;  // (Effective only when the Fill Gap option is ON)
  // Toggles whether to use Median Filter for extending the pixels.

  // Get the pixel size of bokehAmount ( referenced ino_blur.cpp )
  float getBokehPixelAmount(const double frame, const TAffine affine);

  // normalize the source raster image to 0-1 and set to dstMem
  // returns true if the source is (seems to be) premultiplied
  template <typename RASTER, typename PIXEL>
  bool setSourceRaster(const RASTER srcRas, float4* dstMem, TDimensionI dim);

  // normalize brightness of the depth reference image to unsigned char
  // and store into dstMem
  template <typename RASTER, typename PIXEL>
  void setDepthRaster(const RASTER srcRas, unsigned char* dstMem,
                      TDimensionI dim);
  template <typename RASTER, typename PIXEL>
  void setDepthRasterGray(const RASTER srcRas, unsigned char* dstMem,
                          TDimensionI dim);

  // create the depth index map
  void defineSegemntDepth(const unsigned char* indexMap_main,
                          const unsigned char* indexMap_sub,
                          const float* mainSub_ratio,
                          const unsigned char* depth_buff,
                          const TDimensionI& dimOut, const double frame,
                          QVector<float>& segmentDepth_main,
                          QVector<float>& segmentDepth_sub);

  // set the result
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4* srcMem, const RASTER dstRas, TDimensionI dim,
                       TDimensionI margin);

  // obtain iris size from the depth value
  float calcIrisSize(const float depth, const float bokehPixelAmount,
                     const double onFocusDistance);

  // resize/invert the iris according to the size ratio
  // normalize the brightness
  // resize to the output size
  void convertIris(const float irisSize, const TRectD& irisBBox,
                   const TTile& irisTile, const TDimensionI& enlargedDim,
                   kiss_fft_cpx* fftcpx_iris_before);

  // convert source image value rgb -> exposure
  void convertRGBToExposure(const float4* source_buff, int size,
                            float filmGamma, bool sourceIsPremultiplied);

  // generate the segment layer source at the current depth
  // considering fillGap and doMedian options
  void retrieveLayer(const float4* source_buff,
                     const float4* segment_layer_buff,
                     const unsigned char* indexMap_mainSub, int index, int lx,
                     int ly, bool fillGap, bool doMedian, int margin);

  // apply single median filter
  void doSingleMedian(const float4* source_buff,
                      const float4* segment_layer_buff,
                      const unsigned char* indexMap_mainSub, int index, int lx,
                      int ly, const unsigned char* generation_buff, int curGen);

  // normal-composite the layer as is, without filtering
  void compositeAsIs(const float4* segment_layer_buff,
                     const float4* result_buff_mainSub, int size);

  // retrieve segment layer image for each channel
  void retrieveChannel(const float4* segment_layer_buff,  // src
                       kiss_fft_cpx* fftcpx_r_before,     // dst
                       kiss_fft_cpx* fftcpx_g_before,     // dst
                       kiss_fft_cpx* fftcpx_b_before,     // dst
                       kiss_fft_cpx* fftcpx_a_before,     // dst
                       int size);

  // multiply filter on channel
  void multiplyFilter(kiss_fft_cpx* fftcpx_channel,  // dst
                      kiss_fft_cpx* fftcpx_iris,     // filter
                      int size);

  // normal comosite the alpha channel
  void compositeAlpha(const float4* result_buff,         // dst
                      const kiss_fft_cpx* fftcpx_alpha,  // alpha
                      int lx, int ly);

  // interpolate main and sub exposures
  // convert exposure -> RGB (0-1)
  // set to the result
  void interpolateExposureAndConvertToRGB(
      const float4* result_main_buff,  // result1
      const float4* result_sub_buff,   // result2
      const float* mainSub_ratio,      // ratio
      float filmGamma,
      const float4* source_buff,  // dst
      int size);

public:
  Iwa_BokehRefFx();

  void doCompute(TTile& tile, double frame, const TRenderSettings& settings);

  bool doGetBBox(double frame, TRectD& bBox, const TRenderSettings& info);

  bool canHandle(const TRenderSettings& info, double frame);

  void doCompute_CPU(const double frame, const TRenderSettings& settings,
                     float bokehPixelAmount, float maxIrisSize, int margin,
                     TDimensionI& dimOut, float4* source_buff,
                     unsigned char* indexMap_main, unsigned char* indexMap_sub,
                     float* mainSub_ratio, QVector<float>& segmentDepth_main,
                     QVector<float>& segmentDepth_sub, TTile& irisTile,
                     TRectD& irisBBox, bool sourceIsPremultiplied);
};

#endif
