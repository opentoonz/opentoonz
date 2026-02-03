#pragma once

#ifndef TVECTORIMAGE_UTILS_INCLUDED
#define TVECTORIMAGE_UTILS_INCLUDED

#include "tcommon.h"
#include "tvectorimage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

void DVAPI getGroupsList(const TVectorImageP &vi,
                         std::vector<TVectorImageP> &list);

#endif  // TVECTORIMAGE_UTILS_INCLUDED
