// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTBitmaps.h

	This header-file contains the declarations of all low-level bitmap-related classes, 
	which are all derived from class FDT:
  
		class FDTDefineBits;
		class FDTDefineBitsJPEG2;
		class FDTDefineBitsJPEG3;
		class FDTDefineBitsLosslessBase;
		class FDTDefineBitsLossless : public FDTDefineBitsLosslessBase;
		class FDTDefineBitsLossless2 : public FDTDefineBitsLosslessBase;
		class FDTJPEGTables;

****************************************************************************************/

#ifndef _F_DEFINE_BITMAPS_H_
#define _F_DEFINE_BITMAPS_H_

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "FDT.h"

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

struct FRGB;
struct FRGBA;

// A flash object which defines a jpeg bitmap image (flash 1.0)

class DVAPI FDTDefineBits : public FDT
{

public:
	// constructed with image size and a pointer to the actual jpeg stream
	FDTDefineBits(U32 _size, U8 *_image);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	U32 size;
	U8 *image;
};

// A flash object which defines a jpeg bitmap image (flash 2.0)
// Differs from FDTDefineBits in that the encoding data and image data are contained in the object separately, but this DefineBitsJPEG2 doesn't really do this (as does not flash)...
// An empty stream is writen where encoding data should normally be encountered and all the JPEG and encoding data is written within the JPEG stream

class DVAPI FDTDefineBitsJPEG2 : public FDT
{

public:
	FDTDefineBitsJPEG2(U8 *_JPEGStream, U32 _JPEGSize);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	~FDTDefineBitsJPEG2();

private:
	U16 characterID;
	U32 JPEGSize;
	U8 *JPEGStream;
};

// A flash object which defines a jpeg bitmap image (flash 3.0)
// Differs from FDTDefineBitsJPEG2 in that Alpha transparency data is contained in this object

class DVAPI FDTDefineBitsJPEG3 : public FDT
{

public:
	FDTDefineBitsJPEG3(U8 *_JPEGStream, U32 _JPEGSize,
					   U8 *_alphaStream, U32 _alphaSize);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	U32 alphaSize;
	U32 JPEGSize;
	U8 *JPEGStream;
	U8 *alphaStream;
};

// Base class for FDTDefineBitsLossless (below) and FDTDefineBitsLossless2.
// Please see those two classes for descripion of parameters.
// Note this class can be constructed - it will write the correct tag when
// WriteToSWFStream is called. Or one of the child classes can be used.

class DVAPI FDTDefineBitsLosslessBase : public FDT
{

public:
	enum {
		bm1Bit,  // 2 color
		bm2Bit,  // 4 color
		bm4Bit,  // 16 color
		bm8Bit,  // 256 color
		bm16Bit, // high
		bm32Bit  // true
	};

	virtual U16 ID() { return characterID; }

	FDTDefineBitsLosslessBase(U8 _format,
							  U16 _width,
							  U16 _height,
							  int _colorTableCount,
							  const void *_colorTableData,
							  const void *_imageData,
							  bool alpha);
	virtual ~FDTDefineBitsLosslessBase();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	U8 format;
	U16 width;
	U16 height;
	int colorTableCount;
	bool alpha;
	TUINT32 compressedSize;	// a count of how many bytes are in the compressed buffer
	unsigned char *compressed; // pointer to the compressed data
};

// Defines a loss-less bitmap object, like a GIF, BMP, or PCT.
// This version does not accept alpha channel data - FDTDefineBitsLossless2 does.
// Accepts raw bitmap data and compresses it.

class DVAPI FDTDefineBitsLossless : public FDTDefineBitsLosslessBase
{

public:
	FDTDefineBitsLossless(U8 _format, // format, from FDTDefineBitsLosslessBase.
						  U16 _width, // size of the image
						  U16 _height,
						  int _colorTableCount,		   // how many entries in the color table - consistent
													   // with format. May be 0.
						  const FRGB *_colorTableData, // Null if no color cable. RGB data
						  const void *_imageData	   // Pointer to the image. (byte aligned.)
						  );
};

// Defines a loss-less bitmap object, like a GIF, BMP, or PCT.
// This version requires alpha channel data. Note the color table is RGBA.
// Accepts raw bitmap data and compresses it.

class DVAPI FDTDefineBitsLossless2 : public FDTDefineBitsLosslessBase
{

public:
	FDTDefineBitsLossless2(U8 _format, // format, from FDTDefineBitsLosslessBase.
						   U16 _width, // size of the image
						   U16 _height,
						   int _colorTableCount,		 // how many entries in the color table - consistent
														 // with format. May be 0.
						   const FRGBA *_colorTableData, // Null if no color cable. RGB data
						   const void *_imageData		 // Pointer to the image. (byte aligned.)
						   );
};

//the JPEGTable structure (contains the encoding scheme for all JPEGs defined using DefineBits

class DVAPI FDTJPEGTables : public FDT
{

public:
	FDTJPEGTables(U32 encodingDataSize, U8 *encodingData);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 encodingDataSize;
	U8 *encodingData;
};
#endif
