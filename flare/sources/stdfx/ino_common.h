#pragma once

#ifndef ino_common_h
#define ino_common_h

#include "trop.h"
#include "trasterfx.h"
#include "stdfx.h"

namespace ino {
/* 一時バッファとの変換機能 */
void ras_to_arr(const TRasterP in_ras, const int channels,
                unsigned char* out_arr);
void ras_to_float_arr(const TRasterP in_ras, const int channels,
                      float* out_arr);
void arr_to_ras(const unsigned char* in_arr, const int channels,
                TRasterP out_ras, const int margin);
void float_arr_to_ras(const unsigned char* in_arr, const int channels,
                      TRasterP out_ras, const int margin);
void ras_to_vec(const TRasterP ras, const int channels,
                std::vector<unsigned char>& vec);
void vec_to_ras(std::vector<unsigned char>& vec, const int channels,
                TRasterP ras, const int margin = 0);
void ras_to_ref_float_arr(const TRasterP in_ras, float* out_arr,
                          const int refer_mode);

// void Lx_to_wrap( TRasterP ras );

/* logのserverアクセスON/OFF,install時設定をするための機能 */
/* TEnv::getConfigDir() + "fx_ino_no_log.setup"
        が存在するとtrueを返す */
bool log_enable_sw(void);

/* toonz6.0.x専用の固定値を返すinline(埋め込み)関数 */
inline double param_range(void) { return 1.0; }  // 1 or 100%
inline int channels(void) { return 4; }          // RGBM is 4 channels
inline int bits(const TRasterP ras) {
  return ((TRaster64P)ras) ? (std::numeric_limits<unsigned short>::digits)
         : ((TRaster32P)ras)
             ? (std::numeric_limits<unsigned char>::digits)
             : (std::numeric_limits<float>::digits);  // TRasterFP
}
inline int pixel_bits(const TRasterP ras) {
  return ino::channels() * ino::bits(ras);
}
// inline double pixel_per_mm(void) { return 640. / 12. / 25.4; }
inline double pixel_per_mm(void) { return 1.; }
// inline double pixel_per_inch(void) { return 640. / 12.; }
}  // namespace ino

class TBlendForeBackRasterFx : public TRasterFx {
public:
  enum ColorSpaceMode { Auto = 0, Linear, Nonlinear };

protected:
  TRasterFxPort m_up;
  TRasterFxPort m_down;
  TDoubleParamP m_opacity;
  TBoolParamP m_clipping_mask;

  TBoolParamP m_linear;
  TIntEnumParamP m_colorSpaceMode;

  TDoubleParamP m_gamma;
  TDoubleParamP m_gammaAdjust;

  // If the pixel is premultiplied, divide color data by the alpha before
  // converting from the colorspace, and then multiply by the alpha afterwards.
  // This will correct the color of the semi-transparent pixels in most cases.
  TBoolParamP m_premultiplied;

  TBoolParamP m_alpha_rendering;  // optional

  void dryComputeUpAndDown(TRectD& rect, double frame,
                           const TRenderSettings& rs,
                           bool upComputesWholeTile = false);

  void doComputeFx(TRasterP& dn_ras_out, const TRasterP& up_ras,
                   const TPoint& pos, const double up_opacity,
                   const double gammaDif, const double colorSpaceGamma,
                   const bool linear_sw);

  template <class T, class Q>
  void nonlinearTmpl(TRasterPT<T> dn_ras_out, const TRasterPT<T>& up_ras,
                     const double up_opacity);

  template <class T, class Q>
  void linearTmpl(TRasterPT<T> dn_ras_out, const TRasterPT<T>& up_ras,
                  const double up_opacity, const double gammaDif);

  template <class T, class Q>
  void premultiToUnpremulti(TRasterPT<T> dn_ras, const TRasterPT<T>& up_ras,
                            const double colorSpaceGamma);

  // when compute in xyz color space, do not clamp channel values in the kernel
  virtual void brendKernel(double& dnr, double& dng, double& dnb, double& dna,
                           const double up_, double upg, double upb, double upa,
                           const double upopacity,
                           const bool alpha_rendering_sw = true,
                           const bool do_clamp           = true) = 0;

  void computeUpAndDown(TTile& tile, double frame, const TRenderSettings& rs,
                        TRasterP& dn_ras, TRasterP& up_ras,
                        bool upComputesWholeTile = false);

public:
  TBlendForeBackRasterFx(bool clipping_mask, bool has_alpha_option = false);

  void onFxVersionSet() override;
  void onObsoleteParamLoaded(const std::string&) override;

  bool canHandle(const TRenderSettings& rs, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& rs) override;
  int getMemoryRequirement(const TRectD& rect, double frame,
                           const TRenderSettings& rs) override {
    return TRasterFx::memorySize(rect, rs.m_bpp);
  }
  void doDryCompute(TRectD& rect, double frame,
                    const TRenderSettings& rs) override {
    this->dryComputeUpAndDown(rect, frame, rs, false);
  }

  void doCompute(TTile& tile, double frame, const TRenderSettings& rs) override;

  /* FX nodeが無効のときの、表示port番号 */
  int getPreferredInputPort() override { return 1; }

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override;

  std::string getPluginId() const override { return PLUGIN_PREFIX; }
};

template <>
void TBlendForeBackRasterFx::nonlinearTmpl<TPixelF, float>(
    TRasterFP dn_ras_out, const TRasterFP& up_ras, const double up_opacity);

template <>
void TBlendForeBackRasterFx::linearTmpl<TPixelF, float>(TRasterFP dn_ras_out,
                                                        const TRasterFP& up_ras,
                                                        const double up_opacity,
                                                        const double gammaDif);

template <>
void TBlendForeBackRasterFx::premultiToUnpremulti<TPixelF, float>(
    TRasterFP dn_ras, const TRasterFP& up_ras, const double colorSpaceGamma);

#endif /* !ino_common_h */
