

#include "stdfx.h"
#include "tfxparam.h"
#include "tparamset.h"
#include "tparamuiconcept.h"

class RadialBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RadialBlurFx)

  TRasterFxPort m_input;
  TPointParamP m_point;
  TDoubleParamP m_radius;
  TDoubleParamP m_blur;

public:
  RadialBlurFx() : m_point(TPointD(0.0, 0.0)), m_radius(0.0), m_blur(5.0) {
    m_point->getX()->setMeasureName("fxLength");
    m_point->getY()->setMeasureName("fxLength");
    m_radius->setMeasureName("fxLength");
    bindParam(this, "point", m_point);
    bindParam(this, "radius", m_radius);
    bindParam(this, "blur", m_blur);
    addInputPort("Source", m_input);
    m_radius->setValueRange(0, (std::numeric_limits<double>::max)());
    m_blur->setValueRange(0, (std::numeric_limits<double>::max)());
  }

  ~RadialBlurFx(){};

  int getMaxBraid(const TRectD &bBox, double frame,
                  const TAffine &aff = TAffine()) {
    double scale  = sqrt(fabs(aff.det()));
    TPointD point = aff * m_point->getValue(frame);
    double radius = m_radius->getValue(frame) * scale;
    double blur   = m_blur->getValue(frame);

    double intensity = blur * M_PI_180;

    TPointD p1 = bBox.getP00() - point;
    TPointD p2 = bBox.getP01() - point;
    TPointD p3 = bBox.getP10() - point;
    TPointD p4 = bBox.getP11() - point;
    double d1  = p1.x * p1.x + p1.y * p1.y;
    double d2  = p2.x * p2.x + p2.y * p2.y;
    double d3  = p3.x * p3.x + p3.y * p3.y;
    double d4  = p4.x * p4.x + p4.y * p4.y;

    double maxD = std::max(std::max(std::max(d3, d4), d2), d1);
    return tround(std::max(sqrt(maxD) - radius, 0.0)) * intensity;
  }

  void enlarge(const TRectD &bbox, TRectD &requestedGeom,
               const TRenderSettings &ri, double frame);

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected()) {
      m_input->doGetBBox(frame, bBox, info);
      bBox = bBox.enlarge(getMaxBraid(bBox, frame));
      return true;
    } else {
      bBox = TRectD();
      return false;
    }
  }

  void transform(double frame, int port, const TRectD &rectOnOutput,
                 const TRenderSettings &infoOnOutput, TRectD &rectOnInput,
                 TRenderSettings &infoOnInput) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override {
    if (info.m_isSwatch) return true;

    return m_blur->getValue(frame) == 0 ? true
                                        : isAlmostIsotropic(info.m_affine);
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 2];

    concepts[0].m_type  = TParamUIConcept::POINT;
    concepts[0].m_label = "Center";
    concepts[0].m_params.push_back(m_point);

    concepts[1].m_type  = TParamUIConcept::RADIUS;
    concepts[1].m_label = "Radius";
    concepts[1].m_params.push_back(m_radius);
    concepts[1].m_params.push_back(m_point);
  }
};

//------------------------------------------------------------------------------
template <typename PIXEL, typename CHANNEL_TYPE, int MAX_CHANNEL_VALUE>
void doRadialBlur(const TRasterPT<PIXEL> rout, const TRasterPT<PIXEL> rin,
                  double blur, double radius, TPointD point) {
  /*-センター位置-*/
  int dx = (int)point.x;
  int dy = (int)point.y;
  int i, j;
  /*- 出力サイズ -*/
  int lx = rout->getLx();
  int ly = rout->getLy();

  PIXEL *src_buf, *dst_buf;

  /*- チャンネル最大値(bppにより異なる) -*/
  double CROP_VAL         = (double)MAX_CHANNEL_VALUE;
  CHANNEL_TYPE U_CROP_VAL = MAX_CHANNEL_VALUE;

  double intensity = blur * M_PI_180;

  /*-出力サイズの画面の中の、ブラーのセンター位置-*/
  int cx = lx / 2 + dx;
  int cy = ly / 2 + dy;
  rin->lock();
  rout->lock();
  for (i = 0; i < ly; i++) {
    src_buf = rin->pixels(i);
    dst_buf = rout->pixels(i);
    for (j = 0; j < lx; j++, src_buf++, dst_buf++) {
      double valr = 0, valg = 0, valb = 0, valm = 0;
      double sinangle = 0, cosangle = 0;
      double angle = 0, dist, rangeinv = 0;
      int ii, vx, vy;
      int shiftx, shifty, range = 0, rangehalf;
      /*- ブラー中心→現在のピクセルへのベクトル -*/
      vx   = (int)(j - cx);
      vy   = (int)(i - cy);
      dist = sqrt((double)(vx * vx + vy * vy));
      /*- ブラーのかかる大きさ。（距離-radius）に比例 -*/
      range = (int)((dist - radius) * intensity);
      /*- ブラーが少しでもかかる場合 -*/
      if (range >= 1 && (dist - radius) > 0) {
        rangehalf = range / 2;
        /*- ブラーの角度 -*/
        angle    = atan2((double)vy, (double)vx);
        cosangle = cos(angle);
        if (vx)
          sinangle = cosangle * (vy / (float)vx);
        else
          sinangle = sin(angle);
        for (ii = 0; ii <= range; ii++) {
          shiftx = (int)((ii - rangehalf) * cosangle);
          shifty = (int)((ii - rangehalf) * sinangle);
          /*- 画面外にはみだす条件 -*/
          if ((j + shiftx) < 0) continue;    // shiftx=-j;
          if ((j + shiftx) >= lx) continue;  // shiftx=lx-j-1;
          if ((i + shifty) < 0) continue;    // shifty=-i;
          if ((i + shifty) >= ly) continue;  // shifty=ly-i-1;
          valr += rin->pixels(i + shifty)[j + shiftx].r;
          valg += rin->pixels(i + shifty)[j + shiftx].g;
          valb += rin->pixels(i + shifty)[j + shiftx].b;
          valm += rin->pixels(i + shifty)[j + shiftx].m;
        }
        rangeinv = 1.0 / (range + 1);
        valr *= rangeinv;
        valg *= rangeinv;
        valb *= rangeinv;
        valm *= rangeinv;
        dst_buf->r = (valr > CROP_VAL) ? U_CROP_VAL
                                       : ((valr < 0) ? 0 : (CHANNEL_TYPE)valr);
        dst_buf->g = (valg > CROP_VAL) ? U_CROP_VAL
                                       : ((valg < 0) ? 0 : (CHANNEL_TYPE)valg);
        dst_buf->b = (valb > CROP_VAL) ? U_CROP_VAL
                                       : ((valb < 0) ? 0 : (CHANNEL_TYPE)valb);
        dst_buf->m = (valm > CROP_VAL) ? U_CROP_VAL
                                       : ((valm < 0) ? 0 : (CHANNEL_TYPE)valm);
      } else {
        *(dst_buf) = *(src_buf);
      }
    }
  }

  rin->unlock();
  rout->unlock();
}

//------------------------------------------------------------------------------

//! Calculates the geometry we need for this node computation, given
//! the known input data (bbox) and the requested output (requestedGeom).
void RadialBlurFx::enlarge(const TRectD &bbox, TRectD &requestedGeom,
                           const TRenderSettings &ri, double frame) {
  TRectD enlargedBbox(bbox);
  TRectD enlargedGeom(requestedGeom);
  TPointD originalP00(requestedGeom.getP00());

  double maxRange = getMaxBraid(enlargedBbox, frame, ri.m_affine);

  /*- 最低でも1pixel追加する -*/
  maxRange = std::max(maxRange, 1.0);

  enlargedBbox = enlargedBbox.enlarge(maxRange);
  enlargedGeom = enlargedGeom.enlarge(maxRange);

  // We are to find out the geometry that is useful for the fx computation.
  // There are some rules to follow:
  //  a) First, the interesting output we can generate is bounded by both
  //     the requestedRect and the blurred bbox (i.e. enlarged by the blur
  //     radius).
  //  b) Pixels contributing to any output are necessarily part of bbox - and
  //  only
  //     those which are blurrable into the requestedRect are useful to us
  //     (i.e. pixels contained in its enlargement by the blur radius).

  requestedGeom = (enlargedGeom * bbox) + (enlargedBbox * requestedGeom);

  // Finally, make sure that the result is coherent with the original P00
  requestedGeom -= originalP00;
  requestedGeom.x0 = tfloor(requestedGeom.x0);
  requestedGeom.y0 = tfloor(requestedGeom.y0);
  requestedGeom.x1 = tceil(requestedGeom.x1);
  requestedGeom.y1 = tceil(requestedGeom.y1);
  requestedGeom += originalP00;
}

//------------------------------------------------------------------------------

void RadialBlurFx::transform(double frame, int port, const TRectD &rectOnOutput,
                             const TRenderSettings &infoOnOutput,
                             TRectD &rectOnInput,
                             TRenderSettings &infoOnInput) {
  TRectD rectOut(rectOnOutput);

  if (canHandle(infoOnOutput, frame))
    infoOnInput = infoOnOutput;
  else {
    infoOnInput          = infoOnOutput;
    infoOnInput.m_affine = TAffine();  // because the affine does not commute
    rectOut              = infoOnOutput.m_affine.inv() * rectOut;
  }

  TRectD bbox;
  m_input->getBBox(frame, bbox, infoOnInput);
  if (rectOnInput == TConsts::infiniteRectD) bbox = rectOut;

  rectOnInput = rectOut;
  enlarge(bbox, rectOnInput, infoOnInput, frame);
}

//------------------------------------------------------------------------------

void RadialBlurFx::doCompute(TTile &tile, double frame,
                             const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  double scale  = sqrt(fabs(ri.m_affine.det()));
  TPointD point = ri.m_affine * m_point->getValue(frame);
  double radius = m_radius->getValue(frame) * scale;
  double blur   = m_blur->getValue(frame);

  TRectD tileRect = convert(tile.getRaster()->getBounds()) + tile.m_pos;

  TRectD bBox;
  m_input->getBBox(frame, bBox, ri);
  if (bBox.isEmpty()) return;

  if (bBox == TConsts::infiniteRectD) bBox = tileRect;

  enlarge(bBox, tileRect, ri, frame);

  TPointD tileRectCenter = (tileRect.getP00() + tileRect.getP11()) * 0.5;
  point -= tileRectCenter;

  int rasInLx = tileRect.getLx();
  int rasInLy = tileRect.getLy();

  TRaster32P raster32 = tile.getRaster();
  TRaster64P raster64 = tile.getRaster();

  TPoint offset = convert(tile.m_pos - tileRect.getP00());
  TTile tileIn;
  if (raster32) {
    m_input->allocateAndCompute(tileIn, tileRect.getP00(),
                                TDimension(rasInLx, rasInLy), raster32, frame,
                                ri);
    TRaster32P rin = tileIn.getRaster();
    TRaster32P app = raster32->create(rasInLx, rasInLy);
    doRadialBlur<TPixel32, UCHAR, 255>(app, rin, blur, radius, point);
    raster32->copy(app, -offset);
  } else if (raster64) {
    TRaster64P raster64 = tile.getRaster();
    m_input->allocateAndCompute(tileIn, tileRect.getP00(),
                                TDimension(rasInLx, rasInLy), raster64, frame,
                                ri);
    TRaster64P rin = tileIn.getRaster();
    TRaster64P app = raster64->create(rasInLx, rasInLy);
    doRadialBlur<TPixel64, USHORT, 65535>(app, rin, blur, radius, point);
    raster64->copy(app, -offset);
  } else
    throw TException("Brightness&Contrast: unsupported Pixel Type");
}

//------------------------------------------------------------------

int RadialBlurFx::getMemoryRequirement(const TRectD &rect, double frame,
                                       const TRenderSettings &info) {
  double scale  = sqrt(fabs(info.m_affine.det()));
  TPointD point = info.m_affine * m_point->getValue(frame);
  double blur   = m_blur->getValue(frame);

  TRectD bBox;
  m_input->getBBox(frame, bBox, info);
  if (bBox.isEmpty()) return 0;

  if (bBox == TConsts::infiniteRectD) bBox = rect;

  TRectD tileRect(rect);
  enlarge(bBox, tileRect, info, frame);

  return TRasterFx::memorySize(tileRect.enlarge(blur), info.m_bpp);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(RadialBlurFx, "radialBlurFx")
