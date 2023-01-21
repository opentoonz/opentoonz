#include "tenv.h"
#include "tsystem.h"

#include "ino_common.h"
#include "tfxparam.h"

#include <sstream> /* std::ostringstream */

/* copy and paste from
 igs_ifx_common.h */
namespace igs {
namespace image {
namespace rgba {
enum num { blu = 0, gre, red, alp, siz };
}
}  // namespace image
}  // namespace igs
//------------------------------------------------------------
namespace {
// T is TPixel32 or TPixel64
// U is unsigned char or unsigned short
template <class T, class U>
void ras_to_arr_(const TRasterPT<T> ras, U* arr, const int channels) {
  using namespace igs::image::rgba;

  for (int yy = 0; yy < ras->getLy(); ++yy) {
    const T* ras_sl = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arr += channels) {
      if (red < channels) {
        arr[red] = ras_sl[xx].r;
      }
      if (gre < channels) {
        arr[gre] = ras_sl[xx].g;
      }
      if (blu < channels) {
        arr[blu] = ras_sl[xx].b;
      }
      if (alp < channels) {
        arr[alp] = ras_sl[xx].m;
      }
    }
  }
}

// T is TPixel32, TPixel64 or TPixelF
// normalize to 0.0 - 1.0
template <class T>
void ras_to_float_arr_(const TRasterPT<T> ras, float* arr, const int channels) {
  using namespace igs::image::rgba;
  float fac = 1.f / (float)T::maxChannelValue;
  for (int yy = 0; yy < ras->getLy(); ++yy) {
    const T* ras_sl = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arr += channels) {
      if (red < channels) {
        arr[red] = (float)ras_sl[xx].r * fac;
      }
      if (gre < channels) {
        arr[gre] = (float)ras_sl[xx].g * fac;
      }
      if (blu < channels) {
        arr[blu] = (float)ras_sl[xx].b * fac;
      }
      if (alp < channels) {
        arr[alp] = (float)ras_sl[xx].m * fac;
      }
    }
  }
}

template <class U, class T>
void arr_to_ras_(const U* arr, const int channels, TRasterPT<T> ras,
                 const int margin  // default is 0
) {
  arr +=
      (ras->getLx() + margin + margin) * margin * channels + margin * channels;

  using namespace igs::image::rgba;

  for (int yy = 0; yy < ras->getLy();
       ++yy, arr += (ras->getLx() + margin + margin) * channels) {
    const U* arrx = arr;
    T* ras_sl     = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arrx += channels) {
      if (red < channels) {
        ras_sl[xx].r = arrx[red];
      }
      if (gre < channels) {
        ras_sl[xx].g = arrx[gre];
      }
      if (blu < channels) {
        ras_sl[xx].b = arrx[blu];
      }
      if (alp < channels) {
        ras_sl[xx].m = arrx[alp];
      }
    }
  }
}

template <class T>
void float_arr_to_ras_(const float* arr, const int channels, TRasterPT<T> ras,
                       const int margin  // default is 0
) {
  arr +=
      (ras->getLx() + margin + margin) * margin * channels + margin * channels;

  using namespace igs::image::rgba;
  float fac = (float)T::maxChannelValue;

  for (int yy = 0; yy < ras->getLy();
       ++yy, arr += (ras->getLx() + margin + margin) * channels) {
    const float* arrx = arr;
    T* ras_sl         = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arrx += channels) {
      if (red < channels) {
        ras_sl[xx].r =
            (arrx[red] >= 1.f) ? T::maxChannelValue
            : (arrx[red] <= 0.f)
                ? (typename T::Channel)0
                : (typename T::Channel)(std::round(arrx[red] * fac + 0.5f));
      }
      if (gre < channels) {
        ras_sl[xx].g =
            (arrx[gre] >= 1.f) ? T::maxChannelValue
            : (arrx[gre] <= 0.f)
                ? (typename T::Channel)0
                : (typename T::Channel)(std::round(arrx[gre] * fac + 0.5f));
      }
      if (blu < channels) {
        ras_sl[xx].b =
            (arrx[blu] >= 1.f) ? T::maxChannelValue
            : (arrx[blu] <= 0.f)
                ? (typename T::Channel)0
                : (typename T::Channel)(std::round(arrx[blu] * fac + 0.5f));
      }
      if (alp < channels) {
        ras_sl[xx].m =
            (arrx[alp] >= 1.f) ? T::maxChannelValue
            : (arrx[alp] <= 0.f)
                ? (typename T::Channel)0
                : (typename T::Channel)(std::round(arrx[alp] * fac + 0.5f));
      }
    }
  }
}

template <>
void float_arr_to_ras_<TPixelF>(const float* arr, const int channels,
                                TRasterFP ras,
                                const int margin  // default is 0
) {
  arr +=
      (ras->getLx() + margin + margin) * margin * channels + margin * channels;

  using namespace igs::image::rgba;

  for (int yy = 0; yy < ras->getLy();
       ++yy, arr += (ras->getLx() + margin + margin) * channels) {
    const float* arrx = arr;
    TPixelF* ras_sl   = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arrx += channels) {
      if (red < channels) ras_sl[xx].r = arrx[red];
      if (gre < channels) ras_sl[xx].g = arrx[gre];
      if (blu < channels) ras_sl[xx].b = arrx[blu];
      if (alp < channels) ras_sl[xx].m = arrx[alp];
    }
  }
}

template <class T>
float getFactor() {
  return 1.f / (float)T::maxChannelValue;
}

template <>
float getFactor<TPixelF>() {
  return 1.f;
}

// T is either TPixel32, TPixel64, or TPixelF
template <class T>
void ras_to_ref_float_arr_(const TRasterPT<T> ras, float* arr,
                           const int refer_mode) {
  float fac = getFactor<T>();
  for (int yy = 0; yy < ras->getLy(); ++yy) {
    const T* ras_sl = ras->pixels(yy);
    for (int xx = 0; xx < ras->getLx(); ++xx, arr++, ras_sl++) {
      switch (refer_mode) {
      case 0:
        *arr = static_cast<float>(ras_sl->r) * fac;
        break;
      case 1:
        *arr = static_cast<float>(ras_sl->g) * fac;
        break;
      case 2:
        *arr = static_cast<float>(ras_sl->b) * fac;
        break;
      case 3:
        *arr = static_cast<float>(ras_sl->m) * fac;
        break;
      case 4:
        *arr = /* 輝度(Luminance)(CCIR Rec.601) */
            (0.298912f * static_cast<float>(ras_sl->r) +
             0.586611f * static_cast<float>(ras_sl->g) +
             0.114478f * static_cast<float>(ras_sl->b)) *
            fac;
        break;
      }
      // clamp 0.f to 1.f in case computing TPixelF
      *arr = std::min(1.f, std::max(0.f, *arr));
    }
  }
}

}  // namespace
//--------------------
void ino::ras_to_arr(const TRasterP in_ras, const int channels,
                     unsigned char* out_arr) {
  if ((TRaster32P)in_ras) {
    ras_to_arr_<TPixel32, unsigned char>(in_ras, out_arr, channels);
  } else if ((TRaster64P)in_ras) {
    ras_to_arr_<TPixel64, unsigned short>(
        in_ras, reinterpret_cast<unsigned short*>(out_arr), channels);
  } else if ((TRasterFP)in_ras) {
    ras_to_float_arr(in_ras, channels, reinterpret_cast<float*>(out_arr));
  }
}
void ino::ras_to_float_arr(const TRasterP in_ras, const int channels,
                           float* out_arr) {
  if ((TRaster32P)in_ras) {
    ras_to_float_arr_<TPixel32>(in_ras, out_arr, channels);
  } else if ((TRaster64P)in_ras) {
    ras_to_float_arr_<TPixel64>(in_ras, out_arr, channels);
  } else if ((TRasterFP)in_ras) {
    ras_to_float_arr_<TPixelF>(in_ras, out_arr, channels);
  }
}
void ino::arr_to_ras(const unsigned char* in_arr, const int channels,
                     TRasterP out_ras, const int margin) {
  if ((TRaster32P)out_ras) {
    arr_to_ras_<unsigned char, TPixel32>(in_arr, channels, out_ras, margin);
  } else if ((TRaster64P)out_ras) {
    arr_to_ras_<unsigned short, TPixel64>(
        reinterpret_cast<const unsigned short*>(in_arr), channels, out_ras,
        margin);
  } else if ((TRasterFP)out_ras) {
    arr_to_ras_<float, TPixelF>(reinterpret_cast<const float*>(in_arr),
                                channels, out_ras, margin);
  }
}
void ino::float_arr_to_ras(const unsigned char* in_arr, const int channels,
                           TRasterP out_ras, const int margin) {
  if ((TRaster32P)out_ras) {
    float_arr_to_ras_<TPixel32>(reinterpret_cast<const float*>(in_arr),
                                channels, out_ras, margin);
  } else if ((TRaster64P)out_ras) {
    float_arr_to_ras_<TPixel64>(reinterpret_cast<const float*>(in_arr),
                                channels, out_ras, margin);
  } else if ((TRasterFP)out_ras) {
    float_arr_to_ras_<TPixelF>(reinterpret_cast<const float*>(in_arr), channels,
                               out_ras, margin);
  }
}
//--------------------
void ino::ras_to_vec(const TRasterP in_ras, const int channels,
                     std::vector<unsigned char>& out_vec) {
  out_vec.resize(
      in_ras->getLy() * in_ras->getLx() * channels *
      (((TRaster64P)in_ras) ? sizeof(unsigned short) : sizeof(unsigned char)));
  ino::ras_to_arr(in_ras, channels, &out_vec.at(0));
}
void ino::vec_to_ras(std::vector<unsigned char>& in_vec, const int channels,
                     TRasterP out_ras, const int margin  // default is 0
) {
  ino::arr_to_ras(&in_vec.at(0), channels, out_ras, margin);
  in_vec.clear();
}
//--------------------

void ino::ras_to_ref_float_arr(const TRasterP in_ras, float* out_arr,
                               const int refer_mode) {
  if ((TRaster32P)in_ras) {
    ras_to_ref_float_arr_<TPixel32>(in_ras, out_arr, refer_mode);
  } else if ((TRaster64P)in_ras) {
    ras_to_ref_float_arr_<TPixel64>(in_ras, out_arr, refer_mode);
  } else if ((TRasterFP)in_ras) {
    ras_to_ref_float_arr_<TPixelF>(in_ras, out_arr, refer_mode);
  }
}

//--------------------
#if 0   //---
void ino::Lx_to_wrap( TRasterP ras ) {
	/*
	ras->getLx()   : 描画の幅
	ras->getWrap() : データの存在幅
	描画幅よりデータの存在幅の方が大きい場合、
	存在幅位置に置き直し、残りをゼロクリア
	*/
	if ( ras->getWrap() <= ras->getLx() ) { return; }

	const int rowSize  = ras->getLx()   * ras->getPixelSize();
	const int wrapSize = ras->getWrap() * ras->getPixelSize();
	const int restSize = wrapSize - rowSize;
	const UCHAR *rowImg  = ras->getRawData()+rowSize *(ras->getLy()-1);
	      UCHAR *wrapImg = ras->getRawData()+wrapSize*(ras->getLy()-1);
	for (int yy = 0; yy < ras->getLy(); ++yy) {
		::memcpy(wrapImg, rowImg, rowSize);
		::memset(wrapImg+rowSize, 0, restSize); /* 上下にはみ出すとここで落ちる */
		rowImg  -= rowSize;
		wrapImg -= wrapSize;
	}
}
#endif  //---

//------------------------------------------------------------
namespace {
bool enable_sw_ = true;
bool check_sw_  = true;
}  // namespace
bool ino::log_enable_sw(void) {
  if (check_sw_) {
    TFileStatus file(
        // ToonzFolder::getProfileFolder()
        TEnv::getConfigDir() + "fx_ino_no_log.setup");
    if (file.doesExist()) {
      enable_sw_ = false;
    }

    check_sw_ = false;
  }
  return enable_sw_;
}

//------------------------------------------------------------
namespace {
/* より大きな四角エリアにPixel整数値で密着する */
void makeRectCoherent(TRectD& rect, const TPointD& pos) {
  rect -= pos;
  rect.x0 = tfloor(rect.x0); /* ((x)<(int)(x)? (int)(x)-1: (int)(x))*/
  rect.y0 = tfloor(rect.y0);
  rect.x1 = tceil(rect.x1); /* ((int)(x)<(x)? (int)(x)+1: (int)(x))*/
  rect.y1 = tceil(rect.y1);
  rect += pos;
}

inline void to_xyz(double* xyz, double const* bgr) {
  xyz[0] = 0.6069 * bgr[2] + 0.1735 * bgr[1] + 0.2003 * bgr[0];  // X
  xyz[1] = 0.2989 * bgr[2] + 0.5866 * bgr[1] + 0.1145 * bgr[0];  // Y
  xyz[2] = 0.0000 * bgr[2] + 0.0661 * bgr[1] + 1.1162 * bgr[0];  // Z
}

inline void to_bgr(double* bgr, double const* xyz) {
  bgr[0] = +0.0585 * xyz[0] - 0.1187 * xyz[1] + 0.9017 * xyz[2];  // blue
  bgr[1] = -0.9844 * xyz[0] + 1.9985 * xyz[1] - 0.0279 * xyz[2];  // green
  bgr[2] = +1.9104 * xyz[0] - 0.5338 * xyz[1] - 0.2891 * xyz[2];  // red
}

// convert sRGB color space to power space
template <typename T = double>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  // return -std::log(T(1) - std::pow(nonlinear_color, gamma)) / exposure;
  if (nonlinear_color <= T(0)) return T(0);
  return std::pow(nonlinear_color, gamma) / exposure;
}
// convert power space to sRGB color space
template <typename T = double>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  // return std::pow(T(1) - std::exp(-exposure * linear_color), T(1) / gamma);
  if (linear_color <= T(0)) return T(0);
  return std::pow(linear_color * exposure, T(1) / gamma);
}

template <class T = double>
const T& clamp(const T& v, const T& lo, const T& hi) {
  assert(!(hi < lo));
  return (v < lo) ? lo : (hi < v) ? hi : v;
}
}  // namespace
//------------------------------------------------------------

TBlendForeBackRasterFx::TBlendForeBackRasterFx(bool clipping_mask,
                                               bool has_alpha_option)
    : m_opacity(1.0 * ino::param_range())
    , m_clipping_mask(clipping_mask)
    , m_linear(false)
    , m_gamma(2.2)
    , m_gammaAdjust(0.)
    , m_premultiplied(true)
    , m_colorSpaceMode(new TIntEnumParam(Auto, "Auto")) {
  addInputPort("Fore", this->m_up);
  addInputPort("Back", this->m_down);
  bindParam(this, "opacity", this->m_opacity);
  bindParam(this, "clipping_mask", this->m_clipping_mask);
  bindParam(this, "linear", this->m_linear, true, true);  // obsolete
  bindParam(this, "colorSpaceMode", this->m_colorSpaceMode);
  bindParam(this, "gamma", this->m_gamma);
  bindParam(this, "gammaAdjust", this->m_gammaAdjust);
  bindParam(this, "premultiplied", this->m_premultiplied);
  this->m_opacity->setValueRange(0, 1.0 * ino::param_range());
  this->m_gamma->setValueRange(0.2, 5.0);

  this->m_gammaAdjust->setValueRange(-5., 5.);

  m_colorSpaceMode->addItem(Linear, "Linear");
  m_colorSpaceMode->addItem(Nonlinear, "Nonlinear");

  if (has_alpha_option) {
    m_alpha_rendering = TBoolParamP(true);
    bindParam(this, "alpha_rendering", this->m_alpha_rendering);
  }
  enableComputeInFloat(true);

  // version 1: Gamma had been diretory specified
  // version 2: Gamma is computed by rs.m_colorSpaceGamma + gammaAdjust
  setFxVersion(2);
}
//--------------------------------------------

void TBlendForeBackRasterFx::onFxVersionSet() {
  bool useGamma = getFxVersion() == 1;
  if (useGamma) {
    // Automatically update version
    if (m_gamma->getKeyframeCount() == 0 &&
        areAlmostEqual(m_gamma->getDefaultValue(), 2.2)) {
      useGamma = false;
      // call onObsoleteParamLoaded here in case loading the old fx before
      // introducing the linear option
      onObsoleteParamLoaded("linear");
      setFxVersion(2);
    }
  }
  getParams()->getParamVar("gamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("gammaAdjust")->setIsHidden(useGamma);
}

//------------------------------------------------
// This will be called in TFx::loadData when obsolete "linear" value is
// loaded
void TBlendForeBackRasterFx::onObsoleteParamLoaded(
    const std::string& paramName) {
  if (paramName != "linear") return;

  if (m_linear->getValue())
    m_colorSpaceMode->setValue(Linear);
  else
    m_colorSpaceMode->setValue(Nonlinear);
}

//------------------------------------------------------------

bool TBlendForeBackRasterFx::doGetBBox(double frame, TRectD& bBox,
                                       const TRenderSettings& rs) {
  TRectD up_bx;
  const bool up_sw =
      (m_up.isConnected() ? m_up->doGetBBox(frame, up_bx, rs) : false);
  TRectD dn_bx;
  const bool dn_sw =
      (m_down.isConnected() ? m_down->doGetBBox(frame, dn_bx, rs) : false);
  if (up_sw && dn_sw) {
    bBox = up_bx + dn_bx;
    return !bBox.isEmpty();
  } else if (up_sw) {
    bBox = up_bx;
    return true;
  } else if (dn_sw) {
    bBox = dn_bx;
    return true;
  } else {
    bBox = TRectD();
    return false;
  }
}
//------------------------------------------------------------

void TBlendForeBackRasterFx::dryComputeUpAndDown(TRectD& rect, double frame,
                                                 const TRenderSettings& rs,
                                                 bool upComputesWholeTile) {
  const bool up_is   = (this->m_up.isConnected() &&
                      this->m_up.getFx()->getTimeRegion().contains(frame));
  const bool down_is = (this->m_down.isConnected() &&
                        this->m_down.getFx()->getTimeRegion().contains(frame));
  /* ------ 両方とも切断の時処理しない ---------------------- */
  if (!up_is && !down_is) {
    return;
  }
  /* ------ up接続かつdown切断の時 -------------------------- */
  if (up_is && !down_is) {
    this->m_up->dryCompute(rect, frame, rs);
    return;
  }
  /* ------ down接続時 -------------------------------------- */
  if (down_is) {
    this->m_down->dryCompute(rect, frame, rs);
  }
  /* ------ up切断時 ---------------------------------------- */
  if (!up_is) {
    return;
  }

  /* ------ tileのgeometryを計算する ------------------------ */
  TRectD upBBox;

  if (upComputesWholeTile) {
    upBBox = rect;
  } else {
    this->m_up->getBBox(frame, upBBox, rs);
    upBBox *= rect;
    makeRectCoherent(upBBox, rect.getP00());
  }
  if ((upBBox.getLx() > 0.5) && (upBBox.getLy() > 0.5)) {
    this->m_up->dryCompute(upBBox, frame, rs);
  }
}

//------------------------------------------------------------
void TBlendForeBackRasterFx::doCompute(TTile& tile, double frame,
                                       const TRenderSettings& rs) {
  /* ------ 画像生成 ---------------------------------------- */
  TRasterP dn_ras, up_ras;
  this->computeUpAndDown(tile, frame, rs, dn_ras, up_ras);
  if (!up_ras) {
    return;
  }
  // blend on the empty raster if the back port is not active
  if (!dn_ras) {
    dn_ras = tile.getRaster();
  }
  /* ------ 動作パラメータを得る ---------------------------- */
  const double up_opacity =
      this->m_opacity->getValue(frame) / ino::param_range();
  double gamma;
  if (getFxVersion() == 1)
    gamma = this->m_gamma->getValue(frame);
  else {
    gamma = std::max(1., rs.m_colorSpaceGamma + m_gammaAdjust->getValue(frame));
  }

  bool linear_sw = toBeComputedInLinearColorSpace(rs.m_linearColorSpace,
                                                  tile.getRaster()->isLinear());

  /* ------ (app_begin)log記憶 ------------------------------ */
  const bool log_sw = ino::log_enable_sw();

  if (log_sw) {
    std::ostringstream os;
    os << "params"
       << "  up_opacity " << up_opacity << "   dn_tile w " << dn_ras->getLx()
       << "  wrap " << dn_ras->getWrap() << "  h " << dn_ras->getLy()
       << "  pixbits " << ino::pixel_bits(dn_ras) << "   up_tile w "
       << up_ras->getLx() << "  wrap " << up_ras->getWrap() << "  h "
       << up_ras->getLy() << "  pixbits " << ino::pixel_bits(up_ras)
       << "   frame " << frame;
  }
  /* ------ fx処理 ------------------------------------------ */
  try {
    if (dn_ras) {
      dn_ras->lock();
    }
    if (up_ras) {
      up_ras->lock();
    }
    doComputeFx(dn_ras, up_ras, TPoint(), up_opacity,
                gamma / rs.m_colorSpaceGamma, rs.m_colorSpaceGamma, linear_sw);
    // fx_(dn_ras, up_ras, TPoint(), up_opacity,
    // this->m_clipping_mask->getValue(),
    //  this->m_linear->getValue(), gamma, this->m_premultiplied->getValue());
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
  }
  /* ------ error処理 --------------------------------------- */
  catch (std::exception& e) {
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
    if (log_sw) {
      std::string str("exception <");
      str += e.what();
      str += '>';
    }
    throw;
  } catch (...) {
    if (up_ras) {
      up_ras->unlock();
    }
    if (dn_ras) {
      dn_ras->unlock();
    }
    if (log_sw) {
      std::string str("other exception");
    }
    throw;
  }
}

//------------------------------------------------------------
void TBlendForeBackRasterFx::doComputeFx(
    TRasterP& dn_ras_out, const TRasterP& up_ras, const TPoint& pos,
    const double up_opacity, const double gammaDif,
    const double colorSpaceGamma, const bool linear_sw) {
  /* 交差したエリアを処理するようにする、いるのか??? */
  TRect outRect(dn_ras_out->getBounds());
  TRect upRect(up_ras->getBounds() + pos);
  TRect intersection = outRect * upRect;
  if (intersection.isEmpty()) return;

  TRasterP cRout = dn_ras_out->extract(intersection);
  TRect rr       = intersection - pos;
  TRasterP cRup  = up_ras->extract(rr);

  TRaster32P rout32 = cRout, rup32 = cRup;
  TRaster64P rout64 = cRout, rup64 = cRup;
  TRasterFP routF = cRout, rupF = cRup;

  bool premultiplied_sw = this->m_premultiplied->getValue();

  if (rout32 && rup32) {
    if (linear_sw) {
      if (!premultiplied_sw)
        premultiToUnpremulti<TPixel32, UCHAR>(rout32, rup32, colorSpaceGamma);

      linearTmpl<TPixel32, UCHAR>(rout32, rup32, up_opacity, gammaDif);
    }
    // linearAdd<TPixel32, UCHAR>(rout32, rup32, up_opacity, clipping_mask_sw,
    //  gamma, premultiplied_sw);
    else
      nonlinearTmpl<TPixel32, UCHAR>(rout32, rup32, up_opacity);
    // tmpl_<TPixel32, UCHAR>(rout32, rup32, up_opacity, clipping_mask_sw);
  } else if (rout64 && rup64) {
    if (linear_sw) {
      if (!premultiplied_sw)
        premultiToUnpremulti<TPixel64, USHORT>(rout64, rup64, colorSpaceGamma);

      linearTmpl<TPixel64, USHORT>(rout64, rup64, up_opacity, gammaDif);
    } else
      nonlinearTmpl<TPixel64, USHORT>(rout64, rup64, up_opacity);
  } else if (routF && rupF) {
    if (linear_sw) {
      if (!premultiplied_sw)
        premultiToUnpremulti<TPixelF, float>(routF, rupF, colorSpaceGamma);

      linearTmpl<TPixelF, float>(routF, rupF, up_opacity, gammaDif);
    } else
      nonlinearTmpl<TPixelF, float>(routF, rupF, up_opacity);
  } else {
    throw TRopException("unsupported pixel type");
  }
}

//------------------------------------------------------------
template <class T, class Q>
void TBlendForeBackRasterFx::nonlinearTmpl(TRasterPT<T> dn_ras_out,
                                           const TRasterPT<T>& up_ras,
                                           const double up_opacity) {
  bool clipping_mask_sw   = this->m_clipping_mask->getValue();
  bool alpha_rendering_sw = (m_alpha_rendering.getPointer())
                                ? this->m_alpha_rendering->getValue()
                                : true;

  double maxi = static_cast<double>(T::maxChannelValue);  // 255or65535

  assert(dn_ras_out->getSize() == up_ras->getSize());
  assert(dn_ras_out->isLinear() == up_ras->isLinear());

  for (int yy = 0; yy < dn_ras_out->getLy(); ++yy) {
    T* out_pix             = dn_ras_out->pixels(yy);
    const T* const out_end = out_pix + dn_ras_out->getLx();
    const T* up_pix        = up_ras->pixels(yy);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      double upr = static_cast<double>(up_pix->r) / maxi;
      double upg = static_cast<double>(up_pix->g) / maxi;
      double upb = static_cast<double>(up_pix->b) / maxi;
      double upa = static_cast<double>(up_pix->m) / maxi;
      double dnr = static_cast<double>(out_pix->r) / maxi;
      double dng = static_cast<double>(out_pix->g) / maxi;
      double dnb = static_cast<double>(out_pix->b) / maxi;
      double dna = static_cast<double>(out_pix->m) / maxi;
      brendKernel(dnr, dng, dnb, dna, upr, upg, upb, upa,
                  clipping_mask_sw ? up_opacity * dna : up_opacity,
                  alpha_rendering_sw, true);
      out_pix->r = static_cast<Q>(dnr * (maxi + 0.999999));
      out_pix->g = static_cast<Q>(dng * (maxi + 0.999999));
      out_pix->b = static_cast<Q>(dnb * (maxi + 0.999999));
      out_pix->m = static_cast<Q>(dna * (maxi + 0.999999));
    }
  }
}

//------------------------------------------------------------
template <>
void TBlendForeBackRasterFx::nonlinearTmpl<TPixelF, float>(
    TRasterFP dn_ras_out, const TRasterFP& up_ras, const double up_opacity) {
  bool clipping_mask_sw   = this->m_clipping_mask->getValue();
  bool alpha_rendering_sw = (m_alpha_rendering.getPointer())
                                ? this->m_alpha_rendering->getValue()
                                : true;

  assert(dn_ras_out->getSize() == up_ras->getSize());
  assert(dn_ras_out->isLinear() == up_ras->isLinear());

  for (int yy = 0; yy < dn_ras_out->getLy(); ++yy) {
    TPixelF* out_pix             = dn_ras_out->pixels(yy);
    const TPixelF* const out_end = out_pix + dn_ras_out->getLx();
    const TPixelF* up_pix        = up_ras->pixels(yy);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      double dnr = static_cast<double>(out_pix->r);
      double dng = static_cast<double>(out_pix->g);
      double dnb = static_cast<double>(out_pix->b);
      double dna = static_cast<double>(out_pix->m);
      brendKernel(dnr, dng, dnb, dna, up_pix->r, up_pix->g, up_pix->b,
                  up_pix->m, clipping_mask_sw ? up_opacity * dna : up_opacity,
                  alpha_rendering_sw, false);
      out_pix->r = dnr;
      out_pix->g = dng;
      out_pix->b = dnb;
      out_pix->m = dna;
    }
  }
}

//------------------------------------------------------------
template <class T, class Q>
void TBlendForeBackRasterFx::linearTmpl(TRasterPT<T> dn_ras_out,
                                        const TRasterPT<T>& up_ras,
                                        const double up_opacity,
                                        const double gammaDif) {
  bool clipping_mask_sw   = this->m_clipping_mask->getValue();
  bool alpha_rendering_sw = (m_alpha_rendering.getPointer())
                                ? this->m_alpha_rendering->getValue()
                                : true;
  bool premultiplied_sw   = this->m_premultiplied->getValue();
  double maxi  = static_cast<double>(T::maxChannelValue);  // 255or65535
  double limit = (maxi + 0.5) / (maxi + 1.0);

  assert(dn_ras_out->getSize() == up_ras->getSize());

  for (int yy = 0; yy < dn_ras_out->getLy(); ++yy) {
    T* out_pix             = dn_ras_out->pixels(yy);
    const T* const out_end = out_pix + dn_ras_out->getLx();
    const T* up_pix        = up_ras->pixels(yy);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (up_pix->m <= 0 || up_opacity <= 0) {
        continue;
      }

      double dna         = static_cast<double>(out_pix->m) / maxi;
      double tmp_opacity = clipping_mask_sw ? up_opacity * dna : up_opacity;
      if (tmp_opacity <= 0) continue;

      double dnBGR[3];
      dnBGR[0]        = static_cast<double>(out_pix->b) / maxi;
      dnBGR[1]        = static_cast<double>(out_pix->g) / maxi;
      dnBGR[2]        = static_cast<double>(out_pix->r) / maxi;
      double dnXYZ[3] = {0.0, 0.0, 0.0};
      if (dna > 0.0) {
        for (int c = 0; c < 3; c++) {
          if (premultiplied_sw)
            dnBGR[c] =
                to_linear_color_space(dnBGR[c] / dna, 1.0, gammaDif) * dna;
          else
            dnBGR[c] = to_linear_color_space(dnBGR[c], 1.0, gammaDif);
        }

        to_xyz(dnXYZ, dnBGR);
      }

      double upBGR[3];
      upBGR[0]   = static_cast<double>(up_pix->b) / maxi;
      upBGR[1]   = static_cast<double>(up_pix->g) / maxi;
      upBGR[2]   = static_cast<double>(up_pix->r) / maxi;
      double upa = static_cast<double>(up_pix->m) / maxi;

      for (int c = 0; c < 3; c++) {
        if (premultiplied_sw)
          upBGR[c] = to_linear_color_space(upBGR[c] / upa, 1.0, gammaDif) * upa;
        else
          upBGR[c] = to_linear_color_space(upBGR[c], 1.0, gammaDif);
      }

      double upXYZ[3];
      to_xyz(upXYZ, upBGR);

      brendKernel(dnXYZ[0], dnXYZ[1], dnXYZ[2], dna, upXYZ[0], upXYZ[1],
                  upXYZ[2], upa, tmp_opacity, alpha_rendering_sw, false);

      to_bgr(dnBGR, dnXYZ);

      // premultiply the result
      double nonlinear_b =
          to_nonlinear_color_space(dnBGR[0] / dna, 1.0, gammaDif) * dna;
      double nonlinear_g =
          to_nonlinear_color_space(dnBGR[1] / dna, 1.0, gammaDif) * dna;
      double nonlinear_r =
          to_nonlinear_color_space(dnBGR[2] / dna, 1.0, gammaDif) * dna;

      out_pix->r =
          static_cast<Q>(clamp(nonlinear_r, 0.0, 1.0) * (maxi + 0.999999));
      out_pix->g =
          static_cast<Q>(clamp(nonlinear_g, 0.0, 1.0) * (maxi + 0.999999));
      out_pix->b =
          static_cast<Q>(clamp(nonlinear_b, 0.0, 1.0) * (maxi + 0.999999));
      out_pix->m = static_cast<Q>(dna * (maxi + 0.999999));
    }
  }
}

//------------------------------------------------------------

template <>
void TBlendForeBackRasterFx::linearTmpl<TPixelF, float>(TRasterFP dn_ras_out,
                                                        const TRasterFP& up_ras,
                                                        const double up_opacity,
                                                        const double gammaDif) {
  bool clipping_mask_sw   = this->m_clipping_mask->getValue();
  bool alpha_rendering_sw = (m_alpha_rendering.getPointer())
                                ? this->m_alpha_rendering->getValue()
                                : true;
  bool premultiplied_sw   = this->m_premultiplied->getValue();
  // double maxi = static_cast<double>(T::maxChannelValue);  // 255or65535
  // double limit = (maxi + 0.5) / (maxi + 1.0);

  assert(dn_ras_out->getSize() == up_ras->getSize());

  for (int yy = 0; yy < dn_ras_out->getLy(); ++yy) {
    TPixelF* out_pix             = dn_ras_out->pixels(yy);
    const TPixelF* const out_end = out_pix + dn_ras_out->getLx();
    const TPixelF* up_pix        = up_ras->pixels(yy);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (up_pix->m <= 0.f || up_opacity <= 0.f) {
        continue;
      }

      double dna         = static_cast<double>(out_pix->m);
      double tmp_opacity = clipping_mask_sw ? up_opacity * dna : up_opacity;
      if (tmp_opacity <= 0.) continue;

      double dnBGR[3];
      dnBGR[0]        = static_cast<double>(out_pix->b);
      dnBGR[1]        = static_cast<double>(out_pix->g);
      dnBGR[2]        = static_cast<double>(out_pix->r);
      double dnXYZ[3] = {0.0, 0.0, 0.0};
      if (dna > 0.0) {
        for (int c = 0; c < 3; c++) {
          if (premultiplied_sw)
            dnBGR[c] =
                to_linear_color_space(dnBGR[c] / dna, 1.0, gammaDif) * dna;
          else
            dnBGR[c] = to_linear_color_space(dnBGR[c], 1.0, gammaDif);
        }
        to_xyz(dnXYZ, dnBGR);
      }

      double upBGR[3];
      upBGR[0]   = static_cast<double>(up_pix->b);
      upBGR[1]   = static_cast<double>(up_pix->g);
      upBGR[2]   = static_cast<double>(up_pix->r);
      double upa = static_cast<double>(up_pix->m);

      for (int c = 0; c < 3; c++) {
        if (premultiplied_sw)
          upBGR[c] = to_linear_color_space(upBGR[c] / upa, 1.0, gammaDif) * upa;
        else
          upBGR[c] = to_linear_color_space(upBGR[c], 1.0, gammaDif);
      }

      double upXYZ[3];
      to_xyz(upXYZ, upBGR);

      brendKernel(dnXYZ[0], dnXYZ[1], dnXYZ[2], dna, upXYZ[0], upXYZ[1],
                  upXYZ[2], upa, tmp_opacity, alpha_rendering_sw, false);

      to_bgr(dnBGR, dnXYZ);

      // premultiply the result
      double nonlinear_b =
          to_nonlinear_color_space(dnBGR[0] / dna, 1.0, gammaDif) * dna;
      double nonlinear_g =
          to_nonlinear_color_space(dnBGR[1] / dna, 1.0, gammaDif) * dna;
      double nonlinear_r =
          to_nonlinear_color_space(dnBGR[2] / dna, 1.0, gammaDif) * dna;

      out_pix->r = nonlinear_r;
      out_pix->g = nonlinear_g;
      out_pix->b = nonlinear_b;
      out_pix->m = dna;
    }
  }
}

//------------------------------------------------------------
template <class T, class Q>
void TBlendForeBackRasterFx::premultiToUnpremulti(
    TRasterPT<T> dn_ras, const TRasterPT<T>& up_ras,
    const double colorSpaceGamma) {
  double maxi = static_cast<double>(T::maxChannelValue);  // 255or65535

  assert(dn_ras->getSize() == up_ras->getSize());
  assert(dn_ras->isLinear() == up_ras->isLinear());

  for (int yy = 0; yy < dn_ras->getLy(); ++yy) {
    T* dn_pix             = dn_ras->pixels(yy);
    const T* const dn_end = dn_pix + dn_ras->getLx();
    T* up_pix             = up_ras->pixels(yy);
    for (; dn_pix < dn_end; ++dn_pix, ++up_pix) {
      double upa = static_cast<double>(up_pix->m) / maxi;
      if (upa > 0. && upa < 1.) {
        double upr    = static_cast<double>(up_pix->r) / maxi;
        double upg    = static_cast<double>(up_pix->g) / maxi;
        double upb    = static_cast<double>(up_pix->b) / maxi;
        double up_fac = std::pow(upa, colorSpaceGamma - 1.);
        up_pix->r     = static_cast<Q>(upr * up_fac * (maxi + 0.999999));
        up_pix->g     = static_cast<Q>(upg * up_fac * (maxi + 0.999999));
        up_pix->b     = static_cast<Q>(upb * up_fac * (maxi + 0.999999));
      }
      double dna = static_cast<double>(dn_pix->m) / maxi;
      if (dna > 0. && dna < 1.) {
        double dnr    = static_cast<double>(dn_pix->r) / maxi;
        double dng    = static_cast<double>(dn_pix->g) / maxi;
        double dnb    = static_cast<double>(dn_pix->b) / maxi;
        double dn_fac = std::pow(dna, colorSpaceGamma - 1.);
        dn_pix->r     = static_cast<Q>(dnr * dn_fac * (maxi + 0.999999));
        dn_pix->g     = static_cast<Q>(dng * dn_fac * (maxi + 0.999999));
        dn_pix->b     = static_cast<Q>(dnb * dn_fac * (maxi + 0.999999));
      }
    }
  }
}

//------------------------------------------------------------
template <>
void TBlendForeBackRasterFx::premultiToUnpremulti<TPixelF, float>(
    TRasterFP dn_ras, const TRasterFP& up_ras, const double colorSpaceGamma) {
  assert(dn_ras->getSize() == up_ras->getSize());
  assert(dn_ras->isLinear() == up_ras->isLinear());

  for (int yy = 0; yy < dn_ras->getLy(); ++yy) {
    TPixelF* dn_pix             = dn_ras->pixels(yy);
    const TPixelF* const dn_end = dn_pix + dn_ras->getLx();
    TPixelF* up_pix             = up_ras->pixels(yy);
    for (; dn_pix < dn_end; ++dn_pix, ++up_pix) {
      if (up_pix->m > 0.f && up_pix->m < 1.f) {
        float up_fac =
            std::pow(up_pix->m, static_cast<float>(colorSpaceGamma - 1.));
        up_pix->r *= up_fac;
        up_pix->g *= up_fac;
        up_pix->b *= up_fac;
      }
      if (dn_pix->m > 0.f && dn_pix->m < 1.f) {
        float dn_fac =
            std::pow(dn_pix->m, static_cast<float>(colorSpaceGamma - 1.));
        dn_pix->r *= dn_fac;
        dn_pix->g *= dn_fac;
        dn_pix->b *= dn_fac;
      }
    }
  }
}

//------------------------------------------------------------

void TBlendForeBackRasterFx::computeUpAndDown(TTile& tile, double frame,
                                              const TRenderSettings& rs,
                                              TRasterP& dn_ras,
                                              TRasterP& up_ras,
                                              bool upComputesWholeTile) {
  /* ------ サポートしていないPixelタイプはエラーを投げる --- */
  if (!((TRaster32P)tile.getRaster()) && !((TRaster64P)tile.getRaster()) &&
      !((TRasterFP)tile.getRaster())) {
    throw TRopException("unsupported input pixel type");
  }
  /*
m_down,m_upは繋がっている方があればそれを表示する
両方とも接続していれば合成処理する
表示スイッチを切ってあるならm_upを表示する
fxをreplaceすると、
  m_source   --> m_up  (=port0)
  m_reference --> m_down(=port1)
となる
*/
  const bool up_is   = (this->m_up.isConnected() &&
                      this->m_up.getFx()->getTimeRegion().contains(frame));
  const bool down_is = (this->m_down.isConnected() &&
                        this->m_down.getFx()->getTimeRegion().contains(frame));
  /* ------ 両方とも切断の時処理しない ---------------------- */
  if (!up_is && !down_is) {
    tile.getRaster()->clear();
    return;
  }
  /* ------ up接続かつdown切断の時 -------------------------- */
  if (up_is && !down_is) {
    TTile upTile;
    this->m_up->allocateAndCompute(upTile, tile.m_pos,
                                   tile.getRaster()->getSize(),
                                   tile.getRaster(), frame, rs);
    up_ras = upTile.getRaster();
    return;
  }
  /* ------ down接続時 downのみ描画して... ------------------ */
  if (down_is) {
    this->m_down->compute(tile, frame, rs);
  }
  /* ------ up切断時 ---------------------------------------- */
  if (!up_is) {
    return;
  }

  /* upと重なる部分を描画する */

  /* ------ tileの範囲 -------------------------------------- */
  const TDimension tsz(tile.getRaster()->getSize()); /* 整数 */
  const TRectD tileRect(tile.m_pos, TDimensionD(tsz.lx, tsz.ly));
  TRectD upBBox;
  if (upComputesWholeTile) {
    upBBox = tileRect;
  }      /* tile全体を得る */
  else { /* 厳密なエリア... */
    this->m_up->getBBox(frame, upBBox, rs);
    upBBox *= tileRect; /* upとtileの交差エリア */

    /* より大きな四角エリアにPixel整数値で密着する */
    makeRectCoherent(upBBox, tile.m_pos);  // double-->int grid
  }

  TDimensionI upSize(                        /* TRectDをTDimensionIに変換 */
                     tround(upBBox.getLx())  // getLx() = "x1>=x0?x1-x0:0"
                     ,
                     tround(upBBox.getLy())  // getLy() = "y1>=y0?y1-y0:0"
  );
  if ((upSize.lx <= 0) || (upSize.ly <= 0)) {
    return;
  }

  /* ------ upのメモリ確保と描画 ---------------------------- */
  TTile upTile;
  this->m_up->allocateAndCompute(upTile, upBBox.getP00(), upSize,
                                 tile.getRaster() /* 32/64bitsの判定に使う */
                                 ,
                                 frame, rs);
  /* ------ upとdownのTRasterを得る ------------------------- */
  TRectI dnRect(upTile.getRaster()->getSize());  // TDimensionI(-)

  dnRect += convert(upTile.m_pos - tile.m_pos); /* uptile->tile原点 */
  /*
  ここで問題はdoubleの位置を、四捨五入して整数値にしていること
  移動してから四捨五入ではないの???
  dnRectの元位置が整数位置なので、問題ないか...
  */

  dn_ras = upComputesWholeTile ? tile.getRaster()
                               : tile.getRaster()->extract(dnRect);
  up_ras = upTile.getRaster();
  assert(dn_ras->getSize() == up_ras->getSize());
}

//------------------------------------------------------------

bool TBlendForeBackRasterFx::toBeComputedInLinearColorSpace(
    bool settingsIsLinear, bool tileIsLinear) const {
  ColorSpaceMode mode =
      static_cast<ColorSpaceMode>(m_colorSpaceMode->getValue());
  return mode == Linear || (mode == Auto && settingsIsLinear);
}
