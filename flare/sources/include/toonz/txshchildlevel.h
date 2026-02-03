#pragma once

#ifndef TXSHCHILDLEVEL_INCLUDED
#define TXSHCHILDLEVEL_INCLUDED

#include "toonz/txshlevel.h"

#include "traster.h"

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

//=============================================================================
//! The TXshChildLevel class provides a child level of xsheet and allows its
//! management.
/*!Inherits \b TXshLevel.
\n A child level getChildLevel() is a xsheet reduced to level.
   The class allows to edit xsheet getXsheet(), change xsheet setXsheet() and
change
   scene xsheet setScene().
   It's possibile change level name setName() and know how many frame are in
level
   getFrameCount().

   The class provides methods to control child level icon, makeIcon(), getIcon()
   and invalidateIcon().
 */
//=============================================================================

class DVAPI TXshChildLevel final : public TXshLevel {
  PERSIST_DECLARATION(TXshChildLevel)

  TXsheet *m_xsheet;
  std::string m_iconId;

  DECLARE_CLASS_CODE
public:
  /*!
Constructs a TXshChildLevel with \b TXshLevel name \b name
*/
  TXshChildLevel(std::wstring name = L"");

  /*!
Destroys the TXshChildLevel object.
*/
  ~TXshChildLevel();

  /*!
Return the \b TXshChildLevel child level.
*/
  TXshChildLevel *getChildLevel() override { return this; }
  /*!
Return the level \b TXsheet.
\sa setXsheet()
*/
  TXsheet *getXsheet() { return m_xsheet; }
  /*!
Set the level \b TXsheet to \b xsheet.
\sa getXsheet()
*/
  void setXsheet(TXsheet *xsheet);

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void load() override {}
  void save() override {}

  /*!
Override. Set level scene to \b scene.
*/
  void setScene(ToonzScene *scene) override;
  /*!
Return the frame count.
*/
  int getFrameCount() const override;
  /*!
Set the vector \b fids to the frameId of level.
*/
  void getFids(std::vector<TFrameId> &fids) const override;

private:
  // not implemented
  TXshChildLevel(const TXshChildLevel &);
  TXshChildLevel &operator=(const TXshChildLevel &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshChildLevel>;
#endif
typedef TSmartPointerT<TXshChildLevel> TXshChildLevelP;

#endif
