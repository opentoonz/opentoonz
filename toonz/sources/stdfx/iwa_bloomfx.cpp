#include "iwa_bloomfx.h"

#include "tparamuiconcept.h"

#include <QVector>
#include <QPair>

namespace {
// convert sRGB color space to power space
template <typename T = double>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  if (nonlinear_color <= T(0)) return T(0);
  return std::pow(nonlinear_color, gamma) / exposure;
}
// convert power space to sRGB color space
template <typename T = double>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  if (linear_color <= T(0)) return T(0);
  return std::pow(linear_color * exposure, T(1) / gamma);
}
template <class T = double>
const T &clamp(const T &v, const T &lo, const T &hi) {
  assert(!(hi < lo));
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

void blurByRotate(cv::Mat &mat) {
  double angle = 45.0;

  int size   = std::ceil(std::sqrt(mat.cols * mat.cols + mat.rows * mat.rows));
  int width  = ((size - mat.cols) % 2 == 0) ? size : size + 1;
  int height = ((size - mat.rows) % 2 == 0) ? size : size + 1;

  cv::Point2f center((mat.cols - 1) / 2.0, (mat.rows - 1) / 2.0);

  cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
  rot.at<double>(0, 2) += (width - mat.cols) / 2.0;
  rot.at<double>(1, 2) += (height - mat.rows) / 2.0;

  cv::Mat tmp;
  cv::warpAffine(mat, tmp, rot, cv::Size(width, height));

  center = cv::Point2f((width - 1) / 2.0, (height - 1) / 2.0);
  rot    = cv::getRotationMatrix2D(center, -angle, 1.0);
  rot.at<double>(0, 2) += (mat.cols - width) / 2.0;
  rot.at<double>(1, 2) += (mat.rows - height) / 2.0;

  cv::warpAffine(tmp, mat, rot, mat.size());
}

}  // namespace

//--------------------------------------------
// Iwa_BloomFx
//--------------------------------------------
Iwa_BloomFx::Iwa_BloomFx()
    : m_gamma(2.2)
    , m_gammaAdjust(0.)
    , m_auto_gain(false)
    , m_gain_adjust(0.0)
    , m_gain(2.0)
    , m_decay(1)
    , m_size(100.0)
    , m_alpha_rendering(false)
    , m_alpha_mode(new TIntEnumParam(NoAlpha, "No Alpha")) {
  addInputPort("Source", m_source);
  bindParam(this, "gamma", m_gamma);
  bindParam(this, "gammaAdjust", m_gammaAdjust);
  bindParam(this, "auto_gain", m_auto_gain);
  bindParam(this, "gain_adjust", m_gain_adjust);
  bindParam(this, "gain", m_gain);
  bindParam(this, "decay", m_decay);
  bindParam(this, "size", m_size);
  bindParam(this, "alpha_mode", m_alpha_mode);
  bindParam(this, "alpha_rendering", m_alpha_rendering, false,
            true);  // obsolete

  m_alpha_mode->addItem(Light, "Light");
  m_alpha_mode->addItem(LightAndSource, "Light and Source");

  m_gamma->setValueRange(0.1, 5.0);
  m_gammaAdjust->setValueRange(-5., 5.);
  m_gain_adjust->setValueRange(-1.0, 1.0);
  m_gain->setValueRange(0.1, 10.0);
  m_decay->setValueRange(0, 4);
  m_size->setValueRange(0.1, 1024.0);

  m_size->setMeasureName("fxLength");

  enableComputeInFloat(true);

  // Version 1 : gaussian filter applied with standard deviation 0
  // Version 2 : standard deviation = blurRadius * 0.3
  // Version 3: Gamma is computed by rs.m_colorSpaceGamma + gammaAdjust
  setFxVersion(3);
}
//------------------------------------------------

double Iwa_BloomFx::getSizePixelAmount(const double val, const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect;
  vect.x = val;
  vect.y = 0.0;
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return sqrt(vect.x * vect.x + vect.y * vect.y);
}
//------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_BloomFx::setSourceTileToMat(const RASTER ras, cv::Mat &imgMat,
                                     const double gamma) {
  double maxi = static_cast<double>(PIXEL::maxChannelValue);  // 255or65535or1.0
  for (int j = 0; j < ras->getLy(); j++) {
    const PIXEL *pix = ras->pixels(j);
    cv::Vec3f *mat_p = imgMat.ptr<cv::Vec3f>(j);
    for (int i = 0; i < ras->getLx(); i++, pix++, mat_p++) {
      double pix_a = static_cast<double>(pix->m) / maxi;
      if (pix_a <= 0.0) {
        *mat_p = cv::Vec3f(0, 0, 0);
        continue;
      }
      double bgra[3];
      if (areAlmostEqual(gamma, 1.0)) {
        bgra[0] = static_cast<double>(pix->b) / maxi;
        bgra[1] = static_cast<double>(pix->g) / maxi;
        bgra[2] = static_cast<double>(pix->r) / maxi;
      } else {
        bgra[0] = static_cast<double>(pix->b) / maxi;
        bgra[1] = static_cast<double>(pix->g) / maxi;
        bgra[2] = static_cast<double>(pix->r) / maxi;
        for (int c = 0; c < 3; c++) {
          // assuming that the source image is premultiplied
          bgra[c] = to_linear_color_space(bgra[c] / pix_a, 1.0, gamma) * pix_a;
        }
      }
      *mat_p = cv::Vec3f(bgra[0], bgra[1], bgra[2]);
    }
  }
}
//------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_BloomFx::setMatToOutput(const RASTER ras, const RASTER srcRas,
                                 cv::Mat &imgMat, const double gamma,
                                 const double gain, const AlphaMode alphaMode,
                                 const int margin) {
  double maxi = static_cast<double>(PIXEL::maxChannelValue);  // 255or65535
  for (int j = 0; j < ras->getLy(); j++) {
    cv::Vec3f const *mat_p = imgMat.ptr<cv::Vec3f>(j);
    PIXEL *pix             = ras->pixels(j);
    PIXEL *srcPix          = srcRas->pixels(j + margin) + margin;

    for (int i = 0; i < ras->getLx(); i++, pix++, srcPix++, mat_p++) {
      double nonlinear_b, nonlinear_g, nonlinear_r;
      if (areAlmostEqual(gamma, 1.)) {
        nonlinear_b = (double)(*mat_p)[0] * gain;
        nonlinear_g = (double)(*mat_p)[1] * gain;
        nonlinear_r = (double)(*mat_p)[2] * gain;
      } else {
        nonlinear_b =
            to_nonlinear_color_space((double)(*mat_p)[0] * gain, 1.0, gamma);
        nonlinear_g =
            to_nonlinear_color_space((double)(*mat_p)[1] * gain, 1.0, gamma);
        nonlinear_r =
            to_nonlinear_color_space((double)(*mat_p)[2] * gain, 1.0, gamma);
      }

      nonlinear_b = clamp(nonlinear_b, 0.0, 1.0);
      nonlinear_g = clamp(nonlinear_g, 0.0, 1.0);
      nonlinear_r = clamp(nonlinear_r, 0.0, 1.0);

      pix->r = (typename PIXEL::Channel)(nonlinear_r * (maxi + 0.999999));
      pix->g = (typename PIXEL::Channel)(nonlinear_g * (maxi + 0.999999));
      pix->b = (typename PIXEL::Channel)(nonlinear_b * (maxi + 0.999999));
      if (alphaMode == NoAlpha)
        pix->m = (typename PIXEL::Channel)(PIXEL::maxChannelValue);
      else {
        double chan_a =
            std::max(std::max(nonlinear_b, nonlinear_g), nonlinear_r);
        if (alphaMode == Light)
          pix->m = (typename PIXEL::Channel)(chan_a * (maxi + 0.999999));
        else  // alphaMode == LightAndSource
          pix->m = std::max(
              (typename PIXEL::Channel)(chan_a * (maxi + 0.999999)), srcPix->m);
      }
    }
  }
}

template <>
void Iwa_BloomFx::setMatToOutput<TRasterFP, TPixelF>(
    const TRasterFP ras, const TRasterFP srcRas, cv::Mat &imgMat,
    const double gamma, const double gain, const AlphaMode alphaMode,
    const int margin) {
  for (int j = 0; j < ras->getLy(); j++) {
    cv::Vec3f const *mat_p = imgMat.ptr<cv::Vec3f>(j);
    TPixelF *pix           = ras->pixels(j);
    TPixelF *srcPix        = srcRas->pixels(j + margin) + margin;

    for (int i = 0; i < ras->getLx(); i++, pix++, srcPix++, mat_p++) {
      if (areAlmostEqual(gamma, 1.)) {
        pix->b = (*mat_p)[0] * (float)gain;
        pix->g = (*mat_p)[1] * (float)gain;
        pix->r = (*mat_p)[2] * (float)gain;
      } else {
        pix->b = to_nonlinear_color_space((*mat_p)[0] * (float)gain, 1.f,
                                          (float)gamma);
        pix->g = to_nonlinear_color_space((*mat_p)[1] * (float)gain, 1.f,
                                          (float)gamma);
        pix->r = to_nonlinear_color_space((*mat_p)[2] * (float)gain, 1.f,
                                          (float)gamma);
      }

      if (alphaMode == NoAlpha)
        pix->m = 1.f;
      else {
        float chan_a = std::max(std::max(pix->b, pix->g), pix->r);
        if (alphaMode == Light)
          pix->m = chan_a;
        else  // alphaMode == LightAndSource
          pix->m = std::max(chan_a, srcPix->m);
      }
    }
  }
}

//------------------------------------------------

double Iwa_BloomFx::computeAutoGain(cv::Mat &imgMat) {
  double maxChanelValue = 0.0;
  for (int j = 0; j < imgMat.size().height; j++) {
    cv::Vec3f const *mat_p = imgMat.ptr<cv::Vec3f>(j);
    for (int i = 0; i < imgMat.size().width; i++, mat_p++) {
      for (int c = 0; c < 3; c++)
        maxChanelValue = std::max(maxChanelValue, (double)(*mat_p)[c]);
    }
  }

  if (maxChanelValue == 0.0) return 1.0;

  return 1.0 / maxChanelValue;
}

//------------------------------------------------

void Iwa_BloomFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &settings) {
  // If the source is not connected, then do nothing
  if (!m_source.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  // obtain parameters
  double gamma;
  if (getFxVersion() <= 2)
    gamma = m_gamma->getValue(frame);
  else
    gamma = std::max(
        1., settings.m_colorSpaceGamma + m_gammaAdjust->getValue(frame));
  double adjustGamma = gamma;
  if (tile.getRaster()->isLinear()) gamma /= settings.m_colorSpaceGamma;

  bool autoGain = m_auto_gain->getValue();
  double gainAdjust =
      (autoGain) ? std::pow(10.0, m_gain_adjust->getValue(frame)) : 1.0;
  double gain    = (autoGain) ? 1.0 : m_gain->getValue(frame);
  int blurRadius = (int)std::round(m_decay->getValue(frame));
  double size = getSizePixelAmount(m_size->getValue(frame), settings.m_affine);
  AlphaMode alphaMode = (AlphaMode)m_alpha_mode->getValue();

  int margin = static_cast<int>(std::ceil(size));
  TRectD _rect(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                       tile.getRaster()->getLy()));
  _rect = _rect.enlarge(static_cast<double>(margin));
  TDimensionI dimSrc(static_cast<int>(_rect.getLx() + 0.5),
                     static_cast<int>(_rect.getLy() + 0.5));

  // obtain the source tile
  TTile sourceTile;
  m_source->allocateAndCompute(sourceTile, _rect.getP00(), dimSrc,
                               tile.getRaster(), frame, settings);

  // set the source image to cvMat, converting to linear color space
  cv::Mat imgMat(cv::Size(dimSrc.lx, dimSrc.ly), CV_32FC3);
  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  TRasterFP rasF   = tile.getRaster();
  if (ras32)
    setSourceTileToMat<TRaster32P, TPixel32>(sourceTile.getRaster(), imgMat,
                                             gamma);
  else if (ras64)
    setSourceTileToMat<TRaster64P, TPixel64>(sourceTile.getRaster(), imgMat,
                                             gamma);
  else if (rasF)
    setSourceTileToMat<TRasterFP, TPixelF>(sourceTile.getRaster(), imgMat,
                                           gamma);

  // compute size and intensity ratios of resampled layers
  // resample size is reduced from the specified size, taking into account
  // that the gaussian blur (x 2) and the blur by rotation resampling (x sqrt2)
  double blurScale    = 1.0 + (double)blurRadius;
  double no_blur_size = size / (blurScale * 1.5);
  // find the minimum "power of 2" value which is the same as or larger than the
  // filter size
  int level         = 1;
  double power_of_2 = 1.0;
  while (1) {
    if (power_of_2 >= no_blur_size) break;
    level++;
    power_of_2 *= 2;
  }
  // store the size of resampled layers
  QVector<cv::Size> sizes;
  double tmp_filterSize = no_blur_size;
  double width          = static_cast<double>(imgMat.size().width);
  double height         = static_cast<double>(imgMat.size().height);
  for (int lvl = 0; lvl < level - 1; lvl++) {
    int tmp_w = static_cast<int>(std::ceil(width / tmp_filterSize));
    int tmp_h = static_cast<int>(std::ceil(height / tmp_filterSize));
    sizes.push_front(cv::Size(tmp_w, tmp_h));
    tmp_filterSize *= 0.5;
  }
  sizes.push_front(imgMat.size());

  // the filter is based on the nearest power-of-2 sized one with an adjustment
  // reducing the sizes and increasing the intensity with this ratio
  double ratio = power_of_2 / no_blur_size;
  // base filter sizes will be 1, 2, 4, ... 2^(level-1)
  // intensity of the filter with sizes > 2
  double intensity_all = power_of_2 / (power_of_2 * 2.0 - 1.0);
  // intensity of the filter with size 1, so that the amount of the filter at
  // the center point is always 1.0
  double intensity_front = 1.0 - (1.0 - intensity_all) * ratio;

  std::vector<cv::Mat> dst(level);
  cv::Size const ksize(1 + blurRadius * 2, 1 + blurRadius * 2);
  // standard deviation
  double sdRatio = (getFxVersion() == 1) ? 0.0 : 0.3;

  cv::Mat tmp;
  int i;
  // for each level of filter (from larger to smaller)
  for (i = 0; i < level;) {
    // scaling down the size
    if (i) {
      cv::resize(imgMat, tmp, sizes[i], 0.0, 0.0, cv::INTER_AREA);
      imgMat = tmp;
    }
    // gaussian blur
    if (blurRadius == 0)
      dst[i] = imgMat;
    else
      cv::GaussianBlur(imgMat, dst[i], ksize, (double)blurRadius * sdRatio);
    ++i;
  }
  // for each level of filter (from smaller to larger)
  for (--i; i > 0; --i) {
    // scaling up the size
    cv::resize(dst[i], tmp, dst[i - 1].size());
    // blur by rotational resampling in order to reduce box-shaped artifact
    blurByRotate(tmp);
    // add to the upper resampled image
    if (i > 1)
      dst[i - 1] += tmp;
    else
      imgMat = dst[0] * intensity_front + tmp * intensity_all;
  }

  // get the subimage without margin
  cv::Rect roi(cv::Point(margin, margin),
               cv::Size(tile.getRaster()->getLx(), tile.getRaster()->getLy()));
  imgMat = imgMat(roi);

  if (autoGain) {
    gain = to_linear_color_space(gainAdjust, 1.0, adjustGamma) *
           computeAutoGain(imgMat);
  }

  // set the result to the tile, converting to rgb channel values
  if (ras32)
    setMatToOutput<TRaster32P, TPixel32>(tile.getRaster(),
                                         sourceTile.getRaster(), imgMat, gamma,
                                         gain, alphaMode, margin);
  else if (ras64)
    setMatToOutput<TRaster64P, TPixel64>(tile.getRaster(),
                                         sourceTile.getRaster(), imgMat, gamma,
                                         gain, alphaMode, margin);
  else if (rasF)
    setMatToOutput<TRasterFP, TPixelF>(tile.getRaster(), sourceTile.getRaster(),
                                       imgMat, gamma, gain, alphaMode, margin);
}
//------------------------------------------------

bool Iwa_BloomFx::doGetBBox(double frame, TRectD &bBox,
                            const TRenderSettings &info) {
  if (!m_source.isConnected()) {
    bBox = TRectD();
    return false;
  }
  bool ret   = m_source->doGetBBox(frame, bBox, info);
  int margin = static_cast<int>(
      std::ceil(getSizePixelAmount(m_size->getValue(frame), info.m_affine)));
  if (margin > 0) {
    bBox = bBox.enlarge(static_cast<double>(margin));
  }
  return ret;
}
//------------------------------------------------

bool Iwa_BloomFx::canHandle(const TRenderSettings &info, double frame) {
  return false;
}
//------------------------------------------------

void Iwa_BloomFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 1];

  concepts[0].m_type  = TParamUIConcept::RADIUS;
  concepts[0].m_label = "Size";
  concepts[0].m_params.push_back(m_size);
}
//------------------------------------------------
// This will be called in TFx::loadData when obsolete "alpha rendering" value is
// loaded
void Iwa_BloomFx::onObsoleteParamLoaded(const std::string &paramName) {
  if (paramName != "alpha_rendering") return;

  // this condition is to prevent overwriting the alpha mode parameter
  // when both "alpha rendering" and "alpha mode" are saved in the scene
  // due to the previous bug
  if (m_alpha_mode->getValue() != NoAlpha) return;

  if (m_alpha_rendering->getValue())
    m_alpha_mode->setValue(LightAndSource);
  else
    m_alpha_mode->setValue(NoAlpha);
}
//------------------------------------------------

void Iwa_BloomFx::onFxVersionSet() {
  bool useGamma = getFxVersion() <= 2;
  if (getFxVersion() == 2) {
    // Automatically update version
    if (m_gamma->getKeyframeCount() == 0 &&
        areAlmostEqual(m_gamma->getDefaultValue(), 2.2)) {
      useGamma = false;
      setFxVersion(3);
    }
  }
  getParams()->getParamVar("gamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("gammaAdjust")->setIsHidden(useGamma);
}

//------------------------------------------------

bool Iwa_BloomFx::toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                                 bool tileIsLinear) const {
  // made this effect to compute always in nonlinear
  return false;
  // return tileIsLinear;
}

//------------------------------------------------
FX_PLUGIN_IDENTIFIER(Iwa_BloomFx, "iwa_BloomFx")