#pragma once

#ifndef FILLTOOL_H
#define FILLTOOL_H

#include <string>

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
typedef std::vector<std::pair<TXshSimpleLevel *, TFrameId>> SlFidsPairs;
typedef std::map<std::string, TRaster32P> RefImgTable;

namespace {

class AreaFillTool {
public:
  enum Type { RECT, FREEHAND, POLYLINE, FREEPICK };

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

  int m_bckStyleId;

public:
  AreaFillTool(TTool *Parent);
  void draw();
  int pick(const TImageP &image, const TPointD &pos, const int frame, int mode);
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
}  // namespace

class FillTool final : public QObject, public TTool {
  // Q_DECLARE_TR_FUNCTIONS(FillTool)
  Q_OBJECT
  bool m_firstTime;
  TPointD m_firstPoint, m_clickPoint, m_mousePos;
  bool m_firstFrameSelected;
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
  TBoolProperty m_closeGap;
  TBoolProperty m_referFill;
  TDoubleProperty m_maxGapDistance;
  AreaFillTool *m_areaFillTool;
  NormalLineFillTool *m_normalLineFillTool;

  TPropertyGroup m_prop;
  struct cellPos {
    int col, row;
  } m_beginCell;

  std::vector<TFilledRegionInf> m_oldFillInformation;
#ifdef _DEBUG
  std::vector<TRect> m_rects;
#endif

  // For the raster fill tool, autopaint lines is optional and can be temporary
  // disabled
  TBoolProperty m_autopaintLines;

  SlFidsPairs m_slFidsPairs;
  RefImgTable m_refImgTable;// imageId

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

  bool onPropertyChanged(std::string propertyName, bool addToUndo) override;
  void onImageChanged() override;
  void draw() override;

  // if frame = -1 it uses current frame
  int pick(const TImageP &image, const TPointD &pos, const int frame = -1);
  int pickOnionColor(const TPointD &pos);

  void onEnter() override;

  void onActivate() override;
  void onDeactivate() override;

  int getCursorId() const override;

  int getColorClass() const { return 2; }

  void buildFillInfo(const FillParameters &params);
  void computeRefImgsIfNeeded(const FillParameters &params);

  const SlFidsPairs &getSlFidsPairs() const { return m_slFidsPairs; }
  const RefImgTable &getRefImgTable() const { return m_refImgTable; }

public slots:
  void onFrameSwitched() override;
};

#endif  // FILLTOOL_H
