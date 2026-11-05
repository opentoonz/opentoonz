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

template <class PIXEL>
void applyToneHighlight(const TRasterPT<PIXEL> &ras,
                        const TRasterPT<PIXEL> &matte, bool highlight,
                        const TPixel32 &color, double intensity,
                        bool protectLines, double lineThreshold,
                        bool invertMatte) {
  const double maxValue = PIXEL::maxChannelValue;
  const double colorR = color.r / 255.0, colorG = color.g / 255.0,
               colorB = color.b / 255.0;
  // Lines fade out of the shading over this luminance range.
  const double lineRamp = 0.1;

  ras->lock();
  matte->lock();
  for (int y = 0; y < ras->getLy(); ++y) {
    PIXEL *pix            = ras->pixels(y);
    const PIXEL *mattePix = matte->pixels(y);
    for (int x = 0; x < ras->getLx(); ++x, ++pix, ++mattePix) {
      if (pix->m == 0) continue;

      double amount = mattePix->m / maxValue;
      if (invertMatte) amount = 1.0 - amount;
      amount *= intensity;
      if (amount <= 0.0) continue;

      // Work on straight colors
      double alpha = pix->m / maxValue;
      double r = pix->r / maxValue / alpha, g = pix->g / maxValue / alpha,
             b = pix->b / maxValue / alpha;

      if (protectLines) {
        double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
        amount *= tcrop((luminance - lineThreshold) / lineRamp + 0.5, 0.0, 1.0);
        if (amount <= 0.0) continue;
      }

      double shadedR, shadedG, shadedB;
      if (highlight) {  // screen
        shadedR = 1.0 - (1.0 - r) * (1.0 - colorR);
        shadedG = 1.0 - (1.0 - g) * (1.0 - colorG);
        shadedB = 1.0 - (1.0 - b) * (1.0 - colorB);
      } else {  // multiply
        shadedR = r * colorR;
        shadedG = g * colorG;
        shadedB = b * colorB;
      }
      r += amount * (shadedR - r);
      g += amount * (shadedG - g);
      b += amount * (shadedB - b);

      // Back to premultiplied channels; alpha is unchanged
      double premult = alpha * maxValue;
      pix->r =
          (typename PIXEL::Channel)tcrop(tround(r * premult), 0, (int)maxValue);
      pix->g =
          (typename PIXEL::Channel)tcrop(tround(g * premult), 0, (int)maxValue);
      pix->b =
          (typename PIXEL::Channel)tcrop(tround(b * premult), 0, (int)maxValue);
    }
  }
  matte->unlock();
  ras->unlock();
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
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_source.isConnected()) return m_source->doGetBBox(frame, bBox, info);
    bBox = TRectD();
    return false;
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    // The matte blur is isotropic, as in BlurFx
    return m_softness->getValue(frame) == 0 || isAlmostIsotropic(info.m_affine);
  }

  int getSoftnessRadius(double frame, const TRenderSettings &info) {
    double softness =
        fabs(m_softness->getValue(frame)) * sqrt(fabs(info.m_affine.det()));
    return tceil(softness);
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected()) return;
    m_source->dryCompute(rect, frame, info);
    if (m_matte.isConnected()) {
      TRectD matteRect = rect.enlarge(getSoftnessRadius(frame, info));
      m_matte->dryCompute(matteRect, frame, info);
    }
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    return TRasterFx::memorySize(rect.enlarge(getSoftnessRadius(frame, info)),
                                 info.m_bpp);
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};

//------------------------------------------------------------------------------

void ToneHighlightFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &ri) {
  if (!m_source.isConnected()) return;
  m_source->compute(tile, frame, ri);

  double intensity = m_intensity->getValue(frame);
  if (!m_matte.isConnected() || intensity <= 0.0) return;

  // Compute the matte over the tile, blurred by the softness
  const TDimension size = tile.getRaster()->getSize();
  double softness =
      fabs(m_softness->getValue(frame)) * sqrt(fabs(ri.m_affine.det()));
  int radius = tceil(softness);

  TRasterP matte;
  if (radius > 0) {
    TTile matteTile;
    m_matte->allocateAndCompute(
        matteTile, tile.m_pos - TPointD(radius, radius),
        TDimension(size.lx + 2 * radius, size.ly + 2 * radius),
        tile.getRaster(), frame, ri);
    matte = tile.getRaster()->create(size.lx, size.ly);
    matte->clear();
    TRop::blur(matte, matteTile.getRaster(), softness, -radius, -radius, false);
  } else {
    TTile matteTile;
    m_matte->allocateAndCompute(matteTile, tile.m_pos, size, tile.getRaster(),
                                frame, ri);
    matte = matteTile.getRaster();
  }

  const bool highlight    = m_mode->getValue() == HIGHLIGHT;
  const TPixel32 color    = m_color->getValue(frame);
  const bool protectLines = m_protectLines->getValue();
  const double threshold  = m_lineThreshold->getValue(frame);
  const bool invertMatte  = m_invertMatte->getValue();

  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  if (ras32)
    applyToneHighlight<TPixel32>(ras32, matte, highlight, color, intensity,
                                 protectLines, threshold, invertMatte);
  else if (ras64)
    applyToneHighlight<TPixel64>(ras64, matte, highlight, color, intensity,
                                 protectLines, threshold, invertMatte);
  else
    throw TException("Tone / Highlight: unsupported pixel type");
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(ToneHighlightFx, "toneHighlightFx")
