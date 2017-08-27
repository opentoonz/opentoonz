#pragma once

#ifndef FULLCOLORBRUSHTOOL_H
#define FULLCOLORBRUSHTOOL_H

#include <ctime>

#include "brushtool.h"
#include "mypainttoonzbrush.h"
#include "toonz/mypaintbrushstyle.h"
#include <QElapsedTimer>

//==============================================================

//  Forward declarations

class TTileSetFullColor;
class TTileSaverFullColor;
class MyPaintToonzBrush;
class FullColorBrushToolNotifier;
namespace mypaint { class Brush; }

//==============================================================

//************************************************************************
//    FullColor Brush Tool declaration
//************************************************************************

class FullColorBrushTool final : public TTool, public RasterController {
  Q_DECLARE_TR_FUNCTIONS(FullColorBrushTool)

  void updateCurrentStyle();
  double restartBrushTimer();
  void applyClassicToonzBrushSettings(mypaint::Brush &mypaintBrush);
  void applyToonzBrushSettings(mypaint::Brush &mypaintBrush);

public:
  FullColorBrushTool(std::string name);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  ToolOptionsBox *createOptionsBox() override;

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;

  bool preLeftButtonDown() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;

  void onImageChanged() override;
  void setWorkAndBackupImages();
  void updateWorkAndBackupRasters(const TRect &rect);

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();

  void onCanvasSizeChanged();
  void onColorStyleChanged();

  TMyPaintBrushStyle* getBrushStyle();

protected:
  TPropertyGroup m_prop;

  TIntPairProperty m_thickness;
  TBoolProperty m_pressure;
  TDoublePairProperty m_opacity;
  TDoubleProperty m_hardness;
  TDoubleProperty m_modifierSize;
  TDoubleProperty m_modifierOpacity;
  TEnumProperty m_preset;

  TPixel32 m_currentColor;
  bool m_enabledPressure;
  int m_minCursorThick, m_maxCursorThick;

  TPointD m_mousePos,  //!< Current mouse position, in world coordinates.
          m_brushPos;  //!< World position the brush will be painted at.

  TRasterP m_backUpRas;
  TRaster32P m_workRaster;

  TRect m_strokeRect, m_strokeSegmentRect, m_lastRect;

  MyPaintToonzBrush *m_toonz_brush;
  QElapsedTimer m_brushTimer;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance
  FullColorBrushToolNotifier *m_notifier;

  bool m_presetsLoaded;
  bool m_firstTime;
  bool m_mousePressed = false;
  TMouseEvent m_mouseEvent;
};

//------------------------------------------------------------

class FullColorBrushToolNotifier final : public QObject {
  Q_OBJECT

  FullColorBrushTool *m_tool;

public:
  FullColorBrushToolNotifier(FullColorBrushTool *tool);

protected slots:
  void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // FULLCOLORBRUSHTOOL_H
