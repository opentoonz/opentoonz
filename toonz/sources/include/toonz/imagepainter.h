#pragma once

#ifndef IMAGEPAINTER_H
#define IMAGEPAINTER_H

// TnzCore includes
#include "tgeometry.h"
#include "timage.h"
#include "tpixel.h"
#include "ttoonzimage.h"

// TnzExt includes
#include "ext/plasticvisualsettings.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===========================================================================

//    Forward declaration

class TSceneProperties;
class TSceneProperties;

//===========================================================================

namespace ImagePainter {

//*************************************************************************************************
//    ImagePainter::VisualSettings  declaration
//*************************************************************************************************

//! Stores generic settings used by Toonz painters to draw images
class DVAPI VisualSettings {
public:
  int m_colorMask;  //!< Combination of TRop::rChan, gChan, bChan or mChan.
  //!< Note: 0 means rChan | gChan | bChan (draw all channels).
  bool m_greytones;  //!< Whether color channels are visualized in greytones
  int m_bg;          //!< Can be equal to eBlackBg, or eWhiteBg, or eCheckBg
  bool m_drawExternalBG;  // ... not sure ...
  bool m_showBBox;        //!< Show the bounding box of current level

  TSceneProperties *m_sceneProperties;  //!< Scene properties reference - which
                                        //! contains further options

  PlasticVisualSettings m_plasticVisualSettings;  //!< Settings for mesh images
                                                  //! and plastic deformations

  // Other (misplaced) misc options

  bool m_doCompare;      //!< Whehter must compare between two images
  bool m_defineLoadbox;  //!< Flipbook-specific
  bool m_useLoadbox;     //!< Flipbook-specific

  TPixel32 m_blankColor;  //!< The color of blank frames during playback

  bool m_useTexture;  //!< Whether should we use OpenGL textures instead of
                      //! drawPixels()
  bool m_recomputeIfNeeded;
  bool m_drawBlankFrame;
  bool m_useChecks;  //!< whether to consider  paint check and ink check
  bool m_forSceneIcon = false;  // whether it is redered for the scene icons
public:
  VisualSettings();

  bool needRepaint(const VisualSettings &newVs) const;
};

//*************************************************************************************************
//    ImagePainter::VisualSettings  declaration
//*************************************************************************************************

//! Stores settings used to compare two images
class DVAPI CompareSettings {
public:
  //! Used to set compared image width
  double m_compareX;
  //! Used to set compared image height
  double m_compareY;

  //! Used to change compared image width
  bool m_dragCompareX;
  //! Used to change compared image height
  bool m_dragCompareY;

  //! Used to swap compared images
  bool m_swapCompared;

public:
  CompareSettings();
};

const double DefaultCompareValue = 0.01;

//*************************************************************************************************
//    ImagePainter  global functions
//*************************************************************************************************

DVAPI void paintImage(const TImageP &image, const TDimension &imageSize,
                      const TDimension &viewerSize, const TAffine &aff,
                      const VisualSettings &visualSettings,
                      const CompareSettings &compareSettings,
                      const TRect &loadbox);
}

#endif  // IMAGEPAINTER_H
