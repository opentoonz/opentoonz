// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************

				File Summary: FCT.h

  This header-file contains the declarations of the low-level Control-Tag related classes:
  
	 1)	class FCT;
	 2)	all the derived classes of class FCT:
  
		class FCTDoAction;
		class FCTFrameLabel;
		class FCTPlaceObject;
		class FCTPlaceObject2;
		class FCTProtect;
		class FCTRemoveObject;
		class FCTRemoveObject2;
		class FCTSetBackgroundColor;
		class FCTShowFrame;
		class FCTStartSound;


****************************************************************************************/

#ifdef WIN32 // added from DV
#pragma warning(push)
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)

#endif

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif
#ifndef _F_C_T_H_
#define _F_C_T_H_

#include "Macromedia.h"
#include "FObj.h"
#include "FAction.h"
#include "FPrimitive.h"

class FCXForm;
class FCXFormWAlpha;

class FSoundInfo;

// A "control tag" type of flash object

class DVAPI FCT : public FObj
{
public:
	virtual ~FCT() {}
	virtual void WriteToSWFStream(FSWFStream * /*_SWFStream*/) {}
};

// flash object that directs the flash player to complete a given set of actions at frame completion

class DVAPI FCTDoAction : public FCT
{
public:
	FCTDoAction();
	virtual ~FCTDoAction();

	void AddAction(FActionRecord *_actionRecord);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	std::list<FActionRecord *> actionRecordList;
};

//  Associates a label with the frame.  This label can then be used
//	in the Go to Label Action.

class DVAPI FCTFrameLabel : public FCT
{

public:
	FCTFrameLabel(FString *_frameName);
	virtual ~FCTFrameLabel();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FString *frameName;
};

// a flash object that directs the player to add an object to the display list (flash 1.0)

class DVAPI FCTPlaceObject : public FCT
{
public:
	// if no color transform then _colorTransform argument must be null
	FCTPlaceObject(U16 _characterID,
				   U16 _depth,
				   FMatrix *_matrix,		  // always present
				   FCXForm *_colorTransform); // NULL if there is not a color transform
	virtual ~FCTPlaceObject();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 characterID;
	U16 depth;
	FMatrix *matrix;
	FCXForm *colorTransform;
};

class DVAPI FCTPlaceObject2 : public FCT
{
public:
	// if a certain flag is not true, you must provide a NULL value for its associated argument(s)
	FCTPlaceObject2(U16 _hasClipDepth, /*DM*/
					U16 _hasRatio,
					U16 _hasCharID,
					U16 _hasMove,
					U16 _depth,
					U16 _characterID,
					FMatrix *_matrix,				// NULL if the object does not have a matrix
					FCXFormWAlpha *_colorTransform, // NULL if there is no color transform
					U16 _ratio,
					FString *_name,  // NULL if there is no name
					U16 _clipDepth); /*DM*/

	//now is possible to give a clip action
	// if a certain flag is not true, you must provide a NULL value for its associated argument(s)
	FCTPlaceObject2(U16 _hasClipAction,
					U16 _hasClipDepth, /*DM*/
					U16 _hasRatio,
					U16 _hasCharID,
					U16 _hasMove,
					U16 _depth,
					U16 _characterID,
					FMatrix *_matrix,				// NULL if the object does not have a matrix
					FCXFormWAlpha *_colorTransform, // NULL if there is no color transform
					U16 _ratio,
					FString *_name, // NULL if there is no name
					U16 _clipDepth,
					FClipAction *_clipAction); /*DM*/

	FCTPlaceObject2(const FCTPlaceObject2 &_obj);

	void SetParameters(U16 _hasClipDepth, /*DM*/
					   U16 _hasRatio,
					   U16 _hasCharID,
					   U16 _hasMove,
					   U16 _depth,
					   U16 _characterID,
					   FMatrix *_matrix,			   // NULL if the object does not have a matrix
					   FCXFormWAlpha *_colorTransform, // NULL if there is no color transform
					   U16 _ratio,
					   FString *_name,  // NULL if there is no name
					   U16 _clipDepth); /*DM*/

	// Used to fix the high level manager depth sorting problem.
	int GetDepth() { return depth; }
	void SetDepth(int d) { depth = d; }

	virtual ~FCTPlaceObject2();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	U32 IsPlaceObject();
	int GetPlacedId();
	std::string GetName() { return name ? name->GetString() : string(); }
	void SetId(U16 id);
	void ChangePlacedId(U16 id);

	void SetMatrix(FMatrix *matrix);
	void ApplyMatrix(const FMatrix &_matrix)
	{
		if (matrix)
			(*matrix) = (*matrix) * _matrix;
		else
			matrix = new FMatrix(_matrix);
	}

private:
	//flags
	U16 hasClipAction;
	U16 hasClipDepth; /*DM*/
	U16 hasRatio;
	U16 hasCharID;
	U16 hasMove;

	U16 depth;
	U16 characterID;
	FMatrix *matrix;
	FCXFormWAlpha *colorTransform;
	U16 ratio;
	FString *name;
	U16 clipDepth; /*DM*/
	FClipAction *clipAction;
};

// a flash control tag object which marks a SWF movie as uneditable

class DVAPI FCTProtect : public FCT
{
public:
	FCTProtect();

	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
};

// a Flash control tag object which indicates to the flash player to remove an object from the display list (flash 1.0)

class DVAPI FCTRemoveObject : public FCT
{

public:
	FCTRemoveObject(U16 _characterID, U16 _depth);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

	// Used to fix the high level manager depth sorting problem.
	int GetDepth() { return depth; }
	void SetDepth(int d) { depth = d; }

private:
	U16 characterID;
	U16 depth;
};

// a Flash control tag object which indicates to the flash player to remove an object from the display list (flash 1.0)

class DVAPI FCTRemoveObject2 : public FCT
{

public:
	FCTRemoveObject2(U16 _depth);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 depth;
};

// A flash object which sets a movie's background color

class DVAPI FCTSetBackgroundColor : public FCT
{
public:
	FCTSetBackgroundColor(FColor *_color);
	virtual ~FCTSetBackgroundColor();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FColor *color;
};

class DVAPI FCTUnparsedTag : public FCT
{
public:
	FCTUnparsedTag(U16 _tagId, U32 _lenght, U8 *_data);
	virtual ~FCTUnparsedTag();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
	USHORT GetID();
	void SetID(USHORT id);

private:
	U8 *data;
	U32 lenght;
	U16 tagId;
};

//a control tag that indicates end of the current frame

class DVAPI FCTShowFrame : public FCT
{

public:
	FCTShowFrame();
	U32 IsShowFrame();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
};

// A flash object that instructs flash player to start a sound

class DVAPI FCTStartSound : public FCT
{
public:
	FCTStartSound(U16 _soundID, FSoundInfo *_soundInfo);
	virtual ~FCTStartSound();
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 soundID;
	FSoundInfo *soundInfo;
};

#ifdef WIN32 // added from DV
#pragma warning(pop)
#endif

#endif
