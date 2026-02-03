// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FCT.cpp

  This source file contains the definition for the low-level Control-Tag related functions,
  grouped by classes, which are all derived from class FCT:


		Class						Member Function

		FCTDoAction				FCTDoAction(void);
								~FCTDoAction();
								void AddAction(FActionRecord*);
								void WriteToSWFStream(FSWFStream*);

		FCTFrameLabel			FCTFrameLabel(FString*);
								~FCTFrameLabel();
								void WriteToSWFStream(FSWFStream*);

		FCTPlaceObject			FCTPlaceObject(U16, U16, FMatrix*, FCXForm*);
								~FCTPlaceObject();
								void WriteToSWFStream(FSWFStream*);

		FCTPlaceObject2			FCTPlaceObject2(U16, U16, U16, U16, U16, U16, FMatrix*,
												FCXForm*, U16, FString*, U16);
								~FCTPlaceObject2();
								void WriteToSWFStream(FSWFStream*);

		FCTProtect				FCTProtect();
								void WriteToSWFStream(FSWFStream*);

		FCTRemoveObject			FCTRemoveObject(U16, U16);
								void WriteToSWFStream(FSWFStream*);

		FCTRemoveObject2		FCTRemoveObject2(U16);
								void WriteToSWFStream(FSWFStream*);

		FCTSetBackgroundColor	FCTSetBackgroundColor(FColor*);
								~FCTSetBackgroundColor();
								void WriteToSWFStream(FSWFStream*);

		FCTShowFrame			FCTShowFrame();
								U32 IsShowFrame();
								void WriteToSWFStream(FSWFStream*);

		FCTStartSound			FCTStartSound(U16, FSoundInfo*);
								~FCTStartSound();
								void WriteToSWFStream(FSWFStream*);


****************************************************************************************/

#include "FCT.h"
#include "FDTShapes.h"
#include "FDTSounds.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCTDoAction ------------------------------------------------------------

FCTDoAction::FCTDoAction(void)
{
}

FCTDoAction::~FCTDoAction()
{

	while (!actionRecordList.empty()) {

		delete actionRecordList.front();

		actionRecordList.pop_front();
	}
}

void FCTDoAction::AddAction(FActionRecord *_actionRecord)
{

	actionRecordList.push_back(_actionRecord);
}

void FCTDoAction::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	std::list<FActionRecord *>::iterator cursor;
	for (cursor = actionRecordList.begin(); cursor != actionRecordList.end(); cursor++) {

		(*cursor)->WriteToSWFStream(&body);
	}

	body.WriteByte(0);

	_SWFStream->AppendTag(stagDoAction, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCTFrameLabel ----------------------------------------------------------

FCTFrameLabel::FCTFrameLabel(FString *_frameName)
{
	frameName = _frameName;
}

FCTFrameLabel::~FCTFrameLabel()
{
	delete frameName;
}

void FCTFrameLabel::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;
	frameName->WriteToSWFStream(&body, true);

	_SWFStream->AppendTag(stagFrameLabel, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCTPlaceObject ----------------------------------------------------------

FCTPlaceObject::FCTPlaceObject(U16 _characterID,
							   U16 _depth,
							   FMatrix *_matrix,
							   FCXForm *_colorTransform)
{
	characterID = _characterID;
	depth = _depth;
	matrix = _matrix;
	colorTransform = _colorTransform;

	FLASHASSERT(_colorTransform);
}

FCTPlaceObject::~FCTPlaceObject()
{
	delete matrix;
	delete colorTransform;
}
void FCTPlaceObject::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	;

	body.WriteWord((U32)characterID);

	body.WriteWord((U32)depth);

	matrix->WriteToSWFStream(&body);

	if (colorTransform) {
		colorTransform->WriteToSWFStream(&body);
	}

	_SWFStream->AppendTag(stagPlaceObject, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FCTPlaceObject2 --------------------------------------------------------

FCTPlaceObject2::FCTPlaceObject2(U16 _hasClipDepth,
								 U16 _hasRatio,
								 U16 _hasChar,
								 U16 _hasMove,
								 U16 _depth,
								 U16 _characterID,
								 FMatrix *_matrix,
								 FCXFormWAlpha *_colorTransform,
								 U16 _ratio,
								 FString *_name,
								 U16 _clipDepth)
{
	SetParameters(_hasClipDepth, _hasRatio, _hasChar, _hasMove, _depth, _characterID,
				  _matrix, _colorTransform, _ratio, _name, _clipDepth);
}

FCTPlaceObject2::FCTPlaceObject2(const FCTPlaceObject2 &obj)
{
	SetParameters(obj.hasClipDepth, obj.hasRatio, obj.hasCharID, obj.hasMove,
				  obj.depth, obj.characterID, 0, obj.colorTransform,
				  obj.ratio, 0, obj.clipDepth);
	matrix = obj.matrix ? new FMatrix(*(obj.matrix)) : 0;
	name = obj.name ? new FString(*(obj.name)) : 0;
}

void FCTPlaceObject2::SetParameters(U16 _hasClipDepth,
									U16 _hasRatio,
									U16 _hasChar,
									U16 _hasMove,
									U16 _depth,
									U16 _characterID,
									FMatrix *_matrix,
									FCXFormWAlpha *_colorTransform,
									U16 _ratio,
									FString *_name,
									U16 _clipDepth)
{
	hasClipAction = 0;
	hasClipDepth = _hasClipDepth;
	hasRatio = _hasRatio;
	hasCharID = _hasChar;
	hasMove = _hasMove;
	depth = _depth;
	characterID = _characterID;
	matrix = _matrix;
	colorTransform = _colorTransform;
	ratio = _ratio;
	name = _name;
	clipDepth = _clipDepth;
	clipAction = NULL;
}

FCTPlaceObject2::FCTPlaceObject2(U16 _hasClipAction,
								 U16 _hasClipDepth,
								 U16 _hasRatio,
								 U16 _hasChar,
								 U16 _hasMove,
								 U16 _depth,
								 U16 _characterID,
								 FMatrix *_matrix,
								 FCXFormWAlpha *_colorTransform,
								 U16 _ratio,
								 FString *_name,
								 U16 _clipDepth,
								 FClipAction *_clipAction)
{
	hasClipAction = _hasClipAction;
	hasClipDepth = _hasClipDepth;
	hasRatio = _hasRatio;
	hasCharID = _hasChar;
	hasMove = _hasMove;
	depth = _depth;
	characterID = _characterID;
	matrix = _matrix;
	colorTransform = _colorTransform;
	ratio = _ratio;
	name = _name;
	clipDepth = _clipDepth;
	clipAction = _clipAction;
}

FCTPlaceObject2::~FCTPlaceObject2()
{

	delete name;
	delete matrix;
	delete colorTransform;
	delete clipAction;
}

U32 FCTPlaceObject2::IsPlaceObject()
{

	return 1;
}

int FCTPlaceObject2::GetPlacedId()
{
	return hasCharID ? characterID : -1;
}

void FCTPlaceObject2::SetId(U16 id)
{
	characterID = id;
	hasMove = 0;
	hasCharID = 1;
}

void FCTPlaceObject2::ChangePlacedId(U16 id)
{
	if (hasCharID) {
		characterID = id;
		//hasMove=0;
	}
}

void FCTPlaceObject2::SetMatrix(FMatrix *_matrix)
{
	matrix = _matrix;
}

void FCTPlaceObject2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteBits(hasClipAction, 1); /*DM*/
	body.WriteBits(hasClipDepth, 1);  /*DM*/
	body.WriteBits((name != 0), 1);
	body.WriteBits(hasRatio, 1);
	body.WriteBits((colorTransform != 0), 1);
	body.WriteBits((matrix != 0), 1);
	body.WriteBits(hasCharID, 1);
	body.WriteBits(hasMove, 1);

	body.WriteWord(depth);

	if (hasCharID)
		body.WriteWord((U32)characterID);
	if (matrix)
		matrix->WriteToSWFStream(&body);
	if (colorTransform)
		colorTransform->WriteToSWFStream(&body);
	if (hasRatio)
		body.WriteWord((U32)ratio);
	if (name)
		name->WriteToSWFStream(&body, true);
	if (hasClipDepth)					/*DM*/
		body.WriteWord((U32)clipDepth); /*DM*/
	if (hasClipAction)
		clipAction->WriteToSWFStream(&body);

	body.FlushBits();

	_SWFStream->AppendTag(stagPlaceObject2, body.Size(), &body);
}

/////////////////////////////////////////////////////////////////////////////////
//  --------  FCTProtect --------------------------------------------------------

FCTProtect::FCTProtect()
{
}

void FCTProtect::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->AppendTag(stagProtect, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////
//  --------  FCTRemoveObject ---------------------------------------------------

FCTRemoveObject::FCTRemoveObject(U16 _characterID, U16 _depth)
{
	characterID = _characterID;
	depth = _depth;
}

void FCTRemoveObject::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	body.WriteWord((U32)characterID);
	body.WriteWord((U32)depth);

	_SWFStream->AppendTag(stagRemoveObject, body.Size(), &body);
}

/////////////////////////////////////////////////////////////////////////////////
//  --------  FCTRemoveObject2 --------------------------------------------------

FCTRemoveObject2::FCTRemoveObject2(U16 _depth)
{
	depth = _depth;
}

void FCTRemoveObject2::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;
	body.WriteWord((U32)depth);

	_SWFStream->AppendTag(stagRemoveObject2, body.Size(), &body);
}

/////////////////////////////////////////////////////////////////////////////////
//  --------  FCTSetBackgroundColor --------------------------------------------------

FCTSetBackgroundColor::FCTSetBackgroundColor(FColor *_color)
{
	color = _color;
	// here changed.
	// Background color is always opaque, i.e. no alpha channel.
	color->AlphaChannel(false);
}

FCTSetBackgroundColor::~FCTSetBackgroundColor()
{
	delete (color);
}

void FCTSetBackgroundColor::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	// here changed.
	color->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagSetBackgroundColor, body.Size(), &body);
}

/////////////////////////////////////////////////////////////////////////////////
//  --------  FCTUnparsedTag --------------------------------------------------

FCTUnparsedTag::FCTUnparsedTag(U16 _tagId, U32 _lenght, U8 *_data)
{
	lenght = _lenght;
	if (lenght > 0) {
		data = new U8[lenght];
		memcpy(data, _data, lenght);
	} else
		data = 0;

	tagId = _tagId;
}

FCTUnparsedTag::~FCTUnparsedTag()
{
	delete data;
}

void FCTUnparsedTag::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	// here changed.
	if (lenght > 0)
		body.WriteLargeData(data, lenght);

	_SWFStream->AppendTag(tagId, body.Size(), &body);
}

USHORT FCTUnparsedTag::GetID()
{
	if (tagId == stagDefineButton2 ||
		tagId == stagDefineButton ||
		tagId == stagDefineText ||
		tagId == stagDefineText2 ||
		tagId == stagDefineEditText ||
		tagId == stagDefineMorphShape ||
		tagId == stagDefineBitsLossless2 ||
		tagId == stagDefineBitsLossless ||
		tagId == stagDefineBitsJPEG3)
		return (USHORT)(data[0] | (data[1] << 8));
	return 0;
}

void FCTUnparsedTag::SetID(USHORT id)
{
	if (tagId == stagDefineButton2 ||
		tagId == stagDefineButton ||
		tagId == stagDefineText ||
		tagId == stagDefineText2 ||
		tagId == stagDefineEditText ||
		tagId == stagDefineMorphShape ||
		tagId == stagDefineBitsLossless2 ||
		tagId == stagDefineBitsLossless ||
		tagId == stagDefineBitsJPEG3) {
		data[0] = (id & 0xff);
		data[1] = (id >> 8);
	}
}

/////////////////////////////////////////////////////////////////////////////
//  --------  FCTShowFrame --------------------------------------------------

FCTShowFrame::FCTShowFrame()
{
}

U32 FCTShowFrame::IsShowFrame()
{

	return 1;
}

void FCTShowFrame::WriteToSWFStream(FSWFStream *_SWFStream)
{

	_SWFStream->AppendTag(stagShowFrame, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////
//  --------  FCTStartSound --------------------------------------------------

FCTStartSound::FCTStartSound(U16 _soundID, FSoundInfo *_soundInfo)
{

	soundID = _soundID;
	soundInfo = _soundInfo;
}

FCTStartSound::~FCTStartSound()
{

	delete soundInfo;
}

void FCTStartSound::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)soundID);
	soundInfo->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagStartSound, body.Size(), &body);
}
