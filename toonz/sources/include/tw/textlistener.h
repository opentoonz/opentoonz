#pragma once

#ifndef TW_TEXT_LISTENER_INCLUDED
#define TW_TEXT_LISTENER_INCLUDED

//#include "tw/tw.h"

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TTextListener {
public:
#ifndef MACOSX
  // togliere ifndef con il gcc3.3.2
  virtual bool getCaret(TPoint &pos, int &height) = 0;
#endif
};

#endif
