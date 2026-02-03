// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FPrimitive.cpp

  This source file contains the definition for all low-level primitive-related functions,
  grouped by classes:

		Class						Member Function
  
	FColor						FColor(U32, U32, U32);
								FColor(U32, U32, U32, U32);
								FColor(FRGB);
								FColor(FRGBA);
								void WriteToSWFStream(FSWFStream*, int);

	FMatrix						FMatrix(U32, SFIXED, SFIXED,
										U32, SFIXED, SFIXED,
										SCOORD , SCOORD);
								Matrix();
								U32 MinBits(S32, S32);
								void WriteToSWFStream(FSWFStream*);

	FRect						FRect(SCOORD, SCOORD, SCOORD, SCOORD);
								TUINT32 MinBits();
								void WriteToSWFStream(FSWFStream *);

	FString						FString(const U8*);
								void WriteToSWFStream(FSWFStream*, bool);

****************************************************************************************/

#include "FPrimitive.h"
#include "FSWFStream.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FColor  ----------------------------------------------------------------

FColor::FColor(U32 _red, U32 _green, U32 _blue)
	: red(_red),
	  green(_green),
	  blue(_blue),
	  alpha(0xff),
	  alphaT(false)
{
}

FColor::FColor(U32 _red, U32 _green, U32 _blue, U32 _alpha)
	: red(_red),
	  green(_green),
	  blue(_blue),
	  alpha(_alpha),
	  alphaT(true)
{
}

FColor::FColor(FRGB rgb)
	: red(rgb.red),
	  green(rgb.green),
	  blue(rgb.blue),
	  alpha(0xff),
	  alphaT(false)
{
}

FColor::FColor(FRGBA rgba)
	: red(rgba.red),
	  green(rgba.green),
	  blue(rgba.blue),
	  alpha(rgba.alpha),
	  alphaT(true)
{
}

void FColor::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//no filling, everything should already be byte aligned
	_SWFStream->WriteByte(red);
	_SWFStream->WriteByte(green);
	_SWFStream->WriteByte(blue);
	if (alphaT) {
		_SWFStream->WriteByte(alpha);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMatrix  ---------------------------------------------------------------

//	The Matrix class constructor
FMatrix::FMatrix(U32 _hasScale, SFIXED _scaleX, SFIXED _scaleY, U32 _hasRotate, SFIXED
																					_rotateSkew0,
				 SFIXED _rotateSkew1, SCOORD _translateX, SCOORD _translateY)
{

	hasScale = _hasScale;

	scaleX = _scaleX;
	scaleY = _scaleY;

	hasRotate = _hasRotate;
	rotateSkew0 = _rotateSkew0;
	rotateSkew1 = _rotateSkew1;

	translateX = _translateX;
	translateY = _translateY;

	nScaleBits = MinBits(scaleX, scaleY);
	nRotateBits = MinBits(rotateSkew0, rotateSkew1);
	nTranslateBits = MinBits(translateX, translateY);

	identity = false;
}

//	Creates an identity matrix.

FMatrix::FMatrix(void)
{

	identity = true;
	hasScale = false;
	scaleX = 1;
	scaleY = 1;

	hasRotate = false;
	rotateSkew0 = 0;
	rotateSkew1 = 0;
}

//	Calculates the minimum number of bits necessary to represent the given 2 signed numbers.
//	This is used to calculate the 3 nbit fields in the Matrix class.  Takes two signed
//	numbers, sees which has the greatest magnitude, and calls FileWrite::MinBits with the
//	unsigned magnitude of the larger number and the sign flag equal to 1 to account for the
//	fact that the numbers are signed.

U32 FMatrix::MinBits(S32 x, S32 y)
{

	int xAbs = abs(x);
	int yAbs = abs(y);

	if (xAbs > yAbs)
		return FSWFStream::MinBits((U32)xAbs, 1);

	else
		return FSWFStream::MinBits((U32)yAbs, 1);
}

FMatrix FMatrix::operator*(const FMatrix &b) const
{
	SFIXED _scaleX = scaleX * b.scaleX + rotateSkew0 * b.rotateSkew1;
	SFIXED _scaleY = scaleY * b.scaleY + rotateSkew0 * b.rotateSkew1;
	SFIXED _rot0 = scaleX * b.rotateSkew0 + rotateSkew0 * b.scaleY;
	SFIXED _rot1 = rotateSkew1 * b.scaleX + scaleY * b.rotateSkew1;

	return FMatrix(
		_scaleX != 1 || scaleY != 1, _scaleX, _scaleY,
		_rot0 != 0 || _rot1 != 0, _rot0, _rot1,
		scaleX * b.translateX + rotateSkew0 * b.translateY + translateX,
		rotateSkew1 * b.translateX + scaleY * b.translateY + translateY);
}

//	Writes the Matrix to the given _SWFStream.

void FMatrix::WriteToSWFStream(FSWFStream *_SWFStream)
{

	if (identity) {

		_SWFStream->WriteByte(0);

	}

	else {
		_SWFStream->FlushBits();

		_SWFStream->WriteBits(hasScale, 1);

		if (hasScale) {

			_SWFStream->WriteBits(nScaleBits, 5);
			_SWFStream->WriteBits((U32)scaleX, nScaleBits);
			_SWFStream->WriteBits((U32)scaleY, nScaleBits);
		}

		_SWFStream->WriteBits(hasRotate, 1);

		if (hasRotate) {

			_SWFStream->WriteBits(nRotateBits, 5);
			_SWFStream->WriteBits((U32)rotateSkew0, nRotateBits);
			_SWFStream->WriteBits((U32)rotateSkew1, nRotateBits);
		}
		if (translateX == 0 && translateY == 0)
			_SWFStream->WriteBits(0, 5);
		else {
			_SWFStream->WriteBits(nTranslateBits, 5);

			_SWFStream->WriteBits((U32)translateX, nTranslateBits);
			_SWFStream->WriteBits((U32)translateY, nTranslateBits);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FRect ------------------------------------------------------------------

//	Rectangle class constructor.
FRect::FRect(SCOORD _xmin, SCOORD _ymin, SCOORD _xmax, SCOORD _ymax)
{

	xmin = _xmin;
	xmax = _xmax;
	ymin = _ymin;
	ymax = _ymax;
}

//	FRect's MinBits function.  Calls MinBits with number being the absolute value of the
//  rectangle coordinate with the greatest magnitude, and sign being 1 since a rectangle
//	coord is signed.

TUINT32 FRect::MinBits(void)
{
	TUINT32 maxCoord = FSWFStream::MaxNum(xmin, xmax, ymin, ymax);

	return FSWFStream::MinBits(maxCoord, 1);
}

//	Writes the rectangle to the given buffer.

void FRect::WriteToSWFStream(FSWFStream *_SWFStream)
{
	int nBits = (int)MinBits();
	_SWFStream->WriteBits(nBits, 5);

	_SWFStream->WriteBits((U32)xmin, nBits);
	_SWFStream->WriteBits((U32)xmax, nBits);
	_SWFStream->WriteBits((U32)ymin, nBits);
	_SWFStream->WriteBits((U32)ymax, nBits);

	_SWFStream->FlushBits();
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FString  ---------------------------------------------------------------

FString::FString(const U8 *_string)
{
	text = (const char *)_string;
}

FString::FString(const char *_string)
{
	text = _string;
}

void FString::WriteToSWFStream(FSWFStream *_SWFStream, bool null_end)
{
	U16 i = 0;
	for (i = 0; i < text.length(); i++) {
		_SWFStream->WriteByte((U32)text[i]);
	}
	if (null_end)
		_SWFStream->WriteByte(0);
}
