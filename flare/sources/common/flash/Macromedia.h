// Copyright © 1999 Middlesoft, Inc. All rights reserved.
// First Created By Lee Thomason.
// First Created On 09/08/1999.
// Last Modified On 11/09/1999.
// Last Modified On 18/06/2002  by DV for Fixes
/****************************************************************************************

				File Summary: Macromedia.h

	This header file defines various structs and enums used by Flash File Format SDK
	low-level manager.

****************************************************************************************/

#ifndef _MACROMEDIA_H_
#define _MACROMEDIA_H_

#ifdef WIN32 // added by DV
#pragma warning(disable : 4786)
#endif

#include "tcommon.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

class FSWFStream;

#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif

// Some basic defines for debugging:
#ifdef DEBUG
#define FLASHOUTPUT printf
#define FLASHASSERT assert
#define FLASHPRINT printf
//
// 	#define FLASHOUTPUT ((void)0)
// 	#define FLASHASSERT ((void)0)
// 	#define FLASHPRINT  ((void)0)
#else
inline void nothing(...)
{
}
#define FLASHOUTPUT nothing //((void)0)
#define FLASHASSERT nothing //((void)0)
#define FLASHPRINT printf
#endif

/*  // removed by DV
#ifndef min
	#define min( a, b ) ( ( a < b ) ? a : b )
#endif
#ifndef max
	#define max( a, b ) ( ( a > b ) ? a : b )
#endif
*/

// Global Types
typedef float FLOAT;
typedef TUINT32 U32, *P_U32, **PP_U32;
typedef TINT32 S32, *P_S32, **PP_S32;
typedef unsigned short U16, *P_U16, **PP_U16;
typedef signed short S16, *P_S16, **PP_S16;
typedef unsigned char U8, *P_U8, **PP_U8;
typedef signed char S8, *P_S8, **PP_S8;
typedef TINT32 SFIXED, *P_SFIXED;
typedef TINT32 SCOORD, *P_SCOORD;
typedef int BOOL;

const SFIXED Fixed1 = 0x00010000;
const SCOORD SCoord1 = 20;

typedef struct SRECT {
	SCOORD xmin;
	SCOORD xmax;
	SCOORD ymin;
	SCOORD ymax;
} SRECT, *P_SRECT;

const U8 Snd5k = 0;
const U8 Snd11k = 1;
const U8 Snd22k = 2;
const U8 Snd44k = 3;

const U8 Snd8Bit = 0;
const U8 Snd16Bit = 1;

const U8 SndMono = 0;
const U8 SndStereo = 1;

typedef struct SSound {
	U8 format;		 // 0 none, 1 PCM
	U8 rate;		 // Snd5k...Snd44k
	U8 size;		 // 0 8bit, 1 16bit
	U8 type;		 // 0 mono, 1 stereo
	U32 sampleCount; // the number of samples
	U32 soundSize;   // the number of bytes in the sample
	U8 *sound;		 // pointer to the sound data
} SSound, *P_SSound;

typedef struct SPOINT {
	SCOORD x;
	SCOORD y;
} SPOINT, *P_SPOINT;

// Start Sound Flags
enum {
	soundHasInPoint = 0x01,
	soundHasOutPoint = 0x02,
	soundHasLoops = 0x04,
	soundHasEnvelope = 0x08

	// the upper 4 bits are reserved for synchronization flags
};

enum {
	fillSolid = 0x00,
	fillGradient = 0x10,
	fillLinearGradient = 0x10,
	fillRadialGradient = 0x12,
	fillMaxGradientColors = 8,
	// Texture/bitmap fills
	fillTiledBits = 0x40, // if this bit is set, must be a bitmap pattern
	fillClippedBits = 0x41
};

enum {
	CURVED_EDGE = 0,
	STRAIGHT_EDGE = 1
};

enum {
	NOT_EDGE_REC = 0,
	EDGE_REC = 1
};

// Flags for defining a shape character
enum {
	// These flag codes are used for state changes - and as return values from ShapeParser::GetEdge()
	eflagsMoveTo = 0x01,
	eflagsFill0 = 0x02,
	eflagsFill1 = 0x04,
	eflagsLine = 0x08,
	eflagsNewStyles = 0x10,

	eflagsEnd = 0x80 // a state change with no change marks the end
};

#ifndef NULL
#define NULL 0
#endif

// Tag values that represent actions or data in a Flash script.
enum {
	stagEnd = 0,
	stagShowFrame = 1,
	stagDefineShape = 2,
	stagFreeCharacter = 3,
	stagPlaceObject = 4,
	stagRemoveObject = 5,
	stagDefineBits = 6,
	stagDefineButton = 7,
	stagJPEGTables = 8,
	stagSetBackgroundColor = 9,
	stagDefineFont = 10,
	stagDefineText = 11,
	stagDoAction = 12,
	stagDefineFontInfo = 13,
	stagDefineSound = 14, // Event sound tags.
	stagStartSound = 15,
	stagDefineButtonSound = 17,
	stagSoundStreamHead = 18,
	stagSoundStreamBlock = 19,
	stagDefineBitsLossless = 20, // A bitmap using lossless zlib compression.
	stagDefineBitsJPEG2 = 21,	// A bitmap using an internal JPEG compression table.
	stagDefineShape2 = 22,
	stagDefineButtonCxform = 23,
	stagProtect = 24, // This file should not be importable for editing.

	stagPathsArePostScript = 25, // assume shapes are filled as PostScript style paths

	// These are the new tags for Flash 3.
	stagPlaceObject2 = 26,  // The new style place w/ alpha color transform and name.
	stagRemoveObject2 = 28, // A more compact remove object that omits the character tag (just depth).

	// This tag is used for RealMedia only
	stagSyncFrame = 29, // Handle a synchronization of the display list

	stagFreeAll = 31, // Free all of the characters

	stagDefineShape3 = 32,		  // A shape V3 includes alpha values.
	stagDefineText2 = 33,		  // A text V2 includes alpha values.
	stagDefineButton2 = 34,		  // A button V2 includes color transform, alpha and multiple actions
	stagDefineBitsJPEG3 = 35,	 // A JPEG bitmap with alpha info.
	stagDefineBitsLossless2 = 36, // A lossless bitmap with alpha info.
	stagDefineSprite = 39,		  // Define a sequence of tags that describe the behavior of a sprite.
	stagNameCharacter = 40,		  // Name a character definition, character id and a string, (used for buttons, bitmaps, sprites and sounds).

	stagSerialNumber = 41,	 // a tag command for the Flash Generator customer serial id and cpu information
	stagDefineTextFormat = 42, // define the contents of a text block with formating information

	stagFrameLabel = 43,	   // A string label for the current frame.
	stagSoundStreamHead2 = 45, // For lossless streaming sound, should not have needed this...
	stagDefineMorphShape = 46, // A morph shape definition

	stagFrameTag = 47,		   // a tag command for the Flash Generator (WORD duration, STRING label)
	stagDefineFont2 = 48,	  // a tag command for the Flash Generator Font information
	stagGenCommand = 49,	   // a tag command for the Flash Generator intrinsic
	stagDefineCommandObj = 50, // a tag command for the Flash Generator intrinsic Command
	stagCharacterSet = 51,	 // defines the character set used to store strings
	stagFontRef = 52,		   // defines a reference to an external font source

	// Flash 4 tags
	stagDefineEditText = 37, // an edit text object (bounds, width, font, variable name)
	stagDefineVideo = 38,	// a reference to an external video stream

	// NOTE: If tag values exceed 255 we need to expand SCharacter::tagCode from a BYTE to a WORD
	stagDefineBitsPtr = 1023 // a special tag used only in the editor
};

// PlaceObject2 Flags
enum {
	splaceMove = 0x01,				 // this place moves an exisiting object
	splaceCharacter = 0x02,			 // there is a character tag	(if no tag, must be a move)
	splaceMatrix = 0x04,			 // there is a matrix (matrix)
	splaceColorTransform = 0x08,	 // there is a color transform (cxform with alpha)
	splaceRatio = 0x10,				 // there is a blend ratio (word)
	splaceName = 0x20,				 // there is an object name (string)
	splaceDefineClip = 0x40,		 // this shape should open or close a clipping bracket (character != 0 to open, character == 0 to close)
	splaceCloneExternalSprite = 0x80 // cloning a movie which was loaded externally
									 // one bit left for expansion
};

//ActionConditions
enum {
	OverDownToIdle = 1,
	IdleToOverDown = 2,
	OutDownToIdle = 3,
	OutDownToOverDown = 4,
	OverDownToOutDown = 5,
	OverDownToOverUp = 6,
	OverUpToOverDown = 7,
	OverUpToIdle = 8,
	IdleToOverUp = 9
};

//Clip Action
enum {
	//Flash 5-
	ClipEventLoad = 0x00000001,
	ClipEventEnterFrame = 0x00000002,
	ClipEventUnload = 0x00000004,
	ClipEventMouseMove = 0x00000008,
	ClipEventMouseDown = 0x00000010,
	ClipEventMouseUp = 0x00000020,
	ClipEventKeyDown = 0x00000040,
	ClipEventKeyUp = 0x00000080,
	ClipEventData = 0x00000100,

	//Flash 6+
	ClipEventInitialize = 0x00000200,
	ClipEventPress = 0x00000400,
	ClipEventRelease = 0x00000800,
	ClipEventReleaseOutside = 0x00001000,
	ClipEventRollOver = 0x00002000,
	ClipEventRollOut = 0x00004000,
	ClipEventDragOver = 0x00008000,
	ClipEventDragOut = 0x00010000,
	ClipEventKeyPress = 0x00020000,
};

//Key Codes
enum {
	ID_KEY_LEFT = 0x01,
	ID_KEY_RIGHT = 0x02,
	ID_KEY_HOME = 0x03,
	ID_KEY_END = 0x04,
	ID_KEY_INSERT = 0x05,
	ID_KEY_DELETE = 0x06,
	ID_KEY_CLEAR = 0x07,
	ID_KEY_BACKSPACE = 0x08,
	ID_KEY_ENTER = 0x0D,
	ID_KEY_UP = 0x0E,
	ID_KEY_DOWN = 0x0F,
	ID_KEY_PAGE_UP = 0x10,
	ID_KEY_PAGE_DOWN = 0x11,
	ID_KEY_TAB = 0x12
};

// Action codes
enum {
	// Flash 1 and 2 actions
	sactionHasLength = 0x80,
	sactionNone = 0x00,
	sactionGotoFrame = 0x81, // frame num (WORD)
	sactionGetURL = 0x83,	// url (STR), window (STR)
	sactionNextFrame = 0x04,
	sactionPrevFrame = 0x05,
	sactionPlay = 0x06,
	sactionStop = 0x07,
	sactionToggleQuality = 0x08,
	sactionStopSounds = 0x09,
	sactionWaitForFrame = 0x8A, // frame needed (WORD), actions to skip (BYTE)

	// Flash 3 Actions
	sactionSetTarget = 0x8B, // name (STR)
	sactionGotoLabel = 0x8C, // name (STR)

	// Flash 4 Actions
	sactionAdd = 0x0A,			   // Stack IN: number, number, OUT: number
	sactionSubtract = 0x0B,		   // Stack IN: number, number, OUT: number
	sactionMultiply = 0x0C,		   // Stack IN: number, number, OUT: number
	sactionDivide = 0x0D,		   // Stack IN: dividend, divisor, OUT: number
	sactionEquals = 0x0E,		   // Stack IN: number, number, OUT: bool
	sactionLess = 0x0F,			   // Stack IN: number, number, OUT: bool
	sactionAnd = 0x10,			   // Stack IN: bool, bool, OUT: bool
	sactionOr = 0x11,			   // Stack IN: bool, bool, OUT: bool
	sactionNot = 0x12,			   // Stack IN: bool, OUT: bool
	sactionStringEquals = 0x13,	// Stack IN: string, string, OUT: bool
	sactionStringLength = 0x14,	// Stack IN: string, OUT: number
	sactionStringAdd = 0x21,	   // Stack IN: string, strng, OUT: string
	sactionStringExtract = 0x15,   // Stack IN: string, index, count, OUT: substring
	sactionPush = 0x96,			   // type (BYTE), value (STRING or FLOAT)
	sactionPop = 0x17,			   // no arguments
	sactionToInteger = 0x18,	   // Stack IN: number, OUT: integer
	sactionJump = 0x99,			   // offset (WORD)
	sactionIf = 0x9D,			   // offset (WORD) Stack IN: bool
	sactionCall = 0x9E,			   // Stack IN: name
	sactionGetVariable = 0x1C,	 // Stack IN: name, OUT: value
	sactionSetVariable = 0x1D,	 // Stack IN: name, value
	sactionGetURL2 = 0x9A,		   // method (BYTE) Stack IN: url, window
	sactionGotoFrame2 = 0x9F,	  // flags (BYTE) Stack IN: frame
	sactionSetTarget2 = 0x20,	  // Stack IN: target
	sactionGetProperty = 0x22,	 // Stack IN: target, property, OUT: value
	sactionSetProperty = 0x23,	 // Stack IN: target, property, value
	sactionCloneSprite = 0x24,	 // Stack IN: source, target, depth
	sactionRemoveSprite = 0x25,	// Stack IN: target
	sactionTrace = 0x26,		   // Stack IN: message
	sactionStartDrag = 0x27,	   // Stack IN: no constraint: 0, center, target
								   //           constraint: x1, y1, x2, y2, 1, center, target
	sactionEndDrag = 0x28,		   // no arguments
	sactionStringLess = 0x29,	  // Stack IN: string, string, OUT: bool
	sactionWaitForFrame2 = 0x8D,   // skipCount (BYTE) Stack IN: frame
	sactionRandomNumber = 0x30,	// Stack IN: maximum, OUT: result
	sactionMBStringLength = 0x31,  // Stack IN: string, OUT: length
	sactionCharToAscii = 0x32,	 // Stack IN: character, OUT: ASCII code
	sactionAsciiToChar = 0x33,	 // Stack IN: ASCII code, OUT: character
	sactionGetTime = 0x34,		   // Stack OUT: milliseconds since Player start
	sactionMBStringExtract = 0x35, // Stack IN: string, index, count, OUT: substring
	sactionMBCharToAscii = 0x36,   // Stack IN: character, OUT: ASCII code
	sactionMBAsciiToChar = 0x37,   // Stack IN: ASCII code, OUT: character

	// Flash 5 Actions
	sactionConstantPool = 0x88, // create a set of constant
	sactionLess2 = 0x48,		// Stack IN: number, number, OUT: bool
	sactionEquals2 = 0x49,		// Stack IN: number, number, OUT: bool
	sactionCallMethod = 0x52,   // Stack IN: string, string, number, [number] OUT: return value or undefined

	// Reserved for Quicktime
	sactionQuickTime = 0xAA // I think this is what they are using...
};

enum {
	kStringType = 0,
	kFloatType = 1
};

enum {
	kSpritePosX = 0,
	kSpritePosY,
	kSpriteScaleX,
	kSpriteScaleY,
	kSpriteCurFrame,	// (can only get but not set)
	kSpriteTotalframes, // (can only get but  not set)
	kSpriteAlpha,		// (a value between 0 and 100 %)
	kSpriteVisible,		// (if zero this means we don't hit test the object)
	kSpriteWidth,		// (can only get, but not set)
	kSpriteHeight,		// (can only get, but not set),
	kSpriteRotate,
	kSpriteTarget,
	kSpriteLastFrameLoaded,
	kSpriteName,
	kSpriteDropTarget,
	kSpriteURL,
	kSpriteHighQuality,	// (global)
	kSpriteFocusRect,	  // (global)
	kSpriteSoundBufferTime // (global)
};

// Mouse target conditions
enum {
	stargetMouseEnter = 1,
	stargetMouseExit = 2,
	stargetMouseDown = 3,
	stargetMouseUp = 4
};

// Bitmap Alpha types
enum {
	sbitsAlphaFlag = 0, // just a flag that the alpha channel is valid
	sbitsAlphaCTab = 1, // alpha values for a color table
	sbitsAlphaMask = 2  // a complete alpha mask for a jpeg image
};

// Server Packet Flags
enum {
	spktObject = 0x00, // packet types
	spktFrame = 0x01,
	spktMask = 0x03,

	spktResend = 0x04, // flags for object packets

	spktSeekPoint = 0x04, // flags for frame packets
	spktKeyFrame = 0x08

	// Upper 4 bits are reserved for a sequence number
};

// Template Text Flags.
enum {
	stextEnd = 0x00,	   // end of text flag
	stextStyle = 0x80,	 // font style: followed by 8-bit flags (bold, italic, etc...)
	stextFont = 0x81,	  // font identifier: followed by 16-bit font identifier
	stextSize = 0x82,	  // font size: followed by 16-bit value in twips
	stextColor = 0x83,	 // font color: followed by 32-bit RGB value
	stextPosition = 0x84,  // font position: followed by 8-bit position (normal, super or subscript)
	stextKerning = 0x85,   // font kerning: followed by 16-bit kerning value in twips
	stextReserved1 = 0x86, // reserved value
	stextReserved2 = 0x87, // reserved value
	stextAlignment = 0x88, // paragraph alignment: followed by 8-bit alignment value
	stextIndent = 0x89,	// paragraph alignment: followed by 16-bit indent value in twips
	stextLMargin = 0x8a,   // paragraph left margin: followed by 16-bit left margin value in twips
	stextRMargin = 0x8b,   // paragraph right margin: followed by 16-bit right margin value in twips
	stextLeading = 0x8c,   // paragraph leading: followed by 16-bit leading value in twips
	stextReserved3 = 0x8d, // reserved value
	stextReserved4 = 0x8e, // reserved value
	stextReserved5 = 0x8f  // reserved value
};

// Template Text Style Flags
enum {
	stextStyleNone = 0x00,
	stextStyleBold = 0x01,
	stextStyleItalic = 0x02
	// 6 bits left for expansion
};

// Template Text Position Values
enum {
	stextPosNormal = 0x00,
	stextPosSuperScript = 0x01,
	stextPosSubScript = 0x02
};

// Template Text Alignment Values
enum {
	stextAlignLeft = 0x00,
	stextAlignRight = 0x01,
	stextAlignCenter = 0x02,
	stextAlignJustify = 0x03
};

// Template Text Flags
enum {
	sfontFlagsBold = 0x01,
	sfontFlagsItalic = 0x02,
	sfontFlagsWideCodes = 0x04,
	sfontFlagsWideOffsets = 0x08,
	sfontFlagsANSI = 0x10,
	sfontFlagsUnicode = 0x20,
	sfontFlagsShiftJIS = 0x40,
	sfontFlagsHasLayout = 0x80
};

// GetURL2 methods
enum {
	kHttpDontSend = 0,
	kHttpSendUseGet = 1,
	kHttpSendUsePost = 2,
	kHttpLoadTarget = 0x40,
	kHttpLoadVariables = 0x80
};

// Edit Text Flags
enum {
	seditTextFlagsHasFont = 0x0001,
	seditTextFlagsHasMaxLength = 0x0002,
	seditTextFlagsHasTextColor = 0x0004,
	seditTextFlagsReadOnly = 0x0008,
	seditTextFlagsPassword = 0x0010,
	seditTextFlagsMultiline = 0x0020,
	seditTextFlagsWordWrap = 0x0040,
	seditTextFlagsHasText = 0x0080,
	seditTextFlagsUseOutlines = 0x0100,
	seditTextFlagsBorder = 0x0800,
	seditTextFlagsNoSelect = 0x1000,
	seditTextFlagsHasLayout = 0x2000
};

// Drag constrants
enum {
	sdragFromPoint = 0,
	sdragFromCenter = 1
};
enum {
	sdragNoConstraint = 0,
	sdragRectConstraint = 1
};

#endif
