#include <memory>

#include "tpixel.h"
#include <stdio.h>
#include "tfxparam.h"
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "tpixelutils.h"
#include "trop.h"
#include "stdfx.h"
#include "tgeometry.h"
#include "tfxattributes.h"
#include "tparamuiconcept.h"
//#include "timage_io.h"

/*---------------------------------------------------------------------------*/
namespace {

/*
template<class T>
void overlayPixels(const T& dw, const T& up, T& ris, double glob)
{
int value;
double aux;
int max = T::maxChannelValue;

if (up.m == 0)
  ris = dw;
else if ((up.m == max)&&(glob==1))
  ris = up;
else if (glob==1)
  {
  aux = (max-up.m)/max;
  value = up.r + troundp(dw.r * aux);
  ris.r = (value <max)?value:max;
  value = up.g + troundp(dw.g * aux);
  ris.g = (value <max)?value:max;
  value = up.b + troundp(dw.b * aux);
  ris.b = (value <max)?value:max;
  value = up.m + dw.m;
  ris.m = (value <max)?value:max;
  }
else
  {
  aux = (max-up.m*glob)/max;

  value = (int)(up.r*glob) + troundp(dw.r * aux);
  ris.r = (value <max)?value:max;
  value = (int)(up.g*glob) + troundp(dw.g * aux);
  ris.g = (value <max)?value:max;
  value = (int)(up.b*glob) + troundp(dw.b * aux);
  ris.b = (value <max)?value:max;
  value = (int)(up.m*glob) + dw.m;
  ris.m = (value <max)?value:max;
        }
}
*/
/*---------------------------------------------------------------------------*/

template <class T>
inline void blur_code(T *row1, T *row2, int length, double coeff, double coeffq,
                      int brad, double diff, double globmatte) {
  int i;
  double rsum, gsum, bsum, msum;

  TPixelD sigma1, sigma2, sigma3, desigma;
  T *pix1, *pix2, *pix3, *pix4;
  int max = T::maxChannelValue;

  pix1 = row1;
  pix2 = row1 - 1;

  sigma1.r = pix1->r;
  sigma1.g = pix1->g;
  sigma1.b = pix1->b;
  sigma1.m = pix1->m;
  pix1++;

  sigma2.r = sigma2.g = sigma2.b = sigma2.m = 0.0;
  sigma3.r = sigma3.g = sigma3.b = sigma3.m = 0.0;

  for (i = 1; i < brad; i++) {
    sigma1.r += pix1->r;
    sigma1.g += pix1->g;
    sigma1.b += pix1->b;
    sigma1.m += pix1->m;

    sigma2.r += pix2->r;
    sigma2.g += pix2->g;
    sigma2.b += pix2->b;
    sigma2.m += pix2->m;

    sigma3.r += i * (pix1->r + pix2->r);
    sigma3.g += i * (pix1->g + pix2->g);
    sigma3.b += i * (pix1->b + pix2->b);
    sigma3.m += i * (pix1->m + pix2->m);

    pix1++;
    pix2--;
  }

  rsum = (sigma1.r + sigma2.r) * coeff - sigma3.r * coeffq + 0.5;
  gsum = (sigma1.g + sigma2.g) * coeff - sigma3.g * coeffq + 0.5;
  bsum = (sigma1.b + sigma2.b) * coeff - sigma3.b * coeffq + 0.5;
  msum = (sigma1.m + sigma2.m) * coeff - sigma3.m * coeffq + 0.5;

  row2->r = tcrop((int)rsum, 0, max);
  row2->g = tcrop((int)gsum, 0, max);
  row2->b = tcrop((int)bsum, 0, max);
  row2->m = tcrop((int)msum, 0, max);

  if (globmatte != 1.0) {
    row2->r = (int)(row2->r * globmatte);
    row2->g = (int)(row2->g * globmatte);
    row2->b = (int)(row2->b * globmatte);
    row2->m = (int)(row2->m * globmatte);
  }
  *row2 = overPix(*row1, *row2);
  // overlayPixels<T>(*row1, *row2, *row2, globmatte);

  // row2++;

  sigma2.r += row1[-brad].r;
  sigma2.g += row1[-brad].g;
  sigma2.b += row1[-brad].b;
  sigma2.m += row1[-brad].m;

  pix1 = row1 + brad;
  pix2 = row1;
  pix3 = row1 - brad;
  pix4 = row1 - brad + 1;

  desigma.r = sigma1.r - sigma2.r;
  desigma.g = sigma1.g - sigma2.g;
  desigma.b = sigma1.b - sigma2.b;
  desigma.m = sigma1.m - sigma2.m;

  for (i = 1; i < length; i++) {
    desigma.r += pix1->r - 2 * pix2->r + pix3->r;
    desigma.g += pix1->g - 2 * pix2->g + pix3->g;
    desigma.b += pix1->b - 2 * pix2->b + pix3->b;
    desigma.m += pix1->m - 2 * pix2->m + pix3->m;

    rsum += (desigma.r + diff * (pix1->r - pix4->r)) * coeffq;
    gsum += (desigma.g + diff * (pix1->g - pix4->g)) * coeffq;
    bsum += (desigma.b + diff * (pix1->b - pix4->b)) * coeffq;
    msum += (desigma.m + diff * (pix1->m - pix4->m)) * coeffq;

    row2->r = tcrop((int)rsum, 0, max);
    row2->g = tcrop((int)gsum, 0, max);
    row2->b = tcrop((int)bsum, 0, max);
    row2->m = tcrop((int)msum, 0, max);
    if (globmatte != 1.0) {
      row2->r = (int)(row2->r * globmatte);
      row2->g = (int)(row2->g * globmatte);
      row2->b = (int)(row2->b * globmatte);
      row2->m = (int)(row2->m * globmatte);
    }
    *row2 = overPix(*pix2, *row2);
    // overlayPixels<T>(*pix2, *row2, *row2, 0.8);

    row2++;
    pix1++, pix2++, pix3++, pix4++;
  }
}

/*---------------------------------------------------------------------------*/

template <class T>
void do_filtering(T *row1, T *row2, int length, double coeff, int brad,
                  double Mblur, double globmatte) {
  int i;
  double rsum, gsum, bsum, msum;
  TPixelD sigma1, sigma2;
  int max = T::maxChannelValue;

  sigma1.r = sigma1.g = sigma1.b = sigma1.m = 0.0;
  sigma2.r = sigma2.g = sigma2.b = sigma2.m = 0.0;

  for (i = 0; i <= brad; i++) {
    sigma1.r += row1[-i].r;
    sigma1.g += row1[-i].g;
    sigma1.b += row1[-i].b;
    sigma1.m += row1[-i].m;

    sigma2.r += -i * row1[-i].r;
    sigma2.g += -i * row1[-i].g;
    sigma2.b += -i * row1[-i].b;
    sigma2.m += -i * row1[-i].m;
  }

  for (i = 0; i < length; i++) /* for the ith point the previous computing is
                                  used, with the values */
  {
    /* stored in the auxiliar variables sigma1 and sigma2. */
    rsum = ((Mblur - i) * sigma1.r + sigma2.r) / coeff;
    gsum = ((Mblur - i) * sigma1.g + sigma2.g) / coeff;
    bsum = ((Mblur - i) * sigma1.b + sigma2.b) / coeff;
    msum = ((Mblur - i) * sigma1.m + sigma2.m) / coeff;

    row2[i].r = (rsum < max) ? troundp(rsum) : max;
    row2[i].g = (gsum < max) ? troundp(gsum) : max;
    row2[i].b = (bsum < max) ? troundp(bsum) : max;
    row2[i].m = (msum < max) ? troundp(msum) : max;
    if (globmatte != 1.0) {
      row2[i].r = (int)(row2[i].r * globmatte);
      row2[i].g = (int)(row2[i].g * globmatte);
      row2[i].b = (int)(row2[i].b * globmatte);
      row2[i].m = (int)(row2[i].m * globmatte);
    }
    row2[i] = overPix(row1[i], row2[i]);
    // overlayPixels<T>(row1[i], row2[i], row2[i], globmatte);

    if (i < length - 1) {
      sigma1.r += row1[i + 1].r - row1[i - brad].r;
      sigma1.g += row1[i + 1].g - row1[i - brad].g;
      sigma1.b += row1[i + 1].b - row1[i - brad].b;
      sigma1.m += row1[i + 1].m - row1[i - brad].m;

      sigma2.r += (i + 1) * row2[i + 1].r - (i - brad) * row1[i - brad].r;
      sigma2.g += (i + 1) * row2[i + 1].g - (i - brad) * row1[i - brad].g;
      sigma2.b += (i + 1) * row2[i + 1].b - (i - brad) * row1[i - brad].b;
      sigma2.m += (i + 1) * row2[i + 1].m - (i - brad) * row1[i - brad].m;
    }
  }
}

/*---------------------------------------------------------------------------*/

template <class T>
void takeRow(T *rin, T *row, int lx, int brad, bool bidirectional) {
  int i;

  for (i = 0; i < lx; i++) row[i] = rin[i];

  for (i = -1; i > -brad;
       i--) /* pixels equal to the ones of border of image are added   */
    row[i] = row[0]; /* to avoid a black blur to get into the picture. */

  if (bidirectional)
    for (i = lx; i < lx + brad;
         i++) /* pixels equal to the ones of border of image are added   */
      row[i] = row[lx - 1]; /* to avoid a black blur to get into the picture. */
}

/*---------------------------------------------------------------------------*/

template <class T>
void doDirectionalBlur(TRasterPT<T> r, double blur, bool bidirectional) {
  int i, lx, ly, brad;
  double coeff, coeffq, diff, globmatte;

  brad = tfloor(blur); /* number of pixels involved in the filtering. */
  if (bidirectional) {
    coeff =
        blur /
        (brad - brad * brad +
         blur * (2 * brad - 1));  // sum of the weights of triangolar filter.
    coeffq = coeff / blur;
    diff   = blur - brad;
  } else
    coeff = (brad + 1) * (1 - brad / (2 * blur)) *
            blur; /*sum of the weights of triangolar filter.              */

  lx = r->getLx();
  ly = r->getLy();

  if ((lx == 0) || (ly == 0)) return;

  std::unique_ptr<T[]> row(new T[lx + 2 * brad + 2]);
  if (!row) return;
  memset(row.get(), 0, (lx + 2 * brad + 2) * sizeof(T));

  globmatte = 0.8; /* a little bit of transparency is also added */

  r->lock();
  for (i = 0; i < ly; i++) {
    T *buffer = (T *)r->pixels(i);
    takeRow(buffer, row.get() + brad, lx, brad, bidirectional);
    if (bidirectional)
      blur_code<T>(row.get() + brad, buffer, lx, coeff, coeffq, brad, diff,
                   globmatte);
    else
      do_filtering<T>(row.get() + brad, buffer, lx, coeff, brad, blur,
                      globmatte);
  }

  r->unlock();
}

/*---------------------------------------------------------------------------*/

template <class T>
void directionalBlur(TRasterPT<T> rout, TRasterPT<T> rin, const TPointD &blur,
                     const TPoint &offset, bool bidirectional) {
  double cs, sn, cx_aux, cy_aux;
  int lx, ly, lx_aux, ly_aux;

  double blurValue = tdistance(TPointD(), blur);

  lx = rin->getLx();
  ly = rin->getLy();
  cs = blur.x / blurValue;
  sn = blur.y / blurValue;

  lx_aux = (int)tceil(lx * fabs(cs) + ly * fabs(sn));
  ly_aux = (int)tceil(lx * fabs(sn) + ly * fabs(cs));
  cx_aux = lx_aux * .5;
  cy_aux = ly_aux * .5;

  TRasterPT<T> raux = TRasterPT<T>(lx_aux, ly_aux);
  TAffine rot(cs, sn, 0, -sn, cs, 0);
  rot = rot.place(convert(rin->getCenter()), convert(raux->getCenter()));

  TRop::resample(raux, rin, rot);

  doDirectionalBlur<T>(raux, blurValue, bidirectional);

  TAffine rotInv(cs, -sn, 0, sn, cs, 0);
  rotInv = rotInv.place(convert(raux->getCenter()),
                        convert(rin->getCenter() + offset));

  TRop::resample(rout, raux, rotInv);
}

}  // namespace

static void enlargeDir(TRectD &r, TPointD p, bool bidirectional) {
  if (bidirectional) {
    r.x1 += fabs(p.x);
    r.x0 -= fabs(p.x);
    r.y1 += fabs(p.y);
    r.y0 -= fabs(p.y);
  } else {
    if (p.x > 0)
      r.x1 += p.x;
    else
      r.x0 -= -p.x;

    if (p.y > 0)
      r.y1 += p.y;
    else
      r.y0 -= -p.y;
  }
}

static void reduceDir(TRectD &r, TPointD p, bool bidirectional) {
  if (bidirectional) {
    r.x1 -= fabs(p.x);
    r.x0 += fabs(p.x);
    r.y1 -= fabs(p.y);
    r.y0 += fabs(p.y);
  } else {
    if (p.x > 0)
      r.x1 -= p.x;
    else
      r.x0 += -p.x;

    if (p.y > 0)
      r.y1 -= p.y;
    else
      r.y0 += -p.y;
  }
}

class DirectionalBlurBaseFx : public TStandardRasterFx {
  // FX_PLUGIN_DECLARATION(DirectionalBlurBaseFx)

protected:
  bool m_isMotionBlur;
  TRasterFxPort m_input;
  TDoubleParamP m_angle;
  TDoubleParamP m_intensity;
  TBoolParamP m_bidirectional;
  TBoolParamP m_spread;
  //  TBoolParamP m_useSSE;

public:
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  DirectionalBlurBaseFx(bool isMotionBLur)
      : m_isMotionBlur(isMotionBLur)
      , m_angle(0.0)
      , m_intensity(10.0)
      , m_bidirectional(false)
      , m_spread(true) {
    bindParam(this, "intensity", m_intensity);
    bindParam(this, "bidirectional", m_bidirectional);
    bindParam(this, "spread", m_spread);

    addInputPort("Source", m_input);
    m_intensity->setValueRange(0, (std::numeric_limits<double>::max)());

    getAttributes()->setIsSpeedAware(true);
  }

  TPointD getBlurVector(double frame) const {
    TPointD speed = getAttributes()->getSpeed();
    double value  = m_intensity->getValue(frame);
    return value * speed;
  }

  ~DirectionalBlurBaseFx(){};

  void doCompute(TTile &tile, double frame, const TRenderSettings &) override;

  bool canHandle(const TRenderSettings &info, double frame) override {
    return isAlmostIsotropic(info.m_affine) ||
           m_intensity->getValue(frame) == 0;
  }

  int getMemoryRequirement(const TRectD &rect, double frame,
                           const TRenderSettings &info) override;
};

class DirectionalBlurFx final : public DirectionalBlurBaseFx

{
  FX_PLUGIN_DECLARATION(DirectionalBlurFx)

public:
  DirectionalBlurFx() : DirectionalBlurBaseFx(false) {
    m_intensity->setMeasureName("fxLength");
    m_angle->setMeasureName("angle");
    bindParam(this, "angle", m_angle);
  }

  void getParamUIs(TParamUIConcept *&concepts, int &length) override {
    concepts = new TParamUIConcept[length = 1];

    concepts[0].m_type  = TParamUIConcept::POLAR;
    concepts[0].m_label = "Angle and Intensity";
    concepts[0].m_params.push_back(m_angle);
    concepts[0].m_params.push_back(m_intensity);
  }
};

class MotionBlurFx final : public DirectionalBlurBaseFx

{
  FX_PLUGIN_DECLARATION(MotionBlurFx)
public:
  MotionBlurFx() : DirectionalBlurBaseFx(true) {}

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override {
    std::string alias = getFxType();
    alias += "[";

    // alias of the effects related to the input ports separated by commas
    // a port that is not connected to an alias blank (empty string)
    int i;
    for (i = 0; i < getInputPortCount(); i++) {
      TFxPort *port = getInputPort(i);
      if (port->isConnected()) {
        TRasterFxP ifx = port->getFx();
        assert(ifx);
        alias += ifx->getAlias(frame, info);
      }
      alias += ",";
    }

    unsigned long id = getIdentifier();
    double value     = m_intensity->getValue(frame);
    return alias + std::to_string(id) + "," + std::to_string(frame) + "," +
           std::to_string(value) + "]";
  }
};

//-------------------------------------------------------------------

bool DirectionalBlurBaseFx::doGetBBox(double frame, TRectD &bBox,
                                      const TRenderSettings &info) {
  if (m_input.isConnected()) {
    bool ret = m_input->doGetBBox(frame, bBox, info);
    if (m_spread->getValue()) {
      TPointD blur;

      if (m_isMotionBlur)
        blur = getBlurVector(frame);
      else {
        double angle = m_angle->getValue(frame) * M_PI_180;
        blur.x       = m_intensity->getValue(frame) * cos(angle);
        blur.y       = m_intensity->getValue(frame) * sin(angle);
      }

      enlargeDir(bBox, blur, m_bidirectional->getValue());
    }
    return ret;
  } else {
    bBox = TRectD();
    return false;
  }
}

//-------------------------------------------------------------------

void DirectionalBlurBaseFx::doCompute(TTile &tile, double frame,
                                      const TRenderSettings &ri) {
  if (!m_input.isConnected()) return;

  TPointD blurVector;
  if (m_isMotionBlur) {
    blurVector = getBlurVector(frame);
  } else {
    double angle = m_angle->getValue(frame) * M_PI_180;
    blurVector.x = m_intensity->getValue(frame) * cos(angle);
    blurVector.y = m_intensity->getValue(frame) * sin(angle);
  }

  const double limit = 900.0;
  blurVector.x       = tcrop(blurVector.x, -limit, limit);
  blurVector.y       = tcrop(blurVector.y, -limit, limit);
  double shrink      = (ri.m_shrinkX + ri.m_shrinkY) / 2.0;

  TAffine aff = ri.m_affine;
  aff.a13 = aff.a23 = 0;
  TPointD blur      = (1.0 / shrink) * (aff * blurVector);

  double blurValue = norm(blur);
  if (areAlmostEqual(blurValue, 0, 1e-3)) {
    m_input->compute(tile, frame, ri);
    return;
  }

  TRectD rectTile = TRectD(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                                   tile.getRaster()->getLy()));
  if (!rectTile.isEmpty()) {
    TRectD bboxIn;
    m_input->getBBox(frame, bboxIn, ri);
    if (bboxIn == TConsts::infiniteRectD) bboxIn = rectTile;
    TPointD blur = (1.0 / shrink) * (aff * blurVector);
    // enlarge must be bidirectional, because we need pixel on the ohter side of
    // blur
    enlargeDir(bboxIn, blur, true);
    rectTile = bboxIn * rectTile.enlarge(fabs(blur.x), fabs(blur.y));
    TRect rectIn;
    rectIn.x0 = tfloor(rectTile.x0);
    rectIn.y0 = tfloor(rectTile.y0);
    rectIn.x1 = tceil(rectTile.x1);
    rectIn.y1 = tceil(rectTile.y1);

    int rasInLx = rectIn.getLx();
    int rasInLy = rectIn.getLy();
    TTile tileIn;
    m_input->allocateAndCompute(tileIn, convert(rectIn.getP00()),
                                TDimension(rasInLx, rasInLy), tile.getRaster(),
                                frame, ri);
    TRasterP rasIn = tileIn.getRaster();
    TRect rectOut =
        rasIn->getBounds() + (rectIn.getP00() - convert(tile.m_pos));
    TRasterP rasOut = tile.getRaster()->extract(rectOut);

    TPoint p;
    if (rectIn.x0 < tile.m_pos.x) p.x = rectIn.x0 - tile.m_pos.x;
    if (rectIn.y0 < tile.m_pos.y) p.y = rectIn.y0 - tile.m_pos.y;
    if ((TRaster32P)rasIn && (TRaster32P)rasOut)
      directionalBlur<TPixel32>(rasOut, rasIn, blur, p,
                                m_bidirectional->getValue());
    else if ((TRaster64P)rasIn && (TRaster64P)rasOut)
      directionalBlur<TPixel64>(rasOut, rasIn, blur, p,
                                m_bidirectional->getValue());
  }
}

//---------------------------------------------------------------------------

int DirectionalBlurBaseFx::getMemoryRequirement(const TRectD &rect,
                                                double frame,
                                                const TRenderSettings &info) {
  TPointD blurVector;
  if (m_isMotionBlur) {
    blurVector = getBlurVector(frame);
  } else {
    double angle = m_angle->getValue(frame) * M_PI_180;
    blurVector.x = m_intensity->getValue(frame) * cos(angle);
    blurVector.y = m_intensity->getValue(frame) * sin(angle);
  }

  const double limit = 900.0;
  blurVector.x       = tcrop(blurVector.x, -limit, limit);
  blurVector.y       = tcrop(blurVector.y, -limit, limit);
  double shrink      = (info.m_shrinkX + info.m_shrinkY) / 2.0;

  TAffine aff = info.m_affine;
  aff.a13 = aff.a23 = 0;
  TPointD blur      = (1.0 / shrink) * (aff * blurVector);

  return TRasterFx::memorySize(rect.enlarge(blur.x, blur.y), info.m_bpp);
}

//---------------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(DirectionalBlurFx, "directionalBlurFx");
FX_PLUGIN_IDENTIFIER(MotionBlurFx, "motionBlurFx");
