// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTBitmaps.cpp

  This source file contains the definition for all low-level bitmap-related functions,
  grouped by classes, which are all derived from class FDT, and related to bitmaps:


		Class						Member Function

	FDTDefineBits				FDTDefineBits(U32, U8*);
								U16 ID(void);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineBitsJPEG2			FDTDefineBitsJPEG2(U8*, U32);
								U16 FDTDefineBitsJPEG2::ID(void);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineBitsJPEG3			FDTDefineBitsJPEG3(U8*, U32, U8*, U32);
								U16 FDTDefineBitsJPEG3::ID(void);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineBitsLosslessBase	FDTDefineBitsLosslessBase(U8, U8, U16, int, 
														  void*, void*, bool);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineBitsLossless		FDTDefineBitsLossless(U8, U16, U16, int, FRGB*, void*);

	FDTDefineBitsLossless2		FDTDefineBitsLossless(U8, U16, U16, int, FRGBA*, void*);

	FDTJPEGTables				FDTJPEGTables(U32, U8*);
								void WriteToSWFStream(FSWFStream*);



****************************************************************************************/

#ifdef WIN32
#pragma warning(disable : 4786)
#endif

#include "FSWFStream.h"
#include "FObj.h"
#include "FDTBitmaps.h"
#include "zlib.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBits ----------------------------------------------------------

FDTDefineBits::FDTDefineBits(U32 _size, U8 *_image)
{
	size = _size;
	image = _image;
	characterID = FObjCollection::Increment();
}

U16 FDTDefineBits::ID(void)
{
	return characterID;
}

void FDTDefineBits::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);
	body.WriteLargeData(image, size);

	_SWFStream->AppendTag(stagDefineBits, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBitsJPEG2 -----------------------------------------------------

FDTDefineBitsJPEG2::FDTDefineBitsJPEG2(U8 *_JPEGStream, U32 _JPEGSize)
{

	JPEGStream = new U8[_JPEGSize];
	memcpy(JPEGStream, _JPEGStream, _JPEGSize);
	JPEGSize = _JPEGSize;
	characterID = FObjCollection::Increment();
}

U16 FDTDefineBitsJPEG2::ID(void)
{

	return characterID;
}

FDTDefineBitsJPEG2::~FDTDefineBitsJPEG2()
{
}

void FDTDefineBitsJPEG2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)characterID);

	// 2 bytes indicating end of encoding stream
	// no encoding data is written here because it is an empty stream
	body.WriteByte(0xff);
	body.WriteByte(0xd9);

	//2 bytes indicating beginning of JPEG stream
	body.WriteByte(0xff);
	body.WriteByte(0xd8);

	//the entire JPEG stream
	body.WriteLargeData(JPEGStream, JPEGSize);

	//2 bytes indicating end of JPEG stream
	body.WriteByte(0xff);
	body.WriteByte(0xd9);

	_SWFStream->AppendTag(stagDefineBitsJPEG2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBitsJPEG3 -----------------------------------------------------

FDTDefineBitsJPEG3::FDTDefineBitsJPEG3(U8 *_JPEGStream, U32 _JPEGSize,
									   U8 *_alphaStream, U32 _alphaSize)
{
	JPEGStream = _JPEGStream;
	JPEGSize = _JPEGSize;
	alphaStream = _alphaStream;
	alphaSize = _alphaSize;
	characterID = FObjCollection::Increment();
}

U16 FDTDefineBitsJPEG3::ID(void)
{
	return characterID;
}

void FDTDefineBitsJPEG3::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)characterID);

	//offset includes the 2 end of stream tags and the 1 beginning stream tag
	U32 offset = JPEGSize + 6;
	body.WriteDWord(offset);

	// 2 bytes indicating end of encoding stream
	// no encoding data is written here
	// an empty stream is written
	body.WriteByte(0xff);
	body.WriteByte(0xd9);

	//2 bytes indicating begining of JPEG stream
	body.WriteByte(0xff);
	body.WriteByte(0xd8);

	//the entire JPEG stream
	body.WriteLargeData(JPEGStream, JPEGSize);

	//2 bytes indicating end of JPEG steam
	body.WriteByte(0xff);
	body.WriteByte(0xd9);

	// alpha data
	body.WriteLargeData(alphaStream, alphaSize);

	_SWFStream->AppendTag(stagDefineBitsJPEG3, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBitsLosslessBase ----------------------------------------------

FDTDefineBitsLosslessBase::FDTDefineBitsLosslessBase(U8 _format,
													 U16 _width,
													 U16 _height,
													 int _colorTableCount,
													 const void *_colorTableData,
													 const void *_imageData,
													 bool _alpha)
{
	format = _format;
	width = _width;
	height = _height;
	colorTableCount = _colorTableCount;
	alpha = _alpha;
	characterID = FObjCollection::Increment();

	// copy the memory to another block to be compressed
	int rgbBytes = (alpha) ? 4 : 3;
	int tableBytes = colorTableCount * rgbBytes;

	int bits = (1 << format); // how many bits does this format have?

	int imageBytes = (width * height * bits + 7) / 8;
	TUINT32 totalBytes = imageBytes + tableBytes;

	// copy the image and the table to a new buffer
	unsigned char *raw = new unsigned char[totalBytes];
	if (tableBytes) {
		memcpy(raw, _colorTableData, tableBytes);
	}
	memcpy(&raw[tableBytes], _imageData, imageBytes);

	// a compressed buffer - the allocated size is based on a zlib formula
	compressedSize = totalBytes + totalBytes / 100 + 12;
	compressed = new unsigned char[compressedSize];

	// now compress the raw data - note this will change compressedSize
	int ret = compress2(compressed, (uLongf *)&compressedSize, raw, totalBytes, Z_BEST_COMPRESSION);

	FLASHASSERT(ret == Z_OK);

	delete[] raw;
}

FDTDefineBitsLosslessBase::~FDTDefineBitsLosslessBase()
{
	delete[] compressed;
}

void FDTDefineBitsLosslessBase::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);
	body.WriteByte((U32)format);
	body.WriteWord((U32)width);
	body.WriteWord((U32)height);

	if (format <= bm8Bit) {
		body.WriteByte((U32)(colorTableCount - 1));
	}

	body.WriteLargeData(compressed, compressedSize);

	if (alpha)
		_SWFStream->AppendTag(stagDefineBitsLossless2, body.Size(), &body);
	else
		_SWFStream->AppendTag(stagDefineBitsLossless, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBitsLossless --------------------------------------------------

FDTDefineBitsLossless::FDTDefineBitsLossless(U8 _format,
											 U16 _width,
											 U16 _height,
											 int _colorTableCount,
											 const FRGB *_colorTableData,
											 const void *_imageData)
	: FDTDefineBitsLosslessBase(_format, _width, _height, _colorTableCount, _colorTableData, _imageData, false)
{
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineBitsLossless2 -------------------------------------------------

FDTDefineBitsLossless2::FDTDefineBitsLossless2(U8 _format,
											   U16 _width,
											   U16 _height,
											   int _colorTableCount,
											   const FRGBA *_colorTableData,
											   const void *_imageData)
	: FDTDefineBitsLosslessBase(_format, _width, _height, _colorTableCount, _colorTableData, _imageData, true)
{
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTJPEGTables ----------------------------------------------------------

//	Constructor.  Currently takes in a U32 indicating the size of the data in bytes, and
//	a pointer to the beginning of the stream of data.
FDTJPEGTables::FDTJPEGTables(U32 encodingDataSize, U8 *encodingData)
{
	this->encodingData = encodingData;
	this->encodingDataSize = encodingDataSize;
}

//	Writes to the given _SWFStream.

void FDTJPEGTables::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	body.WriteLargeData(encodingData, encodingDataSize);

	_SWFStream->AppendTag(stagJPEGTables, body.Size(), &body);
}
