#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tpixelutils.h"
#include "trop.h"

#include <algorithm>
#include <cmath>
#include <limits>

//==============================================================================
//    Tone / Highlight Fx
//
// Cel-style shading: darkens (Tone) or lightens (Highlight) the parts of the
// Source that the Matte covers. The result stays inside the Source's own
// shape, so shadow and highlight shapes can be drawn loosely over a character
// on a separate level. Dark line pixels can be left untouched so the shading
// only affects the paint.
//==============================================================================

namespace {

struct ToneHighlightSettings {
  bool highlight;
  TPixel32 color;
  double intensity;
  bool protectLines;
  double lineThreshold;
  bool invertMatte;
};

template <class PIXEL>
void storeChannels(PIXEL &pixel, double r, double g, double b) {
  pixel.r =
      (typename PIXEL::Channel)tcrop(tround(r), 0, PIXEL::maxChannelValue);
  pixel.g =
      (typename PIXEL::Channel)tcrop(tround(g), 0, PIXEL::maxChannelValue);
  pixel.b =
      (typename PIXEL::Channel)tcrop(tround(b), 0, PIXEL::maxChannelValue);
}

template <>
void storeChannels<TPixelF>(TPixelF &pixel, double r, double g, double b) {
  pixel.r = static_cast<float>(r);
  pixel.g = static_cast<float>(g);
  pixel.b = static_cast<float>(b);
}

template <class PIXEL>
void shadePixel(PIXEL &pixel, const PIXEL &matte,
                const ToneHighlightSettings &settings) {
  if (pixel.m == 0) return;
  const double maxValue = PIXEL::maxChannelValue;
  double amount         = matte.m / maxValue;
  if (settings.invertMatte) amount = 1.0 - amount;
  amount *= settings.intensity;
  if (amount <= 0.0) return;

  const double alpha = pixel.m / maxValue;
  double r = pixel.r / maxValue / alpha, g = pixel.g / maxValue / alpha,
         b = pixel.b / maxValue / alpha;
  if (settings.protectLines) {
    const double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
    // Lines fade out of the shading over this luminance range.
    amount *= tcrop((luminance - settings.lineThreshold) / 0.1 + 0.5, 0.0, 1.0);
    if (amount <= 0.0) return;
  }

  const double colorR = settings.color.r / 255.0;
  const double colorG = settings.color.g / 255.0;
  const double colorB = settings.color.b / 255.0;
  double shadedR, shadedG, shadedB;
  if (settings.highlight) {
    shadedR = 1.0 - (1.0 - r) * (1.0 - colorR);
    shadedG = 1.0 - (1.0 - g) * (1.0 - colorG);
    shadedB = 1.0 - (1.0 - b) * (1.0 - colorB);
  } else {
    shadedR = r * colorR;
    shadedG = g * colorG;
    shadedB = b * colorB;
  }
  r += amount * (shadedR - r);
  g += amount * (shadedG - g);
  b += amount * (shadedB - b);

  const double premult = alpha * maxValue;
  storeChannels(pixel, r * premult, g * premult, b * premult);
}

template <class PIXEL>
void applyToneHighlight(const TRasterPT<PIXEL> &ras,
                        const TRasterPT<PIXEL> &matte,
                        const ToneHighlightSettings &settings) {
  ras->lock();
  matte->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    PIXEL *pixel          = ras->pixels(y);
    const PIXEL *mattePix = matte->pixels(y);
    for (int x = 0; x < ras->getLx(); ++x)
      shadePixel(pixel[x], mattePix[x], settings);
  }
  matte->unlock();
  ras->unlock();
}

double blurPixelBytes(int bpp) {
  return 4.0 * (bpp == 32 ? sizeof(float) : sizeof(double));
}

double workspaceBytes(double width, double height, double radius, int bpp) {
  const double pixelBytes   = bpp / 8.0;
  const double paddedWidth  = width + 2.0 * radius;
  const double paddedHeight = height + 2.0 * radius;
  const double source       = width * height * pixelBytes;
  const double matte        = paddedWidth * paddedHeight * pixelBytes;
  if (radius == 0.0) return source + matte;

  // TRop::blur keeps a four-channel float/double image and three scratch rows.
  const double blurPixel = blurPixelBytes(bpp);
  const double scratch   = (paddedWidth + 2.0 * radius) * pixelBytes +
                         (paddedHeight + 2.0 * radius) * blurPixel +
                         paddedHeight * pixelBytes;
  return source + matte + source + paddedWidth * paddedHeight * blurPixel +
         scratch;
}

TDimension checkedMatteSize(const TDimension &size, int radius, int bpp) {
  const double width  = double(size.lx) + 2.0 * radius;
  const double height = double(size.ly) + 2.0 * radius;
  // Raster allocation multiplies dimensions as signed ints, including the
  // byte-wide grayscale raster used for the blur's float/double workspace.
  const double bytes =
      radius > 0 ? std::max(bpp / 8.0, blurPixelBytes(bpp)) : bpp / 8.0;
  if (width <= 0.0 || height <= 0.0 ||
      width * height * bytes > std::numeric_limits<int>::max())
    throw TException(
        "Tone / Highlight: padded tile exceeds raster limits; "
        "reduce render tile size or Softness");
  return TDimension(static_cast<int>(width), static_cast<int>(height));
}

}  // namespace

//------------------------------------------------------------------------------

class ToneHighlightFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ToneHighlightFx)

  TRasterFxPort m_source, m_matte;
  TIntEnumParamP m_mode;
  TPixelParamP m_color;
  TDoubleParamP m_intensity;
  TDoubleParamP m_softness;
  TBoolParamP m_protectLines;
  TDoubleParamP m_lineThreshold;
  TBoolParamP m_invertMatte;

public:
  enum { TONE, HIGHLIGHT };

  ToneHighlightFx()
      : m_mode(new TIntEnumParam(TONE, "Tone"))
      , m_color(TPixel32(90, 80, 140))
      , m_intensity(0.5)
      , m_softness(0.0)
      , m_protectLines(true)
      , m_lineThreshold(0.25)
      , m_invertMatte(false) {
    m_mode->addItem(HIGHLIGHT, "Highlight");
    m_color->enableMatte(false);
    m_intensity->setValueRange(0.0, 1.0);
    m_softness->setMeasureName("fxLength");
    m_softness->setValueRange(0, (std::numeric_limits<double>::max)());
    m_lineThreshold->setValueRange(0.0, 1.0);

    bindParam(this, "mode", m_mode);
    bindParam(this, "color", m_color);
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "softness", m_softness);
    bindParam(this, "protectLines", m_protectLines);
    bindParam(this, "lineThreshold", m_lineThreshold);
    bindParam(this, "invertMatte", m_invertMatte);

    addInputPort("Source", m_source);
    addInputPort("Matte", m_matte);
    enableComputeInFloat(true);
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_source.isConnected()) return m_source->doGetBBox(frame, bBox, info);
    bBox = TRectD();
    return false;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    // The matte blur is isotropic, as in BlurFx
    return isPassThrough(frame) || m_softness->getValue(frame) == 0 ||
           isAlmostIsotropic(info.m_affine);
  }

  double getSoftness(double frame, const TRenderSettings &info) {
    const double softness =
        fabs(m_softness->getValue(frame)) * sqrt(fabs(info.m_affine.det()));
    const double radius = std::ceil(softness);
    if (!std::isfinite(radius))
      throw TException(
          "Tone / Highlight: non-finite blur radius; "
          "reduce Softness or render scale");
    // A triangular kernel this narrow samples only its center. Using TRop::blur
    // introduces cancellation or reciprocal overflow for small subpixel radii.
    if (softness <= 1.0) return 0.0;
    // Subdivision cannot remove the halo around even a single output pixel.
    // Bound it before integer conversion and before TRop::blur squares it.
    const int budgetMiB = std::min(std::max(info.m_maxTileSize, 1), 256);
    if (workspaceBytes(1.0, 1.0, radius, info.m_bpp) >
        double(budgetMiB) * 1024.0 * 1024.0)
      throw TException("Tone / Highlight: minimum blur workspace exceeds the " +
                       std::to_string(budgetMiB) +
                       " MiB render limit; reduce Softness or render scale");
    return softness;
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected()) return;
    if (isPassThrough(frame)) {
      m_source->dryCompute(rect, frame, info);
      return;
    }
    const double radius = std::ceil(getSoftness(frame, info));
    TRectD matteRect    = rect.enlarge(radius);
    m_source->dryCompute(rect, frame, info);
    m_matte->dryCompute(matteRect, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    if (!m_source.isConnected() || isPassThrough(frame) || rect.isEmpty())
      return 0;
    const double radius = std::ceil(getSoftness(frame, info));
    const double bytes =
        workspaceBytes(std::ceil(rect.getLx()) + 1.0,
                       std::ceil(rect.getLy()) + 1.0, radius, info.m_bpp);
    const double total = std::ceil(bytes / (1024.0 * 1024.0));
    if (!std::isfinite(total) || total >= std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    return static_cast<int>(total);
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

private:
  bool isPassThrough(double frame) const {
    return !m_matte.isConnected() || m_intensity->getValue(frame) <= 0.0;
  }
};

//------------------------------------------------------------------------------

void ToneHighlightFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &ri) {
  if (!m_source.isConnected()) return;
  if (isPassThrough(frame)) {
    m_source->compute(tile, frame, ri);
    return;
  }

  // Compute the matte over the tile, blurred by the softness
  const TDimension size      = tile.getRaster()->getSize();
  const double softness      = getSoftness(frame, ri);
  const int radius           = static_cast<int>(std::ceil(softness));
  const TDimension matteSize = checkedMatteSize(size, radius, ri.m_bpp);
  m_source->compute(tile, frame, ri);

  TRasterP matte;
  if (radius > 0) {
    TTile matteTile;
    m_matte->allocateAndCompute(matteTile, tile.m_pos - TPointD(radius, radius),
                                matteSize, tile.getRaster(), frame, ri);
    matte = tile.getRaster()->create(size.lx, size.ly);
    matte->clear();
    TRop::blur(matte, matteTile.getRaster(), softness, -radius, -radius, false);
  } else {
    TTile matteTile;
    m_matte->allocateAndCompute(matteTile, tile.m_pos, size, tile.getRaster(),
                                frame, ri);
    matte = matteTile.getRaster();
  }

  const ToneHighlightSettings settings = {
      m_mode->getValue() == HIGHLIGHT,  m_color->getValue(frame),
      m_intensity->getValue(frame),     m_protectLines->getValue(),
      m_lineThreshold->getValue(frame), m_invertMatte->getValue()};

  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  TRasterFP rasF   = tile.getRaster();
  if (ras32)
    applyToneHighlight<TPixel32>(ras32, matte, settings);
  else if (ras64)
    applyToneHighlight<TPixel64>(ras64, matte, settings);
  else if (rasF)
    applyToneHighlight<TPixelF>(rasF, matte, settings);
  else
    throw TException("Tone / Highlight: unsupported pixel type");
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(ToneHighlightFx, "toneHighlightFx")
