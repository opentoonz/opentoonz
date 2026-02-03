// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTText.h

	This header-file contains the declarations of low-level text-related classes.
	All parent classes are in the parentheses:

		class FTextRecord;
		class FDTDefineEditText; (public FDT)
		class FDTDefineText; (public FDT)
		class FDTDefineText2; (public FDT)
		class FTextChangeRecord; (public FTextRecord)
		class FTextGlyphRecord; (public FTextRecord)

****************************************************************************************/

#ifndef _F_DEFINE_TEXT_H_
#define _F_DEFINE_TEXT_H_

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"
#include "Macromedia.h"
#include "FDT.h"
#include "FSWFStream.h"
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

class FGlyphEntry;

class DVAPI FTextRecord
{
public:
	virtual U32 MinAdvanceBits() = 0; // the computed minimum # of bits to record the advance values
	virtual U32 MinCodeBits() = 0;	// the computed minimum # of bits to record the character code values
	virtual ~FTextRecord() {}

	// Because of multiple text records, the DefineText will specify the code and advance bits
	// when it writes.
	virtual void WriteToSWFStream(FSWFStream *_SWFStream, int codeBits, int advanceBits) = 0;
};

// A flash object that defines a font's appearance

class DVAPI FDTDefineEditText : public FDT
{

public:
	FDTDefineEditText(FRect *_bounds, U8 _hasFont, U8 _hasMaxLength, U8 _hasTextColor,
					  U8 _readOnly, U8 _password, U8 _multiline, U8 _wordWrap, U8 _hasText,
					  U8 _useOutlines, U8 _border, U8 _noSelect, U8 _hasLayout, U16 _fontID,
					  U16 _fontHeight, FColor *_textColor, U16 _maxLength, U8 _alignment,
					  U16 _leftMargin, U16 _rightMargin, U16 _indent, U16 _leading,
					  FString *_variableName, FString *_initialText);
	virtual ~FDTDefineEditText(void);
	U16 ID(void) { return characterID; }
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FRect *bounds;
	U8 hasFont;
	U8 hasMaxLength;
	U8 hasTextColor;
	U8 readOnly;
	U8 password;
	U8 multiline;
	U8 wordWrap;
	U8 hasText;
	U8 useOutlines;
	U8 border;
	U8 noSelect;
	U8 hasLayout;
	U16 fontID;
	U16 fontHeight;
	FColor *textColor;
	U16 maxLength;
	U8 alignment;
	U16 leftMargin;
	U16 rightMargin;
	U16 indent;
	U16 leading;
	FString *variableName;
	FString *initialText;
	U16 characterID;
};

// A flash object that defines the font and formating of text characters in the record (flash 1.0)
// takes only RGB color values

class DVAPI FDTDefineText : public FDT
{

public:
	FDTDefineText(FRect *_textBounds,
				  FMatrix *_textMatrix,
				  int glyhpsInFont); // glyhpsInFont: how many characters are in the font?
	virtual ~FDTDefineText();
	void AddTextRecord(FTextRecord *_textRec);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 characterID;
	FRect *textBounds;
	FMatrix *textMatrix;
	std::list<FTextRecord *> textRecords;
	U16 nIndexBits;
};

// A flash object that defines the font and formating of text characters in the record (flash 1.0)
// takes RGBA color values

class DVAPI FDTDefineText2 : public FDT
{

public:
	FDTDefineText2(FRect *_textBounds, FMatrix *_textMatrix,
				   int glyhpsInFont);
	virtual ~FDTDefineText2();
	void AddTextRecord(FTextRecord *_textRec);
	U16 ID(void);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U32 characterID;
	FRect *textBounds;
	FMatrix *textMatrix;
	std::list<FTextRecord *> textRecords;
	U16 nIndexBits;
};

// specifies text format changes in a flash DefineText object

class DVAPI FTextChangeRecord : public FTextRecord
{

public:
	FTextChangeRecord(U16 _hasFontFlag, U16 _hasColorFlag,
					  U16 _hasXOffsetFlag, U16 _hasYOffsetFlag,
					  U16 _fontID, U16 _fontHeight, FColor *_fontColor,
					  S16 _xOffset, S16 _yOffset);
	virtual ~FTextChangeRecord();

	virtual U32 MinAdvanceBits() { return 0; }
	virtual U32 MinCodeBits() { return 0; }

	virtual void WriteToSWFStream(FSWFStream *_SWFStream, int advanceBits, int codeBits);

private:
	U16 reserved;
	U16 hasFontFlag;
	U16 hasColorFlag;
	U16 hasYOffsetFlag;
	U16 hasXOffsetFlag;
	U16 fontID;
	FColor *fontColor;
	S16 xOffset;
	S16 yOffset;
	U16 fontHeight;
};

class DVAPI FTextGlyphRecord : public FTextRecord
{

public:
	FTextGlyphRecord();
	virtual ~FTextGlyphRecord();

	void AddGlyphEntry(U16 code, U16 advance);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream, int advanceBits, int codeBits);

	virtual U32 MinAdvanceBits(); // number of bits needed to write the advance data
	virtual U32 MinCodeBits();	// number of bits needed to write the character code data

private:
	struct GlyphEntry {
		U16 code;
		U16 advance;
	};
	std::list<GlyphEntry> glyphEntries;
};

#endif
