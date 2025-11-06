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
  bool m_isCtrlPressed;
  bool m_isAltPressed;
  TPointD m_oldMousePos;
  TBoolProperty m_cameraCentered;
  TBoolProperty m_rotateByStep;
  TDoubleProperty m_rotateStepAngle;
  TDoubleProperty m_rotateCommandAngle;
  TPropertyGroup m_prop;
  bool m_firstTime = true;

public:
  RotateTool(std::string name);

  ToolType getToolType() const override { return TTool::GenericTool; }
  void updateMatrix() override { return setMatrix(TAffine()); }
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override;

  int getCursorId() const override;

  void onActivate() override;
  bool onPropertyChanged(std::string propertyName, bool addToUndo) override;

  void updateTranslation() override {
    m_cameraCentered.setQStringName(tr("Rotate On Camera Center"));
    m_rotateByStep.setQStringName(tr("Rotate by Step"));
    m_rotateStepAngle.setQStringName(tr("Step Angle:"));
    m_rotateCommandAngle.setQStringName(tr("Rotate Left/Right Angle:"));
  }
};

#endif  // VIEWTOOLS_H
