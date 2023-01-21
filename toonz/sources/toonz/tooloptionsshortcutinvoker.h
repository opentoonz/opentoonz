#pragma once
#ifndef TOOLOPTIONSSHORTCUTINVOKER_H
#define TOOLOPTIONSSHORTCUTINVOKER_H

#include "tproperty.h"

#include <QObject>
#include <QMultiMap>
#include <QSet>

class TProperty;
class QAction;
class TTool;

namespace ToolOptionsShortcutWorker {
class BoolWorker;
}

class ToolOptionShortcutConnector final : public TProperty::Visitor {
  TTool* m_tool;

public:
  ToolOptionShortcutConnector(TTool* tool);

private:
  void visit(TDoubleProperty* p) override;
  void visit(TDoublePairProperty* p) override;
  void visit(TIntPairProperty* p) override;
  void visit(TIntProperty* p) override;
  void visit(TBoolProperty* p) override;
  void visit(TStringProperty* p) override {}
  void visit(TEnumProperty* p) override;
  void visit(TStyleIndexProperty* p) override {}
  void visit(TPointerProperty* p) override {}
};

//=============================================================================

namespace ToolOptionsShortcutWorker {

//-----------------------------------------------------------------

class DoubleWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TDoubleProperty* m_property;

public:
  DoubleWorker(QObject* parent, TTool* tool, TDoubleProperty* property)
      : QObject(parent), m_tool(tool), m_property(property) {}
protected slots:
  void increase(double step = 1.0);
  void increaseFractional();
  void decrease(double step = 1.0);
  void decreaseFractional();
};

//-----------------------------------------------------------------

class DoublePairWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TDoublePairProperty* m_property;

public:
  DoublePairWorker(QObject* parent, TTool* tool, TDoublePairProperty* property)
      : QObject(parent), m_tool(tool), m_property(property) {}
protected slots:
  void increaseMaxValue();
  void decreaseMaxValue();
  void increaseMinValue();
  void decreaseMinValue();
};

//-----------------------------------------------------------------

class IntPairWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TIntPairProperty* m_property;

public:
  IntPairWorker(QObject* parent, TTool* tool, TIntPairProperty* property)
      : QObject(parent), m_tool(tool), m_property(property) {}
protected slots:
  void increaseMaxValue();
  void decreaseMaxValue();
  void increaseMinValue();
  void decreaseMinValue();
};

//-----------------------------------------------------------------

class IntWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TIntProperty* m_property;

public:
  IntWorker(QObject* parent, TTool* tool, TIntProperty* property)
      : QObject(parent), m_tool(tool), m_property(property) {}
protected slots:
  void increase();
  void decrease();
};

//-----------------------------------------------------------------

class BoolWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TBoolProperty* m_property;
  QAction* m_action;

public:
  BoolWorker(QObject* parent, TTool* tool, TBoolProperty* property,
             QAction* action)
      : QObject(parent), m_tool(tool), m_property(property), m_action(action) {}
  void syncActionState();
protected slots:
  void doCheck(bool);
};

//-----------------------------------------------------------------

class EnumWorker : public QObject {
  Q_OBJECT
  TTool* m_tool;
  TEnumProperty* m_property;

public:
  EnumWorker(QObject* parent, TTool* tool, TEnumProperty* property)
      : QObject(parent), m_tool(tool), m_property(property) {}
protected slots:
  void cycleOptions();
  void doOnActivated(int);
};
}  // namespace ToolOptionsShortcutWorker

//=============================================================================

class ToolOptionsShortcutInvoker : public QObject  // singleton
{
  Q_OBJECT
  QSet<TTool*> m_tools;
  //ÉcÅ[ÉãÇ™êÿÇËë÷ÇÌÇ¡ÇΩÇÁ ActionÇÃCheckèÛë‘ÇìØä˙Ç≥ÇπÇÈ
  QMultiMap<TTool*, ToolOptionsShortcutWorker::BoolWorker*> m_checkProps;

public:
  static ToolOptionsShortcutInvoker* instance();
  void initialize();
  void notifyTool(TTool*, TProperty*, bool = false);
  void registerCheckProperty(TTool*, ToolOptionsShortcutWorker::BoolWorker*);

private:
  ToolOptionsShortcutInvoker(){};
protected slots:
  void onToolSwitched();

  /*-- Animate tool + mode switching shortcuts --*/
  void toggleEditNextMode();
  void toggleEditPosition();
  void toggleEditRotation();
  void toggleEditNextScale();
  void toggleEditNextShear();
  void toggleEditNextCenter();
  void toggleEditNextAll();

  /*-- Selection tool + mode switching shortcuts --*/
  void toggleSelectionNextType();
  void toggleSelectionRectangular();
  void toggleSelectionFreehand();
  void toggleSelectionPolyline();

  /*-- Geometric tool + shape switching shortcuts --*/
  void toggleGeometricNextShape();
  void toggleGeometricRectangle();
  void toggleGeometricCircle();
  void toggleGeometricEllipse();
  void toggleGeometricLine();
  void toggleGeometricPolyline();
  void toggleGeometricArc();
  void toggleGeometricMultiArc();
  void toggleGeometricPolygon();

  /*-- Type tool + style switching shortcuts --*/
  void toggleTypeNextStyle();
  void toggleTypeOblique();
  void toggleTypeRegular();
  void toggleTypeBoldOblique();
  void toggleTypeBold();

  /*-- Paint Brush tool + mode switching shortcuts --*/
  void togglePaintBrushNextMode();
  void togglePaintBrushAreas();
  void togglePaintBrushLines();
  void togglePaintBrushLinesAndAreas();

  /*-- Fill tool + mode switching shortcuts --*/
  void toggleFillNextType();
  void toggleFillNormal();
  void toggleFillRectangular();
  void toggleFillFreehand();
  void toggleFillPolyline();
  void toggleFillFreepick();
  void toggleFillNextMode();
  void toggleFillAreas();
  void toggleFillLines();
  void toggleFillLinesAndAreas();

  /*-- Eraser tool + type switching shortcuts --*/
  void toggleEraserNextType();
  void toggleEraserNormal();
  void toggleEraserRectangular();
  void toggleEraserFreehand();
  void toggleEraserPolyline();
  void toggleEraserSegment();

  /*-- Tape tool + type/mode switching shortcuts --*/
  void toggleTapeNextType();
  void toggleTapeNormal();
  void toggleTapeRectangular();
  void toggleTapeNextMode();
  void toggleTapeEndpointToEndpoint();
  void toggleTapeEndpointToLine();
  void toggleTapeLineToLine();

  /*-- Style Picker tool + mode switching shortcuts --*/
  void togglePickStyleNextMode();
  void togglePickStyleAreas();
  void togglePickStyleLines();
  void togglePickStyleLinesAndAreas();

  /*-- RGB Picker tool + type switching shortcuts --*/
  void toggleRGBPickerNextType();
  void toggleRGBPickerNormal();
  void toggleRGBPickerRectangular();
  void toggleRGBPickerFreehand();
  void toggleRGBPickerPolyline();

  /*-- Skeleton tool + mode switching shortcuts --*/
  void ToggleSkeletonNextMode();
  void ToggleSkeletonBuildSkeleton();
  void ToggleSkeletonAnimate();
  void ToggleSkeletonInverseKinematics();

  /*-- Plastic tool + mode switching shortcuts --*/
  void TogglePlasticNextMode();
  void TogglePlasticEditMesh();
  void TogglePlasticPaintRigid();
  void TogglePlasticBuildSkeleton();
  void TogglePlasticAnimate();
};

#endif