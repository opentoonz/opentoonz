

// Toonz includes
#include "tpixelutils.h"
#include "tpalette.h"
#include "tcolorstyles.h"
#include "timage_io.h"
#include "tropcm.h"
#include "ttile.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "autoadjust.h"
#include "autopos.h"
#include "cleanuppalette.h"
#include "cleanupcommon.h"
#include "tmsgcore.h"
#include "toonz/cleanupparameters.h"

#include "toonz/tcleanupper.h"

using namespace CleanupTypes;

/*  The Cleanup Process Reworked   -   EXPLANATION (by Daniele)

INTRODUCTION:

  The purpose of a Cleanup Process is hereby intended as the task of
transforming
  a fullcolor image (any bpp, matte supported*) into a TRasterCM32 -
  that is, a colormap image - given an externally specified palette of ink
colors to
  be recognized. No paint color is assumed at this stage.

  Typically, artists draw outlines using a black or dark color, whereas
different hues are
  used to mark lines that denote shadows or lights on characters.

  Additional processing steps include the ability to recognize and counter any
linear
  transformation in the image which is 'signaled' by the presence of (black)
pegbar holes,
  so that the countering linear transformation maps those peg holes in the usual
centered
  horizontal fashion.

  Post-processing include despeckling (ie removal or recoloration of little
blots with
  uniform color), and tones' brigthness/contrast manipulation.

  (*) The image is first overed on top of a white background


CONSTRAINTS:

  We assume the following constraints throughout the process:

  - Palette colors:

    * Color 0 represents the PAPER color, and will just substitute it in the
colormap.

    * Colors with index >= 2 are MATCH-LINE colors, and their HUE ONLY (the H in
HSV coordinates)
    is essential to line recognition. The hue of image pixels is compared to
that of each
    matchline color - and the nearest matchline color is associated to that
pixel.
    If that associated matchline color is still too 'hue-distant' from the pixel
color
    (beyond a user-specified parameter), the pixel is ignored (ie associated to
paper).
    Furthermore, each matchline color also has a parameter corresponding to a
saturation
    threshold; pixels whose color's saturation is below the threshold specified
by the
    associated color are reassociated to the PAPER color.

    * Color 1 represents the OUTLINE color, and its VALUE (the V in HSV
coordinates) is
    assumed to be the image's lowest. Its H and S components are unused.
    Pixels whose value is below this value + a user-defined threshold parameter
are
    'outline-PRONE' pixels (even matchline-associated pixels can be).
    They are assumed to be full outline pixels if their CHROMA (S*V) is above a
    'Color threshold'. This condition lets the user settle how outline/matchline
    disputed pixels should be considered.

  - The Colormap tone for a pixel is extracted according to these rules:

    * Paper pixels are completely transparent (tone = 255).

    * Undisputed matchline colors build the tone upon the pixel's Saturation,
scaled
      so that 1.0 maps to 0, and the saturation threshold for that matchline
      color maps to 255.

    * Undisputed Outline colors do similarly, with the Value.

    * Disputed outline/matchline colors result in the blend PRODUCT of the above
tones.
      This makes the tone smoother on outline/matchline intersections.
*/

//**************************************************************************************
//    Local namespace stuff
//**************************************************************************************

namespace {

// some useful functions for doing math

inline double affMV1(const TAffine &aff, double v1, double v2) {
  return aff.a11 * v1 + aff.a12 * v2 + aff.a13;
}

//-------------------------------------------------------------------------

inline double affMV2(const TAffine &aff, double v1, double v2) {
  return aff.a21 * v1 + aff.a22 * v2 + aff.a23;
}

//=========================================================================

//! Auxiliary class for HSV (toonz 4.x style)
struct HSVColor {
  double m_h;
  double m_s;
  double m_v;

public:
  HSVColor(double h = 0, double s = 0, double v = 0) : m_h(h), m_s(s), m_v(v) {}

  static HSVColor fromRGB(double r, double g, double b);
};

//-------------------------------------------------------------------------

HSVColor HSVColor::fromRGB(double r, double g, double b) {
  double h, s, v;
  double max, min, delta;

  max = std::max({r, g, b});
  min = std::min({r, g, b});

  v = max;

  if (max != 0)
    s = (max - min) / max;
  else
    s = 0;

  if (s == 0)
    h = 0;
  else {
    delta = max - min;

    if (r == max)
      h = (g - b) / delta;
    else if (g == max)
      h = 2.0 + (b - r) / delta;
    else if (b == max)
      h = 4.0 + (r - g) / delta;
    h   = h * 60.0;
    if (h < 0) h += 360.0;
  }

  return HSVColor(h, s, v);
}

//=========================================================================

//! Precomputation data about target colors
struct TargetColorData {
  int m_idx;                 //!< Palette color index
  HSVColor m_hsv;            //!< HSV coordinates of the color
  double m_saturationLower;  //!< Pixel colors associated with this color must
                             //! be above this
  double m_hueLower, m_hueUpper;  //!< Pixel colors associated with this color
                                  //! must in this range

public:
  TargetColorData(const TargetColor &color)
      : m_idx(-1)
      , m_hsv(HSVColor::fromRGB(color.m_color.r / 255.0,
                                color.m_color.g / 255.0,
                                color.m_color.b / 255.0))
      , m_saturationLower(1.0 - color.m_threshold / 100.0)
      , m_hueLower(m_hsv.m_h - color.m_hRange * 0.5)
      , m_hueUpper(m_hsv.m_h + color.m_hRange * 0.5) {
    if (m_hueLower < 0.0) m_hueLower += 360.0;
    if (m_hueUpper > 360.0) m_hueUpper -= 360.0;
  }
};

//=========================================================================

//! Birghtness/Contrast color transform data

#define MAX_N_PENCILS 8

/* the following must be updated at every change in palette content */
int N_pencils = 4;                      /* not counting autoclose */
TPixelRGBM32 Pencil[MAX_N_PENCILS + 1]; /* last is autoclose pencil */
int Pencil_index[MAX_N_PENCILS + 1];    /* "" */
int Pencil_id[MAX_N_PENCILS + 1];       /* "" */
TPixelRGBM32 Paper = TPixel32::White;

//=========================================================================

//! Birghtness/Contrast color transform structure
class TransfFunction {
  USHORT TransfFun[(MAX_N_PENCILS + 1) << 8];

  void setTransfFun(int pencil, int b1, int c1) {
    int i, p1, p2, brig, cont, max;

    cont = 255 - c1;
    brig = 255 - b1;
    max  = 255;
    notLessThan(1, cont);
    p2 = brig;
    p1 = p2 - cont;

    for (i = 0; i <= p1; i++) TransfFun[pencil << 8 | i] = 0;
    for (; i < p2; i++)
      TransfFun[pencil << 8 | i] = std::min(max, max * (i - p1) / cont);
    for (; i < 256; i++) TransfFun[pencil << 8 | i] = max;
  }

public:
  TransfFunction(const TargetColors &colors) {
    memset(TransfFun, 0, sizeof TransfFun);
    int count = std::min(colors.getColorCount(), MAX_N_PENCILS);
    for (int p = 0; p < count; p++) {
      int brightness = troundp(2.55 * colors.getColor(p).m_brightness);
      int contrast   = troundp(2.55 * colors.getColor(p).m_contrast);
      setTransfFun(p, brightness, contrast);
    }
  }

  USHORT *getTransfFun() { return TransfFun; }
};

//=========================================================================

//! Brightness/Contrast functions

void brightnessContrast(const TRasterCM32P &cm, const TargetColors &colors) {
  TransfFunction transform(colors);
  USHORT *transf_fun = transform.getTransfFun();

  int ink, tone;
  int newTone, newInk;

  for (int y = 0; y < cm->getLy(); ++y) {
    TPixelCM32 *pix    = cm->pixels(y);
    TPixelCM32 *endPix = pix + cm->getLx();

    for (; pix < endPix; ++pix) {
      tone = pix->getTone();
      if (tone < 255) {
        ink     = pix->getInk();
        newTone = transf_fun[ink << 8 | tone];
        newInk  = (newTone == 255) ? 0 : colors.getColor(ink).m_index;

        *pix = TPixelCM32(newInk, 0, newTone);
      }
    }
  }
}

//------------------------------------------------------------------------------------

void brightnessContrastGR8(const TRasterCM32P &cm, const TargetColors &colors) {
  TransfFunction transform(colors);
  USHORT *transf_fun = transform.getTransfFun();

  int val, black = colors.getColor(1).m_index;

  for (int y = 0; y < cm->getLy(); ++y) {
    TPixelCM32 *pix    = cm->pixels(y);
    TPixelCM32 *endPix = pix + cm->getLx();

    for (; pix < endPix; ++pix) {
      val  = transf_fun[pix->getValue() + 256];
      *pix = (val < 255) ? TPixelCM32(black, 0, val) : TPixelCM32();
    }
  }
}

//=========================================================================

//! Transparency check

void transparencyCheck(const TRasterCM32P &cmin, const TRaster32P &rasout) {
  for (int y = 0; y < cmin->getLy(); ++y) {
    TPixelCM32 *pix    = cmin->pixels(y);
    TPixelCM32 *endPix = pix + cmin->getLx();

    TPixel32 *outPix = rasout->pixels(y);
    for (; pix < endPix; ++pix, ++outPix) {
      int ink  = pix->getInk();
      int tone = pix->getTone();

      if (ink == 4095)
        *outPix = TPixel32::Green;
      else
        *outPix = (tone == 0) ? TPixel32::Black
                              : (tone == 255) ? TPixel32::White : TPixel32::Red;
    }
  }
}

}  // namespace

//**************************************************************************************
//    TCleanupper implementation - elementary functions
//**************************************************************************************

TCleanupper *TCleanupper::instance() {
  static TCleanupper theCleanupper;
  return &theCleanupper;
}

//------------------------------------------------------------------------------------

void TCleanupper::setParameters(CleanupParameters *parameters) {
  m_parameters = parameters;
}

//------------------------------------------------------------------------------------

TPalette *TCleanupper::createToonzPaletteFromCleanupPalette() {
  TPalette *cleanupPalette = m_parameters->m_cleanupPalette.getPointer();
  return createToonzPalette(cleanupPalette, 1);
}

//**************************************************************************************
//    CleanupProcessedImage implementation
//**************************************************************************************

TToonzImageP CleanupPreprocessedImage::getImg() const {
  return (TToonzImageP)(TImageCache::instance()->get(m_imgId, true));
}

//-----------------------------------------------------------------------------------

CleanupPreprocessedImage::CleanupPreprocessedImage(
    CleanupParameters *parameters, TToonzImageP processed, bool fromGr8)
    : m_wasFromGR8(fromGr8)
    , m_autocentered(false)
    , m_size(processed->getSize()) {
  if (!processed)
    m_imgId = "";
  else {
    m_imgId = TImageCache::instance()->getUniqueId();
    assert(!processed->getRaster()->getParent());
    TImageCache::instance()->add(m_imgId, (TImageP)processed);
  }

  if (!m_wasFromGR8) {
    const TPixel32 white(255, 255, 255, 0);
    for (int i = 0; i < parameters->m_colors.getColorCount(); ++i) {
      TPixel32 cc = parameters->m_colors.getColor(i).m_color;
      for (int tone = 0; tone < 256; tone++) {
        m_pixelsLut.push_back(blend(parameters->m_colors.getColor(i).m_color,
                                    white, tone, TPixelCM32::getMaxTone()));
      }
    }
  }
}

//-----------------------------------------------------------------------------------

CleanupPreprocessedImage::~CleanupPreprocessedImage() {
  TImageCache::instance()->remove(m_imgId);
}

//-----------------------------------------------------------------------------------

TRasterImageP CleanupPreprocessedImage::getPreviewImage() const {
  TRaster32P ras(getSize());
  TRasterImageP ri(ras);
  double xdpi = 0, ydpi = 0;
  getImg()->getDpi(xdpi, ydpi);
  ri->setDpi(xdpi, ydpi);
  return ri;
}

//**************************************************************************************
//    TCleanupper implementation  -  Process functions
//**************************************************************************************

bool TCleanupper::getResampleValues(const TRasterImageP &image, TAffine &aff,
                                    double &blur, TDimension &outDim,
                                    TPointD &outDpi, bool isCameraTest,
                                    bool &isSameDpi) {
  double outlp, outlq;
  double scalex, scaley;
  double cxin, cyin, cpout, cqout;
  double max_blur;
  TPointD dpi;

  // Locking the input image to be cleanupped
  image->getRaster()->lock();

  // Retrieve image infos
  int rasterLx = image->getRaster()->getLx();
  int rasterLy = image->getRaster()->getLy();

  /*---入力画像サイズとSaveBoxのサイズが一致しているか？の判定---*/
  TRect saveBox          = image->getSavebox();
  bool raster_is_savebox = true;
  if (saveBox == TRect() &&
      (saveBox.getLx() > 0 && saveBox.getLx() < rasterLx ||
       saveBox.getLy() > 0 && saveBox.getLy() < rasterLy))
    raster_is_savebox = false;

  // Use the same source dpi throughout the level
  dpi = getSourceDpi();
  if (dpi == TPointD())
    dpi.x = dpi.y = 65.0;  // using 65.0 as default DPI //??????WHY
  else if (!dpi.x)
    dpi.x = dpi.y;
  else if (!dpi.y)
    dpi.y = dpi.x;

  // Retrieve some cleanup parameters
  int rotate = m_parameters->m_rotate;

  // Build scaling/dpi data
  {
    m_parameters->getOutputImageInfo(outDim, outDpi.x, outDpi.y);

    // input -> output scale factor
    scalex = outDpi.x / dpi.x;
    scaley = outDpi.y / dpi.y;

    outlp = outDim.lx;
    outlq = outDim.ly;
  }

  /*---
   * 拡大／縮小をしていない場合（DPIが変わらない場合）、NearestNeighborでリサンプリングする。---*/
  isSameDpi = areAlmostEqual(outDpi.x, dpi.x, 0.1) &&
              areAlmostEqual(outDpi.y, dpi.y, 0.1);

  // Retrieve input center
  if (raster_is_savebox) {
    cxin = -saveBox.getP00().x + (saveBox.getLx() - 1) / 2.0;
    cyin = -saveBox.getP00().y + (saveBox.getLy() - 1) / 2.0;
  } else {
    cxin = (rasterLx - 1) / 2.0;
    cyin = (rasterLy - 1) / 2.0;
  }

  // Retrieve output center
  cpout = (outlp - 1) / 2.0;
  cqout = (outlq - 1) / 2.0;
  // Perform autocenter if any is found
  double angle = 0.0;
  double skew  = 0.0;
  TAffine pre_aff;
  image->getRaster()->lock();

  bool autocentered =
      doAutocenter(angle, skew, cxin, cyin, cqout, cpout, dpi.x, dpi.y,
                   raster_is_savebox, saveBox, image, scalex);
  image->getRaster()->unlock();

  // Build the image transform as deduced by the autocenter
  if (m_parameters->m_autocenterType == AUTOCENTER_CTR && skew) {
    pre_aff.a11 = cos(skew * M_PI_180);
    pre_aff.a21 = sin(skew * M_PI_180);
  }

  aff = (TScale(scalex, scaley) * pre_aff) * TRotation(angle);
  aff = aff.place(cxin, cyin, cpout, cqout);

  // Apply eventual additional user-defined transforms
  TPointD pout = TPointD((outlp - 1) / 2.0, (outlq - 1) / 2.0);

  if (m_parameters->m_rotate != 0)
    aff = TRotation(-(double)m_parameters->m_rotate).place(pout, pout) * aff;

  if (m_parameters->m_flipx || m_parameters->m_flipy)
    aff = TScale(m_parameters->m_flipx ? -1 : 1, m_parameters->m_flipy ? -1 : 1)
              .place(pout, pout) *
          aff;

  if (!isCameraTest)
    aff = TTranslation(m_parameters->m_offx * outDpi.x / 2,
                       m_parameters->m_offy * outDpi.y / 2) *
          aff;

  max_blur = 20.0 * sqrt(fabs(scalex /*** * oversample_factor ***/));
  blur     = pow(max_blur, (100 - m_parameters->m_sharpness) / (100 - 1));
  return autocentered;
}

//------------------------------------------------------------------------------------

// this one incorporate the preprocessColors and the finalize function; used for
// swatch.(tipically on very small rasters)
TRasterP TCleanupper::processColors(const TRasterP &rin) {
  if (m_parameters->m_lineProcessingMode == lpNone) return rin;

  TRasterCM32P rcm = TRasterCM32P(rin->getSize());
  if (!rcm) {
    assert(!"failed finalRas allocation!");
    return TRasterCM32P();
  }

  // Copy current cleanup palette to parameters' colors
  m_parameters->m_colors.update(m_parameters->m_cleanupPalette.getPointer(),
                                m_parameters->m_noAntialias);

  bool toGr8 = (m_parameters->m_lineProcessingMode == lpGrey);
  if (toGr8) {
    // No (color) processing. Not even thresholding. This just means that all
    // the important
    // stuff here is made in the brightness/contrast stage...

    // NOTE: Most of the color processing should be DISABLED in this case!!

    // finalRas->clear();
    rin->lock();
    rcm->lock();

    if (TRasterGR8P(rin)) {
      UCHAR *rowin    = rin->getRawData();
      TUINT32 *rowout = reinterpret_cast<TUINT32 *>(rcm->getRawData());
      for (int i = 0; i < rin->getLy(); i++) {
        for (int j = 0; j < rin->getLx(); j++)
          *rowout++ = *rowin++;  // Direct copy for now... :(
        rowin += rin->getWrap() - rin->getLx();
        rowout += rcm->getWrap() - rcm->getLx();
      }
    } else {
      TPixel32 *rowin = reinterpret_cast<TPixel32 *>(rin->getRawData());
      TUINT32 *rowout = reinterpret_cast<TUINT32 *>(rcm->getRawData());
      for (int i = 0; i < rin->getLy(); i++) {
        for (int j = 0; j < rin->getLx(); j++)
          *rowout++ = TPixelGR8::from(*rowin++).value;
        rowin += rin->getWrap() - rin->getLx();
        rowout += rcm->getWrap() - rcm->getLx();
      }
    }

    rin->unlock();
    rcm->unlock();
  } else {
    assert(TRaster32P(rin));
    preprocessColors(rcm, rin, m_parameters->m_colors);
  }

  // outImg->setDpi(outDpi.x, outDpi.y);
  CleanupPreprocessedImage cpi(m_parameters,
                               TToonzImageP(rcm, rcm->getBounds()), toGr8);
  cpi.m_autocentered = true;

  TRaster32P rout = TRaster32P(rin->getSize());
  finalize(rout, &cpi);
  return rout;
}

//------------------------------------------------------------------------------------

CleanupPreprocessedImage *TCleanupper::process(
    TRasterImageP &image, bool first_image, TRasterImageP &onlyResampledImage,
    bool isCameraTest, bool returnResampled, bool onlyForSwatch,
    TAffine *resampleAff) {
  TAffine aff;
  double blur;
  TDimension outDim(0, 0);
  TPointD outDpi;

  bool isSameDpi    = false;
  bool autocentered = getResampleValues(image, aff, blur, outDim, outDpi,
                                        isCameraTest, isSameDpi);
  if (m_parameters->m_autocenterType != AUTOCENTER_NONE && !autocentered)
    DVGui::warning(
        QObject::tr("The autocentering failed on the current drawing."));

  bool fromGr8 = (bool)TRasterGR8P(image->getRaster());
  bool toGr8   = (m_parameters->m_lineProcessingMode == lpGrey);

  // If necessary, perform auto-adjust
  if (!isCameraTest && m_parameters->m_lineProcessingMode != lpNone && toGr8 &&
      m_parameters->m_autoAdjustMode != AUTO_ADJ_NONE && !onlyForSwatch) {
    static int ref_cum[256];
    UCHAR lut[256];
    int cum[256];
    double x0_src_f, y0_src_f, x1_src_f, y1_src_f;
    int x0_src, y0_src, x1_src, y1_src;

    // cleanup_message("Autoadjusting... \n");

    TAffine inv = aff.inv();

    x0_src_f = affMV1(inv, 0, 0);
    y0_src_f = affMV2(inv, 0, 0);
    x1_src_f = affMV1(inv, outDim.lx - 1, outDim.ly - 1);
    y1_src_f = affMV2(inv, outDim.lx - 1, outDim.ly - 1);

    x0_src = tround(x0_src_f);
    y0_src = tround(y0_src_f);
    x1_src = tround(x1_src_f);
    y1_src = tround(y1_src_f);

    set_autoadjust_window(x0_src, y0_src, x1_src, y1_src);

    if (!TRasterGR8P(image->getRaster())) {
      // Auto-adjusting a 32-bit image. This means that a white background must
      // be introduced first.
      TRaster32P ras32(image->getRaster()->clone());
      TRop::addBackground(ras32, TPixel32::White);
      image = TRasterImageP(ras32);  // old image is released here
      ras32 = TRaster32P();

      TRasterGR8P rgr(image->getRaster()->getSize());
      TRop::copy(rgr, image->getRaster());

      // This is now legit. It was NOT before the clone, since the original
      // could be cached.
      image->setRaster(rgr);
    }
    switch (m_parameters->m_autoAdjustMode) {
    case AUTO_ADJ_HISTOGRAM:
      if (first_image) {
        build_gr_cum(image, ref_cum);
      } else {
        build_gr_cum(image, cum);
        build_gr_lut(ref_cum, cum, lut);
        apply_lut(image, lut);
      }
      break;

    case AUTO_ADJ_HISTO_L:
      histo_l_algo(image, first_image);
      break;

    case AUTO_ADJ_BLACK_EQ:
      black_eq_algo(image);
      break;

    case AUTO_ADJ_NONE:
    default:
      assert(false);
      break;
    }
  }

  fromGr8 = (bool)TRasterGR8P(
      image->getRaster());  // may have changed type due to auto-adjust

  assert(returnResampled ||
         !onlyForSwatch);  // if onlyForSwatch, then returnResampled

  // Allocate output colormap raster
  TRasterCM32P finalRas;
  if (!onlyForSwatch) {
    finalRas = TRasterCM32P(outDim);
    if (!finalRas) {
      TImageCache::instance()->outputMap(outDim.lx * outDim.ly * 4,
                                         "C:\\cachelog");
      assert(!"failed finalRas allocation!");
      return 0;
    }
  }

  // In case the input raster was a greymap, we cannot reutilize finalRas's
  // buffer to transform the final
  // fullcolor pixels to colormap pixels directly (1 32-bit pixel would hold 4
  // 8-bit pixels) - therefore,
  // a secondary greymap is allocated.

  // NOTE: This should be considered obsolete? By using TRop::resample(
  // <TRaster32P& instance> , ...) we
  // should get the same effect!!

  TRasterP tmp_ras;

  if (returnResampled || (fromGr8 && toGr8)) {
    if (fromGr8 && toGr8)
      tmp_ras = TRasterGR8P(outDim);
    else
      tmp_ras = TRaster32P(outDim);

    if (!tmp_ras) {
      TImageCache::instance()->outputMap(outDim.lx * outDim.ly * 4,
                                         "C:\\cachelog");
      assert(!"failed tmp_ras allocation!");
      return 0;
    }
  } else
    // if finalRas is allocated, and the intermediate raster has to be 32-bit,
    // we can perform pixel
    // conversion directly on the same output buffer
    tmp_ras = TRaster32P(outDim.lx, outDim.ly, outDim.lx,
                         (TPixel32 *)finalRas->getRawData());

  TRop::ResampleFilterType flt_type;
  if (isSameDpi)
    flt_type = TRop::ClosestPixel;  // NearestNeighbor
  else if (isCameraTest)
    flt_type = TRop::Triangle;
  else
    flt_type = TRop::Hann2;
  TRop::resample(tmp_ras, image->getRaster(), aff, flt_type, blur);

  if ((TRaster32P)tmp_ras)
    // Add white background to deal with semitransparent pixels
    TRop::addBackground(tmp_ras, TPixel32::White);

  if (resampleAff) *resampleAff = aff;

  image->getRaster()->unlock();
  image = TRasterImageP();

  if (returnResampled) {
    onlyResampledImage = TRasterImageP(tmp_ras);
    onlyResampledImage->setDpi(outDpi.x, outDpi.y);
  }

  if (onlyForSwatch) return 0;

  assert(finalRas);

  // Copy current cleanup palette to parameters' colors
  m_parameters->m_colors.update(m_parameters->m_cleanupPalette.getPointer(),
                                m_parameters->m_noAntialias);

  if (toGr8) {
    // No (color) processing. Not even thresholding. This just means that all
    // the important
    // stuff here is made in the brightness/contrast stage...

    // NOTE: Most of the color processing should be DISABLED in this case!!

    tmp_ras->lock();
    finalRas->lock();
    assert(tmp_ras->getSize() == finalRas->getSize());
    assert(tmp_ras->getLx() == tmp_ras->getWrap());
    assert(finalRas->getLx() == finalRas->getWrap());

    int pixCount = outDim.lx * outDim.ly;

    if (fromGr8) {
      UCHAR *rowin    = tmp_ras->getRawData();
      TUINT32 *rowout = reinterpret_cast<TUINT32 *>(finalRas->getRawData());
      for (int i = 0; i < pixCount; i++)
        *rowout++ = *rowin++;  // Direct copy for now... :(
    } else {
      TPixel32 *rowin = reinterpret_cast<TPixel32 *>(tmp_ras->getRawData());
      TUINT32 *rowout = reinterpret_cast<TUINT32 *>(finalRas->getRawData());
      for (int i = 0; i < pixCount; i++)
        *rowout++ = TPixelGR8::from(*rowin++).value;
    }

    tmp_ras->unlock();
    finalRas->unlock();
  } else {
    // WARNING: finalRas and tmp_ras may share the SAME buffer!
    assert(TRaster32P(tmp_ras));
    preprocessColors(finalRas, tmp_ras, m_parameters->m_colors);
  }

  TToonzImageP final;
  final = TToonzImageP(finalRas, finalRas->getBounds());
  final->setDpi(outDpi.x, outDpi.y);

  CleanupPreprocessedImage *cpi =
      new CleanupPreprocessedImage(m_parameters, final, toGr8);
  cpi->m_autocentered = autocentered;
  cpi->m_appliedAff   = aff;
  return cpi;
}

//------------------------------------------------------------------------------------

TRasterImageP TCleanupper::autocenterOnly(const TRasterImageP &image,
                                          bool isCameraTest,
                                          bool &autocentered) {
  double xDpi, yDpi;
  // double inlx, inly, zoom_factor, max_blur;
  double skew = 0, angle = 0,
         dist = 0 /*lq_nozoom, lp_nozoom,, cx, cy, scalex, scaley*/;
  double cxin, cyin, cpout, cqout;
  int rasterIsSavebox = true;
  TAffine aff, preAff, inv;
  int rasterLx, finalLx, rasterLy, finalLy;

  rasterLx = finalLx = image->getRaster()->getLx();
  rasterLy = finalLy = image->getRaster()->getLy();

  TRect saveBox = image->getSavebox();
  if ((saveBox == TRect()) &&
      ((saveBox.getLx() > 0 && saveBox.getLx() < rasterLx) ||
       (saveBox.getLy() > 0 && saveBox.getLy() < rasterLy)))
    rasterIsSavebox = false;

  int rotate = m_parameters->m_rotate;

  image->getDpi(xDpi, yDpi);

  if (!xDpi)  // using 65.0 as default DPI
    xDpi = (yDpi ? yDpi : 65);

  if (!yDpi) yDpi = (xDpi ? xDpi : 65);

  if (rasterIsSavebox) {
    cxin = -saveBox.getP00().x + (saveBox.getLx() - 1) / 2.0;
    cyin = -saveBox.getP00().y + (saveBox.getLy() - 1) / 2.0;
  } else {
    cxin = (rasterLx - 1) / 2.0;
    cyin = (rasterLy - 1) / 2.0;
  }

  cpout = (rasterLx - 1) / 2.0;
  cqout = (rasterLy - 1) / 2.0;

  if (m_parameters->m_autocenterType != AUTOCENTER_NONE)
    autocentered = doAutocenter(angle, skew, cxin, cyin, cqout, cpout, xDpi,
                                yDpi, rasterIsSavebox, saveBox, image, 1.0);
  else
    autocentered = true;

  if (m_parameters->m_autocenterType == AUTOCENTER_CTR && skew) {
    aff.a11 = cos(skew * M_PI_180);
    aff.a21 = sin(skew * M_PI_180);
  }

  aff = aff * TRotation(angle);

  aff = aff.place(cxin, cyin, cpout, cqout);

  if (rotate != 0 && rotate != 180) std::swap(finalLx, finalLy);

  TPointD pin  = TPointD((rasterLx - 1) / 2.0, (rasterLy - 1) / 2.0);
  TPointD pout = TPointD((finalLx - 1) / 2.0, (finalLy - 1) / 2.0);

  if (rotate != 0) aff = TRotation(-(double)rotate).place(pin, pout) * aff;

  if (m_parameters->m_flipx || m_parameters->m_flipy)
    aff = TScale(m_parameters->m_flipx ? -1 : 1, m_parameters->m_flipy ? -1 : 1)
              .place(pout, pout) *
          aff;

  if (!isCameraTest)
    aff = TTranslation(m_parameters->m_offx * xDpi / 2,
                       m_parameters->m_offy * yDpi / 2) *
          aff;

  TRasterP tmpRas;
  TPoint dp;
  if (isCameraTest)  // in cameratest, I don't want to crop the image to be
                     // shown.
  // so, I resample without cropping, and I compute the offset needed to have it
  // autocentered.
  // That offset is stored in the RasterImage(setOffset below) and then used
  // when displaying the image in camerastand (in method
  // RasterPainter::onRasterImage)
  {
    // TPointD srcActualCenter = aff.inv()*TPointD(finalLx/2.0, finalLy/2.0);//
    // the autocenter position in the source image
    // TPointD srcCenter = imageToResample->getRaster()->getCenterD();*/
    TPointD dstActualCenter = TPointD(finalLx / 2.0, finalLy / 2.0);
    TPointD dstCenter       = aff * image->getRaster()->getCenterD();

    dp = convert(
        dstCenter -
        dstActualCenter);  // the amount to be offset in the destination image.

    TRect r = convert(aff * convert(image->getRaster()->getBounds()));
    aff     = (TTranslation(convert(-r.getP00())) * aff);
    // aff = aff.place(srcActualCenter, dstActualCenter);
    tmpRas = image->getRaster()->create(r.getLx(), r.getLy());

  } else
    tmpRas = image->getRaster()->create(finalLx, finalLy);

  TRop::resample(tmpRas, image->getRaster(), aff);
  // TImageWriter::save(TFilePath("C:\\temp\\incleanup.tif"), imageToResample);
  // TImageWriter::save(TFilePath("C:\\temp\\outcleanup.tif"), tmp_ras);

  TRasterImageP final(tmpRas);
  final->setOffset(dp);

  final->setDpi(xDpi, yDpi);
  // final->sethPos(finalHPos);

  return final;
}

//**************************************************************************************
//    AutoCenter
//**************************************************************************************

bool TCleanupper::doAutocenter(

    double &angle, double &skew, double &cxin, double &cyin, double &cqout,
    double &cpout,

    const double xdpi, const double ydpi, const int raster_is_savebox,
    const TRect saveBox, const TRasterImageP &image, const double scalex) {
  double sigma = 0, theta = 0;
  FDG_INFO fdg_info = m_parameters->getFdgInfo();

  switch (m_parameters->m_autocenterType) {
  case AUTOCENTER_CTR:
    angle = fdg_info.ctr_angle;
    skew  = fdg_info.ctr_skew;
    cxin  = mmToPixel(fdg_info.ctr_x, xdpi);
    cyin  = mmToPixel(fdg_info.ctr_y, ydpi);
    if (raster_is_savebox) {
      cxin -= saveBox.getP00().x;
      cyin -= saveBox.getP00().y;
    }

    break;

  case AUTOCENTER_FDG: {
    // e se image->raster_is_savebox?
    // cleanup_message ("Autocentering...");
    int strip_width = compute_strip_pixel(&fdg_info, xdpi) + 1; /* ?!? */

    switch (m_parameters->m_pegSide) {
    case PEGS_BOTTOM:
      sigma = 0.0;
      break;
    case PEGS_RIGHT:
      sigma = 90.0;
      break;
    case PEGS_TOP:
      sigma = 180.0;
      break;
    case PEGS_LEFT:
      sigma = -90.0;
      break;
    default:
      sigma = 0.0;
      break;
    }

    theta = sigma;
    if (theta > 180.0)
      theta -= 360.0;
    else if (theta <= -180.0)
      theta += 360.0;

    PEGS_SIDE pegs_ras_side;
    if (theta == 0.0)
      pegs_ras_side = PEGS_BOTTOM;
    else if (theta == 90.0)
      pegs_ras_side = PEGS_RIGHT;
    else if (theta == 180.0)
      pegs_ras_side = PEGS_TOP;
    else if (theta == -90.0)
      pegs_ras_side = PEGS_LEFT;
    else
      pegs_ras_side = PEGS_BOTTOM;

    switch (pegs_ras_side) {
    case PEGS_LEFT:
    case PEGS_RIGHT:
      notMoreThan(image->getRaster()->getLx(), strip_width);
      break;
    default:
      notMoreThan(image->getRaster()->getLy(), strip_width);
      break;
    }

    convert_dots_mm_to_pixel(&fdg_info.dots[0], fdg_info.dots.size(), xdpi,
                             ydpi);

    double cx, cy;
    if (!get_image_rotation_and_center(
            image->getRaster(), strip_width, pegs_ras_side, &angle, &cx, &cy,
            &fdg_info.dots[0], fdg_info.dots.size())) {
      return false;
    } else {
      angle *= M_180_PI;
      cxin = cx;
      cyin = cy;
      double dist =
          (double)mmToPixel(fdg_info.dist_ctr_to_ctr_hole, xdpi * scalex);
      switch (m_parameters->m_pegSide) {
      case PEGS_BOTTOM:
        cqout -= dist;
        break;
      case PEGS_TOP:
        cqout += dist;
        break;
      case PEGS_LEFT:
        cpout -= dist;
        break;
      case PEGS_RIGHT:
        cpout += dist;
        break;
      default:
        // bad pegs side
        return false;
      }
    }
    fdg_info.dots.clear();

    break;
  }

  default:
    return false;
  }

  return true;
}

//**************************************************************************************
//    (Pre) Processing  (ie the core Cleanup procedure)
//**************************************************************************************

inline void preprocessColor(const TPixel32 &pix,
                            const TargetColorData &blackColor,
                            const std::vector<TargetColorData> &featureColors,
                            int nFeatures, TPixelCM32 &outpix) {
  // Translate the pixel to HSV
  HSVColor pixHSV(
      HSVColor::fromRGB(pix.r / 255.0, pix.g / 255.0, pix.b / 255.0));

  // First, check against matchline colors. This is needed as outline pixels'
  // tone is based upon that
  // extracted here.
  int idx = -1, tone = 255;
  double hDist = (std::numeric_limits<double>::max)(), newHDist;

  for (int i = 0; i < nFeatures; ++i) {
    const TargetColorData &fColor = featureColors[i];

    // Feature Color

    // Retrieve the hue distance and, in case it's less than current one, this
    // idx better
    // approximates the color.
    newHDist = (pixHSV.m_h > fColor.m_hsv.m_h)
                   ? std::min(pixHSV.m_h - fColor.m_hsv.m_h,
                              fColor.m_hsv.m_h - pixHSV.m_h + 360.0)
                   : std::min(fColor.m_hsv.m_h - pixHSV.m_h,
                              pixHSV.m_h - fColor.m_hsv.m_h + 360.0);
    if (newHDist < hDist) {
      hDist = newHDist;
      idx   = i;
    }
  }

  if (idx >= 0) {
    const TargetColorData &fColor = featureColors[idx];

    // First, perform saturation check
    bool saturationOk = (pixHSV.m_s > fColor.m_saturationLower) &&
                        ((fColor.m_hueLower <= fColor.m_hueUpper)
                             ? (pixHSV.m_h >= fColor.m_hueLower) &&
                                   (pixHSV.m_h <= fColor.m_hueUpper)
                             : (pixHSV.m_h >= fColor.m_hueLower) ||
                                   (pixHSV.m_h <= fColor.m_hueUpper));

    if (saturationOk) {
      tone = 255.0 * (1.0 - pixHSV.m_s) / (1.0 - fColor.m_saturationLower);
      idx  = fColor.m_idx;
    } else
      idx = -1;
  }

  // Check against outline color
  if (pixHSV.m_v < blackColor.m_hsv.m_v) {
    // Outline-sensitive tone is imposed when the value check passes
    tone = (tone * pixHSV.m_v / blackColor.m_hsv.m_v);

    // A further Chroma test is applied to decide whether a would-be outline
    // color
    // is to be intended as a matchline color instead (it has too much color)
    if ((idx < 0) || (pixHSV.m_s * pixHSV.m_v) < blackColor.m_saturationLower)
      // Outline Color
      idx = 1;
  }

  outpix = (idx > 0 && tone < 255) ? TPixelCM32(idx, 0, tone) : TPixelCM32();
}

//-----------------------------------------------------------------------------------------

void TCleanupper::preprocessColors(const TRasterCM32P &outRas,
                                   const TRaster32P &raster32,
                                   const TargetColors &colors) {
  assert(outRas && outRas->getSize() == raster32->getSize());

  // Convert the target palette to HSV colorspace
  std::vector<TargetColorData> pencilsHSV;
  for (int i = 2; i < colors.getColorCount(); ++i) {
    TargetColorData cdata(colors.getColor(i));
    cdata.m_idx = i;
    pencilsHSV.push_back(cdata);
  }

  // Extract the 'black' Value
  TargetColor black = colors.getColor(1);
  TargetColorData blackData(black);
  blackData.m_hsv.m_v += (1.0 - black.m_threshold / 100.0);
  blackData.m_saturationLower = sq(1.0 - black.m_hRange / 100.0);

  raster32->lock();
  outRas->lock();

  // For every image pixel, process it
  for (int j = 0; j < raster32->getLy(); j++) {
    TPixel32 *pix      = raster32->pixels(j);
    TPixel32 *endPix   = pix + raster32->getLx();
    TPixelCM32 *outPix = outRas->pixels(j);

    while (pix < endPix) {
      if (*pix == TPixel32::White ||
          pix->m <
              255)  // sometimes the resampling produces semitransparent pixels
        // on the  border of the raster; I discards those pixels.
        //(which otherwise creates a black border in the final cleanupped image)
        // vinz
        *outPix = TPixelCM32();
      else
        preprocessColor(*pix, blackData, pencilsHSV, pencilsHSV.size(),
                        *outPix);

      pix++;
      outPix++;
    }
  }

  raster32->unlock();
  outRas->unlock();
}

//**************************************************************************************
//    Post-Processing
//**************************************************************************************

void TCleanupper::finalize(const TRaster32P &outRas,
                           CleanupPreprocessedImage *srcImg) {
  if (!outRas) return;

  if (srcImg->m_wasFromGR8)
    doPostProcessingGR8(outRas, srcImg);
  else
    doPostProcessingColor(outRas, srcImg);
}

//-----------------------------------------------------------------------------------------

TToonzImageP TCleanupper::finalize(CleanupPreprocessedImage *src,
                                   bool isCleanupper) {
  if (src->m_wasFromGR8)
    return doPostProcessingGR8(src);
  else
    return doPostProcessingColor(src->getImg(), isCleanupper);
}

//-----------------------------------------------------------------------------------------

void TCleanupper::doPostProcessingGR8(const TRaster32P &outRas,
                                      CleanupPreprocessedImage *srcImg) {
  TToonzImageP image   = srcImg->getImg();
  TRasterCM32P rasCM32 = image->getRaster();

  rasCM32->lock();
  outRas->lock();

  TRasterCM32P cmout(outRas->getLx(), outRas->getLy(), outRas->getWrap(),
                     (TPixelCM32 *)outRas->getRawData());
  TRop::copy(cmout, rasCM32);

  rasCM32->unlock();

  // Apply brightness/contrast and grayscale conversion directly
  brightnessContrastGR8(cmout, m_parameters->m_colors);

  // Apply despeckling
  if (m_parameters->m_despeckling)
    TRop::despeckle(cmout, m_parameters->m_despeckling,
                    m_parameters->m_transparencyCheckEnabled);

  // Morphological antialiasing
  if (m_parameters->m_postAntialias) {
    TRasterCM32P newRas(cmout->getLx(), cmout->getLy());
    TRop::antialias(cmout, newRas, 10, m_parameters->m_aaValue);

    cmout->unlock();
    cmout = newRas;
    cmout->lock();
  }

  // Finally, do transparency check
  if (m_parameters->m_transparencyCheckEnabled)
    transparencyCheck(cmout, outRas);
  else
    // TRop::convert(outRas, cmout, m_parameters->m_cleanupPalette);
    TRop::convert(outRas, cmout, createToonzPaletteFromCleanupPalette());

  outRas->unlock();
}

//-----------------------------------------------------------------------------------------

TToonzImageP TCleanupper::doPostProcessingGR8(
    const CleanupPreprocessedImage *img) {
  TToonzImageP image = img->getImg();

  TRasterCM32P rasCM32 = image->getRaster();
  TRasterCM32P cmout(rasCM32->clone());

  cmout->lock();

  // Apply brightness/contrast and grayscale conversion directly
  brightnessContrastGR8(cmout, m_parameters->m_colors);

  // Apply despeckling
  if (m_parameters->m_despeckling)
    TRop::despeckle(cmout, m_parameters->m_despeckling, false);

  // Morphological antialiasing
  if (m_parameters->m_postAntialias) {
    TRasterCM32P newRas(cmout->getLx(), cmout->getLy());
    TRop::antialias(cmout, newRas, 10, m_parameters->m_aaValue);

    cmout->unlock();
    cmout = newRas;
    cmout->lock();
  }

  cmout->unlock();

  // Rebuild the cmap's bbox
  TRect bbox;
  TRop::computeBBox(cmout, bbox);

  // Copy the dpi
  TToonzImageP outImg(cmout, bbox);
  double dpix, dpiy;
  image->getDpi(dpix, dpiy);
  outImg->setDpi(dpix, dpiy);

  return outImg;
}

//-----------------------------------------------------------------------------------------

void TCleanupper::doPostProcessingColor(const TRaster32P &outRas,
                                        CleanupPreprocessedImage *srcImg) {
  assert(srcImg);
  assert(outRas->getSize() == srcImg->getSize());

  TToonzImageP imgToProcess = srcImg->getImg();
  TRasterCM32P rasCM32      = imgToProcess->getRaster();

  rasCM32->lock();
  outRas->lock();

  TRasterCM32P cmout(outRas->getLx(), outRas->getLy(), outRas->getWrap(),
                     (TPixelCM32 *)outRas->getRawData());
  TRop::copy(cmout, rasCM32);

  rasCM32->unlock();

  // First, deal with brightness/contrast
  brightnessContrast(cmout, m_parameters->m_colors);

  // Then, apply despeckling
  if (m_parameters->m_despeckling)
    TRop::despeckle(cmout, m_parameters->m_despeckling,
                    m_parameters->m_transparencyCheckEnabled);

  // Morphological antialiasing
  if (m_parameters->m_postAntialias) {
    TRasterCM32P newRas(cmout->getLx(), cmout->getLy());
    TRop::antialias(cmout, newRas, 10, m_parameters->m_aaValue);

    cmout->unlock();
    cmout = newRas;
    cmout->lock();
  }

  // Finally, do transparency check
  if (m_parameters->m_transparencyCheckEnabled)
    transparencyCheck(cmout, outRas);
  else
    // TRop::convert(outRas, cmout, m_parameters->m_cleanupPalette);
    TRop::convert(outRas, cmout, createToonzPaletteFromCleanupPalette());

  outRas->unlock();
}

//------------------------------------------------------------------------------

TToonzImageP TCleanupper::doPostProcessingColor(
    const TToonzImageP &imgToProcess, bool isCleanupper) {
  //(Build and) Copy imgToProcess to output image
  TToonzImageP outImage;
  if (isCleanupper)
    outImage = imgToProcess;
  else
    outImage = TToonzImageP(imgToProcess->cloneImage());

  assert(outImage);
  assert(m_parameters->m_colors.getColorCount() < 9);

  // Perform post-processing
  TRasterCM32P outRasCM32 = outImage->getRaster();
  outRasCM32->lock();

  // Brightness/Contrast
  brightnessContrast(outRasCM32, m_parameters->m_colors);

  // Despeckling
  if (m_parameters->m_despeckling)
    TRop::despeckle(outRasCM32, m_parameters->m_despeckling, false);

  // Morphological antialiasing
  if (m_parameters->m_postAntialias) {
    TRasterCM32P newRas(outRasCM32->getLx(), outRasCM32->getLy());
    TRop::antialias(outRasCM32, newRas, 10, m_parameters->m_aaValue);

    outRasCM32->unlock();
    outRasCM32 = newRas;
    outImage->setCMapped(outRasCM32);
    outRasCM32->lock();
  }

  TRect bbox;
  TRop::computeBBox(outRasCM32, bbox);
  outImage->setSavebox(bbox);

  outRasCM32->unlock();
  return outImage;
}
