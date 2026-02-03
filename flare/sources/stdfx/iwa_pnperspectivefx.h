#pragma once

/*------------------------------------------------------------
 Iwa_PNPerspectiveFx
 render perspective noise pattern.
------------------------------------------------------------*/
#ifndef IWA_PN_PERSPECTIVE_H
#define IWA_PN_PERSPECTIVE_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

struct double3 {
  double x, y, z;
};
struct double4 {
  double x, y, z, w;
};

// parameters
struct PN_Params {
  enum RenderMode {
    Noise = 0,
    Noise_NoResample,
    WarpHV,
    Fresnel,
    WarpHV2
  } renderMode;
  enum NoiseType { Perlin = 0, Simplex } noiseType;
  double size;         // noise size of the first generation
  int octaves;         // generation count
  TPointD offset;      // offset of the first generation
  double p_intensity;  // intensity ratio between gen
  double p_size;       // size ratio between gen
  double p_offset;     // offset ratio between gen
  TPointD eyeLevel;
  int drawLevel;  // vertical distance from the bottom to the top of the drawing
                  // region
  bool alp_rend_sw;
  double waveHeight;  // used in the WarpHV and Fresnel modes
  double fy_2;
  double A;
  double3 cam_pos;
  double base_fresnel_ref;  // used in the Fresnel mode
  double top_fresnel_ref;   // used in the Fresnel mode
  double int_sum;
  TAffine aff;
  double time;
  double p_evolution;
};

class Iwa_PNPerspectiveFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_PNPerspectiveFx)

  TIntEnumParamP m_renderMode;
  TIntEnumParamP m_noiseType;
  TDoubleParamP m_size;
  TDoubleParamP m_evolution;
  TIntEnumParamP m_octaves;
  TPointParamP m_offset;

  TDoubleParamP m_persistance_intensity;
  TDoubleParamP m_persistance_size;
  TDoubleParamP m_persistance_evolution;
  TDoubleParamP m_persistance_offset;

  TDoubleParamP m_fov;      // vertical angle of camera fov in degrees
  TPointParamP m_eyeLevel;  // vanishing point

  TBoolParamP m_alpha_rendering;  // specify if render noise pattern to the
                                  // alpha channel as well

  TDoubleParamP m_waveHeight;

  TBoolParamP m_normalize_fresnel;  // normalize fresnel reflectivity
  TDoubleParamP m_normalize_margin;

  template <typename RASTER, typename PIXEL>
  void setOutputRaster(double4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int drawLevel, const bool alp_rend_sw);

  void getPNParameters(TTile &tile, double frame,
                       const TRenderSettings &settings, PN_Params &params,
                       TDimensionI &dimOut);

  // render for 2 Noise modes
  void calcPerinNoise_CPU(double4 *out_host, TDimensionI &dimOut, PN_Params &p,
                          bool doResample);

  // render for WarpHV / Fresnel modes
  void calcPNNormal_CPU(double4 *out_host, TDimensionI &dimOut, PN_Params &p,
                        bool isSubWave = false);

public:
  Iwa_PNPerspectiveFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;

  void doCompute_CPU(double frame, const TRenderSettings &settings,
                     double4 *out_host, TDimensionI &dimOut,
                     PN_Params &pnParams);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif