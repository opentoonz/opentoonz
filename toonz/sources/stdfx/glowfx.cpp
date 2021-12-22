

#include "texception.h"
#include "stdfx.h"
#include "tpixelutils.h"
#include "tbasefx.h"
#include "tfxparam.h"
#include "trop.h"

#include "tparamset.h"

//===================================================================

//    Local namespace stuff

namespace {

enum Status {
  NoPortsConnected = 0,
  Port0Connected   = 1 << 1,
  Port1Connected   = 1 << 2,
  StatusGood       = Port0Connected | Port1Connected
};

//---------------------------------------------------------------------------

inline Status operator|(const Status &l, const Status &r) {
  return Status(((int)l | (int)r));
}

//---------------------------------------------------------------------------

inline int operator&(const Status &l) { return int(l); }

//---------------------------------------------------------------------------

Status getFxStatus(const TRasterFxPort &port0, const TRasterFxPort &port1) {
  Status status                   = NoPortsConnected;
  if (port0.isConnected()) status = status | Port0Connected;
  if (port1.isConnected()) status = status | Port1Connected;
  return status;
}

//---------------------------------------------------------------------------

void makeRectCoherent(TRectD &rect, const TPointD &pos) {
  rect -= pos;
  rect.x0 = tfloor(rect.x0);
  rect.y0 = tfloor(rect.y0);
  rect.x1 = tceil(rect.x1);
  rect.y1 = tceil(rect.y1);
  rect += pos;
}
}

//===================================================================

//========================
//    Glow functions
//------------------------

namespace {
template <typename T>
void fade(TRasterPT<T> ras, double fade, T color)  // Why it is not in TRop..??
{
  if (fade <= 0.0) return;

  double maxChannelValueD = T::maxChannelValue;

  assert(fade <= 1.0);
  ras->lock();
  for (int j = 0; j < ras->getLy(); j++) {
    T *pix    = ras->pixels(j);
    T *endPix = pix + ras->getLx();

    for (; pix < endPix; ++pix) {
      if (pix->m > 0) {
        if (pix->m == T::maxChannelValue) {
          pix->r = troundp(pix->r + fade * (color.r - pix->r));
          pix->g = troundp(pix->g + fade * (color.g - pix->g));
          pix->b = troundp(pix->b + fade * (color.b - pix->b));
          pix->m = troundp(pix->m + fade * (color.m - pix->m));
        } else {
          int val;
          double factor = pix->m / maxChannelValueD;

          val    = troundp(pix->r + fade * (color.r * factor - pix->r));
          pix->r = (val > T::maxChannelValue) ? T::maxChannelValue : val;

          val    = troundp(pix->g + fade * (color.g * factor - pix->g));
          pix->g = (val > T::maxChannelValue) ? T::maxChannelValue : val;

          val    = troundp(pix->b + fade * (color.b * factor - pix->b));
          pix->b = (val > T::maxChannelValue) ? T::maxChannelValue : val;

          val    = troundp(pix->m + fade * (color.m * factor - pix->m));
          pix->m = (val > T::maxChannelValue) ? T::maxChannelValue : val;
        }
      }
    }
  }

  ras->unlock();
}
}

//=====================================================================

//=================
//    Glow Fx
//-----------------

class GlowFx final : public TBaseRasterFx {
  FX_DECLARATION(GlowFx)

  TRasterFxPort m_lighted, m_light;
  TDoubleParamP m_value;
  TDoubleParamP m_brightness;
  TDoubleParamP m_fade;
  TPixelParamP m_color;

public:
  GlowFx()
      : m_value(10.0)
      , m_brightness(100.0)
      , m_color(TPixel::White)
      , m_fade(0.0) {
    m_value->setMeasureName("fxLength");
    m_color->enableMatte(true);
    m_value->setValueRange(0, (std::numeric_limits<double>::max)());
    m_brightness->setValueRange(0, (std::numeric_limits<double>::max)());
    m_fade->setValueRange(0.0, 100.0);
    bindParam(this, "value", m_value);
    bindParam(this, "brightness", m_brightness);
    bindParam(this, "color", m_color);
    bindParam(this, "fade", m_fade);

    addInputPort("Light", m_light);
    addInputPort("Source", m_lighted);
  }

  //---------------------------------------------------------------------------

  ~GlowFx() {}

  //---------------------------------------------------------------------------

  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (getActiveTimeRegion().contains(frame)) {
      if (m_light.isConnected()) {
        TRectD b0, b1;
        bool ret = m_light->doGetBBox(frame, b0, info);
        bbox     = b0.enlarge(tceil(m_value->getValue(frame)));
        if (m_lighted.isConnected()) {
          ret = ret && m_lighted->doGetBBox(frame, b1, info);
          bbox += b1;
        }
        return ret;
      } else if (m_lighted.isConnected()) {
        return m_lighted->doGetBBox(frame, bbox, info);
      }
    }

    return false;
  }

  //---------------------------------------------------------------------------

  inline void buildLightRects(const TRectD &tileRect, TRectD &inRect,
                              TRectD &outRect, double blur) {
    if (inRect !=
        TConsts::infiniteRectD)  // Could be, if the input light is a zerary Fx
      makeRectCoherent(inRect, tileRect.getP00());

    int blurI = tceil(blur);

    // It seems that the TRop::blur does wrong with these (cuts at the borders).
    // I don't know why - they would be best...
    // TRectD blurOutRect((lightRect).enlarge(blurI) * tileRect);
    // lightRect = ((tileRect).enlarge(blurI) * lightRect);

    // So we revert to the sum of the two
    outRect = inRect = ((tileRect).enlarge(blurI) * inRect) +
                       ((inRect).enlarge(blurI) * tileRect);
  }

  //---------------------------------------------------------------------------

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    Status status = getFxStatus(m_light, m_lighted);

    if (status & NoPortsConnected)
      // If no port, just do nothing :)
      return;

    // Calculate source
    if (status & Port1Connected) m_lighted->compute(tile, frame, ri);

    // Calculate light
    if (status & Port0Connected) {
      // Init light infos
      TDimension tileSize(tile.getRaster()->getSize());
      TRectD tileRect(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly));

      double scale = sqrt(fabs(ri.m_affine.det()));
      double blur  = m_value->getValue(frame) * scale;

      // Build the light interesting rect
      TRectD lightRect, blurOutRect;
      m_light->getBBox(frame, lightRect, ri);

      buildLightRects(tileRect, lightRect, blurOutRect, blur);

      if ((lightRect.getLx() <= 0) || (lightRect.getLy() <= 0)) return;
      if ((blurOutRect.getLx() <= 0) || (blurOutRect.getLy() <= 0)) return;

      // Calculate the light tile
      TTile lightTile;
      TDimension lightSize(tround(lightRect.getLx()),
                           tround(lightRect.getLy()));
      m_light->allocateAndCompute(lightTile, lightRect.getP00(), lightSize,
                                  tile.getRaster(), frame, ri);

      // Init glow parameters
      TPixel32 color    = m_color->getValue(frame);
      double brightness = m_brightness->getValue(frame) / 100.0;
      double fade       = m_fade->getValue(frame) / 100.0;

      // Now, apply the glow

      // First, deal with the fade
      {
        TRasterP light     = lightTile.getRaster();
        TRaster32P light32 = light;
        TRaster64P light64 = light;
        if (light32)
          ::fade(light32, fade, color);
        else if (light64)
          ::fade(light64, fade, toPixel64(color));
        else
          assert(false);
      }

      // Then, build the blur
      TRasterP blurOut;
      if (blur > 0) {
        // Build a temporary output to the blur
        {
          TRasterP light(lightTile.getRaster());

          blurOut = light->create(tround(blurOutRect.getLx()),
                                  tround(blurOutRect.getLy()));

          // Apply the blur. Please note that SSE2 should not be used for now -
          // I've seen it
          // doing strange things to the blur...
          TPointD displacement(lightRect.getP00() - blurOutRect.getP00());
          TRop::blur(blurOut, light, blur, tround(displacement.x),
                     tround(displacement.y), false);
        }
      } else
        blurOut = lightTile.getRaster();

      // Apply the rgbm scale
      TRop::rgbmScale(blurOut, blurOut, 1, 1, 1, brightness);

      // Apply the add
      {
        TRectD interestingRect(tileRect * blurOutRect);

        TRect interestingTileRect(tround(interestingRect.x0 - tileRect.x0),
                                  tround(interestingRect.y0 - tileRect.y0),
                                  tround(interestingRect.x1 - tileRect.x0) - 1,
                                  tround(interestingRect.y1 - tileRect.y0) - 1);
        TRect interestingBlurRect(
            tround(interestingRect.x0 - blurOutRect.x0),
            tround(interestingRect.y0 - blurOutRect.y0),
            tround(interestingRect.x1 - blurOutRect.x0) - 1,
            tround(interestingRect.y1 - blurOutRect.y0) - 1);

        if ((interestingTileRect.getLx() <= 0) ||
            (interestingTileRect.getLy() <= 0))
          return;
        if ((interestingBlurRect.getLx() <= 0) ||
            (interestingBlurRect.getLy() <= 0))
          return;

        TRasterP tileInterestRas(
            tile.getRaster()->extract(interestingTileRect));
        TRasterP blurInterestRas(blurOut->extract(interestingBlurRect));
        TRop::add(blurInterestRas, tileInterestRas, tileInterestRas);
      }
    }
  }

  //---------------------------------------------------------------------------

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    Status status = getFxStatus(m_light, m_lighted);
    if (status & NoPortsConnected) return;

    if (status & Port1Connected) m_lighted->dryCompute(rect, frame, info);

    if (status & Port0Connected) {
      double scale = sqrt(fabs(info.m_affine.det()));
      double blur  = m_value->getValue(frame) * scale;

      TRectD lightRect, blurOutRect;
      m_light->getBBox(frame, lightRect, info);

      buildLightRects(rect, lightRect, blurOutRect, blur);

      if ((lightRect.getLx() <= 0) || (lightRect.getLy() <= 0)) return;
      if ((blurOutRect.getLx() <= 0) || (blurOutRect.getLy() <= 0)) return;

      m_light->dryCompute(lightRect, frame, info);
    }
  }

  //---------------------------------------------------------------------------

  // Just like the blur
  bool canHandle(const TRenderSettings &info, double frame) override {
    if (m_light.isConnected())
      return (m_value->getValue(frame) == 0) ? true
                                             : isAlmostIsotropic(info.m_affine);

    return true;
  }

  //---------------------------------------------------------------------------

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    double scale = sqrt(fabs(info.m_affine.det()));
    double blur  = m_value->getValue(frame) * scale;

    return TRasterFx::memorySize(rect.enlarge(blur), info.m_bpp);
  }

  //---------------------------------------------------------------------------

  TFxPort *getXsheetPort() const override { return getInputPort(1); }
};

//==================================================================

FX_PLUGIN_IDENTIFIER(GlowFx, "glowFx")
//==================================================================
