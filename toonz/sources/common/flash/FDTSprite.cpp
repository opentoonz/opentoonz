// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTSprite.cpp

  This source file contains the definition for all low-level sprite-related functions,
  grouped by classes:

		Class						Member Function

	FDTSprite					FDTSprite();
								~FDTSprite();
								void AddFObj(FObj* );
								void WriteToSWFStream(FSWFStream* );


****************************************************************************************/
#ifdef WIN32
#pragma warning(disable : 4786)
#endif
#include "FSWFStream.h"
#include "FDTSprite.h"

//////////////////////////////////////////////////////////////////////////////////////
//  --------  FDTSprite  -------------------------------------------------------------

FDTSprite::FDTSprite()
{
	characterID = FObjCollection::Increment();
	numOfFrames = 0;
}

FDTSprite::~FDTSprite()
{
	while (!objectList.empty()) {

		delete objectList.front();
		objectList.pop_front();
	}
}

void FDTSprite::AddFObj(FObj *_object)
{
	if (_object->IsShowFrame())
		numOfFrames++;

	objectList.push_back(_object);
}

void FDTSprite::WriteToSWFStream(FSWFStream *_SWFStream)
{

	FSWFStream body;

	body.WriteWord((U32)characterID);

	body.WriteWord(numOfFrames);

	std::list<FObj *>::iterator cursor;
	for (cursor = objectList.begin(); cursor != objectList.end(); cursor++) {

		(*cursor)->WriteToSWFStream(&body);
	}

	// Put an end tag on the end of the temporary buffer:
	body.AppendTag(stagEnd, 0, 0);

	// put the buffer into the main stream
	_SWFStream->AppendTag(stagDefineSprite, body.Size(), &body);
}
