

// tcg includes
#include "tcg/tcg_misc.h"

#include "trop.h"

/*! \file terodilate.cpp

This file contains an implementation of a greyscale (ie per-channel)
erode/dilate
morphological operator, following the van Herk/Gil-Werman O(row*cols) algorithm.

An extension with circular structuring element is attempted - unfortunately I
could
not retrieve a copy of Miyataka's paper about that, which seemingly claimed
O(rows * cols) too. The implemented algorithm is a sub-optimal
O(rows*cols*radius).
*/

//********************************************************
//    Auxiliary  functions
//********************************************************

namespace {

template <typename Pix>
void copyMatte(const TRasterPT<Pix> &src,
               const TRasterPT<typename Pix::Channel> &matte) {
  typedef typename Pix::Channel Chan;

  int y, lx = src->getLx(), ly = src->getLy();
  for (y = 0; y != ly; ++y) {
    Pix *s, *sBegin = src->pixels(y), *sEnd = sBegin + lx;
    Chan *m, *mBegin = matte->pixels(y);

    for (s = sBegin, m = mBegin; s != sEnd; ++s, ++m) *m = s->m;
  }
}

//--------------------------------------------------------------

template <typename Pix>
void copyChannels_erode(const TRasterPT<Pix> &src,
                        const TRasterPT<typename Pix::Channel> &matte,
                        const TRasterPT<Pix> &dst) {
  typedef typename Pix::Channel Chan;

  // Just assemble src and matte, remembering to depremultiply src pixels before
  // applying the new matte

  double fac;

  int y, lx = src->getLx(), ly = src->getLy();
  for (y = 0; y != ly; ++y) {
    const Pix *s, *sBegin = src->pixels(y), *sEnd = sBegin + lx;
    Pix *d, *dBegin = dst->pixels(y);

    Chan *m, *mBegin = matte->pixels(y);

    for (s = sBegin, d = dBegin, m = mBegin; s != sEnd; ++s, ++d, ++m) {
      fac  = double(*m) / double(s->m);
      d->r = fac * s->r, d->g = fac * s->g, d->b = fac * s->b, d->m = *m;
    }
  }
}

//--------------------------------------------------------------

template <typename Pix>
void copyChannels_dilate(const TRasterPT<Pix> &src,
                         const TRasterPT<typename Pix::Channel> &matte,
                         const TRasterPT<Pix> &dst) {
  typedef typename Pix::Channel Chan;

  // Trickier - since src is presumably premultiplied, increasing its pixels'
  // alpha by direct
  // substitution would expose the excessive RGB discretization of pixels with a
  // low matte value.
  // So, let's just put the pixels on a black background. It should do fine.

  double max = Pix::maxChannelValue;

  int y, lx = src->getLx(), ly = src->getLy();
  for (y = 0; y != ly; ++y) {
    const Pix *s, *sBegin = src->pixels(y), *sEnd = sBegin + lx;
    Pix *d, *dBegin = dst->pixels(y);

    const Chan *m, *mBegin = matte->pixels(y);

    for (s = sBegin, d = dBegin, m = mBegin; s != sEnd; ++s, ++d, ++m) {
      *d   = *s;
      d->m = s->m + (1.0 - s->m / max) * *m;
    }
  }
}

}  // namespace

//********************************************************
//    EroDilate  algorithms
//********************************************************

namespace {

template <typename Chan>
struct MaxFunc {
  inline Chan operator()(const Chan &a, const Chan &b) {
    return std::max(a, b);
  }
};

template <typename Chan>
struct MinFunc {
  inline Chan operator()(const Chan &a, const Chan &b) {
    return std::min(a, b);
  }
};

//--------------------------------------------------------------

// NOTE: src and dst must be NOT OVERLAPPING (eg src != dst)

template <typename Chan, typename Func>
void erodilate_row(int len, const Chan *src, int sIncr, Chan *dst, int dIncr,
                   int rad, double radR, Func func) {
  assert(rad >= 0);

  // Segment the row of specified length into wCount windows of max wSize
  // elements
  int w, wSize = 2 * rad + 1, wCount = len / wSize + 1;
  int swIncr = wSize * sIncr, srIncr = rad * sIncr;
  int dwIncr = wSize * dIncr, drIncr = rad * dIncr;

  const Chan *s, *sEnd = src + len * sIncr;
  Chan *d, *dEnd       = dst + len * dIncr;

  double one_radR = (1.0 - radR);

  for (w = 0; w != wCount; ++w) {
    Chan *dwBegin = dst + w * dwIncr, *dwEnd = std::min(dwBegin + dwIncr, dEnd);

    // Compute prefixes
    const Chan *swBegin = src + std::max(w * swIncr - srIncr - sIncr, 0),
               *swEnd =
                   src + std::min(w * swIncr + srIncr + sIncr, len * sIncr);

    s = swEnd - sIncr, d = dst + ((s - src) / sIncr) * dIncr +
                           drIncr;  // d already decremented by dIncr

    Chan val = *s, oldVal;

    for (s -= sIncr; (d >= dEnd) && (s >= swBegin);
         s -= sIncr, d -= dIncr)  // s decremented here
    {
      assert(s >= src);
      assert(s < sEnd);
      assert((s - src) % sIncr == 0);
      assert(d >= dst);
      assert((d - dst) % dIncr == 0);

      val = func(oldVal = val, *s);
    }

    for (; s >= swBegin; s -= sIncr, d -= dIncr) {
      assert(s >= src);
      assert(s < sEnd);
      assert((s - src) % sIncr == 0);
      assert(d >= dst);
      assert(d < dEnd);
      assert((d - dst) % dIncr == 0);

      val = func(oldVal = val, *s);
      *d = (oldVal == val) ? val : one_radR * oldVal + radR * val;
    }

    for (d = std::min(d, dEnd - dIncr); d >= dwBegin; d -= dIncr) {
      assert(d >= dst);
      assert(d < dEnd);
      assert((d - dst) % dIncr == 0);

      val = func(oldVal = val, 0);
      *d = (oldVal == val) ? val : one_radR * oldVal + radR * val;
    }

    // Compute suffixes
    swBegin = src + w * swIncr + srIncr,
    swEnd   = std::min(swBegin + swIncr + sIncr, sEnd);
    if (swBegin >= swEnd) continue;

    s = swBegin, d = dwBegin;

    val = *s;

    for (s += sIncr; (s < swEnd); s += sIncr, d += dIncr) {
      assert(s >= src);
      assert(s < sEnd);
      assert((s - src) % sIncr == 0);
      assert(d >= dst);
      assert(d < dEnd);
      assert((d - dst) % dIncr == 0);

      val = func(oldVal = val, *s);
      *d = func(*d, (oldVal == val) ? val : one_radR * oldVal + radR * val);
    }

    for (; d < dwEnd; d += dIncr) {
      assert(d >= dst);
      assert(d < dEnd);
      assert((d - dst) % dIncr == 0);

      val = func(oldVal = val, 0);
      *d = func(*d, (oldVal == val) ? val : one_radR * oldVal + radR * val);
    }
  }
}

//--------------------------------------------------------------

template <typename Pix, typename Chan>
void erodilate_chan(const TRasterPT<Pix> &src, const TRasterPT<Chan> &dst,
                    double radius, bool dilate) {
  assert(radius > 0.0);

  int radI    = tfloor(radius);
  double radR = radius - radI;

  // Using a temporary raster to keep intermediate results. This allows us to
  // perform a cache-friendly iteration in the separable/square kernel case
  int x, y, lx = src->getLx(), ly = src->getLy();

  // Peform rows erodilation
  TRasterPT<Chan> temp(ly, lx);  // Notice transposition plz

  {
    if (dilate)
      for (y = 0; y != ly; ++y)
        ::erodilate_row(lx, &src->pixels(y)->m, 4, temp->pixels(0) + y, ly,
                        radI, radR, MaxFunc<Chan>());
    else
      for (y = 0; y != ly; ++y)
        ::erodilate_row(lx, &src->pixels(y)->m, 4, temp->pixels(0) + y, ly,
                        radI, radR, MinFunc<Chan>());
  }

  // Perform columns erodilation
  {
    if (dilate)
      for (x = 0; x != lx; ++x)
        ::erodilate_row(ly, temp->pixels(x), 1, dst->pixels(0) + x,
                        dst->getWrap(), radI, radR, MaxFunc<Chan>());
    else
      for (x = 0; x != lx; ++x)
        ::erodilate_row(ly, temp->pixels(x), 1, dst->pixels(0) + x,
                        dst->getWrap(), radI, radR, MinFunc<Chan>());
  }
}

//--------------------------------------------------------------

template <typename Pix>
void rect_erodilate(const TRasterPT<Pix> &src, const TRasterPT<Pix> &dst,
                    double radius) {
  typedef typename Pix::Channel Chan;

  if (radius == 0.0) {
    // No-op case
    TRop::copy(dst, src);
    return;
  }

  bool dilate = (radius >= 0.0);

  // Perform columns erodilation
  TRasterPT<Chan> temp(src->getLx(), src->getLy());
  ::erodilate_chan(src, temp, fabs(radius), dilate);

  // Remember that we have just calculated the matte values. We still have to
  // apply them to the old RGB
  // values, which requires depremultiplying from source matte and
  // premultiplying with the new one.
  if (dilate)
    ::copyChannels_dilate(src, temp, dst);
  else
    ::copyChannels_erode(src, temp, dst);
}

}  // namespace

//********************************************************
//    EroDilate  round algorithm
//********************************************************

namespace {

template <typename Chan, typename Func>
void erodilate_quarters(int lx, int ly, Chan *src, int sIncrX, int sIncrY,
                        Chan *dst, int dIncrX, int dIncrY, double radius,
                        double shift, Func func) {
  double sqRadius     = sq(radius);
  double squareHeight = radius * M_SQRT1_2;
  int squareHeightI   = tfloor(squareHeight);

  // For every arc point
  int arcY;
  for (arcY = -squareHeightI; arcY <= squareHeightI; ++arcY) {
    // Calculate x and weights
    double sqArcY = sq(arcY);
    assert(sqRadius >= sqArcY);

    double x = shift + sqrt(sqRadius - sqArcY) - squareHeight;

    int arcX = tfloor(x);
    double w = x - arcX, one_w = 1.0 - w;

    // Build dst area influenced by the arc point. Func with 0 outside that.
    TRect bounds(0, 0, lx, ly);

    TRect dRect(bounds * (bounds + TPoint(-arcX, -arcY)));
    TRect sRect(bounds * (bounds + TPoint(arcX, arcY)));

    int sy, dy;

    // Func with 0 before dRect.y0
    for (dy = 0; dy < dRect.y0; ++dy) {
      Chan *d, *dBegin = dst + dy * dIncrY, *dEnd = dBegin + lx * dIncrX;
      for (d = dBegin; d != dEnd; d += dIncrX) {
        // assert(d >= dst); assert(d < dEnd); assert((d-dst) % dIncrX == 0);
        *d = func(*d, 0);
      }
    }

    // Func with 0 after dRect.y1
    for (dy = dRect.y1; dy < ly; ++dy) {
      Chan *d, *dBegin = dst + dy * dIncrY, *dEnd = dBegin + lx * dIncrX;
      for (d = dBegin; d != dEnd; d += dIncrX) {
        // assert(d >= dst); assert(d < dEnd); assert((d-dst) % dIncrX == 0);
        *d = func(*d, 0);
      }
    }

    // For every dst pixel in the area, Func with the corresponding pixel in src
    for (dy = dRect.y0, sy = sRect.y0; dy != dRect.y1; ++dy, ++sy) {
      Chan *d, *dLine = dst + dy * dIncrY, *dBegin = dLine + dRect.x0 * dIncrX;
      Chan *s, *sLine = src + sy * sIncrY, *sBegin = sLine + sRect.x0 * sIncrX,
               *sEnd = sLine + sRect.x1 * sIncrX;

      Chan *sLast = sEnd - sIncrX;  // sLast would lerp with sEnd

      for (d = dBegin, s = sBegin; s != sLast;
           d += dIncrX, s += sIncrX)  // hence we stop before it
      {
        // assert(s >= src); assert(s < sEnd); assert((s-src) % sIncrX == 0);
        // assert(d >= dst); assert(d < dEnd); assert((d-dst) % dIncrX == 0);

        *d = func(*d, *s * one_w + *(s + sIncrX) * w);
      }

      // assert(s >= src); assert(s < sEnd); assert((s-src) % sIncrX == 0);
      // assert(d >= dst); assert(d < dEnd); assert((d-dst) % dIncrX == 0);

      *d = func(*d, *s * one_w);  // lerp sLast with 0
    }
  }
}

//--------------------------------------------------------------

template <typename Pix>
void circular_erodilate(const TRasterPT<Pix> &src, const TRasterPT<Pix> &dst,
                        double radius) {
  typedef typename Pix::Channel Chan;

  if (radius == 0.0) {
    // No-op case
    TRop::copy(dst, src);
    return;
  }

  // Ok, the idea is: consider the maximal embedded square in our circular
  // structuring element.
  // Erodilating by it consists in the consecutive erodilation by rows and
  // columns with the same
  // 'square' radius. Now, it's easy to see that the square could be 'bent' so
  // that one of its
  // edges matches that of a 1/4 of the circle's edge, while remaining inside
  // the circle.
  // Erodilating by the bent square can be achieved by erodilating first by rows
  // or column for
  // the square edge radius, followed by perpendicular erodilationg with a
  // fourth of our
  // circumference. Sum the 4 erodilations needed to complete the circumference
  // - and it's done.

  // NOTE: Unfortunately, the above decomposition has lots of intersections
  // among the pieces - yet
  // it's simple enough and removes an O(radius) from the naive algorithm. Could
  // be done better?

  // First, build the various erodilation data
  bool dilate = (radius >= 0.0);
  radius      = fabs(radius);

  double inner_square_diameter = radius * M_SQRT2;

  double shift =
      0.25 *
      inner_square_diameter;  // Shift of the bent square SE needed to avoid
                              // touching the circumference on the other side
  double row_filter_radius = 0.5 * (inner_square_diameter - shift);
  double cseShift = 0.5 * shift;  // circumference structuring element shift

  int lx = src->getLx(), ly = src->getLy();

  TRasterPT<Chan> temp1(lx, ly), temp2(lx, ly);

  int radI    = tfloor(row_filter_radius);
  double radR = row_filter_radius - radI;

  if (dilate) {
    temp2->fill(0);  // Initialize with a Func-neutral value

    if (row_filter_radius > 0.0)
      for (int y = 0; y != ly; ++y)
        ::erodilate_row(lx, &src->pixels(y)->m, 4, temp1->pixels(y), 1, radI,
                        radR, MaxFunc<Chan>());
    else
      ::copyMatte(src, temp1);

    ::erodilate_quarters(lx, ly, temp1->pixels(0), 1, lx, temp2->pixels(0), 1,
                         lx, radius, cseShift, MaxFunc<Chan>());
    ::erodilate_quarters(lx, ly, temp1->pixels(0) + lx - 1, -1, lx,
                         temp2->pixels(0) + lx - 1, -1, lx, radius, cseShift,
                         MaxFunc<Chan>());

    if (row_filter_radius > 0.0)
      for (int x = 0; x != lx; ++x)
        ::erodilate_row(ly, &src->pixels(0)[x].m, 4 * src->getWrap(),
                        temp1->pixels(0) + x, lx, radI, radR, MaxFunc<Chan>());
    else
      ::copyMatte(src, temp1);

    ::erodilate_quarters(ly, lx, temp1->pixels(0), lx, 1, temp2->pixels(0), lx,
                         1, radius, cseShift, MaxFunc<Chan>());
    ::erodilate_quarters(ly, lx, temp1->pixels(0) + lx * ly - 1, -lx, -1,
                         temp2->pixels(0) + lx * ly - 1, -lx, -1, radius,
                         cseShift, MaxFunc<Chan>());
  } else {
    temp2->fill((std::numeric_limits<Chan>::max)());  // Initialize with a
                                                      // Func-neutral value

    if (row_filter_radius > 0.0)
      for (int y = 0; y != ly; ++y)
        ::erodilate_row(lx, &src->pixels(y)->m, 4, temp1->pixels(y), 1, radI,
                        radR, MinFunc<Chan>());
    else
      ::copyMatte(src, temp1);

    ::erodilate_quarters(lx, ly, temp1->pixels(0), 1, lx, temp2->pixels(0), 1,
                         lx, radius, cseShift, MinFunc<Chan>());
    ::erodilate_quarters(lx, ly, temp1->pixels(0) + lx - 1, -1, lx,
                         temp2->pixels(0) + lx - 1, -1, lx, radius, cseShift,
                         MinFunc<Chan>());

    if (row_filter_radius > 0.0)
      for (int x = 0; x != lx; ++x)
        ::erodilate_row(ly, &src->pixels(0)[x].m, 4 * src->getWrap(),
                        temp1->pixels(0) + x, lx, radI, radR, MinFunc<Chan>());
    else
      ::copyMatte(src, temp1);

    ::erodilate_quarters(ly, lx, temp1->pixels(0), lx, 1, temp2->pixels(0), lx,
                         1, radius, cseShift, MinFunc<Chan>());
    ::erodilate_quarters(ly, lx, temp1->pixels(0) + lx * ly - 1, -lx, -1,
                         temp2->pixels(0) + lx * ly - 1, -lx, -1, radius,
                         cseShift, MinFunc<Chan>());
  }

  // Remember that we have just calculated the matte values. We still have to
  // apply them to the old RGB
  // values, which requires depremultiplying from source matte and
  // premultiplying with the new one.
  if (dilate)
    ::copyChannels_dilate(src, temp2, dst);
  else
    ::copyChannels_erode(src, temp2, dst);
}

}  // namespace

//********************************************************
//    EroDilate  main functions
//********************************************************

void TRop::erodilate(const TRasterP &src, const TRasterP &dst, double radius,
                     ErodilateMaskType type) {
  assert(src->getSize() == dst->getSize());

  src->lock(), dst->lock();

  if ((TRaster32P)src && (TRaster32P)dst) switch (type) {
    case ED_rectangular:
      ::rect_erodilate<TPixel32>(src, dst, radius);
      break;
    case ED_circular:
      ::circular_erodilate<TPixel32>(src, dst, radius);
      break;
    default:
      assert(!"Unknown mask type");
      break;
    }
  else if ((TRaster64P)src && (TRaster64P)dst)
    switch (type) {
    case ED_rectangular:
      ::rect_erodilate<TPixel64>(src, dst, radius);
      break;
    case ED_circular:
      ::circular_erodilate<TPixel64>(src, dst, radius);
      break;
    default:
      assert(!"Unknown mask type");
      break;
    }
  else
    assert(!"Unsupported raster type!");

  src->unlock(), dst->unlock();
}
