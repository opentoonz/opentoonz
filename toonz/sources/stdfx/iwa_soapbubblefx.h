#pragma once

/*------------------------------------
Iwa_SoapBubbleFx
Generates thin film interference colors from two reference images;
one is for thickness and the other one is for shape or normal vector
distribution of the film.
Inherits Iwa_SpectrumFx.
------------------------------------*/

#ifndef IWA_SOAPBUBBLE_H
#define IWA_SOAPBUBBLE_H

#include "iwa_spectrumfx.h"

class Iwa_SoapBubbleFx final : public Iwa_SpectrumFx {
  FX_PLUGIN_DECLARATION(Iwa_SoapBubbleFx)

protected:
  /* target shape, used to create a pseudo normal vector */
  TRasterFxPort m_shape;
  /* another option, to input a depth map directly */
  TRasterFxPort m_depth;
  // shape parameters
  TDoubleParamP m_binarize_threshold;
  TDoubleParamP m_shape_aspect_ratio;
  TDoubleParamP m_blur_radius;
  TDoubleParamP m_blur_power;

  // noise parameters
  TIntParamP m_normal_sample_distance;
  TIntParamP m_noise_sub_depth;
  TDoubleParamP m_noise_resolution_s;
  TDoubleParamP m_noise_resolution_t;
  TDoubleParamP m_noise_sub_composite_ratio;
  TDoubleParamP m_noise_evolution;
  TDoubleParamP m_noise_depth_mix_ratio;
  TDoubleParamP m_noise_thickness_mix_ratio;

  template <typename RASTER, typename PIXEL>
  void convertToBrightness(const RASTER srcRas, float* dst, TDimensionI dim);

  template <typename RASTER, typename PIXEL>
  void convertToRaster(const RASTER ras, float* thickness_map_p,
                       float* depth_map_p, TDimensionI dim,
                       float3* bubbleColor_p);

  void processShape(double frame, TTile& shape_tile, float* depth_map_p,
                    TDimensionI dim, const TRenderSettings& settings);

  void do_binarize(TRaster32P srcRas, char* dst_p, float thres,
                   float* distance_p, TDimensionI dim);

  void do_createBlurFilter(float* dst_p, int size, float radius);

  void do_applyFilter(float* depth_map_p, TDimensionI dim, float* distace_p,
                      char* binarized_p, float* blur_filter_p,
                      int blur_filter_size, double frame);

  void processNoise(float* thickness_map_p, float* depth_map_p, TDimensionI dim,
                    double frame, const TRenderSettings& settings);

  void calc_norm_angle(float* norm_angle_p, float* depth_map_p, TDimensionI dim,
                       int shrink);

  void make_noise_map(float* noise_map_p, float* depth_map_p,
                      float* norm_angle_p, TDimensionI dim,
                      const QList<int>& noise_amount,
                      const QList<QSize>& noise_base_resolution,
                      int noise_sub_depth, float* noise_base);

  float noise_interp(int left, int right, int bottom, int top, float ratio_s,
                     float ratio_t, float* noise_layer_base, int noise_dim_x);

  void add_noise(float* thickness_map_p, float* depth_map_p, TDimensionI dim,
                 float* noise_map_p, float noise_thickness_mix_ratio,
                 float noise_depth_mix_ratio);

  void do_distance_transform(float* dst_p, TDimensionI dim, double frame);

public:
  Iwa_SoapBubbleFx();

  void doCompute(TTile& tile, double frame,
                 const TRenderSettings& settings) override;
};

#endif