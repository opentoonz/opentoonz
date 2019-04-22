#pragma once

#ifndef TOONZVECTORBRUSHTOOL_H
#define TOONZVECTORBRUSHTOOL_H

#include "tgeometry.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tstroke.h"
#include "toonz/strokegenerator.h"

#include "tools/tool.h"
#include "tools/cursors.h"

#include "toonzrasterbrushtool.h"

#include <QCoreApplication>
#include <QRadialGradient>

//--------------------------------------------------------------

//  Forward declarations

class TTileSetCM32;
class TTileSaverCM32;
class RasterStrokeGenerator;
class BluredBrush;

//--------------------------------------------------------------

//************************************************************************
//    Brush Data declaration
//************************************************************************

struct VectorBrushData final : public TPersist {
  PERSIST_DECLARATION(VectorBrushData)
  // frameRange, snapSensitivity and snap are not included
  // Those options are not really a part of the brush settings,
  // just the overall tool.

  std::wstring m_name;
  double m_min, m_max, m_acc, m_smooth;
  bool m_breakAngles, m_pressure;
  int m_cap, m_join, m_miter;

  VectorBrushData();
  VectorBrushData(const std::wstring &name);

  bool operator<(const VectorBrushData &other) const {
    return m_name < other.m_name;
  }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************
//    Brush Preset Manager declaration
//************************************************************************

class VectorBrushPresetManager {
  TFilePath m_fp;                       //!< Presets file path
  std::set<VectorBrushData> m_presets;  //!< Current presets container

public:
  VectorBrushPresetManager() {}

  void load(const TFilePath &fp);
  void save();

  const TFilePath &path() { return m_fp; };
  const std::set<VectorBrushData> &presets() const { return m_presets; }

  void addPreset(const VectorBrushData &data);
  void removePreset(const std::wstring &name);
};

//************************************************************************
//    Brush Tool declaration
//************************************************************************

class ToonzVectorBrushTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(BrushTool)

public:
  ToonzVectorBrushTool(std::string name, int targetType);

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
  bool keyDown(QKeyEvent *event) override;

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;
  void resetFrameRange();

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();

  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  bool isPencilModeActive() override;

  void addTrackPoint(const TThickPoint &point, double pixelSize2);
  void flushTrackPoint();
  bool doFrameRangeStrokes(TFrameId firstFrameId, TStroke *firstStroke,
                           TFrameId lastFrameId, TStroke *lastStroke,
                           bool drawFirstStroke = true);
  void checkGuideSnapping(bool beforeMousePress, bool invertCheck);
  void checkStrokeSnapping(bool beforeMousePress, bool invertCheck);

protected:
  TPropertyGroup m_prop[2];

  TDoublePairProperty m_thickness;
  TDoubleProperty m_accuracy;
  TDoubleProperty m_smooth;
  TEnumProperty m_preset;
  TBoolProperty m_breakAngles;
  TBoolProperty m_pressure;
  TBoolProperty m_snap;
  TEnumProperty m_frameRange;
  TEnumProperty m_snapSensitivity;
  TEnumProperty m_capStyle;
  TEnumProperty m_joinStyle;
  TIntProperty m_miterJoinLimit;

  StrokeGenerator m_track;
  StrokeGenerator m_rangeTrack;
  RasterStrokeGenerator *m_rasterTrack;
  TStroke *m_firstStroke;
  TTileSetCM32 *m_tileSet;
  TTileSaverCM32 *m_tileSaver;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  TPixel32 m_currentColor;
  int m_styleId;
  double m_minThick, m_maxThick;

  // for snapping and framerange
  int m_strokeIndex1, m_strokeIndex2, m_col, m_firstFrame, m_veryFirstFrame,
      m_veryFirstCol, m_targetType;
  double m_w1, m_w2, m_pixelSize, m_currThickness, m_minDistance2;
  bool m_foundFirstSnap = false, m_foundLastSnap = false, m_dragDraw = true,
       m_altPressed = false, m_snapSelf = false;
  TRectD m_modifiedRegion;
  TPointD m_dpiScale,
      m_mousePos,  //!< Current mouse position, in world coordinates.
      m_brushPos,  //!< World position the brush will be painted at.
      m_firstSnapPoint, m_lastSnapPoint;

  BluredBrush *m_bluredBrush;
  QRadialGradient m_brushPad;

  TRasterCM32P m_backupRas;
  TRaster32P m_workRas;

  std::vector<TThickPoint> m_points;
  TRect m_strokeRect, m_lastRect;

  SmoothStroke m_smoothStroke;

  VectorBrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_active, m_enabled,
      m_isPrompting,  //!< Whether the tool is prompting for spline
                      //! substitution.
      m_firstTime, m_isPath, m_presetsLoaded, m_firstFrameRange;

  /*---
  作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）に別のフレームに
  移動していたときの不具合を修正する。---*/
  TFrameId m_workingFrameId;
};

#endif  // TOONZVECTORBRUSHTOOL_H
