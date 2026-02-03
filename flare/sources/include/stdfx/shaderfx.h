#pragma once

#ifndef SHADERFX_H
#define SHADERFX_H

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZSTDFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//    Forward declarations

class TFilePath;

//=========================================================

void DVAPI loadShaderInterfaces(const TFilePath &shaderInterfacesFolder);

#endif  // SHADERFX_H
