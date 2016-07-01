#pragma once

#ifndef RULERTOOL_H
#define RULERTOOL_H

#include "tools/tool.h"
#include "tproperty.h"
#include "tools/tooloptions.h"

#include <QCoreApplication>

//----------------------------------------------------------------------------------------------

class RulerTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(RulerTool)

  enum DragMode { MakeNewRuler = 0, MoveFirstPos, MoveSecondPos, MoveRuler };

  TPointD m_firstPos, m_secondPos;
  TPointD m_mousePos;

  DragMode m_dragMode;

  bool m_justClicked;

  std::vector<RulerToolOptionsBox *> m_toolOptionsBox;

public:
  RulerTool();

  void setToolOptionsBox(RulerToolOptionsBox *toolOptionsBox);

  ToolType getToolType() const override { return TTool::GenericTool; }

  void onImageChanged() override;

  void draw() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void onActivate() override;

  int getCursorId() const override;

  void updateToolOption();

  /*--- 現在のマウス位置がRulerに十分近ければTrue ---*/
  bool isNearRuler();

private:
  /*--- 基準点に対し、マウス位置を0,45,90度にフィットさせた位置を返す ---*/
  TPointD getHVCoordinatedPos(TPointD p, TPointD centerPos);
};

#endif