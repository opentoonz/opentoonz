// #pragma once // could not use by INCLUDE_HPP

#ifndef BORDERS_EXTRACTOR_H
#define BORDERS_EXTRACTOR_H

#include "traster.h"
#include "runsmap.h"

#include "raster_edge_iterator.h"

namespace TRop {
namespace borders {

//*******************************************************************
//    Pixel Selector model
//*******************************************************************

/*!
  This class is just a stub for pixel selector classes.

  Pixel Selectors are objects used in raster borders recognition
  to interpret a raster's pixels and direct edge iterators.
\n\n
  The main purpose of this class is that of distinguishing pixels
  from their <I> values <\I>. Values are the actual entities recognized
  in the borders extraction process - adjacent pixels that are cast to
  the same value will be included in the same border.
\n\n
  For example, if we want to extract the borders of a fullcolor image
  that separate bright areas from dark ones, we can just have a pixel selector
  cast each pixel to the integer value 0 if the pixel's value is below the
  brigthness threshold, and 1 if above.
\n\n
  Also, pixel selectors tell whether pixels are to be intended as
  completely transparent (virtual pixels outside the raster boundaries are).
*/
template <typename Pix, typename Val>
class pixel_selector {
public:
  typedef Pix pixel_type;  //!< The pixel type naming the selector
  typedef Val value_type;  //!< A value type representing pixel contents.
                           //!< Typically the pixel itself.

public:
  value_type value(const pixel_type &pix) const;
  bool equal(const pixel_type &a, const pixel_type &b) const {
    return value(a) == value(b);
  }

  value_type transparent() const;
  bool transparent(const pixel_type &pix) const {
    return value(pix) == transparent();
  }

  //! Returns whether a border point must be read or not (corners are always
  //! read).
  bool skip(const value_type &prevLeftValue,
            const value_type &leftValue) const {
    return true;
  }
};

//----------------------------------------------------------------------------------

enum RunType {
  _BORDER_LEFT        = 0x20,
  _BORDER_RIGHT       = 0x10,
  _HIERARCHY_INCREASE = 0x8,
  _HIERARCHY_DECREASE = 0x4
};

//----------------------------------------------------------------------------------

/*!
  Reads the borders of the input raster, according to the specified selector.
  Outputs the borders by supplying a container reader with the raster edge
  iterators
  corresponding to border vertices. The runsmap used in the process can be
  returned in output.
*/
template <typename Pixel, typename PixelSelector, typename ContainerReader>
void readBorders(const TRasterPT<Pixel> &raster, const PixelSelector &selector,
                 ContainerReader &reader, RunsMapP *rasterRunsMap = 0);

//=====================================================================================

template <typename PixelSelector, typename Mesh, typename ContainersReader>
void readMeshes(const TRasterPT<typename PixelSelector::pixel_type> &raster,
                const PixelSelector &selector,
                ContainersReader &meshesDataReader,
                RunsMapP *rasterRunsMap = 0);
}
}  // namespace TRop::borders

#endif  // BORDERS_EXTRACTOR_H

//=====================================================================================

#ifdef INCLUDE_HPP
#include "borders_extractor.hpp"
#endif  // INCLUDE_HPP
