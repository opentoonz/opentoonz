

#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tools/cursors.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/strokegenerator.h"
#include "toonz/txshsimplelevel.h"

#include "tenv.h"

#include "tmathutil.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tproperty.h"
#include "tgl.h"
#include "drawutil.h"
#include "thidelinesegment.h"
#include "tconvert.h"

#include <QCoreApplication>

using namespace ToolUtils;

TEnv::StringVar HideLineType("InknpaintHideLineType", "Segment");
TEnv::StringVar HideLineMode("InknpaintHideLineMode", "Invisible");
TEnv::IntVar HideLineSelective("InknpaintHideLineSelective", 0);
TEnv::IntVar HideLineUnhide("InknpaintHideLineUnhide", 0);
TEnv::DoubleVar HideLineSize("InknpaintHideLineSize", 10);

namespace {

#define NORMAL_HIDE L"Normal"
#define SEGMENT_HIDE L"Segment"
#define FREEHAND_HIDE L"Freehand"
#define INVISIBLE_MODE L"Invisible"
#define HIDDEN_MODE L"Hidden"

const double minDistance2 = 16.0;

THideLineMode toHideLineMode(const std::wstring &value) {
  return value == HIDDEN_MODE ? THideLineMode::Hidden : THideLineMode::Invisible;
}

double brushRadiusFromToolSize(double toolSize) {
  const double minRange = 1;
  const double maxRange = 100;
  const double minSize  = 2;
  const double maxSize  = 100;
  return ((toolSize - minRange) / (maxRange - minRange) * (maxSize - minSize) +
          minSize) *
         0.5;
}

//-----------------------------------------------------------------------------

class UndoHideLine final : public ToolUtils::TToolUndo {
  std::map<int, std::vector<THideLineSegment>> m_oldSegments;
  std::map<int, std::vector<THideLineSegment>> m_newSegments;

public:
  UndoHideLine(TXshSimpleLevel *level, const TFrameId &frameId)
      : ToolUtils::TToolUndo(level, frameId) {}

  void addStrokeChange(int index, const std::vector<THideLineSegment> &oldSegs,
                       const std::vector<THideLineSegment> &newSegs) {
    if (m_oldSegments.find(index) == m_oldSegments.end())
      m_oldSegments[index] = oldSegs;
    m_newSegments[index] = newSegs;
  }

  std::vector<int> changedStrokeIndices() const {
    std::vector<int> indices;
    indices.reserve(m_oldSegments.size());
    for (const auto &kv : m_oldSegments) indices.push_back(kv.first);
    return indices;
  }

  bool empty() const { return m_oldSegments.empty(); }

  void undo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    for (const auto &kv : m_oldSegments) {
      if (kv.first < 0 ||
          static_cast<UINT>(kv.first) >= image->getStrokeCount())
        continue;
      image->setHideLineSegments(kv.first, kv.second);
    }
    notifyImageChanged();
  }

  void redo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    for (const auto &kv : m_newSegments) {
      if (kv.first < 0 ||
          static_cast<UINT>(kv.first) >= image->getStrokeCount())
        continue;
      image->setHideLineSegments(kv.first, kv.second);
    }
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this) + 500; }

  QString getToolName() override { return QString("Hide Line Tool"); }
};

//-----------------------------------------------------------------------------

class HideLineTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(HideLineTool)

  TPropertyGroup m_prop;
  TEnumProperty m_hideType;
  TEnumProperty m_hideMode;
  TBoolProperty m_unhide;
  TBoolProperty m_selective;
  TDoubleProperty m_toolSize;

  StrokeGenerator m_track;
  TStroke *m_stroke;
  UndoHideLine *m_undo;

  bool m_active;
  bool m_firstTime;
  double m_thick;
  double m_pointSize;
  double m_distance2;

  TPointD m_firstPos;
  TPointD m_oldMousePos;
  TPointD m_brushPos;
  TPointD m_mousePos;

  void updateBrushSize() {
    m_pointSize = brushRadiusFromToolSize(m_toolSize.getValue());
  }

  void loadToolOptionsIfNeeded() {
    if (!m_firstTime) return;
    m_toolSize.setValue(HideLineSize);
    m_hideType.setValue(::to_wstring(HideLineType.getValue()));
    m_hideMode.setValue(::to_wstring(HideLineMode.getValue()));
    m_unhide.setValue(HideLineUnhide ? 1 : 0);
    m_selective.setValue(HideLineSelective ? 1 : 0);
    m_firstTime = false;
  }

  UndoHideLine *ensureUndo() {
    if (m_undo) return m_undo;
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    m_undo = new UndoHideLine(level, getCurrentFid());
    return m_undo;
  }

  void commitUndo() {
    if (m_undo && !m_undo->empty()) {
      TUndoManager::manager()->add(m_undo);
    } else if (m_undo) {
      delete m_undo;
    }
    m_undo = nullptr;
  }

  void applyRangesToStroke(const TVectorImageP &vi, int strokeIndex,
                           const std::vector<DoublePair> &ranges,
                           THideLineMode mode, bool unhide) {
    if (ranges.empty()) return;
    std::vector<THideLineSegment> oldSegs =
        vi->getHideLineSegments(strokeIndex);
    if (unhide) {
      vi->removeHideLineSegmentsDuringEdit(strokeIndex, ranges);
    } else {
      vi->appendHideLineSegmentsDuringEdit(strokeIndex, ranges, mode);
    }
    ensureUndo()->addStrokeChange(strokeIndex, oldSegs,
                                  vi->getHideLineSegments(strokeIndex));
  }

  void applyRangesToStroke(const TVectorImageP &vi, int strokeIndex,
                           const std::vector<DoublePair> &ranges,
                           THideLineMode mode) {
    applyRangesToStroke(vi, strokeIndex, ranges, mode, false);
  }

  bool isUnhideMode() const { return m_unhide.getValue(); }

  bool isNormalType() const {
    return m_hideType.getValue() == NORMAL_HIDE;
  }

  void hideAtBrush(const TVectorImageP &vi, const TPointD &pos) {
    if (!vi) return;

    int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    THideLineMode mode = toHideLineMode(m_hideMode.getValue());

    TRectD circumscribedSquare(pos.x - m_pointSize, pos.y - m_pointSize,
                               pos.x + m_pointSize, pos.y + m_pointSize);
    if (!circumscribedSquare.overlaps(vi->getBBox())) return;

    QMutexLocker lock(vi->getMutex());
    for (UINT i = 0; i < vi->getStrokeCount(); ++i) {
      if (!vi->inCurrentGroup(i)) continue;
      TStroke *stroke = vi->getStroke(i);
      if (m_selective.getValue() && stroke->getStyle() != colorStyle) continue;

      std::vector<DoublePair> ranges =
          computeBrushHiddenRanges(stroke, pos, m_pointSize);
      applyRangesToStroke(vi, i, ranges, mode, isUnhideMode());
    }
  }

  void hideSegments(const TVectorImageP &vi, TStroke *lineStroke) {
    if (!vi || !lineStroke) return;

    int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    std::vector<StrokeSegmentRanges> touched = computeSegmentTouchRanges(
        vi, lineStroke, m_selective.getValue(), colorStyle);
    if (touched.empty()) return;

    THideLineMode mode = toHideLineMode(m_hideMode.getValue());
    QMutexLocker lock(vi->getMutex());
    for (const StrokeSegmentRanges &item : touched)
      applyRangesToStroke(vi, item.strokeIndex, item.ranges, mode,
                          isUnhideMode());
  }

  void hideRegion(const TVectorImageP &vi, TStroke *boundaryStroke) {
    if (!vi || !boundaryStroke) return;

    int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    std::vector<int> strokeIndices = findStrokesInClosedRegion(
        vi, boundaryStroke, m_selective.getValue(), colorStyle);
    if (strokeIndices.empty()) return;

    THideLineMode mode = toHideLineMode(m_hideMode.getValue());
    const std::vector<DoublePair> fullStrokeRange = {DoublePair(0.0, 1.0)};

    QMutexLocker lock(vi->getMutex());
    for (int index : strokeIndices)
      applyRangesToStroke(vi, index, fullStrokeRange, mode, isUnhideMode());
  }

  void startFreehandTrack(const TPointD &pos) {
    m_track.clear();
    m_firstPos = pos;
    m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
  }

  void dragFreehandTrack(const TPointD &pos) {
    m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
    invalidate(m_track.getModifiedRegion());
  }

  void closeFreehandTrack() {
    if (m_track.isEmpty()) return;
    m_track.add(TThickPoint(m_firstPos, m_thick),
                getPixelSize() * getPixelSize());
    m_track.filterPoints();
  }

  TStroke *makeTrackStroke(bool closeLoop) {
    if (m_track.isEmpty()) return nullptr;
    if (closeLoop) closeFreehandTrack();

    double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
    TStroke *stroke = m_track.makeStroke(error);
    if (stroke) stroke->setStyle(1);
    return stroke;
  }

  void finishEdit(const TVectorImageP &vi) {
    if (vi && m_undo && !m_undo->empty())
      vi->notifyHideLineFillChanged(m_undo->changedStrokeIndices());
    commitUndo();
    m_track.clear();
    if (vi) {
      notifyImageChanged();
      invalidate(vi->getBBox());
    } else {
      invalidate();
    }
  }

public:
  HideLineTool()
      : TTool("T_HideLine")
      , m_hideType("Type:")
      , m_hideMode("Mode:")
      , m_unhide("Unhide", false)
      , m_selective("Selective", false)
      , m_toolSize("Size:", 1, 100, 10)
      , m_stroke(nullptr)
      , m_undo(nullptr)
      , m_active(false)
      , m_firstTime(true)
      , m_thick(1.0)
      , m_pointSize(-1)
      , m_distance2(0.0) {
    bind(TTool::VectorImage);

    m_toolSize.setNonLinearSlider();

    m_prop.bind(m_toolSize);
    m_prop.bind(m_hideType);
    m_hideType.addValue(NORMAL_HIDE);
    m_hideType.addValue(SEGMENT_HIDE);
    m_hideType.addValue(FREEHAND_HIDE);
    m_prop.bind(m_hideMode);
    m_hideMode.addValue(INVISIBLE_MODE);
    m_hideMode.addValue(HIDDEN_MODE);
    m_prop.bind(m_unhide);
    m_prop.bind(m_selective);

    m_hideType.setId("Type");
    m_hideMode.setId("Mode");
    m_unhide.setId("Unhide");
    m_selective.setId("Selective");
    m_toolSize.setId("Size");

    updateBrushSize();
  }

  ~HideLineTool() {
    if (m_stroke) delete m_stroke;
    if (m_undo) delete m_undo;
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  TPropertyGroup *getProperties(int) override { return &m_prop; }

  int getCursorId() const override { return ToolCursor::HideLineCursor; }

  bool onPropertyChanged(std::string propertyName) override {
    HideLineSize         = m_toolSize.getValue();
    HideLineType         = ::to_string(m_hideType.getValue());
    HideLineMode         = ::to_string(m_hideMode.getValue());
    HideLineSelective    = m_selective.getValue();
    HideLineUnhide       = m_unhide.getValue();
    updateBrushSize();
    invalidate();
    return true;
  }

  void onActivate() override {
    loadToolOptionsIfNeeded();
    updateBrushSize();
    invalidate();
  }

  void onEnter() override {
    updateBrushSize();
    invalidate();
  }

  void onLeave() override {
    m_pointSize = -1;
    invalidate();
  }

  void updateTranslation() override {
    m_toolSize.setQStringName(tr("Size:"));
    m_hideType.setQStringName(tr("Type:"));
    m_hideType.setItemUIName(NORMAL_HIDE, tr("Normal"));
    m_hideType.setItemUIName(SEGMENT_HIDE, tr("Segment"));
    m_hideType.setItemUIName(FREEHAND_HIDE, tr("Freehand"));
    m_hideMode.setQStringName(tr("Mode:"));
    m_hideMode.setItemUIName(INVISIBLE_MODE, tr("Invisible"));
    m_hideMode.setItemUIName(HIDDEN_MODE, tr("Hidden"));
    m_unhide.setQStringName(tr("Unhide"));
    m_selective.setQStringName(tr("Selective"));
  }

  void draw() override {
    updateBrushSize();
    if (m_pointSize <= 0) return;

    double pixelSize2 = getPixelSize() * getPixelSize();
    m_thick           = pixelSize2 / 2.0;

    TImageP image(getImage(false));
    TVectorImageP vi = image;
    if (!vi) return;

    if (isNormalType()) {
      tglColor(TPixel32(255, 0, 255));
      tglDrawCircle(m_brushPos, m_pointSize);
      return;
    }

    if (m_track.isEmpty()) return;
    tglColor(TPixel32::Black);
    glPushMatrix();
    m_track.drawAllFragments();
    glPopMatrix();
  }

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    struct Locals {
      HideLineTool *m_this;

      void setValue(TDoubleProperty &prop, double value) {
        prop.setValue(value);
        m_this->onPropertyChanged(prop.getName());
        TTool::getApplication()->getCurrentTool()->notifyToolChanged();
      }

      void addValue(TDoubleProperty &prop, double add) {
        const TDoubleProperty::Range &range = prop.getRange();
        setValue(prop, tcrop(prop.getValue() + add, range.first, range.second));
      }
    } locals = {this};

    switch (e.getModifiersMask()) {
    case TMouseEvent::ALT_KEY: {
      const TPointD &diff = pos - m_mousePos;
      double add          = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;
      locals.addValue(m_toolSize, add);
      break;
    }
    default:
      m_brushPos = pos;
      break;
    }

    m_oldMousePos = m_mousePos = pos;
    invalidate();
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    m_active              = true;
    m_brushPos = m_mousePos = pos;

    if (isNormalType()) {
      m_oldMousePos = pos;
      m_distance2   = 0;
      if (TVectorImageP vi = getImage(true)) hideAtBrush(vi, pos);
      return;
    }

    startFreehandTrack(pos);
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    if (!m_active) return;
    m_brushPos = m_mousePos = pos;

    if (isNormalType()) {
      m_distance2 += tdistance2(m_oldMousePos, pos);
      if (m_distance2 < minDistance2 * getPixelSize() * getPixelSize()) return;
      m_distance2   = 0;
      m_oldMousePos = pos;
      if (TVectorImageP vi = getImage(true)) {
        hideAtBrush(vi, pos);
        invalidate(vi->getBBox());
      }
      return;
    }

    dragFreehandTrack(pos);
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {
    if (!m_active) return;
    m_active              = false;
    m_brushPos = m_mousePos = pos;

    TVectorImageP vi = getImage(true);
    if (!vi) {
      commitUndo();
      m_track.clear();
      invalidate();
      return;
    }

    if (isNormalType()) {
      finishEdit(vi);
      return;
    }

    if (m_hideType.getValue() == SEGMENT_HIDE) {
      if (m_stroke) delete m_stroke;
      m_stroke = makeTrackStroke(false);
      if (m_stroke) hideSegments(vi, m_stroke);
      finishEdit(vi);
      return;
    }

    if (m_hideType.getValue() == FREEHAND_HIDE) {
      if (m_stroke) delete m_stroke;
      m_stroke = makeTrackStroke(true);
      if (m_stroke) hideRegion(vi, m_stroke);
      finishEdit(vi);
    }
  }

  void onDeactivate() override {
    m_active = false;
    m_track.clear();
    if (m_undo) {
      delete m_undo;
      m_undo = nullptr;
    }
    m_pointSize = -1;
    invalidate();
  }
} hideLineTool;

}  // namespace
