

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "tparamset.h"

//************************************************************************
//    Local namespace
//************************************************************************

namespace {

enum PixelOp { OVER = 0, ADD, SUBTRACT, MULTIPLY, LIGHTEN, DARKEN };

//------------------------------------------------------------------------------

template <typename PIXEL>
int doBlurValue(PIXEL *SRC, int SRC_WRAP, int BLUR) {
  unsigned int count = 0;
  int h, k;
  int blur_val;
  PIXEL *pix, *line;
  pix = line = (SRC) + (-(BLUR) + 1) * (SRC_WRAP);
  for (h = -(BLUR) + 1; h < (BLUR); h++) {
    for (k = -(BLUR) + 1; k < (BLUR); k++) count += pix[k].m;
    line += SRC_WRAP;
    pix = line;
  }
  blur_val = (int)(count / (4 * (float)(BLUR) * (float)((BLUR)-1) + 1));
  return blur_val;
}

//------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doBlur(CHANNEL_TYPE *greymap, const TRasterPT<PIXEL> &rin, int blur) {
  // Perform a flat mean-convolution filter
  // NOTE: This is improved due to separability of convolution kernel, plus
  // - since its elements are all the same, only pixels entering and quitting
  // the convolution area are added/subtracted from the sums.
  // As a result, this yields an O(row*columns) complexity, independently
  // from the blur factor.

  int i, j;
  int blurDiameter = 2 * blur + 1;
  unsigned long sum;
  int wrapSrc = rin->getWrap();
  int wrapOut = rin->getLx();

  // First, blur each column independently

  // We'll need a temporary col for storing sums
  std::unique_ptr<unsigned long[]> tempCol(new unsigned long[rin->getLy()]);
  int edge = std::min(blur + 1, rin->getLy());

  for (i = 0; i < rin->getLx(); ++i) {
    PIXEL *lineSrcPix        = rin->pixels(0) + i;
    CHANNEL_TYPE *lineOutPix = greymap + i;
    PIXEL *pixin             = lineSrcPix;
    unsigned long *pixsum    = tempCol.get();

    memset(tempCol.get(), 0, rin->getLy() * sizeof(unsigned long));

    // Build up to blur with retro-sums
    sum = 0;
    for (j = 0; j < edge; ++j, pixin += wrapSrc, ++pixsum) {
      sum += pixin->m;
      *pixsum = sum;
    }

    // Fill in after blur
    PIXEL *queuepix = lineSrcPix;
    for (j = edge; j < rin->getLy();
         ++j, pixin += wrapSrc, queuepix += wrapSrc, ++pixsum) {
      sum += (pixin->m - queuepix->m);
      *pixsum = sum;
    }

    // Now, the same in reverse
    lineSrcPix = lineSrcPix + (rin->getLy() - 1) * wrapSrc;
    pixin      = lineSrcPix;
    pixsum     = tempCol.get() + rin->getLy() - 1;

    sum = 0;
    for (j = 0; j < edge; ++j, pixin -= wrapSrc, --pixsum) {
      *pixsum += sum;
      sum += pixin->m;
    }

    queuepix = lineSrcPix;
    for (j = edge; j < rin->getLy();
         ++j, pixin -= wrapSrc, queuepix -= wrapSrc, --pixsum) {
      sum -= queuepix->m;
      *pixsum += sum;
      sum += pixin->m;
    }

    // Finally, transfer sums to the output greymap, divided by the blur.
    pixsum               = tempCol.get();
    CHANNEL_TYPE *pixout = lineOutPix;
    for (j = 0; j < rin->getLy(); ++j, pixout += wrapOut, ++pixsum)
      *pixout = (*pixsum) / blurDiameter;
  }

  // Then, the same for all greymap rows

  // We'll need a temporary row for sums
  std::unique_ptr<unsigned long[]> tempRow(new unsigned long[rin->getLx()]);
  edge = std::min(blur + 1, rin->getLx());

  for (j = 0; j < rin->getLy(); ++j) {
    CHANNEL_TYPE *lineSrcPix = greymap + j * wrapOut;
    CHANNEL_TYPE *lineOutPix = lineSrcPix;
    unsigned long *pixsum    = tempRow.get();
    CHANNEL_TYPE *pixin      = lineSrcPix;

    memset(tempRow.get(), 0, rin->getLx() * sizeof(unsigned long));

    // Build up to blur with retro-sums
    sum = 0;
    for (i = 0; i < edge; ++i, ++pixin, ++pixsum) {
      sum += *pixin;
      *pixsum = sum;
    }

    // Fill in after blur
    CHANNEL_TYPE *queuepix = lineSrcPix;
    for (i = edge; i < rin->getLx(); ++i, ++pixin, ++pixsum, ++queuepix) {
      sum += *pixin;
      sum -= *queuepix;
      *pixsum = sum;
    }

    // Now, the same in reverse
    lineSrcPix = lineSrcPix + rin->getLx() - 1;
    pixin      = lineSrcPix;
    pixsum     = tempRow.get() + rin->getLx() - 1;

    sum = 0;
    for (i = 0; i < edge; ++i, --pixin, --pixsum) {
      *pixsum += sum;
      sum += *pixin;
    }

    queuepix = lineSrcPix;
    for (i = edge; i < rin->getLx(); ++i, --pixin, --pixsum, --queuepix) {
      sum -= *queuepix;
      *pixsum += sum;
      sum += *pixin;
    }

    // Finally, transfer sums to the output greymap, divided by the blur.
    CHANNEL_TYPE *pixout = lineOutPix;
    pixsum               = tempRow.get();
    for (i = 0; i < rin->getLx(); ++i, ++pixout, ++pixsum)
      *pixout = (*pixsum) / blurDiameter;
  }
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void myOver(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  pixout = color;
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void myAdd(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  pixout.r = std::min(pixin.r + color.r, PIXEL::maxChannelValue);
  pixout.g = std::min(pixin.g + color.g, PIXEL::maxChannelValue);
  pixout.b = std::min(pixin.b + color.b, PIXEL::maxChannelValue);
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void mySub(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  pixout.r = std::max(pixin.r - color.r, 0);
  pixout.g = std::max(pixin.g - color.g, 0);
  pixout.b = std::max(pixin.b - color.b, 0);
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void myMult(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  static const double den = PIXEL::maxChannelValue;

  pixout.r = pixin.r * (color.r / den);
  pixout.g = pixin.g * (color.g / den);
  pixout.b = pixin.b * (color.b / den);
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void myLighten(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  pixout.r = pixin.r > color.r ? pixin.r : color.r;
  pixout.g = pixin.g > color.g ? pixin.g : color.g;
  pixout.b = pixin.b > color.b ? pixin.b : color.b;
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void myDarken(PIXEL &pixout, const PIXEL &pixin, const PIXEL &color) {
  pixout.r = pixin.r < color.r ? pixin.r : color.r;
  pixout.g = pixin.g < color.g ? pixin.g : color.g;
  pixout.b = pixin.b < color.b ? pixin.b : color.b;
}

//------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL>
void doLayerBlending(PIXEL *pixin, PIXEL *pixout, CHANNEL *pixmatte, int inLx,
                     int outLx, int outLy, int wrapIn, int wrapOut, int dx,
                     int dy, double transp, PIXEL color,
                     void (*pixelOp)(PIXEL &, const PIXEL &, const PIXEL &)) {
  double CROP_VAL    = PIXEL::maxChannelValue;
  CHANNEL U_CROP_VAL = PIXEL::maxChannelValue;

  int const_transp = CROP_VAL * transp + 0.5;
  double val_r, val_g, val_b, val_m;
  int blur_val;

  PIXEL opColor;
  double k, matte;

  CHANNEL shadow_matte;

  int x, y;
  for (y = 0; y < outLy; ++y, pixin += wrapIn - outLx,
      pixout += wrapOut - outLx, pixmatte += inLx - outLx)
    for (x = 0; x < outLx; ++x, ++pixin, ++pixout, ++pixmatte) {
      if (pixin->m != 0)  // where the image is transparent, no shadow
      {
        blur_val     = *(pixmatte + dy * inLx + dx);
        shadow_matte = (blur_val) ? (int)((CROP_VAL - blur_val) * transp + 0.5)
                                  : const_transp;

        k     = (double)(CROP_VAL - shadow_matte) / CROP_VAL;
        val_r = pixin->r * k + 0.5;
        val_g = pixin->g * k + 0.5;
        val_b = pixin->b * k + 0.5;
        val_m = pixin->m;

        pixelOp(opColor, *pixin, color);
        matte = (1 - k) * (val_m / CROP_VAL);

        val_r += matte * opColor.r;
        val_g += matte * opColor.g;
        val_b += matte * opColor.b;

        pixout->r = (val_r > CROP_VAL) ? U_CROP_VAL
                                       : ((val_r < 0) ? 0 : (CHANNEL)val_r);
        pixout->g = (val_g > CROP_VAL) ? U_CROP_VAL
                                       : ((val_g < 0) ? 0 : (CHANNEL)val_g);
        pixout->b = (val_b > CROP_VAL) ? U_CROP_VAL
                                       : ((val_b < 0) ? 0 : (CHANNEL)val_b);
        pixout->m = (val_m > CROP_VAL) ? U_CROP_VAL
                                       : ((val_m < 0) ? 0 : (CHANNEL)val_m);
      } else
        *pixout = PIXEL::Transparent;
    }
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void doBodyHighlight(const TRasterPT<PIXEL> rout, const TRasterPT<PIXEL> rin,
                     TRectD rectIn, int rasInLx, int rasInLy, double frame,
                     int blur, double transp, PIXEL color, TPointD point,
                     bool invert,
                     void (*pixelOp)(PIXEL &, const PIXEL &, const PIXEL &),
                     TRasterFxPort &m_input, const TRenderSettings &ri) {
  typedef typename PIXEL::Channel CHANNEL_TYPE;

  rin->lock();
  rout->lock();

  int inLx = rin->getLx(), inLy = rin->getLy(), inWrap = rin->getWrap();
  int outLx = rout->getLx(), outLy = rout->getLy(), outWrap = rout->getWrap();
  int dx = point.x, dy = point.y;

  transp = 1.0 - transp;

  PIXEL *src_buf, *dst_buf;

  dst_buf = rout->pixels(0);
  src_buf = rin->pixels(0) + blur + blur * inWrap;
  if (dy < 0) src_buf -= dy * inWrap;
  if (dx < 0) src_buf -= dx;

  // First, perform an optimized mean-convolution of the interesting part of
  // image's matte
  int matteLxLy = inLx * inLy;
  CHANNEL_TYPE *matteGreymap =
      (CHANNEL_TYPE *)malloc(matteLxLy * sizeof(CHANNEL_TYPE));
  if (!matteGreymap) return;

  memset(matteGreymap, 0, matteLxLy * sizeof(CHANNEL_TYPE));
  doBlur<PIXEL, CHANNEL_TYPE>(matteGreymap, rin, blur);

  CHANNEL_TYPE U_CROP_VAL = PIXEL::maxChannelValue;

  // If specified, invert the matte
  if (invert) {
    CHANNEL_TYPE *mpix, *end = matteGreymap + matteLxLy;
    for (mpix = matteGreymap; mpix < end; ++mpix) *mpix = U_CROP_VAL - *mpix;
  }

  CHANNEL_TYPE *mattepix = matteGreymap + blur + blur * inLx;
  if (dy < 0) mattepix -= dy * inLx;
  if (dx < 0) mattepix -= dx;

  doLayerBlending(src_buf, dst_buf, mattepix, inLx, outLx, outLy, inWrap,
                  outWrap, dx, dy, transp, color, pixelOp);

  free(matteGreymap);

  rin->unlock();
  rout->unlock();
}

}  // namespace

//************************************************************************
//    BodyHighLightFx implementation
//************************************************************************

class BodyHighLightFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(BodyHighLightFx)

  TRasterFxPort m_input;
  TIntEnumParamP m_mode;
  TPointParamP m_point;
  TDoubleParamP m_transparency;
  TDoubleParamP m_blur;
  TPixelParamP m_color;
  TBoolParamP m_invert;

public:
  BodyHighLightFx()
      : m_point(TPointD(10.0, 10.0))
      , m_mode(new TIntEnumParam(OVER, "Over"))
      , m_transparency(0.5)
      , m_blur(2.0)
      , m_color(TPixel32::White)
      , m_invert(false) {
    m_point->getX()->setMeasureName("fxLength");
    m_point->getY()->setMeasureName("fxLength");
    m_blur->setMeasureName("fxLength");
    bindParam(this, "mode", m_mode);
    bindParam(this, "point", m_point);
    bindParam(this, "transparency", m_transparency);
    bindParam(this, "blur", m_blur);
    bindParam(this, "color", m_color);
    bindParam(this, "invert", m_invert);
    addInputPort("Source", m_input);
    m_transparency->setValueRange(0.0, 1.0);
    m_blur->setValueRange(0, (std::numeric_limits<double>::max)());
    m_color->enableMatte(false);

    m_mode->addItem(ADD, "Add");
    m_mode->addItem(SUBTRACT, "Subtract");
    m_mode->addItem(MULTIPLY, "Multiply");
    m_mode->addItem(LIGHTEN, "Lighten");
    m_mode->addItem(DARKEN, "Darken");
  }

  ~BodyHighLightFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
};

//------------------------------------------------------------------------------

void BodyHighLightFx::doDryCompute(TRectD &rect, double frame,
                                   const TRenderSettings &info) {
  m_input->dryCompute(rect, frame, info);

  double fac   = sqrt(fabs(info.m_affine.det()));
  int blur     = (int)(fac * fabs(m_blur->getValue(frame)));
  TPoint point = convert(fac * m_point->getValue(frame));

  TRectD rectIn = rect.enlarge(blur);

  int rasInLx = tround(rectIn.getLx() + abs(point.x)) + 1;
  int rasInLy = tround(rectIn.getLy() + abs(point.y)) + 1;
  if (point.x < 0) {
    rectIn.x0 += point.x;
  }
  if (point.y < 0) {
    rectIn.y0 += point.y;
  }

  rectIn = TRectD(rectIn.getP00(), TDimensionD(rasInLx, rasInLy));

  m_input->dryCompute(rectIn, frame, info);
}

//------------------------------------------------------------------------------

void BodyHighLightFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &ri) {
  double fac = sqrt(fabs(ri.m_affine.det()));

  if (!m_input.isConnected()) return;

  // Compute the effective input tile
  m_input->compute(tile, frame, ri);

  double transp = m_transparency->getValue(frame);
  int blur      = (int)(fac * fabs(m_blur->getValue(frame)));
  TPoint point  = convert(fac * m_point->getValue(frame));
  bool invert   = m_invert->getValue();

  // Build the shadow, blurred tile
  TDimension rectSize(tile.getRaster()->getSize());
  TRectD rectIn(tile.m_pos, TDimensionD(rectSize.lx, rectSize.ly));
  rectIn = rectIn.enlarge(blur);

  // Shift rectIn by the input 'point'
  int rasInLx = tround(rectIn.getLx() + abs(point.x)) + 1;
  int rasInLy = tround(rectIn.getLy() + abs(point.y)) + 1;
  if (point.x < 0) {
    rectIn.x0 += point.x;
  } else {
    rectIn.x1 += point.x;
  }
  if (point.y < 0) {
    rectIn.y0 += point.y;
  } else {
    rectIn.y1 += point.y;
  }

  const TPixel32 color = m_color->getPremultipliedValue(frame);

  TRaster32P raster32 = tile.getRaster();
  TRaster64P raster64 = tile.getRaster();

  TTile tileIn;
  m_input->allocateAndCompute(tileIn, rectIn.getP00(),
                              TDimension(rasInLx, rasInLy), tile.getRaster(),
                              frame, ri);

  // Select the specified pixel operation
  void (*pixelOp32)(TPixel32 &, const TPixel32 &, const TPixel32 &);
  void (*pixelOp64)(TPixel64 &, const TPixel64 &, const TPixel64 &);

  switch (m_mode->getValue()) {
  case OVER: {
    pixelOp32 = ::myOver<TPixel32>;
    pixelOp64 = ::myOver<TPixel64>;
    break;
  }

  case ADD: {
    pixelOp32 = ::myAdd<TPixel32>;
    pixelOp64 = ::myAdd<TPixel64>;
    break;
  }

  case SUBTRACT: {
    pixelOp32 = ::mySub<TPixel32>;
    pixelOp64 = ::mySub<TPixel64>;
    break;
  }

  case MULTIPLY: {
    pixelOp32 = ::myMult<TPixel32>;
    pixelOp64 = ::myMult<TPixel64>;
    break;
  }

  case LIGHTEN: {
    pixelOp32 = ::myLighten<TPixel32>;
    pixelOp64 = ::myLighten<TPixel64>;
    break;
  }

  case DARKEN: {
    pixelOp32 = ::myDarken<TPixel32>;
    pixelOp64 = ::myDarken<TPixel64>;
    break;
  }
  }

  TRaster32P rin = tileIn.getRaster();
  if (raster32)
    doBodyHighlight<TPixel32>(raster32, rin, rectIn, rasInLx, rasInLy, frame,
                              blur, transp, color, convert(point), invert,
                              pixelOp32, m_input, ri);
  else {
    TRaster64P rin = tileIn.getRaster();
    if (raster64)
      doBodyHighlight<TPixel64>(raster64, rin, rectIn, rasInLx, rasInLy, frame,
                                blur, transp, toPixel64(color), convert(point),
                                invert, pixelOp64, m_input, ri);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

int BodyHighLightFx::getMemoryRequirement(const TRectD &rect, double frame,
                                          const TRenderSettings &info) {
  int blur =
      (int)(sqrt(fabs(info.m_affine.det())) * fabs(m_blur->getValue(frame)));
  return TRasterFx::memorySize(rect.enlarge(blur), info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(BodyHighLightFx, "bodyHighLightFx")
