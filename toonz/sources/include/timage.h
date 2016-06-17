#pragma once

#ifndef TIMAGE_INCLUDED
#define TIMAGE_INCLUDED

#include "tsmartpointer.h"
#include "tgeometry.h"
#include "traster.h"
#undef DVAPI
#undef DVVAR
#ifdef TIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TPalette;
//-------------------------------------------------------------------

//! This is an abstract class representing an image.
/*!

        */
class DVAPI TImage : public TSmartObject {
  DECLARE_CLASS_CODE
  TPalette *m_palette;

public:
  /*!
          This class represents an interface to a generic image object.
          An image can be:
  \li raster, i.e. a data structure representing a generally rectangular grid of
pixels,
          or points of color or a vector image,
\li vector, i.e. an image represented by geometrical primitives such as points,
lines, curves, and shapes or polygon(s),
      which are all based upon mathematical equations
  */
  TImage();
  virtual ~TImage();

  /*!
  This enum
*/
  enum Type {
    NONE         = 0,  //!< Not a recognized image (should never happen).
    RASTER       = 1,  //!< A fullcolor raster image.
    VECTOR       = 2,  //!< A vector image.
    TOONZ_RASTER = 3,  //!< A colormap raster image.
    MESH         = 4   //!< A textured mesh image.
  };

  /*!
          This method must return the type of the image. \sa Type.
*/
  virtual Type getType() const = 0;
  /*!
          This method must return the bounding box of the image.
          The bounding box is the minimum rectangle containing the image.
*/
  virtual TRectD getBBox() const = 0;
  /*!
          This method must return a pointer to a newly created image that is a
     clone of \e this.
  */
  virtual TImage *cloneImage() const = 0;
  /*!
          Returns a pointer to the palette associated to this image. A palette
     is a
          limited set of colors, identified by indexes and composed by a
     combination of
          primitive colors, i.e. RGB color.
  */

  virtual TRasterP raster() const { return TRasterP(); }

  TPalette *getPalette() const { return m_palette; }
  /*!
          Sets the \e palette for this image.
  */
  virtual void setPalette(TPalette *palette);

private:
  // not implemented
  TImage(const TImage &);
  TImage &operator=(const TImage &);
};

//-------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TImage>;
#endif

typedef TSmartPointerT<TImage> TImageP;

#endif
