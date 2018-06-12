#pragma once

#ifndef VIEWTOOLS_H
#define VIEWTOOLS_H

#include "tools/tool.h"
#include "tstopwatch.h"
#include "tproperty.h"

//=============================================================================
// Rotate Tool
//-----------------------------------------------------------------------------

class RotateTool final : public QObject, public TTool {
  Q_OBJECT

  TStopWatch m_sw;
  TPointD m_oldPos;
  TPointD m_center;
  bool m_dragging;
  double m_angle;
  TPointD m_oldMousePos;
  TBoolProperty m_cameraCentered;
  TPropertyGroup m_prop;

public:
  RotateTool();

  ToolType getToolType() const override { return TTool::GenericTool; }
  void updateMatrix() override { return setMatrix(TAffine()); }
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override;

  int getCursorId() const override;

  void updateTranslation() {
    m_cameraCentered.setQStringName(tr("Rotate On Camera Center"));
  }
};

#endif VIEWTOOLS_H