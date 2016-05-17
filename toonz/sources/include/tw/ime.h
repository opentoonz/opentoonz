#pragma once

#ifndef T_IME_INCLUDED
#define T_IME_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

DVAPI void setImePosition(int x, int y, int size);
DVAPI void closeImeWindow();

DVAPI void enableIme(TWidget *w, bool on);

#endif
