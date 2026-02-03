

#ifndef TCG_IMAGE_OPS_H
#define TCG_IMAGE_OPS_H

// tcg includes
#include "pixel_ops.h"

namespace tcg {

//***********************************************************************
//    Image Traits  definition
//***********************************************************************

template <typename Img>
struct image_traits_types {
  typedef Img image_type;
  typedef typename Img::pixel_type pixel_type;
  typedef typename Img::pixel_ptr_type pixel_ptr_type;
  typedef typename Img::pixel_category pixel_category;
};

//-------------------------------------------------------------------

template <typename Img>
class image_traits : public image_traits_types<Img> {
  typedef image_traits_types<Img> tr;

public:
  static int width(const typename tr::image_type &img);
  static int height(const typename tr::image_type &img);
  static int wrap(const typename tr::image_type &img);

  static typename tr::pixel_ptr_type pixel(const typename tr::image_type &img,
                                           int x, int y);
  static typename tr::pixel_type outsideColor(
      const typename tr::image_type &img);
};

namespace image_ops {

//***********************************************************************
//    Image Functions
//***********************************************************************

template <typename ImgIn, typename ImgOut, typename Scalar>
void blurRows(const ImgIn &imgIn, ImgOut &imgOut, int radius, Scalar = 0);

template <typename ImgIn, typename ImgOut, typename SelectorFunc,
          typename Scalar>
void blurRows(const ImgIn &imgIn, ImgOut &imgOut, int radius, SelectorFunc func,
              Scalar = 0);

template <typename ImgIn, typename ImgOut, typename Scalar>
void blurCols(const ImgIn &imgIn, ImgOut &imgOut, int radius, Scalar = 0);

template <typename ImgIn, typename ImgOut, typename SelectorFunc,
          typename Scalar>
void blurCols(const ImgIn &imgIn, ImgOut &imgOut, int radius, SelectorFunc func,
              Scalar = 0);

template <typename Img, typename Scalar>
void blur(const Img &imgIn, Img &imgOut, int radius, Scalar = 0);

template <typename Img, typename SelectorFunc, typename Scalar>
void blur(const Img &imgIn, Img &imgOut, int radius, SelectorFunc func,
          Scalar = 0);
}
}  // namespace tcg::image_ops

#endif  // TCG_IMAGE_OPS_H

//===================================================================

#ifdef INCLUDE_HPP
#include "hpp/image_ops.hpp"
#endif  // INCLUDE_HPP
