

#include "trop.h"

/*
See Alexander Reshetov's "Morphological Antialiasing" paper on Intel Labs site.

Basically, this antialiasing algorithm is based on the following ideas:

 - Suppose that our image is just made up of flat colors. Then, a simple
antialiasing
   approach is that of assuming that the 'actual' line separating two distinct
colors
   is the polyline that passes through the midpoint of each edge of its original
jaggy
   counterpart.
   As pixels around the border are cut through by the polyline, the area of the
pixel
   that is filled of a certain color is its weight in the output filtered pixel.

 - The above method can be applied on each single uniform piece of a scanline,
considering
   the lines originated by the vertical extensions of its left and right edges.

 - Of these lines, only those which lie completely on pixels adjacent to the
edge are
   considered - so that the antialiasing effect is kept only around the
contours.

This algorithm would yield a good result at what may be considered 50% softness.
Implementing
a generalized softness simply requires that the line slopes used above are
modified
accordingly (divide by 2 * softFactor).
*/

//-----------------------------------------------------------------------------------------

namespace {
template <typename PIX>
class PixelSelector {
public:
  typedef PIX pixel_type;

private:
  int m_thresh;

public:
  PixelSelector(int thresh) : m_thresh(thresh) {}

  bool areEqual(const PIX &a, const PIX &b) const {
    return std::max({abs((int)a.r - b.r), abs((int)a.g - b.g),
                     abs((int)a.b - b.b), abs((int)a.m - b.m)}) < m_thresh;
  }
};

//-----------------------------------------------------------------------------------------

template <>
class PixelSelector<TPixelCM32> {
public:
  typedef TPixelCM32 pixel_type;

private:
  int m_thresh;

public:
  PixelSelector(int thresh) : m_thresh(thresh) {}

  bool areEqual(const TPixelCM32 &a, const TPixelCM32 &b) const {
    return (a.getInk() == b.getInk()) &&
           (abs(a.getTone() - b.getTone()) < m_thresh);
  }
};

//-----------------------------------------------------------------------------------------

template <typename PIX>
inline void weightPix(PIX *out, const PIX *a, const PIX *b, double weightA,
                      double weightB) {
  out->r = a->r * weightA + b->r * weightB;
  out->g = a->g * weightA + b->g * weightB;
  out->b = a->b * weightA + b->b * weightB;
  out->m = a->m * weightA + b->m * weightB;
}

//-----------------------------------------------------------------------------------------

template <>
inline void weightPix<TPixelCM32>(TPixelCM32 *out, const TPixelCM32 *a,
                                  const TPixelCM32 *b, double weightA,
                                  double weightB) {
  *out =
      TPixelCM32(out->isPurePaint() ? b->getInk() : a->getInk(), a->getPaint(),
                 a->getTone() * weightA + b->getTone() * weightB);
}

//-----------------------------------------------------------------------------------------

// Returns 0 if pixels to connect are on the 00-11 diagonal, 1 on the 01-10 one.
template <typename PIX, typename SELECTOR>
inline bool checkNeighbourHood(int x, int y, PIX *pix, int lx, int ly, int dx,
                               int dy, const SELECTOR &sel) {
  int count1 = 0, count2 = 0;
  int dx2 = 2 * dx, dy2 = 2 * dy;
  if (y > 1) {
    // Lower edge
    count1 += (int)sel.areEqual(*(pix - dx), *(pix - dy2)) +
              (int)sel.areEqual(*(pix - dx), *(pix - dy2 - dx));
    count2 += (int)sel.areEqual(*pix, *(pix - dy2)) +
              (int)sel.areEqual(*pix, *(pix - dy2 - dx));
  }
  if (y < ly - 1) {
    // Upper edge
    count1 += (int)sel.areEqual(*(pix - dx), *(pix + dy)) +
              (int)sel.areEqual(*(pix - dx), *(pix + dy - dx));
    count2 += (int)sel.areEqual(*pix, *(pix + dy)) +
              (int)sel.areEqual(*pix, *(pix + dy - dx));
  }
  if (x > 1) {
    // Left edge
    count1 += (int)sel.areEqual(*(pix - dx), *(pix - dx2)) +
              (int)sel.areEqual(*(pix - dx), *(pix - dx2 - dy));
    count2 += (int)sel.areEqual(*pix, *(pix - dx2)) +
              (int)sel.areEqual(*pix, *(pix - dx2 - dy));
  }
  if (x < lx - 1) {
    // Left edge
    count1 += (int)sel.areEqual(*(pix - dx), *(pix + dx)) +
              (int)sel.areEqual(*(pix - dx), *(pix + dx - dy));
    count2 += (int)sel.areEqual(*pix, *(pix + dx)) +
              (int)sel.areEqual(*pix, *(pix + dx - dy));
  }

  // Connect by minority: if there are more pixels like those on the 00-11
  // diagonal, connect the other,
  // and viceversa.
  return count1 > count2;
}
}

//========================================================================================

template <typename PIX>
inline void filterLine(PIX *inLPix, PIX *inUPix, PIX *outLPix, PIX *outUPix,
                       int ll, int inDl, int outLDl, int outUDl, double hStart,
                       double slope, bool filterLower) {
  assert(hStart >= 0.0 && slope > 0.0);

  double h0   = hStart, h1, area;
  double base = hStart / slope;

  int i, end = std::min(tfloor(base), ll);
  if (filterLower) {
    // Filter lower line
    for (i = 0; i < end;
         ++i, h0 = h1, inLPix += inDl, inUPix += inDl, outLPix += outLDl) {
      h1   = h0 - slope;
      area = 0.5 * (h0 + h1);
      weightPix(outLPix, outLPix, inUPix, 1.0 - area, area);
    }

    if (i < ll) {
      double remnant = base - end;
      area           = 0.5 * remnant * h0;
      weightPix(outLPix, outLPix, inUPix, 1.0 - area, area);
    }
  } else {
    // Filter upper line
    for (i = 0; i < end;
         ++i, h0 = h1, inLPix += inDl, inUPix += inDl, outUPix += outUDl) {
      h1   = h0 - slope;
      area = 0.5 * (h0 + h1);
      weightPix(outUPix, outUPix, inLPix, 1.0 - area, area);
    }

    if (i < ll) {
      double remnant = base - end;
      area           = 0.5 * remnant * h0;
      weightPix(outUPix, outUPix, inLPix, 1.0 - area, area);
    }
  }
}

//---------------------------------------------------------------------------------------

template <typename PIX, typename SELECTOR>
inline bool checkLength(int lLine, int y, int ly, int dy, PIX *pixL1,
                        PIX *pixU1, PIX *pixL2, PIX *pixU2, bool uniteU,
                        bool do1Line, const SELECTOR &sel) {
  // 1-length edges must be processed (as primary edges) only if explicitly
  // required,
  // and only when its associated secondary edge is of the same length.

  return (lLine > 1) ||
         (do1Line && ((uniteU && (y > 1 &&
                                  !(sel.areEqual(*pixL1, *(pixL1 - dy)) &&
                                    sel.areEqual(*pixL2, *(pixL2 - dy))))) ||
                      (y < ly - 1 &&
                       !(sel.areEqual(*pixU1, *(pixU1 + dy)) &&
                         sel.areEqual(*pixU2, *(pixU2 + dy))))));
}

//---------------------------------------------------------------------------------------

template <typename PIX, typename SELECTOR>
void processLine(int r, int lx, int ly, PIX *inLRow, PIX *inURow, PIX *outLRow,
                 PIX *outURow, int inDx, int inDy, int outLDx, int outUDx,
                 bool do1Line, double hStart, double slope,
                 const SELECTOR &sel) {
  // Using a 'horizontal' notation here - but the same applies in vertical too
  ++r;

  // As long as we don't reach row end, process uninterrupted separation lines
  // between colors

  PIX *inLL = inLRow, *inLR, *inUL = inURow, *inUR;
  PIX *inLL_1, *inUL_1, *inLR_1, *inUR_1;
  PIX *inLEnd = inLRow + lx * inDx;
  int x, lLine;
  bool uniteLL, uniteUL, uniteLR, uniteUR;

  // Special case: a line at row start has different weights
  if (!sel.areEqual(*inLL, *inUL)) {
    // Look for line ends
    for (inLR = inLL + inDx, inUR = inUL + inDx;
         inLR != inLEnd && sel.areEqual(*inLL, *inLR) &&
         sel.areEqual(*inUL, *inUR);
         inLR += inDx, inUR += inDx)
      ;

    if (inLR != inLEnd) {
      // Found a line to process
      lLine = (inLR - inLL) / inDx;

      inLR_1 = inLR - inDx, inUR_1 = inUR - inDx;
      x = (inLR_1 - inLRow) / inDx;

      uniteUR = sel.areEqual(*inUR_1, *inLR);
      uniteLR = sel.areEqual(*inLR_1, *inUR);
      if (uniteUR || uniteLR) {
        if (uniteUR && uniteLR)
          // Ambiguous case. Check neighborhood to find out which one must be
          // actually united.
          uniteUR =
              !checkNeighbourHood(x + 1, r, inUR, lx, ly, inDx, inDy, sel);

        if (checkLength(lLine, r, ly, inDy, inLR_1, inUR_1, inLR, inUR, uniteUR,
                        do1Line, sel))
          filterLine(inLR_1, inUR_1, outLRow + x * outLDx, outURow + x * outUDx,
                     lLine, -inDx, -outLDx, -outUDx, hStart,
                     slope / (lLine << 1), uniteUR);
      }
    }

    // Update lefts
    inLL = inLR, inUL = inUR;
  }

  // Search for a line start
  for (; inLL != inLEnd && sel.areEqual(*inLL, *inUL);
       inLL += inDx, inUL += inDx)
    ;

  while (inLL != inLEnd) {
    // Look for line ends
    for (inLR = inLL + inDx, inUR = inUL + inDx;
         inLR != inLEnd && sel.areEqual(*inLL, *inLR) &&
         sel.areEqual(*inUL, *inUR);
         inLR += inDx, inUR += inDx)
      ;

    if (inLR == inLEnd) break;  // Dealt with later

    // Found a line to process
    lLine = (inLR - inLL) / inDx;

    // First, filter left to right
    inLL_1 = inLL - inDx, inUL_1 = inUL - inDx;
    x = (inLL - inLRow) / inDx;

    uniteUL = sel.areEqual(*inUL, *inLL_1);
    uniteLL = sel.areEqual(*inLL, *inUL_1);
    if (uniteUL || uniteLL) {
      if (uniteUL && uniteLL)
        uniteUL = checkNeighbourHood(x, r, inUL, lx, ly, inDx, inDy, sel);

      if (checkLength(lLine, r, ly, inDy, inLL_1, inUL_1, inLL, inUL, uniteUL,
                      do1Line, sel))
        filterLine(inLL, inUL, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, inDx, outLDx, outUDx, hStart, slope / lLine, uniteUL);
    }

    // Then, filter right to left
    inLR_1 = inLR - inDx, inUR_1 = inUR - inDx;
    x = (inLR_1 - inLRow) / inDx;

    uniteUR = sel.areEqual(*inUR_1, *inLR);
    uniteLR = sel.areEqual(*inLR_1, *inUR);
    if (uniteUR || uniteLR) {
      if (uniteUR && uniteLR)
        uniteUR = !checkNeighbourHood(x + 1, r, inUR, lx, ly, inDx, inDy, sel);

      if (checkLength(lLine, r, ly, inDy, inLR_1, inUR_1, inLR, inUR, uniteUR,
                      do1Line, sel))
        filterLine(inLR_1, inUR_1, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, -inDx, -outLDx, -outUDx, hStart, slope / lLine,
                   uniteUR);
    }

    // Update lefts - search for a new line start
    inLL = inLR, inUL = inUR;
    for (; inLL != inLEnd && sel.areEqual(*inLL, *inUL);
         inLL += inDx, inUL += inDx)
      ;
  }

  // Special case: filter the last line in the row
  if (inLL != inLEnd) {
    // Found a line to process
    lLine = (inLR - inLL) / inDx;

    inLL_1 = inLL - inDx, inUL_1 = inUL - inDx;
    x = (inLL - inLRow) / inDx;

    uniteUL = sel.areEqual(*inUL, *inLL_1);
    uniteLL = sel.areEqual(*inLL, *inUL_1);
    if (uniteUL || uniteLL) {
      if (uniteUL && uniteLL)
        uniteUL = checkNeighbourHood(x, r, inUL, lx, ly, inDx, inDy, sel);

      if (checkLength(lLine, r, ly, inDy, inLL_1, inUL_1, inLL, inUL, uniteUL,
                      do1Line, sel))
        filterLine(inLL, inUL, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, inDx, outLDx, outUDx, hStart, slope / (lLine << 1),
                   uniteUL);
    }
  }
}

//---------------------------------------------------------------------------------------

template <typename PIX>
void makeAntialias(const TRasterPT<PIX> &src, TRasterPT<PIX> &dst,
                   int threshold, int softness) {
  dst->copy(src);
  if (softness == 0) return;

  double slope  = (50.0 / softness);
  double hStart = 0.5;  // fixed for now

  src->lock();
  dst->lock();

  PixelSelector<PIX> sel(threshold);

  // First, filter by rows
  int x, y, lx = src->getLx(), ly = src->getLy(), lx_1 = lx - 1, ly_1 = ly - 1;
  for (y = 0; y < ly_1; ++y) {
    processLine(y, lx, ly, src->pixels(y), src->pixels(y + 1), dst->pixels(y),
                dst->pixels(y + 1), 1, src->getWrap(), 1, 1, true, hStart,
                slope, sel);
  }

  // Then, go by columns
  for (x = 0; x < lx_1; ++x) {
    processLine(x, ly, lx, src->pixels(0) + x, src->pixels(0) + x + 1,
                dst->pixels(0) + x, dst->pixels(0) + x + 1, src->getWrap(), 1,
                dst->getWrap(), dst->getWrap(), false, hStart, slope, sel);
  }

  dst->unlock();
  src->unlock();
}

//---------------------------------------------------------------------------------------

void TRop::antialias(const TRasterP &src, const TRasterP &dst, int threshold,
                     int softness) {
  assert(src->getSize() == dst->getSize());

  TRaster32P src32(src), dst32(dst);
  if (src32 && dst32) {
    makeAntialias<TPixel32>(src32, dst32, threshold, softness);
    return;
  }

  TRaster64P src64(src), dst64(dst);
  if (src64 && dst64) {
    makeAntialias<TPixel64>(src64, dst64, threshold << 8, softness);
    return;
  }

  TRasterCM32P srcCM(src), dstCM(dst);
  if (srcCM && dstCM) {
    makeAntialias<TPixelCM32>(srcCM, dstCM, threshold, softness);
    return;
  }

  assert(!"Source and destination rasters must be of the same type!");
}
