#pragma once

#ifndef TEXTUREUTILS_H
#define TEXTUREUTILS_H

#include "ext/ttexturesstorage.h"

//! \file textureutils.h
/*!
  This file contains functions related to the texturization of Toonz scene
  contents,
  namely image levels and whole xsheets.
*/

//==================================================================

//    Forward declarations

class TXsheet;
class TXshSimpleLevel;
class TFrameId;

//==================================================================

//**********************************************************************************************
//    Texture Utility Functions  declaration
//**********************************************************************************************

namespace texture_utils {

//! Returns the OpenGL data of a loaded texture corresponding to sl's content
//! at fid with specified subsampling.
DrawableTextureDataP getTextureData(const TXshSimpleLevel *sl,
                                    const TFrameId &fid, int subsampling);

//! Invalidates any currently stored texture associated with sl at the specified
//! fid.
void invalidateTexture(const TXshSimpleLevel *sl, const TFrameId &fid);

//! Invalidates any currently stored texture associated with sl.
void invalidateTextures(const TXshSimpleLevel *sl);

//------------------------------------------------------------------------------------

//! Returns the OpenGL data of a loaded texture corresponding to xsh's content
//! at the specified frame.
DrawableTextureDataP getTextureData(const TXsheet *xsh, int frame);

//! Invalidates any currently stored texture associated with xsh at the
//! specified frame.
void invalidateTexture(const TXsheet *xsh, int frame);

//! Invalidates any currently stored texture associated with sl.
void invalidateTextures(const TXsheet *xsh);

}  // namespace texture_utils

#endif  // TEXTUREUTILS_H
