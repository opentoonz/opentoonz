#pragma once

#ifndef IWA_FLOORBUMPFX_H
#define IWA_FLOORBUMPFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

#include <QVector3D>
struct float4 {
  float x, y, z, w;
  float4 operator*(const float &v) const {
    return float4{x * v, y * v, z * v, w * v};
  }
  float4 &operator+=(const float4 &v) {
    x += v.x;
    y += v.y;
    z += v.z;
    w += v.w;
    return *this;
  }
};

class Iwa_FloorBumpFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_FloorBumpFx)
public:
  enum RenderMode {
    TextureMode = 0,
    DiffuseMode,
    SpecularMode,
    FresnelMode,
    RefractionMode,
    ReflectionMode
  };

  struct FloorBumpVars {
    double waveHeight;
    double displacement;
    int refHeight;
    TDimensionI outDim;
    TDimensionI resultDim;
    int margin;
    double precision;
    // add margins to all ends and multiply by precision value
    TDimensionI sourceDim;  // u
    // only add margins for height image
    TDimensionI refDim;  // u

    // collecting parameters
    double textureOffsetAmount;  // u
    double spread;               // u
    double camAltitude;
    int renderMode;       // u
    bool differenceMode;  // u

    // making pixels in gray128 to be zero level height
    // ( 128/255. IT'S NOT 0.5! )
    double zeroLevel;  // u
    double H;          // u
    double W;          // u
    // angle between the optical axis and the horizontal axis
    double angle_el;  // u
    // Y coordinate of the Eye position (P)
    double P_y;  // u
    // distance from the Eye (P) to the center of the projection plane (T)
    double d_PT;  // u

    // Z-Y position of the center of top edge of the projection plane (A)
    QPointF A;  // u
    // Z-Y position of the center of bottom edge of the projection plane (B)
    QPointF B;  // u

    // (C) is an intersection between the XZ plane and the line P->B
    double C_z;               // u
    QVector3D sunVec;         // u
    double base_fresnel_ref;  // u
    double depth, r_index;    // uu
    double distance;          // u
    QVector3D eyePos;         // u
  };

protected:
  TRasterFxPort m_heightRef;  // height reference image
  TRasterFxPort m_texture;    // texture image
  TRasterFxPort m_dispRef;    // displacement image

  TIntEnumParamP m_renderMode;

  TDoubleParamP m_fov;  // camera fov (degrees)
  TDoubleParamP
      m_cameraAltitude;  // height of the bottom edge of projection plane

  TDoubleParamP m_eyeLevel;   // height of the vanishing point
  TDoubleParamP m_drawLevel;  // upper rendering boundary

  TDoubleParamP m_waveHeight;  // height of waves to the both sides (i.e.
                               // amplitude becomes 2*waveHeight)

  TBoolParamP
      m_differenceMode;  // available in diffuse and fresnel mode,
                         // render brightness difference from unbumped state

  // Texture mode parameters
  TDoubleParamP m_textureOffsetAmount;  // amount of texture trailing along with
                                        // gradient of the bump
  TDoubleParamP
      m_textureOffsetSpread;  // adding "blur" to the gradient distribution

  TDoubleParamP m_sourcePrecision;  // to load the texture with higher dpi
  TDoubleParamP m_souceMargin;  // margins to be added to all edges for both the
                                // height reference and the texture images

  TDoubleParamP m_displacement;

  // Shading (Diffuse and Specular) modes parameters
  TDoubleParamP m_lightAzimuth;  // light is in front of camera with azimuth=0.
                                 // clockwise angle (degrees)
  TDoubleParamP m_lightElevation;  // (degrees)

  // Refraction mode parameters
  TDoubleParamP m_depth;  // water depth. the bottom will be placed at -depth
  TDoubleParamP
      m_refractiveIndex;  // refractive index of the medium under the surface

  // Reflection mode parameter
  TDoubleParamP m_distanceLevel;  // the distance of the reflected object
                                  // specified by the position on the surface

  // convert output values (in float4) to channel value
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int drawLevel);

  // convert input tile's channel values to float4 values
  template <typename RASTER, typename PIXEL>
  void setSourceRaster(const RASTER srcRas, float4 *srcMem, TDimensionI dim);

  void setRefRaster(const TRaster64P refRas, float *refMem, TDimensionI dim,
                    bool isRef);

  inline void initVars(FloorBumpVars &vars, TTile &tile,
                       const TRenderSettings &settings, double frame);

public:
  Iwa_FloorBumpFx();
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;
  void doCompute_CPU(TTile &tile, const double frame,
                     const TRenderSettings &settings, const FloorBumpVars &vars,
                     float4 *source_host, float *ref_host, float4 *result_host);
  void doCompute_with_Displacement(TTile &tile, const double frame,
                                   const TRenderSettings &settings,
                                   const FloorBumpVars &vars,
                                   float4 *source_host, float *ref_host,
                                   float *disp_host, float4 *result_host);
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif