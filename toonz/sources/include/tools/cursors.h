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
  PickerCursorOrganize,

  PickerRGB,
  PickerRGBWhite,

  FillCursorL,

  MoveEWCursor,
  MoveNSCursor,
  DisableCursor,
  MoveZCursor,
  ScaleGlobalCursor,
  ScaleHVCursor,
  FxGadgetCursor,
  RulerModifyCursor,
  RulerNewCursor,

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
  Ex_HV               = 0x100000
};

}  // namespace

#endif
