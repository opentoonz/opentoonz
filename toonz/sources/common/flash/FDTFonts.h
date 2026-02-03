// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTFonts.h

  	This header-file contains the declarations of all low-level font-related classes. 
	Their parent classes are in the parentheses:

		class FKerningRec;
		class FDTDefineFont; (public FDT)
		class FDTDefineFont2; (public FDT)
		class FDTDefineFontInfo; (public FDT)
//		class FGlyphEntry;

  	Note: Class FGlyphEntry has been commented out. Need to fix.

****************************************************************************************/

#ifndef _FDT_FONTS_H_
#define _FDT_FONTS_H_

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"
#include "FDT.h"
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

class FShape;

//	A kerning record

class FKerningRec
{

public:
	FKerningRec(U16 _code1, U16 _code2, S16 _kerningAdjust);
	void CodesWide(U16 _flag);
	virtual void WriteToSWFStream(FSWFStream *b);

	virtual ~FKerningRec() {}

private:
	U16 wideCodesFlag;
	U16 code1;
	U16 code2;
	S16 kerningAdjust;
};

// A flash object that defines a font's appearance

class DVAPI FDTDefineFont : public FDT
{

public:
	FDTDefineFont(void);
	virtual ~FDTDefineFont();

	U16 ID(void);
	void AddShapeGlyph(FShape *_shape);
	int NumberOfGlyphs() { return shapeGlyphs.size(); }

	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 characterID;
	std::list<FShape *> shapeGlyphs;
	U32 nFillBits;
	U32 nLineBits;
};

// A flash object that defines a font's appearance (flash 3.0)

class DVAPI FDTDefineFont2 : public FDT
{
public:
	FDTDefineFont2(const char *_fontName,
				   U16 _encodeType, //  ShiftJIS, Unicode, ANSI
				   U16 _italicFlag,
				   U16 _boldFlag);

	FDTDefineFont2(const char *_fontName,
				   U16 _encodeType,
				   U16 _italicFlag,
				   U16 _boldFlag,
				   S16 _ascenderHeight,
				   S16 _descenderHeight,
				   S16 _leadingHeight);
	virtual ~FDTDefineFont2();

	void AddShapeGlyph(FShape *_shape, U16 _shapeCode, S16 _shapeAdvance = 0,
					   FRect *_shapeBounds = 0);
	void AddKerningRec(FKerningRec *_kerningRecord);
	U16 nIndexBits();
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 fontID;
	int hasLayoutFlag;
	U16 encodeType;
	int italicFlag;
	int boldFlag;

	FString *fontName;
	struct Glyph {
		FShape *shape;
		U16 code;
		S16 advance;
		FRect *bounds;
	};
	S16 ascenderHeight;
	S16 descenderHeight;
	S16 leadingHeight;

	std::list<Glyph> glyphs;

	std::list<FKerningRec *> kerningTable;
	U32 nFillBits;
	U32 nLineBits;
};

// A flash object that defines the mapping from a flash font object to a TrueType or ATM font so that a player can optionally use them

class DVAPI FDTDefineFontInfo : public FDT
{

public:
	FDTDefineFontInfo(const char *_fontName,
					  U16 _fontID,
					  U16 _encodeType,
					  U16 _italicFlag,
					  U16 _boldFlag);
	virtual ~FDTDefineFontInfo();
	void AddCode(U16 _someCode);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 characterID;
	FString *fontName;
	U16 encodeType;
	U16 italicFlag;
	U16 boldFlag;
	std::list<U16> codeTable;
	U16 fontID;
};

//	Found in DefineText.  Used to describe the glyph index and X advance value to use for a
//	particular character in the currently selected font for the text record.

// class FGlyphEntry {
//
// public:
//
// 	FGlyphEntry (U16 index, S16 advance);
// 	S16 AdvanceValue();
// 	void IncludeNBitInfo(U16 _nIndexBits, U16 _nAdvanceBits);
// 	void WriteToSWFStream(FSWFStream *_SWFStream);
//
//
// private:
//
// 	U16 glyphIndex;
// 	S16 glyphAdvance;
// 	U16 nIndexBits;
// 	U16 nAdvanceBits;
//
// };

#endif
