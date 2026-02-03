// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTFonts.cpp

  This source file contains the definition for all low-level font-related functions,
  grouped by classes:

  		Class						Member Function

	FDTDefineFont				FDTDefineFont();
								~FDTDefineFont();
								U16 ID();
								void AddShapeGlyph(FShape*);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineFont2				FDTDefineFont2(char*, U16, U16, U16);
								FDTDefineFont2(char*, U16, U16, U16, S16, S16, S16);
								~FDTDefineFont2();
								void AddShapeGlyph(FShape*, U16, S16, FRect*);
								void AddKerningRec(FKerningRec*);
								U16 nIndexBits();
								U16 ID(void);
								void WriteToSWFStream(FSWFStream*);

	FDTDefineFontInfo			FDTDefineFontInfo(const char*, U16, U16, U16, U16);
								void FDTDefineFontInfo::AddCode(U16);
								U16 FDTDefineFontInfo::ID();
								void WriteToSWFStream(FSWFStream*);

//	FGlyphEntry					FGlyphEntry(U16, S16);
//								S16 AdvanceValue();
//								void IncludeNBitInfo(U16, U16);
//								void WriteToSWFStream(FSWFStream*);

	FKerningRec					FKerningRec (U16, U16, short);
								void CodesWide (U16);
								void WriteToSWFStream(FSWFStream*);

	
	Note: All member functions of FGlyphEntry have been commented out. Need to fix.

****************************************************************************************/
#ifdef WIN32
#pragma warning(disable : 4786)
#endif

#include "FDTFonts.h"
#include "FDTShapes.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FButtonRecord1 ---------------------------------------------------------

FDTDefineFont::FDTDefineFont()
{

	characterID = FObjCollection::Increment();

	nFillBits = 1;
	nLineBits = 0;
}

FDTDefineFont::~FDTDefineFont()
{

	while (!shapeGlyphs.empty()) {

		delete shapeGlyphs.front();
		shapeGlyphs.pop_front();
	}
}

U16 FDTDefineFont::ID()
{

	return (U8)characterID;
}

void FDTDefineFont::AddShapeGlyph(FShape *_shape)
{

	_shape->SetFillBits(nFillBits);
	_shape->SetLineBits(nLineBits);
	shapeGlyphs.push_back(_shape);
}

void FDTDefineFont::WriteToSWFStream(FSWFStream *_SWFStream)
{

	U32 offsetsBufferSize = shapeGlyphs.size() * 2;

	FSWFStream body;
	FSWFStream shapeBuffer;
	std::list<U32> offsetsList;

	// get values for offsets and place them in a list
	// write list of shapeGlyphs to shape buffer
	offsetsList.push_back(offsetsBufferSize);

	std::list<FShape *>::iterator cursor;
	std::list<FShape *>::iterator nextToLast = shapeGlyphs.end();
	nextToLast--;
	for (cursor = shapeGlyphs.begin(); cursor != nextToLast; cursor++) {

		(*cursor)->WriteToSWFStream(&shapeBuffer);
		offsetsList.push_back(offsetsBufferSize + shapeBuffer.Size());
	}

	(*cursor)->WriteToSWFStream(&shapeBuffer);

	body.WriteWord((U32)characterID);

	// write offsetsList to body
	while (!offsetsList.empty()) {

		body.WriteWord((U32)offsetsList.front());
		offsetsList.pop_front();
	}

	//write shape buffer to body
	body.Append(&shapeBuffer);

	//put tag on body and write to stream
	_SWFStream->AppendTag(stagDefineFont, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineFont2 ---------------------------------------------------------

FDTDefineFont2::FDTDefineFont2(const char *_fontName, U16 _encodeType, U16 _italicFlag, U16 _boldFlag)
{
	fontID = FObjCollection::Increment();
	hasLayoutFlag = 1;
	encodeType = _encodeType;
	italicFlag = _italicFlag;
	boldFlag = _boldFlag;
	fontName = new FString((U8 *)_fontName);
	nFillBits = 1;
	nLineBits = 0;
	ascenderHeight = 0;
	descenderHeight = 0;
	leadingHeight = 0;
}

FDTDefineFont2::FDTDefineFont2(const char *_fontName, U16 _encodeType, U16 _italicFlag,
							   U16 _boldFlag, S16 _ascenderHeight,
							   S16 _descenderHeight, S16 _leadingHeight)
{

	fontID = FObjCollection::Increment();
	hasLayoutFlag = 1;
	encodeType = _encodeType;
	italicFlag = _italicFlag;
	boldFlag = _boldFlag;
	fontName = new FString((U8 *)_fontName);

	ascenderHeight = _ascenderHeight;
	descenderHeight = _descenderHeight;
	leadingHeight = _leadingHeight;

	nFillBits = 1;
	nLineBits = 0;
}

FDTDefineFont2::~FDTDefineFont2()
{

	delete fontName;

	while (!glyphs.empty()) {
		delete glyphs.front().shape;
		delete glyphs.front().bounds;
		glyphs.pop_front();
	}

	while (!kerningTable.empty()) {

		delete kerningTable.front();
		kerningTable.pop_front();
	}
}

void FDTDefineFont2::AddShapeGlyph(FShape *_shape, U16 _shapeCode, S16 _shapeAdvance,
								   FRect *_shapeBounds)
{
	// FIXME, don't know what nFillBits and nLineBits are
	_shape->SetFillBits(nFillBits);
	_shape->SetLineBits(nLineBits);
	Glyph g;
	g.advance = _shapeAdvance;
	g.bounds = _shapeBounds;
	g.code = _shapeCode;
	g.shape = _shape;
	glyphs.push_back(g);
}

void FDTDefineFont2::AddKerningRec(FKerningRec *_kerningRecord)
{

	if (hasLayoutFlag)
		kerningTable.push_back(_kerningRecord);
}

U16 FDTDefineFont2::nIndexBits()
{

	return (U16)FSWFStream::MinBits(glyphs.size() - 1, false);
}

U16 FDTDefineFont2::ID(void)
{

	return fontID;
}

void FDTDefineFont2::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//	U32 offsetsSize		= (glyphs.size() * 2);
	//	U32 offsetsSizeWide = glyphs.size() * 4;
	//	U16 wideOffsetsFlag = 0;
	//	U16 wideCodesFlag	= 0;
	//	std::list<U32> offsetsList;

	// FIXME: add wide offset later
	S16 *offsetTable;
	if (glyphs.size() > 0)
		offsetTable = new S16[glyphs.size()];
	else
		offsetTable = 0;

	int i;

	FSWFStream body;
	FSWFStream shapeBuffer;

	std::list<Glyph>::iterator glyphCursor;
	for (glyphCursor = glyphs.begin(), i = 0; glyphCursor != glyphs.end(); glyphCursor++, i++) {
		offsetTable[i] = (S8)shapeBuffer.Size();
		glyphCursor->shape->WriteToSWFStream(&shapeBuffer);
	}

	body.WriteWord((U32)fontID);
	//write flags to body
	body.WriteBits((U32)hasLayoutFlag, 1);

	switch (encodeType) {

	case ShiftJIS:
		body.WriteBits((U32)1, 1);
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)0, 1);
		break;
	case Unicode:
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)1, 1);
		body.WriteBits((U32)0, 1);
		break;
	case ANSI:
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)1, 1);
		break;
	}
	body.WriteBits((U32)0, 1); // 0 for narrowOffsetFlag
	body.WriteBits((U32)0, 1); // 0 for narrowOffsetCode
	body.WriteBits((U32)italicFlag, 1);
	body.WriteBits((U32)boldFlag, 1);
	body.WriteByte(0); // FontFlagsReserved UB[8]

	body.WriteByte((U32)fontName->Length());
	fontName->WriteToSWFStream(&body, false); // write the name

	body.WriteWord((U32)glyphs.size());

	// write offsetsList to body
	if (glyphs.size() > 0)
		body.WriteLargeData((U8 *)offsetTable, 2 * glyphs.size());

	//write shape glyph buffer to body
	body.Append(&shapeBuffer);

	for (glyphCursor = glyphs.begin(), i = 0; glyphCursor != glyphs.end(); glyphCursor++, i++) {
		body.WriteWord(glyphCursor->code);
	}

	// write layout information to body
	if (hasLayoutFlag) {
		body.WriteWord((U32)ascenderHeight);
		body.WriteWord((U32)descenderHeight);
		body.WriteWord((U32)leadingHeight);

		for (glyphCursor = glyphs.begin(), i = 0; glyphCursor != glyphs.end(); glyphCursor++, i++) {
			body.WriteWord(glyphCursor->advance);
		}

		for (glyphCursor = glyphs.begin(), i = 0; glyphCursor != glyphs.end(); glyphCursor++, i++) {
			glyphCursor->bounds->WriteToSWFStream(&body);
		}

		body.WriteWord((U32)kerningTable.size());

		std::list<FKerningRec *>::iterator kernCursor;

		for (kernCursor = kerningTable.begin(); kernCursor != kerningTable.end(); kernCursor++) {

			(*kernCursor)->CodesWide(0); // 0 is narraw offset flag
			(*kernCursor)->WriteToSWFStream(&body);
		}
	}

	// create entire tag with record header
	_SWFStream->AppendTag(stagDefineFont2, body.Size(), &body);
	delete[] offsetTable;
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FButtonRecord1 ---------------------------------------------------------

FDTDefineFontInfo::FDTDefineFontInfo(const char *_fontName, U16 _fontID,
									 U16 _encodeType, U16 _italicFlag,
									 U16 _boldFlag)
{

	characterID = FObjCollection::Increment();
	encodeType = _encodeType;
	italicFlag = _italicFlag;
	boldFlag = _boldFlag;

	fontID = _fontID;

	fontName = new FString((U8 *)_fontName);
}

FDTDefineFontInfo::~FDTDefineFontInfo()
{
	delete fontName;
}

void FDTDefineFontInfo::AddCode(U16 _someCode)
{

	codeTable.push_back(_someCode);
}

U16 FDTDefineFontInfo::ID()
{
	return (U8)characterID;
}

void FDTDefineFontInfo::WriteToSWFStream(FSWFStream *_SWFStream)
{

	U16 wideCodesFlag = 0;
	FSWFStream body;

	//determine whether 8 or 16 bit code fields are needed
	std::list<U16>::iterator cursor;
	for (cursor = codeTable.begin(); (cursor != codeTable.end()) && (wideCodesFlag == 0) // fixed from DV
		 ;
		 cursor++) {
		if ((*cursor) > 65530)
			wideCodesFlag = 1;
	}

	body.WriteWord((U32)fontID);

	body.WriteByte((U32)fontName->Length());

	fontName->WriteToSWFStream(&body, false);

	// 	body.WriteBits((U32) reservedFlags, 2);
	body.WriteBits((U32)0, 2);

	switch (encodeType) {

	case ShiftJIS:
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)1, 1);
		body.WriteBits((U32)0, 1);
		break;
	case Unicode:
		body.WriteBits((U32)1, 1);
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)0, 1);
		break;
	case ANSI:
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)0, 1);
		body.WriteBits((U32)1, 1);
		break;
	}

	body.WriteBits((U32)italicFlag, 1);
	body.WriteBits((U32)boldFlag, 1);

	body.WriteBits((U32)wideCodesFlag, 1);

	//write code table to body
	while (!codeTable.empty()) {
		if (wideCodesFlag) {
			body.WriteWord((U32)codeTable.front());
			codeTable.pop_front();
		} else {
			body.WriteByte((U32)codeTable.front());
			codeTable.pop_front();
		}
	}

	// create entire tag with record header
	_SWFStream->AppendTag(stagDefineFontInfo, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FGlyphEntry ------------------------------------------------------------

// FGlyphEntry::FGlyphEntry(U16 index, S16 advance)
// {
//
// 	glyphIndex = index;
// 	glyphAdvance = advance;
//
// }
//
//
// S16 FGlyphEntry::AdvanceValue()
// {
//
// 	return glyphAdvance;
//
// }

//	Used to specify the nBit info for the entries.  This is determined and passed to
//	the glyph entry just before write time.

// void FGlyphEntry::IncludeNBitInfo(U16 _nIndexBits, U16 _nAdvanceBits)
// {
//
// 	nIndexBits = _nIndexBits;
// 	nAdvanceBits = _nAdvanceBits;
// }
//
// void FGlyphEntry::WriteToSWFStream(FSWFStream *_SWFStream)
// {
//
// 	_SWFStream->WriteBits((U32) glyphIndex, nIndexBits);
// 	_SWFStream->WriteBits((U32) glyphAdvance, nAdvanceBits);
//
// }

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FKerningRec ------------------------------------------------------------

FKerningRec::FKerningRec(U16 cd1, U16 cd2, short krnAdj)
{

	wideCodesFlag = 0; // default not wide
	code1 = cd1;
	code2 = cd2;
	kerningAdjust = krnAdj;
}

void FKerningRec::CodesWide(U16 _flag)
{

	if (_flag)
		wideCodesFlag = 1;
	else
		wideCodesFlag = 0;
}

void FKerningRec::WriteToSWFStream(FSWFStream *b)
{
	if (wideCodesFlag) {
		b->WriteWord((U32)code1);
		b->WriteWord((U32)code2);
	} else {
		b->WriteByte((U32)code1);
		b->WriteByte((U32)code2);
	}

	b->WriteWord((U32)kerningAdjust);
}
