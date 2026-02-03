// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTShapes.cpp

  This source file contains the definition for all low-level shape-related functions,
  grouped by classes:

		Class						Member Function

	FCXForm						FCXForm(U32, U32, S32, S32, S32, S32, S32, S32);
								U32 FCXForm::MinBits();
								void WriteToSWFStream(FSWFStream*);

	FCXFormWAlpha				FCXFormWAlpha(U32, U32, S32, S32, S32, S32, S32, S32, S32, S32);
								U32 MinBits();
								void WriteToSWFStream(FSWFStream*);

	FDTDefineMorphShape			FDTDefineMorphShape (FRect*, FRect*);
								~FDTDefineMorphShape();
								U32 AddFillStyle(FMorphFillStyle*);
								U32 AddLineStyle(U32, FColor*,
												 U32, FColor*);
								void FinishStyleArrays();
								void AddShapeRec_1(FShapeRec*);
								void AddEdgeRec_2(FShapeRec*);
								U16 ID();
								void WriteToSWFStream(FSWFStream*);

	FDTDefineShape				FDTDefineShape(FRect*);
								~FDTDefineShape();
								void AddShapeRec(FShapeRec*);
								U32 AddFillStyle(FFillStyle*);
								U32 AddSolidFillStyle(FColor*);
								U32 AddLineStyle(U32, FColor*);
								void FinishStyleArrays();
								U16 ID();
								void WriteToSWFStream(FSWFStream*);

	FDTDefineShape2				FDTDefineShape2(FRect*);
								~FDTDefineShape2();
								void AddShapeRec(FShapeRec*);
								U32 AddFillStyle(FFillStyle*);
								U32 AddSolidFillStyle(FColor*);
								U32 AddLineStyle(U32, FColor*);
								void FinishStyleArrays();
								U16 ID();
								void WriteToSWFStream(FSWFStream*);

	FDTDefineShape3				FDTDefineShape3(FRect*);
								~FDTDefineShape3();
								void AddShapeRec(FShapeRec*);
								U32 AddFillStyle(FFillStyle*);
								U32 AddSolidFillStyle(FColor*);
								U32 AddLineStyle(U32, FColor*);
								void FinishStyleArrays();
								U16 ID();
								void WriteToSWFStream(FSWFStream*);

	FFillStyleArray				U32 Size();
								U32 Add(FAFillStyle*);
								void WriteToSWFStream(FSWFStream*);

	FFillStyleBitmap			FFillStyleBitmap(int, U16, FMatrix*);
								~FFillStyleBitmap();
								void WriteToSWFStream(FSWFStream*);

	FFillStyleGradient			FFillStyleGradient(int, FMatrix*, FGradient*);
								~FFillStyleGradient();
								void WriteToSWFStream(FSWFStream*);

	FFillStyleSolid				FFillStyleSolid(FColor*);
								~FFillStyleSolid();
								void WriteToSWFStream(FSWFStream*);

	FGradient					FGradient(void);
								~FGradient(void);
								void Add(FAGradRecord*);
								void WriteToSWFStream(FSWFStream*);

	FGradRecord					FGradRecord(U32, FColor*);
								~FGradRecord();
								void WriteToSWFStream(FSWFStream*);

	FLineStyle					FLineStyle(U32, FColor*);
								~FLineStyle();
								void WriteToSWFStream(FSWFStream*);

	FLineStyleArray				FLineStyleArray();
								~FLineStyleArray();
								U32 Size();
								U32 Add(FALineStyle *);
								void WriteToSWFStream(FSWFStream *);

	FMorphFillStyleBitmap		FMorphFillStyleBitmap(int, U16, FMatrix*, FMatrix*);
								~FMorphFillStyleBitmap();
								void WriteToSWFStream(FSWFStream *);

	FMorphFillStyleGradient		FMorphFillStyleGradient(int, FMatrix*, FMatrix*, FGradient*);
								~FMorphFillStyleGradient();
								void WriteToSWFStream(FSWFStream *);

	FMorphFillStyleSolid		FMorphFillStyleSolid( FColor*, FColor*);
								~FMorphFillStyleSolid()
								void WriteToSWFStream(FSWFStream *);

	FMorphGradRecord			FMorphGradRecord(U32, FColor*, U32, FColor*);
								~FMorphGradRecord();
								void WriteToSWFStream(FSWFStream*);

	FMorphLineStyle				FMorphLineStyle(U32, U32, FColor*, FColor*);
								~FMorphLineStyle();
								void WriteToSWFStream(FSWFStream *);

	FShape						FShape();
								~FShape();
								SetFillBits(U32);
								SetLineBits(U32);
								AddShapeRec(FShapeRec *);
								void WriteToSWFStream(FSWFStream *);

	FShapeRecChange				FShapeRecChange(U32, U32, U32, U32, U32,
												S32, S32, U32, U32, U32, 
												FFillStyleArray*, FLineStyleArray*);
								~FShapeRecChange();
								void IncludeNFillBitInfo(U32);
								void IncludeNLineBitInfo(U32);
								U32 MinBits();
								void WriteToSWFStream(FSWFStream*);

	FShapeRecEdgeStraight		FShapeRecEdgeStraight(S32, S32); 
								U32 MinBits(void);
								void IncludeNFillBitInfo(U32);
								void IncludeNLineBitInfo(U32);
								void WriteToSWFStream(FSWFStream*);

	FShapeRecEdgeCurved			FShapeRecEdgeCurved(S32, S32, S32, S32);
								U32 MinBits(void);
								void IncludeNFillBitInfo(U32 );
								void IncludeNLineBitInfo(U32 );
								void WriteToSWFStream(FSWFStream*);	

	FShapeRecEnd				FShapeRecEnd();
								void IncludeNFillBitInfo(U32 );
								void IncludeNLineBitInfo(U32 );
								void WriteToSWFStream(FSWFStream*);

	FShapeWStyle				FShapeWStyle(FFillStyleArray*, FLineStyleArray*);
								~FShapeWStyle() ;
								void AddShapeRec(FShapeRec*);
								U32 NumFillBits();
								U32 NumLineBits();
								void WriteToSWFStream(FSWFStream*);

****************************************************************************************/

#include "tpixel.h"
#include "FDTShapes.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCXForm ----------------------------------------------------------------

FCXForm::FCXForm(U32 _hasAdd, U32 _hasMult, S32 _redMultTerm, S32 _greenMultTerm, S32 _blueMultTerm, S32 _redAddTerm, S32 _greenAddTerm, S32 _blueAddTerm)
{
	hasAdd = _hasAdd;
	hasMult = _hasMult;
	redMultTerm = _redMultTerm;
	greenMultTerm = _greenMultTerm;
	blueMultTerm = _blueMultTerm;
	redAddTerm = _redAddTerm;
	greenAddTerm = _greenAddTerm;
	blueAddTerm = _blueAddTerm;
	nBits = MinBits();
}

//
U32 FCXForm::MinBits()
{

	// two step process to find maximum value of 6 numbers because "FSWFStream::MaxNum" takes only 4 arguments
	U32 maxValue = FSWFStream::MaxNum(redMultTerm, greenMultTerm, blueMultTerm, redAddTerm);
	maxValue = FSWFStream::MaxNum(greenAddTerm, blueAddTerm, (S32)maxValue, 0);

	return FSWFStream::MinBits(maxValue, 1) + 1;
}

//
void FCXForm::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->WriteBits(hasMult, 1);
	_SWFStream->WriteBits(hasAdd, 1);
	_SWFStream->WriteBits(nBits, 4);

	if (hasMult) {
		_SWFStream->WriteBits((S32)redMultTerm, nBits);
		_SWFStream->WriteBits((S32)greenMultTerm, nBits);
		_SWFStream->WriteBits((S32)blueMultTerm, nBits);
	}
	if (hasAdd) {
		_SWFStream->WriteBits((S32)redAddTerm, nBits);
		_SWFStream->WriteBits((S32)greenAddTerm, nBits);
		_SWFStream->WriteBits((S32)blueAddTerm, nBits);
	}

	_SWFStream->FlushBits();
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCXFormWAlpha ----------------------------------------------------------

FCXFormWAlpha::FCXFormWAlpha(U32 _hasAdd, U32 _hasMult, S32 _redMultTerm, S32 _greenMultTerm, S32 _blueMultTerm, S32 _alphaMultTerm,
							 S32 _redAddTerm, S32 _greenAddTerm, S32 _blueAddTerm, S32 _alphaAddTerm)
	: FCXForm(_hasAdd, _hasMult, _redMultTerm, _greenMultTerm, _blueMultTerm,
			  _redAddTerm, _greenAddTerm, _blueAddTerm)
{
	alphaMultTerm = _alphaMultTerm;

	alphaAddTerm = _alphaAddTerm;

	nBits = MinBits();
}

U32 FCXFormWAlpha::MinBits()
{

	// FFileWrite's MaxNum method takes only 4 arguments, so finding maximum value of 8 arguments takes three steps:
	U32 maxMult = FSWFStream::MaxNum(redMultTerm, greenMultTerm, blueMultTerm, alphaMultTerm);
	U32 maxAdd = FSWFStream::MaxNum(redAddTerm, greenAddTerm, blueAddTerm, alphaAddTerm);
	U32 maxValue = FSWFStream::MaxNum((S32)maxMult, (S32)maxAdd, 0, 0);

	return FSWFStream::MinBits(maxValue, 1) + 1;
}

void FCXFormWAlpha::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//fill? I think so
	_SWFStream->FlushBits();

	_SWFStream->WriteBits(hasMult, 1);
	_SWFStream->WriteBits(hasAdd, 1);
	_SWFStream->WriteBits(nBits, 4);

	if (hasMult) {
		_SWFStream->WriteBits((S32)redMultTerm, nBits);
		_SWFStream->WriteBits((S32)greenMultTerm, nBits);
		_SWFStream->WriteBits((S32)blueMultTerm, nBits);
		_SWFStream->WriteBits((S32)alphaMultTerm, nBits);
	}
	if (hasAdd) {
		_SWFStream->WriteBits((S32)redAddTerm, nBits);
		_SWFStream->WriteBits((S32)greenAddTerm, nBits);
		_SWFStream->WriteBits((S32)blueAddTerm, nBits);
		_SWFStream->WriteBits((S32)alphaAddTerm, nBits);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineMorphShape ----------------------------------------------------

FDTDefineMorphShape::FDTDefineMorphShape(FRect *_rect1, FRect *_rect2)
{

	characterID = FObjCollection::Increment();
	shapeBounds_1 = _rect1;
	shapeBounds_2 = _rect2;
	morphFillStyleArray = new FFillStyleArray();
	morphLineStyleArray = new FLineStyleArray();
	styleArraysFinished = false;
}

FDTDefineMorphShape::~FDTDefineMorphShape()
{

	delete shapeBounds_1;
	delete shapeBounds_2;
	delete morphFillStyleArray;
	delete morphLineStyleArray;
}

U32 FDTDefineMorphShape::AddFillStyle(FMorphFillStyle *fillStyle)
{

	assert(!styleArraysFinished);
	return morphFillStyleArray->Add(fillStyle);
}

U32 FDTDefineMorphShape::AddLineStyle(U32 startLineWidth, FColor *startLineColor,
									  U32 finalLineWidth, FColor *finalLineColor)
{
	assert(!styleArraysFinished);

	// here changed.
	// morph shape always has RGBA color.
	startLineColor->AlphaChannel(true);
	finalLineColor->AlphaChannel(true);

	// Create a morph line style (one which contains both "morph from" and "morph to" line style information
	FMorphLineStyle *startToFinalLine = new FMorphLineStyle(startLineWidth, finalLineWidth,
															startLineColor, finalLineColor);
	return morphLineStyleArray->Add(startToFinalLine);
}

void FDTDefineMorphShape::FinishStyleArrays(void)
{

	styleArraysFinished = true;
	nFillBits = FSWFStream::MinBits(morphFillStyleArray->Size(), 0);
	nLineBits = FSWFStream::MinBits(morphLineStyleArray->Size(), 0);

	shape1.SetFillBits(nFillBits);
	shape1.SetLineBits(nLineBits);

	// asumption: shape2 will never contain fill or line syle info and this means no need
	// for fill or line style bits
}

void FDTDefineMorphShape::AddShapeRec_1(FShapeRec *_shapeRec)
{

	assert(styleArraysFinished);
	shape1.AddShapeRec(_shapeRec);
}

void FDTDefineMorphShape::AddEdgeRec_2(FShapeRec *_shapeRec)
{

	assert(styleArraysFinished);
	_shapeRec->IncludeNFillBitInfo(0); // Always Zero
	_shapeRec->IncludeNLineBitInfo(0);
	shape2.AddShapeRec(_shapeRec);
}

U16 FDTDefineMorphShape::ID(void)
{

	return characterID;
}

void FDTDefineMorphShape::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);

	shapeBounds_1->WriteToSWFStream(&body);

	shapeBounds_2->WriteToSWFStream(&body);

	FSWFStream temp;

	morphFillStyleArray->WriteToSWFStream(&temp);
	morphLineStyleArray->WriteToSWFStream(&temp);

	shape1.WriteToSWFStream(&temp);

	body.WriteDWord(temp.Size());

	body.Append(&temp);

	shape2.WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagDefineMorphShape, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineShape ---------------------------------------------------------

FDTDefineShape::FDTDefineShape(FRect *_rect)
{

	characterID = FObjCollection::Increment();
	shapeBounds = _rect;
	fillStyleArray = new FFillStyleArray();
	lineStyleArray = new FLineStyleArray();
	shapeWithStyle = NULL;
	styleArraysFinished = false;
}

FDTDefineShape::~FDTDefineShape()
{

	delete fillStyleArray;
	delete lineStyleArray;
	delete shapeBounds;
	delete shapeWithStyle;
}

void FDTDefineShape::AddShapeRec(FShapeRec *_shapeRec)
{

	//you must be done creating the fill style and line style arrays
	//before you can begin adding shape records
	assert(styleArraysFinished);

	//Change rec doesn't know how many bits for saving the FillStyle0
	//and FillStyle1.  It gets this from the ShapeWithStyle which contains
	//it.  What happens when the change Rec also contains Fill and Line
	//Syles???  The other types of ShapeRecs just use the "current style"
	//and don't need this info so their methods don't actually do anything.
	//Why not just go to the container when writing?
	_shapeRec->IncludeNFillBitInfo(shapeWithStyle->NumFillBits());
	_shapeRec->IncludeNLineBitInfo(shapeWithStyle->NumLineBits());
	//now have the shapeWithStyle add the ShapeRec
	shapeWithStyle->AddShapeRec(_shapeRec);
}

//This shapes internal fillStyleArray adds a fillStyle to itself.
U32 FDTDefineShape::AddFillStyle(FFillStyle *fillStyle)
{

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}
// here changed.
U32 FDTDefineShape::AddSolidFillStyle(FColor *fillColor)
{
	fillColor->AlphaChannel(false);
	FFillStyle *fillStyle = new FFillStyleSolid(fillColor);

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}

U32 FDTDefineShape::AddLineStyle(U32 lineWidth, FColor *lineColor)
{

	assert(!styleArraysFinished); //once complete, cannot change

	// here changed.
	lineColor->AlphaChannel(false);
	FLineStyle *lineStyle = new FLineStyle(lineWidth, lineColor);

	//add line style to rectangle, remembering to store the position of the line style
	//just as in the fill style
	return lineStyleArray->Add(lineStyle);
}

/*!	Creates the shape with style.  Can only do this once the fill style
	and line style arrays are complete because the shapeWithStyle constructor
	takes in complete fill and line style arrays. 
*/
void FDTDefineShape::FinishStyleArrays(void)
{

	styleArraysFinished = true;
	shapeWithStyle = new FShapeWStyle(fillStyleArray, lineStyleArray);
	fillStyleArray = NULL; /*AMM*/
	lineStyleArray = NULL;
}

U16 FDTDefineShape::ID(void)
{

	return characterID;
}

//Called by the FObjCollection when writing to the stream
//In turn, it calls the WriteToSWFStream methods of its embedded objects.

void FDTDefineShape::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream tempBuffer;

	tempBuffer.WriteWord((U32)characterID);
	shapeBounds->WriteToSWFStream(&tempBuffer);
	shapeWithStyle->WriteToSWFStream(&tempBuffer);

	_SWFStream->AppendTag(stagDefineShape, tempBuffer.Size(), &tempBuffer);
}

bool FDTDefineShape::IsDefineShape()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineShape2 --------------------------------------------------------
FDTDefineShape2::FDTDefineShape2(FRect *_rect)
{

	characterID = FObjCollection::Increment();
	shapeBounds = _rect;
	fillStyleArray = new FFillStyleArray();
	lineStyleArray = new FLineStyleArray();
	shapeWithStyle = NULL;
	styleArraysFinished = false;
}

FDTDefineShape2::~FDTDefineShape2()
{

	delete fillStyleArray;
	delete lineStyleArray;
	delete shapeBounds;
	delete shapeWithStyle;
}

void FDTDefineShape2::AddShapeRec(FShapeRec *_shapeRec)
{

	//you must be done creating the fill style and line style arrays
	//before you can begin adding shape records
	assert(styleArraysFinished);

	_shapeRec->IncludeNFillBitInfo(shapeWithStyle->NumFillBits());
	_shapeRec->IncludeNLineBitInfo(shapeWithStyle->NumLineBits());
	shapeWithStyle->AddShapeRec(_shapeRec);
}

U32 FDTDefineShape2::AddFillStyle(FFillStyle *fillStyle)
{

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}
// here changed.
U32 FDTDefineShape2::AddSolidFillStyle(FColor *fillColor)
{
	fillColor->AlphaChannel(false);
	FFillStyle *fillStyle = new FFillStyleSolid(fillColor);

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}

U32 FDTDefineShape2::AddLineStyle(U32 lineWidth, FColor *lineColor)
{

	assert(!styleArraysFinished); //once complete, cannot change

	// here changed.
	lineColor->AlphaChannel(false);
	FLineStyle *lineStyle = new FLineStyle(lineWidth, lineColor);

	//add line style to rectangle, remembering to store the position of the line style
	//just as in the fill style
	return lineStyleArray->Add(lineStyle);
}

//	Creates the shape with style.  Can only do this once the fill style
//	and line style arrays are complete because the shapeWithStyle constructor
//	takes in complete fill and line style arrays.

void FDTDefineShape2::FinishStyleArrays(void)
{

	styleArraysFinished = true;
	shapeWithStyle = new FShapeWStyle(fillStyleArray, lineStyleArray);
	fillStyleArray = NULL; /*AMM*/
	lineStyleArray = NULL;
}

U16 FDTDefineShape2::ID(void)
{

	return characterID;
}

bool FDTDefineShape2::IsDefineShape()
{
	assert(false);
	return true;
}

void FDTDefineShape2::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);
	shapeBounds->WriteToSWFStream(&body);
	shapeWithStyle->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagDefineShape2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineShape3 --------------------------------------------------------

FDTDefineShape3::FDTDefineShape3(FRect *_rect)
{
	characterID = FObjCollection::Increment();
	shapeBounds = _rect;
	fillStyleArray = new FFillStyleArray();
	lineStyleArray = new FLineStyleArray();
	shapeWithStyle = NULL;
	styleArraysFinished = false;
}

//////////////////////////////////////////////////////////////////////////////////////

FDTDefineShape3::FDTDefineShape3()
{

	characterID = FObjCollection::Increment();
	shapeBounds = 0;
	fillStyleArray = new FFillStyleArray();
	lineStyleArray = new FLineStyleArray();
	shapeWithStyle = NULL;
	styleArraysFinished = false;
}

FDTDefineShape3::~FDTDefineShape3()
{

	delete fillStyleArray;
	delete lineStyleArray;
	delete shapeBounds;
	delete shapeWithStyle;
}

void FDTDefineShape3::setBounds(FRect *_rect)
{
	if (shapeBounds)
		delete shapeBounds;
	shapeBounds = _rect;
}

//////////////////////////////////////////////////////////////////////////////////////

bool FDTDefineShape3::IsDefineShape()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

inline FColor tpixel2fcolor(const TPixel &color)
{
	return FColor(color.r, color.g, color.b, color.m);
}

//////////////////////////////////////////////////////////////////////////////////////

inline TPixel fcolor2tpixel(FColor &color)
{
	return TPixel(color.Red(), color.Green(), color.Blue(), color.Alpha());
}

//////////////////////////////////////////////////////////////////////////////////////

void FDTDefineShape3::changeColor(const std::map<TPixel, TPixel> &table)
{
	unsigned int i;
	if (fillStyleArray)
		for (i = 0; i < fillStyleArray->Size(); i++) {
			FAFillStyle *style = fillStyleArray->get(i);
			if (style->IsSolidStyle()) {
				TPixel oldColor = fcolor2tpixel(*(((FFillStyleSolid *)style)->getColor()));
				std::map<TPixel, TPixel>::const_iterator it = table.find(oldColor);
				if (it != table.end())
					((FFillStyleSolid *)style)->setColor(new FColor(tpixel2fcolor(it->second)));
			}
		}

	shapeWithStyle->changeColor(table);
}

//////////////////////////////////////////////////////////////////////////////////////

void FDTDefineShape3::changeColor(const FColor &oldColor, const FColor &newColor)
{
	unsigned int i;
	if (fillStyleArray)
		for (i = 0; i < fillStyleArray->Size(); i++) {
			FAFillStyle *style = fillStyleArray->get(i);
			if (style->IsSolidStyle() && (*((FFillStyleSolid *)style)->getColor()) == oldColor)
				((FFillStyleSolid *)style)->setColor(new FColor(newColor));
		}

	shapeWithStyle->changeColor(oldColor, newColor);
}

//////////////////////////////////////////////////////////////////////////////////////

void FDTDefineShape3::AddShapeRec(FShapeRec *_shapeRec)
{

	//you must be done creating the fill style and line style arrays
	//before you can begin adding shape records
	assert(styleArraysFinished);

	_shapeRec->IncludeNFillBitInfo(shapeWithStyle->NumFillBits());
	_shapeRec->IncludeNLineBitInfo(shapeWithStyle->NumLineBits());
	shapeWithStyle->AddShapeRec(_shapeRec);
}

U32 FDTDefineShape3::AddFillStyle(FFillStyle *fillStyle)
{

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}

// here changed.
U32 FDTDefineShape3::AddSolidFillStyle(FColor *fillColor)
{
	fillColor->AlphaChannel(true);
	FFillStyle *fillStyle = new FFillStyleSolid(fillColor);

	assert(!styleArraysFinished); //once complete, cannot change
	return fillStyleArray->Add(fillStyle);
}

U32 FDTDefineShape3::AddLineStyle(U32 lineWidth, FColor *lineColor)
{

	assert(!styleArraysFinished); //once complete, cannot change

	// here changed.
	lineColor->AlphaChannel(true);
	FLineStyle *lineStyle = new FLineStyle(lineWidth, lineColor);

	//add line style to rectangle, remembering to store the position of the line style
	//just as in the fill style
	return lineStyleArray->Add(lineStyle);
}

//	Creates the shape with style.  Can only do this once the fill style
//	and line style arrays are complete because the shapeWithStyle constructor
//	takes in complete fill and line style arrays.

void FDTDefineShape3::FinishStyleArrays(void)
{

	styleArraysFinished = true;
	shapeWithStyle = new FShapeWStyle(fillStyleArray, lineStyleArray);
	fillStyleArray = NULL;
	lineStyleArray = NULL;
}

U16 FDTDefineShape3::ID(void)
{

	return characterID;
}

void FDTDefineShape3::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);
	shapeBounds->WriteToSWFStream(&body);
	shapeWithStyle->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagDefineShape3, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FFillStyleArray --------------------------------------------------------

FFillStyleArray::~FFillStyleArray()
{
	while (!fillStyleArray.empty()) {
		delete fillStyleArray.back();
		fillStyleArray.pop_back();
	}
}

//	Returns the size of the fill style list.
U32 FFillStyleArray::Size(void)
{
	return (U32)fillStyleArray.size();
}

//	The given fill style is added to the end of the fill style array.  The position of
//  the added fill style is returned so that the fill style can later be referenced.

U32 FFillStyleArray::Add(FAFillStyle *fillStyle)
{
	FLASHASSERT(fillStyle);

	fillStyleArray.push_back(fillStyle);
	return ((U32)fillStyleArray.size());
}

//////////////////////////////////////////////////////////////////////////////////////

FAFillStyle *FFillStyleArray::get(unsigned int index)
{
	return fillStyleArray[index];
}

//	Writes to the stream by travelling through all of the nodes in the array and writing
//  their fill styles.  First has to write the count of fill style arrays.  See's if count
//  is small enough to fit in to an 8 bit field, and either writes the count into an 8 bit
//  field, or writes all 1's into an 8 bit field and writes the real count into a 16 bit
//  field.

void FFillStyleArray::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the size
	U32 size = fillStyleArray.size();

	if (size >= 0xff) {

		_SWFStream->WriteByte(0xff);
		_SWFStream->WriteWord(size);

	} else {

		_SWFStream->WriteByte(size);
	}

	//write the fill styles
	std::vector<FAFillStyle *>::iterator cursor;

	for (cursor = fillStyleArray.begin(); cursor != fillStyleArray.end(); cursor++) {
		(*cursor)->WriteToSWFStream(_SWFStream);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FFillStyleBitmap -------------------------------------------------------

//	The tiledFlag indicates if the Bitmap fill style is tiled (tiledFlag==1) or
//	clipped (tiledFlag==0).
FFillStyleBitmap::FFillStyleBitmap(int tiled, U16 ID, FMatrix *matrix)
{

	tiledFlag = tiled;
	bitmapID = ID;
	bitmapMatrix = matrix;
}

//	Deletes the matrix.

FFillStyleBitmap::~FFillStyleBitmap(void)
{

	delete bitmapMatrix;
	bitmapMatrix = NULL;
}

//	Writes the bitmap fill style to the given FSWFStream.

void FFillStyleBitmap::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the type
	if (tiledFlag)
		_SWFStream->WriteByte(fillTiledBits);
	else
		_SWFStream->WriteByte(fillClippedBits);

	//write the bitmap id
	_SWFStream->WriteWord(bitmapID);

	//write the matrix
	bitmapMatrix->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FFillStyleGradient -----------------------------------------------------

//	The linearFlag indicates if the gradient fill style is linear (linearFlag==1) or
//	radial (linearFlag==0).
FFillStyleGradient::FFillStyleGradient(int linear, FMatrix *matrix, FGradient *gradient)
{
	linearFlag = linear;
	gradFillMatrix = matrix;
	gradFill = gradient;
}

//	Deletes the matrix and gradient.

FFillStyleGradient::~FFillStyleGradient(void)
{
	delete gradFillMatrix;
	delete gradFill;
}

//	Writes the Gradient fill style to the given FSWFStream.

void FFillStyleGradient::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//write the type
	if (linearFlag)
		_SWFStream->WriteByte(fillLinearGradient);
	else
		_SWFStream->WriteByte(fillRadialGradient);

	//write the matrix
	gradFillMatrix->WriteToSWFStream(_SWFStream);

	//write the gradient
	gradFill->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FFillStyleSolid --------------------------------------------------------

//	Stores the color of the solid fill style.
FFillStyleSolid::FFillStyleSolid(FColor *_color) : color(_color) {}

FFillStyleSolid::~FFillStyleSolid()
{
	delete color;
}

//	Writes the solid fill style to the given FSWFStream.

void FFillStyleSolid::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the type
	_SWFStream->WriteByte(fillSolid); //cast to U32?

	//write the color
	color->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FGradient --------------------------------------------------------------

//	Constructor.  The grad record list is automatically constructed.
FGradient::FGradient(void) {}

//	Removes and deletes all the grad records from the grad record list.

FGradient::~FGradient(void)
{

	while (!gradRecords.empty()) {

		delete gradRecords.front();

		gradRecords.pop_front();
	}
}

//	Adds the given grad record to the end of the grad record list.

void FGradient::Add(FAGradRecord *gradRec)
{

	gradRecords.push_back(gradRec);
}

//	Writes the grad records to the given _SWFStream.

void FGradient::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the size

	_SWFStream->WriteByte((U32)gradRecords.size());

	//write the grad records

	std::list<FAGradRecord *>::iterator cursor;

	for (cursor = gradRecords.begin(); cursor != gradRecords.end(); cursor++) {

		(*cursor)->WriteToSWFStream(_SWFStream);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FGradientRecord --------------------------------------------------------

//	FGradRecord class constructor.
FGradRecord::FGradRecord(U32 _ratio, FColor *_color)
{
	color = _color;
	ratio = _ratio;
}

FGradRecord::~FGradRecord()
{
	delete color;
}

//	Writes the grad record to the given buffer.

void FGradRecord::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->WriteByte((U32)ratio);

	color->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FLineStyle -------------------------------------------------------------

//	LineStyle class constructor.
FLineStyle::FLineStyle(U32 _width, FColor *_color)
{
	color = _color;
	width = _width; //in TWIPS
}

FLineStyle::~FLineStyle()
{
	delete color;
}

//	Writes the object to the given _SWFStream.

void FLineStyle::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->WriteWord(width);
	color->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FLineStyleArray --------------------------------------------------------

//	The line style array is automatically constructed
FLineStyleArray::FLineStyleArray(void) {}

//	Removes and deletes every element in the list.

FLineStyleArray::~FLineStyleArray(void)
{

	while (!lineStyleArray.empty()) {

		delete lineStyleArray.front();
		lineStyleArray.pop_front();
	}
}

//  Returns the size of the line style list.

U32 FLineStyleArray::Size(void)
{

	return (U32)lineStyleArray.size();
}

//	The given line style is added to the end of the line style array.  The position of
//	the added line style is returned so that the line style can later be referenced.

U32 FLineStyleArray::Add(FALineStyle *lineStyle)
{

	lineStyleArray.push_back(lineStyle);
	return ((U32)lineStyleArray.size());
}

//	Writes to the stream by travelling through all of the nodes in the array and writing
//  their line styles.  First has to write the count of line style arrays.  See's if count
//  is small enough to fit in to an 8 bit field, and either writes the count into an 8 bit
//  field, or writes all 1's into an 8 bit field and writes the real count into a 16 bit
//  field.

void FLineStyleArray::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the size
	U32 size = lineStyleArray.size();

	if (size >= 0xff) {

		_SWFStream->WriteByte(0xff);
		_SWFStream->WriteWord(size);

	} else {

		_SWFStream->WriteByte(size);
	}

	//write the line styles
	std::list<FALineStyle *>::iterator cursor;

	for (cursor = lineStyleArray.begin(); cursor != lineStyleArray.end(); cursor++) {

		(*cursor)->WriteToSWFStream(_SWFStream);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMorphFillStyleBitmap --------------------------------------------------

//	The tiledFlag indicates if the Bitmap fill style is tiled (tiledFlag==1) or
//	clipped (tiledFlag==0).
FMorphFillStyleBitmap::FMorphFillStyleBitmap(int tiled, U16 ID,
											 FMatrix *matrix1, FMatrix *matrix2)
{

	tiledFlag = tiled;
	bitmapID = ID;
	bitmapMatrix1 = matrix1;
	bitmapMatrix2 = matrix2;
}

//	Deletes the matrices.

FMorphFillStyleBitmap::~FMorphFillStyleBitmap(void)
{

	delete bitmapMatrix1;
	bitmapMatrix1 = NULL;

	delete bitmapMatrix2;
	bitmapMatrix2 = NULL;
}

//	Writes the bitmap fill style to the given FSWFStream.

void FMorphFillStyleBitmap::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the type
	if (tiledFlag)
		_SWFStream->WriteByte(fillTiledBits);
	else
		_SWFStream->WriteByte(fillClippedBits);

	//write the bitmap id
	_SWFStream->WriteWord(bitmapID);

	//write the matrices
	bitmapMatrix1->WriteToSWFStream(_SWFStream);
	bitmapMatrix2->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMorphFillStyleGradient ------------------------------------------------

//	The linearFlag indicates if the gradient fill style is linear (linearFlag==1) or
//	radial (linearFlag==0).
FMorphFillStyleGradient::FMorphFillStyleGradient(int linear, FMatrix *matrix1, FMatrix *matrix2,
												 FGradient *gradient)
{

	linearFlag = linear;
	gradFillMatrix1 = matrix1;
	gradFillMatrix2 = matrix2;
	gradFill = gradient;
}

//	Deletes the matrices and gradient.

FMorphFillStyleGradient::~FMorphFillStyleGradient(void)
{

	delete gradFillMatrix1;
	gradFillMatrix1 = NULL;
	delete gradFillMatrix2;
	gradFillMatrix2 = NULL;
	delete gradFill;
	gradFill = NULL;
}

//	Writes the Gradient fill style to the given FSWFStream.

void FMorphFillStyleGradient::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the type
	if (linearFlag)
		_SWFStream->WriteByte(fillLinearGradient);
	else
		_SWFStream->WriteByte(fillRadialGradient);

	//write the matrices
	gradFillMatrix1->WriteToSWFStream(_SWFStream);
	gradFillMatrix2->WriteToSWFStream(_SWFStream);

	//write the gradient
	gradFill->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMorphFillStyleSolid ---------------------------------------------------

//	Stores the colors of the solid fill style.
FMorphFillStyleSolid::FMorphFillStyleSolid(FColor *_color1, FColor *_color2)
{
	color1 = _color1;
	color2 = _color2;
}

FMorphFillStyleSolid::~FMorphFillStyleSolid()
{
	delete color1;
	delete color2;
}

//	Writes the solid fill style to the given FSWFStream.

void FMorphFillStyleSolid::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//write the type
	_SWFStream->WriteByte(fillSolid); //cast to U32?

	//write the colors
	color1->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
	color2->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMorphGradRecord -------------------------------------------------------

//	Constructor.
FMorphGradRecord::FMorphGradRecord(U32 _ratio1, FColor *_color1, U32 _ratio2, FColor *_color2)
{

	ratio1 = _ratio1;
	ratio2 = _ratio2;

	color1 = _color1;
	color2 = _color2;
}

FMorphGradRecord::~FMorphGradRecord()
{
	delete color1;
	delete color2;
}

//  Writes to the given _SWFStream.

void FMorphGradRecord::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->WriteByte((U32)ratio1);
	color1->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.

	_SWFStream->WriteByte((U32)ratio2);
	color2->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FMorphLineStyle --------------------------------------------------------

//The line style used by morph shapes
FMorphLineStyle::FMorphLineStyle(U32 _width1, U32 _width2, FColor *_color1,
								 FColor *_color2) : color1(_color1), color2(_color2)
{
	width1 = _width1;
	width2 = _width2;

	color1 = _color1;
	color2 = _color2;
}

FMorphLineStyle::~FMorphLineStyle()
{
	delete color1;
	delete color2;
}

//	Writes to the given _SWFStream.

void FMorphLineStyle::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//write the widths
	_SWFStream->WriteWord(width1);
	_SWFStream->WriteWord(width2);

	//write the colors
	color1->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
	color2->WriteToSWFStream(_SWFStream); //, FColor::WRITE_SMALLEST ); changed here.
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShape -----------------------------------------------------------------

//	Shape class constructor.  The shape record list is automatically constructed
FShape::FShape(void)
{

	nFillBits = 0;
	nLineBits = 0;
}

//	Removes and deletes every element in the list.

FShape::~FShape(void)
{

	while (!shapeRecs.empty()) {

		delete shapeRecs.back();
		shapeRecs.pop_back();
	}
}

//	Sets the nFillBits field.

void FShape::SetFillBits(U32 _nFillBits)
{

	nFillBits = _nFillBits;
}

//	Sets the nLineBits field.

void FShape::SetLineBits(U32 _nLineBits)
{

	nLineBits = _nLineBits;
}

//	Adds a shape record to the end of the shape record list.

void FShape::AddShapeRec(FShapeRec *shapeRec)
{

	shapeRecs.push_back(shapeRec);
}

//	Writes the object to the given buffer.

void FShape::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->WriteBits(nFillBits, 4);
	_SWFStream->WriteBits(nLineBits, 4);

	std::vector<FShapeRec *>::iterator cursor;

	for (cursor = shapeRecs.begin(); cursor != shapeRecs.end(); cursor++) {
		//the shape rec might be referring to a fillor line style arry
		//external (ShapesWStyle) and will need the # of fillBits.
		(*cursor)->IncludeNFillBitInfo(nFillBits); //enter fillbits
		(*cursor)->IncludeNLineBitInfo(nLineBits); //enter linebits
		(*cursor)->WriteToSWFStream(_SWFStream);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShapeRecChange --------------------------------------------------------

//	FShapeRecChange class constructor.  It is passed the nFillBits and nLineBits values.
FShapeRecChange::FShapeRecChange(U32 _stateNewStyles,
								 U32 _stateLineStyle,
								 U32 _stateFillStyle1,
								 U32 _stateFillStyle0,
								 U32 _stateMoveTo,
								 S32 _moveDeltaX,
								 S32 _moveDeltaY,
								 U32 _fill0Style,
								 U32 _fill1Style,
								 U32 _lineStyle,
								 FFillStyleArray *_fillStyles,
								 FLineStyleArray *_lineStyles)
{
	stateNewStyles = _stateNewStyles;
	stateLineStyle = _stateLineStyle;
	stateFillStyle1 = _stateFillStyle1;
	stateFillStyle0 = _stateFillStyle0;
	stateMoveTo = _stateMoveTo;

	moveDeltaX = _moveDeltaX;
	moveDeltaY = _moveDeltaY;
	fill0Style = _fill0Style;
	fill1Style = _fill1Style;
	lineStyle = _lineStyle;
	fillStyles = _fillStyles;
	lineStyles = _lineStyles;
	nMoveBits = MinBits();
}

FFillStyleArray *FShapeRecChange::GetFillStyles()
{
	return fillStyles;
}

//	Deletes fillStyles and lineStyles if they exist.

FShapeRecChange::~FShapeRecChange(void)
{

	if (fillStyles) { //if fillStyles isn't NULL

		delete fillStyles;
		fillStyles = NULL;
	}

	if (lineStyles) { //if lineStyles isn't NULL

		delete lineStyles;
		lineStyles = NULL;
	}
}

//!Change Rec needs to know how many bits to write for the Fill and Line Styles
/*!Change Rec doesn't actually store the nLineBits or nFillBits, but needs to know
	them when it writes the active style fields so that it knows how many bits
	to write
	\param	_nFillBits the size in bits of the index into the FillStyleArray
*/
void FShapeRecChange::IncludeNFillBitInfo(U32 _nFillBits)
{

	nFillBits = _nFillBits;
}

//!Change Rec needs to know how many bits to write for the Fill and Line Styles
/*!Change Rec doesn't actually store the nLineBits or nFillBits, but needs to know
	them when it writes the active style fields so that it knows how many bits
	to write
	\param	_nLineBits the size in bits of the index into the LineStyleArray
*/
void FShapeRecChange::IncludeNLineBitInfo(U32 _nLineBits)
{

	nLineBits = _nLineBits;
}

//	Calculates nMoveBits by returning the number of bits needed to store the larger of
//	moveDeltaX and moveDeltaY.

U32 FShapeRecChange::MinBits(void)
{

	U32 max = FSWFStream::MaxNum(moveDeltaX, moveDeltaY, 0, 0);
	return FSWFStream::MinBits(max, 1);
}

//	Writes the shape record to the given _SWFStream.

void FShapeRecChange::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//non-edge record flag
	_SWFStream->WriteBits(NOT_EDGE_REC, 1);

	_SWFStream->WriteBits(stateNewStyles, 1);
	_SWFStream->WriteBits(stateLineStyle, 1);
	_SWFStream->WriteBits(stateFillStyle1, 1);
	_SWFStream->WriteBits(stateFillStyle0, 1);
	_SWFStream->WriteBits(stateMoveTo, 1);

	if (stateMoveTo) {

		_SWFStream->WriteBits(nMoveBits, 5);
		_SWFStream->WriteBits(moveDeltaX, nMoveBits);
		_SWFStream->WriteBits(moveDeltaY, nMoveBits);
	}

	if (stateFillStyle0) {

		_SWFStream->WriteBits(fill0Style, nFillBits);
	}

	if (stateFillStyle1) {

		_SWFStream->WriteBits(fill1Style, nFillBits);
	}

	if (stateLineStyle) {

		_SWFStream->WriteBits(lineStyle, nLineBits);
	}

	if (stateNewStyles) {

		fillStyles->WriteToSWFStream(_SWFStream);
		lineStyles->WriteToSWFStream(_SWFStream);

		nFillBits = FSWFStream::MinBits(fillStyles->Size(), 0);
		nLineBits = FSWFStream::MinBits(lineStyles->Size(), 0);

		_SWFStream->WriteBits(nFillBits, 4); //li mortacci loro!!! "dimenticavano" di mettere questi...
		_SWFStream->WriteBits(nLineBits, 4);
	}

	//NO FILLING IN BETWEEN SHAPE RECORDS
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShapeRecEdgeStraight --------------------------------------------------

FShapeRecEdgeStraight::FShapeRecEdgeStraight(S32 dx, S32 dy)
{
	generalLineFlag = 0;
	generalDeltaX = 0;
	generalDeltaY = 0;
	verticalLineFlag = 0;
	horizontalDeltaX = 0;
	verticalDeltaY = 0;

	edgeFlag = 1;

	// is this a general line?
	if (dx != 0 && dy != 0) {
		generalLineFlag = true;
		generalDeltaX = dx;
		generalDeltaY = dy;
	} else if (dx == 0) // not general, is it vertical?
	{
		verticalLineFlag = true;
		verticalDeltaY = dy;
	} else {
		verticalLineFlag = false;
		horizontalDeltaX = dx;
	}
	nBits = MinBits();
	assert(nBits < 16 + 2); //vincenzo: se no non entra nei quattro bits riservati per scrivere questo valore...
}

U32 FShapeRecEdgeStraight::MinBits(void)
{

	U32 maxDelta = FSWFStream::MaxNum(generalDeltaX, generalDeltaY,
									  horizontalDeltaX, verticalDeltaY);

	return FSWFStream::MinBits(maxDelta, 1);
}

void FShapeRecEdgeStraight::IncludeNFillBitInfo(U32 /*_nFillBits*/)
{
}

void FShapeRecEdgeStraight::IncludeNLineBitInfo(U32 /*_nLineBits*/)
{
}

void FShapeRecEdgeStraight::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//edge record flag
	_SWFStream->WriteBits(EDGE_REC, 1);

	_SWFStream->WriteBits(STRAIGHT_EDGE, 1); //This is a Straight edge record
	_SWFStream->WriteBits(nBits - 2, 4);
	_SWFStream->WriteBits(generalLineFlag, 1);

	if (generalLineFlag) {

		_SWFStream->WriteBits((U32)generalDeltaX, nBits);
		_SWFStream->WriteBits((U32)generalDeltaY, nBits);

	} else {

		_SWFStream->WriteBits(verticalLineFlag, 1); //verticalFlag is supposed to be signed but don't think it matters

		if (!verticalLineFlag)
			_SWFStream->WriteBits((U32)horizontalDeltaX, nBits);

		else
			_SWFStream->WriteBits((U32)verticalDeltaY, nBits);
	}

	//NO FILLING IN BETWEEN SHAPE RECORDS
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShapeRecEdgeCurved  ---------------------------------------------------

//	FShapeRecEdge class constructor.
FShapeRecEdgeCurved::FShapeRecEdgeCurved(S32 controlDX, S32 controlDY, S32 anchorDX, S32 anchorDY)
{
	edgeFlag = 0;

	controlDeltaX = controlDX;
	controlDeltaY = controlDY;
	anchorDeltaX = anchorDX;
	anchorDeltaY = anchorDY;

	nBits = MinBits();
}
//	Finds the min bits necessary to represent the 4 fields, by seeing how many bits are
//	necessary to represent the largest field of the four.

U32 FShapeRecEdgeCurved::MinBits(void)
{

	U32 maxDelta = FSWFStream::MaxNum(controlDeltaX, controlDeltaY, anchorDeltaX, anchorDeltaY);

	return FSWFStream::MinBits(maxDelta, 1);
}

void FShapeRecEdgeCurved::IncludeNFillBitInfo(U32 /*_nFillBits*/)
{
}

void FShapeRecEdgeCurved::IncludeNLineBitInfo(U32 /*_nLineBits*/)
{
}

//	Writes the shape record to the given _SWFStream.

void FShapeRecEdgeCurved::WriteToSWFStream(FSWFStream *_SWFStream)
{
	//edge record flag
	_SWFStream->WriteBits(EDGE_REC, 1);

	_SWFStream->WriteBits(CURVED_EDGE, 1); //This is a curved edge record

	_SWFStream->WriteBits(nBits - 2, 4);

	_SWFStream->WriteBits((U32)controlDeltaX, nBits);
	_SWFStream->WriteBits((U32)controlDeltaY, nBits);
	_SWFStream->WriteBits((U32)anchorDeltaX, nBits);
	_SWFStream->WriteBits((U32)anchorDeltaY, nBits);

	//NO FILLING IN BETWEEN SHAPE RECORDS
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShapeRecEnd  ----------------------------------------------------------

//	ShapeRecEnd class constructor.  Doesn't take in anything because the
//	object serves as an end tag and has no details.
FShapeRecEnd::FShapeRecEnd(void) {}

void FShapeRecEnd::IncludeNFillBitInfo(U32 /*_nFillBits*/)
{
	// does nothing
	//virtual method needed for shape rec change
}

void FShapeRecEnd::IncludeNLineBitInfo(U32 /*_nLineBits*/)
{
	//same deal
}

//	Writes the object to the given buffer.

void FShapeRecEnd::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//stream of 0's signifies the end
	_SWFStream->WriteBits(0, 6);

	//need to fill to end shape rec array structure.
	_SWFStream->FlushBits();
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FShapeWStyle  ----------------------------------------------------------

//	FShapeWStyle class constructor.  The shape record list is automatically constructed.
FShapeWStyle::FShapeWStyle(FFillStyleArray *_fillStyles, FLineStyleArray *_lineStyles)
{

	fillStyles = _fillStyles;
	lineStyles = _lineStyles;

	nFillBits = FSWFStream::MinBits(fillStyles->Size(), 0);
	nLineBits = FSWFStream::MinBits(lineStyles->Size(), 0);
}

//	Deleted the fill style array, line style array, and shape records.

FShapeWStyle::~FShapeWStyle(void)
{

	delete fillStyles;
	fillStyles = NULL;

	delete lineStyles;
	lineStyles = NULL;

	while (!shapeRecs.empty()) {

		delete shapeRecs.back();
		shapeRecs.pop_back();
	}
}

//	Adds a shape record to the end of the shape record list.

void FShapeWStyle::AddShapeRec(FShapeRec *shapeRec)
{

	shapeRecs.push_back(shapeRec);
}

U32 FShapeWStyle::NumFillBits()
{

	return nFillBits;
}

U32 FShapeWStyle::NumLineBits()
{

	return nLineBits;
}

//////////////////////////////////////////////////////////////////////

void FShapeWStyle::changeColor(const std::map<TPixel, TPixel> &table)
{
	unsigned int i, j;
	if (fillStyles)
		for (i = 0; i < fillStyles->Size(); i++) {
			FAFillStyle *style = fillStyles->get(i);
			//FColor* color = ((FFillStyleSolid*)style)->getColor();
			if (style->IsSolidStyle()) {
				TPixel oldColor = fcolor2tpixel(*(((FFillStyleSolid *)style)->getColor()));
				std::map<TPixel, TPixel>::const_iterator it = table.find(oldColor);
				if (it != table.end())
					((FFillStyleSolid *)style)->setColor(new FColor(tpixel2fcolor(it->second)));
			}
		}

	for (i = 0; i < shapeRecs.size(); i++) {
		FShapeRec *rec = shapeRecs[i];
		if (rec->isFShapeRecChange() && ((FShapeRecChange *)rec)->GetFillStyles())
			for (j = 0; j < ((FShapeRecChange *)rec)->GetFillStyles()->Size(); j++) {
				FAFillStyle *style = ((FShapeRecChange *)rec)->GetFillStyles()->get(j);
				//FColor* color = ((FFillStyleSolid*)style)->getColor();
				if (style->IsSolidStyle()) {
					TPixel oldColor = fcolor2tpixel(*(((FFillStyleSolid *)style)->getColor()));
					std::map<TPixel, TPixel>::const_iterator it = table.find(oldColor);
					if (it != table.end())
						((FFillStyleSolid *)style)->setColor(new FColor(tpixel2fcolor(it->second)));
				}
			}
	}
}

/////////////////////////////////////////////////////////////////////

void FShapeWStyle::changeColor(const FColor &oldColor, const FColor &newColor)
{
	unsigned int i, j;
	if (fillStyles)
		for (i = 0; i < fillStyles->Size(); i++) {
			FAFillStyle *style = fillStyles->get(i);
			FColor *color = ((FFillStyleSolid *)style)->getColor();
			if (style->IsSolidStyle() && *color == oldColor)
				((FFillStyleSolid *)style)->setColor(new FColor(newColor));
			color = ((FFillStyleSolid *)style)->getColor();
		}

	for (i = 0; i < shapeRecs.size(); i++) {
		FShapeRec *rec = shapeRecs[i];
		if (rec->isFShapeRecChange() && ((FShapeRecChange *)rec)->GetFillStyles())
			for (j = 0; j < ((FShapeRecChange *)rec)->GetFillStyles()->Size(); j++) {
				FAFillStyle *style = ((FShapeRecChange *)rec)->GetFillStyles()->get(j);
				FColor *color = ((FFillStyleSolid *)style)->getColor();
				if (style->IsSolidStyle() && *color == oldColor)
					((FFillStyleSolid *)style)->setColor(new FColor(newColor));
				color = ((FFillStyleSolid *)style)->getColor();
			}
	}
}

/////////////////////////////////////////////////////////////////////

//	Writes the shape with style to the given _SWFStream.

void FShapeWStyle::WriteToSWFStream(FSWFStream *_SWFStream)
{

	fillStyles->WriteToSWFStream(_SWFStream);
	lineStyles->WriteToSWFStream(_SWFStream);

	_SWFStream->WriteBits(nFillBits, 4);
	_SWFStream->WriteBits(nLineBits, 4);

	//write the shape records
	std::vector<FShapeRec *>::iterator cursor;

	for (cursor = shapeRecs.begin(); cursor != shapeRecs.end(); cursor++) {
		(*cursor)->IncludeNFillBitInfo(nFillBits); //vincenzo
		(*cursor)->IncludeNLineBitInfo(nLineBits); //vincenzo

		(*cursor)->WriteToSWFStream(_SWFStream);
		if ((*cursor)->isFShapeRecChange())										   //vincenzo
			((FShapeRecChange *)(*cursor))->getFillLineBits(nFillBits, nLineBits); //vincenzo
	}
	_SWFStream->FlushBits();
}
