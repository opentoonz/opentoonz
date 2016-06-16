

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
//===================================================================

class EmbossFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(EmbossFx)

  TRasterFxPort m_input;
  TDoubleParamP m_intensity;
  TDoubleParamP m_elevation;
  TDoubleParamP m_direction;
  TDoubleParamP m_radius;

public:
  EmbossFx()
      : m_intensity(0.5), m_elevation(45.0), m_direction(90.0), m_radius(1.0) {
    m_radius->setMeasureName("fxLength");
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "elevation", m_elevation);
    bindParam(this, "direction", m_direction);
    bindParam(this, "radius", m_radius);
    addInputPort("Source", m_input);
    m_intensity->setValueRange(0.0, 1.0, 0.1);
    m_elevation->setValueRange(0.0, 360.0);
    m_direction->setValueRange(0.0, 360.0);
    m_radius->setValueRange(0.0, 10.0);
  }

  ~EmbossFx(){};

  bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info) {
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
                 TRenderSettings &infoOnInput);

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri);
  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info);

  bool canHandle(const TRenderSettings &info, double frame) {
    return (isAlmostIsotropic(info.m_affine));
  }
};

//-------------------------------------------------------------------
template <typename PIXEL, typename PIXELGRAY, typename CHANNEL_TYPE>
void doEmboss(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> srcraster, double azimuth,
              double elevation, double intensity, double radius) {
  double Lx  = cos(azimuth) * cos(elevation) * PIXEL::maxChannelValue;
  double Ly  = sin(azimuth) * cos(elevation) * PIXEL::maxChannelValue;
  double Lz  = sin(elevation) * PIXEL::maxChannelValue;
  double Nz  = (6 * PIXEL::maxChannelValue) * (1 - intensity);
  double Nz2 = Nz * Nz;
  int j, m, n;

  int border            = (int)radius + 1;
  double borderFracMult = radius - (int)radius;

  int wrap = srcraster->getWrap();

  double nsbuffer, ewbuffer;
  double nsFracbuffer, ewFracbuffer;
  double NdotL;
  double background = Lz;
  srcraster->lock();
  ras->lock();
  for (j = border; j < srcraster->getLy() - border; j++) {
    PIXEL *pixout    = ras->pixels(j - border);
    PIXEL *pix       = srcraster->pixels(j) + border;
    PIXEL *endPixout = pixout + ras->getLx();
    while (pixout < endPixout) {
      double val;

      // Explanation: The fx is performed by calculating kind of a normal vector
      // N for
      // the discrete surface generated by the {(x,y,v)} of the raster, where v
      // is the
      // grey tone of the pixel at (x,y).
      // This vector is then dot-product-matched with the input light ray
      // vector, and a pixel
      // value according with the product is then returned.

      // Here, we build up the sums for the X and Y components of N.
      nsbuffer = 0;
      ewbuffer = 0;
      for (m = 1; m < border; m++)
        for (n = -m; n <= m; n++) {
          nsbuffer += PIXELGRAY::from(*(pix + m * wrap + n)).value;
          nsbuffer -= PIXELGRAY::from(*(pix - m * wrap + n)).value;
          ewbuffer += PIXELGRAY::from(*(pix + n * wrap + m)).value;
          ewbuffer -= PIXELGRAY::from(*(pix + n * wrap - m)).value;
        }

      // As the fx radius (aka border) is a double, we make its fractionary part
      // count less. This ensures continuity of the result against radius
      // changes.
      nsFracbuffer = 0;
      ewFracbuffer = 0;
      for (n = -m; n <= m; n++) {
        nsFracbuffer += PIXELGRAY::from(*(pix + m * wrap + n)).value;
        nsFracbuffer -= PIXELGRAY::from(*(pix - m * wrap + n)).value;
        ewFracbuffer += PIXELGRAY::from(*(pix + n * wrap + m)).value;
        ewFracbuffer -= PIXELGRAY::from(*(pix + n * wrap - m)).value;
      }
      nsbuffer += nsFracbuffer * borderFracMult;
      ewbuffer += ewFracbuffer * borderFracMult;

      // Here the dot-product is performed and the result is finally returned.
      double Nx = ewbuffer;
      double Ny = nsbuffer;
      Nx        = Nx / radius;
      Ny        = Ny / radius;
      // val= 127+sinsin*nordsud(pix, wrap)+coscos*eastwest(pix, wrap);
      if (Nx == 0 && Ny == 0)
        val = background;
      else if ((NdotL = Nx * Lx + Ny * Ly + Nz * Lz) < 0)
        val = 0;
      else
        val       = NdotL / sqrt(Nx * Nx + Ny * Ny + Nz2);
      (pixout)->r = (val < PIXEL::maxChannelValue)
                        ? (val > 0 ? (CHANNEL_TYPE)val : 0)
                        : PIXEL::maxChannelValue;
      (pixout)->g = pixout->r;
      (pixout)->b = pixout->r;
      (pixout)->m = (pix)->m;
      *pixout     = premultiply(*pixout);
      *pix++;
      *pixout++;
    }
  }

  srcraster->unlock();
  ras->unlock();
}

//-------------------------------------------------------------------

void EmbossFx::doCompute(TTile &tile, double frame, const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double min, max, step;
  m_radius->getValueRange(min, max, step);

  double scale     = sqrt(fabs(ri.m_affine.det()));
  double radius    = tcrop(m_radius->getValue(frame), min, max) * scale;
  double direction = (m_direction->getValue(frame));
  double elevation = (m_elevation->getValue(frame)) * M_PI_180;
  double intensity = m_intensity->getValue(frame);
  double azimuth   = direction * M_PI_180;

  // NOTE: This enlargement is perhaps needed in the calculation of the fx - but
  // no output will
  // be generated for it - so there is no trace of it in the doGetBBox
  // function...
  int border = radius + 1;
  TRasterP srcRas =
      tile.getRaster()->create(tile.getRaster()->getLx() + border * 2,
                               tile.getRaster()->getLy() + border * 2);
  TTile srcTile(srcRas, tile.m_pos - TPointD(border, border));

  m_input->compute(srcTile, frame, ri);
  TRaster32P raster32    = tile.getRaster();
  TRaster32P srcraster32 = srcTile.getRaster();

  if (raster32)
    doEmboss<TPixel32, TPixelGR8, UCHAR>(raster32, srcraster32, azimuth,
                                         elevation, intensity, radius);
  else {
    TRaster64P raster64    = tile.getRaster();
    TRaster64P srcraster64 = srcTile.getRaster();
    if (raster64)
      doEmboss<TPixel64, TPixelGR16, USHORT>(raster64, srcraster64, azimuth,
                                             elevation, intensity, radius);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

//------------------------------------------------------------------

void EmbossFx::transform(double frame, int port, const TRectD &rectOnOutput,
                         const TRenderSettings &infoOnOutput,
                         TRectD &rectOnInput, TRenderSettings &infoOnInput) {
  infoOnInput = infoOnOutput;

  double min, max, step;
  m_radius->getValueRange(min, max, step);

  double scale  = sqrt(fabs(infoOnOutput.m_affine.det()));
  double radius = (tcrop(m_radius->getValue(frame), min, max) * scale);
  int border    = radius + 1;

  rectOnInput = rectOnOutput.enlarge(border);
}

//------------------------------------------------------------------

int EmbossFx::getMemoryRequirement(const TRectD &rect, double frame,
                                   const TRenderSettings &info) {
  double scale  = sqrt(fabs(info.m_affine.det()));
  double radius = m_radius->getValue(frame) * scale;
  int border    = radius + 1;

  return TRasterFx::memorySize(rect.enlarge(border), info.m_bpp);
}

FX_PLUGIN_IDENTIFIER(EmbossFx, "embossFx");
