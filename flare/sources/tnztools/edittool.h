#pragma once

#ifndef EDITTOOL_INCLUDED
#define EDITTOOL_INCLUDED

#include "tool.h"
#include "tproperty.h"
#include "edittoolgadgets.h"

// For Qt translation support
#include <QCoreApplication>

using EditToolGadgets::DragTool;

//=============================================================================
// EditTool
//-----------------------------------------------------------------------------

class EditTool final : public QObject, public TTool {
  Q_OBJECT

  DragTool* m_dragTool;

  bool m_firstTime;

  enum {
    None        = -1,
    Translation = 1,
    Rotation,
    Scale,
    ScaleX,
    ScaleY,
    ScaleXY,
    Center,
    ZTranslation,
    Shear,
  };

  // DragInfo m_dragInfo;

  TPointD m_lastPos;
  TPointD m_curPos;
  TPointD m_firstPos;
  TPointD m_curCenter;

  bool m_active;
  bool m_keyFrameAdded;
  int m_what;
  int m_highlightedDevice;

  double m_oldValues[2];

  double m_currentScaleFactor;
  FxGadgetController* m_fxGadgetController;

  bool m_isAltPressed;

  TEnumProperty m_scaleConstraint;
  TEnumProperty m_autoSelect;
  TBoolProperty m_globalKeyframes;

  TBoolProperty m_lockCenterX;
  TBoolProperty m_lockCenterY;
  TBoolProperty m_lockPositionX;
  TBoolProperty m_lockPositionY;
  TBoolProperty m_lockRotation;
  TBoolProperty m_lockShearH;
  TBoolProperty m_lockShearV;
  TBoolProperty m_lockScaleH;
  TBoolProperty m_lockScaleV;
  TBoolProperty m_lockGlobalScale;

  TBoolProperty m_showEWNSposition;
  TBoolProperty m_showZposition;
  TBoolProperty m_showSOposition;
  TBoolProperty m_showRotation;
  TBoolProperty m_showGlobalScale;
  TBoolProperty m_showHVscale;
  TBoolProperty m_showShear;
  TBoolProperty m_showCenterPosition;

  TEnumProperty m_activeAxis;

  TPropertyGroup m_prop;

  void drawMainHandle();
  void onEditAllLeftButtonDown(TPointD& pos, const TMouseEvent& e);

public:
  EditTool();
  ~EditTool();

  ToolType getToolType() const override { return TTool::ColumnTool; }

  bool doesApply() const;  // ritorna vero se posso deformare l'oggetto corrente
  void saveOldValues();
  bool transformEnabled() const;

  const TStroke* getSpline() const;

  void rotate();
  void move();
  void moveCenter();
  void scale();
  void isoScale();
  void squeeze();
  void shear(const TPointD& pos, bool single);

  void updateTranslation() override;

  void leftButtonDown(const TPointD& pos, const TMouseEvent&) override;
  void leftButtonDrag(const TPointD& pos, const TMouseEvent&) override;
  void leftButtonUp(const TPointD& pos, const TMouseEvent&) override;

  void mouseMove(const TPointD&, const TMouseEvent& e) override;

  void draw() override;

  void transform(const TAffine& aff);

  void onActivate() override;
  void onDeactivate() override;
  bool onPropertyChanged(std::string propertyName) override;

  void computeBBox();

  int getCursorId() const override;

  TPropertyGroup* getProperties(int targetType) override { return &m_prop; }

  void updateMatrix() override {
    setMatrix(
        getCurrentObjectParentMatrix2());  // getCurrentObjectParentMatrix());
  }

  void drawText(const TPointD& p, double unit, std::string text);

  QString updateEnabled(int rowIndex, int columnIndex) override;

signals:
  void clickFlipHorizontal();
  void clickFlipVertical();
  void clickRotateLeft();
  void clickRotateRight();
};

#endif  // EDITTOOL_INCLUDED
