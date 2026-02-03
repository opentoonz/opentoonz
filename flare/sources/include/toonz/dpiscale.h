#pragma once

#ifndef DPISCALE_INCLUDED
#define DPISCALE_INCLUDED

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

//=============================================================================
// forward declarations
class ToonzScene;
class TXshSimpleLevel;
class TFrameId;
class TCamera;

//=============================================================================

/*!
  Return Dpi \b TAffine of \b level frame \b fid.
*/
DVAPI TAffine getDpiAffine(TXshSimpleLevel *level, const TFrameId &fid,
                           bool forceFullSubsampling = false);

/*!
  Return Dpi \b TAffine of \b camera.
*/
DVAPI TAffine getDpiAffine(TCamera *camera);

/*!
  Return current Dpi scale factor.
*/
DVAPI TPointD getCurrentDpiScale(TXshSimpleLevel *sl, const TFrameId &fid);

/*!
  Return current camera stand Dpi.
*/
DVAPI double getCurrentCameraStandDpi();

#endif
