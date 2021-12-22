#pragma once

#ifndef TMOVSETTINGS_INCLUDED
#define TMOVSETTINGS_INCLUDED

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#if !defined(x64) && !defined(__LP64__) && !(defined(LINUX) || defined(FREEBSD)) && !(defined(__GNUC__) && defined(_WIN32))

#ifdef _WIN32

#define list QuickTime_list
#define map QuickTime_map
#define iterator QuickTime_iterator
#define float_t QuickTime_float_t
#define GetProcessInformation QuickTime_GetProcessInformation
#define int_fast8_t QuickTime_int_fast8_t
#define int_fast16_t QuickTime_int_fast16_t
#define uint_fast16_t QuickTime_uint_fast16_t

#include "QTML.h"
#include "Movies.h"
#include "Script.h"
#include "FixMath.h"
#include "Sound.h"

#include "QuickTimeComponents.h"
#include "tquicktime.h"

#undef list
#undef map
#undef iterator
#undef float_t
#undef GetProcessInformation
#undef int_fast8_t
#undef int_fast16_t
#undef uint_fast16_t

#include "texception.h"
#include "tpropertytype.h"
//#include "timageinfo.h"
//#include "tlevel_io.h"
#include "tproperty.h"

#else  // _WIN32

#define list List
#define map Map
#define iterator Iterator
#undef float_t
#include <Carbon/Carbon.h>
#include <QuickTime/Movies.h>
#include <QuickTime/ImageCompression.h>
#include <QuickTime/QuickTimeComponents.h>

#undef list
#undef map
#undef iterator
#undef float_t

#endif  // !_WIN32

void DVAPI fromPropertiesToAtoms(TPropertyGroup &pg, QTAtomContainer &atoms);
void DVAPI fromAtomsToProperties(const QTAtomContainer &atoms,
                                 TPropertyGroup &pg);

#endif  //! 64 bit

void DVAPI openMovSettingsPopup(TPropertyGroup *props,
                                bool macBringToFront = false);

#endif
