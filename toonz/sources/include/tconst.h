#pragma once

#ifndef TCONST_INCLUDED
#define TCONST_INCLUDED

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TGEOMETRY_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TConst {

const TPointD nowhere(0.1234e10, 0.5678e10);
};

#endif
