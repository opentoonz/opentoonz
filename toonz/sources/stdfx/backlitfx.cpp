

#include "texception.h"
#include "stdfx.h"
#include "tpixelutils.h"
#include "tbasefx.h"
#include "tfxparam.h"
#include "trop.h"

#include "tparamset.h"

//---------------------------------------------------------------------

static void backlit(TRaster32P lighted, TRaster32P light, TRaster32P out,
                    double blur, const TPixel &color, double fade,
                    double scale) {
  assert(light && lighted && out);

  assert(lighted->getSize() == light->getSize());
  assert(lighted->getSize() == out->getSize());

  int j;

  if (fade > 0.0) {
    assert(fade <= 1.0);
    light->lock();
    for (j = 0; j < light->getLy(); j++) {
      TPixel *pix    = light->pixels(j);
      TPixel *endPix = pix + light->getLx();

      while (pix < endPix) {
        if (pix->m > 0) {
          if (pix->m == 255) {
            pix->r = troundp(pix->r + fade * (color.r - pix->r));
            pix->g = troundp(pix->g + fade * (color.g - pix->g));
            pix->b = troundp(pix->b + fade * (color.b - pix->b));
            pix->m = troundp(pix->m + fade * (color.m - pix->m));
          } else {
            int val;
            double factor = pix->m / 255.0;
            val    = troundp(pix->r + fade * (color.r * factor - pix->r));
            pix->r = (val > 255) ? 255 : val;
            val    = troundp(pix->g + fade * (color.g * factor - pix->g));
            pix->g = (val > 255) ? 255 : val;
            val    = troundp(pix->b + fade * (color.b * factor - pix->b));
            pix->b = (val > 255) ? 255 : val;
            val    = troundp(pix->m + fade * (color.m * factor - pix->m));
            pix->m = (val > 255) ? 255 : val;
          }
        }
        ++pix;
      }
    }
    light->unlock();
  }

  TRop::ropout(light, lighted, light);

  if (blur > 0) {
    double _blur = blur * scale;
    int bi       = tceil(_blur);
    TRaster32P ras(light->getLx() + bi, light->getLy() + bi);

    TRect rect(light->getBounds() + TPoint(bi, bi));
    TRop::copy(ras->extract(rect), light);

    TRop::blur(out, ras, _blur, -bi, -bi, true);

    double ampl = 1.0 + blur / 15.0;
    out->lock();
    for (j = 0; j < out->getLy(); j++) {
      TPixel *pix    = out->pixels(j);
      TPixel *endPix = pix + out->getLx();
      while (pix < endPix) {
        if (pix->m != 0) {
          int val = troundp(pix->r * ampl);
          pix->r  = (val > 204) ? 204 : val;
          val     = troundp(pix->g * ampl);
          pix->g  = (val > 204) ? 204 : val;
          val     = troundp(pix->b * ampl);
          pix->b  = (val > 204) ? 204 : val;
          val     = troundp(pix->m * ampl);
          pix->m  = (val > 204) ? 204 : val;
        }
        pix++;
      }
    }
    out->unlock();
    TRop::over(out, lighted, out);
  } else
    TRop::over(out, lighted, light);
}

//---------------------------------------------------------------------

static void backlit(TRaster64P lighted, TRaster64P light, TRaster64P out,
                    double blur, const TPixel &color32, double fade,
                    double scale) {
  assert(light && lighted && out);

  assert(lighted->getSize() == light->getSize());
  assert(lighted->getSize() == out->getSize());

  int j;
  TPixel64 color = toPixel64(color32);

  if (fade > 0.0) {
    assert(fade <= 1.0);
    light->lock();
    for (j = 0; j < light->getLy(); j++) {
      TPixel64 *pix    = light->pixels(j);
      TPixel64 *endPix = pix + light->getLx();

      while (pix < endPix) {
        if (pix->m > 0) {
          if (pix->m == 65535) {
            pix->r = troundp(pix->r + fade * (color.r - pix->r));
            pix->g = troundp(pix->g + fade * (color.g - pix->g));
            pix->b = troundp(pix->b + fade * (color.b - pix->b));
            pix->m = troundp(pix->m + fade * (color.m - pix->m));
          } else {
            int val;
            double factor = pix->m / 65535.0;
            val    = troundp(pix->r + fade * (color.r * factor - pix->r));
            pix->r = (val > 65535) ? 65535 : val;
            val    = troundp(pix->g + fade * (color.g * factor - pix->g));
            pix->g = (val > 65535) ? 65535 : val;
            val    = troundp(pix->b + fade * (color.b * factor - pix->b));
            pix->b = (val > 65535) ? 65535 : val;
            val    = troundp(pix->m + fade * (color.m * factor - pix->m));
            pix->m = (val > 65535) ? 65535 : val;
          }
        }
        ++pix;
      }
    }
    light->unlock();
  }

  TRop::ropout(light, lighted, light);

  if (blur > 0) {
    double _blur = blur * scale;
    int bi       = tceil(_blur);
    TRaster64P ras(light->getLx() + bi, light->getLy() + bi);

    TRect rect(light->getBounds() + TPoint(bi, bi));
    TRop::copy(ras->extract(rect), light);

    TRop::blur(out, ras, _blur, -bi, -bi, false);

    double ampl = 1.0 + blur / 15.0;
    out->lock();
    int fac = (204 << 8) | 204;

    for (j = 0; j < out->getLy(); j++) {
      TPixel64 *pix    = out->pixels(j);
      TPixel64 *endPix = pix + out->getLx();
      while (pix < endPix) {
        if (pix->m != 0) {
          int val = troundp(pix->r * ampl);
          pix->r  = (val > fac) ? fac : val;
          val     = troundp(pix->g * ampl);
          pix->g  = (val > fac) ? fac : val;
          val     = troundp(pix->b * ampl);
          pix->b  = (val > fac) ? fac : val;
          val     = troundp(pix->m * ampl);
          pix->m  = (val > fac) ? fac : val;
        }
        pix++;
      }
    }
    out->unlock();
    TRop::over(out, lighted, out);
  } else
    TRop::over(out, lighted, light);
}

//---------------------------------------------------------------------

class BacklitFx final : public TBaseRasterFx {
  FX_DECLARATION(BacklitFx)
  TRasterFxPort m_lighted, m_light;
  TDoubleParamP m_value;
  TDoubleParamP m_fade;

  TPixelParamP m_color;

public:
  BacklitFx() : m_value(0.0), m_color(TPixel::White), m_fade(0.0) {
    m_color->enableMatte(true);
    m_value->setValueRange(0, (std::numeric_limits<double>::max)());
    m_fade->setValueRange(0.0, 100.0);
    bindParam(this, "value", m_value);
    bindParam(this, "color", m_color);
    bindParam(this, "fade", m_fade);

    addInputPort("Light", m_light);
    addInputPort("Source", m_lighted);
  }
  ~BacklitFx() {}
  bool doGetBBox(double frame, TRectD &bbox,
                 const TRenderSettings &info) override {
    if (getActiveTimeRegion().contains(frame)) {
      if (m_light.isConnected()) {
        if (m_lighted.isConnected()) {
          TRectD b0, b1;
          bool ret = m_light->doGetBBox(frame, b0, info);
          ret      = ret && m_lighted->doGetBBox(frame, b1, info);
          bbox     = b0.enlarge(tceil(m_value->getValue(frame))) + b1;
          return ret;
        } else {
          return m_light->doGetBBox(frame, bbox, info);
	}
      } else if (m_lighted.isConnected()) {
        return m_lighted->doGetBBox(frame, bbox, info);
      }
    }
    return false;
  }

  void doDryCompute(TRectD &rect, double frame,
                    const TRenderSettings &info) override {
    if (!m_light.isConnected()) return;
    if (!m_lighted.isConnected()) {
      m_light->dryCompute(rect, frame, info);
      return;
    }

    double value = m_value->getValue(frame);
    double scale = sqrt(fabs(info.m_affine.det()));
    int brad     = tceil(value * scale);

    TRectD inRect = rect.enlarge(brad);
    inRect = TRectD(tfloor(inRect.x0), tfloor(inRect.y0), tceil(inRect.x1),
                    tceil(inRect.y1));

    m_lighted->dryCompute(inRect, frame, info);
    m_light->dryCompute(inRect, frame, info);
  }

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &ri) override {
    if (!m_light.isConnected()) return;

    if (!m_lighted.isConnected()) {
      m_light->compute(tile, frame, ri);
      return;
    }

    double value = m_value->getValue(frame);
    double scale = sqrt(fabs(ri.m_affine.det()));
    int brad     = tceil(value * scale);

    TDimension tileSize(tile.getRaster()->getSize());
    TRectD rect(tile.m_pos, TDimensionD(tileSize.lx, tileSize.ly));

    // TRectD
    // rect(tile.m_pos,convert(tile.getRaster()->getBounds().getP11())+tile.m_pos);
    rect = rect.enlarge(brad);
    TRect rectI(tfloor(rect.x0), tfloor(rect.y0), tceil(rect.x1) - 1,
                tceil(rect.y1) - 1);
    // m_light->compute(tile, frame, ri);
    TTile srcTile;
    m_lighted->allocateAndCompute(srcTile, rect.getP00(), rectI.getSize(),
                                  tile.getRaster(), frame, ri);
    TTile ctrTile;
    m_light->allocateAndCompute(ctrTile, rect.getP00(), rectI.getSize(),
                                tile.getRaster(), frame, ri);
    if ((TRaster32P)srcTile.getRaster() && (TRaster32P)tile.getRaster())
      backlit((TRaster32P)srcTile.getRaster(), (TRaster32P)ctrTile.getRaster(),
              (TRaster32P)ctrTile.getRaster(), value, m_color->getValue(frame),
              m_fade->getValue(frame) / 100.0, scale);
    else if ((TRaster64P)srcTile.getRaster() && (TRaster64P)tile.getRaster())
      backlit((TRaster64P)srcTile.getRaster(), (TRaster64P)ctrTile.getRaster(),
              (TRaster64P)ctrTile.getRaster(), value, m_color->getValue(frame),
              m_fade->getValue(frame) / 100.0, scale);
    else
      throw TRopException("TRop::max invalid raster combination");
    tile.getRaster()->copy(ctrTile.getRaster(), TPoint(-brad, -brad));
  }

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override {
    double value = m_value->getValue(frame);
    double scale = sqrt(fabs(info.m_affine.det()));
    int brad     = tceil(value * scale);

    return TRasterFx::memorySize(rect.enlarge(brad), info.m_bpp);
  }
};

//==================================================================

FX_PLUGIN_IDENTIFIER(BacklitFx, "backlitFx")
//==================================================================
