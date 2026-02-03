#pragma once

#ifndef TW_COLORS_INCLUDED
#define TW_COLORS_INCLUDED

//#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TwConsts {

DVVAR extern const TGuiColor ToonzBgColor;
DVVAR extern const TGuiColor ToonzHighlightColor;
DVVAR extern const TGuiColor Red;
DVVAR extern const TGuiColor Green;
DVVAR extern const TGuiColor Blue;
DVVAR extern const TGuiColor Yellow;
DVVAR extern const TGuiColor Magenta;
DVVAR extern const TGuiColor Cyan;
DVVAR extern const TGuiColor Pink;

DVVAR extern const TGuiColor Gray128;
DVVAR extern const TGuiColor Gray127;

DVVAR extern const TGuiColor Gray30;
DVVAR extern const TGuiColor Gray60;
DVVAR extern const TGuiColor Gray120;
DVVAR extern const TGuiColor Gray150;
DVVAR extern const TGuiColor Gray180;
DVVAR extern const TGuiColor Gray210;
DVVAR extern const TGuiColor Gray225;
DVVAR extern const TGuiColor Gray240;
}

#endif
