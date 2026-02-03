// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FSWFStream.h

	This header-file contains the declarations of low-level SWF-stream class, i.e.
	
		class FSWFStream;

****************************************************************************************/

#ifndef SWFSTREAM_INCLUDED
#define SWFSTREAM_INCLUDED

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "Macromedia.h"

// #include <stdio.h>
// #include <fstream.h>
#include <list>
#include <vector>

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

// class used to store data before it is written to a .swf file

class DVAPI FSWFStream
{
public:
	FSWFStream();

	U32 Size();
	U32 FullSize();

	U8 *Memory() { return &stream[0]; } // Useful to get the origin of the stream memory.
										// Used by CreateMovie to write the header information.

	void WriteBits(U32 data, U32 size);			   // Writes # of bits size, that has value data
	void WriteLargeData(const U8 *data, U32 size); // Stores the pointer to a large block of data
												   // to be written. Ownership of the data is NOT passed
												   // to this stream.

	//	NOTE:  These functions all take in U32's even though in some situations a smaller data
	//	type can suffice. This is for the sake of simplicity.
	void WriteDWord(U32 data);
	void WriteWord(U32 data);
	void WriteByte(U32 data);

	void FlushBits(); //	Kick out the current partially filled byte to the stream.

	void AppendTag(U16 _tagID, U32 _length, FSWFStream *_tagBody); // Copies the stream, adding tag id
																   // and length information.
	void Append(FSWFStream *_SWFStream);						   // Copies another stream to the end
																   // of this without any changes. The stream
																   // will be empty but still needs to be deleted
																   // by the caller.

	// Utility functions
	void WriteToFile(FILE *out);		 // writes this data to file
	void WriteToFileVersion6(FILE *out); // writes this data to file, compressing with zlib(flash 6)
	void WriteToMemory(U8 *memory);		 // copy this data to a memory buffer (use Size() to make sure it is large enough)

	//functions used to calculate optimal size for fields
	static U32 MinBits(U32 number, U16 sign);
	static U32 MaxNum(S32 a, S32 b, S32 c, S32 d);

private:
	struct LargeData {
		U32 position;
		const U8 *data;
		U32 size;
	};

	std::vector<U8> stream;

	U32 streamPos;
	U32 bytePos;
	U8 currentByte;

	std::list<LargeData> outDataList; // stores pointer data - the overhead is not as bad as it seems
};

#endif
