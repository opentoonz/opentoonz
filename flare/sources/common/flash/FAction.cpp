// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FAction.cpp

  This source file contains the definition for the low-level action-related functions, 
  grouped by classes, which are all derived from class FAction (except FActionCondition):


		Class					Member Function

	FActionGetURL 			FActionGetURL(FString*, FString*);
							~FActionGetURL();
							void WriteToSWFStream(FSWFStream*);

	FActionGetURL2			FActionGetURL2(U8);
							void WriteToSWFStream(FSWFStream*);

	FActionGotoFrame		FActionGotoFrame(U16);
							void WriteToSWFStream(FSWFStream*);

	FActionGotoFrame2		FActionGotoFrame2(U8);
							void WriteToSWFStream(FSWFStream*);

	FActionGotoLabel		FActionGotoLabel(FString*);
							~FActionGotoLabel();
							void WriteToSWFStream(FSWFStream*);

	FActionIf				FActionIf(S16);
							void WriteToSWFStream(FSWFStream*);

	FActionJump				FActionJump(S16);
							void WriteToSWFStream(FSWFStream*);

	FActionPush				FActionPush(FString*);
							FActionPush(FLOAT);
							void WriteToSWFStream(FSWFStream*);

	FActionSetTarget		FActionSetTarget(FString*);
							~FActionSetTarget();
							void WriteToSWFStream(FSWFStream*);

	FActionWaitForFrame		FActionWaitForFrame(U16, U16);
							void WriteToSWFStream(FSWFStream*);

	FActionWaitForFrame2	FActionWaitForFrame2(U8);
							void WriteToSWFStream(FSWFStream*);

	FActionCondition		FActionCondition();
							~FActionCondition();
							Clear();
							AddActionRecord(FActionRecord*);
							AddKeyCode(U32);
							AddCondition(U16);
							void WriteToSWFStream(FSWFStream*);
							void WriteToSWFStream(FSWFStream*, int);


	Functions of these classes have not been implemented yet:

		class FActionAdd;
		class FActionAnd;
		class FActionAsciiToChar;
		class FActionCall;
		class FActionCharToAscii;
		class FActionCloneSprite;
		class FActionDivide;
		class FActionEndDrag;
		class FActionEquals;
		class FActionGetProperty;
		class FActionGetTime;
		class FActionGetVariable;
		class FActionLess;
		class FActionMBAsciiToChar;
		class FActionMBCharToAscii;
		class FActionMBStringExtract;
		class FActionMBStringLength;
		class FActionMultiply;
		class FActionNextFrame;
		class FActionNot;
		class FActionOr;
		class FActionPlay;
		class FActionPop;
		class FActionPrevFrame;
		class FActionRandomNumber;
		class FActionRemoveSprite;
		class FActionSetProperty;
		class FActionSetTarget2;
		class FActionSetVariable;
		class FActionStartDrag;
		class FActionStop;
		class FActionStopSounds;
		class FActionStringAdd;
		class FActionStringEquals;
		class FActionStringExtract;
		class FActionStringLength;
		class FActionStringLess;
		class FActionSubtract;
		class FActionToggleQuality;
		class FActionToInteger;
		class FActionTrace.


****************************************************************************************/

#include "FPrimitive.h"
#include "FAction.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionUnparsed ----------------------------------------------------------

FActionUnparsed::FActionUnparsed(UCHAR _code, USHORT _length, UCHAR *_data) : code(_code)
{
	if (code >= 0x80)
		length = _length, data = _data;
	else
		length = 0, data = 0;
}

void FActionUnparsed::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->WriteByte(code);
	if (code >= 0x80) {
		_SWFStream->WriteWord(length);
		for (int i = 0; i < length; i++)
			_SWFStream->WriteByte(data[i]);
	}
}
//////////////////////////////////////////////////////////////////////////////////////

FActionGetURL::FActionGetURL(FString *_url, FString *_window)
{
	url = _url;
	window = _window;
}

FActionGetURL::~FActionGetURL()
{
	delete url;
	delete window;
}

void FActionGetURL::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream body;

	url->WriteToSWFStream(&body, true);

	window->WriteToSWFStream(&body, true);

	_SWFStream->WriteByte(sactionGetURL);
	_SWFStream->WriteWord(body.Size());

	_SWFStream->Append(&body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionGetURL2 ---------------------------------------------------------

FActionGetURL2::FActionGetURL2(U8 _method)
{
	method = _method;
}

void FActionGetURL2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//sactionEquals: enumerated in "Macromedia.h"
	_SWFStream->WriteByte(sactionGetURL2);

	//write the length since the high bit is set in the action tag
	_SWFStream->WriteWord(1);

	//write the method
	_SWFStream->WriteByte(method);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionGotoFrame -------------------------------------------------------

FActionGotoFrame::FActionGotoFrame(U16 _frameIndex)
{
	frameIndex = _frameIndex;
}

void FActionGotoFrame::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//enumerated constant (see "Macromedia.h")
	_SWFStream->WriteByte(sactionGotoFrame);

	_SWFStream->WriteWord(2);

	_SWFStream->WriteWord((U32)frameIndex);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionGotoFrame2 ------------------------------------------------------

//	Constructor takes in play flag.

FActionGotoFrame2::FActionGotoFrame2(U8 _play)
{

	play = _play;
}

void FActionGotoFrame2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//sactionGotoFrame2: enumerated in "Macromedia.h"
	_SWFStream->WriteByte(sactionGotoFrame2);

	//write the length since the high bit is set in the action tag
	_SWFStream->WriteWord(1);

	//write the play flag
	_SWFStream->WriteByte(play);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionGotoLabel -------------------------------------------------------

FActionGotoLabel::FActionGotoLabel(FString *_label)
{
	label = _label;
}

FActionGotoLabel::~FActionGotoLabel()
{

	delete label;
}

void FActionGotoLabel::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//enumerated type (see "Macromedia.h")
	_SWFStream->WriteByte(sactionGotoLabel);

	label->WriteToSWFStream(_SWFStream, true);
}

///////////////////////////////////////////////////////////////////////////////
//  --------  FActionIf -------------------------------------------------------

//	Constructor records the branch offset.

FActionIf::FActionIf(S16 _branchOffset)
{

	branchOffset = _branchOffset;
}

void FActionIf::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//enumerations in "Macromedia.h"
	_SWFStream->WriteByte(sactionIf);

	//write the length since the high bit is set in the action tag
	_SWFStream->WriteWord(2);

	//write the offset
	_SWFStream->WriteWord((U32)branchOffset);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionJump -------------------------------------------------------

//	Constructor records the branch offset.

FActionJump::FActionJump(S16 _branchOffset)
{

	branchOffset = _branchOffset;
}

void FActionJump::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//enumerations in "Macromedia.h"
	_SWFStream->WriteByte(sactionJump);

	//write the length since the high bit is set in the action tag
	_SWFStream->WriteWord(2);

	//write the offset
	_SWFStream->WriteWord((U32)branchOffset);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionPush -------------------------------------------------------
//	Constructor for string push.

FActionPush::FActionPush(FString *_string)
{

	string = _string;
	number = 0;
	cValue = 0;
	dValue = 0;
	type = 0;
}

FActionPush::FActionPush(FLOAT _number)
{

	number = _number;
	string = 0;
	cValue = 0;
	dValue = 0;
	type = 1;
}

//	Constructor for float push.

FActionPush::FActionPush(U8 _type, FLOAT _number)
{

	number = _number;
	string = 0;
	cValue = 0;
	dValue = 0;
	type = _type;
}

FActionPush::FActionPush(double _number)
{
	number = 0;
	string = 0;
	cValue = 0;
	dValue = _number;
	type = 6;
}

FActionPush::FActionPush(U8 _type, U8 _value)
{
	number = 0;
	string = 0;
	cValue = _value;
	dValue = 0;
	type = _type;
}

void FActionPush::WriteToSWFStream(FSWFStream *_SWFStream)
{

	//sactionPush: enumerated in "Macromedia.h"
	_SWFStream->WriteByte(sactionPush);

	U16 size = 0;
	FSWFStream temp;
	// un byte c'e' sempre per memorizzare il tipo
	switch (type) {
	case 0:
		size = 1 + string->Length() + 1; //bisogna tener conto che la deve essere null terminated
		string->WriteToSWFStream(&temp, true);
		break;
	case 1:
	case 7:
		size = 5;
		temp.WriteDWord((U32)number);
		break;
	case 4:
	case 5:
	case 8:
		size = 2;
		temp.WriteByte(cValue);
		break;
	case 6: {
		size = 9;
		U8 *bPtr = (U8 *)&dValue;
#ifndef __LP64__
		temp.WriteByte((U32)(bPtr + 4));
		temp.WriteByte((U32)(bPtr + 5));
		temp.WriteByte((U32)(bPtr + 6));
		temp.WriteByte((U32)(bPtr + 7));
		temp.WriteByte((U32)bPtr);
		temp.WriteByte((U32)(bPtr + 1));
		temp.WriteByte((U32)(bPtr + 2));
		temp.WriteByte((U32)(bPtr + 3));
#else
		typedef unsigned long U64;
		temp.WriteByte((U64)(bPtr + 4));
		temp.WriteByte((U64)(bPtr + 5));
		temp.WriteByte((U64)(bPtr + 6));
		temp.WriteByte((U64)(bPtr + 7));
		temp.WriteByte((U64)bPtr);
		temp.WriteByte((U64)(bPtr + 1));
		temp.WriteByte((U64)(bPtr + 2));
		temp.WriteByte((U64)(bPtr + 3));
#endif
		break;
	}
	default:
		break;
	}
	_SWFStream->WriteWord(size);

	//write the type
	_SWFStream->WriteByte((U32)type);

	//write the object to be pushed
	_SWFStream->Append(&temp);
}

/*//originale
//  Writes to the given FSWFStream.
void FActionPush::WriteToSWFStream(FSWFStream *_SWFStream){

	//sactionPush: enumerated in "Macromedia.h"
	_SWFStream->WriteByte(sactionPush);

  // un byte c'e' sempre per memorizzare il tipo
	//since tag's high bit is set, must write the length
	if ( type == 0 ) {
		_SWFStream->WriteWord( 1 + (U32)string->Length() + 1); //bisogna tener conto che la deve essere null terminated
	} else {
		_SWFStream->WriteWord( 1 + 4 ); //scrive una Dword => 4 bytes
	}
	
	//write the type
	_SWFStream->WriteByte((U32)type);
	
	//write the object to be pushed
	if ( type == 0 ) {
		string->WriteToSWFStream(_SWFStream, true);
	} else {
		_SWFStream->WriteDWord( (U32)number );
	}
}
*/

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionSetTarget -------------------------------------------------------
FActionSetTarget::FActionSetTarget(FString *_targetName)
{
	targetName = _targetName;
}

FActionSetTarget::~FActionSetTarget()
{
	delete targetName;
}

void FActionSetTarget::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->WriteByte(sactionSetTarget);
	_SWFStream->WriteWord(targetName->Length() + 1);
	targetName->WriteToSWFStream(_SWFStream, true);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionWaitForFrame ----------------------------------------------------
FActionWaitForFrame::FActionWaitForFrame(U16 index, U16 count)
{
	//	frameIndex = frameIndex;
	//	skipCount = skipCount;
	frameIndex = index; // fixed from DV
	skipCount = count;
}

void FActionWaitForFrame::WriteToSWFStream(FSWFStream *_SWFStream)
{

	// enumerations in "Macromedia.h"
	_SWFStream->WriteByte(sactionWaitForFrame);

	_SWFStream->WriteWord(3);
	_SWFStream->WriteWord((U32)frameIndex);
	_SWFStream->WriteByte((U32)skipCount);
}

///////////////////////////////////////////////////////////////////////////////////////
//  --------  FActionWaitForFrame2 ----------------------------------------------------

FActionWaitForFrame2::FActionWaitForFrame2(U8 _skipCount)
{

	skipCount = _skipCount;
}

void FActionWaitForFrame2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	// enumerations in "Macromedia.h"
	_SWFStream->WriteByte(sactionWaitForFrame2);

	//write the length since high bit in tag id is set
	_SWFStream->WriteWord(1);

	//write the skip count
	_SWFStream->WriteByte(skipCount);
}

//	Constructor.  Initially, there are no actions or conditions so the conditions flag
//	equals zero, and the action record list is empty.

FActionCondition::FActionCondition()
{

	conditionFlags = 0;
}

//	Deletes all of the action records in the action record list.

FActionCondition::~FActionCondition()
{

	while (!actionRecordList.empty()) {

		delete actionRecordList.front();
		actionRecordList.pop_front();
	}
}

void FActionCondition::Clear()
{
	while (!actionRecordList.empty()) {

		delete actionRecordList.front();
		actionRecordList.pop_front();
	}
}

//	Adds an action record to the action record list

void FActionCondition::AddActionRecord(FActionRecord *_ActionRecord)
{

	actionRecordList.push_back(_ActionRecord);
}

//	Adds the key code information to the action condition (Flash 4).

void FActionCondition::AddKeyCode(U32 keyCode)
{

	conditionFlags |= (keyCode << 9);
}

//	Adds a condition to the list of conditions for which the actions in the action record list
//	will be taken.  Sees what condition is passed, and sets the bit which corresponds to the
//  condition in the conditionFlags field to 1.

void FActionCondition::AddCondition(U16 _condition)
{

	switch (_condition)

	{

	case OverDownToIdle:
		conditionFlags = conditionFlags | 256; //this bitwise OR has the same effect as making the 8th bit a "1"
		break;
	case IdleToOverDown:
		conditionFlags = conditionFlags | 128; //makes 9th bit a "1" ... etc.
		break;
	case OutDownToIdle:
		conditionFlags = conditionFlags | 64;
		break;
	case OutDownToOverDown:
		conditionFlags = conditionFlags | 32;
		break;
	case OverDownToOutDown:
		conditionFlags = conditionFlags | 16;
		break;
	case OverDownToOverUp:
		conditionFlags = conditionFlags | 8;
		break;
	case OverUpToOverDown:
		conditionFlags = conditionFlags | 4;
		break;
	case OverUpToIdle:
		conditionFlags = conditionFlags | 2;
		break;
	case IdleToOverUp:
		conditionFlags = conditionFlags | 1;
	}
}

//	Writes the action condition to the given FSWFStream.  Since the action condition contains
//	an offset field to the next action condition, the body of the action condition must first
//  be written to a temporary stream so that size can be calculated for the offset.

void FActionCondition::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream temp;

	temp.WriteWord((U32)conditionFlags);
	int boh = temp.Size();

	std::list<FActionRecord *>::iterator cursor;
	for (cursor = actionRecordList.begin(); cursor != actionRecordList.end(); cursor++) {

		(*cursor)->WriteToSWFStream(&temp);
		boh = temp.Size();
	}

	//add 2 to the size because the offset is actually to the beginning of the
	//next action condition's conditionFlags
	_SWFStream->WriteWord(2 + temp.Size());

	_SWFStream->Append(&temp);
}

//	Writes the action condition to the given stream.  The lastFlag indicates that this is the
//	last action condition so the offset is set to 0 by default.

void FActionCondition::WriteToSWFStream(FSWFStream *_SWFStream, int /*lastFlag*/)
{

	_SWFStream->WriteWord(0); //the offset

	_SWFStream->WriteWord((U32)conditionFlags);

	std::list<FActionRecord *>::iterator cursor;
	for (cursor = actionRecordList.begin(); cursor != actionRecordList.end(); cursor++) {

		(*cursor)->WriteToSWFStream(_SWFStream);
	}

	//_SWFStream->FlushBits();

	_SWFStream->WriteByte((U32)0);
}

/*	
FActionConditionList::FActionConditionList(){
}


FActionConditionList::~FActionConditionList(){
	
	while (!conditionList.empty()){

		delete conditionList.front();

		conditionList.pop_front();
	
	}

}


void FActionConditionList::AddActionCondition(FActionCondition* _ActionCondition){
	
	conditionList.push_back(_ActionCondition);

}

U32 FActionConditionList::Size(){

	return conditionList.size();
	
 }


 void FActionConditionList::WriteToSWFStream(FSWFStream* _SWFStream){

	std::list<FActionCondition*>::iterator cursor;
	std::list<FActionCondition*>::iterator cursor2;
	cursor2 = (conditionList.end());
	cursor2--;
	
	for (cursor = conditionList.begin();    cursor != cursor2;      cursor++) {
	 
		(*cursor)->WriteToSWFStream(_SWFStream);
	
	}

	(*cursor)->WriteToSWFStream(_SWFStream, 1); //flag indicating it is the last action condition

}
*/

//=============================================================================
//                        ALCUNE AZIONI FLASH 5
//=============================================================================

FActionConstantPool::FActionConstantPool(FString *string)
{
	constantList.push_back(string);
}

FActionConstantPool::~FActionConstantPool()
{
	while (!constantList.empty()) {
		delete constantList.front();
		constantList.pop_front();
	}
}

void FActionConstantPool::addConstant(FString *string)
{
	constantList.push_back(string);
}

void FActionConstantPool::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream temp;
	std::list<FString *>::iterator it = constantList.begin();
	while (it != constantList.end()) {
		(*it)->WriteToSWFStream(&temp, true);
		++it;
	}

	_SWFStream->WriteByte(sactionConstantPool);
	_SWFStream->WriteWord(temp.FullSize() + 2);
	_SWFStream->WriteWord(constantList.size());
	_SWFStream->Append(&temp);
}

//=============================================================================
//                        CLIP ACTION / CLIP ACTION RECORD
//=============================================================================

FClipAction::FClipAction()
{
	eventFlags = 0;
}

FClipAction::~FClipAction()
{
	Clear();
}

void FClipAction::Clear()
{
	while (!clipActionRecordList.empty()) {
		delete clipActionRecordList.front();
		clipActionRecordList.pop_front();
	}
}

void FClipAction::AddClipActionRecord(FClipActionRecord *_clipActionRecord)
{
	clipActionRecordList.push_back(_clipActionRecord);
}

void FClipAction::AddFlags(U32 _flags)
{
	//e' meglio definire i flags come enumerazione che sono gia' i valori da
	// mettere in OR per settare l'abilitazione del particolare evento
	eventFlags |= _flags;
}

void FClipAction::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream temp;

	//Reserved
	temp.WriteWord(0);

	//allEvent
	temp.WriteDWord(eventFlags);

	//tutti i clip
	std::list<FClipActionRecord *>::iterator it = clipActionRecordList.begin();
	int i = 0;
	while (it != clipActionRecordList.end()) {
		(*it)->WriteToSWFStream(&temp); //, i);//prova zozza
		++it;
		++i;
	}

	//end clipAction
	temp.WriteDWord(0);
	_SWFStream->Append(&temp);
}

//=============================================================================

FClipActionRecord::FClipActionRecord()
{
	eventFlags = 0;
	keyCode = 0;
}

FClipActionRecord::~FClipActionRecord()
{
	Clear();
}

void FClipActionRecord::Clear()
{
	while (!actionRecordList.empty()) {
		delete actionRecordList.front();
		actionRecordList.pop_front();
	}
}

void FClipActionRecord::AddActionRecord(FActionRecord *_actionRecord)
{
	actionRecordList.push_back(_actionRecord);
}

void FClipActionRecord::AddKeyCode(U8 _keyCode)
{
	keyCode |= _keyCode;
}

void FClipActionRecord::AddFlags(U32 _flags)
{
	//e' meglio definire i flags come enumerazione che sono gia' i valori da
	// mettere in OR per settare l'abilitazione del particolare evento
	eventFlags |= _flags;
}

void FClipActionRecord::WriteToSWFStream(FSWFStream *_SWFStream)
{
	FSWFStream temp;
	//metto tutte le azioni su per sapere la size
	std::list<FActionRecord *>::iterator it = actionRecordList.begin();
	while (it != actionRecordList.end()) {
		(*it)->WriteToSWFStream(&temp);
		++it;
	}
	temp.WriteByte(sactionNone);

	//e' un offset deve andare a quello dopo
	U32 size = temp.FullSize();

	FSWFStream temp1;
	temp1.WriteDWord(eventFlags);

	if ((eventFlags & ClipEventKeyPress) == ClipEventKeyPress) {
		temp1.WriteDWord(size + 1);
		temp1.WriteByte(keyCode);
	} else
		temp1.WriteDWord(size);

	temp1.Append(&temp);

	_SWFStream->Append(&temp1);
}
