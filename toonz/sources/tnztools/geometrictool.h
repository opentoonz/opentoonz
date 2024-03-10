#pragma once

#ifndef GEOMETRICTOOL_H
#define GEOMETRICTOOL_H

#include "tgeometry.h"
#include "tproperty.h"
#include "tools/tool.h"
#include "tools/cursors.h"
#include "mypainttoonzbrush.h"
#include "trasterimage.h"
#include <QObject>

// For Qt translation support
#include <QCoreApplication>

class Primitive;
class FullColorGeometricToolNotifier;
class TTileSaverFullColor;
class TTileSaverCM32;

//-----------------------------------------------------------------------------

class PrimitiveParam {
  Q_DECLARE_TR_FUNCTIONS(PrimitiveParam)

public:
  TDoubleProperty m_toolSize;
  TIntProperty m_rasterToolSize;
  TDoubleProperty m_opacity;
  TDoubleProperty m_hardness;
  TEnumProperty m_type;
  TIntProperty m_edgeCount;
  TBoolProperty m_rotate;
  TBoolProperty m_autogroup;
  TBoolProperty m_autofill;
  TBoolProperty m_smooth;
  TBoolProperty m_selective;
  TBoolProperty m_pencil;
  TEnumProperty m_capStyle;
  TEnumProperty m_joinStyle;
  TIntProperty m_miterJoinLimit;
  TBoolProperty m_snap;
  TEnumProperty m_snapSensitivity;
  // for mypaint styles
  TDoubleProperty m_modifierSize;
  TDoubleProperty m_modifierOpacity;

  TPropertyGroup m_prop[2];

  int m_targetType;

  // for snapping
  int m_strokeIndex1;
  double m_w1, m_pixelSize, m_currThickness, m_minDistance2;
  bool m_foundSnap = false;
  TPointD m_snapPoint;

  PrimitiveParam(int targetType);

  void updateTranslation();
};
//=============================================================================
// Geometric Tool
//-----------------------------------------------------------------------------

class GeometricTool final : public TTool, public RasterController {
protected:
  Primitive* m_primitive;
  std::map<std::wstring, Primitive*> m_primitiveTable;
  PrimitiveParam m_param;
  std::wstring m_typeCode;
  bool m_active;
  bool m_firstTime;

  // for both rotation and move
  bool m_isRotatingOrMoving;
  bool m_wasCtrlPressed;
  TStroke* m_rotatedStroke;
  TPointD m_originalCursorPos;
  TPointD m_currentCursorPos;
  TPixel32 m_color;

  // for rotation
  double m_lastRotateAngle;
  TPointD m_rotateCenter;

  // for move
  TPointD m_lastMoveStrokePos;
  TRect m_strokeRect;
  TRect m_lastRect;
  TRasterP m_workRaster;
  TTileSaverFullColor* m_tileSaver;
  TTileSaverCM32* m_tileSaverCM;
  FullColorGeometricToolNotifier* m_notifier;

public:
  GeometricTool(int targetType);
  ~GeometricTool();
  ToolType getToolType() const override { return TTool::LevelWriteTool; }
  void updateTranslation() override;

  void addPrimitive(Primitive* p);
  void changeType(std::wstring name);

  bool preLeftButtonDown() override;
  void leftButtonDown(const TPointD& p, const TMouseEvent& e) override;
  void leftButtonDrag(const TPointD& p, const TMouseEvent& e) override;
  void leftButtonUp(const TPointD& p, const TMouseEvent& e) override;
  void leftButtonDoubleClick(const TPointD& p, const TMouseEvent& e) override;
  bool keyDown(QKeyEvent* event) override;
  void onImageChanged() override;
  void onColorStyleChanged();
  void rightButtonDown(const TPointD& p, const TMouseEvent& e) override;
  void mouseMove(const TPointD& p, const TMouseEvent& e) override;
  void onActivate() override;
  void onDeactivate() override;
  void onEnter() override;
  void draw() override;
  int getCursorId() const override;
  int getColorClass() const { return 1; }
  TPropertyGroup* getProperties(int idx) override;
  bool onPropertyChanged(std::string propertyName) override;
  void addStroke();
  void addRasterMyPaintStroke(const TToonzImageP& ti, TStroke* stroke,
                              TXshSimpleLevel* sl, const TFrameId& id);
  void addFullColorMyPaintStroke(const TRasterImageP& ri, TStroke* stroke,
                                 TXshSimpleLevel* sl, const TFrameId& id);

  void updateWorkRaster(const TRect& rect);
  bool askRead(const TRect& rect) override;
  bool askWrite(const TRect& rect) override;
};

//------------------------------------------------------------

class FullColorGeometricToolNotifier final : public QObject {
  Q_OBJECT

  GeometricTool* m_tool;

public:
  FullColorGeometricToolNotifier(GeometricTool* tool);

protected slots:
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif