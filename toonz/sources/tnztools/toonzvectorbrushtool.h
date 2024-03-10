#pragma once

#ifndef TOONZVECTORBRUSHTOOL_H
#define TOONZVECTORBRUSHTOOL_H

#include <tgeometry.h>
#include <tproperty.h>
#include <trasterimage.h>
#include <ttoonzimage.h>
#include <tstroke.h>
#include <toonz/strokegenerator.h>

#include <tools/tool.h>
#include <tools/cursors.h>

#include <tools/inputmanager.h>
#include <tools/modifiers/modifierline.h>
#include <tools/modifiers/modifiertangents.h>
#include <tools/modifiers/modifierassistants.h>
#include <tools/modifiers/modifiersegmentation.h>
#include <tools/modifiers/modifiersimplify.h>
#include <tools/modifiers/modifiersmooth.h>
#ifndef NDEBUG
#include <tools/modifiers/modifiertest.h>
#endif

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

class ToonzVectorBrushTool final : public TTool,
                                   public TInputHandler
{
  Q_DECLARE_TR_FUNCTIONS(ToonzVectorBrushTool)

public:
  ToonzVectorBrushTool(std::string name, int targetType);

  ToolType getToolType() const override
    { return TTool::LevelWriteTool; }
  unsigned int getToolHints() const override;

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

  void inputMouseMove(const TPointD &position,
                      const TInputState &state) override;
  void inputSetBusy(bool busy) override;
  void inputPaintTracks(const TTrackList &tracks) override;
  void inputInvalidateRect(const TRectD &bounds) override { invalidate(bounds); }
  TTool *inputGetTool() override { return this; };

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return ToolCursor::PenCursor;
  }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;
  void resetFrameRange();

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();

  void loadLastBrush();

  // return true if the pencil mode is active in the Brush / PaintBrush / Eraser
  // Tools.
  bool isPencilModeActive() override;

  bool doFrameRangeStrokes(TFrameId firstFrameId, TStroke *firstStroke,
                           TFrameId lastFrameId, TStroke *lastStroke,
                           int interpolationType, bool breakAngles,
                           bool autoGroup = false, bool autoFill = false,
                           bool drawFirstStroke = true,
                           bool drawLastStroke = true, bool withUndo = true);
  bool doGuidedAutoInbetween(TFrameId cFid, const TVectorImageP &cvi,
                             TStroke *cStroke, bool breakAngles,
                             bool autoGroup = false, bool autoFill = false,
                             bool drawStroke = true);

protected:
  typedef std::vector<StrokeGenerator> TrackList;
  typedef std::vector<TStroke*> StrokeList;
  void deleteStrokes(StrokeList &strokes);
  void copyStrokes(StrokeList &dst, const StrokeList &src);

  void snap(const TPointD &pos, bool snapEnabled, bool withSelfSnap = false);

  void updateModifiers();
  
  enum MouseEventType { ME_DOWN, ME_DRAG, ME_UP, ME_MOVE };
  void handleMouseEvent(MouseEventType type, const TPointD &pos,
                        const TMouseEvent &e);

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
  TBoolProperty m_assistants;

  TInputManager m_inputmanager;
  TSmartPointerT<TModifierLine> m_modifierLine;
  TSmartPointerT<TModifierTangents> m_modifierTangents;
  TSmartPointerT<TModifierAssistants> m_modifierAssistants;
  TSmartPointerT<TModifierSegmentation> m_modifierSegmentation;
  TSmartPointerT<TModifierSegmentation> m_modifierSmoothSegmentation;
  TSmartPointerT<TModifierSmooth> m_modifierSmooth[3];
  TSmartPointerT<TModifierSimplify> m_modifierSimplify;
#ifndef NDEBUG
  TSmartPointerT<TModifierTest> m_modifierTest;
#endif
  TInputModifier::List m_modifierReplicate;
  
  TrackList m_tracks;
  TrackList m_rangeTracks;
  StrokeList m_firstStrokes;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  TPixel32 m_currentColor;
  int m_styleId;
  double m_minThick, m_maxThick;

  // for snapping and framerange
  int m_col, m_firstFrame, m_veryFirstFrame,
      m_veryFirstCol, m_targetType;
  double m_pixelSize, m_minDistance2;
  
  bool m_snapped;
  bool m_snappedSelf;
  TPointD m_snapPoint;
  TPointD m_snapPointSelf;
  
  TPointD m_mousePos;  //!< Current mouse position, in world coordinates.
  TPointD m_brushPos;  //!< World position the brush will be painted at.

  VectorBrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_active, m_firstTime, m_isPath,
       m_presetsLoaded, m_firstFrameRange;

  bool m_propertyUpdating;
};

#endif  // TOONZVECTORBRUSHTOOL_H
