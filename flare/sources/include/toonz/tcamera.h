#pragma once

#ifndef TCAMERA_INCLUDED
#define TCAMERA_INCLUDED

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TIStream;
class TOStream;

//=============================================================================
//! The TCamera class provides a camera and allows its management.
/*!A camera is specified by size, getSize() and resolution, getRes().
   It can be changed using the setSize(), setRes() functions.tcamera

   The class moreover gives methods to know camera Dpi getDpi(), camera aspect
   ratio
   getAspectRatio() and stage rect getStageRect().
*/
//=============================================================================

class DVAPI TCamera {
  TDimensionD m_size;
  TDimension m_res;
  bool m_xPrevalence;
  TRect m_interestRect;

public:
  /*!
Constructs TCamera with default value, size (12,9) and resolution (768,576).
  Constructs TCamera with default value, size (36, 20.25) and resolution
(1920,1080).
- 05/31/16
Constructs TCamera with default value, size (16, 9) and resolution
(1920,1080).
- 08/16/16
*/
  TCamera();

  /*!
Return camera size.
\sa setSize()
*/
  const TDimensionD &getSize() const { return m_size; }
  /*!
Set camera size to \b size.
\sa getSize()
*/
  void setSize(const TDimensionD &size, bool preserveDpi = false,
               bool preserveAR = false);

  /*!
Return camera aspect ratio.
*/
  double getAspectRatio() const;

  /*!
Return camera resolution.
\sa setRes()
*/
  const TDimension &getRes() const { return m_res; }
  /*!
Set camera resolution to \b res.
\sa getRes()
*/
  void setRes(const TDimension &res);

  /*!
Return camera Dpi.
*/
  TPointD getDpi() const;

  /*!
Return true if "resolution width per size height" is equal to
"resolution height per size width".
*/
  bool isPixelSquared() const;

  /*!
Returns the reference change matrix from stage reference to camera (resolution)
reference
*/
  TAffine getStageToCameraRef() const;

  /*!
Returns the reference change matrix from camera (resolution) reference to stage
reference
*/
  TAffine getCameraToStageRef() const;

  /*!
Return stage rect.
*/
  TRectD getStageRect() const;

  /*!
Sets the camera's interest rect. Typically used in preview modes.
*/
  void setInterestRect(const TRect &rect);

  /*!
Returns the camera's interest rect.
*/
  TRect getInterestRect() const { return m_interestRect; }

  /*!
Sets the interest rect from stage coordinates
*/
  void setInterestStageRect(const TRectD &rect);

  /*!
Returns the interest rect in stage coordinates.
*/
  TRectD getInterestStageRect() const;

  void setXPrevalence(bool value) { m_xPrevalence = value; }
  bool isXPrevalence() { return m_xPrevalence; }

  void saveData(TOStream &os) const;
  void loadData(TIStream &is);
};

#endif
