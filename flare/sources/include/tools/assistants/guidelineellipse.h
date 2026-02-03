#pragma once

#ifndef GUIDELINEELLIPSE_INCLUDED
#define GUIDELINEELLIPSE_INCLUDED

// TnzTools includes
#include <tools/assistant.h>


#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//==============================================================

//*****************************************************************************************
//    TGuidelineEllipse definition
//*****************************************************************************************

class DVAPI TGuidelineEllipse : public TGuideline {
public:
  const TAffine matrix;
  const TAffine matrixInv;
  const double Rx;
  const double Ry;

  TGuidelineEllipse(
    bool enabled,
    double magnetism,
    const TPixelD &color,
    const TAffine &matrix );

  TGuidelineEllipse(
    bool enabled,
    double magnetism,
    const TPixelD &color,
    const TAffine &matrix,
    const TAffine &matrixInv );

  TGuidelineEllipse(
    bool enabled,
    double magnetism,
    const TPixelD &color,
    const TAffine &matrix,
    const TAffine &matrixInv,
    double Rx,
    double Ry );

  //! returns false when ellipse is invisible
  static bool truncateEllipse(
    TAngleRangeSet &ranges,
    const TAffine &ellipseMatrixInv,
    const TRectD &bounds );

  static int calcSegmentsCount(const TAffine &ellipseMatrix, double pixelSize);

  TTrackPoint transformPoint(const TTrackPoint &point) const override;
  TPointD nearestPoint(const TPointD &point) const override;
  void draw(bool active, bool enabled) const override;
};

#endif
