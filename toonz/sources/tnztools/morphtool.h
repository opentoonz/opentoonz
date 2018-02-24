#pragma once

#ifndef MORPH_TOOOL
#define MORPH_TOOOL

// #include "tstroke.h"
#include "tools/tool.h"
#include "tvectorimage.h"

class MorphTool {
  TPointD m_lastPos;
  TPointD m_curPos;
  TPointD m_firstPos;
  TAffine m_transformation;
  TPointD m_delta;
  TVectorImageP m_vi, m_vi2;

  bool m_active;
  bool m_dragging;
  double m_pixelSize;

public:
  MorphTool();
  ~MorphTool();

  void setImage(const TVectorImageP &vi);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e);

  bool keyDown(QKeyEvent *event);

  void draw();
};

#endif
