

#include "stdfx.h"
#include "tfxparam.h"
//#include "trop.h"
#include "tpixelutils.h"
#include "trasterfx.h"
#include "tparamset.h"

//==================================================================

class TargetSpotFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(TargetSpotFx)

  TDoubleParamP m_z;
  TDoubleParamP m_angle;
  TDoubleParamP m_decay;
  TDoubleParamP m_sizex;
  TDoubleParamP m_sizey;
  TPixelParamP m_color;

public:
  TargetSpotFx()
      : m_z(100)                // args, "Z")
      , m_angle(10)             // args, "Angle")
      , m_decay(.01)            // args, "Angle")
      , m_sizex(1)              // args, "SizeX")
      , m_sizey(1)              // args, "sizeY")
      , m_color(TPixel::White)  // args, "Color")
  {
    bindParam(this, "z", m_z);
    bindParam(this, "angle", m_angle);
    bindParam(this, "decay", m_decay);
    bindParam(this, "sizeX", m_sizex);
    bindParam(this, "sizeY", m_sizey);
    bindParam(this, "color", m_color);
    m_decay->setValueRange(0.0, 1.0);
    m_sizex->setValueRange(0, (std::numeric_limits<double>::max)());
    m_sizey->setValueRange(0, (std::numeric_limits<double>::max)());
    m_z->setValueRange(0, (std::numeric_limits<double>::max)());
    m_angle->setMeasureName("angle");
  }
  ~TargetSpotFx(){};

  bool doGetBBox(double, TRectD &bBox, const TRenderSettings &info) override {
    bBox = TConsts::infiniteRectD;
    return true;  // si potrebbe/dovrebbe fare meglio
  };

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return false;
  }
};

template <typename PIXEL>
void doTargetSpot(const TRasterPT<PIXEL> &ras, TPixel32 m_color0, double sizex,
                  double sizey, double angle, double decay, double z,
                  TPointD tilepos) {
  PIXEL color1 = PIXEL::Black;
  PIXEL color0 = PixelConverter<PIXEL>::from(m_color0);

  double normx     = 1 / sizex;
  double normy     = 1 / sizey;
  double reference = 5 * z;
  angle            = angle * M_PI_180;
  double ttan;
  ttan = tan(angle);

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD pos = tilepos;
    pos.y += j;
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double dist, norm;

      pos.x += 1.0;
      const double ztmp(-ttan * pos.x + z);
      const double zz(ztmp * ztmp);
      const double distxy(pos.x * pos.x * normx + pos.y * pos.y * normy);
      dist = distxy - zz;
      norm = sqrt(distxy + zz);
      if (norm < 0) norm *= -1;
      if (dist < 0 && (-ttan * pos.x + z) > 0) {
        *pix++ =
            blend(color0, color1,
                  norm * decay > 1 ? 1 : (norm * decay < 0 ? 0 : norm * decay));
      } else if (dist < reference && ztmp > 0) {
        PIXEL tmp =
            blend(color0, color1,
                  norm * decay > 1 ? 1 : (norm * decay < 0 ? 0 : norm * decay));
        *pix++ = blend(tmp, color1, dist / (reference));
      } else
        *pix++ = color1;
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------

void TargetSpotFx::doCompute(TTile &tile, double frame,
                             const TRenderSettings &ri) {
  double z     = m_z->getValue(frame) * ri.m_affine.a11;
  double angle = m_angle->getValue(frame);
  double sizex = m_sizex->getValue(frame) / (ri.m_shrinkX * ri.m_shrinkX);
  double sizey = m_sizey->getValue(frame) / (ri.m_shrinkX * ri.m_shrinkX);
  const TPixel32 m_color0 = m_color->getPremultipliedValue(frame);
  double decay            = m_decay->getValue(frame) / 100;

  TPointD pos = tile.m_pos;

  TRaster32P raster32 = tile.getRaster();
  if (raster32)
    doTargetSpot<TPixel32>(raster32, m_color0, sizex, sizey, angle, decay, z,
                           pos);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doTargetSpot<TPixel64>(raster64, m_color0, sizex, sizey, angle, decay, z,
                             pos);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(TargetSpotFx, "targetSpotFx")
