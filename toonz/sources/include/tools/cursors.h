#pragma once

#ifndef CURSORS_INCLUDED
#define CURSORS_INCLUDED

namespace ToolCursor {

enum {
  CURSOR_NONE,                   // no cursor...
  CURSOR_DEFAULT = CURSOR_NONE,  // window class cursor...
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
  PenLargeCursor,
  PenCrosshairCursor,
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

  NormalEraserCursor,
  RectEraserCursor,
  PickerCursorOrganize,

  PickerRGBWhite,

  FillCursorL,

  MoveEWCursor,
  MoveNSCursor,
  DisableCursor,
  ScaleGlobalCursor,
  RulerModifyCursor,
  RulerNewCursor,

  // Base cursors with fixed set of decorations. See below
  FxGadgetCursorBase,
  EditFxCursorBase,
  MoveZCursorBase,
  PickerCursorLineBase,
  PickerCursorAreaBase,
  PickerRGBBase,
  ScaleHVCursorBase,

  // extra options for decorating the cursor
  Ex_Negate           = 0x100,  // used for black bg
  Ex_FreeHand         = 0x200,
  Ex_PolyLine         = 0x400,
  Ex_Rectangle        = 0x800,
  Ex_Line             = 0x1000,
  Ex_Area             = 0x2000,
  Ex_Fill_NoAutopaint = 0x4000,
  Ex_FX               = 0x8000,
  Ex_Z                = 0x10000,
  Ex_StyleLine        = 0x20000,
  Ex_StyleArea        = 0x40000,
  Ex_RGB              = 0x80000,
  Ex_HV               = 0x100000,

  // This section is for cursors that have fixed text that needs to
  // be handled separately when flipping for left-handed cursors.
  // The base gets flipped, but a left-handed version of text will be
  // used instead of flipped.
  FxGadgetCursor   = FxGadgetCursorBase | Ex_FX,
  EditFxCursor     = EditFxCursorBase | Ex_FX,
  MoveZCursor      = MoveZCursorBase | Ex_Z,
  PickerCursorLine = PickerCursorLineBase | Ex_StyleLine,
  PickerCursorArea = PickerCursorAreaBase | Ex_StyleArea,
  PickerRGB        = PickerRGBBase | Ex_RGB,
  ScaleHVCursor    = ScaleHVCursorBase | Ex_HV
};

}  // namespace

#endif
