#pragma once

#ifndef TCG_IMAGE_OPS_HPP
#define TCG_IMAGE_OPS_HPP

// tcg includes
#include "../image_ops.h"
#include "../pixel.h"

// STD includes
#include <assert.h>
#include <memory>

namespace tcg {
namespace image_ops {

//**************************************************************************
//    Flat Blur  implementation
//**************************************************************************

template <typename PtrIn, typename PtrOut, typename PixSum>
void _flatFilterLine(PtrIn in, PtrOut out, int lx, int addIn, int addOut,
                     int radius, PixSum *sums) {
  typedef typename std::iterator_traits<PtrIn> trIn;
  typedef typename trIn::value_type pixIn_type;

  struct locals {
    inline static void fillSums(int lx, int radius, PtrIn lineIn,
                                PixSum *pixsum, int addIn, int addSum) {
      using namespace pixel_ops;

      PtrIn newPixIn = lineIn;

      PixSum sum;

      // Build up to radius pixels adding new pixels only
      int x, xNew, xEnd = tmin(radius, lx);

      for (xNew = 0; xNew != xEnd; ++xNew, newPixIn += addIn)
        sum = sum + *newPixIn;

      // Store sums AND add new pixels. Observe that the current pixel value is
      // decreased
      // BEFORE storage. This makes this function "strict" in calculating the
      // sums.
      for (x = 0; xNew != lx;
           ++x, ++xNew, lineIn += addIn, newPixIn += addIn, pixsum += addSum) {
        sum     = (sum + *newPixIn) - *lineIn;
        *pixsum = *pixsum + sum;
      }

      // Finally, without adding new pixels
      for (; x != lx; ++x, lineIn += addIn, pixsum += addSum) {
        sum     = sum - *lineIn;
        *pixsum = *pixsum + sum;
      }
    }
  };

  memset(sums, 0, lx * sizeof(PixSum));

  // Add *strict* halves of the convolution sums
  locals::fillSums(lx, radius, in, sums, addIn,
                   1);  // Right part of the convolution
  locals::fillSums(lx, radius, in + addIn * (lx - 1), sums + lx - 1, -addIn,
                   -1);  // Left   ...

  // Normalize sums
  int diameter = 2 * radius + 1;

  for (int x = 0; x != lx; ++x, in += addIn, out += addOut, ++sums) {
    using namespace pixel_ops;

    pixIn_type pix(*in);
    pixel_ops::assign(
        pix, (*sums + pix) / diameter);  // Note that *in is added, due *strict*
                                         // halving the sums above
    *out = pix;
  }
}

//----------------------------------------------------------------------

template <typename PtrIn, typename PtrOut, typename PixSum,
          typename SelectorFunc>
void _flatFilterLine(PtrIn in, PtrOut out, int lx, int addIn, int addOut,
                     SelectorFunc selector, int radius, PixSum *sums,
                     int *counts) {
  typedef typename std::iterator_traits<PtrIn> trIn;
  typedef typename trIn::value_type pixIn_type;

  struct locals {
    inline static void fillSums(int lx, int radius, PtrIn lineIn,
                                PixSum *pixsum, int *pixcount, int addIn,
                                int addSum, SelectorFunc selector) {
      using namespace pixel_ops;

      PtrIn newPixIn = lineIn;

      PixSum sum;
      int count = 0;

      // Build up to radius pixels adding new pixels only
      int x, xNew, xEnd = tmin(radius, lx);

      for (xNew = 0; xNew != xEnd; ++xNew, newPixIn += addIn) {
        const pixIn_type &npi = *newPixIn;

        if (selector(npi)) sum = sum + npi, ++count;
      }

      // Store sums AND add new pixels. Observe that the current pixel value is
      // decreased
      // BEFORE storage. This makes this function "strict" in calculating the
      // sums.
      for (x = 0; xNew != lx; ++x, ++xNew, lineIn += addIn, newPixIn += addIn,
          pixsum += addSum, pixcount += addSum) {
        const pixIn_type &npi = *newPixIn, &li = *lineIn;

        if (selector(npi)) sum = sum + npi, ++count;
        if (selector(li)) sum  = sum - li, --count;

        *pixsum = *pixsum + sum, *pixcount += count;
      }

      // Finally, without adding new pixels
      for (; x != lx;
           ++x, lineIn += addIn, pixsum += addSum, pixcount += addSum) {
        const pixIn_type &li = *lineIn;

        if (selector(li)) sum = sum - li, --count;

        *pixsum = *pixsum + sum, *pixcount += count;
      }
    }
  };

  memset(sums, 0, lx * sizeof(PixSum));
  memset(counts, 0, lx * sizeof(int));

  // Add *strict* halves of the convolution sums
  locals::fillSums(lx, radius, in, sums, counts, addIn, 1, selector);
  locals::fillSums(lx, radius, in + addIn * (lx - 1), sums + lx - 1,
                   counts + lx - 1, -addIn, -1, selector);

  // Normalize sums
  for (int x = 0; x != lx; ++x, in += addIn, out += addOut, ++sums, ++counts) {
    using namespace pixel_ops;

    assert(*counts >= 0);

    pixIn_type pix(*in);
    pixel_ops::assign(pix, (*sums + pix) / (*counts + 1));

    *out = pix;
  }
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename Scalar>
void blurRows(const ImgIn &imgIn, ImgOut &imgOut, int radius, Scalar) {
  typedef typename image_traits<ImgIn>::pixel_ptr_type PtrIn;
  typedef typename image_traits<ImgOut>::pixel_ptr_type PtrOut;
  typedef tcg::Pixel<Scalar, image_traits<ImgIn>::pixel_category> PixSum;

  // Validate parameters
  int inLx  = image_traits<ImgIn>::width(imgIn),
      inLy  = image_traits<ImgIn>::height(imgIn);
  int outLx = image_traits<ImgOut>::width(imgOut),
      outLy = image_traits<ImgOut>::height(imgOut);

  assert(inLx == outLx && inLy == outLy);

  int inWrap  = image_traits<ImgIn>::wrap(imgIn),
      outWrap = image_traits<ImgOut>::wrap(imgOut);

  // Allocate an intermediate line of pixels to store sum values
  std::unique_ptr<PixSum[]> sums(new PixSum[inLx]);

  // Filter rows
  for (int y = 0; y != inLy; ++y) {
    PtrIn lineIn   = image_traits<ImgIn>::pixel(imgIn, 0, y);
    PtrOut lineOut = image_traits<ImgOut>::pixel(imgOut, 0, y);

    _flatFilterLine(lineIn, lineOut, inLx, 1, 1, radius, sums.get());
  }
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename SelectorFunc,
          typename Scalar>
void blurRows(const ImgIn &imgIn, ImgOut &imgOut, int radius,
              SelectorFunc selector, Scalar) {
  typedef typename image_traits<ImgIn>::pixel_ptr_type PtrIn;
  typedef typename image_traits<ImgOut>::pixel_ptr_type PtrOut;
  typedef tcg::Pixel<Scalar, image_traits<ImgIn>::pixel_category> PixSum;

  // Validate parameters
  int inLx  = image_traits<ImgIn>::width(imgIn),
      inLy  = image_traits<ImgIn>::height(imgIn);
  int outLx = image_traits<ImgOut>::width(imgOut),
      outLy = image_traits<ImgOut>::height(imgOut);

  assert(inLx == outLx && inLy == outLy);

  int inWrap  = image_traits<ImgIn>::wrap(imgIn),
      outWrap = image_traits<ImgOut>::wrap(imgOut);

  // Allocate an intermediate line of pixels to store sum values
  std::unique_ptr<PixSum[]> sums(new PixSum[inLx]);
  std::unique_ptr<int[]> counts(new int[inLx]);

  // Filter rows
  for (int y = 0; y != inLy; ++y) {
    PtrIn lineIn   = image_traits<ImgIn>::pixel(imgIn, 0, y);
    PtrOut lineOut = image_traits<ImgOut>::pixel(imgOut, 0, y);

    _flatFilterLine(lineIn, lineOut, inLx, 1, 1, selector, radius, sums.get(),
                    counts.get());
  }
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename Scalar>
void blurCols(const ImgIn &imgIn, ImgOut &imgOut, int radius, Scalar) {
  typedef typename image_traits<ImgIn>::pixel_ptr_type PtrIn;
  typedef typename image_traits<ImgOut>::pixel_ptr_type PtrOut;
  typedef tcg::Pixel<Scalar, typename image_traits<ImgIn>::pixel_category>
      PixSum;

  // Validate parameters
  int inLx  = image_traits<ImgIn>::width(imgIn),
      inLy  = image_traits<ImgIn>::height(imgIn);
  int outLx = image_traits<ImgOut>::width(imgOut),
      outLy = image_traits<ImgOut>::height(imgOut);

  assert(inLx == outLx && inLy == outLy);

  int inWrap  = image_traits<ImgIn>::wrap(imgIn),
      outWrap = image_traits<ImgOut>::wrap(imgOut);

  // Allocate an intermediate line of pixels to store sum values
  std::unique_ptr<PixSum[]> sums(new PixSum[inLy]);

  // Filter columns
  for (int x = 0; x != inLx; ++x) {
    PtrIn lineIn   = image_traits<ImgIn>::pixel(imgIn, x, 0);
    PtrOut lineOut = image_traits<ImgOut>::pixel(imgOut, x, 0);

    _flatFilterLine(lineIn, lineOut, inLy, inWrap, outWrap, radius, sums.get());
  }
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename SelectorFunc,
          typename Scalar>
void blurCols(const ImgIn &imgIn, ImgOut &imgOut, int radius,
              SelectorFunc selector, Scalar) {
  typedef typename image_traits<ImgIn>::pixel_ptr_type PtrIn;
  typedef typename image_traits<ImgOut>::pixel_ptr_type PtrOut;
  typedef tcg::Pixel<Scalar, typename image_traits<ImgIn>::pixel_category>
      PixSum;

  // Validate parameters
  int inLx  = image_traits<ImgIn>::width(imgIn),
      inLy  = image_traits<ImgIn>::height(imgIn);
  int outLx = image_traits<ImgOut>::width(imgOut),
      outLy = image_traits<ImgOut>::height(imgOut);

  assert(inLx == outLx && inLy == outLy);

  int inWrap  = image_traits<ImgIn>::wrap(imgIn),
      outWrap = image_traits<ImgOut>::wrap(imgOut);

  // Allocate an intermediate line of pixels to store sum values
  std::unique_ptr<PixSum[]> sums(new PixSum[inLy]);
  std::unique_ptr<int[]> counts(new int[inLy]);

  // Filter columns
  for (int x = 0; x != inLx; ++x) {
    PtrIn lineIn   = image_traits<ImgIn>::pixel(imgIn, x, 0);
    PtrOut lineOut = image_traits<ImgOut>::pixel(imgOut, x, 0);

    _flatFilterLine(lineIn, lineOut, inLy, inWrap, outWrap, selector, radius,
                    sums.get(), counts.get());
  }
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename Scalar>
void blur(const ImgIn &imgIn, ImgOut &imgOut, int radius, Scalar) {
  blurRows(imgIn, imgOut, radius, Scalar);
  blurCols(imgOut, imgOut, radius, Scalar);
}

//----------------------------------------------------------------------

template <typename ImgIn, typename ImgOut, typename SelectorFunc,
          typename Scalar>
void blur(const ImgIn &imgIn, ImgOut &imgOut, int radius, SelectorFunc selector,
          Scalar) {
  blurRows(imgIn, imgOut, radius, selector, Scalar);
  blurCols(imgOut, imgOut, radius, selector, Scalar);
}
}
}  // namespace tcg::image_ops

#endif  // TCG_IMAGE_OPS_HPP
