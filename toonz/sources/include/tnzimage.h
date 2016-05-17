#pragma once

#ifndef TNZIMAGE_INCLUDED
#define TNZIMAGE_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef IMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

DVAPI void initImageIo(bool lightVersion = false);

#endif
