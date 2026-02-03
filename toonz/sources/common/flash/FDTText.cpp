// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTText.cpp

  This source file contains the definition for all low-level text-related functions,
  grouped by classes:

		Class						Member Function

	FDTDefineEditText			FDTDefineEditText(FRect*, U8, U8, U8, U8, U8, U8, U8, U8,
									 U8, U8, U8, U8, U16, U16, FColor*, U16, U8, U16, U16,
									 U16, U16, FString*, FString*);
								~FDTDefineEditText();
								void WriteToSWFStream(FSWFStream*);
	
	FDTDefineText				FDTDefineText(FRect*, FMatrix*, int);
								~FDTDefineText();
								void AddTextRecord(FTextRecord*);
								U16 ID();
								void WriteToSWFStream( FSWFStream*);

	FDTDefineText2				FDTDefineText2(FRect*, FMatrix*, int);
								~FDTDefineText2();
								void AddTextRecord(FTextRecord*);
								U16 ID();
								void WriteToSWFStream(FSWFStream*);

	FTextChangeRecord			FTextChangeRecord(U16, U16, U16, U16, U16, U16, FColor*,
												  S16, S16);
								~FTextChangeRecord()

//	FTextChangeRecord			void IncludeNBitInfo(U16, U16);
								void WriteToSWFStream(FSWFStream* , int, int);

	FTextGlyphRecord			FTextGlyphRecord();
								~FTextGlyphRecord();
								void AddGlyphEntry(U16, U16);
								U32  MinAdvanceBits();
								U32  MinCodeBits();
								void WriteToSWFStream(FSWFStream*, int, int);


****************************************************************************************/

#include "FDTText.h"
#include "FDTFonts.h"

//	Constructor.

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineEditText  -----------------------------------------------------

FDTDefineEditText::FDTDefineEditText(FRect *_bounds, U8 _hasFont, U8 _hasMaxLength, U8 _hasTextColor,
									 U8 _readOnly, U8 _password, U8 _multiline, U8 _wordWrap, U8 _hasText,
									 U8 _useOutlines, U8 _border, U8 _noSelect, U8 _hasLayout, U16 _fontID,
									 U16 _fontHeight, FColor *_textColor, U16 _maxLength, U8 _alignment,
									 U16 _leftMargin, U16 _rightMargin, U16 _indent, U16 _leading,
									 FString *_variableName, FString *_initialText) : textColor(_textColor)
{

	bounds = _bounds;
	hasFont = _hasFont;
	hasMaxLength = _hasMaxLength;
	hasTextColor = _hasTextColor;
	readOnly = _readOnly;
	password = _password;
	multiline = _multiline;
	wordWrap = _wordWrap;
	hasText = _hasText;
	useOutlines = _useOutlines;
	border = _border;
	noSelect = _noSelect;
	hasLayout = _hasLayout;
	fontID = _fontID;
	fontHeight = _fontHeight;
	textColor = _textColor;
	maxLength = _maxLength;
	alignment = _alignment;
	leftMargin = _leftMargin;
	rightMargin = _rightMargin;
	indent = _indent;
	leading = _leading;
	variableName = _variableName;
	initialText = _initialText;

	characterID = FObjCollection::Increment();
}

//	deletes the FRECT and 2 FString's if they exist.

FDTDefineEditText::~FDTDefineEditText(void)
{

	delete bounds;
	delete variableName;
	delete initialText;
	delete textColor;
}

//	Writes to the given FSWFStream.

void FDTDefineEditText::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream tempBuffer;

	tempBuffer.WriteWord((U32)characterID);
	//character ID

	//write the bounds, it's an object that knows how to write itself
	bounds->WriteToSWFStream(&tempBuffer);

	//write all flags

	tempBuffer.WriteBits(hasText, 1);
	tempBuffer.WriteBits(wordWrap, 1);
	tempBuffer.WriteBits(multiline, 1);
	tempBuffer.WriteBits(password, 1);
	tempBuffer.WriteBits(readOnly, 1);
	tempBuffer.WriteBits(hasTextColor, 1);
	tempBuffer.WriteBits(hasMaxLength, 1);
	tempBuffer.WriteBits(hasFont, 1);

	tempBuffer.WriteBits(0, 2); //unknown flags
	tempBuffer.WriteBits(hasLayout, 1);
	tempBuffer.WriteBits(noSelect, 1);
	tempBuffer.WriteBits(border, 1);
	tempBuffer.WriteBits(0, 2); //unknown flags
	tempBuffer.WriteBits(useOutlines, 1);

	//tempBuffer.WriteBits(hasFont, 1);
	//tempBuffer.WriteBits(hasMaxLength, 1);
	//tempBuffer.WriteBits(hasTextColor, 1);
	//tempBuffer.WriteBits(readOnly, 1);
	//tempBuffer.WriteBits(password, 1);
	//tempBuffer.WriteBits(multiline, 1);
	//tempBuffer.WriteBits(wordWrap, 1);
	//tempBuffer.WriteBits(hasText, 1);
	//tempBuffer.WriteBits(useOutlines, 1);
	//tempBuffer.WriteBits(border, 1);
	//tempBuffer.WriteBits(noSelect, 1);
	//tempBuffer.WriteBits(hasLayout, 1);
	//tempBuffer.WriteBits(0, 4);			//check on this, 3 in docs but 4 gives 16 bits

	if (hasFont) {
		tempBuffer.WriteWord((U32)fontID);
		tempBuffer.WriteWord((U32)fontHeight);
	}

	if (hasTextColor) { // here changed.
		textColor->AlphaChannel(true);
		textColor->WriteToSWFStream(&tempBuffer);
	}

	if (hasMaxLength) {
		tempBuffer.WriteWord((U32)maxLength);
	}

	if (hasLayout) {
		tempBuffer.WriteByte(alignment);
		tempBuffer.WriteWord(leftMargin);
		tempBuffer.WriteWord(rightMargin);
		tempBuffer.WriteWord(indent);
		tempBuffer.WriteWord(leading);
	}

	variableName->WriteToSWFStream(&tempBuffer, true);

	if (hasText) {
		initialText->WriteToSWFStream(&tempBuffer, true);
	}

	_SWFStream->AppendTag(stagDefineEditText, tempBuffer.Size(), &tempBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineText  ---------------------------------------------------------

FDTDefineText::FDTDefineText(FRect *_textBounds, FMatrix *_textMatrix, int glyhpsInFont)
{

	characterID = FObjCollection::Increment();
	textBounds = _textBounds;
	textMatrix = _textMatrix;
	nIndexBits = (U8)FSWFStream::MinBits(glyhpsInFont, 0);
}

FDTDefineText::~FDTDefineText()
{

	delete textBounds;

	delete textMatrix;

	while (!textRecords.empty()) {

		delete textRecords.front();

		textRecords.pop_front();
	}
}

void FDTDefineText::AddTextRecord(FTextRecord *_textRec)
{

	textRecords.push_back(_textRec);
}

U16 FDTDefineText::ID(void)
{

	return (U16)characterID;
}

void FDTDefineText::WriteToSWFStream(FSWFStream *_SWFStream)
{
	U16 nAdvanceBits = 0;
	U16 nCodeBits = 0;
	FSWFStream body;

	body.WriteWord((U32)characterID);

	textBounds->WriteToSWFStream(&body);
	textMatrix->WriteToSWFStream(&body);

	// Get the bits needed to write the advance and character codes
	std::list<FTextRecord *>::iterator cursor1;
	for (cursor1 = textRecords.begin(); cursor1 != textRecords.end(); cursor1++) {
		nAdvanceBits = (U16)std::max((*cursor1)->MinAdvanceBits(), (U32)nAdvanceBits);
		nCodeBits = (U16)std::max((*cursor1)->MinCodeBits(), (U32)nCodeBits);
	}
	body.WriteByte((U32)nCodeBits);
	body.WriteByte((U32)nAdvanceBits);

	std::list<FTextRecord *>::iterator cursor;
	for (cursor = textRecords.begin(); cursor != textRecords.end(); cursor++) {
		(*cursor)->WriteToSWFStream(&body, nCodeBits, nAdvanceBits);
	}

	body.FlushBits();
	body.WriteByte((U32)0);

	_SWFStream->AppendTag(stagDefineText, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineText2  --------------------------------------------------------

FDTDefineText2::FDTDefineText2(FRect *_textBounds, FMatrix *_textMatrix, int glyhpsInFont)
{
	characterID = FObjCollection::Increment();
	textBounds = _textBounds;
	textMatrix = _textMatrix;
	nIndexBits = (int)FSWFStream::MinBits(glyhpsInFont, 0);
}

FDTDefineText2::~FDTDefineText2()
{

	delete textBounds;

	delete textMatrix;

	while (!textRecords.empty()) {

		delete textRecords.front();

		textRecords.pop_front();
	}
}

void FDTDefineText2::AddTextRecord(FTextRecord *_textRec)
{

	textRecords.push_back(_textRec);
}

U16 FDTDefineText2::ID(void)
{

	return (U16)characterID;
}

void FDTDefineText2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	U16 nAdvanceBits = 0;
	U16 nCodeBits = 0;
	FSWFStream body;

	body.WriteWord((U32)characterID);

	textBounds->WriteToSWFStream(&body);

	textMatrix->WriteToSWFStream(&body);

	// Get the bits needed to write the advance and character codes
	std::list<FTextRecord *>::iterator cursor1;
	for (cursor1 = textRecords.begin(); cursor1 != textRecords.end(); cursor1++) {
		nAdvanceBits = (U16)std::max((*cursor1)->MinAdvanceBits(), (U32)nAdvanceBits);
		nCodeBits = (U16)std::max((*cursor1)->MinCodeBits(), (U32)nCodeBits);
	}

	body.WriteByte((U32)nCodeBits);
	body.WriteByte((U32)nAdvanceBits);

	std::list<FTextRecord *>::iterator cursor;
	for (cursor = textRecords.begin(); cursor != textRecords.end(); cursor++) {
		(*cursor)->WriteToSWFStream(&body, nCodeBits, nAdvanceBits);
	}

	body.FlushBits();

	body.WriteByte((U32)0);

	_SWFStream->AppendTag(stagDefineText2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FTextChangeRecord  -----------------------------------------------------

FTextChangeRecord::FTextChangeRecord(U16 _hasFontFlag, U16 _hasColorFlag,
									 U16 _hasXOffsetFlag, U16 _hasYOffsetFlag,
									 U16 _fontID, U16 _fontHeight, FColor *_fontColor,
									 S16 _xOffset, S16 _yOffset)
{
	hasFontFlag = _hasFontFlag;
	hasColorFlag = _hasColorFlag;
	hasXOffsetFlag = _hasXOffsetFlag;
	hasYOffsetFlag = _hasYOffsetFlag;
	fontID = _fontID;
	fontColor = _fontColor;
	xOffset = _xOffset;
	yOffset = _yOffset;
	fontHeight = _fontHeight;
}

FTextChangeRecord::~FTextChangeRecord()
{
	delete fontColor;
}

// void FTextChangeRecord::IncludeNBitInfo(U16 _nIndexBits, U16 _nAdvanceBits){
// Does absolutely nothing
// TextGlyphRecord needs this method
// Had to be virtual so it could be called on any TextRecord
// }

void FTextChangeRecord::WriteToSWFStream(FSWFStream *_SWFStream, int /*codeBits*/, int /*advanceBits */)
{

	_SWFStream->WriteBits((U32)1, 1);

	// 	_SWFStream->WriteBits((U32) reserved, 3);
	_SWFStream->WriteBits((U32)0, 3);

	_SWFStream->WriteBits((U32)hasFontFlag, 1);
	_SWFStream->WriteBits((U32)hasColorFlag, 1);
	_SWFStream->WriteBits((U32)hasYOffsetFlag, 1);
	_SWFStream->WriteBits((U32)hasXOffsetFlag, 1);

	if (hasFontFlag)
		_SWFStream->WriteWord((U32)fontID);

	if (hasColorFlag) { // here changed.
		fontColor->AlphaChannel(true);
		fontColor->WriteToSWFStream(_SWFStream);
	}

	if (hasXOffsetFlag)
		_SWFStream->WriteWord((U32)xOffset);

	if (hasYOffsetFlag)
		_SWFStream->WriteWord((U32)yOffset);

	if (hasFontFlag)
		_SWFStream->WriteWord((U32)fontHeight);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FTextGlyphRecord  ------------------------------------------------------

FTextGlyphRecord::FTextGlyphRecord()
{
}

FTextGlyphRecord::~FTextGlyphRecord()
{
	while (!glyphEntries.empty()) {
		glyphEntries.pop_front();
	}
}

void FTextGlyphRecord::AddGlyphEntry(U16 code, U16 advance)
{
	GlyphEntry glyph = {code, advance};

	glyphEntries.push_back(glyph);
}

U32 FTextGlyphRecord::MinAdvanceBits()
{
	std::list<GlyphEntry>::iterator it;

	int maxBit = 0;
	for (it = glyphEntries.begin(); it != glyphEntries.end(); ++it) {
		maxBit = (int)std::max(FSWFStream::MinBits(it->advance, 1), (U32)maxBit);
	}
	return maxBit;
}

U32 FTextGlyphRecord::MinCodeBits()
{
	// 	return FSWFStream::MinBits( glyphEntries.size(), 0 );
	std::list<GlyphEntry>::iterator it;

	int maxBit = 0;
	for (it = glyphEntries.begin(); it != glyphEntries.end(); ++it) {
		int code = it->code;
		maxBit = (int)std::max(FSWFStream::MinBits(code, 1), (U32)maxBit);
	}
	return maxBit;
}

void FTextGlyphRecord::WriteToSWFStream(FSWFStream *_SWFStream, int codeBits, int advanceBits)
{
	FLASHASSERT((int)MinAdvanceBits() <= advanceBits);
	FLASHASSERT((int)MinCodeBits() <= codeBits);

	// Bug: The number of glyphs used to limit the the max code size, but this changes with
	// DefineFont2, where we could use glyph 38 as our sole glyph. So the re-write begins....
	int numberOfGlyphs = glyphEntries.size();

	_SWFStream->WriteBits((U32)0, 1);
	_SWFStream->WriteBits((U32)numberOfGlyphs, 7);

	std::list<GlyphEntry>::iterator cursor;

	for (cursor = glyphEntries.begin(); cursor != glyphEntries.end(); cursor++) {
		_SWFStream->WriteBits((U32)cursor->code, codeBits);
		_SWFStream->WriteBits((U32)cursor->advance, advanceBits);
	}
}
