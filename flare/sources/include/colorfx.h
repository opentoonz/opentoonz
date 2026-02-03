#pragma once

#ifndef COLORFX_INCLUDED
#define COLORFX_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef COLORFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

DVAPI void initColorFx();

#endif
