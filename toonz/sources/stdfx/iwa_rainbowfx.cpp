#include "iwa_rainbowfx.h"
#include "iwa_cie_d65.h"
#include "iwa_xyz.h"
#include "iwa_rainbow_intensity.h"
#include "tparamuiconcept.h"
#include "trop.h"

//--------------------------------------------------------------
double Iwa_RainbowFx::getSizePixelAmount(const double val,
                                         const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect;
  vect.x = val;
  vect.y = 0.0;
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return sqrt(vect.x * vect.x + vect.y * vect.y);
}

//------------------------------------------------------------

void Iwa_RainbowFx::buildRainbowColorMap(double3* core, double3* wide,
                                         double intensity, double inside,
                                         double secondary, bool doClamp) {
  auto clamp01 = [](double val) { return std::min(1.0, std::max(0.0, val)); };

  int mapSize[2] = {301, 91};
  // secondary rainbow : gradually darken from 133 to 136 degrees
  double secondary_grad_range[2] = {133, 136};
  // supernumerary rainbow inside the primary rainbow : gradually darken from
  // 139.75 to 139.2 degrees
  double inside_grad_width    = 0.57;
  double inside_grad_start[2] = {139.75, 139.2};

  for (int m = 0; m < 2; m++) {
    double3* out_p = (m == 0) ? core : wide;
    double xyz_sum[3];
    for (int a = 0; a < mapSize[m]; a++, out_p++) {
      double angle = (m == 0) ? 120.0 + (double)a * 0.1 : 90.0 + (double)a;

      double second_ratio = 1.0;
      if (angle <= secondary_grad_range[0]) {
        second_ratio = secondary;
      } else if (angle < secondary_grad_range[1]) {
        double r = (angle - (double)secondary_grad_range[0]) /
                   (double)(secondary_grad_range[1] - secondary_grad_range[0]);
        second_ratio = (1.0 - r) * secondary + r;
      }

      xyz_sum[0] = 0.0;
      xyz_sum[1] = 0.0;
      xyz_sum[2] = 0.0;
      // sum for each wavelength (in the range of visible light, 380nm-710nm)
      for (int ram = 0; ram < 34; ram++) {
        double start =
            inside_grad_start[0] +
            (inside_grad_start[1] - inside_grad_start[0]) * (double)ram / 33.0;
        double end          = start + inside_grad_width;
        double inside_ratio = 1.0;
        if (angle >= end) {
          inside_ratio = inside;
        } else if (angle > start) {
          double r     = (angle - start) / inside_grad_width;
          inside_ratio = (1.0 - r) + r * inside;
        }

        double* data =
            (m == 0) ? &rainbow_core_data[a][0] : &rainbow_wide_data[a][0];
        // accumulate XYZ channel values
        for (int c = 0; c < 3; c++)
          xyz_sum[c] +=
              cie_d65[ram] * data[ram] * xyz[ram * 3 + c] * inside_ratio;
      }
      double tmp_intensity = intensity * 25000.0 * second_ratio;
      out_p->r             = (3.240479 * xyz_sum[0] - 1.537150 * xyz_sum[1] -
                  0.498535 * xyz_sum[2]) *
                 tmp_intensity;
      out_p->g = (-0.969256 * xyz_sum[0] + 1.875992 * xyz_sum[1] +
                  0.041556 * xyz_sum[2]) *
                 tmp_intensity;
      out_p->b = (0.055648 * xyz_sum[0] - 0.204043 * xyz_sum[1] +
                  1.057311f * xyz_sum[2]) *
                 tmp_intensity;
      if (doClamp) {
        out_p->r = clamp01(out_p->r);
        out_p->g = clamp01(out_p->g);
        out_p->b = clamp01(out_p->b);
      }
    }
  }
}

//------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_RainbowFx::setOutputRaster(const RASTER ras, TDimensionI dim,
                                    double3* outBuf_p) {
  bool withAlpha = m_alpha_rendering->getValue();
  double maxi    = static_cast<double>(PIXEL::maxChannelValue);  // 255or65535
  double3* out_p = outBuf_p;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL* pix = ras->pixels(j);
    for (int i = 0; i < dim.lx; i++, out_p++, pix++) {
      pix->r = (typename PIXEL::Channel)(out_p->r * (maxi + 0.999999));
      pix->g = (typename PIXEL::Channel)(out_p->g * (maxi + 0.999999));
      pix->b = (typename PIXEL::Channel)(out_p->b * (maxi + 0.999999));

      if (withAlpha) {
        double chan_a = std::max(std::max(out_p->r, out_p->g), out_p->b);
        pix->m        = (typename PIXEL::Channel)(chan_a * (maxi + 0.999999));
      } else
        pix->m = (typename PIXEL::Channel)(PIXEL::maxChannelValue);
    }
  }
}

template <>
void Iwa_RainbowFx::setOutputRaster<TRasterFP, TPixelF>(const TRasterFP ras,
                                                        TDimensionI dim,
                                                        double3* outBuf_p) {
  bool withAlpha = m_alpha_rendering->getValue();
  double3* out_p = outBuf_p;
  for (int j = 0; j < dim.ly; j++) {
    TPixelF* pix = ras->pixels(j);
    for (int i = 0; i < dim.lx; i++, out_p++, pix++) {
      pix->r = (float)(out_p->r);
      pix->g = (float)(out_p->g);
      pix->b = (float)(out_p->b);

      if (withAlpha)
        pix->m = std::max(std::max(pix->r, pix->g), pix->b);
      else
        pix->m = 1.f;
    }
  }
}

//------------------------------------------------------------

Iwa_RainbowFx::Iwa_RainbowFx()
    : m_center(TPointD(0.0, 0.0))
    , m_radius(200.0)
    , m_intensity(1.0)
    , m_width_scale(1.0)
    , m_inside(1.0)
    , m_secondary_rainbow(1.0)
    , m_alpha_rendering(false) {
  // Fx Version 1: *2.2 Gamma when linear rendering (it duplicately applies
  // gamma and is not correct) Fx Version 2: *1/2.2 Gamma when non-linear
  // rendering
  setFxVersion(2);

  bindParam(this, "center", m_center);
  bindParam(this, "radius", m_radius);
  bindParam(this, "intensity", m_intensity);
  bindParam(this, "width_scale", m_width_scale);
  bindParam(this, "inside", m_inside);
  bindParam(this, "secondary_rainbow", m_secondary_rainbow);
  bindParam(this, "alpha_rendering", m_alpha_rendering);

  m_radius->setValueRange(0.0, std::numeric_limits<double>::max());
  m_intensity->setValueRange(0.1, 10.0);
  m_inside->setValueRange(0.0, 1.0);
  m_secondary_rainbow->setValueRange(0.0, 10.0);
  m_width_scale->setValueRange(0.1, 50.0);

  enableComputeInFloat(true);
}

//------------------------------------------------------------

bool Iwa_RainbowFx::doGetBBox(double frame, TRectD& bBox,
                              const TRenderSettings& ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------

inline double3 Iwa_RainbowFx::angleToColor(double angle, double3* core,
                                           double3* wide) {
  // boundary conditions
  if (angle <= 90.0)
    return wide[0];
  else if (angle >= 180.0)
    return wide[90];

  // inside of the range of hi-res color table
  if (angle > 120.0 && angle < 150.0) {
    double tablePos = (angle - 120) / 0.1;
    int tableId     = (int)std::floor(tablePos);
    double ratio    = tablePos - (double)tableId;
    return {core[tableId].r * (1.0 - ratio) + core[tableId + 1].r * ratio,
            core[tableId].g * (1.0 - ratio) + core[tableId + 1].g * ratio,
            core[tableId].b * (1.0 - ratio) + core[tableId + 1].b * ratio};
  }

  // low-res color table
  double tablePos = (angle - 90) / 1.0;
  int tableId     = (int)std::floor(tablePos);
  double ratio    = tablePos - (double)tableId;
  return {wide[tableId].r * (1.0 - ratio) + wide[tableId + 1].r * ratio,
          wide[tableId].g * (1.0 - ratio) + wide[tableId + 1].g * ratio,
          wide[tableId].b * (1.0 - ratio) + wide[tableId + 1].b * ratio};
}

//------------------------------------------------------------
void Iwa_RainbowFx::doCompute(TTile& tile, double frame,
                              const TRenderSettings& ri) {
  // build raibow color map
  TRasterGR8P rainbowColorCore_ras(sizeof(double3) * 301, 1);
  rainbowColorCore_ras->lock();
  double3* rainbowColorCore_p = (double3*)rainbowColorCore_ras->getRawData();

  TRasterGR8P rainbowColorWide_ras(sizeof(double3) * 91, 1);
  rainbowColorWide_ras->lock();
  double3* rainbowColorWide_p = (double3*)rainbowColorWide_ras->getRawData();

  double intensity = m_intensity->getValue(frame);
  double inside    = m_inside->getValue(frame);
  double secondary = m_secondary_rainbow->getValue(frame);

  bool doClamp = (tile.getRaster()->getPixelSize() != 16);

  buildRainbowColorMap(rainbowColorCore_p, rainbowColorWide_p, intensity,
                       inside, secondary, doClamp);

  // convert center position to render region coordinate
  TAffine aff = ri.m_affine;
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TPointD dimOffset((float)dimOut.lx / 2.0f, (float)dimOut.ly / 2.0f);
  TPointD centerPos = m_center->getValue(frame);
  centerPos = aff * centerPos - (tile.m_pos + tile.getRaster()->getCenterD()) +
              dimOffset;

  // result image buffer
  TRasterGR8P outBuf_ras(sizeof(double3) * dimOut.lx * dimOut.ly, 1);
  outBuf_ras->lock();
  double3* outBuf_p = (double3*)outBuf_ras->getRawData();

  double theta_peak = 41.3;
  double peakRadius =
      getSizePixelAmount(m_radius->getValue(frame), ri.m_affine);
  double anglePerPixel = theta_peak / peakRadius;
  double widthScale    = m_width_scale->getValue(frame);
  double3* out_p       = outBuf_p;
  // loop for all pixels
  for (int y = 0; y < dimOut.ly; y++) {
    for (int x = 0; x < dimOut.lx; x++, out_p++) {
      // convert pixel distance from the center to scattering angle (degrees)
      double s     = x - centerPos.x;
      double t     = y - centerPos.y;
      double theta = std::sqrt(s * s + t * t) * anglePerPixel;
      double phi   = 180.0 - theta_peak + (theta_peak - theta) / widthScale;

      *out_p = angleToColor(phi, rainbowColorCore_p, rainbowColorWide_p);
    }
  }

  rainbowColorCore_ras->unlock();
  rainbowColorWide_ras->unlock();

  // convert to channel values
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  TRasterFP outRasF   = (TRasterFP)tile.getRaster();
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(outRas32, dimOut, outBuf_p);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(outRas64, dimOut, outBuf_p);
  else if (outRasF)
    setOutputRaster<TRasterFP, TPixelF>(outRasF, dimOut, outBuf_p);

  // modify gamma
  if (getFxVersion() == 1 && tile.getRaster()->isLinear()) {
    tile.getRaster()->setLinear(false);
    TRop::toLinearRGB(tile.getRaster(), ri.m_colorSpaceGamma);
  } else if (getFxVersion() >= 2 && !tile.getRaster()->isLinear()) {
    tile.getRaster()->setLinear(true);
    TRop::tosRGB(tile.getRaster(), ri.m_colorSpaceGamma);
  }

  outBuf_ras->unlock();
}

//------------------------------------------------------------

void Iwa_RainbowFx::getParamUIs(TParamUIConcept*& concepts, int& length) {
  concepts = new TParamUIConcept[length = 3];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Center";
  concepts[0].m_params.push_back(m_center);

  concepts[1].m_type  = TParamUIConcept::RADIUS;
  concepts[1].m_label = "Radius";
  concepts[1].m_params.push_back(m_radius);
  concepts[1].m_params.push_back(m_center);

  concepts[2].m_type  = TParamUIConcept::RAINBOW_WIDTH;
  concepts[2].m_label = "Width";
  concepts[2].m_params.push_back(m_width_scale);
  concepts[2].m_params.push_back(m_radius);
  concepts[2].m_params.push_back(m_center);
}

//------------------------------------------------------------

bool Iwa_RainbowFx::toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                                   bool tileIsLinear) const {
  return settingsIsLinear;
}

//==============================================================================

FX_PLUGIN_IDENTIFIER(Iwa_RainbowFx, "iwa_RainbowFx");
