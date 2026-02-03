// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTButtons.h

	This header-file contains the declarations of all low-level button-related classes. 
	Their parent classes are in the parentheses:

		class FAButtonRecord;
		class FButtonRecord1; (public FAButtonRecord)
		class FButtonRecord2; (public FAButtonRecord)
		class FButtonRecordList;
		class FDTDefineButton; (public FDT)
		class FDTDefineButton2; (public FDT)
		class FDTDefineButtonCXForm; (public FDT)
		class FDTDefineButtonSound. (public FDT)

****************************************************************************************/

#ifndef _F_DTBUTTONS_H_
#define _F_DTBUTTONS_H_

#include "FDT.h"

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TFLASH_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class FMatrix;
class FACXForm;
class FActionRecord;
class FActionCondition;
class FCXForm;
class FSoundInfo;

// Specifies appearance aspects for a button definition

class DVAPI FAButtonRecord
{

public:
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;

	virtual ~FAButtonRecord() {}
};

// Specifies appearance aspects for a button definition (flash 1.0)

class DVAPI FButtonRecord1 : public FAButtonRecord
{

public:
	FButtonRecord1(U8 _hit, U8 _down, U8 _over, U8 _up, U16 _layer, FMatrix *_matrix);
	virtual ~FButtonRecord1();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 hit;
	U8 down;
	U8 over;
	U8 up;
	U16 layer;
	FMatrix *matrix;
	U16 characterID;
};

// Specifies appearance aspects for a button definition (flash 3.0)

class DVAPI FButtonRecord2 : public FAButtonRecord
{

public:
	FButtonRecord2(U8 _hit, U8 _down, U8 _over, U8 _up, U16 _characterID, U16 _layer, FMatrix *_matrix, FACXForm *_colorTransform);
	virtual ~FButtonRecord2();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 hit;
	U8 down;
	U8 over;
	U8 up;
	U16 layer;
	FMatrix *matrix;
	FACXForm *colorTransform;
	U16 characterID;
};

// a list of button records

class DVAPI FButtonRecordList
{

public:
	FButtonRecordList();
	virtual ~FButtonRecordList();
	void AddRecord(FAButtonRecord *_buttonRecord);
	int Size();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	std::list<FAButtonRecord *> listOfButtonRecords;
};

// a flash object that defines a button in a SWF movie (flash 1.0)

class DVAPI FDTDefineButton : public FDT
{

public:
	FDTDefineButton(void);
	virtual ~FDTDefineButton();
	U16 ID(void);
	void AddButtonRecord(FButtonRecord1 *_buttonRecord);
	void AddActionRecord(FActionRecord *_actionRecord);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	std::list<FActionRecord *> listOfActionRecords;
	std::list<FButtonRecord1 *> listOfButtonRecords;
};

// a flash object that defines a button in a SWF movie (flash 3.0)

class DVAPI FDTDefineButton2 : public FDT
{

public:
	FDTDefineButton2(U8 _menuFlag);
	virtual ~FDTDefineButton2(void);
	U16 ID(void);
	void AddButtonRecord(FButtonRecord2 *_buttonRecord);
	void AddActionCondition(FActionCondition *_actionCondition);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	virtual void SetID(U16 id) { characterID = id; }

private:
	U16 characterID;
	U8 menuFlag;
	std::list<FActionCondition *> conditionList;
	std::list<FButtonRecord2 *> listOfButtonRecords;
};

//A flash object that defines a color transformation on a button

class DVAPI FDTDefineButtonCXForm : public FDT
{

public:
	FDTDefineButtonCXForm(U16 _characterID, FCXForm *_colorTransform);
	virtual ~FDTDefineButtonCXForm();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	FCXForm *colorTransform;
};

class DVAPI FDTDefineButtonSound : public FDT
{

public:
	FDTDefineButtonSound(U16 _buttonID, U16 _soundID0, FSoundInfo *_soundInfo0,
						 U16 _soundID1, FSoundInfo *_soundInfo1,
						 U16 _soundID2, FSoundInfo *_soundInfo2,
						 U16 _soundID3, FSoundInfo *_soundInfo3);
	virtual ~FDTDefineButtonSound();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 buttonID;
	U16 soundID0;
	U16 soundID1;
	U16 soundID2;
	U16 soundID3;
	FSoundInfo *soundInfo0;
	FSoundInfo *soundInfo1;
	FSoundInfo *soundInfo2;
	FSoundInfo *soundInfo3;
};

#endif
