#pragma once

#ifndef SCENEFX_INCLUDED
#define SCENEFX_INCLUDED

#include "tfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TXsheet;
class ToonzScene;

/*
  NOTE:  The following functions are DEPRECATED, but still in use throughout
  Toonz at the moment.
         Please use the unified function below in newly written code.
*/

DVAPI TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row,
                        int whichLevels, int shrink, bool isPreview);
DVAPI TFxP buildSceneFx(ToonzScene *scene, double row, int shrink,
                        bool isPreview);
DVAPI TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row, int shrink,
                        bool isPreview);
DVAPI TFxP buildSceneFx(ToonzScene *scene, double row, const TFxP &root,
                        bool isPreview);
DVAPI TFxP buildSceneFx(ToonzScene *scene, TXsheet *xsh, double row,
                        const TFxP &root, bool isPreview);
DVAPI TFxP buildPartialSceneFx(ToonzScene *scene, double row, const TFxP &root,
                               int shrink, bool isPreview);
DVAPI TFxP buildPartialSceneFx(ToonzScene *scene, TXsheet *xsh, double row,
                               const TFxP &root, int shrink, bool isPreview);
DVAPI TFxP buildPostSceneFx(ToonzScene *scene, double row, int shrink,
                            bool isPreview);

//-------------------------------------------------------------------------------------------------------------------------

enum BSFX_Transforms_Enum {
  BSFX_NO_TR         = 0x0,  //!< No transform is applied.
  BSFX_CAMERA_TR     = 0x1,  //!< Adds the current camera transform.
  BSFX_CAMERA_DPI_TR = 0x2,  //!< Adds the current camera DPI transform.
  BSFX_COLUMN_TR     = 0x4,  //!< Adds column transforms to all columns.

  BSFX_DEFAULT_TR = BSFX_CAMERA_TR | BSFX_CAMERA_DPI_TR |
                    BSFX_COLUMN_TR  //!< Default includes all transforms.
};  //!< Affine transform included in a built scene
    //!  fx whenever <I>a root fx is specified</I>.

/*!
  \brief    The buildSceneFx() function implements all the supported parameter
  combinations
            historically used by Toonz to decorate a schematic tree for
  rendering purposes.

  \details  Only two parameters are explicitly required from the user: the scene
  to be decorated,
            and the frame at which the decoration takes place.

            All the other parameters are turned to default or taken by current
  settings if not
            specified by the user.
*/

DVAPI TFxP buildSceneFx(ToonzScene *scene, double frame, TXsheet *xsh = 0,
                        const TFxP &root                = TFxP(),
                        BSFX_Transforms_Enum transforms = BSFX_DEFAULT_TR,
                        bool isPreview = false, int whichLevels = -1,
                        int shrink = 1);

// temo non debba andare qui
// in ogni caso gestisce anche lo zdepth
// (utilizzando la camera corrente, il che forse non e' una buona idea)
// ritorna false se la colonna e' "dietro la camera" e quindi non visibile
bool DVAPI getColumnPlacement(TAffine &aff, TXsheet *xsh, double row, int col,
                              bool isPreview);

#endif
