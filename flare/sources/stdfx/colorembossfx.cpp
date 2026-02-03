

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelgr.h"

//===================================================================

class ColorEmbossFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ColorEmbossFx)

  TRasterFxPort m_input;
  TRasterFxPort m_controller;
  TDoubleParamP m_intensity;
  TDoubleParamP m_elevation;
  TDoubleParamP m_direction;
  TDoubleParamP m_radius;

public:
  ColorEmbossFx()
      : m_intensity(0.5), m_elevation(45.0), m_direction(90.0), m_radius(1.0) {
    m_radius->setMeasureName("fxLength");
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "elevation", m_elevation);
    bindParam(this, "direction", m_direction);
    bindParam(this, "radius", m_radius);
    addInputPort("Source", m_input);
    addInputPort("Controller", m_controller);
    m_intensity->setValueRange(0.0, 1.0, 0.1);
    m_elevation->setValueRange(0.0, 360.0);
    m_direction->setValueRange(0.0, 360.0);
    m_radius->setValueRange(0.0, 10.0);
  }

  ~ColorEmbossFx(){};

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      bool ret = m_input->doGetBBox(frame, bBox, info);
      return ret;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return (isAlmostIsotropic(info.m_affine));
  }
};

//-------------------------------------------------------------------
template <typename PIXEL, typename PIXELGRAY, typename CHANNEL_TYPE>
void doColorEmboss(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> srcraster,
                   TRasterPT<PIXEL> ctrraster, double azimuth, double elevation,
                   double intensity, double radius) {
  double Lx         = cos(azimuth) * cos(elevation) * PIXEL::maxChannelValue;
  double Ly         = sin(azimuth) * cos(elevation) * PIXEL::maxChannelValue;
  double Lz         = sin(elevation) * PIXEL::maxChannelValue;
  double Nz         = (6 * PIXEL::maxChannelValue) * (1 - intensity);
  double Nz2        = Nz * Nz;
  double background = Lz;
  double NdotL;
  int j, m, n;

  int border            = radius + 1;
  double borderFracMult = radius - (int)radius;

  int wrap = srcraster->getWrap();

  double nsbuffer, ewbuffer;
  double nsFracbuffer, ewFracbuffer;

  ras->lock();
  srcraster->lock();
  ctrraster->lock();
  for (j = border; j < srcraster->getLy() - border; j++) {
    PIXEL *pixout    = ras->pixels(j - border);
    PIXEL *pix       = srcraster->pixels(j) + border;
    PIXEL *ctrpix    = ctrraster->pixels(j) + border;
    PIXEL *endPixout = pixout + ras->getLx();

    while (pixout < endPixout) {
      double val, emboss;
      {
        nsbuffer = 0;
        ewbuffer = 0;
        for (m = 1; m < border; m++)
          for (n = -m; n <= m; n++) {
            nsbuffer += PIXELGRAY::from(*(ctrpix + m * wrap + n)).value;
            nsbuffer -= PIXELGRAY::from(*(ctrpix - m * wrap + n)).value;
            ewbuffer += PIXELGRAY::from(*(ctrpix + n * wrap + m)).value;
            ewbuffer -= PIXELGRAY::from(*(ctrpix + n * wrap - m)).value;
          }

        nsFracbuffer = 0;
        ewFracbuffer = 0;
        for (n = -m; n <= m; n++) {
          nsFracbuffer += PIXELGRAY::from(*(ctrpix + m * wrap + n)).value;
          nsFracbuffer -= PIXELGRAY::from(*(ctrpix - m * wrap + n)).value;
          ewFracbuffer += PIXELGRAY::from(*(ctrpix + n * wrap + m)).value;
          ewFracbuffer -= PIXELGRAY::from(*(ctrpix + n * wrap - m)).value;
        }
        nsbuffer += nsFracbuffer * borderFracMult;
        ewbuffer += ewFracbuffer * borderFracMult;
      }
      double Nx = ewbuffer;
      double Ny = nsbuffer;
      Nx        = Nx / radius;
      Ny        = Ny / radius;
      // val= 127+sinsin*nordsud(pix, wrap)+coscos*eastwest(pix, wrap);
      if (Nx == 0 && Ny == 0)
        emboss = background;
      else if ((NdotL = Nx * Lx + Ny * Ly + Nz * Lz) < 0)
        emboss = 0;
      else
        emboss    = NdotL / sqrt(Nx * Nx + Ny * Ny + Nz2);
      val         = emboss * pix->r / PIXEL::maxChannelValue;
      (pixout)->r = (val < PIXEL::maxChannelValue)
                        ? (val > 0 ? (CHANNEL_TYPE)val : 0)
                        : PIXEL::maxChannelValue;
      val         = emboss * pix->g / PIXEL::maxChannelValue;
      (pixout)->g = (val < PIXEL::maxChannelValue)
                        ? (val > 0 ? (CHANNEL_TYPE)val : 0)
                        : PIXEL::maxChannelValue;
      val         = emboss * pix->b / PIXEL::maxChannelValue;
      (pixout)->b = (val < PIXEL::maxChannelValue)
                        ? (val > 0 ? (CHANNEL_TYPE)val : 0)
                        : PIXEL::maxChannelValue;
      (pixout)->m = (pix)->m;
      pix++;
      pixout++;
      ctrpix++;
    }
  }
  ras->unlock();
  srcraster->unlock();
  ctrraster->unlock();
}

//-------------------------------------------------------------------

void ColorEmbossFx::transform(double frame, int port,
                              const TRectD &rectOnOutput,
                              const TRenderSettings &infoOnOutput,
                              TRectD &rectOnInput,
                              TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;

  double scale  = sqrt(fabs(infoOnOutput.m_affine.det()));
  double radius = m_radius->getValue(frame) * scale;
  int border    = radius + 1;

  rectOnInput = rectOnOutput.enlarge(border);
}

//-------------------------------------------------------------------

void ColorEmbossFx::doCompute(TTile &tile, double frame,
                              const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale     = sqrt(fabs(ri.m_affine.det()));
  double radius    = m_radius->getValue(frame) * scale;
  double direction = m_direction->getValue(frame);
  double elevation = (m_elevation->getValue(frame)) * M_PI_180;
  double intensity = m_intensity->getValue(frame);
  double azimuth   = direction * M_PI_180;

  int border = radius + 1;
  TRasterP srcRas =
      tile.getRaster()->create(tile.getRaster()->getLx() + border * 2,
                               tile.getRaster()->getLy() + border * 2);
  // TRaster32P srcRas(tile.getRaster()->getLx() + border*2,
  // tile.getRaster()->getLy() + border*2);
  TTile srcTile(srcRas, tile.m_pos - TPointD(border, border));

  TTile ctrTile;

  m_input->compute(srcTile, frame, ri);
  if (!m_controller.isConnected()) {
    ctrTile.m_pos = srcTile.m_pos;
    ctrTile.setRaster(srcTile.getRaster());
  } else {
    ctrTile.m_pos = tile.m_pos - TPointD(border, border);
    ctrTile.setRaster(
        tile.getRaster()->create(tile.getRaster()->getLx() + border * 2,
                                 tile.getRaster()->getLy() + border * 2));
    m_controller->allocateAndCompute(ctrTile, ctrTile.m_pos,
                                     ctrTile.getRaster()->getSize(),
                                     ctrTile.getRaster(), frame, ri);
    // m_controller->compute(ctrTile,frame,ri);
  }
  TRaster32P raster32    = tile.getRaster();
  TRaster32P srcraster32 = srcTile.getRaster();
  TRaster32P ctrraster32 = ctrTile.getRaster();
  if (raster32)
    doColorEmboss<TPixel32, TPixelGR8, UCHAR>(raster32, srcraster32,
                                              ctrraster32, azimuth, elevation,
                                              intensity, radius);
  else {
    TRaster64P raster64    = tile.getRaster();
    TRaster64P srcraster64 = srcTile.getRaster();
    TRaster64P ctrraster64 = ctrTile.getRaster();
    if (raster64)
      doColorEmboss<TPixel64, TPixelGR16, USHORT>(raster64, srcraster64,
                                                  ctrraster64, azimuth,
                                                  elevation, intensity, radius);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

int ColorEmbossFx::getMemoryRequirement(const TRectD &rect, double frame,
                                        const TRenderSettings &info) {
  double scale  = sqrt(fabs(info.m_affine.det()));
  double radius = m_radius->getValue(frame) * scale;
  int border    = radius + 1;

  return TRasterFx::memorySize(rect.enlarge(border), info.m_bpp);
}

FX_PLUGIN_IDENTIFIER(ColorEmbossFx, "colorEmbossFx");
