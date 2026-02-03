#pragma once

#ifndef VECTORIZERPARAMETERS_H
#define VECTORIZERPARAMETERS_H

// TnzCore includes
#include "tpersist.h"
#include "tvectorimage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=======================================================

//    Forward declarations

class TIStream;
class TOStream;

//=======================================================

//*****************************************************************************
//    VectorizerConfiguration  definition
//*****************************************************************************

/*!
  \brief    Provides a base container for vectorization options.

  \details  All vectorization options and information are passed from higher
            (like \p VectorizerPopup) to lower layers (\p VectorizerCore) of
            the vectorization process inside a \p VectorizerConfiguration
  variable.
            This typically includes vectorization modes, various sensitivity and
            accuracy parameters, and post-processing information. This class
            merely acts as a base parameters container (although no pure virtual
            method is present) - meaning that every vectorization method
  inherits
            this base class to include its specific parameters.

  \sa       Classes \p OutlineConfiguration, \p CenterlineConfiguration,
            \p VectorizerPopup, \p Vectorizer and \p VectorizerCore.
*/
class DVAPI VectorizerConfiguration {
public:
  bool m_outline;  //!< Vectorization mode between outline and centerline

  int m_threshold;  //!< Cut-out parameter to distinguish paper or painted
                    //! background
  //!  from recognizable strokes. A pixel whose tone (for colormaps)
  //!  or HSV value is under \p m_threshold is considered ink-colored.
  bool m_leaveUnpainted;  //!< Whether color recognition for areas should be
                          //! applied

  TAffine m_affine;  //!< Affine transform applied to the vectorization results
  double m_thickScale;  //!< Impose a thickness reduction by this ratio

  // Align stroke directions to be the same (clockwise, i.e. left to right as
  // viewed from inside of the shape).
  bool m_alignBoundaryStrokesDirection;

public:
  VectorizerConfiguration(bool outline)
      : m_outline(outline)
      , m_threshold(200)
      , m_leaveUnpainted(true)
      , m_affine()
      , m_thickScale(1.0)
      , m_alignBoundaryStrokesDirection(false) {}
};

//*****************************************************************************
//    CenterlineConfiguration  definition
//*****************************************************************************

/*!
  \brief    CenterlineConfiguration is the VectorizerConfiguration
            specialization for the centerline vectorization method.
*/

class DVAPI CenterlineConfiguration final : public VectorizerConfiguration {
public:
  /*!After threshold is done, raster zones of uniform ink or paint color whose
area is under this parameter
are discarded from vectorization process. This typically helps in reducing image
scannerization noise.*/
  int m_despeckling;

  /*!Specifies the maximum thickness allowed for stroke detection. Large ink
regions can
therefore be painted with dark colors, rather than covered with very thick
strokes.
Observe that placing 0 here has the effect of an outline vectorization.*/
  double m_maxThickness;

  /*!The m_accuracy dual (see VectorizerPopup). Specifies the user preference
between
accuracy of the identified strokes, and their simplicity. It generally does not
affect the vectorization speed.
For the conversion accuracy=>penalty, see
VectorizerParameters::getCenterlineConfiguration, defined in
vectorizerparameters.cpp
*/
  double m_penalty;

  //! Imposes a thickness reduction by this ratio, at the end of
  //! VectorizerCore::vectorize method.
  double m_thicknessRatio;

  /*!Includes the transparent frame of the image in the output. Region computing
can take
advantage of it to identify close-to-boundary regions.*/
  bool m_makeFrame;

  /*!Assume that the source input is a full-color non-antialiased image (e.g.
painted level made with Retas).
This kind of image must be pre-processed and transformed to toonz-image */
  bool m_naaSource;

public:
  /*!Constructs a VectorizerConfiguration with default values.
Default options consists of a full-thickness centerline vectorization, medium
accuracy settings,
with activated region computing and painting.*/
  CenterlineConfiguration()
      : VectorizerConfiguration(false)
      , m_despeckling(10)
      , m_maxThickness(100.0)
      , m_penalty(0.5)
      , m_thicknessRatio(100.0)
      , m_makeFrame(false)
      , m_naaSource(false) {}
};

//*****************************************************************************
//    NewOutlineConfiguration  definition
//*****************************************************************************

/*!
  \brief    NewOutlineConfiguration is the \p VectorizerConfiguration
            specialization for the (new) outline vectorization method.
*/

class DVAPI NewOutlineConfiguration final : public VectorizerConfiguration {
public:
  double m_adherenceTol;  //!< Adherence to contour corners
  double m_angleTol;      //!< Angle-based corners tolerance
  double m_relativeTol;   //!< Relative curvature radius-based corners tolerance
  double m_mergeTol;      //!< Quadratics merging factor
  int m_despeckling;  //!< Despeckling edge size (size x size gets despeckled)

  int m_maxColors;  //!< Maximum number of palette color from fullcolor
                    //! quantization
  TPixel32 m_transparentColor;  //!< Color to be recognized as transparent in
                                //! the fullcolor case

  int m_toneTol;  //!< Tone threshold to be used in the colormap case

public:
  NewOutlineConfiguration()
      : VectorizerConfiguration(true)
      , m_adherenceTol(0.5)
      , m_angleTol(0.25)
      , m_relativeTol(0.25)
      , m_mergeTol(1.0)
      , m_despeckling(4)
      , m_maxColors(50)
      , m_transparentColor(TPixel32::White)
      , m_toneTol(128) {}
};

//*****************************************************************************
//    OutlineConfiguration  definition
//*****************************************************************************

/*!
  \brief        OutlineConfiguration is the \p VectorizerConfiguration
                specialization for the outline vectorization method.

  \deprecated   Substituted by \p NewOutlineConfiguration, along
                with a different outline vectorization algorithm.
*/

class DVAPI OutlineConfiguration final : public VectorizerConfiguration {
public:
  int m_smoothness;  // Outline

  //! User can specify a color to recognize as ink, in outline vectorization
  //! mode.
  TPixel32 m_inkColor;  // Outline
  int m_strokeStyleId;  // Outline
  //! Ignore colors separation for ink areas, in outline mode.
  bool m_ignoreInkColors;       // Outline
  double m_interpolationError;  // Outline

  double m_resolution;  // Outline

public:
  OutlineConfiguration()
      : VectorizerConfiguration(true)
      , m_smoothness(3)
      , m_inkColor(TPixel::Black)
      , m_strokeStyleId(1)
      , m_ignoreInkColors(false)
      , m_interpolationError(0.4)
      , m_resolution(1) {}
};

//*****************************************************************************
//    VectorizerParameters  declaration
//*****************************************************************************

/*!
  \brief    Container class for scene-bound vectorizer options.
*/

class DVAPI VectorizerParameters final : public TPersist {
  PERSIST_DECLARATION(VectorizerParameters)

public:
  // Centerline options

  int m_cThreshold;
  int m_cAccuracy;
  int m_cDespeckling;
  int m_cMaxThickness;

  double m_cThicknessRatioFirst;
  double m_cThicknessRatioLast;

  bool m_cMakeFrame;
  bool m_cPaintFill;
  bool m_cAlignBoundaryStrokesDirection;
  bool m_cNaaSource;

  // Outline options

  int m_oDespeckling;
  int m_oAccuracy;
  int m_oAdherence;
  int m_oAngle;
  int m_oRelative;
  int m_oMaxColors;
  int m_oToneThreshold;

  TPixel32 m_oTransparentColor;

  bool m_oPaintFill;
  bool m_oAlignBoundaryStrokesDirection;

  // Generic data

  unsigned int
      m_visibilityBits;  //!< Variable storing the visibility of each parameter
                         //!  in the \p VectorizerPopup. Reset on version change
                         //!  is considered acceptable in case params layout
                         //!  does not match any more.
  bool m_isOutline;      //!< Specifies the currently active parameters
                         //!  selection (outline / centerline).
public:
  VectorizerParameters();

  CenterlineConfiguration getCenterlineConfiguration(double weight) const;
  NewOutlineConfiguration getOutlineConfiguration(double weight) const;

  /*! \brief    Builds a copy of the currently active configuration allocated
          using the <I>new operator</I>.

\param    weight  \a Position of the requested configuration in an abstract
                  frame range, normalized to the <TT>[0, 1]</TT> range.       */

  VectorizerConfiguration *getCurrentConfiguration(double weight) const {
    return m_isOutline ? (VectorizerConfiguration *)new NewOutlineConfiguration(
                             getOutlineConfiguration(weight))
                       : (VectorizerConfiguration *)new CenterlineConfiguration(
                             getCenterlineConfiguration(weight));
  }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

#endif  // VECTORIZERPARAMETERS_H
