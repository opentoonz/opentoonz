#pragma once

#ifndef TABCURSORS_INCLUDED
#define TABCURSORS_INCLUDED

#include "tw/cursor.h"

namespace TabCursor {

enum {
  PenCursor = TCursorFactory::LAST_STOCK_CURSOR + 1,
  BenderCursor,
  CutterCursor,
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
  RotateCursor,
  ScaleCursor,
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
  TrackerCursor
};

void loadCursors();

}  // namespace

#endif
