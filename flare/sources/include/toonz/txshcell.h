#pragma once

#ifndef TXSHCELL_INCLUDED
#define TXSHCELL_INCLUDED

#include "toonz/txshlevel.h"
#include "timage.h"

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
//! The TXshCell class provides a cell of xsheet, this structure contains a
//! level frame.
/*!TXshCell is composed by a pointer to level \b TXshLevelP and a \b TFrameId
   that
   identifies right frame in level.

   The class provides functions that return the cell element.
   The getImage() function return image associated to cell.
   The getSimpleLevel() function return simple level \b TXshSimpleLevel to which
   cell belongs.
   The getFrameId() function return \b TFrameId frame identification of cell.
   It's possible to know if cell is empty through isEmpty().
*/
//=============================================================================

class DVAPI TXshCell {
public:
  TXshLevelP m_level;
  TFrameId m_frameId;

  /*!
Constructs TXshCell with default value.
*/
  TXshCell() : m_level(), m_frameId() {}

  /*!
Constructs a TXshCell with simple level \a level,
and a frame identification \a frameId
*/
  TXshCell(const TXshLevelP &level, const TFrameId &frameId)
      : m_level(level), m_frameId(frameId) {}

  /*!
Destroys the TLevelSet object.
*/
  ~TXshCell() {}

  /*!
Returns true if cell \a c and this cell have the same simple level
and the same frameId; otherwise returns false.
*/
  bool operator==(const TXshCell &c) const {
    return (m_level.getPointer() == c.m_level.getPointer() &&
            m_frameId == c.m_frameId);
  }

  /*!
Returns true if cell \a c and this cell have different simple level
or different frameId; otherwise returns false.
*/
  bool operator!=(const TXshCell &c) const {
    return (m_level.getPointer() != c.m_level.getPointer() ||
            m_frameId != c.m_frameId);
  }

  /*!
Returns true if level is equal to zero.
*/
  bool isEmpty() const { return !m_level; }

  /*!
Returns the \b TImageP in the current cell.
\n
If bool \b toBeModified is true the image can be modified.
*/
  TImageP getImage(bool toBeModified, int subsampling = 0) const;

  /*!
Returns the \b TXshSimpleLevel to which cell belongs.
*/
  TXshSimpleLevel *getSimpleLevel() const;

  /*!
Return the \b TXshSoundLevel to which cell belongs.
*/
  TXshSoundLevel *getSoundLevel() const;

  /*!
Return the \b TXshSoundTextLevel to which cell belongs.
*/
  TXshSoundTextLevel *getSoundTextLevel() const;

  /*!
Return the \b TXshZeraryFxLevel to which cell belongs.
*/
  TXshZeraryFxLevel *getZeraryFxLevel() const;

  /*!
Return the \b TXshPaletteLevel to which cell belongs.
*/
  TXshPaletteLevel *getPaletteLevel() const;

  /*!
Return the \b TXshChildLevel to which cell belongs.
*/
  TXshChildLevel *getChildLevel() const;

  /*!
Returns the \b TFrameId correspondent to cell.
*/
  TFrameId getFrameId() const { return m_frameId; }

  /*!
Returns the TPalette associated with current cell, if any.
*/
  TPalette *getPalette() const;
};

#endif
