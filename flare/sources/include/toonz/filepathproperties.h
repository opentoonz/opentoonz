#pragma once

#ifndef FILEPATHPROPERTIES_H
#define FILEPATHPROPERTIES_H

#include "tcommon.h"

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
class TIStream;
class TOStream;

//=============================================================================
// FilePathProperties
// This class defines file path condition for sequential image level

class DVAPI FilePathProperties {
  bool m_useStandard;
  bool m_acceptNonAlphabetSuffix;
  int m_letterCountForSuffix;

public:
  FilePathProperties();

  bool useStandard() { return m_useStandard; }
  void setUseStandard(bool on) { m_useStandard = on; }

  bool acceptNonAlphabetSuffix() { return m_acceptNonAlphabetSuffix; }
  void setAcceptNonAlphabetSuffix(bool on) { m_acceptNonAlphabetSuffix = on; }

  int letterCountForSuffix() { return m_letterCountForSuffix; }
  void setLetterCountForSuffix(int val) { m_letterCountForSuffix = val; }

  void saveData(TOStream& os) const;
  void loadData(TIStream& is);

  bool isDefault();
};

#endif