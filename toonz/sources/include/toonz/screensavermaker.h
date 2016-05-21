#pragma once

#ifndef SCREENSAVERMAKER_INCLUDED
#define SCREENSAVERMAKER_INCLUDED

#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

void DVAPI makeScreenSaver(
	TFilePath scr,
	TFilePath swf,
	std::string screenSaverName);

void DVAPI previewScreenSaver(
	TFilePath scr);

void DVAPI installScreenSaver(
	TFilePath scr);

#endif
