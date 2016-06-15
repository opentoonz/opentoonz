#pragma once

#ifndef TTIO_JPG_UTIL_INCLUDED
#define TTIO_JPG_UTIL_INCLUDED

#include "traster.h"

#undef DVAPI
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace Tiio {

DVAPI void createJpg(std::vector<UCHAR> &buffer, const TRaster32P &ras,
                     int quality = 99);

}  // namespace

#endif
