// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.
// Last Modified On 18/06/2002  by DV for Fixes.
/****************************************************************************************

				File Summary: FObj.h

	This header-file contains the declarations of low-level FObj-related classes.
	All parent classes are in the parentheses:

		class FObj;
		class FFragment; (public FObj)
		class FObjCollection;

****************************************************************************************/

#ifndef _FOBJ_H_
#define _FOBJ_H_

#include "tcommon.h"
#include <assert.h>
#include <list>

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "Macromedia.h"
#include "FPrimitive.h"

#undef DVAPI
#undef DVVAR
#ifdef TFLASH_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//All Flash tagged data block objects fall into this category

class DVAPI FObj
{
public:
	virtual ~FObj() {} // Doesn't do anything, just makes all the other destructors virtual

	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
	virtual U32 IsShowFrame() { return false; }
	virtual U32 IsPlaceObject() { return false; }
	virtual bool IsDefineShape() { return false; }
	virtual bool IsSprite() { return false; }

	virtual void SetId(U16 id) { FLASHASSERT(0); }
	virtual void SetMatrix(FMatrix *matrix) { FLASHASSERT(0); }
};

/*! A class for writing an arbitrary block of data (a SWF fragment, perhaps) to
 *	the SWFStream. The data is assumed to be large, so it is not changed.
 */
class FFragment : public FObj
{
public:
	FFragment(const void *data, int size);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	const void *data;
	int size;
};

//	Holds a collection of FObj's so that they can be dumped in a SWF format
//Writes tags in the same order as they appear in the collection

//using namespace std;

class DVAPI FObjCollection
{

public:
	FObjCollection(void);
	~FObjCollection(void);
	void AddFObj(FObj *fobj);
	void InsertFObj(int beforeTag, FObj *fobj);
	void EraseFObj(int tagindex);

	void WriteToSWFStream(FSWFStream *_SWFStream);
	//void CreateCompressedMovie( const TFilePath &_fileName, FRect cameraBox, U32 _frameRate = 12 );
	//void CreateMovie( const TFilePath &_fileName, FRect cameraBox, U32 _frameRate = 12, U8 _version = 6);
	void CreateCompressedMovie(FILE *fp, FRect cameraBox, U32 _frameRate = 12);
	void CreateMovie(FILE *fp, FRect cameraBox, U32 _frameRate = 12, U8 _version = 6);
	void CreateMovie(FSWFStream *stream, FRect cameraBox, U32 _frameRate = 12, U8 _version = 6);
	void CreateSprite(FSWFStream *stream, FRect cameraBox, U32 _frameRate = 12);
	static U16 Increment(void);
	int GetFObjCount() const;
	FObj *GetFObj(int i) const;

private:
	enum {
		HEADER_SIZE = 21
	};

	U32 numOfFrames;
	std::list<FObj *> FObjList;
	void WriteFileHeader(U8 *target, U32 _fileLengthNoHeader, FRect cameraBox,
						 U32 _frameRate, U8 _version);
	void WriteEndTag(FSWFStream *_SWFStream);
	static U16 characterID;
};

#ifdef WIN32 // added from DV
#pragma warning(pop)
#endif

#endif
