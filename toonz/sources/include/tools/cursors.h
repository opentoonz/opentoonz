#pragma once

#ifndef CURSORS_INCLUDED
#define CURSORS_INCLUDED

namespace ToolCursor
{

enum {
	CURSOR_NONE,				  // no cursor...
	CURSOR_DEFAULT = CURSOR_NONE, // window class cursor...
	CURSOR_ARROW,
	CURSOR_HAND,
	CURSOR_HOURGLASS,
	CURSOR_NO,
	CURSOR_DUMMY,
#ifndef _WIN32
	CURSOR_DND,
	CURSOR_QUESTION,
#endif
	PenCursor,
	BenderCursor,
	CutterCursor,
	DistortCursor,
	EraserCursor,
	FillCursor,
	MoveCursor,
	FlipHCursor,
	FlipVCursor,
	IronCursor,
	LevelSelectCursor,
	MagnetCursor,
	PanCursor,
	PickerCursor,
	PickerCursorLine,
	PickerCursorArea,
	PumpCursor,
	RotCursor,
	RotTopLeft,
	RotBottomRight,
	RotBottomLeft,
	RotateCursor,
	ScaleCursor,
	ScaleInvCursor,
	ScaleHCursor,
	ScaleVCursor,
	StrokeSelectCursor,
	TapeCursor,
	TypeInCursor,
	TypeOutCursor,
	ZoomCursor,
	PinchCursor,
	PinchAngleCursor,
	PinchWaveCursor,
	SplineEditorCursor,
	SplineEditorCursorSelect,
	SplineEditorCursorAdd,
	TrackerCursor,
	ForbiddenCursor,
	EditFxCursor,

	NormalEraserCursor,
	RectEraserCursor,
	RectEraserCursorWhite,
	FillCursorWhite,
	TapeCursorWhite,
	PickerCursorWhite,
	PickerCursorWhiteLine,
	PickerCursorWhiteArea,

	PickerRGB,
	PickerRGBWhite,

	FillCursorF,
	FillCursorFWhite,
	FillCursorP,
	FillCursorPWhite,
	FillCursorR,
	FillCursorRWhite,

	FillCursorA,
	FillCursorAWhite,
	FillCursorAF,
	FillCursorAFWhite,
	FillCursorAP,
	FillCursorAPWhite,
	FillCursorAR,
	FillCursorARWhite,

	FillCursorL,
	FillCursorLWhite,
	FillCursorLF,
	FillCursorLFWhite,
	FillCursorLP,
	FillCursorLPWhite,
	FillCursorLR,
	FillCursorLRWhite,

	MoveEWCursor,
	MoveNSCursor,
	DisableCursor,
	MoveZCursor,
	ScaleGlobalCursor,
	ScaleHVCursor,
	FxGadgetCursor,
	RulerModifyCursor,
	RulerNewCursor
};

} // namespace

#endif
