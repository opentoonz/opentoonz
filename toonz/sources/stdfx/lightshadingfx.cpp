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
int shadingMargin(const LightShadingSettings& s) {
  return (s.m_directional ? 2 * s.m_volumeRadius + 1 : 0) + 2 * s.m_softRadius;
}

TDimension paddedTileSize(const TDimension& size, int margin) {
  const int maxMargin =
      (std::numeric_limits<int>::max() - std::max(size.lx, size.ly)) / 2;
  if (margin > maxMargin)
    throw TException(
        "Light Shading: padded tile dimensions exceed the raster "
        "limit; reduce the render tile size");
  return TDimension(size.lx + 2 * margin, size.ly + 2 * margin);
}

template <class PIXEL>
typename PIXEL::Channel shadingChannel(double value) {
  return (typename PIXEL::Channel)tcrop(tround(value), 0,
                                        (int)PIXEL::maxChannelValue);
}

template <>
TPixelF::Channel shadingChannel<TPixelF>(double value) {
  return static_cast<float>(value);
}

//------------------------------------------------------------------------------

void directionalLevels(std::vector<float>& level,
                       const std::vector<float>& region, const TDimension& size,
                       const LightShadingSettings& s) {
  const int w = size.lx, h = size.ly;
  // The outward normal is minus the gradient of the blurred shape. The
  // gradient of a straight edge is at most 1 / (2r + 1) for the two box
  // passes, which gives a normal of length 1.
  std::vector<float> blurred(region), scratch;
  lightshading::boxBlur(blurred, w, h, s.m_volumeRadius, scratch);
  lightshading::boxBlur(blurred, w, h, s.m_volumeRadius, scratch);
  const double gain = 2.0 * s.m_volumeRadius + 1.0;
  for (int y = 0; y < h; ++y) {
    const int y0 = std::max(y - 1, 0), y1 = std::min(y + 1, h - 1);
    for (int x = 0; x < w; ++x) {
      const int x0 = std::max(x - 1, 0), x1 = std::min(x + 1, w - 1);
      double gx = (blurred[(size_t)y * w + x1] - blurred[(size_t)y * w + x0]) /
                  (double)(x1 - x0);
      double gy = (blurred[(size_t)y1 * w + x] - blurred[(size_t)y0 * w + x]) /
                  (double)(y1 - y0);
      level[(size_t)y * w + x] = (float)lightshading::quantizeLevel(
          lightshading::directionalIntensity(-gx * gain, -gy * gain,
                                             s.m_direction.x, s.m_direction.y),
          s.m_bands);
    }
  }
}

void softenLevels(std::vector<float>& level, const std::vector<float>& region,
                  const TDimension& size, int radius) {
  if (radius <= 0) return;
  const size_t count = region.size();
  // The softness blurs the edges between the levels; the levels are weighted
  // by the region so that its border does not fade them
  std::vector<float> weight(region), scratch;
  for (size_t i = 0; i < count; ++i) level[i] *= region[i];
  for (int pass = 0; pass < 2; ++pass) {
    lightshading::boxBlur(level, size.lx, size.ly, radius, scratch);
    lightshading::boxBlur(weight, size.lx, size.ly, radius, scratch);
  }
  for (size_t i = 0; i < count; ++i)
    level[i] = weight[i] > 1e-3f ? level[i] / weight[i] : 0.0f;
}

template <class PIXEL>
PIXEL shadePixel(const PIXEL& pix, double lvl, double matteAlpha,
                 const LightShadingSettings& s) {
  PIXEL result = pix;
  if (pix.m == 0) return result;
  double amount =
      lightshading::shadingAmount(lvl, s.m_shadeAmount, s.m_highlightAmount) *
      matteAlpha;
  if (amount <= 0.0) return result;

  const double maxValue = PIXEL::maxChannelValue;
  const double alpha    = pix.m / maxValue;
  double r = pix.r / maxValue / alpha, g = pix.g / maxValue / alpha,
         b = pix.b / maxValue / alpha;
  if (s.m_protectLines) {
    const double lineThreshold = 0.25, lineRamp = 0.1;
    double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
    amount *= tcrop((luminance - lineThreshold) / lineRamp + 0.5, 0.0, 1.0);
    if (amount <= 0.0) return result;
  }
  double shadedR, shadedG, shadedB;
  if (lvl > 0.0) {
    shadedR = 1.0 - (1.0 - r) * (1.0 - s.m_highlightColor[0]);
    shadedG = 1.0 - (1.0 - g) * (1.0 - s.m_highlightColor[1]);
    shadedB = 1.0 - (1.0 - b) * (1.0 - s.m_highlightColor[2]);
  } else {
    shadedR = r * s.m_shadeColor[0];
    shadedG = g * s.m_shadeColor[1];
    shadedB = b * s.m_shadeColor[2];
  }
  r += amount * (shadedR - r);
  g += amount * (shadedG - g);
  b += amount * (shadedB - b);
  const double premult = alpha * maxValue;
  result.r             = shadingChannel<PIXEL>(r * premult);
  result.g             = shadingChannel<PIXEL>(g * premult);
  result.b             = shadingChannel<PIXEL>(b * premult);
  return result;
}

template <class PIXEL>
void shadeTile(const TRasterPT<PIXEL>& out, const TRasterPT<PIXEL>& src,
               const TRasterPT<PIXEL>& matte, const std::vector<float>& level,
               const LightShadingSettings& s) {
  const int w = src->getLx();
  // Shade the output tile, which lies in the middle of the margined source
  out->lock();
  for (int y = 0; y < out->getLy(); ++y) {
    const size_t row = (size_t)(y + s.m_margin) * w + s.m_margin;
    const PIXEL* pix = src->pixels(y + s.m_margin) + s.m_margin;
    const PIXEL* matteRow =
        matte ? matte->pixels(y + s.m_margin) + s.m_margin : (const PIXEL*)0;
    PIXEL* dst = out->pixels(y);
    for (int x = 0; x < out->getLx(); ++x, ++pix, ++dst) {
      const double matteAlpha =
          matteRow ? matteRow[x].m / double(PIXEL::maxChannelValue) : 1.0;
      *dst = shadePixel(*pix, level[row + x], matteAlpha, s);
    }
  }
  out->unlock();
}

template <class PIXEL>
std::vector<float> readRegion(const TRasterPT<PIXEL>& raster) {
  const int w = raster->getLx(), h = raster->getLy();
  const size_t count = (size_t)w * h;
  std::vector<float> region(count);
  raster->lock();
  for (int y = 0; y < h; ++y) {
    const PIXEL* pix = raster->pixels(y);
    float* dst       = &region[(size_t)y * w];
    for (int x = 0; x < w; ++x, ++pix)
      dst[x] = (float)(pix->m / double(PIXEL::maxChannelValue));
  }
  raster->unlock();
  return region;
}

template <class PIXEL>
void applyLightShading(const TRasterPT<PIXEL>& out, const TRasterPT<PIXEL>& src,
                       const TRasterPT<PIXEL>& matte,
                       const LightShadingSettings& s) {
  const int w = src->getLx(), h = src->getLy();
  std::vector<float> region = readRegion(matte ? matte : src);
  std::vector<float> level(region.size(), 0.0f);
  if (s.m_directional) {
    directionalLevels(level, region, src->getSize(), s);
  } else {
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        double dx = x + 0.5 - s.m_center.x, dy = y + 0.5 - s.m_center.y;
        level[(size_t)y * w + x] = (float)lightshading::quantizeLevel(
            lightshading::radialIntensity(std::sqrt(dx * dx + dy * dy),
                                          s.m_radius),
            s.m_bands);
      }
  }
  softenLevels(level, region, src->getSize(), s.m_softRadius);
  src->lock();
  if (matte) matte->lock();
  shadeTile(out, src, matte, level, s);
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
    enableComputeInFloat(true);
  }

  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& info) override {
    if (m_source.isConnected()) return m_source->doGetBBox(frame, bBox, info);
    bBox = TRectD();
    return false;
  }

  bool canHandle(const TRenderSettings& info, double frame) override {
    // The blurs are isotropic, as in BlurFx
    return isPassThrough(frame) || isAlmostIsotropic(info.m_affine);
  }

  void getParamUIs(TParamUIConcept*& concepts, int& length) override {
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
  void getSettings(LightShadingSettings& s, double frame,
                   const TRenderSettings& ri) {
    const double scale    = sqrt(fabs(ri.m_affine.det()));
    const double softness = fabs(m_softness->getValue(frame)) * scale;

    s.m_directional = m_mode->getValue() == DIRECTIONAL;
    s.m_bands       = m_bands->getValue();
    s.m_radius      = m_lightRadius->getValue(frame) * scale;
    // A volume of a few pixels is the least that gives a normal; softer
    // shading rounds it more
    const double volumeRadius =
        s.m_directional ? std::ceil(8.0 * scale + softness) : 0.0;
    const double softRadius = std::ceil(softness);
    const double margin =
        (s.m_directional ? 2.0 * volumeRadius + 1.0 : 0.0) + 2.0 * softRadius;
    const double side = 2.0 * margin + 1.0;
    // Subdivision cannot remove the halo around even one output pixel. Use
    // the requested tile budget, with a 256 MiB ceiling for untiled renders.
    const int budgetMiB = std::min(std::max(ri.m_maxTileSize, 1), 256);
    const double budget = double(budgetMiB) * 1024.0 * 1024.0;
    if (!std::isfinite(margin))
      throw TException(
          "Light Shading: non-finite blur radius; "
          "reduce Softness or render scale");
    if (side * side * workingBytesPerPixel(ri) > budget)
      throw TException("Light Shading: minimum blur workspace exceeds the " +
                       std::to_string(budgetMiB) +
                       " MiB render limit; reduce Softness or render scale");
    s.m_volumeRadius = static_cast<int>(volumeRadius);
    s.m_softRadius   = static_cast<int>(softRadius);
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

  int getMargin(double frame, const TRenderSettings& info) {
    if (isPassThrough(frame)) return 0;
    LightShadingSettings s;
    getSettings(s, frame, info);
    return s.m_margin;
  }

  void doDryCompute(TRectD& rect, double frame,
                    const TRenderSettings& info) override {
    if (!m_source.isConnected()) return;
    if (isPassThrough(frame)) {
      m_source->dryCompute(rect, frame, info);
      return;
    }
    TRectD marginRect = rect.enlarge(getMargin(frame, info));
    m_source->dryCompute(marginRect, frame, info);
    if (m_matte.isConnected()) m_matte->dryCompute(marginRect, frame, info);
  }

  int getMemoryRequirement(const TRectD& rect, double frame,
                           const TRenderSettings& info) override {
    if (!m_source.isConnected() || isPassThrough(frame) || rect.isEmpty())
      return 0;
    const TRectD area = rect.enlarge(getMargin(frame, info));
    // Region, level and scratch coexist with either blurred or weight, but
    // never both. The optional Matte raster remains live alongside Source.
    const double pixels =
        (std::ceil(area.getLx()) + 1.0) * (std::ceil(area.getLy()) + 1.0);
    const double total =
        std::ceil(pixels * workingBytesPerPixel(info) / (1024.0 * 1024.0));
    if (!std::isfinite(total) || total >= std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    return static_cast<int>(total);
  }

  void doCompute(TTile& tile, double frame, const TRenderSettings& ri) override;

private:
  double workingBytesPerPixel(const TRenderSettings& info) const {
    return (m_matte.isConnected() ? 2.0 : 1.0) * (info.m_bpp / 8.0) +
           4.0 * sizeof(float);
  }

  bool isPassThrough(double frame) const {
    return m_shadeAmount->getValue(frame) <= 0.0 &&
           m_highlightAmount->getValue(frame) <= 0.0;
  }
};

//------------------------------------------------------------------------------

void LightShadingFx::doCompute(TTile& tile, double frame,
                               const TRenderSettings& ri) {
  if (!m_source.isConnected()) return;

  if (isPassThrough(frame)) {
    m_source->compute(tile, frame, ri);
    return;
  }
  LightShadingSettings s;
  getSettings(s, frame, ri);

  // The source and the matte over the tile and its margin
  const TPointD origin = tile.m_pos - TPointD(s.m_margin, s.m_margin);
  const TDimension bigSize =
      paddedTileSize(tile.getRaster()->getSize(), s.m_margin);

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
  TRasterFP rasF   = tile.getRaster();
  if (ras32)
    applyLightShading<TPixel32>(ras32, sourceTile.getRaster(), matte, s);
  else if (ras64)
    applyLightShading<TPixel64>(ras64, sourceTile.getRaster(), matte, s);
  else if (rasF)
    applyLightShading<TPixelF>(rasF, sourceTile.getRaster(), matte, s);
  else
    throw TException("Light Shading: unsupported pixel type");
}

//------------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(LightShadingFx, "lightShadingFx")
