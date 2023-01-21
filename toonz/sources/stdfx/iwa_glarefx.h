#pragma once

/*------------------------------------
Iwa_GlareFx
------------------------------------*/

#ifndef IWA_GLAREFX_H
#define IWA_GLAREFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include "traster.h"
#include "tparamset.h"

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
struct double3 {
  double x, y, z;
};

class Iwa_GlareFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_GlareFx)

protected:
  TRasterFxPort m_source;
  TRasterFxPort m_iris;
  // rendering mode (filter preview / render / iris)
  TIntEnumParamP m_renderMode;
  TIntEnumParamP m_irisMode;

  TDoubleParamP m_irisScale;
  TDoubleParamP m_irisGearEdgeCount;
  TIntParamP m_irisRandomSeed;
  TDoubleParamP m_irisSymmetry;
  TIntEnumParamP m_irisAppearance;

  TDoubleParamP m_intensity;
  TDoubleParamP m_size;

  TDoubleParamP m_rotation;
  TDoubleParamP m_aberration;

  TDoubleParamP m_noise_factor;
  TDoubleParamP m_noise_size;
  TIntEnumParamP m_noise_octave;
  TDoubleParamP m_noise_evolution;
  TPointParamP m_noise_offset;

  enum { RenderMode_FilterPreview = 0, RenderMode_Render, RenderMode_Iris };

  enum {
    Iris_InputImage = 0,
    Iris_Square,
    Iris_Hexagon,
    Iris_Octagon,
    Iris_GearShape
  };

  enum {
    Appearance_ThinLine,
    Appearance_MediumLine,
    Appearance_ThickLine,
    Appearance_Fill,
  };

  double getSizePixelAmount(const double val, const TAffine affine);

  void drawPresetIris(TRaster32P irisRas, double irisSize, const double frame);

  // Resize / flip the iris image according to the size ratio.
  // Normalize the brightness of the iris image.
  // Enlarge the iris to the output size.
  void convertIris(kiss_fft_cpx *kissfft_comp_iris_before, const int &dimIris,
                   const TRectD &irisBBox, const TRasterP irisRaster);

  void powerSpectrum2GlarePattern(const double frame, const TAffine affine,
                                  kiss_fft_cpx *spectrum, double3 *glare,
                                  int dimIris, double intensity,
                                  double irisResizeFactor);

  void distortGlarePattern(const double frame, const TAffine affine,
                           double *glare, const int dimIris);

  // filter preview mode
  template <typename RASTER, typename PIXEL>
  void setFilterPreviewToResult(const RASTER ras, double3 *glare, int dimIris,
                                int2 margin);

  // put the source tile's brightness to fft buffer
  template <typename RASTER, typename PIXEL>
  void setSourceTileToBuffer(const RASTER ras, kiss_fft_cpx *buf);
  template <typename RASTER, typename PIXEL>
  void setSourceTileToBuffer(const RASTER ras, kiss_fft_cpx *buf, int channel);

  void setGlarePatternToBuffer(const double3 *glare, kiss_fft_cpx *buf,
                               const int channel, const int dimIris,
                               const TDimensionI &dimOut);

  void multiplyFilter(kiss_fft_cpx *glare, const kiss_fft_cpx *source,
                      const int count);

  template <typename RASTER, typename PIXEL>
  void setChannelToResult(const RASTER ras, kiss_fft_cpx *buf, int channel,
                          const TDimensionI &dimOut);

public:
  Iwa_GlareFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override;
};

#endif
