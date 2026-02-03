#pragma once

#ifndef T_RASTERCM_INCLUDED
#define T_RASTERCM_INCLUDED

#include "traster.h"
#include "tpixelcm.h"

#undef DVAPI
#undef DVVAR
#ifdef TRASTER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

typedef TRasterT<TPixelCM32> TRasterCM32;

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRasterT<TPixelCM32>>;
template class DVAPI TRasterPT<TPixelCM32>;
#endif
typedef TRasterPT<TPixelCM32> TRasterCM32P;

#endif
