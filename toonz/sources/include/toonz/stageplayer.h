#pragma once

#ifndef STAGEPLAYER_H
#define STAGEPLAYER_H

// TnzCore includes
#include "timage.h"
#include "trastercm.h"
#include "tgl.h"

// TnzExt includes
#include "ext/ttexturesstorage.h"

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

//    Forward declarations

class TXsheet;
class TXshSimpleLevel;
class TXshColumn;
class TStageObject;

//=============================================================================

namespace Stage {

DVVAR extern const double inch;

//*****************************************************************************************
//    Stag::Player  declaration
//*****************************************************************************************

//! Stage::Player is the class that gets visited during standard (simplified)
//! scene drawing.
/*!
  Whenever a Toonz scene needs to be drawn on screen (a simplified form of scene
rendering,
  without fxs and other stuff), a SceneBuilder instance traverses the scene
content and
  compiles a sequence of Stage::Player objects (ordered by Z and stacking
order).
\n\n
  A Stage::Player consists roughly of all the data necessary to draw an image of
a level, in
  particular the image itself.
  Further data include the placement affine and the scaling dpi affine that can
be composed
  to get the actual affine the image must be rendered with. Other data is
available; check
  the public class members for details.
\n\n
  Once the sequence has been completed, a heir of the Stage::Visitor class can
then visit
  each of the Stage::Player objects and perform scene rendering.
*/

class DVAPI Player {
public:
  std::vector<int> m_masks;

  TAffine m_placement;  //!< Placement affine (no dpi)
  TAffine m_dpiAff;     //!< The affine corresponding to m_image's dpi

  double m_z;   //!< Z coordinate of the rendered image (? cumulative ?)
  double m_so;  //!< Secondary stacking parameter (used on equal Zs)

  int m_onionSkinDistance;  //!< Temporal distance from an onion skin
                            //!'companion' (supposedly)

  int m_ancestorColumnIndex;  //!< Index of this object's hierarchycal root
                              //!(top-most column)
  bool m_isCurrentColumn;  //!< Whether this player emanates from (a descendant
                           //! of) the current column
  bool m_isCurrentXsheetLevel;  //!< Whether the player's xsheet is the \a
                                //! current one
  bool m_isEditingLevel;
  bool m_isVisibleinOSM;  // Whether the current frame is in the onion skin
  int m_isGuidedDrawingEnabled = 0;
  TXshSimpleLevel *m_sl;  //!< (not owned) The player's simple level
  TFrameId m_fid;         //!< The player's frame in m_sl
  TFrameId
      m_currentFrameId;  // The current frame selected in the level or xsheet
  TXsheet *m_xsh;        //!< (not owned) The player's xsheet
  int m_column;          //!< The player's xsheet column
  int m_frame;  //!< The player's xhseet row (could be not the current, due to
                //! onion skin?)

  bool m_isPlaying;  //!< Whether this player is in a 'play >' status or rather
                     //!'paused'
  //!< image rendering. I guess. Don't know why this should be useful.

  UCHAR m_opacity;   //!< Opacity the image should be rendered with.
  int m_bingoOrder;  //!< Probably obsolete. Ignore it.

  static double m_onionSkinFrontSize;
  static double m_onionSkinBackSize;
  static double m_firstBackOnionSkin;
  static double m_lastBackVisibleSkin;

  TPixel32 m_filterColor;

public:
  Player();

  TImageP image() const;  //!< Returns the image associated to m_sl at m_fid

  //! Returns the texture data associated to the player
  /*!
\note In case of vector images or sub-xsheet levels, the texture will have a
fixed 1024 x 1024 resolution.
*/
  DrawableTextureDataP texture() const;
};

}  // namespace Stage

#endif  // STAGEPLAYER_H
