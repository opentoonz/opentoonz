#pragma once

#ifndef MESHBUILDER_H
#define MESHBUILDER_H

// TnzCore includes
#include "trasterimage.h"
#include "tmeshimage.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//********************************************************************************************
//    Mesh Image Builder  functions
//********************************************************************************************

struct MeshBuilderOptions {
  int m_margin;               //!< Mesh margin to the original shape (in pixels)
  double m_targetEdgeLength;  //!< The target mesh edge length
  int m_targetMaxVerticesCount;  //!< The desired maximum vertices count
                                 //!< (the returned mesh could still be larger)
  TPixel64 m_transparentColor;   //!< Color to be used as transparent for
                                 //! boundaries recognition
  //!< (for fullcolor images only).
  //!< \note Transparent colors will be recognized as transparent
  //!< anyway
};

//=======================================================================================

//! Builds a mesh image from the specified raster.
/*!
  This function extracts a TMeshImage type from the passed raster, using
  transparent
  pixels to identify the mesh boundary.
  \n\n
  The returned mesh image has coordinates in the original raster reference (ie
  origin at
  the lower-left image corner, pixel metric)

  \warning The resulting mesh image has <B> no dpi <\B>. Users \b must set it
  manually after
           this function has been invoked.
*/
DVAPI TMeshImageP buildMesh(const TRasterP &ras,
                            const MeshBuilderOptions &options);

#endif  // MESHBUILDER_H
