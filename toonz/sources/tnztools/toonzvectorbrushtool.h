#pragma once

#ifndef TOONZVECTORBRUSHTOOL_H
#define TOONZVECTORBRUSHTOOL_H

#include <tgeometry.h>
#include <tproperty.h>
#include <trasterimage.h>
#include <ttoonzimage.h>
#include <tstroke.h>
#include <toonz/strokegenerator.h>
#include "toonz/preferences.h"
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
#include <vector>
#include <utility>

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
  int m_drawOrder;
  bool m_breakAngles, m_pressure;
  int m_cap, m_join, m_miter;
  
  // Style snapshot information (for strict preset restoration)
  int m_styleInfoVersion;       // 0 = old, 1 = full snapshot
  
  // Vector style info: Generated, Trail, or VectorBrush
  bool m_hasVectorStyle;        // true if non-solid style was active
  int m_vectorStyleTagId;       // TagId of the style (e.g., 3000 for VectorBrush)
  std::string m_vectorStyleName; // brush/pattern name for VectorBrush/Trail styles
  
  // Texture style info (kept for backward compatibility)
  bool m_hasTexture;            // true if Texture style was active
  std::string m_texturePath;    // path to texture image
  double m_textureScale;
  double m_textureRotation;
  double m_textureDispX;
  double m_textureDispY;
  double m_textureContrast;
  int m_textureType;            // 0=FIXED, 1=AUTOMATIC, 2=RANDOM
  bool m_textureIsPattern;
  
  // Generic style snapshot (version >= 2)
  // Replaces individual vector/texture fields for new presets.
  // Captures ALL parameters of ANY TColorStyle generically.
  bool m_hasStyleSnapshot;             // true if generic snapshot present
  int m_snapshotStyleTagId;            // TColorStyle::getTagId() for recreation
  std::string m_snapshotBrushIdName;   // TColorStyle::getBrushIdName()
  std::string m_snapshotFilePath;      // Primary file (texture image, VectorBrush)
  std::vector<std::pair<int, double>> m_snapshotParams; // (paramIndex, numericValue)

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

class ToonzVectorBrushTool final : public TTool, public TInputHandler {
  Q_DECLARE_TR_FUNCTIONS(ToonzVectorBrushTool)

public:
  ToonzVectorBrushTool(std::string name, int targetType);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }
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
  void inputInvalidateRect(const TRectD &bounds) override {
    invalidate(bounds);
  }
  TTool *inputGetTool() override { return this; };

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return Preferences::instance()->isUseStrokeEndCursor()
                 ? ToolCursor::CURSOR_NONE
                 : ToolCursor::PenCursor;
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
  typedef std::vector<TStroke *> StrokeList;
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
  TEnumProperty m_drawOrder;
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
  int m_col, m_firstFrame, m_veryFirstFrame, m_veryFirstCol, m_targetType;
  double m_pixelSize, m_minDistance2;

  bool m_snapped;
  bool m_snappedSelf;
  TPointD m_snapPoint;
  TPointD m_snapPointSelf;

  TPointD m_mousePos;  //!< Current mouse position, in world coordinates.
  TPointD m_brushPos;  //!< World position the brush will be painted at.

  VectorBrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance

  bool m_active, m_firstTime, m_isPath, m_presetsLoaded, m_firstFrameRange;

  bool m_propertyUpdating;
  double m_cameraDpi;
};

#endif  // TOONZVECTORBRUSHTOOL_H
