#include <memory>

#include "tropcm.h"

// TnzCore includes
#include "traster.h"

// STD includes
#include <limits>

//#define UNIT_TEST                                             // Enables unit
// testing at program startup

//************************************************************************
//    Rationale
//************************************************************************

/*!
  \file                   tdistancetransform.cpp
  \brief    This file implements an O(rows * cols) 2-dimensional distance
            transform algorithm with customizable action on squared pixel
            distance from the closest pixel.
*/

//************************************************************************
//    Local namespace  stuff
//************************************************************************

namespace {

/*!
  \brief    Given 2 parabolas with (minimal) height at centers \p a and \p b
            and centers separated by distance \p d, returns the min between
            \p d and the value \p x satisfying <TT>a + x^2 == b + (x -
  d)^2</TT>.
*/

unsigned int takeoverDist(unsigned int a, unsigned int b, unsigned int d) {
  // The actual formula is: x = (h^2 + b - a) / 2h. It simplifies as follows
  // using integers only.

  // NOTE: It can be proven that with integer division, x/ab == (x/a)/b.
  return (b < a) ? d : std::max((d + (b - a) / d + 1) / 2,
                                d);  // Note the +1 to get the ceil
}

//--------------------------------------------------------------

template <typename Pix, typename IsInsideFunc>
void initializeDT(const TRasterPT<Pix> &ras,
                  const TRasterPT<unsigned int> &dtRas, IsInsideFunc isInside) {
  assert(ras->getLx() == dtRas->getLx() && ras->getLy() == dtRas->getLy());

  static const unsigned int uiMax =  // Due to the above takeoverDist, for
      (std::numeric_limits<unsigned int>::max)() - 2;  // d == 1

  int lx = ras->getLx(), ly = ras->getLy();
  for (int y = 0; y != ly; ++y) {
    Pix *pix = ras->pixels(y), *rowEnd = pix + lx;
    unsigned int *dt = dtRas->pixels(y);

    for (; pix != rowEnd; ++pix, ++dt) {
      assert(*dt == 0u);

      if (!isInside(*pix)) *dt = uiMax;
    }
  }
}

//--------------------------------------------------------------

template <typename Pix, typename OutFunc>
void expand(int lineLength, int linesCount, Pix *buf, int incrPix, int incrLine,
            unsigned int *dtBuf, int dtIncrPix, int dtIncrLine,
            OutFunc outFunc) {
  struct locals {
    static void copyLine(unsigned int *dst, unsigned int *src,
                         unsigned int *srcEnd, int srcStride) {
      for (; src != srcEnd; src += srcStride, ++dst) *dst = *src;
    }

    static void buildRange(unsigned int *dtRef, unsigned int *dtLineEnd,
                           unsigned int *&dtEnd, unsigned int *&dtNewRef) {
      unsigned int d    = 1,
                   dNew = 0,  // dNew at 0 to provide a consistent dtNewRef
          dMax = (std::numeric_limits<unsigned int>::max)();  // at the end -
                                                              // should not
                                                              // matter though
      unsigned int *dt = dtRef + 1;

      for (; d <= dMax && dt != dtLineEnd;
           ++d, ++dt)  // Pick larger intervals if possible
      {
        unsigned int newDMax = ::takeoverDist(*dtRef, *dt, d);  //
        if (newDMax <= dMax) {
          dNew = d;
          dMax = newDMax;
        }
      }

      dtEnd =
          dtRef + std::min(d, dMax);  // Could end the line before (dMax < d)
      dtNewRef = dtRef + dNew;
    }
  };  // locals

  // Allocate a buffer equivalent to a dt line. It will store the original
  // dt values. Final dt values will be written directly on the dt raster.
  // This is necessary since read and write intervals overlap.
  std::unique_ptr<unsigned[]> dtOriginalLine(new unsigned[lineLength]);

  unsigned int *odtLineStart = dtOriginalLine.get(),
               *odtLineEnd   = odtLineStart + lineLength;

  // Process each line
  for (int l = 0; l != linesCount; ++l) {
    unsigned int *dtLineStart =
                     dtBuf +
                     dtIncrLine *
                         l,  // Using dtBuf to track colors from now on,
        *dtLineEnd =
            dtLineStart +
            dtIncrPix * lineLength,  // it already embeds colorFunc's output due
            *dt         = dtLineStart,  // to the way it was initialized.
                *odtRef = odtLineStart;

    Pix *lineStart = buf + incrLine * l, *pix = lineStart;

    // Make a copy of the original dt values
    locals::copyLine(dtOriginalLine.get(), dtLineStart, dtLineEnd, dtIncrPix);

    // Expand a colored pixel along the line
    while (dt != dtLineEnd) {
      // The line is subdivided in consecutive ranges associated to the same
      // half-parabola - process one

      // Build a half-parabola range
      unsigned int *dtEnd, *odtNewRef;
      locals::buildRange(odtRef, odtLineEnd, dtEnd, odtNewRef);

      assert(odtLineStart <= odtNewRef && odtNewRef <= odtLineEnd);
      assert(odtLineStart <= dtEnd && dtEnd <= odtLineEnd);

      dtEnd =
          dtLineStart +
          dtIncrPix *
              (dtEnd - odtLineStart);  // Convert dtEnd to the dt raster buffer

      // Process the range
      Pix *ref = lineStart + incrPix * (odtRef - odtLineStart);

      unsigned int d = (pix - ref) / incrPix;
      for (; dt != dtEnd; ++d, dt += dtIncrPix, pix += incrPix)
        outFunc(*pix, *ref, *dt = *odtRef + sq(d));

      odtRef = odtNewRef;
    }
  }
}

//--------------------------------------------------------------

/*!
  \brief    Performs an O(rows * cols) distance transform on the specified
            raster image.

  \details  The algorithm relies on the separability of the 2D DT into 2
            passes (by rows and columns) of 1-dimensional DTs.

            The 1D DT sums a pre-existing (from the previous DT step if any)
            DT result with the one currently calculated.

  \warning  Templace parameter OutFunc is supposed to satisfy \a transitivity
            upon comparison of its output - so, if \p b is the output of \p a,
            and \p c is the output of \p b, then \p c is the same as the output
            of \p a.

  \todo     Accept a different output raster - but preserve the case where
            (srcRas == dstRas).
*/

template <typename Pix, typename IsInsideFunc, typename OutFunc>
void distanceTransform(const TRasterPT<Pix> &ras, IsInsideFunc isInside,
                       OutFunc outFunc) {
  int lx = ras->getLx(), ly = ras->getLy();

  // Allocate a suitable temporary raster holding the (squared) distance
  // transform
  // built from the specified color function
  TRasterPT<unsigned int> dtRas(
      lx, ly);  // Summed squared distances will be limited to
                // 2 billions. This is generally suitable.
  ::initializeDT(ras, dtRas,
                 isInside);  // The raster is binarized directly into the
                             // auxiliary dtRas. Pixels in the set to expand
  // will have value 0, the others a suitable high value.
  expand(lx, ly, ras->pixels(0), 1, ras->getWrap(), dtRas->pixels(0), 1,
         dtRas->getWrap(), outFunc);
  expand(lx, ly, ras->pixels(0) + lx - 1, -1, ras->getWrap(),
         dtRas->pixels(0) + lx - 1, -1, dtRas->getWrap(), outFunc);

  expand(ly, lx, ras->pixels(0), ras->getWrap(), 1, dtRas->pixels(0),
         dtRas->getWrap(), 1, outFunc);
  expand(ly, lx, ras->pixels(ly - 1), -ras->getWrap(), 1, dtRas->pixels(ly - 1),
         -dtRas->getWrap(), 1, outFunc);
}
}

//************************************************************************
//    Local Functors
//************************************************************************

/*
  Using functors here just to be absolutely sure that calls are not
  callbacks.
*/

namespace {

struct SomePaint {
  inline bool operator()(const TPixelCM32 &pix) const {
    return (pix.getTone() != 0) || (pix.getPaint() != 0);
  }
};

struct CopyPaint {
  inline void operator()(TPixelCM32 &out, const TPixelCM32 &in,
                         unsigned int) const {
    out.setPaint(in.getPaint());
  }
};
}

//************************************************************************
//    API functions
//************************************************************************

void TRop::expandPaint(const TRasterCM32P &rasCM) {
  distanceTransform(rasCM, SomePaint(), CopyPaint());
}

//************************************************************************
//    Unit testing
//************************************************************************

#if defined UNIT_TEST && !defined NDEBUG

namespace {

void assertEqualBufs(const TRasterT<unsigned int> &a,
                     const TRasterT<unsigned int> &b) {
  for (int y = 0; y != a.getLy(); ++y) {
    for (int x = 0; x != a.getLx(); ++x)
      assert(a.pixels(y)[x] == b.pixels(y)[x]);
  }
}

struct Selector {
  inline bool operator()(unsigned int val) const { return val; }
};

struct OutputDT {
  inline void operator()(unsigned int &out, const unsigned int &in,
                         unsigned int d2) const {
    out = d2;
  }
};

struct DTTest {
  DTTest() {
    unsigned int imgBuf[] = {
        0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1,
        1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    unsigned int dtBuf[] = {
        4, 1, 0, 1, 4, 5, 2, 1, 0, 1, 1, 2, 1, 0, 0,
        0, 0, 1, 1, 0, 1, 1, 1, 2, 2, 1, 2, 4, 4, 5,
    };

    TRasterPT<unsigned int> imgRas(6, 5, 6, imgBuf, false),
        dtRas(6, 5, 6, dtBuf, false);

    distanceTransform(imgRas, Selector(), OutputDT());
    assertEqualBufs(*imgRas, *dtRas);
  }
} dtTest;

}  // namespace

#endif  // UNIT_TEST && !NDEBUG
