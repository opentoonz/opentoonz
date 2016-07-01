

#include "tfxparam.h"
#include "stdfx.h"
#include "tspectrumparam.h"
#include "tpixelutils.h"
#include "tparamuiconcept.h"

namespace {
inline void pixelConvert(TPixel32 &dst, const TPixel32 &src) { dst = src; }
inline void pixelConvert(TPixel64 &dst, const TPixel32 &src) {
  dst = toPixel64(src);
}
}  // namespace

class FourPointsGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(FourPointsGradientFx)
  TSpectrumParamP m_colors;
  TPointParamP m_point1;
  TPointParamP m_point2;
  TPointParamP m_point3;
  TPointParamP m_point4;
  TPixelParamP m_color1;
  TPixelParamP m_color2;
  TPixelParamP m_color3;
  TPixelParamP m_color4;

public:
  FourPointsGradientFx()
      : m_point1(TPointD(200.0, 200.0))
      , m_point2(TPointD(-200.0, 200.0))
      , m_point3(TPointD(-200.0, -200.0))
      , m_point4(TPointD(200.0, -200.0))
      , m_color1(TPixel32::Red)
      , m_color2(TPixel32::Green)
      , m_color3(TPixel32::Blue)
      , m_color4(TPixel32::Yellow) {
    m_point1->getX()->setMeasureName("fxLength");
    m_point1->getY()->setMeasureName("fxLength");
    m_point2->getX()->setMeasureName("fxLength");
    m_point2->getY()->setMeasureName("fxLength");
    m_point3->getX()->setMeasureName("fxLength");
    m_point3->getY()->setMeasureName("fxLength");
    m_point4->getX()->setMeasureName("fxLength");
    m_point4->getY()->setMeasureName("fxLength");
    /*
bindParam(this,"point1", m_point1);
bindParam(this,"point2", m_point2);
bindParam(this,"point3", m_point3);
bindParam(this,"point4", m_point4);
bindParam(this,"color_1", m_color1);
bindParam(this,"color_2", m_color2);
bindParam(this,"color_3", m_color3);
bindParam(this,"color_4", m_color4);
*/
    bindParam(this, "Point_1", m_point1);
    bindParam(this, "Color_1", m_color1);

    bindParam(this, "Point_2", m_point2);
    bindParam(this, "Color_2", m_color2);

    bindParam(this, "Point_3", m_point3);
    bindParam(this, "Color_3", m_color3);

    bindParam(this, "Point_4", m_point4);
    bindParam(this, "Color_4", m_color4);
  }
  ~FourPointsGradientFx(){};

  bool doGetBBox(double, TRectD &bbox, const TRenderSettings &info) override {
    bbox = TConsts::infiniteRectD;
    return true;
  };
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 4];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Point 1";
    concepts[0].m_params.push_back(m_point1);

    concepts[1].m_type  = TParamUIConcept::POINT;
    concepts[1].m_label = "Point 2";
    concepts[1].m_params.push_back(m_point2);

    concepts[2].m_type  = TParamUIConcept::POINT;
    concepts[2].m_label = "Point 3";
    concepts[2].m_params.push_back(m_point3);

    concepts[3].m_type  = TParamUIConcept::POINT;
    concepts[3].m_label = "Point 4";
    concepts[3].m_params.push_back(m_point4);
  }
};

template <typename PIXEL, typename CHANNEL_TYPE>
void doFourPointsGradient(const TRasterPT<PIXEL> &ras, TPointD tilepos,
                          TPointD pos1, TPointD pos2, TPointD pos3,
                          TPointD pos4, TPixel32 ccol1, TPixel32 ccol2,
                          TPixel32 ccol3, TPixel32 ccol4) {
  PIXEL col1, col2, col3, col4;
  pixelConvert(col1, ccol1);
  pixelConvert(col2, ccol2);
  pixelConvert(col3, ccol3);
  pixelConvert(col4, ccol4);

  int j;
  ras->lock();
  for (j = 0; j < ras->getLy(); j++) {
    TPointD pos = tilepos;
    pos.y += j;
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + ras->getLx();
    while (pix < endPix) {
      double d1 = sqrt((pos1.x - pos.x) * (pos1.x - pos.x) +
                       (pos1.y - pos.y) * (pos1.y - pos.y));
      if (!d1) {
        *pix = col1;
        pos.x += 1;
        ++pix;
        continue;
      }
      double d2 = sqrt((pos2.x - pos.x) * (pos2.x - pos.x) +
                       (pos2.y - pos.y) * (pos2.y - pos.y));
      if (!d2) {
        *pix = col2;
        pos.x += 1;
        ++pix;
        continue;
      }
      double d3 = sqrt((pos3.x - pos.x) * (pos3.x - pos.x) +
                       (pos3.y - pos.y) * (pos3.y - pos.y));
      if (!d3) {
        *pix = col3;
        pos.x += 1;
        ++pix;
        continue;
      }
      double d4 = sqrt((pos4.x - pos.x) * (pos4.x - pos.x) +
                       (pos4.y - pos.y) * (pos4.y - pos.y));
      if (!d4) {
        *pix = col4;
        pos.x += 1;
        ++pix;
        continue;
      }
      double dtotal = 1 / d1 + 1 / d2 + 1 / d3 + 1 / d4;
      pix->r        = (CHANNEL_TYPE)(
          ((1 / d1) / dtotal) * col1.r + ((1 / d2) / dtotal) * col2.r +
          ((1 / d3) / dtotal) * col3.r + ((1 / d4) / dtotal) * col4.r);
      pix->g = (CHANNEL_TYPE)(
          ((1 / d1) / dtotal) * col1.g + ((1 / d2) / dtotal) * col2.g +
          ((1 / d3) / dtotal) * col3.g + ((1 / d4) / dtotal) * col4.g);
      pix->b = (CHANNEL_TYPE)(
          ((1 / d1) / dtotal) * col1.b + ((1 / d2) / dtotal) * col2.b +
          ((1 / d3) / dtotal) * col3.b + ((1 / d4) / dtotal) * col4.b);
      pix->m = (CHANNEL_TYPE)(
          ((1 / d1) / dtotal) * col1.m + ((1 / d2) / dtotal) * col2.m +
          ((1 / d3) / dtotal) * col3.m + ((1 / d4) / dtotal) * col4.m);
      pos.x += 1.0;
      ++pix;
    }
  }
  ras->unlock();
}

void FourPointsGradientFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &ri) {
  TPointD pos1  = m_point1->getValue(frame) * (1.0 / ri.m_shrinkX);
  TPointD pos2  = m_point2->getValue(frame) * (1.0 / ri.m_shrinkX);
  TPointD pos3  = m_point3->getValue(frame) * (1.0 / ri.m_shrinkX);
  TPointD pos4  = m_point4->getValue(frame) * (1.0 / ri.m_shrinkX);
  pos1          = ri.m_affine * pos1;
  pos2          = ri.m_affine * pos2;
  pos3          = ri.m_affine * pos3;
  pos4          = ri.m_affine * pos4;
  TPixel32 col1 = m_color1->getPremultipliedValue(frame);
  TPixel32 col2 = m_color2->getPremultipliedValue(frame);
  TPixel32 col3 = m_color3->getPremultipliedValue(frame);
  TPixel32 col4 = m_color4->getPremultipliedValue(frame);

  TPointD pos         = tile.m_pos;
  TRaster32P raster32 = tile.getRaster();

  if (raster32)
    doFourPointsGradient<TPixel32, UCHAR>(raster32, pos, pos1, pos2, pos3, pos4,
                                          col1, col2, col3, col4);
  else {
    TRaster64P raster64 = tile.getRaster();
    if (raster64)
      doFourPointsGradient<TPixel64, USHORT>(raster64, pos, pos1, pos2, pos3,
                                             pos4, col1, col2, col3, col4);
    else
      throw TException("Brightness&Contrast: unsupported Pixel Type");
  }
}

FX_PLUGIN_IDENTIFIER(FourPointsGradientFx, "fourPointsGradientFx");
