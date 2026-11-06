#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tparamuiconcept.h"
#include "tpixelutils.h"
#include "trop.h"

#include "lightshading.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

//==============================================================================
//    Light Shading Fx
//
// Cel-style shading driven by a light, in the manner of Toon Boom Harmony's
// Light Shading, Volume, Glue and Shine nodes.
// Like Tone / Highlight it only changes the colour of the Source: the alpha is
// kept, so the shading stays inside the drawing.
//
//  - Radial: the light intensity falls from 1 at the Light Point to 0 at the
//    Light Radius; the part of the drawing near the light is lit, the far part
//    is in shadow.
//  - Directional ("Volume"): a 2D normal is estimated from the gradient of the
//    blurred alpha of the shape (the Matte if connected, else the Source), and
//    lit with N.L for the Light Angle. The edges that face the light are lit
//    and those that face away are in shadow, as on a rounded volume; the
//    inside of the shape stays neutral. A larger Softness rounds the volume
//    more.
//  - The intensity is cut into Bands flat cel levels on each side of a neutral
//    middle band. One band gives one shadow shape and one light shape ("Shine"
//    is the light side, a highlight screened over the paint). The Softness
//    blurs the edge between the bands; 0 is a hard cel edge.
//  - The Matte (Glue) limits the region: it takes the place of the Source's
//    alpha for the shape and multiplies the shading.
//
// Shadow multiplies the colour with the Shade Color, light screens the
// Highlight Color over it. Dark line pixels can be left untouched.
//
// Limits: CPU only, and the normals come from the alpha of one image; there is
// no depth information, so an overlapping part of the drawing is not shaded
// differently from its neighbours.
//==============================================================================

namespace {

// Everything the pixel loop needs, in the pixels of the margined source tile
struct LightShadingSettings {
  bool m_directional;
  int m_bands;
  TPointD m_center;     // radial: the light point
  double m_radius;      // radial: the light radius
  TPointD m_direction;  // directional: the direction to the light
  int m_volumeRadius;   // directional: the box blur radius of the alpha
  int m_softRadius;     // box blur radius of the shading levels
  int m_margin;         // pixels of margin around the output tile
  double m_shadeColor[3], m_highlightColor[3];
  double m_shadeAmount, m_highlightAmount;
  bool m_protectLines;
  LightShadingSettings()
      : m_directional(false)
      , m_bands(2)
      , m_radius(0.0)
      , m_volumeRadius(0)
      , m_softRadius(0)
      , m_margin(0)
      , m_shadeAmount(0.0)
      , m_highlightAmount(0.0)
      , m_protectLines(true) {
    for (int i = 0; i < 3; ++i) m_shadeColor[i] = m_highlightColor[i] = 0.0;
  }
};

// Pixels beyond the box blurs of the shape, and of the levels
int shadingMargin(const LightShadingSettings &s) {
  return (s.m_directional ? 2 * s.m_volumeRadius + 1 : 0) + 2 * s.m_softRadius;
}

//------------------------------------------------------------------------------

template <class PIXEL>
void applyLightShading(const TRasterPT<PIXEL> &out, const TRasterPT<PIXEL> &src,
                       const TRasterPT<PIXEL> &matte,
                       const LightShadingSettings &s) {
  const double maxValue = PIXEL::maxChannelValue;
  // Lines fade out of the shading over this luminance range, as in
  // Tone / Highlight
  const double lineThreshold = 0.25, lineRamp = 0.1;

  const int w = src->getLx(), h = src->getLy();
  const size_t count = (size_t)w * h;

  src->lock();
  if (matte) matte->lock();

  // The region shaped by the light: the Matte if there is one, else the Source
  std::vector<float> region(count);
  for (int y = 0; y < h; ++y) {
    const PIXEL *pix = matte ? matte->pixels(y) : src->pixels(y);
    float *dst       = &region[(size_t)y * w];
    for (int x = 0; x < w; ++x, ++pix) dst[x] = (float)(pix->m / maxValue);
  }

  // The signed shading level of every pixel
  std::vector<float> level(count, 0.0f), scratch;
  if (!s.m_directional) {
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        double dx = x + 0.5 - s.m_center.x, dy = y + 0.5 - s.m_center.y;
        level[(size_t)y * w + x] = (float)lightshading::quantizeLevel(
            lightshading::radialIntensity(std::sqrt(dx * dx + dy * dy),
                                          s.m_radius),
            s.m_bands);
      }
  } else {
    // The outward normal is minus the gradient of the blurred shape. The
    // gradient of a straight edge is at most 1 / (2r + 1) for the two box
    // passes, which gives a normal of length 1.
    std::vector<float> blurred(region);
    lightshading::boxBlur(blurred, w, h, s.m_volumeRadius, scratch);
    lightshading::boxBlur(blurred, w, h, s.m_volumeRadius, scratch);
    const double gain = 2.0 * s.m_volumeRadius + 1.0;
    for (int y = 0; y < h; ++y) {
      const int y0 = std::max(y - 1, 0), y1 = std::min(y + 1, h - 1);
      for (int x = 0; x < w; ++x) {
        const int x0 = std::max(x - 1, 0), x1 = std::min(x + 1, w - 1);
        double gx =
            (blurred[(size_t)y * w + x1] - blurred[(size_t)y * w + x0]) /
            (double)(x1 - x0);
        double gy =
            (blurred[(size_t)y1 * w + x] - blurred[(size_t)y0 * w + x]) /
            (double)(y1 - y0);
        level[(size_t)y * w + x] = (float)lightshading::quantizeLevel(
            lightshading::directionalIntensity(
                -gx * gain, -gy * gain, s.m_direction.x, s.m_direction.y),
            s.m_bands);
      }
    }
  }

  // The softness blurs the edges between the levels; the levels are weighted
  // by the region so that its border does not fade them
  if (s.m_softRadius > 0) {
    std::vector<float> weight(region);
    for (size_t i = 0; i < count; ++i) level[i] *= region[i];
    for (int pass = 0; pass < 2; ++pass) {
      lightshading::boxBlur(level, w, h, s.m_softRadius, scratch);
      lightshading::boxBlur(weight, w, h, s.m_softRadius, scratch);
    }
    for (size_t i = 0; i < count; ++i)
      level[i] = weight[i] > 1e-3f ? level[i] / weight[i] : 0.0f;
  }

  // Shade the output tile, which lies in the middle of the margined source
  out->lock();
  for (int y = 0; y < out->getLy(); ++y) {
    const size_t row = (size_t)(y + s.m_margin) * w + s.m_margin;
    const PIXEL *pix = src->pixels(y + s.m_margin) + s.m_margin;
    const PIXEL *matteRow =
        matte ? matte->pixels(y + s.m_margin) + s.m_margin : (const PIXEL *)0;
    PIXEL *dst = out->pixels(y);
    for (int x = 0; x < out->getLx(); ++x, ++pix, ++dst) {
      *dst = *pix;
      if (pix->m == 0) continue;

      const double lvl = level[row + x];
      double amount    = lightshading::shadingAmount(lvl, s.m_shadeAmount,
                                                     s.m_highlightAmount);
      if (matteRow) amount *= matteRow[x].m / maxValue;
      if (amount <= 0.0) continue;

      // Work on straight colors
      double alpha = pix->m / maxValue;
      double r = pix->r / maxValue / alpha, g = pix->g / maxValue / alpha,
             b = pix->b / maxValue / alpha;

      if (s.m_protectLines) {
        double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
        amount *= tcrop((luminance - lineThreshold) / lineRamp + 0.5, 0.0, 1.0);
        if (amount <= 0.0) continue;
      }

      double shadedR, shadedG, shadedB;
      if (lvl > 0.0) {  // screen
        shadedR = 1.0 - (1.0 - r) * (1.0 - s.m_highlightColor[0]);
        shadedG = 1.0 - (1.0 - g) * (1.0 - s.m_highlightColor[1]);
        shadedB = 1.0 - (1.0 - b) * (1.0 - s.m_highlightColor[2]);
      } else {  // multiply
        shadedR = r * s.m_shadeColor[0];
        shadedG = g * s.m_shadeColor[1];
        shadedB = b * s.m_shadeColor[2];
      }
      r += amount * (shadedR - r);
      g += amount * (shadedG - g);
      b += amount * (shadedB - b);

      // Back to premultiplied channels; alpha is unchanged
      double premult = alpha * maxValue;
      dst->r =
          (typename PIXEL::Channel)tcrop(tround(r * premult), 0, (int)maxValue);
      dst->g =
          (typename PIXEL::Channel)tcrop(tround(g * premult), 0, (int)maxValue);
      dst->b =
          (typename PIXEL::Channel)tcrop(tround(b * premult), 0, (int)maxValue);
    }
  }
  out->unlock();
  if (matte) matte->unlock();
  src->unlock();
}

}  // namespace

//------------------------------------------------------------------------------

class LightShadingFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(LightShadingFx)

  TRasterFxPort m_source, m_matte;
  TIntEnumParamP m_mode;
  TPointParamP m_lightPoint;
  TDoubleParamP m_lightRadius, m_lightAngle;
  TPixelParamP m_shadeColor, m_highlightColor;
  TDoubleParamP m_shadeAmount, m_highlightAmount;
  TIntParamP m_bands;
  TDoubleParamP m_softness;
  TBoolParamP m_protectLines;

public:
  enum { RADIAL, DIRECTIONAL };

  LightShadingFx()
      : m_mode(new TIntEnumParam(RADIAL, "Radial"))
      , m_lightPoint(TPointD(0.0, 0.0))
      , m_lightRadius(150.0)
      , m_lightAngle(135.0)
      , m_shadeColor(TPixel32(90, 80, 140))
      , m_highlightColor(TPixel32(255, 236, 200))
      , m_shadeAmount(0.5)
      , m_highlightAmount(0.35)
      , m_bands(2)
      , m_softness(0.0)
      , m_protectLines(true) {
    m_mode->addItem(DIRECTIONAL, "Directional");
    m_lightPoint->getX()->setMeasureName("fxLength");
    m_lightPoint->getY()->setMeasureName("fxLength");
    m_lightRadius->setMeasureName("fxLength");
    m_lightRadius->setValueRange(0, (std::numeric_limits<double>::max)());
    m_lightAngle->setMeasureName("angle");
    m_shadeColor->enableMatte(false);
    m_highlightColor->enableMatte(false);
    m_shadeAmount->setValueRange(0.0, 1.0);
    m_highlightAmount->setValueRange(0.0, 1.0);
    m_bands->setValueRange(1, 6);
    m_softness->setMeasureName("fxLength");
    m_softness->setValueRange(0, (std::numeric_limits<double>::max)());

    bindParam(this, "mode", m_mode);
    bindParam(this, "lightPoint", m_lightPoint);
    bindParam(this, "lightRadius", m_lightRadius);
    bindParam(this, "lightAngle", m_lightAngle);
    bindParam(this, "shadeColor", m_shadeColor);
    bindParam(this, "highlightColor", m_highlightColor);
    bindParam(this, "shadeAmount", m_shadeAmount);
    bindParam(this, "highlightAmount", m_highlightAmount);
    bindParam(this, "bands", m_bands);
    bindParam(this, "softness", m_softness);
    bindParam(this, "protectLines", m_protectLines);

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
    // The blurs are isotropic, as in BlurFx
    return isAlmostIsotropic(info.m_affine);
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 2];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Light Point";
    concepts[0].m_params.push_back(m_lightPoint);

    concepts[1].m_type  = TParamUIConcept::RADIUS;
    concepts[1].m_label = "Light Radius";
    concepts[1].m_params.push_back(m_lightRadius);
    concepts[1].m_params.push_back(m_lightPoint);
  }

  // The settings for a render; the point and radius are in the tile's pixels
  // once the tile is known, see doCompute
  void getSettings(LightShadingSettings &s, double frame,
                   const TRenderSettings &ri) {
    const double scale    = sqrt(fabs(ri.m_affine.det()));
    const double softness = fabs(m_softness->getValue(frame)) * scale;

    s.m_directional = m_mode->getValue() == DIRECTIONAL;
    s.m_bands       = m_bands->getValue();
    s.m_radius      = m_lightRadius->getValue(frame) * scale;
    // A volume of a few pixels is the least that gives a normal; softer
    // shading rounds it more
    s.m_volumeRadius = s.m_directional ? tceil(8.0 * scale + softness) : 0;
    s.m_softRadius   = tceil(softness);
    s.m_margin       = shadingMargin(s);

    double angle = m_lightAngle->getValue(frame) * M_PI_180;
    TPointD dir  = ri.m_affine.transformDirection(
         TPointD(std::cos(angle), std::sin(angle)));
    s.m_direction = dir;

    const TPixel32 shade  = m_shadeColor->getValue(frame);
    const TPixel32 light  = m_highlightColor->getValue(frame);
    s.m_shadeColor[0]     = shade.r / 255.0;
    s.m_shadeColor[1]     = shade.g / 255.0;
    s.m_shadeColor[2]     = shade.b / 255.0;
    s.m_highlightColor[0] = light.r / 255.0;
    s.m_highlightColor[1] = light.g / 255.0;
    s.m_highlightColor[2] = light.b / 255.0;
    s.m_shadeAmount       = m_shadeAmount->getValue(frame);
    s.m_highlightAmount   = m_highlightAmount->getValue(frame);
    s.m_protectLines      = m_protectLines->getValue();
  }

  int getMargin(double frame, const TRenderSettings &info) {
    LightShadingSettings s;
    getSettings(s, frame, info);
    return s.m_margin;
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_source.isConnected()) return;
    TRectD marginRect = rect.enlarge(getMargin(frame, info));
    m_source->dryCompute(marginRect, frame, info);
    if (m_matte.isConnected()) m_matte->dryCompute(marginRect, frame, info);
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    const TRectD area = rect.enlarge(getMargin(frame, info));
    // The source tile, and the float planes of the shading (region, level,
    // scratch, blurred and weight), which the renderer must count to split
    // large tiles
    const double planes = std::ceil(area.getLx()) * std::ceil(area.getLy()) *
                          5.0 * sizeof(float) / (1024.0 * 1024.0);
    const double total =
        double(TRasterFx::memorySize(area, info.m_bpp)) + std::ceil(planes);
    return int(std::min(total, double(std::numeric_limits<int>::max())));
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};

//------------------------------------------------------------------------------

void LightShadingFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &ri) {
  if (!m_source.isConnected()) return;

  LightShadingSettings s;
  getSettings(s, frame, ri);
  // Nothing to shade
  if (s.m_shadeAmount <= 0.0 && s.m_highlightAmount <= 0.0) {
    m_source->compute(tile, frame, ri);
    return;
  }

  // The source and the matte over the tile and its margin
  const TDimension size = tile.getRaster()->getSize();
  const TPointD origin  = tile.m_pos - TPointD(s.m_margin, s.m_margin);
  const TDimension bigSize(size.lx + 2 * s.m_margin, size.ly + 2 * s.m_margin);

  TTile sourceTile;
  m_source->allocateAndCompute(sourceTile, origin, bigSize, tile.getRaster(),
                               frame, ri);
  TTile matteTile;
  if (m_matte.isConnected())
    m_matte->allocateAndCompute(matteTile, origin, bigSize, tile.getRaster(),
                                frame, ri);

  // The light point, in the pixels of the margined tile
  s.m_center = ri.m_affine * m_lightPoint->getValue(frame) - origin;

  const TRasterP matte =
      m_matte.isConnected() ? matteTile.getRaster() : TRasterP();

  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  if (ras32)
    applyLightShading<TPixel32>(ras32, sourceTile.getRaster(), matte, s);
  else if (ras64)
    applyLightShading<TPixel64>(ras64, sourceTile.getRaster(), matte, s);
  else
    throw TException("Light Shading: unsupported pixel type");
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(LightShadingFx, "lightShadingFx")
