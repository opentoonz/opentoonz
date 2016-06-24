#pragma once

#ifndef TXSHSOUNDTEXTLEVEL_INCLUDED
#define TXSHSOUNDTEXTLEVEL_INCLUDED

#include "toonz/txshlevel.h"
#include "tsound.h"
#include "tpersist.h"

#include <QList>

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
//! The TXshSoundTextLevel
//=============================================================================

class DVAPI TXshSoundTextLevel final : public TXshLevel {
  PERSIST_DECLARATION(TXshSoundTextLevel)

  DECLARE_CLASS_CODE

  QList<QString> m_framesText;

public:
  TXshSoundTextLevel(std::wstring name = L"");
  ~TXshSoundTextLevel();

  TXshSoundTextLevel *clone() const;

  //! Overridden from TXshLevel
  TXshSoundTextLevel *getSoundTextLevel() override { return this; }

  void setFrameText(int frameIndex, QString);
  QString getFrameText(int frameIndex) const;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void load() override{};
  void save() override{};

private:
  // not implemented
  TXshSoundTextLevel(const TXshSoundTextLevel &);
  TXshSoundTextLevel &operator=(const TXshSoundTextLevel &);
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSoundTextLevel>;
#endif
typedef TSmartPointerT<TXshSoundTextLevel> TXshSoundTextLevelP;

#endif  // TXSHSOUNDTEXTLEVEL_INCLUDED
