#pragma once

#ifndef _TRANSPARENCYCHECK__
#define _TRANSPARENCYCHECK__

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TROP_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

DVAPI void setCheckTransparencyMode(bool mode);

#endif
