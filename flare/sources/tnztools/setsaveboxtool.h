#pragma once

#ifndef SETSAVEBOXTOOL_INCLUDED
#define SETSAVEBOXTOOL_INCLUDED

#include "ttoonzimage.h"
#include "tools/tool.h"

//=============================================================================
// SetSaveboxTool
//-----------------------------------------------------------------------------

class SetSaveboxTool {
  TTool *m_tool;
  TPointD m_pos;
  TRectD m_modifiedRect;

  enum Moviment {
    eNone      = 0x1,
    eMoveRect  = 0x2,
    eMoveLeft  = 0x4,
    eMoveRight = 0x8,
    eMoveUp    = 0x10,
    eMoveDown  = 0x20
  } m_movementType;

  TToonzImage *getImage();
  int getDragType(const TPointD &pos);

public:
  SetSaveboxTool(TTool *tool);

  int getCursorId(const TPointD &pos);

  void leftButtonDown(const TPointD &pos);
  void leftButtonDrag(const TPointD &pos);
  void leftButtonUp(const TPointD &pos);

  void draw();
};

#endif  // SETSAVEBOXTOOL_INCLUDED
