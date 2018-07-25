#include <memory>
#include <array>

#include "toonz/tdistort.h"
#include "traster.h"
#include "trastercm.h"
#include "tgeometry.h"
#include "tpixelutils.h"
#include <QList>

//****************************************************************************************
//    Local namespace stuff
//****************************************************************************************

namespace {
inline double dist(const TPointD &a, const TPointD &b) { return norm(b - a); }
}

//========================================================================================

namespace {
typedef struct {
  TUINT32 val;
  double tot;
} BLOB24;

//---------------------------------------------------------------------------------

TPixelCM32 filterPixel(const TPointD &pos, const TRasterCM32P &rasIn) {
  TPointD distance = TPointD(
      areAlmostEqual(pos.x, tfloor(pos.x), 0.001) ? 0.0
                                                  : fabs(pos.x - tfloor(pos.x)),
      areAlmostEqual(pos.y, tfloor(pos.y), 0.001) ? 0.0 : fabs(pos.y -
                                                               tfloor(pos.y)));
  TPoint nearPos(tfloor(pos.x), tfloor(pos.y));
  if (distance == TPointD(0.0, 0.0)) {
    if (nearPos.x >= 0 && nearPos.x < rasIn->getLx() && nearPos.y >= 0 &&
        nearPos.y < rasIn->getLy())
      return rasIn->pixels(nearPos.y)[nearPos.x];
    else
      return TPixelCM32();
  }

  int i, j, k = 0;
  TPixelCM32 P[4];

  for (j = 0; j < 2; ++j)
    for (i         = 0; i < 2; ++i)
      P[i + 2 * j] = nearPos.x + i < rasIn->getLx() && nearPos.x + i >= 0 &&
                             nearPos.y + j < rasIn->getLy() &&
                             nearPos.y + j >= 0
                         ? rasIn->pixels(nearPos.y + j)[nearPos.x + i]
                         : TPixelCM32();

  if (P[0] == P[1] && P[1] == P[2] && P[2] == P[3]) return P[0];

  double w[4], sum = 0;
  for (j = 1; j >= 0; --j)
    for (i          = 1; i >= 0; --i)
      sum += w[k++] = fabs(distance.x - i) * fabs(distance.y - j);

  for (i = 0; i < 4; i++) w[i] /= sum;

  TPixelCM32 outPix;
  TUINT32 tone;
  double tone_tot = 0.0;
  BLOB24 currBlob, paintBlobs[4], inkBlobs[4];
  int paintBlobsSize = 0, inkBlobsSize = 0;

  // Doing the same thing in TRop::resample

  for (i = 0; i < 4; ++i) {
    // Build paint blobs and sort them
    tone = P[i].getTone();
    tone_tot += tone * w[i];
    currBlob.val = P[i].getPaint();
    currBlob.tot = tone * w[i];
    for (j = 0; j < paintBlobsSize; j++)
      if (paintBlobs[j].val == currBlob.val) break;
    if (j < paintBlobsSize)
      paintBlobs[j].tot += currBlob.tot;
    else
      paintBlobs[paintBlobsSize++] = currBlob;
    for (; j > 0 && paintBlobs[j].tot > paintBlobs[j - 1].tot; j--)
      std::swap(paintBlobs[j], paintBlobs[j - 1]);

    // Same for ink blobs
    currBlob.val = P[i].getInk();
    currBlob.tot = (TPixelCM32::getToneMask() - tone) * w[i];
    for (j = 0; j < inkBlobsSize; j++)
      if (inkBlobs[j].val == currBlob.val) break;
    if (j < inkBlobsSize)
      inkBlobs[j].tot += currBlob.tot;
    else
      inkBlobs[inkBlobsSize++] = currBlob;
    for (; j > 0 && inkBlobs[j].tot > inkBlobs[j - 1].tot; j--)
      std::swap(inkBlobs[j], inkBlobs[j - 1]);
  }

  tone = troundp(tone_tot);

  outPix.setPaint(paintBlobs[0].val);
  outPix.setInk(inkBlobs[0].val);
  outPix.setTone(tone);

  return outPix;
}

//---------------------------------------------------------------------------------

void resample(const TRasterCM32P &rasIn, TRasterCM32P &rasOut,
              const TDistorter &distorter, const TPoint &p) {
  if (rasOut->getLx() < 1 || rasOut->getLy() < 1 || rasIn->getLx() < 1 ||
      rasIn->getLy() < 1)
    return;

  std::unique_ptr<TPointD[]> preImages(new TPointD[distorter.maxInvCount()]);

  int x, y;
  int i;
  for (y = 0; y < rasOut->getLy(); y++) {
    TPixelCM32 *pix    = rasOut->pixels(y);
    TPixelCM32 *endPix = pix + rasOut->getLx();

    for (x = 0; pix != endPix; pix++, x++) {
      TPixelCM32 pixDown;

      int count = distorter.invMap(convert(p) + TPointD(x + 0.5, y + 0.5),
                                   preImages.get());
      for (i = count - 1; i >= 0; --i) {
        TPixelCM32 pixUp;
        TPointD preImage(preImages[i].x - 0.5, preImages[i].y - 0.5);

        if (preImage.x > -1 && preImage.y > -1 &&
            preImage.x < rasIn->getLx() + 1 && preImage.y < rasIn->getLy() + 1)
          pixUp = filterPixel(preImage, rasIn);

        pixDown.setPaint(pixUp.getPaint() ? pixUp.getPaint()
                                          : pixDown.getPaint());
        pixDown.setInk(pixUp.getInk() ? pixUp.getInk() : pixDown.getInk());
        pixDown.setTone(pixUp == TPixelCM32() ? pixDown.getTone()
                                              : pixUp.getTone());
      }
      *pix = pixDown;
    }
  }
}

//---------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
PIXEL closest_pixel(const TPointD &pos, const TRasterPT<PIXEL> &rasIn) {
  TPoint nearPos(tround(pos.x), tround(pos.y));
  if (!rasIn->getBounds().contains(nearPos)) return PIXEL(0, 0, 0, 0);
  return rasIn->pixels(nearPos.y)[nearPos.x];
}

//---------------------------------------------------------------------------------

template <typename T, typename CHANNEL_TYPE>
void resampleClosestPixel(const TRasterPT<T> &rasIn, TRasterPT<T> &rasOut,
                          const TDistorter &distorter, const TPoint &p) {
  if (rasOut->getLx() < 1 || rasOut->getLy() < 1 || rasIn->getLx() < 1 ||
      rasIn->getLy() < 1)
    return;

  std::unique_ptr<TPointD[]> preImages(new TPointD[distorter.maxInvCount()]);

  int x, y;
  int i;
  for (y = 0; y < rasOut->getLy(); y++) {
    T *pix    = rasOut->pixels(y);
    T *endPix = pix + rasOut->getLx();

    x = 0;
    for (; pix != endPix; pix++, x++) {
      T pixDown(0, 0, 0, 0);

      // preImages.clear();
      int count = distorter.invMap(convert(p) + TPointD(x + 0.5, y + 0.5),
                                   preImages.get());
      for (i = count - 1; i >= 0; --i) {
        T pixUp(0, 0, 0, 0);
        TPointD preImage(preImages[i].x - 0.5, preImages[i].y - 0.5);

        if (preImage.x > -1 && preImage.y > -1 &&
            preImage.x < rasIn->getLx() + 1 && preImage.y < rasIn->getLy() + 1)
          pixUp = closest_pixel<T, CHANNEL_TYPE>(preImage, rasIn);

        pixDown = pixUp;
      }
      *pix = pixDown;
    }
  }
}

//---------------------------------------------------------------------------------

TPixelCM32 closest_pixel(const TPointD &pos, const TRasterCM32P &rasIn) {
  TPoint nearPos(tround(pos.x), tround(pos.y));
  if (!rasIn->getBounds().contains(nearPos)) return TPixelCM32(0, 0, 255);
  return rasIn->pixels(nearPos.y)[nearPos.x];
}

//---------------------------------------------------------------------------------

void resampleClosestPixel(const TRasterCM32P &rasIn, TRasterCM32P &rasOut,
                          const TDistorter &distorter, const TPoint &p) {
  if (rasOut->getLx() < 1 || rasOut->getLy() < 1 || rasIn->getLx() < 1 ||
      rasIn->getLy() < 1)
    return;

  std::unique_ptr<TPointD[]> preImages(new TPointD[distorter.maxInvCount()]);

  int x, y;
  int i;
  for (y = 0; y < rasOut->getLy(); y++) {
    TPixelCM32 *pix    = rasOut->pixels(y);
    TPixelCM32 *endPix = pix + rasOut->getLx();

    x = 0;
    for (; pix != endPix; pix++, x++) {
      TPixelCM32 pixDown(0, 0, 255);

      int count = distorter.invMap(convert(p) + TPointD(x + 0.5, y + 0.5),
                                   preImages.get());
      for (i = count - 1; i >= 0; --i) {
        TPixelCM32 pixUp(0, 0, 255);
        TPointD preImage(preImages[i].x - 0.5, preImages[i].y - 0.5);

        if (preImage.x > -1 && preImage.y > -1 &&
            preImage.x < rasIn->getLx() + 1 && preImage.y < rasIn->getLy() + 1)
          pixUp = closest_pixel(preImage, rasIn);

        pixDown = pixUp;
      }
      *pix = pixDown;
    }
  }
}

//=================================================================================

template <typename PIXEL, typename CHANNEL_TYPE>
PIXEL filterPixel(double a, double b, PIXEL *lineSrc, int lineLength,
                  int lineWrap) {
  // Retrieve the interesting pixel interval
  double x0 = std::max(a, 0.0);
  double x1 = std::min(b, (double)lineLength);

  int x0Floor = tfloor(x0);
  int x0Ceil  = tceil(x0);
  int x1Floor = tfloor(x1);

  if (x0 >= x1) return PIXEL::Transparent;

  PIXEL *pix = lineSrc + x0Floor * lineWrap;

  // Build the sums
  double k, sumr = 0, sumg = 0, sumb = 0, summ = 0;

  // Fractionary part on the beginning
  if (x0Ceil > x0) {
    k = x0Ceil - x0;
    sumr += k * pix->r;
    sumg += k * pix->g;
    sumb += k * pix->b;
    summ += k * pix->m;
    pix += lineWrap;
  }

  // Intermediate pixels (ie when pixels contract)
  int x;
  for (x = x0Ceil; x < x1Floor; ++x, pix += lineWrap) {
    sumr += pix->r;
    sumg += pix->g;
    sumb += pix->b;
    summ += pix->m;
  }

  // Fractionary part on the end
  if (x1 < lineLength) {
    k = x1 - x;
    sumr += k * pix->r;
    sumg += k * pix->g;
    sumb += k * pix->b;
    summ += k * pix->m;
  }

  // Finally, divide per the total weight
  double length =
      b - a;  // 'transparent' pixels outside image range *are* considered
  sumr = sumr / length;
  sumg = sumg / length;
  sumb = sumb / length;
  summ = summ / length;

  return PIXEL(sumr, sumg, sumb, summ);
}

//---------------------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
PIXEL filterPixel(double a, double b, double c, double d,
                  const TRasterPT<PIXEL> &rasIn, PIXEL *temp) {
  if (bool(a < b) == false && bool(b <= a) == false)
    return PIXEL::Transparent;  // NaNs
  else
    std::swap(a, b);

  if (bool(c < d) == false && bool(d <= c) == false)
    return PIXEL::Transparent;
  else
    std::swap(c, d);

  // Deal the magnification case, assuming that intervals have at least length
  // 1. This actually stands for:
  //  1. Their midpoint is bilinear filtered whenever their former length wass
  //  less than 1 (see fractionary
  //     parts computing above).
  //  2. This behaviour is continuous with respect to interval lengths - that
  //  is, we pass from supersampling to
  //     subsampling in a smooth manner.
  if (b - a < 1) {
    double v = 0.5 * (a + b);
    a        = v - 0.5;
    b        = v + 0.5;
  }

  if (d - c < 1) {
    double v = 0.5 * (c + d);
    c        = v - 0.5;
    d        = v + 0.5;
  }

  // Now, filter each column in [a, b]
  double x0 = std::max(a, 0.0);
  double x1 = std::min(b, (double)rasIn->getLx());

  if (x0 >= x1) return PIXEL::Transparent;

  int xEnd = tceil(x1);
  for (int x = tfloor(x0); x < xEnd; ++x)
    temp[x]  = filterPixel<PIXEL, CHANNEL_TYPE>(
        c, d, rasIn->pixels(0) + x, rasIn->getLy(), rasIn->getWrap());

  // Then, filter temp
  return filterPixel<PIXEL, CHANNEL_TYPE>(a, b, temp, rasIn->getLx(), 1);
}

//---------------------------------------------------------------------------------

template <typename T, typename CHANNEL_TYPE>
void resample(const TRasterPT<T> &rasIn, TRasterPT<T> &rasOut,
              const TDistorter &distorter, const TPoint &p) {
  if (rasOut->getLx() < 1 || rasOut->getLy() < 1 || rasIn->getLx() < 1 ||
      rasIn->getLy() < 1)
    return;

  int invsCount = distorter.maxInvCount();

  // Allocate buffers
  std::array<std::unique_ptr<TPointD[]>, 2> invs = {
      std::unique_ptr<TPointD[]>(
          new TPointD[invsCount * (rasOut->getLx() + 1)]),
      std::unique_ptr<TPointD[]>(
          new TPointD[invsCount * (rasOut->getLx() + 1)]),
  };
  std::array<std::unique_ptr<int[]>, 2> counts = {
      std::unique_ptr<int[]>(new int[rasOut->getLx() + 1]),
      std::unique_ptr<int[]>(new int[rasOut->getLx() + 1]),
  };
  std::unique_ptr<T[]> temp(new T[rasIn->getLx()]);
  TPointD shift(convert(p) + TPointD(0.5, 0.5));

  // Fill in the first inverses (lower edge of output image)
  {
    TPointD *currOldInv = invs[0].get();
    int *oldCounts      = counts[0].get();
    for (int x     = 0; x <= rasOut->getLx(); currOldInv += invsCount, ++x)
      oldCounts[x] = distorter.invMap(shift + TPointD(x, 0.0), currOldInv);
  }

  // For each output row
  for (int y = 0; y < rasOut->getLy(); ++y) {
    // Alternate inverse buffers
    TPointD *oldInvs = invs[y % 2].get();
    TPointD *newInvs = invs[(y + 1) % 2].get();
    int *oldCounts   = counts[y % 2].get();
    int *newCounts   = counts[(y + 1) % 2].get();

    // Build the new inverses
    {
      TPointD *currNewInv = newInvs;
      for (int x = 0; x <= rasOut->getLx(); currNewInv += invsCount, ++x)
        newCounts[x] =
            distorter.invMap(shift + TPointD(x, y + 1.0), currNewInv);
    }

    // Filter each pixel in the row
    T *pix              = rasOut->pixels(y);
    TPointD *currOldInv = oldInvs;
    TPointD *currNewInv = newInvs;
    for (int x = 0; x < rasOut->getLx();
         currOldInv += invsCount, currNewInv += invsCount, ++x, ++pix) {
      T pixDown(0, 0, 0, 0);

      int count = std::min({oldCounts[x], oldCounts[x + 1], newCounts[x]});
      for (int i = 0; i < count; ++i) {
        T pixUp(0, 0, 0, 0);

        pixUp = filterPixel<T, CHANNEL_TYPE>(
            currOldInv[i].x - 0.5, (currOldInv + invsCount)[i].x - 0.5,
            currOldInv[i].y - 0.5, currNewInv[i].y - 0.5, rasIn, temp.get());

        pixDown = overPix(pixDown, pixUp);
      }

      *pix = pixDown;
    }
  }
}

}  // namespace

//==============================================================================================

void distort(TRasterP &outRas, const TRasterP &inRas,
             const TDistorter &distorter, const TPoint &dstPos,
             const TRop::ResampleFilterType &filter) {
  TRaster32P inRas32      = inRas;
  TRaster64P inRas64      = inRas;
  TRasterCM32P inRasCM32  = inRas;
  TRaster32P outRas32     = outRas;
  TRaster64P outRas64     = outRas;
  TRasterCM32P outRasCM32 = outRas;

  if (filter == TRop::Bilinear) {
    if (inRas32)
      ::resample<TPixel32, UCHAR>(inRas32, outRas32, distorter, dstPos);
    else if (inRas64)
      ::resample<TPixel64, USHORT>(inRas64, outRas64, distorter, dstPos);
    else if (inRasCM32 && outRasCM32)
      ::resample(inRasCM32, outRasCM32, distorter, dstPos);
    else
      assert(0);
  } else if (filter == TRop::ClosestPixel) {
    if (inRas32)
      ::resampleClosestPixel<TPixel32, UCHAR>(inRas32, outRas32, distorter,
                                              dstPos);
    else if (inRas64)
      ::resampleClosestPixel<TPixel64, USHORT>(inRas64, outRas64, distorter,
                                               dstPos);
    else if (inRasCM32 && outRasCM32)
      ::resampleClosestPixel(inRasCM32, outRasCM32, distorter, dstPos);
    else
      assert(0);
  } else
    assert(0);
}

//================================================================================================
//
// BilinearDistorterBase
//
//================================================================================================

BilinearDistorterBase::BilinearDistorterBase(
    const TPointD &p00s, const TPointD &p10s, const TPointD &p01s,
    const TPointD &p11s, const TPointD &p00d, const TPointD &p10d,
    const TPointD &p01d, const TPointD &p11d)
    : TQuadDistorter(p00s, p10s, p01s, p11s, p00d, p10d, p01d, p11d)

{
  m_A = p00d;
  m_B = p10d - p00d;
  m_C = p01d - p00d;
  m_D = p11d - p01d - p10d + p00d;

  m_a  = m_D.x * m_C.y - m_C.x * m_D.y;
  m_b0 = m_B.x * m_C.y - m_C.x * m_B.y;
}

//--------------------------------------------------------------------------------

inline TPointD BilinearDistorterBase::map(const TPointD &p) const {
  double t = (p.x - m_p00s.x) / (m_p10s.x - m_p00s.x);
  double s = (p.y - m_p00s.y) / (m_p01s.y - m_p00s.y);

  return (1 - s) * (1 - t) * m_p00d + (1 - s) * t * m_p10d +
         s * (1 - t) * m_p01d + s * t * m_p11d;
}

//--------------------------------------------------------------------------------

int BilinearDistorterBase::invMap(const TPointD &p, TPointD *results) const {
  // See the book "Digital Image Warping" by G. Wolberg
  double b = m_D.x * (m_A.y - p.y) + m_D.y * (p.x - m_A.x) + m_b0;
  double c = m_B.x * (m_A.y - p.y) + m_B.y * (p.x - m_A.x);

  if (fabs(m_a) > 0.001) {
    // If delta <0, the points cannot be inverse-mapped
    double delta = sq(b) - 4.0 * m_a * c;
    if (delta < 0) return 0;

    double sqrtDelta = sqrt(delta);
    double k         = 0.5 / m_a;
    double v1        = k * (-b + sqrtDelta);
    double v2        = k * (-b - sqrtDelta);

    double u1, u2;
    double den = m_B.x + m_D.x * v1;
    if (fabs(den) > 0.01)
      u1 = (p.x - m_A.x - m_C.x * v1) / den;
    else
      u1 = (p.y - m_A.y - m_C.y * v1) / (m_B.y + m_D.y * v1);

    den = m_B.x + m_D.x * v2;
    if (fabs(den) > 0.01)
      u2 = (p.x - m_A.x - m_C.x * v2) / den;
    else
      u2 = (p.y - m_A.y - m_C.y * v2) / (m_B.y + m_D.y * v2);

    TPointD difP10_P00 = m_p10s - m_p00s;
    TPointD difP01_P00 = m_p01s - m_p00s;

    results[0] = m_p00s + u1 * difP10_P00 + v1 * difP01_P00;
    results[1] = m_p00s + u2 * difP10_P00 + v2 * difP01_P00;
    return 2;
  } else {
    // The equation reduces to first order.
    double v = -c / b;
    double u = (p.x - m_A.x - m_C.x * v) / (m_B.x + m_D.x * v);

    results[0] = m_p00s + u * (m_p10s - m_p00s) + v * (m_p01s - m_p00s);
    return 1;
  }
}

//================================================================================================
//
// BilinearDistorter
//
//================================================================================================

BilinearDistorter::BilinearDistorter(const TPointD &p00s, const TPointD &p10s,
                                     const TPointD &p01s, const TPointD &p11s,
                                     const TPointD &p00d, const TPointD &p10d,
                                     const TPointD &p01d, const TPointD &p11d)
    : TQuadDistorter(p00s, p10s, p01s, p11s, p00d, p10d, p01d, p11d) {
  m_refToSource.m_p00 = p00s;
  m_refToSource.m_p10 = p10s;
  m_refToSource.m_p01 = p01s;
  m_refToSource.m_p11 = p11s;
  m_refToDest.m_p00   = p00d;
  m_refToDest.m_p10   = p10d;
  m_refToDest.m_p01   = p01d;
  m_refToDest.m_p11   = p11d;

  m_refToSource.m_A = p00s;
  m_refToSource.m_B = p10s - p00s;
  m_refToSource.m_C = p01s - p00s;
  m_refToSource.m_D = p11s - p01s - p10s + p00s;
  m_refToDest.m_A   = p00d;
  m_refToDest.m_B   = p10d - p00d;
  m_refToDest.m_C   = p01d - p00d;
  m_refToDest.m_D   = p11d - p01d - p10d + p00d;

  m_refToSource.m_a = m_refToSource.m_D.x * m_refToSource.m_C.y -
                      m_refToSource.m_C.x * m_refToSource.m_D.y;
  m_refToSource.m_b0 = m_refToSource.m_B.x * m_refToSource.m_C.y -
                       m_refToSource.m_C.x * m_refToSource.m_B.y;
  m_refToDest.m_a = m_refToDest.m_D.x * m_refToDest.m_C.y -
                    m_refToDest.m_C.x * m_refToDest.m_D.y;
  m_refToDest.m_b0 = m_refToDest.m_B.x * m_refToDest.m_C.y -
                     m_refToDest.m_C.x * m_refToDest.m_B.y;
}

//---------------------------------------------------------------------------------

BilinearDistorter::~BilinearDistorter() {}

//--------------------------------------------------------------------------------

TPointD BilinearDistorter::map(const TPointD &p) const {
  TPointD sourceToRefPoints[2];
  int returnCount = m_refToSource.invMap(p, sourceToRefPoints);
  if (returnCount > 0) return m_refToDest.map(sourceToRefPoints[0]);

  return TConsts::napd;
}

//--------------------------------------------------------------------------------

inline int BilinearDistorter::invMap(const TPointD &p, TPointD *results) const {
  int returnCount = m_refToDest.invMap(p, results);
  for (int i   = 0; i < returnCount; ++i)
    results[i] = m_refToSource.map(results[i]);
  return returnCount;
}

//--------------------------------------------------------------------------------

inline TPointD BilinearDistorter::Base::map(const TPointD &p) const {
  return (1 - p.y) * (1 - p.x) * m_p00 + (1 - p.y) * p.x * m_p10 +
         p.y * (1 - p.x) * m_p01 + p.y * p.x * m_p11;
}

//--------------------------------------------------------------------------------

int BilinearDistorter::Base::invMap(const TPointD &p, TPointD *results) const {
  // See the book "Digital Image Warping" by G. Wolberg
  double b = m_D.x * (m_A.y - p.y) + m_D.y * (p.x - m_A.x) + m_b0;
  double c = m_B.x * (m_A.y - p.y) + m_B.y * (p.x - m_A.x);

  if (fabs(m_a) > 0.001) {
    // If delta <0, the points cannot be inverse-mapped
    double delta = sq(b) - 4.0 * m_a * c;
    if (delta < 0) return 0;

    double sqrtDelta = sqrt(delta);
    double k         = 0.5 / m_a;
    double v1        = k * (-b + sqrtDelta);
    double v2        = k * (-b - sqrtDelta);

    double u1, u2;
    double den = m_B.x + m_D.x * v1;
    if (fabs(den) > 0.01)
      u1 = (p.x - m_A.x - m_C.x * v1) / den;
    else
      u1 = (p.y - m_A.y - m_C.y * v1) / (m_B.y + m_D.y * v1);

    den = m_B.x + m_D.x * v2;
    if (fabs(den) > 0.01)
      u2 = (p.x - m_A.x - m_C.x * v2) / den;
    else
      u2 = (p.y - m_A.y - m_C.y * v2) / (m_B.y + m_D.y * v2);

    results[0] = TPointD(u1, v1);
    results[1] = TPointD(u2, v2);
    return 2;
  } else {
    // The equation reduces to first order.
    double v = -c / b;
    double u = (p.x - m_A.x - m_C.x * v) / (m_B.x + m_D.x * v);

    results[0] = TPointD(u, v);
    return 1;
  }
}

//=================================================================================
//
// TPerspect
//
//=================================================================================

PerspectiveDistorter::TPerspect::TPerspect()
    : a11(1.0)
    , a12(0.0)
    , a13(0.0)
    , a21(0.0)
    , a22(1.0)
    , a23(0.0)
    , a31(0.0)
    , a32(0.0)
    , a33(1.0) {}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect::TPerspect(double p11, double p12, double p13,
                                           double p21, double p22, double p23,
                                           double p31, double p32, double p33)
    : a11(p11)
    , a12(p12)
    , a13(p13)
    , a21(p21)
    , a22(p22)
    , a23(p23)
    , a31(p31)
    , a32(p32)
    , a33(p33) {}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect::TPerspect(const TPerspect &p)
    : a11(p.a11)
    , a12(p.a12)
    , a13(p.a13)
    , a21(p.a21)
    , a22(p.a22)
    , a23(p.a23)
    , a31(p.a31)
    , a32(p.a32)
    , a33(p.a33) {}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect::~TPerspect() {}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect &PerspectiveDistorter::TPerspect::operator=(
    const PerspectiveDistorter::TPerspect &p) {
  a11 = p.a11;
  a12 = p.a12;
  a13 = p.a13;
  a21 = p.a21;
  a22 = p.a22;
  a23 = p.a23;
  a31 = p.a31;
  a32 = p.a32;
  a33 = p.a33;
  return *this;
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect PerspectiveDistorter::TPerspect::operator*(
    const PerspectiveDistorter::TPerspect &p) const {
  return PerspectiveDistorter::TPerspect(
      a11 * p.a11 + a12 * p.a21 + a13 * p.a31,
      a11 * p.a12 + a12 * p.a22 + a13 * p.a32,
      a11 * p.a13 + a12 * p.a23 + a13 * p.a33,

      a21 * p.a11 + a22 * p.a21 + a23 * p.a31,
      a21 * p.a12 + a22 * p.a22 + a23 * p.a32,
      a21 * p.a13 + a22 * p.a23 + a23 * p.a33,

      a31 * p.a11 + a32 * p.a21 + a33 * p.a31,
      a31 * p.a12 + a32 * p.a22 + a33 * p.a32,
      a31 * p.a13 + a32 * p.a23 + a33 * p.a33);
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect PerspectiveDistorter::TPerspect::operator*(
    const TAffine &aff) const {
  return operator*(TPerspect(aff.a11, aff.a12, aff.a13, aff.a21, aff.a22,
                             aff.a23, 0.0, 0.0, 1.0));
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect operator*(
    const TAffine &aff, const PerspectiveDistorter::TPerspect &p) {
  return PerspectiveDistorter::TPerspect(aff.a11, aff.a12, aff.a13, aff.a21,
                                         aff.a22, aff.a23, 0.0, 0.0, 1.0) *
         p;
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect PerspectiveDistorter::TPerspect::operator*=(
    const PerspectiveDistorter::TPerspect &p) {
  return *this * p;
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect PerspectiveDistorter::TPerspect::inv() const {
  return TPerspect(
      a22 * a33 - a23 * a32, a13 * a32 - a12 * a33, a12 * a23 - a13 * a22,
      a23 * a31 - a21 * a33, a11 * a33 - a13 * a31, a13 * a21 - a11 * a23,
      a21 * a32 - a22 * a31, a12 * a31 - a11 * a32, a11 * a22 - a12 * a21);
}

//---------------------------------------------------------------------------------

double PerspectiveDistorter::TPerspect::det() const {
  return a11 * a22 * a33 + a12 * a23 * a31 + a13 * a21 * a32 - a13 * a22 * a31 -
         a11 * a23 * a32 - a12 * a21 * a33;
}

//---------------------------------------------------------------------------------

bool PerspectiveDistorter::TPerspect::operator==(
    const PerspectiveDistorter::TPerspect &p) const {
  return a11 == p.a11 && a12 == p.a12 && a13 == p.a13 && a21 == p.a21 &&
         a22 == p.a22 && a23 == p.a23 && a31 == p.a31 && a32 == p.a32 &&
         a33 == p.a33;
}

//---------------------------------------------------------------------------------

bool PerspectiveDistorter::TPerspect::operator!=(
    const PerspectiveDistorter::TPerspect &p) const {
  return !(*this == p);
}

//---------------------------------------------------------------------------------

bool PerspectiveDistorter::TPerspect::isIdentity(double err) const {
  return ((a11 - 1.0) * (a11 - 1.0) + (a22 - 1.0) * (a22 - 1.0) +
          (a33 - 1.0) * (a33 - 1.0) + a12 * a12 + a13 * a13 + a21 * a21 +
          a23 * a23 + a31 * a31 + a32 * a32) < err;
}

//---------------------------------------------------------------------------------

TPointD PerspectiveDistorter::TPerspect::operator*(const TPointD &p) const {
  double den = (a31 * p.x + a32 * p.y + a33);
  double x   = (a11 * p.x + a12 * p.y + a13) / den;
  double y   = (a21 * p.x + a22 * p.y + a23) / den;
  return TPointD(x, y);
}

//---------------------------------------------------------------------------------

T3DPointD PerspectiveDistorter::TPerspect::operator*(const T3DPointD &p) const {
  return T3DPointD(a11 * p.x + a12 * p.y + a13 * p.z,
                   a21 * p.x + a22 * p.y + a23 * p.z,
                   a31 * p.x + a32 * p.y + a33 * p.z);
}

//---------------------------------------------------------------------------------

TRectD PerspectiveDistorter::TPerspect::operator*(const TRectD &rect) const {
  if (rect != TConsts::infiniteRectD) {
    TPointD p1 = *this * rect.getP00(), p2 = *this * rect.getP01(),
            p3 = *this * rect.getP10(), p4 = *this * rect.getP11();
    return TRectD(
        std::min({p1.x, p2.x, p3.x, p4.x}), std::min({p1.y, p2.y, p3.y, p4.y}),
        std::max({p1.x, p2.x, p3.x, p4.x}), std::max({p1.y, p2.y, p3.y, p4.y}));
  } else
    return TConsts::infiniteRectD;
}

//================================================================================================
//
// PerspectiveDistorter
//
//================================================================================================

PerspectiveDistorter::PerspectiveDistorter(
    const TPointD &p00s, const TPointD &p10s, const TPointD &p01s,
    const TPointD &p11s, const TPointD &p00d, const TPointD &p10d,
    const TPointD &p01d, const TPointD &p11d)
    : TQuadDistorter(p00s, p10s, p01s, p11s, p00d, p10d, p01d, p11d) {
  computeMatrix();
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::~PerspectiveDistorter() {}

//---------------------------------------------------------------------------------

void PerspectiveDistorter::computeMatrix() {
  // Since source and destination points are intended in hundreds of pixels in
  // their refs,
  // and inverting makes squares with respect to their elements' size, we'd
  // better put the
  // quadrilaterals in more numerically stable references before inversions.

  double srcSize = std::max({dist(m_p00s, m_p10s), dist(m_p00s, m_p01s),
                             dist(m_p10s, m_p11s), dist(m_p01s, m_p11s)});
  double dstSize = std::max({dist(m_p00d, m_p10d), dist(m_p00d, m_p01d),
                             dist(m_p10d, m_p11d), dist(m_p01d, m_p11d)});

  TAffine toSrcNormalizedRef(TScale(1.0 / srcSize) * TTranslation(-m_p00s));
  TAffine toSrcRef(TTranslation(m_p00s) * TScale(srcSize));

  TAffine toDstNormalizedRef(TScale(1.0 / dstSize) * TTranslation(-m_p00d));
  TAffine toDstRef(TTranslation(m_p00d) * TScale(dstSize));

  TPointD p00s;
  TPointD p10s(toSrcNormalizedRef * m_p10s);
  TPointD p01s(toSrcNormalizedRef * m_p01s);
  TPointD p11s(toSrcNormalizedRef * m_p11s);

  TPointD p00d;
  TPointD p10d(toDstNormalizedRef * m_p10d);
  TPointD p01d(toDstNormalizedRef * m_p01d);
  TPointD p11d(toDstNormalizedRef * m_p11d);

  // compute a matrix from (0,0), (1,0), (1,1), (0,1) to m_startPoints.
  TPerspect m1 = computeSquareToMatrix(p00s, p10s, p01s, p11s);

  // compute a matrix from (0,0), (1,0), (1,1), (0,1) to m_endPoints.
  TPerspect m2 = computeSquareToMatrix(p00d, p10d, p01d, p11d);

  m_matrix    = m2 * m1.inv();
  m_matrixInv = toSrcRef * m_matrix.inv() * toDstNormalizedRef;
  m_matrix    = toDstRef * m_matrix * toSrcNormalizedRef;
}

//---------------------------------------------------------------------------------

double PerspectiveDistorter::determinant(double a11, double a12, double a21,
                                         double a22) {
  return a11 * a22 - a12 * a21;
}

//---------------------------------------------------------------------------------

PerspectiveDistorter::TPerspect PerspectiveDistorter::computeSquareToMatrix(
    const TPointD &p00, const TPointD &p10, const TPointD &p01,
    const TPointD &p11) {
  TPointD d1 = p10 - p11;
  TPointD d2 = p01 - p11;
  TPointD d3 = p00 - p10 + p11 - p01;

  TPerspect matrix;

  matrix.a31 =
      determinant(d3.x, d2.x, d3.y, d2.y) / determinant(d1.x, d2.x, d1.y, d2.y);
  matrix.a32 =
      determinant(d1.x, d3.x, d1.y, d3.y) / determinant(d1.x, d2.x, d1.y, d2.y);
  matrix.a11 = p10.x - p00.x + matrix.a31 * p10.x;
  matrix.a12 = p01.x - p00.x + matrix.a32 * p01.x;
  matrix.a13 = p00.x;
  matrix.a21 = p10.y - p00.y + matrix.a31 * p10.y;
  matrix.a22 = p01.y - p00.y + matrix.a32 * p01.y;
  matrix.a23 = p00.y;
  matrix.a33 = 1;

  return matrix;
}

//---------------------------------------------------------------------------------

TPointD PerspectiveDistorter::map(const TPointD &p) const {
  return m_matrix * p;
}

//---------------------------------------------------------------------------------

int PerspectiveDistorter::invMap(const TPointD &p, TPointD *results) const {
  results[0] = m_matrixInv * p;
  return 1;
}

//**********************************************************************************************
//    Rect inversion methods
//**********************************************************************************************

//=============================
//    PerspectiveDistorter
//=============================

// IDEA: Lines are mapped into lines through this distortion and through the
// inverse; plus,
// the distortion is 1-1. There is one major issue: across the horizon line of
// the perspective
// projection (ie the pre-image of {z=0}, which could eventually degenerate to
// the whole plane)
// the jacobian sign is inverted. Observe, further, that the mapping is
// infinitely differentiable
// apart from neighbourhoods of the horizon line.
// It can be shown that bounding estimates based on corner samples on one side
// of the horizon line
// may prove false on the other. Therefore, we shall separate such estimates in
// the two cases,
// and then sum them together.

//---------------------------------------------------------------------------------

//! Returns the jacobian (on source ref) associated to the pre-image of passed
//! destination point.
void PerspectiveDistorter::getJacobian(const TPointD &destPoint,
                                       TPointD &srcPoint, TPointD &xDer,
                                       TPointD &yDer) const {
  srcPoint = m_matrixInv * destPoint;

  T3DPointD im(m_matrix * T3DPointD(srcPoint.x, srcPoint.y, 1.0));

  double imZInv = 1.0 / im.z, minusImZInvSq = -sq(imZInv);
  TPerspect projectionJac(imZInv, 0.0, minusImZInvSq * im.x, 0.0, imZInv,
                          minusImZInvSq * im.y, 0.0, 0.0, 0.0);

  TPerspect result(projectionJac * m_matrix);
  xDer.x = result.a11;
  xDer.y = result.a21;
  yDer.x = result.a12;
  yDer.y = result.a22;
}

//---------------------------------------------------------------------------------

inline void updateResult(const TPointD &srcCorner, const TPointD &xDer,
                         const TPointD &yDer, int rectSideX, int rectSideY,
                         bool &hasPositiveResults, bool &hasNegativeResults,
                         TRectD &posResult, TRectD &negResult) {
  const int securityAddendum = 5;  // Adding a 5 border to the result just to be
                                   // sure about approx errors...

  int jacobianSign = tsign(cross(xDer, yDer));

  int sideDerXAgainstRectSideX = rectSideX * tsign(-xDer.y);
  int sideDerXAgainstRectSideY = rectSideY * tsign(xDer.x);
  int sideDerYAgainstRectSideX = rectSideX * tsign(-yDer.y);
  int sideDerYAgainstRectSideY = rectSideY * tsign(yDer.x);

  if (jacobianSign > 0) {
    hasPositiveResults = true;

    if (sideDerXAgainstRectSideX != -sideDerXAgainstRectSideY)
      // Rect lies on one side of the derivative line extension. Therefore, the
      // inverted rect can be updated.
      if (sideDerXAgainstRectSideX > 0 || sideDerXAgainstRectSideY > 0)
        posResult.y0 = std::min(posResult.y0, srcCorner.y - securityAddendum);
      else
        posResult.y1 = std::max(posResult.y1, srcCorner.y + securityAddendum);

    if (sideDerYAgainstRectSideX != -sideDerYAgainstRectSideY)
      if (sideDerYAgainstRectSideX > 0 || sideDerYAgainstRectSideY > 0)
        posResult.x1 = std::max(posResult.x1, srcCorner.x + securityAddendum);
      else
        posResult.x0 = std::min(posResult.x0, srcCorner.x - securityAddendum);
  } else if (jacobianSign < 0) {
    hasNegativeResults = true;

    if (sideDerXAgainstRectSideX != -sideDerXAgainstRectSideY)
      if (sideDerXAgainstRectSideX > 0 || sideDerXAgainstRectSideY > 0)
        negResult.y1 = std::max(posResult.y1, srcCorner.y + securityAddendum);
      else
        negResult.y0 = std::min(posResult.y0, srcCorner.y - securityAddendum);

    if (sideDerYAgainstRectSideX != -sideDerYAgainstRectSideY)
      if (sideDerYAgainstRectSideX > 0 || sideDerYAgainstRectSideY > 0)
        negResult.x0 = std::min(posResult.x0, srcCorner.x - securityAddendum);
      else
        negResult.x1 = std::max(posResult.x1, srcCorner.x + securityAddendum);
  }
}

//---------------------------------------------------------------------------------

TRectD PerspectiveDistorter::invMap(const TRectD &rect) const {
  // For each corner, find the jacobian. Then, decide by which side the rect's
  // pre-image lie with respect to the partial derivatives.
  // Observe that the two horizon-separated semiplanes do not compete -
  // the requirement for each is added to the result.

  TPointD srcCorner, xDer, yDer;

  double maxD = (std::numeric_limits<double>::max)();
  TRectD positiveResult(maxD, maxD, -maxD, -maxD);
  TRectD negativeResult(positiveResult);
  bool hasPositiveResults = false, hasNegativeResults = false;

  getJacobian(rect.getP00(), srcCorner, xDer, yDer);
  updateResult(srcCorner, xDer, yDer, 1, 1, hasPositiveResults,
               hasNegativeResults, positiveResult, negativeResult);

  getJacobian(rect.getP10(), srcCorner, xDer, yDer);
  updateResult(srcCorner, xDer, yDer, -1, 1, hasPositiveResults,
               hasNegativeResults, positiveResult, negativeResult);

  getJacobian(rect.getP01(), srcCorner, xDer, yDer);
  updateResult(srcCorner, xDer, yDer, 1, -1, hasPositiveResults,
               hasNegativeResults, positiveResult, negativeResult);

  getJacobian(rect.getP11(), srcCorner, xDer, yDer);
  updateResult(srcCorner, xDer, yDer, -1, -1, hasPositiveResults,
               hasNegativeResults, positiveResult, negativeResult);

  // If some maxD remain, no bound on that side was found. So replace with
  // the opposite (unlimited on that side) maxD.
  if (positiveResult.x0 == maxD) positiveResult.x0  = -maxD;
  if (positiveResult.x1 == -maxD) positiveResult.x1 = maxD;
  if (positiveResult.y0 == maxD) positiveResult.y0  = -maxD;
  if (positiveResult.y1 == -maxD) positiveResult.y1 = maxD;

  if (negativeResult.x0 == maxD) negativeResult.x0  = -maxD;
  if (negativeResult.x1 == -maxD) negativeResult.x1 = maxD;
  if (negativeResult.y0 == maxD) negativeResult.y0  = -maxD;
  if (negativeResult.y1 == -maxD) negativeResult.y1 = maxD;

  return hasPositiveResults
             ? hasNegativeResults ? positiveResult + negativeResult
                                  : positiveResult
             : hasNegativeResults ? negativeResult : TConsts::infiniteRectD;
}

//=================================================================================

//=============================
//    BilinearDistorter
//=============================

// IDEA: This time, lines are mapped to curves, in general. Plus, from 0 to 2
// possible inverses
// may exist for the same destination point.

// Given the separation of the bilinear mapping in two bilinear mappings with an
// intermediate reference for the
// convex coordinates, it can be shown that a bounding estimate for the inverse
// of a rect can be found this way:
//  1. Inverse-map the corners of the rect in the convex-reference (ie find
//  their convex coordinates)
//  2. Make their bounding box
//  3. Map its corners to the source reference
//  4. Return their bounding box

// In order to show that this actually works, consider the following result:

// A rect maps to a convex quadrilateral through a forward bilinear mapping (ie
// passages 3->4 and 2->1).
// It is sufficient to consider that lines map to lines through one such
// mapping, and that since it is
// also everywhere differentiable, its local behaviour around corners is that of
// a linear function, through
// which convex angles are only mappable to other convex angles.

//---------------------------------------------------------------------------------

TRectD BilinearDistorter::invMap(const TRectD &rect) const {
  // Build the convex coordinates of the rect's corners.
  TPointD invs[8];
  int count[4];

  count[0] = m_refToDest.invMap(rect.getP00(), &invs[0]);
  count[1] = m_refToDest.invMap(rect.getP10(), &invs[2]);
  count[2] = m_refToDest.invMap(rect.getP01(), &invs[4]);
  count[3] = m_refToDest.invMap(rect.getP11(), &invs[6]);

  double maxD = (std::numeric_limits<double>::max)();
  TRectD bbox(maxD, maxD, -maxD, -maxD);

  int i, j;
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < count[i]; ++j) {
      TPointD &inv(invs[j + 2 * i]);
      bbox.x0 = std::min(bbox.x0, inv.x);
      bbox.y0 = std::min(bbox.y0, inv.y);
      bbox.x1 = std::max(bbox.x1, inv.x);
      bbox.y1 = std::max(bbox.y1, inv.y);
    }
  }

  if (bbox.x1 <= bbox.x0 || bbox.y1 <= bbox.y0)
    return TConsts::infiniteRectD;  // Should happen only if all counts are 0

  invs[0] = m_refToSource.map(bbox.getP00());
  invs[1] = m_refToSource.map(bbox.getP10());
  invs[2] = m_refToSource.map(bbox.getP01());
  invs[3] = m_refToSource.map(bbox.getP11());

  bbox.x0 = std::min({invs[0].x, invs[1].x, invs[2].x, invs[3].x});
  bbox.y0 = std::min({invs[0].y, invs[1].y, invs[2].y, invs[3].y});
  bbox.x1 = std::max({invs[0].x, invs[1].x, invs[2].x, invs[3].x});
  bbox.y1 = std::max({invs[0].y, invs[1].y, invs[2].y, invs[3].y});

  return bbox.enlarge(5);  // Enlarge a little just to be sure
}
