

#include "stdfx.h"
#include "tfxparam.h"
#include "trop.h"
#include "tdoubleparam.h"
#include "trasterfx.h"

/* (Daniele)

  NOTE: Current LocalBlurFx is effectively flawed. Following implementation
  relies on the idea that
        the blurring filter is separable and therefore appliable on rows and
  columns, in sequence.

        It actually is not. It can be easily verified applying the fx on a
  chessboard with a strong
        blur intensity. The squares will be cast vertically towards blurred
  regions.

        Originally, this was a sub-optimal O(lx * ly * blur) algorithm. The
  following (still separated)
        is O(lx * ly) using precomputed sums in an additional line.

        The 'correct' algorithm could be implemented again as O(lx * ly * blur),
  with additional O(lx * blur)
        memory usage with precomputed sums along (blur) rows.

        Pixels would have to be filtered one by one, but the horizontal
  filtering per pixel convolution row
        would take O(1). Thus it would be O(blur) (column filtering) per pixel.
*/

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

struct Sums {
  std::unique_ptr<TUINT64[]>
      m_sumsIX_r;  //!< m_sumsIX1[i+1] = m_sumsIX1[i] + i * pix.r
  std::unique_ptr<TUINT64[]> m_sumsIX_g;
  std::unique_ptr<TUINT64[]> m_sumsIX_b;
  std::unique_ptr<TUINT64[]> m_sumsIX_m;
  std::unique_ptr<TUINT64[]> m_sumsX_r;  //!< m_sumsX[i+1] = m_sumsX[i] + pix.r
  std::unique_ptr<TUINT64[]> m_sumsX_g;
  std::unique_ptr<TUINT64[]> m_sumsX_b;
  std::unique_ptr<TUINT64[]> m_sumsX_m;

  Sums(int length)
      : m_sumsIX_r(new TUINT64[length + 1])
      , m_sumsIX_g(new TUINT64[length + 1])
      , m_sumsIX_b(new TUINT64[length + 1])
      , m_sumsIX_m(new TUINT64[length + 1])
      , m_sumsX_r(new TUINT64[length + 1])
      , m_sumsX_g(new TUINT64[length + 1])
      , m_sumsX_b(new TUINT64[length + 1])
      , m_sumsX_m(new TUINT64[length + 1]) {}

  template <typename Pix>
  void build(Pix *line, int wrap, int n) {
    ++n;

    m_sumsIX_r[0] = m_sumsX_r[0] = 0;
    m_sumsIX_g[0] = m_sumsX_g[0] = 0;
    m_sumsIX_b[0] = m_sumsX_b[0] = 0;
    m_sumsIX_m[0] = m_sumsX_m[0] = 0;

    Pix *pix;
    int i, i_1;

    for (pix = line, i_1 = 0, i = 1; i < n; pix += wrap, i_1 = i++) {
      m_sumsIX_r[i] = m_sumsIX_r[i_1] + i * pix->r;
      m_sumsIX_g[i] = m_sumsIX_g[i_1] + i * pix->g;
      m_sumsIX_b[i] = m_sumsIX_b[i_1] + i * pix->b;
      m_sumsIX_m[i] = m_sumsIX_m[i_1] + i * pix->m;

      m_sumsX_r[i] = m_sumsX_r[i_1] + pix->r;
      m_sumsX_g[i] = m_sumsX_g[i_1] + pix->g;
      m_sumsX_b[i] = m_sumsX_b[i_1] + pix->b;
      m_sumsX_m[i] = m_sumsX_m[i_1] + pix->m;
    }
  }
};

//----------------------------------------------------------------------------

template <typename Pix, typename Grey>
void filterLine(Pix *lineIn, int wrapIn, Grey *lineGr, int wrapGr, Pix *lineOut,
                int wrapOut, int length, double blurFactor, Sums &sums) {
  // Build temporary sums
  sums.build(lineIn, wrapIn, length);

  // Declare vars
  double blur, kLeft, kRight, cLeft, cRight;
  double blurI;

  // Perform line filtering
  Pix *pixIn, *pixOut;
  Grey *pixGr;

  int i, iLeft, iRight;
  ++length;

  for (i = 1, pixIn = lineIn, pixGr = lineGr, pixOut = lineOut; i < length;
       ++i, pixIn += wrapIn, pixGr += wrapGr, pixOut += wrapOut) {
    blur = pixGr->value * blurFactor;  // A table of factors should be made -
                                       // since we have a finite

    if (blur > 0.0) {
      blur +=
          0.5; /*-- 0.5足すのは、注目ピクセルの半径分。例えばBlur0.5は、
                                          注目ピクセルの外側0.5ピクセルボケるということなので。
                                  --*/

      blurI = (double)tfloor(blur);

      double amount = blur + (2 * blur - blurI - 1) * blurI;

      double dR  = 1.0 / amount;
      double ini = (blur - blurI) / amount;

      kLeft  = dR;
      cLeft  = ini - dR * (i - blurI);
      kRight = -dR;
      cRight = blur / amount + dR * i;

      // NOTE: The normalization factor with blur (not integer) would be:
      //        1.0 / (1 + 2 * blurI - (blurI / blur) * (blurI + 1))
      //        -- could be done using a factors table

      iLeft  = std::max(i - tfloor(blur) - 1, 0);
      iRight = std::min(i + tfloor(blur), length - 1);

      pixOut->r =
          troundp(kLeft * (sums.m_sumsIX_r[i] - sums.m_sumsIX_r[iLeft]) +
                  kRight * (sums.m_sumsIX_r[iRight] - sums.m_sumsIX_r[i]) +
                  cLeft * (sums.m_sumsX_r[i] - sums.m_sumsX_r[iLeft]) +
                  cRight * (sums.m_sumsX_r[iRight] - sums.m_sumsX_r[i]));
      pixOut->g =
          troundp(kLeft * (sums.m_sumsIX_g[i] - sums.m_sumsIX_g[iLeft]) +
                  kRight * (sums.m_sumsIX_g[iRight] - sums.m_sumsIX_g[i]) +
                  cLeft * (sums.m_sumsX_g[i] - sums.m_sumsX_g[iLeft]) +
                  cRight * (sums.m_sumsX_g[iRight] - sums.m_sumsX_g[i]));
      pixOut->b =
          troundp(kLeft * (sums.m_sumsIX_b[i] - sums.m_sumsIX_b[iLeft]) +
                  kRight * (sums.m_sumsIX_b[iRight] - sums.m_sumsIX_b[i]) +
                  cLeft * (sums.m_sumsX_b[i] - sums.m_sumsX_b[iLeft]) +
                  cRight * (sums.m_sumsX_b[iRight] - sums.m_sumsX_b[i]));
      pixOut->m =
          troundp(kLeft * (sums.m_sumsIX_m[i] - sums.m_sumsIX_m[iLeft]) +
                  kRight * (sums.m_sumsIX_m[iRight] - sums.m_sumsIX_m[i]) +
                  cLeft * (sums.m_sumsX_m[i] - sums.m_sumsX_m[iLeft]) +
                  cRight * (sums.m_sumsX_m[iRight] - sums.m_sumsX_m[i]));
    } else
      *pixOut = *pixIn;
  }
}

//----------------------------------------------------------------------------

template <typename Pix, typename Grey>
void doLocalBlur(TRasterPT<Pix> rin, TRasterPT<Pix> rcontrol,
                 TRasterPT<Pix> rout, double blur, const TPoint &displacement) {
  assert(rin->getLx() == rcontrol->getLx() &&
         rin->getLy() == rcontrol->getLy());

  double blurFactor = blur / Grey::maxChannelValue;

  int x, y, inLx, inLy, outLx, outLy, wrapIn, wrapOut, wrapC;

  inLx    = rin->getLx();
  inLy    = rin->getLy();
  wrapIn  = rin->getWrap();
  outLx   = rout->getLx();
  outLy   = rout->getLy();
  wrapOut = rout->getWrap();

  // Convert the control raster to grey values (this avoids the overhead of
  // performing the pixel-to-value
  // conversion twice, once per line filtering)
  TRasterPT<Grey> rcontrolGrey(rcontrol->getLx(), rcontrol->getLy());
  TRop::convert(rcontrolGrey, rcontrol);

  wrapC = rcontrolGrey->getWrap();

  Pix *lineIn, *bufIn;
  Grey *lineC, *bufC;

  // Filter rin. The output filtering is still stored in rin.

  rin->lock();
  rcontrolGrey->lock();

  bufIn = rin->pixels(0);
  bufC  = rcontrolGrey->pixels(0);

  {
    Sums sums(inLx);

    for (y = 0, lineIn = bufIn, lineC = bufC; y < inLy;
         ++y, lineIn += wrapIn, lineC += wrapC) {
      // Filter row
      filterLine(lineIn, 1, lineC, 1, lineIn, 1, inLx, blurFactor, sums);
    }
  }

  {
    Sums sums(inLy);

    for (x = 0, lineIn = bufIn, lineC = bufC; x < inLx;
         ++x, ++lineIn, ++lineC) {
      // Filter column
      filterLine(lineIn, wrapIn, lineC, wrapC, lineIn, wrapIn, inLy, blurFactor,
                 sums);
    }
  }

  rin->unlock();
  rcontrolGrey->unlock();

  // Copy the interesting part of rin to rout
  TRect rectOut(rout->getBounds() - displacement);
  TRect rectIn(rin->getBounds() + displacement);

  TRop::copy(rout->extract(rectIn), rin->extract(rectOut));
}

}  // namespace

//********************************************************************************
//    LocalBlurFx implementation
//********************************************************************************

class LocalBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(LocalBlurFx)

protected:
  TRasterFxPort m_up, m_ref;
  TDoubleParamP m_value;

public:
  LocalBlurFx() : m_value(20) {
    m_value->setMeasureName("fxLength");
    addInputPort("Source", m_up);
    addInputPort("Reference", m_ref);
    bindParam(this, "value", m_value);
    m_value->setValueRange(0, (std::numeric_limits<double>::max)());
  }

  ~LocalBlurFx() {}

  bool canHandle(const TRenderSettings &info, double frame) override {
    return (isAlmostIsotropic(info.m_affine) || m_value->getValue(frame) == 0);
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_up.isConnected()) {
      bool ret = m_up->doGetBBox(frame, bBox, info);

      double blur = fabs(m_value->getValue(frame));
      int blurI   = tceil(blur);
      bBox        = bBox.enlarge(blurI);

      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void enlarge(const TRectD &bbox, TRectD &requestedRect, int blur);

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &info) override;

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
};

//-------------------------------------------------------------------

void LocalBlurFx::enlarge(const TRectD &bbox, TRectD &requestedRect, int blur) {
  // See BlurFx: this is a faithful replica
  if (bbox.isEmpty() || requestedRect.isEmpty()) {
    requestedRect.empty();
    return;
  }

  TRectD enlargedBBox(bbox.enlarge(blur));
  TRectD enlargedOut(requestedRect.enlarge(blur));

  TPointD originalP00(requestedRect.getP00());
  requestedRect = (enlargedOut * bbox) + (enlargedBBox * requestedRect);

  requestedRect -= originalP00;
  requestedRect.x0 = tfloor(requestedRect.x0);
  requestedRect.y0 = tfloor(requestedRect.y0);
  requestedRect.x1 = tceil(requestedRect.x1);
  requestedRect.y1 = tceil(requestedRect.y1);
  requestedRect += originalP00;
}

//-------------------------------------------------------------------

void LocalBlurFx::doDryCompute(TRectD &rectOut, double frame,
                               const TRenderSettings &info) {
  // Mimics the doCompute() without actual computation.

  TTile refTile;
  bool isUp   = m_up.isConnected();
  bool isDown = m_ref.isConnected();

  if (!isUp) return;

  if (!isDown) {
    m_up->dryCompute(rectOut, frame, info);
    return;
  }

  double blur =
      fabs(m_value->getValue(frame) * sqrt(fabs(info.m_affine.det())));
  int blurI = tceil(blur);

  TRectD bboxIn;
  if (!m_up->getBBox(frame, bboxIn, info) || rectOut.isEmpty()) return;

  TRectD rectIn(rectOut);
  enlarge(bboxIn, rectIn, blurI);

  if (rectIn.isEmpty()) return;

  m_up->dryCompute(rectOut, frame, info);
  m_ref->dryCompute(rectOut, frame, info);
}

//-------------------------------------------------------------------

void LocalBlurFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &info) {
  TTile refTile;
  bool isUp   = m_up.isConnected();
  bool isDown = m_ref.isConnected();

  if (!isUp) return;

  if (!isDown) {
    m_up->compute(tile, frame, info);
    return;
  }

  // Generic case
  assert(isAlmostIsotropic(info.m_affine));
  double blur =
      fabs(m_value->getValue(frame) * sqrt(fabs(info.m_affine.det())));
  int blurI = tceil(blur);

  // Get the requested tile's geometry
  TRectD rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                         tile.getRaster()->getLy()));

  // Retrieve the input interesting geometry - and ensure that something
  // actually has
  // to be computed
  TRectD bboxIn;
  if (!m_up->getBBox(frame, bboxIn, info) || rectOut.isEmpty()) return;

  TRectD rectIn(rectOut);
  enlarge(bboxIn, rectIn, blurI);

  if (rectIn.isEmpty()) return;

  // Finally, allocate and compute the blur argument
  TTile tileIn;
  m_up->allocateAndCompute(tileIn, rectIn.getP00(),
                           TDimension(rectIn.getLx(), rectIn.getLy()),
                           tile.getRaster(), frame, info);

  TTile tileRef;
  m_ref->allocateAndCompute(tileRef, rectIn.getP00(),
                            TDimension(rectIn.getLx(), rectIn.getLy()),
                            tile.getRaster(), frame, info);

  // Perform Local Blur
  TRasterP inRas(tileIn.getRaster());
  TRasterP refRas(tileRef.getRaster());
  TRasterP outRas(tile.getRaster());

  TRaster32P in32(inRas);
  TRaster32P ref32(refRas);
  TRaster32P out32(outRas);

  TPoint displacement(convert(
      rectIn.getP00() - tile.m_pos));  // inTile position relative to (out)tile
  // The difference already has integer coordinates due to enlarge()

  if (in32 && ref32 && out32)
    doLocalBlur<TPixelRGBM32, TPixelGR8>(in32, ref32, out32, blur,
                                         displacement);
  else {
    TRaster64P in64(inRas);
    TRaster64P ref64(refRas);
    TRaster64P out64(outRas);

    if (in64 && ref64 && out64)
      doLocalBlur<TPixelRGBM64, TPixelGR16>(in64, ref64, out64, blur,
                                            displacement);
    else
      throw TException("LocalBlurFx: unsupported raster type");
  }
}

//-------------------------------------------------------------------

int LocalBlurFx::getMemoryRequirement(const TRectD &rect, double frame,
                                      const TRenderSettings &info) {
  double blur =
      fabs(m_value->getValue(frame) * sqrt(fabs(info.m_affine.det())));
  int blurI = tceil(blur);

  return 2 * TRasterFx::memorySize(rect.enlarge(blurI), info.m_bpp);
}

//-------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(LocalBlurFx, "localBlurFx")
