#pragma once

#ifndef MESHUTILS_H
#define MESHUTILS_H

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=======================================================================

//    Forward Declarations

class TMeshImageP;

struct DrawableTextureData;
struct PlasticDeformerDataGroup;

//=======================================================================

//********************************************************************************************
//    Mesh Image Utility  functions
//********************************************************************************************

/*!
  \brief    Transforms a mesh image by the specified affine transform.
*/

DVAPI void transform(
    const TMeshImageP &image,  //!< Mesh image to be transformed.
    const TAffine &aff  //!< Affine transform to be applied on the input image.
    );

//---------------------------------------------------------------------------

/*!
  \brief    Draws the edges of the input meshImage on current OpenGL context.

  \remark   This function accepts input data associated to a deformation of
            the mesh image to be drawn. See PlasticDeformerStorage for details.
*/

DVAPI void tglDrawEdges(
    const TMeshImage &image,  //!< Input mesh image whose edges will be drawn.
    const PlasticDeformerDataGroup *deformerDatas =
        0  //!< Optional data about a deformation of the input image.
    );

//---------------------------------------------------------------------------

/*!
  \brief    Draws the faces of the input meshImage on current OpenGL context.

  \remark   This function accepts input data associated to a deformation of
            the mesh image to be drawn. See PlasticDeformerStorage for details.
*/

DVAPI void tglDrawFaces(
    const TMeshImage &image,  //!< Input mesh image whose faces will be drawn.
    const PlasticDeformerDataGroup *deformerDatas =
        0  //!< Optional data about a deformation of the input image.
    );

//---------------------------------------------------------------------------

/*!
  \brief    Draws the <I>stacking order</I> information of the input mesh
  image's
            faces, on current OpenGL context.
*/

DVAPI void tglDrawSO(
    const TMeshImage &image,  //!< Input mesh image whose SO will be drawn.
    double minColor[4],  //!< RGBM color quadruple (in [0, 1]) corresponding to
                         //! the lowest SO value.
    double maxColor[4],  //!< RGBM color quadruple corresponding to the highest
                         //! SO value.
    const PlasticDeformerDataGroup *deformerDatas =
        0,  //!< Deformation data structure containing SO data.
    bool deformedDomain =
        false  //!< Whether the image data must be drawn \a deformed
               //!  by the specified deformerDatas, or not.
    );

//---------------------------------------------------------------------------

/*!
  \brief    Draws the \a rigidity information of the input mesh image's
            faces, on current OpenGL context.
*/

DVAPI void tglDrawRigidity(
    const TMeshImage
        &image,          //!< Input mesh image whose rigidity will be drawn.
    double minColor[4],  //!< RGBM color quadruple (in [0, 1]) corresponding to
                         //! the lowest rigidity value.
    double maxColor[4],  //!< RGBM color quadruple corresponding to the highest
                         //! rigidity value.
    const PlasticDeformerDataGroup *deformerDatas =
        0,  //!< Data structure of an optional deformation of the input image.
    bool deformedDomain =
        false  //!< Whether the image data must be drawn \a deformed
               //!  by the specified deformerDatas, or not.
    );

//---------------------------------------------------------------------------

/*!
  \brief    Draws a texturized mesh image on current OpenGL context.

  \remark   The input textures are assumed to be \a nonpremultiplied,
            while the drawn image will be \a premultiplied.
*/

DVAPI void tglDraw(
    const TMeshImage &image,  //!< Mesh image to be drawn.
    const DrawableTextureData
        &texData,  //!< Textures data to use for texturing.
    const TAffine
        &meshToTexAffine,  //!< Transform from mesh to texture coordinates.
    const PlasticDeformerDataGroup
        &deformerDatas  //!< Data structure of a deformation of the input image.
    );

#endif  // MESHUTILS_H
