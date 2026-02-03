// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/18/1999.
// Mostly about FRGB, FRGBA, and FColor.
// Additionally, there is some brain-dump on how color should be implemented in low and
// high level manager.

/****************************************************************************************

				File Summary: FPrmitive.h

	This header-file contains the declarations of low-level prmitive-related structs and
	classes.
	
		struct FRGB;
		struct FRGBA;

		class FColor;
		class FMatrix;
		class FRect;
		class FString;

****************************************************************************************/

#ifndef FPRIMITIVE_INCLUDED
#define FPRIMITIVE_INCLUDED
#include "tcommon.h"
#include "Macromedia.h"
#include <string>
class FSWFStream;

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#undef DVAPI
#undef DVVAR
#ifdef TFLASH_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// color structures

struct FRGB {
	U8 red;
	U8 green;
	U8 blue;
};

struct FRGBA {
	// 	FRGBA( U8 r, U8 g, U8 b, U8 a )		{ red = r; green = g; blue = b; alpha = a; }
	// 	FRGBA()								{ red = 0; green = 0; blue = 0; alpha = 255; }
	// 	void Set( U8 r, U8 g, U8 b, U8 a )	{ red = r; green = g; blue = b; alpha = a; }
	//
	U8 red;
	U8 green;
	U8 blue;
	U8 alpha;
};

//! This object specifies a color object.
/*! FColor is used (copied) as a structure. Do not add dynamic memory or virtual functions.

	There are two types of FColor, i.e. one with alpha channel on and the other with alpha channel.off.
	When you construct an instance of FColor, if you provide three parameters, the alpha channel is off;
	if you provide four, the alpha channel is on.

	Each color-related SWF tag requires the correct color type, either RGB or RGBA. For example,
	DefineShape, DefineShape2, SetBackgroundColor need RGB (alpha off);
	DefineShape3, DefineMorphShape, DefineEditText need RGBA (alpha on).

	The low-level manager can correct some errors of wrong color type, but not all. So don't rely on it.
	If the wrong one got over low-level manager, the flash play might crash on that.

	We recommend you use RGBA whenever reasonable. The reason? Using RGBA is very consistent
	and worth the sacrifice of some file space (in SWF, it's one more byte for each occurance of color info, and some
	additional for color transformation.)

	Actually, we only provide RGBA interface for colors in high-level manager because this solution is much more elegant
	in terms of consistency and compatibility.
*/
class DVAPI FColor
{
public:
	//! Default FColor is white (0xff, 0xff, 0xff), construct a FColor with Alpha channel off.
	FColor(U32 _red = 0xff, U32 _green = 0xff, U32 _blue = 0xff);

	//! Construct a FColor with Alpha channel on.
	FColor(U32 _red, U32 _green, U32 _blue, U32 _alpha);

	//! Construct a FColor with Alpha channel off.
	FColor(FRGB rgb);

	//! Construct a FColor with Alpha channel on.
	FColor(FRGBA rgba);

	//! Returns red component.
	int Red() { return (int)red; }

	//! Returns green component.
	int Green() { return (int)green; }

	//! Returns blue component.
	int Blue() { return (int)blue; }

	//! Returns alpha value.
	int Alpha() { return (int)alpha; }

	bool HasAlpha() { return alphaT; }

	//! Alpha switch: turn on/off alpha channel.
	void AlphaChannel(bool on) { alphaT = on; }

	bool operator==(const FColor &color) { return (red == color.red &&
												   green == color.green &&
												   blue == color.blue &&
												   alpha == color.alpha); }

	/*
	enum {
		WRITE_SMALLEST,
		WRITE_3BYTE,
		WRITE_4BYTE
	};
*/
	// Method for internal use.
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 red;	 // should be U8
	U32 green;   // should be U8
	U32 blue;	// should be U8
	U32 alpha;   // should be U8
	bool alphaT; //alpha type flag
};

class DVAPI FMatrix
{

public:
	FMatrix(void);

	FMatrix(U32 _hasScale,
			SFIXED _scaleX,
			SFIXED _scaleY,
			U32 _hasRotate,
			SFIXED _rotateSkew0,
			SFIXED _rotateSkew1,
			SCOORD _translateX,
			SCOORD _translateY);

	void WriteToSWFStream(FSWFStream *_SWFStream);

	FMatrix operator*(const FMatrix &b) const;

public:
	U32 hasScale;
	U32 nScaleBits;

	SFIXED scaleX;
	SFIXED scaleY;

	U32 hasRotate;
	U32 nRotateBits;
	SFIXED rotateSkew0;
	SFIXED rotateSkew1;

	U32 nTranslateBits;
	SCOORD translateX;
	SCOORD translateY;

	U32 MinBits(S32 x, S32 y);
	U16 identity;
};

//rectangle class

class DVAPI FRect
{
public:
	FRect() { xmin = ymin = xmax = ymax = 0; }
	FRect(SCOORD xmin, SCOORD ymin, SCOORD xmax, SCOORD ymax);

	SCOORD Xmin() { return xmin; }
	SCOORD Ymin() { return ymin; }
	SCOORD Xmax() { return xmax; }
	SCOORD Ymax() { return ymax; }
	SCOORD Width() { return xmax - xmin + 1; }
	SCOORD Height() { return ymax - ymin + 1; }

	void SetXmin(SCOORD val) { xmin = val; }
	void SetYmin(SCOORD val) { ymin = val; }
	void SetXmax(SCOORD val) { xmax = val; }
	void SetYmax(SCOORD val) { ymax = val; }

	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	SCOORD xmin;
	SCOORD xmax;
	SCOORD ymin;
	SCOORD ymax;

	U32 MinBits(void);
};

// a flash string

class DVAPI FString
{

public:
	FString(const U8 *_string);
	FString(const char *_string);
	std::string GetString() { return text; }

	void WriteToSWFStream(FSWFStream *_SWFStream, bool writeNull);
	U16 Length() { return text.length(); }

private:
	std::string text;
};

#ifdef WIN32 // added from DV
#pragma warning(pop)
#endif
#endif
