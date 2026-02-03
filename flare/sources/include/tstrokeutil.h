#pragma once

#ifndef TSTROKE_UTIL_H
#define TSTROKE_UTIL_H

// TnzCore includes
#include "traster.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//    Forward declarations

class TStroke;
class TStrokeDeformation;
class TVectorRenderData;

//=================================================

//*******************************************************************************
//    Utility stroke  functions
//*******************************************************************************

/*!
  \brief    Inserts control points in a stroke according to the input deformer.

  \details  This function inserts redundant control points to use in the
            specified deformation.

              \par  Unverified documentation
            The "increaser" studies the gradient (getDelta) of the input
  deformer
            object, and insert control points if there is increment.\n
            In "extremes" there are the first point of not zero potential and
            the last.\n\n
            Special case (plane at same potential)
*/
DVAPI bool increaseControlPoints(
    TStroke &stroke,                     //!< Stroke to be deformed.
    const TStrokeDeformation &deformer,  //!< Deformer to be applied.
    double pixelSize = 1.0               //!< Resolution modifier.
    );

//-----------------------------------------------------------------------------

DVAPI void modifyControlPoints(TStroke &stroke,
                               const TStrokeDeformation &deformer);

DVAPI void modifyControlPoints(TStroke &stroke,
                               const TStrokeDeformation &deformer,
                               std::vector<double> &controlPointLen);

DVAPI void modifyThickness(TStroke &stroke, const TStrokeDeformation &deformer,
                           std::vector<double> &controlPointLen,
                           bool exponentially = false);

//-----------------------------------------------------------------------------

/*!
  \brief    Transforms a stroke's thickness by a given polynomial function.
*/
DVAPI void transform_thickness(
    TStroke &stroke,      //!< Stroke whose thickness will be transformed.
    const double poly[],  //!< Polynomial coefficients, by increasing degree.
    int deg               //!< Polynomial degree.
    );

//-----------------------------------------------------------------------------

/*!
  \brief    This function translates an input polyline to the control points
            of a sequence of quadratics.

  \details  The conversion is actually a 2 step process. First, segment
  midpoints
            are added to the sequence to be used as quadratic endpoints. Each
            "midpoint-vertex-midpoint" triplet is converted independently to
            quadratics, thus ensuring that tangent vectors at midpoints are
            preserved.

            The triplets conversion step allows the following control
  parameters:

              <UL>
              <LI> \a adherenceTol is the maximum distance allowed for
  quadratics to approximate a
                   polyline corner (\p 0 meaning full corner).</LI>
              <LI> Polyline corners whose edges inner product is strictly above
  \a angleTol are
                   transformed into full corners. The range for \a angleTol
  should be <TT>[-1, 1]</TT>.</LI>
              <LI> A quadratic approximating a polyline vertex triplet yields a
  minimum curvature
                   radius in proximity of the triplet's corner. If the ratios
  between such
                   radius and the edges extending from the corner are both below
  \a relativeTol,
                   the triplet is transformed into a full corner. The edges are
  calculated along
                   fat lines extending from the corner with \a relativeDist
  half-thickness.</LI>
              </UL>

            Once this first approximating sequence has been produced, an optimal
  quadratics
            merging step is employed to simplify the overall sequence.

            The \a mergeTol parameter specifies the allowed merging error that
  can be employed
            in this step (\p 0 meaning no simplification).

  \note     The output cps are thick points with 0 thickness. In other words,
  thickness
            is not considered, but supplied in output as TStrokes require them
  for construction.

  \sa       The tcg::polyline_ops::toQuadratics template function.
*/
DVAPI void polylineToQuadratics(
    const std::vector<TPointD>
        &polyline,  //!< Polyline to be translated to quadratics.
    std::vector<TThickPoint>
        &cps,  //!< Control points of the output quadratics sequence.
    double adherenceTol =
        1.0,  //!< Maximum distance from quadratic to polyline corner.
    double angleTol    = 0.0,  //!< Inner product threshold for polyline angles.
    double relativeTol = 0.25,
    double relativeDistTol = 0.25, double mergeTol = 1.0);

//-----------------------------------------------------------------------------

/*!
  \brief    Draws a stroke on a raster, under the specified rendering
  parameters.
*/
DVAPI void renderStroke(
    const TVectorRenderData
        &rd,  //!< Render parameters (clipping rect, affine transform, etc).
    const TRaster32P &output,  //!< Destination raster.
    TStroke *stroke            //!< Stroke to render.
    );

//-----------------------------------------------------------------------------

namespace Toonz {

/*!
  \brief    Merge an array of stroke in a single stroke.

  \remark   If two consecutive strokes don't touch at the endpoints,
            an additional joining segment is added between them.
*/
DVAPI TStroke *merge(const std::vector<TStroke *> &strokes);

}  // namespace Toonz

#endif  // TSTROKE_UTIL_H
