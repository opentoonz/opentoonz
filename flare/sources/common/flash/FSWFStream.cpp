// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FSWFStream.cpp

  This source file contains the definition for all low-level SWF-stream functions:
  
		Class						Member Function

	FSWFStream					FSWFStream();
								void WriteBits(U32, U32);
								void WriteLargeData(U8*, U32);
								void FlushBits();
								void WriteDWord(U32);
								void WriteWord(U32);
								void WriteByte(U32);
								U32 Size();
								void Append(FSWFStream *);
								void WriteToFile( FILE*);
								void AppendTag(U16, U32, FSWFStream*);
								U32 MinBits(U32, U16);
								U32 MaxNum(S32, S32, S32, S32);

****************************************************************************************/
#ifdef WIN32
#pragma warning(disable : 4786)
#endif

#ifndef _DEBUG
#undef _STLP_DEBUG
#else
#define _STLP_DEBUG 1
#endif

#include "zlib.h"
#include "FObj.h"
#include "FSWFStream.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FSWFStream  ------------------------------------------------------------

// FSWFStream is the object used to store a string of bytes, an SWF file "in memory"
// before being written to the disk.  It handles bit packing and endian issues.

// SWF uses bit packing a lot to reduce the size of the data going over the Net.
// The issue here of course is how to pack and unpack the bits correctly.

FSWFStream::FSWFStream()
{
	streamPos = 0;   //the position in the FSWFStream(how many bytes you have put in)
	bytePos = 8;	 //the number of bits left to fill in the current byte
	currentByte = 0; //the value of the current byte being created

#ifndef DEBUG
	stream.reserve(1024); // start out at 1k to make faster -
						  // if it is a debug build, don't reserve, as we wish to stress the system
#endif
	stream.push_back(0);
}

//	Adds 'size' bits from 'data' to the stream FSWFStream. Data is in the form of
//	a U32.  Size indicates how many of the 32 bits are significant and should
//	be output.  It checks how many bits are available in the current output byte
//	and works by repeatedly stuffing it with the next bits from 'data'
//	and then adding currentByte to the stream until all "size" bits have been output.

void FSWFStream::WriteBits(U32 data, U32 size) //adds individual bits
{
	FLASHASSERT(((int)data <= (0x01 << size) - 1) || (-(S32)(data) <= (0x01 << size) - 1));
	while (size != 0) {
		if (size > bytePos) {
			//if more bits left to write than shift out what will fit
			currentByte |= data << (32 - size) >> (32 - bytePos);

			// shift all the way left, then right to right
			// justify the data to be or'ed in
			stream[streamPos] = currentByte;
			streamPos++;
			stream.push_back(0);
			size -= bytePos;
			currentByte = 0;
			bytePos = 8;
		} else if (size <= bytePos) {
			currentByte |= data << (32 - size) >> (32 - bytePos);
			bytePos -= size;
			size = 0;

			if (!bytePos) { //if current byte is filled
				stream[streamPos] = currentByte;
				streamPos++;
				stream.push_back(0);
				currentByte = 0;
				bytePos = 8;
			}
		}
	}
}

//	For adding large data that is pointed to by a pointer.  The data is only
//	integrated into the stream when it is actually written to disk.
//	E. G. a large JPEG. This is to avoid storing it twice.
//	Stores the current streamPos, data pointer and size in the OutDataList.

void FSWFStream::WriteLargeData(const U8 *data, U32 size)
{
	LargeData large;

	large.position = streamPos;
	large.data = data;
	large.size = size;

	outDataList.push_back(large);
}

//	Kick out the current partially filled byte to the stream.
//	If there is a byte currently being built for addition to the stream, then the end of that
//	byte is filled with zeroes and the byte is added to the stream.

void FSWFStream::FlushBits()
{

	if (bytePos != 8) {

		stream[streamPos] = currentByte;
		streamPos++;
		stream.push_back(0);
		currentByte = 0;
		bytePos = 8;
	}
}

//

//  Writes a 32 bit stream of data to given FSWFStream in the proper form (reversed byte order),
//  so B1B2B3B4 is written as B4B3B2B1. The function does this by sending a byte at a time
//  of the data to the FSWFStream in the appropriate order.

void FSWFStream::WriteDWord(U32 data)
{
	//declare variable used to output the bytes
	U32 v;

	FlushBits(); //vincenzo: l'ho messo io!! col cazzo che era "no bit swapping"! stronzi!

	//output the rightmost byte
	v = data << 24 >> 24;
	WriteBits(v, 8);

	//output the center right byte
	v = data << 16 >> 24;
	WriteBits(v, 8);

	//output the center left byte
	v = data << 8 >> 24;
	WriteBits(v, 8);

	//output the leftmost byte
	v = data >> 24;
	WriteBits(v, 8);
}

//  Writes a 16 bit stream of data to the FSWFStream in the proper form, so B1B2 is written as
//  B2B1.

void FSWFStream::WriteWord(U32 data)
{

	//declare the variable used to output the bytes
	U32 v;
	FlushBits(); //vincenzo: l'ho messo io!! col cazzo che era "no bit swapping"! stronzi!

	//output the rightmost byte
	v = data << 24 >> 24;
	WriteBits(v, 8);

	//output the leftmost byte
	v = data << 16 >> 24;
	WriteBits(v, 8);
}

//  Writes an 8 bit stream of data to the FSWFStream.  There is no bit swapping!! A byte is
//  written as a byte.

void FSWFStream::WriteByte(U32 data)
{

	//declare the variable used to output the byte
	U32 v = 0;
	FlushBits(); //vincenzo: l'ho messo io!! col cazzo che era "no bit swapping"! stronzi!

	//output the byte
	v = data << 24 >> 24;

	WriteBits(v, 8);
}

//	Returns the size of the FSWFStream.  For purposes of denoting size in tags and headers.

U32 FSWFStream::Size(void)
{
	int size = (int)streamPos;
	std::list<LargeData>::iterator it;

	for (it = outDataList.begin(); it != outDataList.end(); it++) {
		LargeData &data = (*it);
		size += data.size;
	}
	return size;
}

//------------------------------------------------------------------
U32 FSWFStream::FullSize(void)
{
	U32 currentStreamPos = 0; //the current position in the FSWFStream for writing

	std::list<LargeData>::iterator it = outDataList.begin(), it_e = outDataList.end();
	U32 size = 0;

	for (; it != it_e; it++) {

		FLASHASSERT((*it).position >= currentStreamPos);

		if ((*it).position - currentStreamPos > 0)
			size += (*it).position - currentStreamPos;

		currentStreamPos = (*it).position;

		//currentStreamPos should now equal currentDataPosition
		FLASHASSERT((*it).size > 0);

		size += (*it).size;
	}

	if (streamPos > currentStreamPos)
		size += streamPos - currentStreamPos;
	return size;
}

//-------------------------------------------------------------------
//	Appends the stream FSWFStream to this.  Doesn't actually write the bitmaps,
//  jpegs ...  Instead it just writes their file name with a note that the actual file
//  should go there.

void FSWFStream::Append(FSWFStream *add)
{
	int addStreamPos = 0; // this functions position in the "add" stream,
						  // remembering that add->streamPos is the END
						  // of the "add" stream.

	// remove all the large data from the other stream
	while (add->outDataList.size()) {
		LargeData data = add->outDataList.front();
		add->outDataList.pop_front();

		for (; addStreamPos < (int)data.position; addStreamPos++) {
			WriteBits(add->stream[addStreamPos], 8);
		}
		//addStreamPos should now equal data.position
		WriteLargeData(data.data, data.size);
	}

	// Write the remainder of the stream data, after the last outData.
	for (; addStreamPos < (int)add->streamPos; addStreamPos++) {
		WriteBits(add->stream[addStreamPos], 8);
	}
}

//	Writes the stream FSWFStream to the given file.
//---------------------------------------------------------------------

void FSWFStream::WriteToFileVersion6(FILE *swfFile)
{
	U32 size = FullSize();
	U8 *buf = new U8[size];
	WriteToMemory(buf);

	assert(buf[0] == 'F');
	assert(buf[1] == 'W');
	assert(buf[2] == 'S');
	//assert(buf[3]==4);
	assert(((U32)(buf[4] + (buf[5] << 8) + (buf[6] << 16) + (buf[7] << 24))) == size);

	buf[3] = 6; //version 6

	U32 sizeOut = (U32)(2 * ((size * 1.1) + 12));
	U8 *bufOut = new U8[sizeOut];

	compress(bufOut, (uLongf *)&sizeOut, buf + 8, size - 8);

	if (size > sizeOut + 8) //meglio non comprimere altrimenti!!!
	{
		buf[0] = 'C';
		fwrite(buf, 1, 8, swfFile);
		fwrite(bufOut, 1, sizeOut, swfFile);
	} else
		fwrite(buf, 1, size, swfFile);

	delete[] bufOut;
	delete[] buf;
}

//-------------------------------------------------------------------------------------

void FSWFStream::WriteToFile(FILE *swfFile)
{
	U32 currentStreamPos = 0; //the current position in the FSWFStream for writing
	const U8 *currentData;
	U32 currentDataSize;
	U32 currentDataPosition;
	U32 outDataListSize = outDataList.size();

	int wrote = 0;

	if (outDataListSize) {

		for (U32 i = 0; i < outDataListSize; i++) {
			currentDataPosition = outDataList.front().position;
			currentData = outDataList.front().data;
			currentDataSize = outDataList.front().size;
			outDataList.pop_front();

			FLASHASSERT(currentDataPosition >= currentStreamPos);

			if (currentDataPosition - currentStreamPos > 0) {
				fwrite(&stream[currentStreamPos], 1, (currentDataPosition - currentStreamPos), swfFile);
			}
			wrote += currentDataPosition - currentStreamPos;
			currentStreamPos = currentDataPosition;

			//currentStreamPos should now equal currentDataPosition
			FLASHASSERT(currentDataSize > 0);

			fwrite(currentData, 1, currentDataSize, swfFile);
			wrote += currentDataSize;
		}
	}

	if (streamPos > currentStreamPos) {
		fwrite(&stream[currentStreamPos], 1, streamPos - currentStreamPos, swfFile);
	}
	wrote += streamPos - currentStreamPos;
}

void FSWFStream::WriteToMemory(U8 *memory)
{
	U32 currentStreamPos = 0; //the current position in the FSWFStream for writing
	const U8 *currentData;
	U32 currentDataSize;
	U32 currentDataPosition;
	U32 outDataListSize = outDataList.size();

	int wrote = 0;

	if (outDataListSize) {

		for (U32 i = 0; i < outDataListSize; i++) {
			currentDataPosition = outDataList.front().position;
			currentData = outDataList.front().data;
			currentDataSize = outDataList.front().size;
			outDataList.pop_front();

			FLASHASSERT(currentDataPosition >= currentStreamPos);

			if (currentDataPosition - currentStreamPos > 0) {
				memcpy(memory, &stream[currentStreamPos], (currentDataPosition - currentStreamPos));
				memory += currentDataPosition - currentStreamPos;
			}
			wrote += currentDataPosition - currentStreamPos;
			currentStreamPos = currentDataPosition;

			//currentStreamPos should now equal currentDataPosition
			FLASHASSERT(currentDataSize > 0);

			memcpy(memory, currentData, currentDataSize);
			memory += currentDataSize;
			wrote += currentDataSize;
		}
	}

	if (streamPos > currentStreamPos) {
		// 		fwrite( &stream[currentStreamPos], 1, streamPos - currentStreamPos, swfFile );
		memcpy(memory, &stream[currentStreamPos], streamPos - currentStreamPos);
		memory += streamPos - currentStreamPos;
	}
	wrote += streamPos - currentStreamPos;
}

void FSWFStream::AppendTag(U16 tagID, U32 length, FSWFStream *buffer)
{
	U32 longLength = 0;
	bool longHead = false;

	if (length > 62 || (tagID == 32 && length == 29) || (tagID == 2 && length == 28) || (tagID == 26 && length == 19)) //If long type tag:
	{
		longHead = true;
		longLength = length; //The actual length is here.
		length = 0x3f;		 //This field's length becomes 63 to indicate a TLong tag.
	} else {				 //Else short type tag:
		longHead = false;	//It is not a TLong header, so the length is valid.
	}

	U16 firstPartOfTag = (U16)((tagID << 6) | length); // Build up the first 2 bytes of the tag:
													   //		10bits for tag ID
													   //		6bits for tag length

	WriteWord((U32)firstPartOfTag);

	if (longHead) {
		WriteDWord(longLength);
	}

	if (buffer) // If there is not a buffer, don't write any more.
	{
		Append(buffer); // Copy the buffer passed in to this object.
	}
}

//  Calculates the minimum number of bits necessary to represent the given number.  The
//	number should be given in its unsigned form with the flag sign equal to 1 if it is
//	signed.  Repeatedly compares number to another unsigned int called x.
//	x is initialized to 1.  The value of x is shifted left i times until x is greater
//	than number.  Now i is equal to the number of bits the UNSIGNED value of number needs.
//	The signed value will need one more bit for the sign so i+1 is returned if the number
//	is signed, and i is returned if the number is unsigned.

U32 FSWFStream::MinBits(U32 number, U16 sign)
{
	//If the number == 0, then 0 bits are necessary for unsigned, and 1 for signed.
	//Sign should either have a value of 0 or 1.
	if (number == 0) {
		return sign;
	}

	//declare and initialize the variable for comparison
	U32 x = 1;
	U32 i;

	//keep increasing the value of x and i until s is greater than the given number
	for (i = 1; i < 33; i++) {
		x <<= 1;
		if (x > number) {
			break;
		}
	}

	FLASHASSERT(sign + i <= 32);

	//return the calculated value and account for the number being signed or not
	return i + sign;
}

//  Compares the absolute values of 4 signed integers and returns the unsigned magnitude of
//  the number with the greatest absolute value.

U32 FSWFStream::MaxNum(S32 a, S32 b, S32 c, S32 d)
{

	//take the absolute values of the given numbers
	int aAbs = abs(a);
	int bAbs = abs(b);
	int cAbs = abs(c);
	int dAbs = abs(d);

	//compare the numbers and return the unsigned value of the one with the greatest magnitude
	if (aAbs > bAbs) {
		if (aAbs > cAbs) {
			if (aAbs > dAbs) {
				return (U32)aAbs;
			} else {
				return (U32)dAbs;
			}
		} else if (cAbs > dAbs) {
			return (U32)cAbs;
		} else {
			return (U32)dAbs;
		}
	} else {
		if (bAbs > cAbs) {
			if (bAbs > dAbs) {
				return (U32)bAbs;
			} else {
				return (U32)dAbs;
			}
		} else if (cAbs > dAbs) {
			return (U32)cAbs;
		} else {
			return (U32)dAbs;
		}
	}
}
