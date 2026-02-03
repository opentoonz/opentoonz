#pragma once

#ifndef TTIO_STD_INCLUDED
#define TTIO_STD_INCLUDED

#include "tiio.h"

#undef DVAPI
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace Tiio {

DVAPI void defineStd();

}  // namespace

#endif
