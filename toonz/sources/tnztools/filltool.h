#pragma once

#ifndef FILLTOOL_H
#define FILLTOOL_H

// TnzCore includes
#include "tproperty.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/strokegenerator.h"
// TnzTools includes
#include "tools/tool.h"
#include "tools/toolutils.h"
#include "autofill.h"
#include "toonz/fill.h"

#include <QObject>

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

class NormalLineFillTool;
namespace {
class AreaFillTool {
public:
  enum Type { RECT, FREEHAND, POLYLINE };

private:
  bool m_frameRange;
  bool m_onlyUnfilled;
  Type m_type;

  bool m_selecting;
  TRectD m_selectingRect;

  TRectD m_firstRect;
  bool m_firstFrameSelected;
  TXshSimpleLevelP m_level;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  TTool *m_parent;
  std::wstring m_colorType;
  std::pair<int, int> m_currCell;
  StrokeGenerator m_track;
  std::vector<TPointD> m_polyline;
  bool m_isPath;
  bool m_active;
  bool m_enabled;
  double m_thick;
  TPointD m_firstPos;
  TStroke *m_firstStroke;
  TPointD m_mousePosition;
  bool m_onion;
  bool m_isLeftButtonPressed;
  bool m_autopaintLines;

public:
  AreaFillTool(TTool *Parent);
  void draw();
  void resetMulti();
  void leftButtonDown(const TPointD &pos, const TMouseEvent &, TImage *img);
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
  void mouseMove(const TPointD &pos, const TMouseEvent &e);
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e);
  void onImageChanged();
  bool onPropertyChanged(bool multi, bool onlyUnfilled, bool onion, Type type,
                         std::wstring colorType, bool autopaintLines);
  void onActivate();
  void onEnter();
};
}
class FillTool final : public QObject, public TTool {
  // Q_DECLARE_TR_FUNCTIONS(FillTool)
  Q_OBJECT
  bool m_firstTime;
  TPointD m_firstPoint, m_clickPoint;
  bool m_firstClick;
  bool m_frameSwitched             = false;
  double m_changedGapOriginalValue = -1.0;
  TXshSimpleLevelP m_level;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_onionStyleId;
  TEnumProperty m_colorType;  // Line, Area
  TEnumProperty m_fillType;   // Rect, Polyline etc.
  TBoolProperty m_onion;
  TBoolProperty m_frameRange;
  TBoolProperty m_selective;
  TDoublePairProperty m_fillDepth;
  TBoolProperty m_segment;
  TDoubleProperty m_maxGapDistance;
  AreaFillTool *m_rectFill;
  NormalLineFillTool *m_normalLineFillTool;

  TPropertyGroup m_prop;
  std::pair<int, int> m_currCell;
  std::vector<TFilledRegionInf> m_oldFillInformation;
#ifdef _DEBUG
  std::vector<TRect> m_rects;
#endif

  // For the raster fill tool, autopaint lines is optional and can be temporary
  // disabled
  TBoolProperty m_autopaintLines;

public:
  FillTool(int targetType);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  FillParameters getFillParameters() const;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void resetMulti();

  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;
  void draw() override;

  int pick(const TImageP &image, const TPointD &pos);
  int pickOnionColor(const TPointD &pos);

  void onEnter() override;

  void onActivate() override;
  void onDeactivate() override;

  int getCursorId() const override;

  int getColorClass() const { return 2; }
public slots:
  void onFrameSwitched() override;
};

#endif  // FILLTOOL_H
