#pragma once

#ifndef TBINARIZER_H
#define TBINARIZER_H

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

class DVAPI TBinarizer {
  bool m_alphaEnabled;

private:
public:
  TBinarizer();
  void process(const TRaster32P &ras);

  void enableAlpha(bool enabled) { m_alphaEnabled = enabled; }
  bool isAlphaEnabled() const { return m_alphaEnabled; }
};

#endif  // TBINARIZER_H
