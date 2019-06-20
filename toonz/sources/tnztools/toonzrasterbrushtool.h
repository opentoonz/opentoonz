#pragma once

#ifndef TOONZRASTERBRUSHTOOL_H
#define TOONZRASTERBRUSHTOOL_H

#include "tgeometry.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tstroke.h"
#include "toonz/strokegenerator.h"

#include "tools/tool.h"
#include "tools/cursors.h"
#include "mypainttoonzbrush.h"

#include <QCoreApplication>
#include <QRadialGradient>
#include <QElapsedTimer>

//--------------------------------------------------------------

//  Forward declarations

class TTileSetCM32;
class TTileSaverCM32;
class RasterStrokeGenerator;
class BluredBrush;
class ToonzRasterBrushToolNotifier;

//--------------------------------------------------------------

//************************************************************************
//  Toonz Raster Brush Data declaration
//************************************************************************

struct BrushData final : public TPersist {
  PERSIST_DECLARATION(BrushData)
  // frameRange, snapSensitivity and snap are not included
  // Those options are not really a part of the brush settings,
  // just the overall tool.

  std::wstring m_name;
  double m_min, m_max, m_smooth, m_hardness, m_opacityMin, m_opacityMax;
  bool m_pencil, m_pressure;
  int m_drawOrder;
  double m_modifierSize, m_modifierOpacity;
  bool m_modifierEraser, m_modifierLockAlpha;

  BrushData();
  BrushData(const std::wstring &name);

  bool operator<(const BrushData &other) const { return m_name < other.m_name; }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************
//   Toonz Raster Brush Preset Manager declaration
//************************************************************************

class BrushPresetManager {
  TFilePath m_fp;                 //!< Presets file path
  std::set<BrushData> m_presets;  //!< Current presets container

public:
  BrushPresetManager() {}

  void load(const TFilePath &fp);
  void save();

  const TFilePath &path() { return m_fp; };
  const std::set<BrushData> &presets() const { return m_presets; }

  void addPreset(const BrushData &data);
  void removePreset(const std::wstring &name);
};

//************************************************************************
//    Smooth Stroke declaration
//    Brush stroke smoothing buffer.
//************************************************************************
class SmoothStroke {
public:
  SmoothStroke() {}
  ~SmoothStroke() {}

  // begin stroke
  // smooth is smooth strength, from 0 to 100
  void beginStroke(int smooth);
  // add stroke point
  void addPoint(const TThickPoint &point);
  // end stroke
  void endStroke();
  // Get generated stroke points which has been smoothed.
  // Both addPoint() and endStroke() generate new smoothed points.
  // This method will removed generated points
  void getSmoothPoints(std::vector<TThickPoint> &smoothPoints);
  // Remove all points - used for straight lines
  void clearPoints();

private:
  void generatePoints();

private:
  int m_smooth;
  int m_outputIndex;
  int m_readIndex;
  std::vector<TThickPoint> m_rawPoints;
  std::vector<TThickPoint> m_outputPoints;
};
//************************************************************************
//   Toonz Raster Brush Tool declaration
//************************************************************************

class ToonzRasterBrushTool final : public TTool, public RasterController {
  Q_DECLARE_TR_FUNCTIONS(ToonzRasterBrushTool)

  void updateCurrentStyle();
  double restartBrushTimer();

public:
  ToonzRasterBrushTool(std::string name, int targetType);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  ToolOptionsBox *createOptionsBox() override;

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

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

  void finishRasterBrush(const TPointD &pos, double pressureVal);
  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  bool isPencilModeActive() override;

  void onColorStyleChanged();
  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;
  bool isMyPaintStyleSelected() { return m_isMyPaintStyleSelected; }

protected:
  TPropertyGroup m_prop[2];

  TDoublePairProperty m_rasThickness;
  TDoubleProperty m_smooth;
  TDoubleProperty m_hardness;
  TEnumProperty m_preset;
  TEnumProperty m_drawOrder;
  TBoolProperty m_pencil;
  TBoolProperty m_pressure;
  TDoubleProperty m_modifierSize;

  RasterStrokeGenerator *m_rasterTrack;
  TTileSetCM32 *m_tileSet;
  TTileSaverCM32 *m_tileSaver;
  TPixel32 m_currentColor;
  int m_styleId;
  double m_minThick, m_maxThick;

  int m_targetType;
  TPointD m_dpiScale,
      m_mousePos,  //!< Current mouse position, in world coordinates.
      m_brushPos;  //!< World position the brush will be painted at.

  BluredBrush *m_bluredBrush;
  QRadialGradient m_brushPad;

  TRasterCM32P m_backupRas;
  TRaster32P m_workRas;

  std::vector<TThickPoint> m_points;
  TRect m_strokeRect, m_lastRect,
      m_strokeSegmentRect;  // used with mypaint brush

  SmoothStroke m_smoothStroke;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_active, m_enabled,
      m_isPrompting,  //!< Whether the tool is prompting for spline
                      //! substitution.
      m_firstTime, m_presetsLoaded;

  /*---
作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）に別のフレームに
移動していたときの不具合を修正する。---*/
  TFrameId m_workingFrameId;

  ToonzRasterBrushToolNotifier *m_notifier;
  bool m_isMyPaintStyleSelected    = false;
  MyPaintToonzBrush *m_toonz_brush = 0;
  QElapsedTimer m_brushTimer;
  int m_minCursorThick, m_maxCursorThick;

protected:
  static void drawLine(const TPointD &point, const TPointD &centre,
                       bool horizontal, bool isDecimal);
  static void drawEmptyCircle(TPointD point, int thick, bool isLxEven,
                              bool isLyEven, bool isPencil);
};

//------------------------------------------------------------

class ToonzRasterBrushToolNotifier final : public QObject {
  Q_OBJECT

  ToonzRasterBrushTool *m_tool;

public:
  ToonzRasterBrushToolNotifier(ToonzRasterBrushTool *tool);

protected slots:
  // void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // TOONZRASTERBRUSHTOOL_H
