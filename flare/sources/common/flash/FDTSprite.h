// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FDTSprite.h

	This header-file contains the declarations of low-level sprite-related class.
	Its parent class is in the parentheses:

		class FDTSprite; (public FDT)

****************************************************************************************/

#ifndef _SPRITE_H_
#define _SPRITE_H_

#include <list>
#include "Macromedia.h"
#include "FDT.h"

//! Defines a low-level sprite object.
/*! A sprite is a flash object that acts as a "movie within a movie".
	\sa FDT
*/
class FDTSprite : public FDT
{
public:
	//! Construct a low-level sprite object.
	/*! */
	FDTSprite();

	//! Destruct a low-level sprite object.
	/*! */
	virtual ~FDTSprite();

	// Method for internal use.
	void AddFObj(FObj *_object);

	// Method for internal use.
	U16 ID() { return characterID; }

	// Method for internal use.
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	std::list<FObj *> objectList;
	U32 numOfFrames;
};

#endif
