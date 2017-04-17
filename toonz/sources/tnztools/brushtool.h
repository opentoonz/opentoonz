#pragma once

#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "tgeometry.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tcurves.h"

#include "toonz/strokegenerator.h"

#include "tools/tool.h"
#include "tools/cursors.h"
#include "tstroke.h"

#include <QCoreApplication>
#include <QRadialGradient>

#include <unordered_set>
//--------------------------------------------------------------

//  Forward declarations

class TTileSetCM32;
class TTileSaverCM32;
class RasterStrokeGenerator;
class BluredBrush;
class AnimationAutoComplete;

//--------------------------------------------------------------

//************************************************************************
//    Brush Data declaration
//************************************************************************

struct BrushData final : public TPersist {
  PERSIST_DECLARATION(BrushData)

  std::wstring m_name;
  double m_min, m_max, m_acc, m_smooth, m_hardness, m_opacityMin, m_opacityMax;
  bool m_selective, m_pencil, m_breakAngles, m_pressure;
  int m_cap, m_join, m_miter;

  BrushData();
  BrushData(const std::wstring &name);

  bool operator<(const BrushData &other) const { return m_name < other.m_name; }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************
//    Brush Preset Manager declaration
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
//<<<<<<< Updated upstream
//=======
//    Animation Auto-complete declaration
//    Detects repetitions in strokes within/across frames and predicts
//    the next stroke(s) accordingly.
//************************************************************************
typedef TThickQuadratic* SamplePoint;

/*class  PointWithStroke
{
public:
    PointWithStroke() {}
    PointWithStroke(SamplePoint point, TStroke* stroke) : point(point), stroke(stroke) {}
    ~PointWithStroke() {}
    SamplePoint point;
    TStroke* stroke;
    int index;
};

struct SimilarPair
{
    double dissimilarityFactor;
    PointWithStroke* point1;
    PointWithStroke* point2;
};


class GlobalSimilarityGraph
{
    std::map<SimilarPair*, std::vector<SimilarPair*>> connections;

public:
    int numberOfNodes = 0;

    GlobalSimilarityGraph() {}
    ~GlobalSimilarityGraph() {}
    void insertNode(SimilarPair* pair, std::vector<SimilarPair*> connections);
    std::vector<SimilarPair *> getConnections(SimilarPair* pair);
};*/

//typedef std::unordered_set< PointWithStroke *> SetOfPoints;


struct minOperationIndex
{
public:
    double score;
    TStroke* stroke;
};

/*
class StrokeWithNeighbours
{
public:
    TStroke* stroke;
    SetOfPoints neighbours;
    StrokeWithNeighbours *nextStroke;
};

class AnimationAutoComplete {
public:
  AnimationAutoComplete() {}
  ~AnimationAutoComplete() {}

  void addStroke(TStroke* stroke);
  std::vector<StrokeWithNeighbours*> getSynthesizedStrokes();

  //TODO: remove at production
  std::vector<TStroke*> drawSpaceVicinity(TStroke* stroke);


  TStroke* drawstrokeLine(TStroke* stroke);

  TPointD  getNormal(PointWithStroke* pointer);

private:
  int m_spaceVicinityRadius = 100;
  std::vector<StrokeWithNeighbours*> m_strokesWithNeighbours;
  std::vector<StrokeWithNeighbours*> m_synthesizedStrokes;
  std::vector<double> points;

  double operationsSimilarity (StrokeWithNeighbours* stroke1, StrokeWithNeighbours* stroke2);

  StrokeWithNeighbours* mostSimilarStroke (StrokeWithNeighbours* stroke);

  StrokeWithNeighbours *generateSynthesizedStroke(StrokeWithNeighbours* lastStroke,StrokeWithNeighbours* similarStroke,
                                                  StrokeWithNeighbours* nextToSimilarStroke);

  SimilarPair getMostSimilarPoint(PointWithStroke* point, TStroke* stroke);

  double pointsSimilarity (PointWithStroke* point1, PointWithStroke* point2);
  double getAppearanceSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getTemporalSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getSpatialSimilarity(PointWithStroke* point1, PointWithStroke* point2);
  double getNeighborhoodSimilarity(StrokeWithNeighbours* operation1, StrokeWithNeighbours* operation2);
  double magnitude(std::vector<double> points);

  SetOfPoints getNeighbours(const SamplePoint point);
  bool withinSpaceVicinity(const SamplePoint samplePoint, const SamplePoint point);
  void initializeSynthesis();
  void search(StrokeWithNeighbours *operation1);
  void assign();


};
*/
//************************************************************************
//>>>>>>> Stashed changes
//    Brush Tool declaration
//************************************************************************

class BrushTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(BrushTool)

public:
  BrushTool(std::string name, int targetType);

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

  void finishRasterBrush(const TPointD &pos, int pressureVal);
  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  bool isPencilModeActive() override;

  void addTrackPoint(const TThickPoint &point, double pixelSize2);
  void flushTrackPoint();

protected:
  TPropertyGroup m_prop[2];

  TDoublePairProperty m_thickness;
  TDoublePairProperty m_rasThickness;
  TDoubleProperty m_accuracy;
  TDoubleProperty m_smooth;
  TDoubleProperty m_hardness;
  TEnumProperty m_preset;
  TBoolProperty m_selective;
  TBoolProperty m_breakAngles;
  TBoolProperty m_pencil;
  TBoolProperty m_pressure;
  TEnumProperty m_capStyle;
  TEnumProperty m_joinStyle;
  TIntProperty m_miterJoinLimit;

  StrokeGenerator m_track;
  RasterStrokeGenerator *m_rasterTrack;

  AnimationAutoComplete *m_animationAutoComplete;

  TTileSetCM32 *m_tileSet;
  TTileSaverCM32 *m_tileSaver;

  TPixel32 m_currentColor;
  int m_styleId;
  double m_minThick, m_maxThick;

  TRectD m_modifiedRegion;
  TPointD m_dpiScale,
      m_mousePos,  //!< Current mouse position, in world coordinates.
      m_brushPos;  //!< World position the brush will be painted at.

  BluredBrush *m_bluredBrush;
  QRadialGradient m_brushPad;

  TRasterCM32P m_backupRas;
  TRaster32P m_workRas;

  std::vector<TThickPoint> m_points;
  TRect m_strokeRect, m_lastRect;

  SmoothStroke m_smoothStroke;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_active, m_enabled,
      m_isPrompting,  //!< Whether the tool is prompting for spline
                      //! substitution.
      m_firstTime, m_isPath, m_presetsLoaded;

  /*---
作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）に別のフレームに
移動していたときの不具合を修正する。---*/
  TFrameId m_workingFrameId;

protected:
  static void drawLine(const TPointD &point, const TPointD &centre,
                       bool horizontal, bool isDecimal);
  static void drawEmptyCircle(TPointD point, int thick, bool isLxEven,
                              bool isLyEven, bool isPencil);
};

#endif  // BRUSHTOOL_H
