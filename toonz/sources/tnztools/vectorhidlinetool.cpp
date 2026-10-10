
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tools/tooloptions.h"
#include "tools/cursors.h"
#include "tooloptionscontrols.h"

#include "toonz/palettecontroller.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/strokegenerator.h"
#include "toonz/txshsimplelevel.h"

#include "tenv.h"

#include "tmathutil.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tproperty.h"
#include "tinbetween.h"
#include "tgl.h"
#include "drawutil.h"
#include "thidelinesegment.h"
#include "tconvert.h"

#include <QCoreApplication>
#include <algorithm>
#include <map>
#include <string>

using namespace ToolUtils;

TEnv::StringVar HideLineType("InknpaintHideLineType", "Segment");
TEnv::StringVar HideLineMode("InknpaintHideLineMode", "Invisible");
TEnv::IntVar HideLineSelective("InknpaintHideLineSelective", 0);
TEnv::IntVar HideLineUnhide("InknpaintHideLineUnhide", 0);
TEnv::IntVar HideLineRange("InknpaintHideLineRange", 0);
TEnv::IntVar HideLineLasso("InknpaintHideLineLasso", 0);
TEnv::IntVar HideLineFollowInk("InknpaintHideLineFollowInk", 0);
TEnv::StringVar HideLineInterpolation("InknpaintHideLineInterpolation",
                                      "Linear");
TEnv::DoubleVar HideLineSize("InknpaintHideLineSize", 10);

namespace {

#define NORMAL_HIDE L"Normal"
#define SEGMENT_HIDE L"Segment"
#define FREEHAND_HIDE L"Freehand"
#define INVISIBLE_MODE L"Invisible"
#define HIDDEN_MODE L"Hidden"
#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

const double minDistance2 = 16.0;

const TPixel32 rangeGuideColors[] = {
    TPixel32(255, 70, 70),  TPixel32(90, 255, 100),  TPixel32(80, 210, 255),
    TPixel32(255, 210, 50), TPixel32(255, 100, 255), TPixel32(50, 255, 230)};

TPixel32 rangeGuideColor(int index) {
  const int count =
      (int)(sizeof(rangeGuideColors) / sizeof(rangeGuideColors[0]));
  return rangeGuideColors[index % count];
}

void deleteRangeGuides(std::vector<TStroke *> &guides) {
  for (TStroke *stroke : guides) delete stroke;
  guides.clear();
}

THideLineMode toHideLineMode(const std::wstring &value) {
  return value == HIDDEN_MODE ? THideLineMode::Hidden
                              : THideLineMode::Invisible;
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

  void refreshViewer() const {
    TTool::Application *app = TTool::getApplication();
    if (!app || !app->getCurrentTool()) return;
    TTool *tool = app->getCurrentTool()->getTool();
    if (tool) tool->invalidate();
  }

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
    refreshViewer();
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
    refreshViewer();
  }

  int getSize() const override { return sizeof(*this) + 500; }

  QString getToolName() override { return QString("Hide Line Tool"); }
};

//-----------------------------------------------------------------------------

class HideLineToolOptionsBox final : public GenericToolOptionsBox {
  TEnumProperty *m_hideType;
  QWidget *m_lasso;
  QWidget *m_followInk;

public:
  HideLineToolOptionsBox(QWidget *parent, TTool *tool,
                         TPaletteHandle *pltHandle, ToolHandle *toolHandle,
                         TEnumProperty *hideType)
      : GenericToolOptionsBox(parent, tool, pltHandle, 0, toolHandle)
      , m_hideType(hideType)
      , m_lasso(dynamic_cast<ToolOptionCheckbox *>(control("Lasso")))
      , m_followInk(dynamic_cast<ToolOptionCheckbox *>(control("Follow Ink"))) {
    updateTypeDependentOptions();
  }

  void updateStatus() override {
    GenericToolOptionsBox::updateStatus();
    updateTypeDependentOptions();
  }

private:
  void updateTypeDependentOptions() {
    if (!m_hideType) return;
    const bool isNormal = m_hideType->getValue() == NORMAL_HIDE;
    if (m_lasso) m_lasso->setEnabled(isNormal);
    if (m_followInk) m_followInk->setEnabled(!isNormal);
  }
};

//-----------------------------------------------------------------------------

class HideLineTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(HideLineTool)

  enum class FrameRangeOperation { Segment, Region, Brush, Portion };

  TPropertyGroup m_prop;
  TEnumProperty m_hideType;
  TEnumProperty m_hideMode;
  TBoolProperty m_unhide;
  TBoolProperty m_selective;
  TBoolProperty m_lasso;
  TBoolProperty m_followInk;
  TBoolProperty m_frameRange;
  TEnumProperty m_interpolation;
  TDoubleProperty m_toolSize;

  StrokeGenerator m_track;
  TStroke *m_stroke;
  UndoHideLine *m_undo;

  std::vector<TStroke *> m_startRangeGuides;
  std::vector<TStroke *> m_endRangeGuides;
  std::vector<double> m_startRangeRadii;
  std::vector<double> m_endRangeRadii;
  TFrameId m_firstRangeFrameId;
  TFrameId m_lastRangeFrameId;
  TFrameId m_veryFirstFrameId;
  std::pair<int, int> m_currCell;
  TXshSimpleLevelP m_rangeLevel;
  bool m_useExplicitUndoFid;
  TFrameId m_explicitUndoFid;
  bool m_rangeCtrl;
  std::vector<TRectD> m_rangeBalloons;

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
    std::wstring hideType = ::to_wstring(HideLineType.getValue());
    if (hideType == L"Lasso") hideType = NORMAL_HIDE;
    m_hideType.setValue(hideType);
    m_hideMode.setValue(::to_wstring(HideLineMode.getValue()));
    m_unhide.setValue(HideLineUnhide ? 1 : 0);
    m_selective.setValue(HideLineSelective ? 1 : 0);
    m_lasso.setValue(HideLineLasso ? 1 : 0);
    m_followInk.setValue(HideLineFollowInk ? 1 : 0);
    m_frameRange.setValue(HideLineRange ? 1 : 0);
    m_interpolation.setValue(::to_wstring(HideLineInterpolation.getValue()));
    m_firstTime = false;
  }

  bool hasPendingRange() const { return !m_startRangeGuides.empty(); }

  void resetFrameRange() {
    deleteRangeGuides(m_startRangeGuides);
    deleteRangeGuides(m_endRangeGuides);
    m_startRangeRadii.clear();
    m_endRangeRadii.clear();
    m_rangeLevel         = nullptr;
    m_useExplicitUndoFid = false;
  }

  void rememberRangeOrigin() {
    TTool::Application *app = TTool::getApplication();
    if (!app || !app->getCurrentFrame()) return;
    m_veryFirstFrameId = getCurrentFid();
    if (app->getCurrentFrame()->isEditingScene() && app->getCurrentColumn())
      m_currCell =
          std::pair<int, int>(app->getCurrentColumn()->getColumnIndex(),
                              app->getCurrentFrame()->getFrame());
  }

  double overlayPixelSize() const {
    return std::min(1.0, std::max(0.05, getPixelSize()));
  }

  void drawRangeIndex(const TPointD &p, int index) {
    TPixel32 color = rangeGuideColor(index);
    color.m        = 220;
    drawBalloon(p, std::to_string(index + 1), color, TPoint(24, 22),
                getPixelSize(), false, &m_rangeBalloons);
  }

  TPointD firstPortionPoint(
      const TVectorImageP &vi,
      const std::map<int, std::vector<DoublePair>> &byStroke,
      const TPointD &fallback) const {
    if (!vi) return fallback;
    for (const auto &kv : byStroke) {
      if (kv.first < 0 || kv.first >= (int)vi->getStrokeCount()) continue;
      TStroke *stroke = vi->getStroke(kv.first);
      if (!stroke || kv.second.empty()) continue;
      const DoublePair &range = kv.second.front();
      return stroke->getPoint(0.5 * (range.first + range.second));
    }
    return fallback;
  }

  void drawOverlayStroke(const TStroke &stroke, double pixelSize, double w0,
                         double w1, const TPixel32 &color, float width,
                         bool dashed) {
    if (w0 > w1) std::swap(w0, w1);
    if (w1 - w0 < 1e-6) return;
    if (dashed) {
      glEnable(GL_LINE_STIPPLE);
      glLineStipple(1, 0x00FF);
    }
    glLineWidth(width + 2.0f);
    tglColor(TPixel32(255, 255, 255, 210));
    drawStrokeCenterline(stroke, pixelSize, w0, w1);
    glLineWidth(width);
    tglColor(color);
    drawStrokeCenterline(stroke, pixelSize, w0, w1);
    if (dashed) glDisable(GL_LINE_STIPPLE);
  }

  void drawRangeOutline(const TStroke &stroke, int index, bool emphasize) {
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_CURRENT_BIT |
                 GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawOverlayStroke(stroke, overlayPixelSize(), 0.0, 1.0,
                      rangeGuideColor(index), emphasize ? 2.5f : 2.0f, false);
    glLineWidth(1.0f);
    glPopAttrib();
  }

  void mergePreviewRanges(std::vector<DoublePair> &ranges) const {
    if (ranges.size() < 2) return;
    std::sort(ranges.begin(), ranges.end(),
              [](const DoublePair &a, const DoublePair &b) {
                return a.first < b.first;
              });
    std::vector<DoublePair> merged;
    merged.push_back(ranges[0]);
    for (size_t i = 1; i < ranges.size(); ++i) {
      if (merged.back().second < ranges[i].first)
        merged.push_back(ranges[i]);
      else if (merged.back().second < ranges[i].second)
        merged.back().second = ranges[i].second;
    }
    ranges.swap(merged);
  }

  void collectBrushPreviewRanges(
      const TVectorImageP &vi, const TStroke &path, double radius,
      std::map<int, std::vector<DoublePair>> &out) const {
    if (!vi || radius <= 0) return;
    const bool selective = m_selective.getValue();

    TTool::Application *app = TTool::getApplication();
    if (!app) return;
    const int colorStyle = app->getCurrentLevelStyleIndex();

    auto addAt = [&](const TPointD &pos) {
      for (UINT i = 0; i < vi->getStrokeCount(); ++i) {
        if (!vi->inCurrentGroup(i)) continue;
        TStroke *stroke = vi->getStroke(i);
        if (!stroke) continue;
        if (selective && stroke->getStyle() != colorStyle) continue;
        const std::vector<DoublePair> ranges =
            computeBrushHiddenRanges(stroke, pos, radius);
        if (!ranges.empty())
          out[(int)i].insert(out[(int)i].end(), ranges.begin(), ranges.end());
      }
    };

    const double length = path.getLength();
    if (length <= 1e-6)
      addAt(path.getPoint(0));
    else {
      const double step = radius * 0.5;
      for (double len = 0; len < length; len += step)
        addAt(path.getPointAtLength(len));
      addAt(path.getPointAtLength(length));
    }
  }

  void drawAffectedPortions(
      const TVectorImageP &vi,
      const std::map<int, std::vector<DoublePair>> &byStroke, int index,
      bool emphasize) {
    if (!vi) return;

    const double pixelSize = overlayPixelSize();
    const bool hidden =
        toHideLineMode(m_hideMode.getValue()) == THideLineMode::Hidden;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_CURRENT_BIT |
                 GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const TPixel32 color = rangeGuideColor(index);
    const float width    = emphasize ? 2.5f : 2.0f;
    for (const auto &kv : byStroke) {
      if (kv.first < 0 || kv.first >= (int)vi->getStrokeCount()) continue;
      TStroke *stroke = vi->getStroke(kv.first);
      if (!stroke) continue;
      std::vector<DoublePair> ranges = kv.second;
      mergePreviewRanges(ranges);
      for (const DoublePair &range : ranges)
        drawOverlayStroke(*stroke, pixelSize, range.first, range.second, color,
                          width, hidden);
    }

    glLineWidth(1.0f);
    glPopAttrib();
  }

  void collectSegmentPreviewRanges(
      const TVectorImageP &vi, TStroke *lineStroke,
      std::map<int, std::vector<DoublePair>> &out) const {
    if (!vi || !lineStroke) return;
    const int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    const std::vector<StrokeSegmentRanges> touched = computeSegmentTouchRanges(
        vi, lineStroke, m_selective.getValue(), colorStyle);
    for (const StrokeSegmentRanges &item : touched)
      out[item.strokeIndex].insert(out[item.strokeIndex].end(),
                                   item.ranges.begin(), item.ranges.end());
  }

  void collectRegionPreviewRanges(
      const TVectorImageP &vi, const TStroke *boundary,
      std::map<int, std::vector<DoublePair>> &out) const {
    if (!vi || !boundary) return;
    const int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    const std::vector<int> indices = findStrokesInClosedRegion(
        vi, boundary, m_selective.getValue(), colorStyle);
    for (int index : indices) out[index].push_back(DoublePair(0.0, 1.0));
  }

  void collectLassoPreviewRanges(
      const TVectorImageP &vi, const TStroke *boundary,
      std::map<int, std::vector<DoublePair>> &out) const {
    if (!vi || !boundary) return;
    const int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    const std::vector<StrokeSegmentRanges> touched = computeRegionPortionRanges(
        vi, boundary, m_selective.getValue(), colorStyle);
    for (const StrokeSegmentRanges &item : touched)
      out[item.strokeIndex].insert(out[item.strokeIndex].end(),
                                   item.ranges.begin(), item.ranges.end());
  }

  void drawPendingRangeOverlay(const TVectorImageP &vi, TStroke *guide,
                               double radius, int index, bool emphasize) {
    if (!guide) return;
    std::map<int, std::vector<DoublePair>> byStroke;
    if (radius > 0)
      collectBrushPreviewRanges(vi, *guide, radius, byStroke);
    else if (isLassoMode()) {
      drawRangeOutline(*guide, index, emphasize);
      collectLassoPreviewRanges(vi, guide, byStroke);
    } else if (m_followInk.getValue()) {
      if (m_hideType.getValue() == SEGMENT_HIDE)
        collectSegmentPreviewRanges(vi, guide, byStroke);
      else
        collectRegionPreviewRanges(vi, guide, byStroke);
    } else {
      drawRangeOutline(*guide, index, emphasize);
      drawRangeIndex(guide->getPoint(0), index);
      return;
    }

    drawAffectedPortions(vi, byStroke, index, emphasize);
    drawRangeIndex(firstPortionPoint(vi, byStroke, guide->getPoint(0)), index);
  }

  void drawRangeBrushEffect(const TVectorImageP &vi, const TStroke &path,
                            double radius, int index, bool emphasize,
                            bool withIndex) {
    if (!vi) return;
    std::map<int, std::vector<DoublePair>> byStroke;
    collectBrushPreviewRanges(vi, path, radius, byStroke);
    drawAffectedPortions(vi, byStroke, index, emphasize);
    if (withIndex)
      drawRangeIndex(firstPortionPoint(vi, byStroke, path.getPoint(0)), index);
  }

  void restoreRangeOrigin() {
    TTool::Application *app = TTool::getApplication();
    if (!app || !app->getCurrentFrame()) return;
    if (app->getCurrentFrame()->isEditingScene()) {
      if (app->getCurrentColumn())
        app->getCurrentColumn()->setColumnIndex(m_currCell.first);
      app->getCurrentFrame()->setFrame(m_currCell.second);
    } else
      app->getCurrentFrame()->setFid(m_veryFirstFrameId);
  }

  UndoHideLine *ensureUndo() {
    if (m_undo) return m_undo;
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    if (!level && m_rangeLevel) level = m_rangeLevel.getPointer();
    if (!level) return nullptr;

    const TFrameId fid =
        m_useExplicitUndoFid ? m_explicitUndoFid : getCurrentFid();
    m_undo = new UndoHideLine(level, fid);
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
    UndoHideLine *undo = ensureUndo();
    if (undo)
      undo->addStrokeChange(strokeIndex, oldSegs,
                            vi->getHideLineSegments(strokeIndex));
  }

  void applyRangesToStroke(const TVectorImageP &vi, int strokeIndex,
                           const std::vector<DoublePair> &ranges,
                           THideLineMode mode) {
    applyRangesToStroke(vi, strokeIndex, ranges, mode, false);
  }

  bool isUnhideMode() const { return m_unhide.getValue(); }

  bool isNormalType() const { return m_hideType.getValue() == NORMAL_HIDE; }

  bool isLassoMode() const { return isNormalType() && m_lasso.getValue(); }

  int livePairIndex() const {
    if (!hasPendingRange()) return 0;
    if (getCurrentFid() == m_firstRangeFrameId)
      return m_rangeCtrl ? (int)m_startRangeGuides.size() : 0;
    return (int)m_endRangeGuides.size();
  }

  bool isFrameRangeType() const {
    return m_hideType.getValue() == NORMAL_HIDE ||
           m_hideType.getValue() == SEGMENT_HIDE ||
           m_hideType.getValue() == FREEHAND_HIDE;
  }

  TInbetween::TweenAlgorithm interpolationAlgorithm() const {
    if (m_interpolation.getValue() == EASE_IN_INTERPOLATION)
      return TInbetween::EaseInInterpolation;
    if (m_interpolation.getValue() == EASE_OUT_INTERPOLATION)
      return TInbetween::EaseOutInterpolation;
    if (m_interpolation.getValue() == EASE_IN_OUT_INTERPOLATION)
      return TInbetween::EaseInOutInterpolation;
    return TInbetween::LinearInterpolation;
  }

  void hideAtBrushUnlocked(const TVectorImageP &vi, const TPointD &pos,
                           double radius) {
    int colorStyle     = TTool::getApplication()->getCurrentLevelStyleIndex();
    THideLineMode mode = toHideLineMode(m_hideMode.getValue());

    TRectD circumscribedSquare(pos.x - radius, pos.y - radius, pos.x + radius,
                               pos.y + radius);
    if (!circumscribedSquare.overlaps(vi->getBBox())) return;

    for (UINT i = 0; i < vi->getStrokeCount(); ++i) {
      if (!vi->inCurrentGroup(i)) continue;
      TStroke *stroke = vi->getStroke(i);
      if (m_selective.getValue() && stroke->getStyle() != colorStyle) continue;

      std::vector<DoublePair> ranges =
          computeBrushHiddenRanges(stroke, pos, radius);
      applyRangesToStroke(vi, i, ranges, mode, isUnhideMode());
    }
  }

  void hideAtBrush(const TVectorImageP &vi, const TPointD &pos) {
    if (!vi) return;
    QMutexLocker lock(vi->getMutex());
    hideAtBrushUnlocked(vi, pos, m_pointSize);
  }

  void hideAlongBrushPath(const TVectorImageP &vi, TStroke *path,
                          double radius) {
    if (!vi || !path) return;
    if (radius <= 0) radius = m_pointSize;

    QMutexLocker lock(vi->getMutex());
    const double length = path->getLength();
    if (length <= 1e-6) {
      hideAtBrushUnlocked(vi, path->getPoint(0), radius);
      return;
    }

    const double step = radius > 0 ? radius * 0.5 : 1.0;
    for (double len = 0; len < length; len += step)
      hideAtBrushUnlocked(vi, path->getPointAtLength(len), radius);
    hideAtBrushUnlocked(vi, path->getPointAtLength(length), radius);
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

  void hidePortions(const TVectorImageP &vi, TStroke *boundaryStroke) {
    if (!vi || !boundaryStroke) return;

    int colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
    std::vector<StrokeSegmentRanges> touched = computeRegionPortionRanges(
        vi, boundaryStroke, m_selective.getValue(), colorStyle);
    if (touched.empty()) return;

    THideLineMode mode = toHideLineMode(m_hideMode.getValue());
    QMutexLocker lock(vi->getMutex());
    for (const StrokeSegmentRanges &item : touched)
      applyRangesToStroke(vi, item.strokeIndex, item.ranges, mode,
                          isUnhideMode());
  }

  void applyFrameRange(const TFrameId &lastFrameId,
                       FrameRangeOperation operation) {
    if (!m_rangeLevel || m_startRangeGuides.empty() ||
        m_startRangeGuides.size() != m_endRangeGuides.size())
      return;

    TFrameId firstFrameId = m_firstRangeFrameId;
    TFrameId endFrameId   = lastFrameId;
    bool backward         = firstFrameId > endFrameId;
    TFrameId low          = backward ? endFrameId : firstFrameId;
    TFrameId high         = backward ? firstFrameId : endFrameId;

    std::vector<TFrameId> allFids;
    m_rangeLevel->getFids(allFids);

    std::vector<TFrameId> fids;
    for (const TFrameId &fid : allFids) {
      if (fid >= low && fid <= high) fids.push_back(fid);
    }
    if (fids.empty()) return;

    const size_t pairCount = m_startRangeGuides.size();
    std::vector<TVectorImageP> firstGuides(pairCount);
    std::vector<TVectorImageP> lastGuides(pairCount);
    for (size_t p = 0; p < pairCount; ++p) {
      firstGuides[p] = new TVectorImage();
      lastGuides[p]  = new TVectorImage();
      firstGuides[p]->addStroke(new TStroke(*m_startRangeGuides[p]));
      lastGuides[p]->addStroke(new TStroke(*m_endRangeGuides[p]));
    }

    TUndoManager::manager()->beginBlock();

    const int count = (int)fids.size();
    for (int i = 0; i < count; ++i) {
      const TFrameId fid  = fids[i];
      TVectorImageP image = m_rangeLevel->getFrame(fid, true);
      if (!image) continue;

      double t = count > 1 ? (double)i / (double)(count - 1) : 0.5;
      t        = TInbetween::interpolation(t, interpolationAlgorithm());
      if (backward) t = 1.0 - t;

      m_useExplicitUndoFid = true;
      m_explicitUndoFid    = fid;

      for (size_t p = 0; p < pairCount; ++p) {
        TStroke *guideStroke = nullptr;
        TVectorImageP tweenGuide;
        if (t <= 0.0) {
          guideStroke = firstGuides[p]->getStroke(0);
        } else if (t >= 1.0) {
          guideStroke = lastGuides[p]->getStroke(0);
        } else {
          TStroke *firstS = firstGuides[p]->getStroke(0);
          TStroke *lastS  = lastGuides[p]->getStroke(0);
          if (!firstS || !lastS || firstS->getLength() < 1e-3 ||
              lastS->getLength() < 1e-3)
            guideStroke = t < 0.5 ? firstS : lastS;
          else {
            tweenGuide  = TInbetween(firstGuides[p], lastGuides[p]).tween(t);
            guideStroke = tweenGuide ? tweenGuide->getStroke(0) : nullptr;
          }
        }
        if (!guideStroke) continue;

        if (operation == FrameRangeOperation::Segment)
          hideSegments(image, guideStroke);
        else if (operation == FrameRangeOperation::Region)
          hideRegion(image, guideStroke);
        else if (operation == FrameRangeOperation::Brush) {
          const double r0 =
              p < m_startRangeRadii.size() ? m_startRangeRadii[p] : m_pointSize;
          const double r1 =
              p < m_endRangeRadii.size() ? m_endRangeRadii[p] : m_pointSize;
          hideAlongBrushPath(image, guideStroke, r0 * (1.0 - t) + r1 * t);
        } else
          hidePortions(image, guideStroke);
      }

      if (m_undo && !m_undo->empty())
        image->notifyHideLineFillChanged(m_undo->changedStrokeIndices());
      commitUndo();
      m_rangeLevel->setFrame(fid, image);
      m_rangeLevel->touchFrame(fid);
    }

    m_useExplicitUndoFid = false;
    TUndoManager::manager()->endBlock();

    notifyImageChanged();
    invalidate();
  }

  void beginRangeGuides(TStroke *stroke, const TXshSimpleLevelP &level,
                        const TFrameId &fid, double radius) {
    resetFrameRange();
    m_startRangeGuides.push_back(new TStroke(*stroke));
    m_startRangeRadii.push_back(radius);
    m_firstRangeFrameId = fid;
    m_rangeLevel        = level;
    rememberRangeOrigin();
  }

  void handleFrameRange(TStroke *stroke, FrameRangeOperation operation,
                        bool shift, bool ctrl, double radius = 0) {
    if (!stroke) return;

    TTool::Application *application = TTool::getApplication();
    if (!application) return;

    TXshLevelHandle *levelHandle = application->getCurrentLevel();
    if (!levelHandle) {
      resetFrameRange();
      return;
    }
    TXshSimpleLevelP level = levelHandle->getSimpleLevel();
    if (!level) {
      resetFrameRange();
      return;
    }

    const TFrameId fid = getCurrentFid();

    if (!hasPendingRange() || !m_rangeLevel ||
        m_rangeLevel.getPointer() != level.getPointer()) {
      beginRangeGuides(stroke, level, fid, radius);
      invalidate();
      return;
    }

    if (fid == m_firstRangeFrameId) {
      if (ctrl) {
        m_startRangeGuides.push_back(new TStroke(*stroke));
        m_startRangeRadii.push_back(radius);
      } else
        beginRangeGuides(stroke, level, fid, radius);
      invalidate();
      return;
    }

    if (!m_endRangeGuides.empty() && fid != m_lastRangeFrameId) {
      deleteRangeGuides(m_endRangeGuides);
      m_endRangeRadii.clear();
    }

    m_lastRangeFrameId = fid;
    m_endRangeGuides.push_back(new TStroke(*stroke));
    m_endRangeRadii.push_back(radius);

    if (m_endRangeGuides.size() == m_startRangeGuides.size()) {
      applyFrameRange(fid, operation);
      if (shift) {
        deleteRangeGuides(m_startRangeGuides);
        m_startRangeGuides.swap(m_endRangeGuides);
        m_startRangeRadii.swap(m_endRangeRadii);
        m_endRangeRadii.clear();
        m_firstRangeFrameId = fid;
      } else {
        restoreRangeOrigin();
        resetFrameRange();
      }
    }
    invalidate();
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

    double error    = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
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
      , m_lasso("Lasso", false)
      , m_followInk("Follow Ink", false)
      , m_frameRange("Frame Range", false)
      , m_interpolation("interpolation:")
      , m_toolSize("Size:", 1, 100, 10)
      , m_stroke(nullptr)
      , m_undo(nullptr)
      , m_currCell(-1, -1)
      , m_useExplicitUndoFid(false)
      , m_rangeCtrl(false)
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
    m_prop.bind(m_lasso);
    m_prop.bind(m_followInk);
    m_prop.bind(m_frameRange);
    m_prop.bind(m_interpolation);
    m_interpolation.addValue(LINEAR_INTERPOLATION);
    m_interpolation.addValue(EASE_IN_INTERPOLATION);
    m_interpolation.addValue(EASE_OUT_INTERPOLATION);
    m_interpolation.addValue(EASE_IN_OUT_INTERPOLATION);

    m_hideType.setId("Type");
    m_hideMode.setId("Mode");
    m_unhide.setId("Unhide");
    m_selective.setId("Selective");
    m_lasso.setId("Lasso");
    m_followInk.setId("FollowInk");
    m_frameRange.setId("FrameRange");
    m_interpolation.setId("Interpolation");
    m_toolSize.setId("Size");

    updateBrushSize();
  }

  ~HideLineTool() {
    resetFrameRange();
    if (m_stroke) delete m_stroke;
    if (m_undo) delete m_undo;
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  ToolOptionsBox *createOptionsBox() override {
    TPaletteHandle *currPalette = TTool::getApplication()
                                      ->getPaletteController()
                                      ->getCurrentLevelPalette();
    ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
    return new HideLineToolOptionsBox(0, this, currPalette, currTool,
                                      &m_hideType);
  }

  TPropertyGroup *getProperties(int) override { return &m_prop; }

  int getCursorId() const override { return ToolCursor::HideLineCursor; }

  bool onPropertyChanged(std::string propertyName) override {
    if (hasPendingRange() && propertyName != m_interpolation.getName() &&
        propertyName != m_followInk.getName())
      resetFrameRange();

    HideLineSize          = m_toolSize.getValue();
    HideLineType          = ::to_string(m_hideType.getValue());
    HideLineMode          = ::to_string(m_hideMode.getValue());
    HideLineSelective     = m_selective.getValue();
    HideLineUnhide        = m_unhide.getValue();
    HideLineLasso         = m_lasso.getValue();
    HideLineFollowInk     = m_followInk.getValue();
    HideLineRange         = m_frameRange.getValue();
    HideLineInterpolation = ::to_string(m_interpolation.getValue());
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
    m_lasso.setQStringName(tr("Lasso"));
    m_followInk.setQStringName(tr("Follow Ink"));
    m_hideMode.setQStringName(tr("Mode:"));
    m_hideMode.setItemUIName(INVISIBLE_MODE, tr("Invisible"));
    m_hideMode.setItemUIName(HIDDEN_MODE, tr("Hidden"));
    m_hideMode.setItemToolTip(INVISIBLE_MODE, tr("Fill-block"));
    m_hideMode.setItemToolTip(HIDDEN_MODE, tr("Fill-through"));
    m_unhide.setQStringName(tr("Unhide"));
    m_selective.setQStringName(tr("Selective"));
    m_frameRange.setQStringName(tr("Frame Range"));
    m_interpolation.setQStringName(tr(""));
    m_interpolation.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
    m_interpolation.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
    m_interpolation.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
    m_interpolation.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
  }

  void draw() override {
    updateBrushSize();
    m_rangeBalloons.clear();

    TVectorImageP vi = getImage(false);

    if (m_frameRange.getValue() && hasPendingRange()) {
      const int nextIndex = (int)m_endRangeGuides.size();
      for (int i = 0; i < (int)m_startRangeGuides.size(); ++i) {
        const double radius =
            i < (int)m_startRangeRadii.size() ? m_startRangeRadii[i] : 0;
        drawPendingRangeOverlay(vi, m_startRangeGuides[i], radius, i,
                                i == nextIndex);
      }
      for (int i = 0; i < (int)m_endRangeGuides.size(); ++i) {
        const double radius =
            i < (int)m_endRangeRadii.size() ? m_endRangeRadii[i] : 0;
        drawPendingRangeOverlay(vi, m_endRangeGuides[i], radius, i, false);
      }
    }

    if (m_pointSize <= 0 && !isLassoMode()) return;

    double pixelSize2 = getPixelSize() * getPixelSize();
    m_thick           = pixelSize2 / 2.0;

    if (!vi) return;

    if (isNormalType()) {
      const int pairIndex = livePairIndex();
      if (isLassoMode()) {
        if (!m_track.isEmpty()) {
          tglColor(m_frameRange.getValue() ? rangeGuideColor(pairIndex)
                                           : TPixel32::Black);
          glPushMatrix();
          m_track.drawAllFragments();
          glPopMatrix();
        }
        return;
      }

      if (m_pointSize > 0) {
        if (m_frameRange.getValue())
          tglColor(rangeGuideColor(pairIndex));
        else
          tglColor(TPixel32(255, 0, 255));
        tglDrawCircle(m_brushPos, m_pointSize);
        if (m_frameRange.getValue()) drawRangeIndex(m_brushPos, pairIndex);
      }

      if (m_frameRange.getValue() && !m_track.isEmpty()) {
        TStroke *live = makeTrackStroke(false);
        if (live) {
          drawRangeBrushEffect(vi, *live, m_pointSize, pairIndex, false, false);
          delete live;
        }
      }
      return;
    }

    if (m_track.isEmpty()) return;
    if (m_frameRange.getValue() && isFrameRangeType()) {
      int colorIndex = 0;
      if (hasPendingRange()) {
        if (getCurrentFid() == m_firstRangeFrameId)
          colorIndex = m_rangeCtrl ? (int)m_startRangeGuides.size() : 0;
        else
          colorIndex = (int)m_endRangeGuides.size();
      }
      tglColor(rangeGuideColor(colorIndex));
    } else {
      tglColor(TPixel32::Black);
    }
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

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_active    = true;
    m_rangeCtrl = e.isCtrlPressed();
    m_brushPos = m_mousePos = pos;

    if (isNormalType()) {
      m_oldMousePos = pos;
      m_distance2   = 0;
      if (isLassoMode() || m_frameRange.getValue()) {
        startFreehandTrack(pos);
        return;
      }
      if (TVectorImageP vi = getImage(true)) hideAtBrush(vi, pos);
      return;
    }

    startFreehandTrack(pos);
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    if (!m_active) return;
    m_brushPos = m_mousePos = pos;

    if (isNormalType()) {
      if (isLassoMode() || m_frameRange.getValue()) {
        dragFreehandTrack(pos);
        return;
      }
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

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    if (!m_active) return;
    m_active   = false;
    m_brushPos = m_mousePos = pos;

    TVectorImageP vi = getImage(true);
    if (!vi) {
      commitUndo();
      m_track.clear();
      invalidate();
      return;
    }

    if (isNormalType()) {
      if (isLassoMode()) {
        if (m_stroke) delete m_stroke;
        m_stroke = makeTrackStroke(true);
        if (m_stroke && m_frameRange.getValue()) {
          handleFrameRange(m_stroke, FrameRangeOperation::Portion,
                           e.isShiftPressed(), e.isCtrlPressed());
          m_track.clear();
          invalidate();
          return;
        }
        if (m_stroke) hidePortions(vi, m_stroke);
        finishEdit(vi);
        return;
      }

      if (m_frameRange.getValue()) {
        if (m_stroke) delete m_stroke;
        m_stroke = makeTrackStroke(false);
        if (!m_stroke) {
          std::vector<TThickPoint> points;
          points.push_back(TThickPoint(pos, 1));
          points.push_back(TThickPoint(pos, 1));
          m_stroke = new TStroke(points);
        }
        handleFrameRange(m_stroke, FrameRangeOperation::Brush,
                         e.isShiftPressed(), e.isCtrlPressed(), m_pointSize);
        m_track.clear();
        invalidate();
        return;
      }
      finishEdit(vi);
      return;
    }

    if (m_hideType.getValue() == SEGMENT_HIDE) {
      if (m_stroke) delete m_stroke;
      m_stroke = makeTrackStroke(false);

      if (m_stroke && m_frameRange.getValue() && isFrameRangeType()) {
        handleFrameRange(m_stroke, FrameRangeOperation::Segment,
                         e.isShiftPressed(), e.isCtrlPressed());
        m_track.clear();
        invalidate();
        return;
      }

      if (m_stroke) hideSegments(vi, m_stroke);
      finishEdit(vi);
      return;
    }

    if (m_hideType.getValue() == FREEHAND_HIDE) {
      if (m_stroke) delete m_stroke;
      m_stroke = makeTrackStroke(true);

      if (m_stroke && m_frameRange.getValue() && isFrameRangeType()) {
        handleFrameRange(m_stroke, FrameRangeOperation::Region,
                         e.isShiftPressed(), e.isCtrlPressed());
        m_track.clear();
        invalidate();
        return;
      }

      if (m_stroke) hideRegion(vi, m_stroke);
      finishEdit(vi);
    }
  }

  void onImageChanged() override {
    TTool::Application *app = TTool::getApplication();
    if (!app || !app->getCurrentLevel()) return;
    TXshSimpleLevel *level = app->getCurrentLevel()->getSimpleLevel();
    if (hasPendingRange() && m_rangeLevel && m_rangeLevel.getPointer() != level)
      resetFrameRange();
  }

  void onDeactivate() override {
    m_active = false;
    m_track.clear();
    resetFrameRange();
    if (m_stroke) {
      delete m_stroke;
      m_stroke = nullptr;
    }
    if (m_undo) {
      delete m_undo;
      m_undo = nullptr;
    }
    m_pointSize = -1;
    invalidate();
  }
} hideLineTool;

}  // namespace
