// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.

/****************************************************************************************
 
				File Summary: FAction.h

  This header-file contains the declarations of the low-level action-related classes:
  
	 1)	class FActionRecord;
	 2)	all the derived classes of class FAction:

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
		class FActionGetURL;
		class FActionGetURL2;
		class FActionGetVariable;
		class FActionGotoFrame;
		class FActionGotoFrame2;
		class FActionGotoLabel;
		class FActionIf;
		class FActionJump;
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
		class FActionPush;
		class FActionRandomNumber;
		class FActionRemoveSprite;
		class FActionSetProperty;
		class FActionSetTarget;
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
		class FActionTrace;
		class FActionWaitForFrame;
		class FActionWaitForFrame2;
	and,

	3)	class FActionCondition.

****************************************************************************************/

#ifndef _F_ACTION_H_
#define _F_ACTION_H_

#include "Macromedia.h"
#include "FSWFStream.h"

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
class FString;

//! A general class specifying an action to be performed by the Flash player
/*!
Buttons and DoAction tags utilize FActionRecords. 
When encountered in a button tag, the Flash player adds action to a list to be processed when a button state has changed.
When encountered in a DoAction tag, the Flash player adds the action to a list to be processed when a FCTShowframe tag is encountered. 
FActionRecords specifiy actions such as: starting and stoping movie play, toggling display quality, performing stack arithmetic... etc. 
Many actions involve the use of a last-on/first-off stack machine (Flash 4.0) that stores string values.
\sa FCTDoAction, FDTDefineButton, FDTDefineButton2
*/

class DVAPI FActionRecord
{
public:
	virtual ~FActionRecord() {}

	//! A general function that will write its object out to a FSWFStream
	/*!
		All FActionRecords contain the WriteToSWFStream member function, a function that will writes out its Object's data out to a FSWFStream
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) = 0;
};

class DVAPI FActionUnparsed : public FActionRecord
{
	UCHAR code;
	USHORT length;
	UCHAR *data;

public:
	FActionUnparsed(UCHAR _code, USHORT _length, UCHAR *_data);
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);
};

//! An action that adds two numbers
/*! \verbatim
1. pops value A off the stack machine    
2. pops value B off the stack machine    
3. A and B are converted to floating-point (if non-numeric,converts to 0)    
4. A and B are added    
5. Result, A + B, is pushed back onto stack    
\endverbatim */

class FActionAdd : public FActionRecord
{
public:
	FActionAdd() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionAdd); }
};

//! An action that performs a logical AND of two numbers
/*! \verbatim	
1. pops value A off the stack    
2. pops value B off the stack    
3. A nd B are converted to floating-point(if non-numeric, converts to 0)    
4. If both A and B are non-zero, a 1 is pushed onto the stack; otherwise a 0 is pushed    
\endverbatim */

class FActionAnd : public FActionRecord
{
public:
	FActionAnd() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionAnd); }
};

//! An action that converts ASCII to character code
/*! \verbatim
1. pops value A off the stack    
2. the value is converted from a number to the corresponding ASCII character    
3. the resulting character is pushed to the stack    
\endverbatim */

class FActionAsciiToChar : public FActionRecord
{
public:
	FActionAsciiToChar() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionAsciiToChar); }
};

//! An action that calls a subroutine
/*! \verbatim	
1. pops value A off the stack
2. this value should be either a string matching a frame label, or a number indicating a frame number
3. The value may be prefixed by a target string identifying the movie clip that contains the frame being called
4. If the frame is successfully located, the actions in the target frame are executed.  After the actions in the target frame are executed, execution resumes at the instruction after the ActionCall instruction.
5.  If the frame cannot be found, nothing happens
6. NOTE: This action's tag (0x9e) has the high bit set, this is a bug
\endverbatim */
class FActionCall : public FActionRecord
{
public:
	FActionCall() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream)
	{
		_SWFStream->WriteByte(sactionCall);
		// Write the length since the high bit is set in the action tag
		_SWFStream->WriteWord(0);
	}
};

//! An action that converts character code to ASCII
/*! \verbatim	
1. pops value A off the stack
2. The first character of value a is converted to a numeric ASCII character code
3. The resulting character code is pushed to the stack	
\endverbatim */
class FActionCharToAscii : public FActionRecord
{
public:
	FActionCharToAscii() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionCharToAscii); }
};

//! An action that clones a sprite
/*! \verbatim	
1. pops value DEPTH (new sprite depth) off the stack
2. pops value TARGET (name of new sprite) off the stack
3. pops value SOURCE (souce sprite) off stack
4. duplicate the movie SOURCE, giving the new movie the name TARGET at z-order depth DEPTH
\endverbatim  */
class FActionCloneSprite : public FActionRecord
{
public:
	FActionCloneSprite() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionCloneSprite); }
};

//! An action that divides two numbers
/*! \verbatim	
1. pops value A off the stack
2. pops value B off stack
3. A and B are converted to floating-point (if non-numeric convert to 0)
4. Result B divided by A is pushed to stack
5. If A is 0, the result is the string "#ERROR#"
\endverbatim */
class FActionDivide : public FActionRecord
{
public:
	FActionDivide() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionDivide); }
};

//! An action that ends drag operation
/*! \verbatim	
1. ends the drag operation in progress (if any)
\endverbatim */
class FActionEndDrag : public FActionRecord
{
public:
	FActionEndDrag() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	 */
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionEndDrag); }
};

//! An action that tests two numbers for equality
/*! \verbatim
1. pops value A off the stack
2. pops value B off stack
3. A and B converted to floating-point (if non-numeric, converted to 0)
4. A and B compared for equality
5. If equal, a 1 is pushed to stack
6. Otherwise, 0 is pushed	
\endverbatim */
class FActionEquals : public FActionRecord
{
public:
	FActionEquals() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionEquals); }
};

//! An action that gets a movie property
/*! \verbatim	
1. pops value INDEX off the stack
2. pops TARGET off the stack
3. retrieve the value of the property enumerated as INDEX from the movie clip with target path TARGET and push this value onto stack
\endverbatim */
class FActionGetProperty : public FActionRecord
{

public:
	FActionGetProperty() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionGetProperty); }
};

//! An action that reports milliseconds since player started
/*! \verbatim	
1. an integer representing the number of minutes since the player was started is pushed onto stack
\endverbatim */
class FActionGetTime : public FActionRecord
{
public:
	FActionGetTime() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionGetTime); }
};

//! An action that opens the given URL in a given window
/*! \verbatim	
An action record specifying that the player open the given URL in a given window
\endverbatim */
class FActionGetURL : public FActionRecord
{
public:
	//! FActionGetURL constructor
	/*!
		\param _url a pointer to an FString representing a URL address
		\param _window a pointer to an FString identifying a window to open the URL in (empty FString indicates current window)
	*/
	FActionGetURL(FString *_url, FString *_window);

	virtual ~FActionGetURL();

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FString *url;
	FString *window;
};

//! An action that opens a URL in an indicated window (stack based)
/*! \verbatim	
1. pops value WINDOW off stack, window specifies the target window, empty string indicates current
2. pops URL off stack, specifies which URL to retrieve
3. retrieve URL in WINDOW using given HTTP request method
		- if method is "HTTP GET" or "HTTP POST", variables in movie clip are sent using standard "x-www-urlencoded" encoding
\endverbatim */
class FActionGetURL2 : public FActionRecord
{
public:
	//! FActionGetURL2 constructor
	/*! 
		\param _method a number indicating an HTTP request method (value 0 indicates that it is not a request method, 1 indicates HTTP GET, 2 indicates HTTP POST)
	*/
	FActionGetURL2(U8 _method);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 method;
};

//! An action that gets a variable's value
/*! \verbatim	
1. pops value NAME off the stack
2. retrieves value of variable NAME
3. pushes NAME's associated value back onto stack
Variables in other execution contexts may be referenced by prefixing a variable name with a target path followed by a colon
\endverbatim */
class FActionGetVariable : public FActionRecord
{
public:
	FActionGetVariable() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionGetVariable); }
};

//! An action that goes to the specified frame
/*! \verbatim	
An action record specifying that the player goto the frame specified by the given frame index
\endverbatim */
class DVAPI FActionGotoFrame : public FActionRecord
{
public:
	//! FActionGotoFrame constructor
	/*!
		\param frameIndex a number specifying the frame index of a frame to go to
	*/
	FActionGotoFrame(U16 frameIndex);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 frameIndex;
};

//! An action that goes to a identified frame (stack based)
/*! \verbatim	
1. pops value FRAME off the stack
2. if frame is a number, the next movie frame displayed will be FRAME'th frame in the current movie
3. if frame is a string, the next movie frame displayed will be the frame designated by label "FRAME" (if no such label exists, action is ignored)
4. FRAME may be prefixed by a target path
5. if given "play" flag is set, the movie begins playing the goto frame, otherwise movie play is stopped on goto frame
\endverbatim */
class FActionGotoFrame2 : public FActionRecord
{
public:
	//! FActionGotoFrame2 constructor
	/*!
		\param _play a true or false value indicating whether or not goto frame should be played
	*/

	FActionGotoFrame2(U8 _play);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 play;
};

//! An action that goes to the frame indicated by the given label
/*! \verbatim	
An action record specifying that the player goto the frame with the given label	
\endverbatim */
class FActionGotoLabel : public FActionRecord
{
public:
	//! FActionLabel constructor
	/*!
		\param _label a FString pointer indicating the label of the frame to go to
	*/
	FActionGotoLabel(FString *_label);

	virtual ~FActionGotoLabel();

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FString *label;
};

//! An action that test a condition and branches
/*! \verbatim	
1. pops value CONDITION off the stack
2. if CONDITION is non-zero, branch to a given number of offset bytes following FActionIf record (a 0 value points to action directly following FActionIf; offset is a signed integer quantity enabling forward and backward branching)	
\endverbatim */
class DVAPI FActionIf : public FActionRecord
{
public:
	//!FActionIf constructor
	/*!
		\param _branchOffset a value representing a number of bytes (pos or neg) following the current tag to brach off to
	*/
	FActionIf(S16 _branchOffset);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	S16 branchOffset;
};

//! An action that performs an unconditional branch
/*! \verbatim	
An action record specifying that the player branch to a given number of bytes following the FActionJump record(a 0 value points to the action record immediately following)
Can branch either forward or backward
\endverbatim */
class FActionJump : public FActionRecord
{
public:
	//! FActionJump constructor
	/*!
		\param _branchOffset a value representing a number of bytes (pos or neg) following the current tagto branch off to
	*/
	FActionJump(S16 _branchOffset);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	S16 branchOffset;
};

//! An action that tests if a number is less than another number
/*! \verbatim	
1. pops value A off the stack
2. pops value B off the stack
3. A and B converted to floating-point (non-numeric converted to 0)
4. if B < A, a 1 is pushed onto stack, otherwise a 0 is pushed	
\endverbatim */
class FActionLess : public FActionRecord
{
public:
	FActionLess() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionLess); }
};

//! An action that converts ASCII to character code (multi-byte aware)
/*! \verbatim	
1. pops value VALUE off the stack
2. the value is converted from a number to the corresponding character. If 16bit value, double byte character constructed
3. resulting character pushed back onto stack
\endverbatim */
class FActionMBAsciiToChar : public FActionRecord
{
public:
	FActionMBAsciiToChar() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionMBAsciiToChar); }
};

//! An action that converts character code to ASCII (multi-byte aware)
/*! \verbatim	
1. pops value VALUE off the stack
2. the first character of VALUE is converted to a numeric character code. If the first character of VALUE is a double-byte character, a 16bit code value is constructed
3. resulting code is pushed to stack	
\endverbatim */
class FActionMBCharToAscii : public FActionRecord
{
public:
	FActionMBCharToAscii() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionMBCharToAscii); }
};

//! An action that extracts a substring from a string (multi-byte aware)
/*! \verbatim	
1. pops number COUNT off the stack
2. pops number INDEX off stack
3. pops string STRING off stack
4. push to stack, the string of length LENGTH starting at the INDEX'th character of STRING
5. if INDEX or COUNT are non-numeric, push empty string to stack	
\endverbatim */
class FActionMBStringExtract : public FActionRecord
{
public:
	FActionMBStringExtract() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionMBStringExtract); }
};

//! An action that computes the length of a string (multi-byte aware)
/*! \verbatim
1. pops value STRING off stack
2. push length of STRING onto stack	
\endverbatim */
class FActionMBStringLength : public FActionRecord
{
public:
	FActionMBStringLength() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionMBStringLength); }
};

//! An action that multiplies two numbers
/*! \verbatim
1. pops value A off stack
2. pops value B off stack
3. A and B are converted to floating-point (non-numeric converted to 0)
4. result of A multiply B is pushed to stack	
\endverbatim */
class FActionMultiply : public FActionRecord
{
public:
	FActionMultiply() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionMultiply); }
};

//! An action that goes to next frame
/*! \verbatim
An action specifying that the Flash player goto the next frame	
\endverbatim */
class FActionNextFrame : public FActionRecord
{
public:
	FActionNextFrame() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionNextFrame); }
};

//! An action that performs the logical NOT of a number
/*! \verbatim
An action specifying that the Flash player:
1. pops value A off stack
2. A is converted to floating-point (non-numeric is converted to zero)
3. if A is zero, 1 pushed to stack
4. otherwise 0 pushed to stack
\endverbatim */
class FActionNot : public FActionRecord
{
public:
	FActionNot() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionNot); }
};

//! An action that performs the logical OR of two numbers
/*! \verbatim
1. pops value A off stack
2. pops value B off stack
3. A and B converted to floating-point (non-numeric converted to zero)
4. if either A or B are non-zero, 1 is pushed to stack, otherwise 0 pushed to stack
\endverbatim */
class FActionOr : public FActionRecord
{
public:
	FActionOr() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionOr); }
};

//! An action that starts playing the movie at the current frame
/*! \verbatim
An action specifying that the Flash player to start playing the movie at the current frame	
\endverbatim */
class FActionPlay : public FActionRecord
{
public:
	FActionPlay() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionPlay); }
};

//! An action that pops a value off the stack
/*! \verbatim
1. pops value A off stack and discards the value
\endverbatim */
class FActionPop : public FActionRecord
{
public:
	FActionPop() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionPop); }
};

//! An action that goes to the previous frame
/*! \verbatim
\endverbatim */
class FActionPrevFrame : public FActionRecord
{
public:
	FActionPrevFrame() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionPrevFrame); }
};

//! An action that pushes a given value onto the stack
/*! \verbatim
Pushes either a string or floating-point number onto the stack
\endverbatim */
class DVAPI FActionPush : public FActionRecord
{
public:
	//! FActionPush constructor for pushing strings
	/*!
		\param _string a pointer to the FString that will be pushed onto stack
	*/
	FActionPush(FString *_string);

	//! FActionPush constructor for pushing numbers
	/*!
		\param _number a pointer to the FLOAT that will be pushed onto stack
	*/
	FActionPush(FLOAT _number);

	FActionPush(U8 _type, FLOAT _number);
	FActionPush(double _number);
	FActionPush(U8 _type, U8 _value);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FString *string;
	FLOAT number;
	U8 cValue;
	double dValue;
	U8 type;
};

//! An action that constructs a random number
/*! \verbatim
1. pop MAXIMUM off stack
2. constructs a random integer in the range 0... (MAXIMUM -1)
3. pushes this random value to stack	
\endverbatim */
class FActionRandomNumber : public FActionRecord
{
public:
	FActionRandomNumber() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionRandomNumber); }
};

//! An action that removes a cloned sprite
/*! \verbatim
1. pops value TARGET off stack
2. Removes the cloned clip identifiesd by target TARGET	
\endverbatim */
class FActionRemoveSprite : public FActionRecord
{
public:
	FActionRemoveSprite() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionRemoveSprite); }
};

//! An action that sets a movie property
/*! \verbatim
1. pops value VALUE off stack
2. pops value INDEX off stack
3. pops value TARGET off stack
4. sets property enumerated as INDEX in the movie clip TARGET to the value VALUE
\endverbatim */
class FActionSetProperty : public FActionRecord
{
public:
	FActionSetProperty() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionSetProperty); }
};

//! An action that sets the context of action
/*! \verbatim
Sets context of action to target named by given string	
\endverbatim */
class FActionSetTarget : public FActionRecord
{
public:
	//! FActionSetTarget constructor
	/*!
	    \param _targetName a pointer to the FString naming the target to set action context to
	*/
	FActionSetTarget(FString *_targetName);

	virtual ~FActionSetTarget();

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	FString *targetName;
};

//! An action that sets the context of action (stack based)
/*! \verbatim
1. pops value TARGET off stack
2. sets current context of action to object identified by TARGET	
\endverbatim */
class FActionSetTarget2 : public FActionRecord
{
public:
	FActionSetTarget2() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionSetTarget2); }
};

//! An action that sets a variable
/*! \verbatim
1. pops value VALUE off stack
2. pops string NAME off stack
3. sets NAME to VALUE in the current execution context
\endverbatim */
class FActionSetVariable : public FActionRecord
{
public:
	FActionSetVariable() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionSetVariable); }
};

//! An action that starts dragging a movie clip
/*! \verbatim
1. pops value TARGET off stack
2. pops LOCKCENTER off stack
3. pops CONSTRAIN
4. if CONSTRAIN is non-zero:
	- pops y2
	- pops x2
	- pops y1
	- pops x1
5. starts dragging of movie clip identified by TARGET
6. if LOCKCENTER is non-zero, the center of clip is locked to the mouse position, otherwise clip moves relative to starting mouse position
7. if CONSTRAIN, draged clip is constrained coordinates x1, y1, x2, y2
\endverbatim */
class FActionStartDrag : public FActionRecord
{
public:
	FActionStartDrag() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStartDrag); }
};

//! An action that stops movie play at the current frame
/*! \verbatim
\endverbatim */
class FActionStop : public FActionRecord
{
public:
	FActionStop() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStop); }
};

//! An action that stops playing all sounds in movie
/*! \verbatim	
\endverbatim */
class FActionStopSounds : public FActionRecord
{
public:
	FActionStopSounds() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStopSounds); }
};

//! An action that concatenates two strings
/*! \verbatim
1. pops string A off stack
2. pops string B off stack
3. the concatenation of BA is pushed to stack	
\endverbatim */
class FActionStringAdd : public FActionRecord
{
public:
	FActionStringAdd() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStringAdd); }
};

//! An action that tests two strings for equality
/*! \verbatim
1. pops string A off stack
2. pops string B off stack
3. A and B are compared (case-sensitive)
4. if strings are equal, a 1 is pushed to stack, otherwise 0 pushed
\endverbatim */
class FActionStringEquals : public FActionRecord
{
public:
	FActionStringEquals() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStringEquals); }
};

//! An action that extracts a substring from a string
/*! \verbatim
1. pops number COUNT off stack
2. pops number INDEX off stack
3. pops string STRING off stack
4. the substring of length COUNT starting at the INDEX'th index of string STRING is pushed to stack
5. if COUNT or INDEX non-numeric, push empty string
\endverbatim */
class FActionStringExtract : public FActionRecord
{
public:
	FActionStringExtract() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStringExtract); }
};

//! An action that computes the length of a string
/*! \verbatim
1. pops string STRING off stack
2. the length of string STRING is pushed to stack
\endverbatim */
class FActionStringLength : public FActionRecord
{
public:
	FActionStringLength() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStringLength); }
};

//! An action that test if a string is less than another string
/*! \verbatim
1. pops string A off stack
2. pops string B off stack
3. if B < A using a byte to byte comparison, a 1 is pushed to the stack, otherwise 0 is pushed to stack	
\endverbatim */
class FActionStringLess : public FActionRecord
{
public:
	FActionStringLess() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionStringLess); }
};

//! An action that subtracts a number from another number
/*! \verbatim
1. pops value A off stack
2. pops value B off stack
3. converts values A and B to floating-point (non-numeric converts to zero)
4. the result, B-A, is pushed to stack
\endverbatim */
class FActionSubtract : public FActionRecord
{
public:
	FActionSubtract() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionSubtract); }
};

//! An action that toggles screen quality between high and low
/*! \verbatim
\endverbatim */
class FActionToggleQuality : public FActionRecord
{
public:
	FActionToggleQuality() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionToggleQuality); }
};

//! An action that converts a value to an integer
/*! \verbatim
1. pops value A off stack
2. A is converted to a floating-point number
3. A is truncated to an integer value
4. truncated A is pushed to stack
\endverbatim */
class FActionToInteger : public FActionRecord
{
public:
	FActionToInteger() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionToInteger); }
};

//! An action that sends debugging output string
/*! \verbatim
1. pops value VALUE off stack
2. in the "Test Movie" mode of the Flash editor, appends VALUE to the output window if the debugging level is set to "None"
3. ignored by Flash Player
\endverbatim */
class FActionTrace : public FActionRecord
{
public:
	FActionTrace();

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionTrace); }
};

//! An action that waits for a specified frame, otherwise skips a specified number of actions
/*! \verbatim
\endverbatim */
class FActionWaitForFrame : public FActionRecord
{
public:
	//! FActionWaitForFrame constructor
	/*!
	    \param _frameIndex a number indexing the frame to wait for
		\param skipCount a number indicating the number of frames to skip
	*/
	FActionWaitForFrame(U16 _frameIndex, U16 skipCount);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U16 frameIndex;
	U16 skipCount;
};

//! An action that waits for a frame to be loaded
/*! \verbatim
1. pops value FRAME off stack
2. frame is evaluated in the same manner as in FActionGotoFrame2
3. if the frame identified by FRAME has been loaded, skip a specified number of actions following the current one
\endverbatim */
class FActionWaitForFrame2 : public FActionRecord
{
public:
	//! FActionWaitForFrame2 constructor
	/*!
	    \param _skipCount a number of actions to skip
	*/
	FActionWaitForFrame2(U8 _skipCount);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	U8 skipCount;
};

//! A class specifying a set of conditions and a set of subsequent actions  to perform
/*! \verbatim
Buttons contain a set of FActionConditions.
An FActionCondition conatins a set of conditions and a set of actions to perform when the set of conditions is met
	\sa FDTDefineButton, FDTDefineButton2
*/
class DVAPI FActionCondition
{
public:
	FActionCondition();

	~FActionCondition();

	void Clear();

	//! A member function that adds another action to the ActionCondition
	/*! 
		    Appends an FActionRecord to the list of FActionRecords.
			These actions will be performed when the set of conditions is met
			\param _ActionRecord any FActionRecord pointer
		*/
	void AddActionRecord(FActionRecord *_ActionRecord);

	//! A member function that adds a key-stroke condition to the set of conditions in an ActionCondition
	/*!
			\param keyCode one of the enumerated key-codes found in "Macromedia.h"
		*/
	void AddKeyCode(U32 keyCode);

	//! A member function that adds a button condition to the set of conditions in an ActionCondition
	/*!
			\param _condition one of the enumerated action-conditions found in "Macromedia.h"
		*/
	void AddCondition(U16 _condition);

	//! Writes the object out to a FSWFStream
	/*! 
			\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
			\sa WriteToSWFStream(FSWFStream* _SWFStream, int lastFlag), FSWFStream
		*/
	void WriteToSWFStream(FSWFStream *_SWFStream);

	//! Writes the object out to a FSWFStream and writes the terminating record to the FSWFStream
	/*! 
			\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
			\param lastFlag a flag that, if true, indicates that this ActionCondition is the last record to be writen
			\sa WriteToSWFStream(FSWFStream* _SWFStream), FSWFStream
		*/
	void WriteToSWFStream(FSWFStream *_SWFStream, int lastFlag);

private:
	std::list<FActionRecord *> actionRecordList;
	U16 conditionFlags;
};

/*
class FActionConditionList
{
public:
	FActionConditionList();
	~FActionConditionList();

	void AddActionCondition(FActionCondition* _ActionCondition);
	U32  Size();
	void WriteToSWFStream(FSWFStream* _SWFStream);

private:
	std::list<FActionCondition*> conditionList;
};
*/
//=============================================================================
//                        ALCUNE AZIONI FLASH 5
//=============================================================================

class FActionConstantPool : public FActionRecord
{
public:
	FActionConstantPool(FString *string);
	~FActionConstantPool();

	void addConstant(FString *string);

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	std::list<FString *> constantList;
	FActionConstantPool();
};

class FActionLess2 : public FActionRecord
{
public:
	FActionLess2() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionLess2); }
};

class FActionEquals2 : public FActionRecord
{
public:
	FActionEquals2() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionEquals2); }
};

class FActionCallMethod : public FActionRecord
{
public:
	FActionCallMethod() {}

	//! Writes the object out to a FSWFStream
	/*! 
		\param _SWFStream any FSWFStream pointer, though usually the FSWFStream given as an argument to the appendTag function of the FSWFStream representing the .swf file being created 
	*/
	virtual void WriteToSWFStream(FSWFStream *_SWFStream) { _SWFStream->WriteByte(sactionCallMethod); }
};

//=============================================================================
//                        CLIP ACTION / CLIP ACTION RECORD
//=============================================================================

class FClipActionRecord;

class DVAPI FClipAction
{
public:
	FClipAction();
	~FClipAction();

	void Clear();
	void AddClipActionRecord(FClipActionRecord *_clipActionRecord);
	void AddFlags(U32 _flags);
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	std::list<FClipActionRecord *> clipActionRecordList;
	U32 eventFlags;
};

//=============================================================================

class DVAPI FClipActionRecord
{
public:
	FClipActionRecord();
	~FClipActionRecord();

	void Clear();
	void AddActionRecord(FActionRecord *_actionRecord);
	void AddKeyCode(U8 _keyCode);
	void AddFlags(U32 _flags);
	void WriteToSWFStream(FSWFStream *_SWFStream);

private:
	std::list<FActionRecord *> actionRecordList;
	U32 eventFlags;
	U8 keyCode;
};
#endif
