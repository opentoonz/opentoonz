

#ifndef TW_KEYCODES_INCLUDED
#define TW_KEYCODES_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TwConsts
{

enum {
	TK_Backspace = 8,
	TK_Return = 13,
	TK_LeftArrow = 1018,
	TK_RightArrow = 1019,
	TK_UpArrow = 1020,
	TK_DownArrow = 1021,
	TK_ShiftLeftArrow,
	TK_ShiftRightArrow,
	TK_ShiftUpArrow,
	TK_ShiftDownArrow,
	TK_Home = 22,
	TK_End = 23,
	TK_PageUp = 24,
	TK_PageDown = 25,
	TK_Esc = 27,

	TK_Delete = -22,
	TK_Insert = -23,
	TK_CapsLock = 1022,

	TK_F1 = -200,
	TK_F2,
	TK_F3,
	TK_F4,
	TK_F5,
	TK_F6,
	TK_F7,
	TK_F8,
	TK_F9,
	TK_F10,
	TK_F11,
	TK_F12

};

enum {
	TK_ShiftPressed = 0x1,
	TK_CtrlPressed = 0x2,
	TK_AltPressed = 0x3
};

} // namespace

DVAPI std::string getKeyName(int key, unsigned long flags);

#endif
