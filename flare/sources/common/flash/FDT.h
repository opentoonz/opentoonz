// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDT.h

	This header-file contains the declaration of low-level class FDT. It is derived from
	low-level class FObj, and also an abstract class from which all other low-level
	FDTxxxx classes are derived.

****************************************************************************************/

#ifndef F_D_T_H_
#define F_D_T_H_

#include "FObj.h"

// A "define type" flash object
// Flash objects are separated into define and control types
// distinction neccecary because in a flash frame, all define objects must come before control objects

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TFLASH_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI FDT : public FObj
{
public:
	virtual ~FDT() {}
	virtual void WriteToSWFStream(FSWFStream * /*_SWFStream*/) {}
	// Defines, used by the font system. Perhaps not the best place for them, but better than
	// the global situation.	lee@middlesoft
	enum {
		ShiftJIS = 1,
		Unicode = 2,
		ANSI = 3
	};

	virtual U16 ID(void)
	{
		FLASHASSERT(0);
		return 0;
	}
	virtual void SetId(U16 id) { FLASHASSERT(0); }
};

#ifdef WIN32 // added from DV
#pragma warning(pop)
#endif
#endif
