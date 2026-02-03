#pragma once

#ifndef FULLCOLORBRUSHTOOL_H
#define FULLCOLORBRUSHTOOL_H

#include <ctime>

#include <tools/inputmanager.h>
#ifndef NDEBUG
#include <tools/modifiers/modifiertest.h>
#endif
#include <tools/modifiers/modifierline.h>
#include <tools/modifiers/modifiertangents.h>
#include <tools/modifiers/modifierassistants.h>
#include <tools/modifiers/modifiersegmentation.h>

#include "toonzrasterbrushtool.h"
#include "mypainttoonzbrush.h"
#include "toonz/mypaintbrushstyle.h"
#include <QElapsedTimer>

//==============================================================

//  Forward declarations

class TTileSetFullColor;
class TTileSaverFullColor;
class MyPaintToonzBrush;
class FullColorBrushToolNotifier;
namespace mypaint {
class Brush;
}

//==============================================================

//************************************************************************
//    FullColor Brush Tool declaration
//************************************************************************

class FullColorBrushTool final : public TTool,
                                 public RasterController,
                                 public TInputHandler {
  Q_DECLARE_TR_FUNCTIONS(FullColorBrushTool)
public:
  class TrackHandler : public TTrackHandler {
  public:
    MyPaintToonzBrush brush;

    TrackHandler(const TRaster32P &ras, RasterController &controller,
                 const mypaint::Brush &brush)
        : brush(ras, controller, brush) {}
  };

private:
  void updateCurrentStyle();
  void applyClassicToonzBrushSettings(mypaint::Brush &mypaintBrush);
  void applyToonzBrushSettings(mypaint::Brush &mypaintBrush);

public:
  FullColorBrushTool(std::string name);

  ToolType getToolType() const override
    { return TTool::LevelWriteTool; }
  unsigned int getToolHints() const override;

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

  void inputMouseMove(const TPointD &position,
                      const TInputState &state) override;
  void inputSetBusy(bool busy) override;
  void inputPaintTrackPoint(const TTrackPoint &point, const TTrack &track,
                            bool firstTrack, bool preview) override;
  void inputInvalidateRect(const TRectD &bounds) override;
  TTool *inputGetTool() override { return this; };

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

  void loadLastBrush();

  void onCanvasSizeChanged();
  void onColorStyleChanged();

  TMyPaintBrushStyle *getBrushStyle();

private:
  void updateModifiers();
  
  enum MouseEventType { ME_DOWN, ME_DRAG, ME_UP, ME_MOVE };
  void handleMouseEvent(MouseEventType type, const TPointD &pos,
                        const TMouseEvent &e);

protected:
  TInputManager m_inputmanager;
#ifndef NDEBUG
  TSmartPointerT<TModifierTest> m_modifierTest;
#endif
  TSmartPointerT<TModifierLine> m_modifierLine;
  TSmartPointerT<TModifierTangents> m_modifierTangents;
  TSmartPointerT<TModifierAssistants> m_modifierAssistants;
  TSmartPointerT<TModifierSegmentation> m_modifierSegmentation;
  TInputModifier::List m_modifierReplicate;

  TPropertyGroup m_prop;

  TIntPairProperty m_thickness;
  TBoolProperty m_pressure;
  TDoublePairProperty m_opacity;
  TDoubleProperty m_hardness;
  TDoubleProperty m_modifierSize;
  TDoubleProperty m_modifierOpacity;
  TBoolProperty m_modifierEraser;
  TBoolProperty m_modifierLockAlpha;
  TBoolProperty m_assistants;
  TEnumProperty m_preset;

  TPixel32 m_currentColor;
  bool m_enabledPressure;
  int m_minCursorThick, m_maxCursorThick;

  TPointD m_mousePos,  //!< Current mouse position, in world coordinates.
      m_brushPos;      //!< World position the brush will be painted at.

  TRasterP m_backUpRas;
  TRaster32P m_workRaster;

  TRect m_strokeRect, m_strokeSegmentRect, m_lastRect;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance
  FullColorBrushToolNotifier *m_notifier;

  bool m_presetsLoaded;
  bool m_firstTime;

  bool m_started;

  bool m_propertyUpdating = false;
};

//------------------------------------------------------------

class FullColorBrushToolNotifier final : public QObject {
  Q_OBJECT

  FullColorBrushTool *m_tool;

public:
  FullColorBrushToolNotifier(FullColorBrushTool *tool);
  void onActivate();
  void onDeactivate();

protected slots:
  void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // FULLCOLORBRUSHTOOL_H
