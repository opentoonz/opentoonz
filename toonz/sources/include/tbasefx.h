#pragma once

#ifndef TBASEFX_INCLUDED
#define TBASEFX_INCLUDED

// TnzBase includes
#include "trasterfx.h"
#include "tzeraryfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//************************************************************************
//    TBaseRasterFx  definition
//************************************************************************

//! Defines built-in Toonz fxs.
class DVAPI TBaseRasterFx : public TRasterFx
{
public:
	std::string getPluginId() const { return "Base"; }
};

//************************************************************************
//    TBaseZeraryFx  definition
//************************************************************************

//! Defines built-in Toonz zerary fxs.
class DVAPI TBaseZeraryFx : public TZeraryFx
{
public:
	std::string getPluginId() const { return "Base"; }
};

#endif // TBASEFX_INCLUDED
