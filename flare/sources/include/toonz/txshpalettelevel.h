#pragma once

#ifndef TXshPaletteLevel_INCLUDED
#define TXshPaletteLevel_INCLUDED

#include "toonz/txshlevel.h"
#include "tpalette.h"
#include <map>
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TContentHistory;

//=============================================================================
//! The TXshPaletteLevel class represents a palette uncoupled from a level
/*!Inherits \b TXshLevel.
\n A palette can be loaded in the xsheet and subsequently can be used as
   "external palette". The number of frames of the palette-level depends on
   the palette animation.

 */
//=============================================================================

class DVAPI TXshPaletteLevel final : public TXshLevel {
private:
  PERSIST_DECLARATION(TXshPaletteLevel)

  TFilePath m_path;
  TPalette *m_palette;
  TContentHistory *m_contentHistory;

  DECLARE_CLASS_CODE

public:
  /*!
Constructs a TXshPaletteLevel with \b name
*/
  TXshPaletteLevel(std::wstring name = L"");

  /*!
Destroys the TXshPaletteLevel object.
*/
  ~TXshPaletteLevel();

  /*!
Return the \b TXshPaletteLevel level (overridden from TXshLevel)
*/
  TXshPaletteLevel *getPaletteLevel() override { return this; }

  /*!
Return the \b TPalette
\sa setPalette()
*/
  TPalette *getPalette() const;

  /*!
Set the level palette to \b palette.
\sa getPalette()
*/
  void setPalette(TPalette *palette);

  /*!
Return level path.
\sa setPath()
*/
  TFilePath getPath() const override { return m_path; }

  /*!
Set level path.
\sa getPath()
*/
  void setPath(const TFilePath &path);

  //! Returns the frame count. It depends on the palette animation:
  //! for a not-animated palette getFrameCount() == 1
  int getFrameCount() const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void load() override;
  void save() override;
  void save(const TFilePath &fp);

  //! note gets the contentHistory. can be 0
  const TContentHistory *getContentHistory() const { return m_contentHistory; }
  TContentHistory *getContentHistory() { return m_contentHistory; }

  //! destroys the old contentHistory and replaces it with the new one. Gets
  //! ownership
  void setContentHistory(TContentHistory *contentHistory);

private:
  // not implemented
  TXshPaletteLevel(const TXshPaletteLevel &);
  TXshPaletteLevel &operator=(const TXshPaletteLevel &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshPaletteLevel>;
#endif
typedef TSmartPointerT<TXshPaletteLevel> TXshPaletteLevelP;

#endif
