// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTButtons.cpp

  This source file contains the definition for all low-level button-related functions, 
  grouped by classes:
  
		Class					Member Function

	FButtonRecord1			FButtonRecord1(U8, U8, U8, U8, U16, FMatrix*);
	FButtonRecord1			~FButtonRecord1();
							void WriteToSWFStream(FSWFStream*);

	FButtonRecord2			FButtonRecord2(U8, U8, U8, U8, U16, U16, FMatrix*, FACXForm*);
							~FButtonRecord2();
							void WriteToSWFStream(FSWFStream*);

	FButtonRecordList		FButtonRecordList();
							~FButtonRecordList();
							void AddRecord(FAButtonRecord*);
							int Size();
							void WriteToSWFStream(FSWFStream*);

	FDTDefineButton			FDTDefineButton(void);
							~FDTDefineButton();
							U16 ID(void);
							void AddButtonRecord(FButtonRecord1*);
							void AddActionRecord(FActionRecord*);
							void WriteToSWFStream(FSWFStream*);

	FDTDefineButton2		FDTDefineButton2(U8);
							~FDTDefineButton2(void);
							U16 ID(void);
							void AddButtonRecord(FButtonRecord2*);
							void AddActionCondition(FActionCondition*);
							void WriteToSWFStream(FSWFStream*);

	FDTDefineButtonCXForm	FDTDefineButtonCXForm(U16, FCXForm*);
							~FDTDefineButtonCXForm();
							void WriteToSWFStream(FSWFStream*);

	FDTDefineButtonSound	FDTDefineButtonSound(U16, U16, FSoundInfo*, U16, FSoundInfo*,
												      U16, FSoundInfo*, U16, FSoundInfo*);
							~FDTDefineButtonSound();
							void WriteToSWFStream(FSWFStream*);

****************************************************************************************/
#ifdef WIN32
#pragma warning(disable : 4786)
#endif

#include "FPrimitive.h"
#include "FDTButtons.h"
#include "FAction.h"
#include "FDTShapes.h"
#include "FDTSounds.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FButtonRecord1 ---------------------------------------------------------

FButtonRecord1::FButtonRecord1(U8 _hit, U8 _down, U8 _over, U8 _up, U16 _layer, FMatrix *_matrix)
{
	hit = _hit;
	down = _down;
	over = _over;
	up = _up;
	layer = _layer;
	matrix = _matrix;
	characterID = FObjCollection::Increment();
}

FButtonRecord1::~FButtonRecord1()
{
	delete matrix;
}

void FButtonRecord1::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->WriteBits(0, 4);
	_SWFStream->WriteBits(hit, 1);
	_SWFStream->WriteBits(down, 1);
	_SWFStream->WriteBits(over, 1);
	_SWFStream->WriteBits(up, 1);
	_SWFStream->WriteWord(characterID);
	_SWFStream->WriteWord(layer);
	matrix->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FButtonRecord2 ---------------------------------------------------------

FButtonRecord2::FButtonRecord2(U8 _hit, U8 _down, U8 _over, U8 _up, U16 _characterID, U16 _layer, FMatrix *_matrix, FACXForm *_colorTransform)
{
	hit = _hit;
	down = _down;
	over = _over;
	up = _up;
	characterID = _characterID;
	layer = _layer;
	matrix = _matrix;
	colorTransform = _colorTransform;
}

FButtonRecord2::~FButtonRecord2()
{
	delete matrix;
	delete colorTransform;
}

void FButtonRecord2::WriteToSWFStream(FSWFStream *_SWFStream)
{
	_SWFStream->FlushBits();
	_SWFStream->WriteBits(0, 4);
	_SWFStream->WriteBits(hit, 1);
	_SWFStream->WriteBits(down, 1);
	_SWFStream->WriteBits(over, 1);
	_SWFStream->WriteBits(up, 1);
	_SWFStream->WriteWord(characterID);
	_SWFStream->WriteWord(layer);

	matrix->WriteToSWFStream(_SWFStream);
	_SWFStream->FlushBits();
	colorTransform->WriteToSWFStream(_SWFStream);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FButtonRecordList ------------------------------------------------------

FButtonRecordList::FButtonRecordList() {}

FButtonRecordList::~FButtonRecordList()
{
	while (!listOfButtonRecords.empty()) {

		delete listOfButtonRecords.front();

		listOfButtonRecords.pop_front();
	}
}

void FButtonRecordList::AddRecord(FAButtonRecord *_buttonRecord)
{

	listOfButtonRecords.push_back(_buttonRecord);
}

int FButtonRecordList::Size()
{

	return listOfButtonRecords.size();
}

void FButtonRecordList::WriteToSWFStream(FSWFStream *_SWFStream)
{

	std::list<FAButtonRecord *>::iterator cursor;

	for (cursor = listOfButtonRecords.begin(); cursor != listOfButtonRecords.end(); cursor++) {

		(*cursor)->WriteToSWFStream(_SWFStream);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineButton --------------------------------------------------------

FDTDefineButton::FDTDefineButton(void)
{

	characterID = FObjCollection::Increment();
}

FDTDefineButton::~FDTDefineButton()
{

	// delete all entries in action record list
	while (!listOfActionRecords.empty()) {

		delete listOfActionRecords.front();
		listOfActionRecords.pop_front();
	}

	//delete all entries in button record list
	while (!listOfButtonRecords.empty()) {

		delete listOfButtonRecords.front();

		listOfButtonRecords.pop_front();
	}
}

U16 FDTDefineButton::ID(void)
{

	return characterID;
}

void FDTDefineButton::AddButtonRecord(FButtonRecord1 *_buttonRecord)
{

	listOfButtonRecords.push_back(_buttonRecord);
}

void FDTDefineButton::AddActionRecord(FActionRecord *_actionRecord)
{

	listOfActionRecords.push_back(_actionRecord);
}

void FDTDefineButton::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;
	body.WriteWord((U32)characterID);

	//write the button record list to the body
	std::list<FButtonRecord1 *>::iterator cursor;

	for (cursor = listOfButtonRecords.begin(); cursor != listOfButtonRecords.end(); cursor++) {

		(*cursor)->WriteToSWFStream(&body);
	}

	//write the end of button record flag
	body.WriteByte(0);

	//write the action record list to the body
	std::list<FActionRecord *>::iterator cursor2;
	for (cursor2 = listOfActionRecords.begin(); cursor2 != listOfActionRecords.end(); cursor2++) {

		(*cursor2)->WriteToSWFStream(&body);
	}

	//write the action end flag
	body.WriteByte(0);

	_SWFStream->AppendTag(stagDefineButton2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineButton2 -------------------------------------------------------

FDTDefineButton2::FDTDefineButton2(U8 _menuFlag)
{

	characterID = FObjCollection::Increment();
	menuFlag = _menuFlag;
}

FDTDefineButton2::~FDTDefineButton2(void)
{

	//delete all entries in conditionsList
	while (!conditionList.empty()) {

		delete conditionList.front();

		conditionList.pop_front();
	}

	//delete all entries in listOfButtonRecords
	while (!listOfButtonRecords.empty()) {

		delete listOfButtonRecords.front();

		listOfButtonRecords.pop_front();
	}
}

U16 FDTDefineButton2::ID(void)
{

	return characterID;
}

void FDTDefineButton2::AddButtonRecord(FButtonRecord2 *_buttonRecord)
{

	listOfButtonRecords.push_back(_buttonRecord);
}

void FDTDefineButton2::AddActionCondition(FActionCondition *_actionCondition)
{

	conditionList.push_back(_actionCondition);
}

void FDTDefineButton2::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)characterID);

	body.WriteByte((U32)menuFlag);

	FSWFStream buttonRecordStream;

	//write the list of button records to button record stream
	std::list<FButtonRecord2 *>::iterator cursor;

	for (cursor = listOfButtonRecords.begin(); cursor != listOfButtonRecords.end(); cursor++) {

		(*cursor)->WriteToSWFStream(&buttonRecordStream);
	}

	buttonRecordStream.WriteByte((U32)0);
	U32 offset = 0;
	if (!conditionList.empty())
		offset = buttonRecordStream.Size() + 2; //have to count the action offset also
	body.WriteWord(offset);
	body.Append(&buttonRecordStream);

	//write the list of action records
	if (!conditionList.empty()) {
		std::list<FActionCondition *>::iterator cursor1;
		std::list<FActionCondition *>::iterator cursor2;
		cursor2 = (conditionList.end());
		cursor2--;

		for (cursor1 = conditionList.begin(); cursor1 != cursor2; cursor1++) {

			(*cursor1)->WriteToSWFStream(&body);
		}

		(*cursor1)->WriteToSWFStream(&body, 1); //flag indicating it is the last action condition
	}

	_SWFStream->AppendTag(stagDefineButton2, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineButtonCXForm --------------------------------------------------

FDTDefineButtonCXForm::FDTDefineButtonCXForm(U16 _characterID, FCXForm *_colorTransform)
{
	characterID = _characterID;
	colorTransform = _colorTransform;
}

FDTDefineButtonCXForm::~FDTDefineButtonCXForm()
{

	delete colorTransform;
}

void FDTDefineButtonCXForm::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)characterID);
	colorTransform->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagDefineButtonCxform, body.Size(), &body);
}

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTDefineButtonSound ---------------------------------------------------

FDTDefineButtonSound::FDTDefineButtonSound(U16 _buttonID, U16 _soundID0, FSoundInfo *_soundInfo0,
										   U16 _soundID1, FSoundInfo *_soundInfo1,
										   U16 _soundID2, FSoundInfo *_soundInfo2,
										   U16 _soundID3, FSoundInfo *_soundInfo3)
{
	buttonID = _buttonID;
	soundID0 = _soundID0;
	soundInfo0 = _soundInfo0;
	soundID1 = _soundID1;
	soundInfo1 = _soundInfo1;
	soundID2 = _soundID2;
	soundInfo2 = _soundInfo2;
	soundID3 = _soundID3;
	soundInfo3 = _soundInfo3;
}

FDTDefineButtonSound::~FDTDefineButtonSound()
{

	delete soundInfo0;
	delete soundInfo1;
	delete soundInfo2;
	delete soundInfo3;
}

void FDTDefineButtonSound::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)buttonID);

	body.WriteWord((U32)soundID0);
	soundInfo0->WriteToSWFStream(&body);

	body.WriteWord((U32)soundID1);
	soundInfo1->WriteToSWFStream(&body);

	body.WriteWord((U32)soundID2);
	soundInfo2->WriteToSWFStream(&body);

	body.WriteWord((U32)soundID3);
	soundInfo3->WriteToSWFStream(&body);

	_SWFStream->AppendTag(stagDefineButtonSound, body.Size(), &body);
}
