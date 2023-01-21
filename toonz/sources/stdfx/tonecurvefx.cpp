

#include "stdfx.h"
#include "tfxparam.h"
#include "tpixelutils.h"
#include "tparamset.h"
#include "ttonecurveparam.h"
#include "tcurves.h"
#include "globalcontrollablefx.h"

//===================================================================

namespace {

int getCubicYfromX(TCubic c, int x, double &s0, double &s1) {
  double s  = (s1 + s0) * 0.5;
  TPointD p = c.getPoint(s);
  if (areAlmostEqual(double(x), p.x, 0.001)) return tround(p.y);

  if (x < p.x)
    return getCubicYfromX(c, x, s0, s);
  else
    return getCubicYfromX(c, x, s, s1);
}

int getLinearYfromX(TSegment t, int x, double &s0, double &s1) {
  double s  = (s1 + s0) * 0.5;
  TPointD p = t.getPoint(s);
  if (areAlmostEqual(double(x), p.x, 0.001)) return tround(p.y);

  if (x < p.x)
    return getLinearYfromX(t, x, s0, s);
  else
    return getLinearYfromX(t, x, s, s1);
}

void truncateSpeeds(double aFrame, double bFrame, TPointD &aSpeedTrunc,
                    TPointD &bSpeedTrunc) {
  double deltaX = bFrame - aFrame;
  if (aSpeedTrunc.x < 0) aSpeedTrunc.x = 0;
  if (bSpeedTrunc.x > 0) bSpeedTrunc.x = 0;

  if (aFrame + aSpeedTrunc.x > bFrame) {
    if (aSpeedTrunc.x != 0) {
      aSpeedTrunc = aSpeedTrunc * (deltaX / aSpeedTrunc.x);
    }
  }

  if (bFrame + bSpeedTrunc.x < aFrame) {
    if (bSpeedTrunc.x != 0) {
      bSpeedTrunc = -bSpeedTrunc * (deltaX / bSpeedTrunc.x);
    }
  }
}

template <typename PIXEL, typename T>
void fill_lut(QList<TPointD> points, std::vector<T> &lut, bool isLinear) {
  int i;
  TPointD p0 = points.at(0);
  TCubic cubic;
  TSegment segment;
  double s0 = 0.0;
  for (i = 1; i < points.size(); i++) {
    TPointD p1 = points.at(i);
    TPointD p2 = points.at(++i);
    TPointD p3 = points.at(++i);
    if (!isLinear) {
      // truncate speed
      TPointD aSpeed(p1 - p0);
      TPointD bSpeed(p2 - p3);
      truncateSpeeds(p0.x, p3.x, aSpeed, bSpeed);
      cubic.setP0(p0);
      cubic.setP1(p0 + aSpeed);
      cubic.setP2(p3 + bSpeed);
      cubic.setP3(p3);
      int x = (int)p0.x;
      while (x < 0) x++;
      while (x < p3.x && x < PIXEL::maxChannelValue + 1) {
        double s1 = 1.0;
        int y     = getCubicYfromX(cubic, x, s0, s1);
        if (y > PIXEL::maxChannelValue)
          y = PIXEL::maxChannelValue;
        else if (y < 0)
          y = 0;
        lut[x] = y;
        x++;
      }
    } else {
      segment.setP0(p0);
      segment.setP1(p3);
      int x = (int)p0.x;
      while (x < 0) x++;
      while (x < p3.x && x < PIXEL::maxChannelValue + 1) {
        double s1 = 1.0;
        int y     = getLinearYfromX(segment, x, s0, s1);
        if (y > PIXEL::maxChannelValue)
          y = PIXEL::maxChannelValue;
        else if (y < 0)
          y = 0;
        lut[x] = y;
        x++;
      }
    }
    p0 = p3;
  }
}

}  // Namespace

//===================================================================

class ToneCurveFx final : public GlobalControllableFx {
  FX_PLUGIN_DECLARATION(ToneCurveFx)

  TRasterFxPort m_input;
  TToneCurveParamP m_toneCurve;

public:
  ToneCurveFx() : m_toneCurve(new TToneCurveParam()) {
    bindParam(this, "curve", m_toneCurve);
    addInputPort("Source", m_input);

    enableComputeInFloat(true);
  }

  ~ToneCurveFx(){};

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override {
    if (m_input.isConnected())
      return m_input->doGetBBox(frame, bBox, info);
    else {
      bBox = TRectD();
      return false;
    }
  }

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
};

//-------------------------------------------------------------------

namespace {
void update_param(double &param, TRaster32P ras) { return; }

void update_param(double &param, TRaster64P ras) {
  param = param * 257;
  return;
}

void update_param(double &param, TRasterFP ras) {
  param /= double(TPixel32::maxChannelValue);
  return;
}

QList<TPointD> getParamSetPoints(const TParamSet *paramSet, int frame) {
  QList<TPointD> points;
  int i;
  for (i = 0; i < paramSet->getParamCount(); i++) {
    TPointParamP pointParam = paramSet->getParam(i);
    TPointD point           = pointParam->getValue(frame);
    int x                   = (int)point.x;
    int y                   = (int)point.y;
    points.push_back(TPointD(x, y));
  }
  return points;
}
}  // namespace

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void doToneCurveFx(TRasterPT<PIXEL> ras, double frame,
                   const TToneCurveParam *toneCurveParam) {
  QList<QList<TPointD>> pointsList;
  int e;
  for (e = 0; e < 6; e++) {
    TParamSet *paramSet =
        toneCurveParam->getParamSet(TToneCurveParam::ToneChannel(e))
            .getPointer();
    QList<TPointD> points = getParamSetPoints(paramSet, frame);
    pointsList.push_back(points);
  }
  bool isLinear = toneCurveParam->isLinear();

  int i, t;
  for (i = 0; i < pointsList.size(); i++) {
    QList<TPointD> &points = pointsList[i];
    for (t = 0; t < points.size(); t++) {
      TPointD &p = points[t];
      double &x  = p.x;
      double &y  = p.y;
      update_param(x, ras);
      update_param(y, ras);
    }
  }

  std::vector<CHANNEL_TYPE> rgbaLut(PIXEL::maxChannelValue + 1);
  std::vector<CHANNEL_TYPE> rgbLut(PIXEL::maxChannelValue + 1);
  std::vector<CHANNEL_TYPE> rLut(PIXEL::maxChannelValue + 1);
  std::vector<CHANNEL_TYPE> gLut(PIXEL::maxChannelValue + 1);
  std::vector<CHANNEL_TYPE> bLut(PIXEL::maxChannelValue + 1);
  std::vector<CHANNEL_TYPE> aLut(PIXEL::maxChannelValue + 1);

  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[0], rgbaLut, isLinear);
  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[1], rgbLut, isLinear);
  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[2], rLut, isLinear);
  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[3], gLut, isLinear);
  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[4], bLut, isLinear);
  fill_lut<PIXEL, CHANNEL_TYPE>(pointsList[5], aLut, isLinear);

  int lx = ras->getLx();
  int ly = ras->getLy();

  int j;
  ras->lock();
  for (j = 0; j < ly; j++) {
    PIXEL *pix    = ras->pixels(j);
    PIXEL *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->m != 0 && pix->m != PIXEL::maxChannelValue)
        *pix = depremultiply(*pix);
      pix->r = rLut[(int)(pix->r)];
      pix->g = gLut[(int)(pix->g)];
      pix->b = bLut[(int)(pix->b)];
      pix->m = aLut[(int)(pix->m)];
      pix->r = rgbLut[(int)(pix->r)];
      pix->g = rgbLut[(int)(pix->g)];
      pix->b = rgbLut[(int)(pix->b)];
      pix->r = rgbaLut[(int)(pix->r)];
      pix->g = rgbaLut[(int)(pix->g)];
      pix->b = rgbaLut[(int)(pix->b)];
      pix->m = rgbaLut[(int)(pix->m)];
      if (pix->m != 0 && pix->m != PIXEL::maxChannelValue)
        *pix = premultiply(*pix);
      pix++;
    }
  }
  ras->unlock();
}

template <>
void doToneCurveFx<TPixelF, float>(TRasterFP ras, double frame,
                                   const TToneCurveParam *toneCurveParam) {
  QList<QList<TPointD>> pointsList;
  int e;
  for (e = 0; e < 6; e++) {
    TParamSet *paramSet =
        toneCurveParam->getParamSet(TToneCurveParam::ToneChannel(e))
            .getPointer();
    QList<TPointD> points = getParamSetPoints(paramSet, frame);
    pointsList.push_back(points);
  }
  bool isLinear = toneCurveParam->isLinear();

  int i, t;
  for (i = 0; i < pointsList.size(); i++) {
    QList<TPointD> &points = pointsList[i];
    for (t = 0; t < points.size(); t++) {
      TPointD &p = points[t];
      double &x  = p.x;
      double &y  = p.y;
      update_param(x, TRaster64P());
      update_param(y, TRaster64P());
    }
  }

  std::vector<USHORT> rgbaLut(TPixel64::maxChannelValue + 1);
  std::vector<USHORT> rgbLut(TPixel64::maxChannelValue + 1);
  std::vector<USHORT> rLut(TPixel64::maxChannelValue + 1);
  std::vector<USHORT> gLut(TPixel64::maxChannelValue + 1);
  std::vector<USHORT> bLut(TPixel64::maxChannelValue + 1);
  std::vector<USHORT> aLut(TPixel64::maxChannelValue + 1);

  fill_lut<TPixel64, USHORT>(pointsList[0], rgbaLut, isLinear);
  fill_lut<TPixel64, USHORT>(pointsList[1], rgbLut, isLinear);
  fill_lut<TPixel64, USHORT>(pointsList[2], rLut, isLinear);
  fill_lut<TPixel64, USHORT>(pointsList[3], gLut, isLinear);
  fill_lut<TPixel64, USHORT>(pointsList[4], bLut, isLinear);
  fill_lut<TPixel64, USHORT>(pointsList[5], aLut, isLinear);

  int lx = ras->getLx();
  int ly = ras->getLy();

  std::vector<float> rLutF(TPixel64::maxChannelValue + 1);
  std::vector<float> gLutF(TPixel64::maxChannelValue + 1);
  std::vector<float> bLutF(TPixel64::maxChannelValue + 1);
  std::vector<float> aLutF(TPixel64::maxChannelValue + 1);

  auto normalizeLut = [&](std::vector<float> &dst,
                          std::vector<USHORT> &chanLut) {
    for (int i = 0; i <= TPixel64::maxChannelValue; i++) {
      int v  = rgbaLut[rgbLut[chanLut[i]]];
      dst[i] = float(v) / float(TPixel64::maxChannelValue);
    }
  };
  normalizeLut(rLutF, rLut);
  normalizeLut(gLutF, gLut);
  normalizeLut(bLutF, bLut);
  for (int i = 0; i <= TPixel64::maxChannelValue; i++) {
    int v    = rgbaLut[aLut[i]];
    aLutF[i] = float(v) / float(TPixel64::maxChannelValue);
  }

  auto getLutValue = [&](std::vector<float> &lut, float val) {
    if (val < 0.f)
      return lut[0];
    else if (val >= 1.f)
      return lut[TPixel64::maxChannelValue];
    float v     = val * float(TPixel64::maxChannelValue);
    int id      = (int)tfloor(v);
    float ratio = v - float(id);
    return lut[id] * (1.f - ratio) + lut[id + 1] * ratio;
  };

  int j;
  ras->lock();
  for (j = 0; j < ly; j++) {
    TPixelF *pix    = ras->pixels(j);
    TPixelF *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->m > 0.f) *pix = depremultiply(*pix);
      pix->r = getLutValue(rLutF, pix->r);
      pix->g = getLutValue(gLutF, pix->g);
      pix->b = getLutValue(bLutF, pix->b);
      pix->m = getLutValue(aLutF, pix->m);
      if (pix->m > 0.f) *pix = premultiply(*pix);
      pix++;
    }
  }
  ras->unlock();
}
//-------------------------------------------------------------------

void ToneCurveFx::doCompute(TTile &tile, double frame,
                            const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TRaster32P raster32 = tile.getRaster();
  TRaster64P raster64 = tile.getRaster();
  TRasterFP rasterF   = tile.getRaster();

  if (raster32)
    doToneCurveFx<TPixel32, UCHAR>(raster32, frame, m_toneCurve.getPointer());
  else if (raster64)
    doToneCurveFx<TPixel64, USHORT>(raster64, frame, m_toneCurve.getPointer());
  else if (rasterF)
    doToneCurveFx<TPixelF, float>(rasterF, frame, m_toneCurve.getPointer());
  else
    throw TException("Brightness&Contrast: unsupported Pixel Type");
}

FX_PLUGIN_IDENTIFIER(ToneCurveFx, "toneCurveFx");
