

#include "tools/tool.h"
#include "tundo.h"
#include "tproperty.h"
#include "tools/cursors.h"
#include "toonz/autoclose.h"
#include "ttoonzimage.h"
#include "toonz/toonzimageutils.h"
#include "tenv.h"
#include "tools/toolutils.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/ttileset.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"

#include "tvectorimage.h"
#include "toonz/strokegenerator.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tinbetween.h"

#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "tools/toolhandle.h"

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

TEnv::StringVar AutocloseVectorType("InknpaintAutocloseVectorType", "Normal");
TEnv::IntVar AutocloseDistance("InknpaintAutocloseDistance", 30);
TEnv::DoubleVar AutocloseAngle("InknpaintAutocloseAngle", 60.0);
TEnv::IntVar AutocloseRange("InknpaintAutocloseRange", 0);
TEnv::IntVar AutocloseOpacity("InknpaintAutocloseOpacity", 255);
TEnv::IntVar AutocloseIgnoreAutoPaint("AutocloseIgnoreAutoPaint", 0);

#define NORMAL_CLOSE L"Normal"
#define RECT_CLOSE L"Rectangular"
#define FREEHAND_CLOSE L"Freehand"
#define POLYLINE_CLOSE L"Polyline"

namespace {

//============================================================

class AutocloseParameters {
public:
  int m_closingDistance, m_inkIndex, m_opacity;
  double m_spotAngle;

  AutocloseParameters()
      : m_closingDistance(0), m_inkIndex(0), m_spotAngle(0), m_opacity(1) {}
};

//============================================================

class RasterAutocloseUndo final : public TRasterUndo {
  AutocloseParameters m_params;
  std::vector<TAutocloser::Segment> m_segments;

public:
  RasterAutocloseUndo(TTileSetCM32 *tileSet, const AutocloseParameters &params,
                      const std::vector<TAutocloser::Segment> &segments,
                      TXshSimpleLevel *level, const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_segments(segments)
      , m_params(params) {}

  //-------------------------------------------------------------------

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    TAutocloser ac(image->getRaster(), m_params.m_closingDistance,
                   m_params.m_spotAngle, m_params.m_inkIndex,
                   m_params.m_opacity);

    ac.draw(m_segments);
    ToolUtils::updateSaveBox();
    /*-- Notify to update Viewer --*/
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  //-------------------------------------------------------------------

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Autoclose Tool"); }
  int getHistoryType() override { return HistoryType::AutocloseTool; }
};

}  // namespace

//============================================================

class RasterTapeTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(RasterTapeTool)

  bool m_selecting;
  TRectD m_selectingRect;
  TRectD m_firstRect;
  TPointD m_firstPoint;
  bool m_firstFrameSelected;
  TXshSimpleLevelP m_level;

  // TBoolProperty  m_isRect;
  TEnumProperty m_closeType;
  TIntProperty m_distance;
  TDoubleProperty m_angle;
  TStyleIndexProperty m_inkIndex;
  TIntProperty m_opacity;
  TPropertyGroup m_prop;
  TBoolProperty m_ignoreAP;
  TBoolProperty m_multi;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  bool m_isXsheetCell;
  std::pair<int, int> m_currCell;

  // Added to draw freehand and polyline
  StrokeGenerator m_track;
  TPointD m_firstPos;
  TPointD m_mousePosition;
  double m_thick;
  TStroke *m_stroke;
  TStroke *m_firstStroke;
  std::vector<TPointD> m_polyline;
  bool m_firstTime;

public:
  RasterTapeTool()
      : TTool("T_Tape")
      , m_closeType("Type:")                    // W_ToolOptions_CloseType
      , m_distance("Distance:", 1, 100, 10)     // W_ToolOptions_Distance
      , m_angle("Angle:", 1, 360, 60)           // W_ToolOptions_Angle
      , m_inkIndex("Style Index:", L"current")  // W_ToolOptions_InkIndex
      , m_opacity("Opacity:", 1, 255, 255)
      , m_multi("Frame Range", false)  // W_ToolOptions_FrameRange
      , m_ignoreAP("Ignore AutoPaint Lines", false)
      , m_selecting(false)
      , m_selectingRect()
      , m_firstRect()
      , m_level(0)
      , m_firstFrameSelected(false)
      , m_isXsheetCell(false)
      , m_currCell(-1, -1)
      , m_firstPos()
      , m_mousePosition()
      , m_thick(0.5)
      , m_stroke(0)
      , m_firstStroke(0)
      , m_firstTime(true) {
    bind(TTool::ToonzImage);
    m_prop.bind(m_closeType);
    m_closeType.addValue(NORMAL_CLOSE);
    m_closeType.addValue(RECT_CLOSE);
    m_closeType.addValue(FREEHAND_CLOSE);
    m_closeType.addValue(POLYLINE_CLOSE);
    m_prop.bind(m_multi);
    m_prop.bind(m_distance);
    m_prop.bind(m_angle);
    m_prop.bind(m_inkIndex);
    m_prop.bind(m_opacity);
    m_prop.bind(m_ignoreAP);
    m_multi.setId("FrameRange");
    m_ignoreAP.setId("IgnoreautoPaintInks");
    m_closeType.setId("Type");
  }

  //------------------------------------------------------------

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  //------------------------------------------------------------

  void updateTranslation() override {
    m_closeType.setQStringName(tr("Type:"));
    m_closeType.setItemUIName(NORMAL_CLOSE, tr("Normal"));
    m_closeType.setItemUIName(RECT_CLOSE, tr("Rectangular"));
    m_closeType.setItemUIName(FREEHAND_CLOSE, tr("Freehand"));
    m_closeType.setItemUIName(POLYLINE_CLOSE, tr("Polyline"));

    m_distance.setQStringName(tr("Distance:"));
    m_inkIndex.setQStringName(tr("Style Index:"));
    m_inkIndex.setValue(tr("current").toStdWString());
    m_opacity.setQStringName(tr("Opacity:"));
    m_multi.setQStringName(tr("Frame Range"));
    m_ignoreAP.setQStringName(tr("Ignore AutoPaint Inks"));
    m_angle.setQStringName(tr("Angle:"));
  }

  //------------------------------------------------------------

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_closeType.getValue() == RECT_CLOSE) {
      if (!m_selecting) return;
      m_selectingRect.x1 = pos.x;
      m_selectingRect.y1 = pos.y;
      invalidate();
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      freehandDrag(pos);
      invalidate();
    }
  }

  //------------------------------------------------------------
  /*-- AutoClose Returns true if executed, false otherwise --*/
  bool applyAutoclose(const TToonzImageP &ti, const TFrameId fid,
                      const TRectD &selRect = TRectD(), TStroke *stroke = 0) {
    if (!ti || !ti->getRaster()) return false;

    TTool::Application *app = TTool::getApplication();
    TXshLevel *level        = app->getCurrentLevel()->getLevel();
    if (!level) return false;

    TXshSimpleLevel *sl = level->getSimpleLevel();
    if (!sl) return false;

    std::string cacheId = sl->getImageId(fid);
    // initialize AutocloseParameters
    AutocloseParameters params;
    params.m_closingDistance = m_distance.getValue();
    params.m_spotAngle       = m_angle.getValue();
    params.m_opacity         = m_opacity.getValue();

    std::string inkString = ::to_string(m_inkIndex.getValue());
    int inkIndex = TTool::getApplication()->getCurrentLevelStyleIndex();
    if (isInt(inkString)) inkIndex = std::stoi(inkString);
    params.m_inkIndex = inkIndex;

    std::wstring closeType = m_closeType.getValue();
    TRasterCM32P raux      = ti->getRaster();

    std::vector<TAutocloser::Segment> segments;

    // Use The Cache
    if (TAutocloser::hasSegmentCache(cacheId)) {
      segments = TAutocloser::getSegmentCache(cacheId);
    } else {
      // Calculate only once and store in cache
      TAutocloser ac(raux, params.m_closingDistance, params.m_spotAngle,
                     params.m_inkIndex, params.m_opacity);
      ac.exec(cacheId);  // <-- This computes + draws + saves in cache!
      segments = TAutocloser::getSegmentCache(cacheId);
    }

    if (segments.empty()) return false;

    // Fiter by region (only if necessary)
    TPoint delta(0, 0);
    std::vector<TAutocloser::Segment> filteredSegments = segments;

    if (closeType == RECT_CLOSE && !selRect.isEmpty()) {
      TRectD orderedRect(
          std::min(selRect.x0, selRect.x1), std::min(selRect.y0, selRect.y1),
          std::max(selRect.x0, selRect.x1), std::max(selRect.y0, selRect.y1));
      TRect rasterRect = ToonzImageUtils::convertWorldToRaster(orderedRect, ti);
      checkSegments(filteredSegments, convert(rasterRect), delta);
    } else if ((closeType == FREEHAND_CLOSE || closeType == POLYLINE_CLOSE) &&
               stroke) {
      checkSegments(filteredSegments, stroke, raux, delta);
    } // Normal mode: use all segments
    /*-- Return false if no segment is obtained --*/
    if (filteredSegments.empty()) return false;

    // Adjust coordinates if necessary
    if (delta != TPoint(0, 0)) {
      for (auto &seg : filteredSegments) {
        seg.first += delta;
        seg.second += delta;
      }
    }

    // UNDO
    TTileSetCM32 *tileSet = new TTileSetCM32(raux->getSize());
    for (const auto &seg : filteredSegments) {
      TRect bbox(seg.first, seg.second);
      tileSet->add(raux, bbox.enlarge(2));
    }

    TUndoManager::manager()->add(
        new RasterAutocloseUndo(tileSet, params, filteredSegments, sl, fid));

    // DRAW
    TAutocloser ac(raux, params.m_closingDistance, params.m_spotAngle,
                   params.m_inkIndex, params.m_opacity);
    ac.draw(filteredSegments);

    ToolUtils::updateSaveBox();
    return true;
  }

  //------------------------------------------------------------

  inline TRectD interpolateRect(const TRectD &r1, const TRectD &r2, double t) {
    return TRectD(r1.x0 + (r2.x0 - r1.x0) * t, r1.y0 + (r2.y0 - r1.y0) * t,
                  r1.x1 + (r2.x1 - r1.x1) * t, r1.y1 + (r2.y1 - r1.y1) * t);
  }

  //============================================================

  void multiApplyAutoclose(TFrameId firstFid, TFrameId lastFid,
                           TRectD firstRect, TRectD lastRect,
                           TStroke *firstStroke = 0, TStroke *lastStroke = 0) {
    bool backward = false;
    if (firstFid > lastFid) {
      std::swap(firstFid, lastFid);
      backward = true;
    }
    assert(firstFid <= lastFid);
    std::vector<TFrameId> allFids;
    m_level->getFids(allFids);

    std::vector<TFrameId>::iterator i0 = allFids.begin();
    while (i0 != allFids.end() && *i0 < firstFid) i0++;
    if (i0 == allFids.end()) return;
    std::vector<TFrameId>::iterator i1 = i0;
    while (i1 != allFids.end() && *i1 <= lastFid) i1++;
    assert(i0 < i1);
    std::vector<TFrameId> fids(i0, i1);
    int m = fids.size();
    assert(m > 0);

    TVectorImageP firstImage;
    TVectorImageP lastImage;
    if ((m_closeType.getValue() == FREEHAND_CLOSE ||
         m_closeType.getValue() == POLYLINE_CLOSE) &&
        firstStroke && lastStroke) {
      TStroke *first = new TStroke(*firstStroke);
      TStroke *last  = new TStroke(*lastStroke);
      firstImage     = new TVectorImage();
      lastImage      = new TVectorImage();
      firstImage->addStroke(first);
      lastImage->addStroke(last);
    }

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < m; ++i) {
      TFrameId fid     = fids[i];
      TToonzImageP img = (TToonzImageP)m_level->getFrame(fid, true);
      if (!img) continue;
      double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
      if (m_closeType.getValue() == RECT_CLOSE)
        applyAutoclose(img, fid, interpolateRect(firstRect, lastRect, t));
      else if ((m_closeType.getValue() == FREEHAND_CLOSE ||
                m_closeType.getValue() == POLYLINE_CLOSE) &&
               firstStroke && lastStroke)
        doClose(t, fid, img, firstImage, lastImage);
      m_level->getProperties()->setDirtyFlag(true);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();

    //		TNotifier::instance()->notify(TLevelChange());
    //		TNotifier::instance()->notify(TStageChange());
  }

  //----------------------------------------------------------------------
  // Normal Mode
  void multiApplyAutoclose(TFrameId firstFrameId, TFrameId lastFrameId) {
    if (firstFrameId > lastFrameId) {
      std::swap(firstFrameId, lastFrameId);
    }
    if (firstFrameId > lastFrameId) return;

    std::vector<TFrameId> allFids;
    m_level->getFids(allFids);

    std::vector<TFrameId>::iterator i0 = allFids.begin();
    while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
    if (i0 == allFids.end()) return;
    std::vector<TFrameId>::iterator i1 = i0;
    while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
    assert(i0 < i1);
    std::vector<TFrameId> fids(i0, i1);
    int m = fids.size();

    TUndoManager::manager()->beginBlock();
    for (auto fid : fids) {
      TImageP img = m_level->getFrame(fid, true);
      applyAutoclose(img, fid);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();

    //		TNotifier::instance()->notify(TLevelChange());
    //		TNotifier::instance()->notify(TStageChange());
  }

  //----------------------------------------------------------------------

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (!ti) return;

    /*-- Normalize Rect coordinates --*/
    if (m_selectingRect.x0 > m_selectingRect.x1)
      std::swap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      std::swap(m_selectingRect.y1, m_selectingRect.y0);

    TTool::Application *app = TTool::getApplication();

    m_selecting = false;
    if (m_closeType.getValue() == RECT_CLOSE) {
      if (m_multi.getValue()) {
        if (m_firstFrameSelected) {
          multiApplyAutoclose(m_firstFrameId, getFrameId(), m_firstRect,
                              m_selectingRect);
          invalidate(m_selectingRect.enlarge(2));
          if (e.isShiftPressed()) {
            m_firstRect    = m_selectingRect;
            m_firstFrameId = getFrameId();
          } else {
            if (m_isXsheetCell) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
          }
        } else {
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          // if (m_isXsheetCell)
          m_currCell = std::pair<int, int>(getColumnIndex(), this->getFrame());
        }
        return;
      }

      /*-- Check if AutoClose was executed --*/
      if (!applyAutoclose(ti, getCurrentFid(), m_selectingRect)) {
        if (m_stroke) {
          delete m_stroke;
          m_stroke = 0;
        }
        invalidate();
        return;
      }

      invalidate();
      notifyImageChanged();
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      closeFreehand(pos);
      if (m_multi.getValue())
        multiAutocloseRegion(m_stroke, e);
      else
        applyAutoclose(ti, getCurrentFid(), TRectD(), m_stroke);
      m_track.clear();
      invalidate();
    }
    if (m_stroke) {
      delete m_stroke;
      m_stroke = 0;
    }
  }

  //------------------------------------------------------------

  void draw() override {
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_thick           = sqrt(pixelSize2) / 2.0;
    if (m_closeType.getValue() == RECT_CLOSE) {
      TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                         ? TPixel32::White
                         : TPixel32::Black;
      if (m_multi.getValue() && m_firstFrameSelected)
        drawRect(m_firstRect, color, 0x3F33, true);

      if (m_selecting || (m_multi.getValue() && !m_firstFrameSelected))
        drawRect(m_selectingRect, color, 0x3F33, true);
    }
    if ((m_closeType.getValue() == FREEHAND_CLOSE ||
         m_closeType.getValue() == POLYLINE_CLOSE) &&
        m_multi.getValue() && m_firstStroke) {
      TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                         ? TPixel32::White
                         : TPixel32::Black;
      tglColor(color);
      drawStrokeCenterline(*m_firstStroke, 1);
    }
    if (m_closeType.getValue() == POLYLINE_CLOSE && !m_polyline.empty()) {
      TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                         ? TPixel32::White
                         : TPixel32::Black;
      tglColor(color);
      tglDrawCircle(m_polyline[0], 2);
      glBegin(GL_LINE_STRIP);
      for (UINT i = 0; i < m_polyline.size(); i++) tglVertex(m_polyline[i]);
      tglVertex(m_mousePosition);
      glEnd();
    } else if (m_closeType.getValue() == FREEHAND_CLOSE && !m_track.isEmpty()) {
      TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                         ? TPixel32::White
                         : TPixel32::Black;
      tglColor(color);
      m_track.drawAllFragments();
    } else if (m_multi.getValue() && m_firstFrameSelected)
      drawCross(m_firstPoint, 5);

    // if (ToonzCheck::instance()->getChecks() & ToonzCheck::eAutoclose) {
    //   auto fid = getCurrentFid();
    //     auto Id =
    //       getApplication()->getCurrentLevel()->getSimpleLevel()->getImageId(
    //           fid, 0);
    //   if (TAutocloser::hasSegmentCache(Id)) {
    //     auto ti        = (TToonzImageP)m_level->getFrame(fid, false);
    //     if (!ti) return;
    //     TPointD center = ti->getRaster()->getCenterD();
    //       tglColor(TPixel32::Red);
    //     for (auto seg : TAutocloser::getSegmentCache(Id)) {
    //       TPointD centerPos = convert((seg.first + seg.second) / 2) - center;
    //       double radius     = std::sqrt(norm2(seg.first - seg.second)) / 2.0;
    //       tglDrawCircle(centerPos, radius);
    //     }
    //   }
    // }
  }

  //------------------------------------------------------------

  bool onPropertyChanged(std::string propertyName) override {
    if (propertyName == m_closeType.getName()) {
      AutocloseVectorType = ::to_string(m_closeType.getValue());
      resetMulti();
    }

    else if (propertyName == m_distance.getName())
      AutocloseDistance = m_distance.getValue();

    else if (propertyName == m_angle.getName())
      AutocloseAngle = m_angle.getValue();

    else if (propertyName == m_inkIndex.getName()) {
    }

    else if (propertyName == m_opacity.getName())
      AutocloseOpacity = m_opacity.getValue();

    else if (propertyName == m_multi.getName()) {
      AutocloseRange = (int)((m_multi.getValue()));
      resetMulti();
    }

    else if (propertyName == m_ignoreAP.getName()) {
      AutocloseIgnoreAutoPaint = (int)(m_ignoreAP.getValue());
    }

    ToonzCheck::instance()->setAutocloseSettings(
        AutocloseDistance, AutocloseAngle, AutocloseOpacity,
        AutocloseIgnoreAutoPaint);

    if (ToonzCheck::instance()->getChecks() & ToonzCheck::eAutoclose)
      notifyImageChanged();

    return true;
  }

  //----------------------------------------------------------------------

  void resetMulti() {
    m_firstFrameSelected = false;
    m_firstRect.empty();
    m_firstPoint = TPointD();
    m_selectingRect.empty();
    TTool::Application *app = TTool::getApplication();
    m_level                 = app->getCurrentLevel()->getLevel()
                                  ? app->getCurrentLevel()->getSimpleLevel()
                                  : 0;
    m_firstFrameId = m_veryFirstFrameId = getFrameId();
    m_firstStroke                       = 0;
  }

  //----------------------------------------------------------------------

  void onImageChanged() override {
    if (!m_multi.getValue()) return;
    TTool::Application *app = TTool::getApplication();
    TXshSimpleLevel *xshl   = 0;
    if (app->getCurrentLevel()->getLevel())
      xshl = app->getCurrentLevel()->getSimpleLevel();

    if (!xshl || m_level.getPointer() != xshl ||
        (m_closeType.getValue() == RECT_CLOSE && m_selectingRect.isEmpty()) ||
        ((m_closeType.getValue() == FREEHAND_CLOSE ||
          m_closeType.getValue() == POLYLINE_CLOSE) &&
         !m_firstStroke))
      resetMulti();
    else if (m_firstFrameId == getFrameId())
      m_firstFrameSelected = false;  // when returning to initial image,
                                     // return to initial state
    else {                           // change state.
      m_firstFrameSelected = true;
      if (m_closeType.getValue() == RECT_CLOSE) {
        assert(!m_selectingRect.isEmpty());
        m_firstRect = m_selectingRect;
      }
    }
  }

  //----------------------------------------------------------------------

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (!ti) return;

    if (m_closeType.getValue() == RECT_CLOSE) {
      m_selecting        = true;
      m_selectingRect.x0 = pos.x;
      m_selectingRect.y0 = pos.y;
      m_selectingRect.x1 = pos.x + 1;
      m_selectingRect.y1 = pos.y + 1;
      return;
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      startFreehand(pos);
      return;
    } else if (m_closeType.getValue() == POLYLINE_CLOSE) {
      addPointPolyline(pos);
      return;
    } else if (m_closeType.getValue() == NORMAL_CLOSE) {
      if (m_multi.getValue()) {
        TTool::Application *app = TTool::getApplication();
        if (m_firstFrameSelected) {
          multiApplyAutoclose(m_firstFrameId, getFrameId());
          invalidate();
          if (m_isXsheetCell) {
            app->getCurrentColumn()->setColumnIndex(m_currCell.first);
            app->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
        } else {
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          // if (m_isXsheetCell)
          m_currCell = std::pair<int, int>(getColumnIndex(), getFrame());
          m_firstFrameSelected = true;
          m_firstPoint         = pos;
        }
        invalidate();
        return;
      }

      m_selecting = false;
      applyAutoclose(ti, getCurrentFid());
      invalidate();
      notifyImageChanged();
    }
  }

  //----------------------------------------------------------------------

  void leftButtonDoubleClick(const TPointD &pos,
                             const TMouseEvent &e) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (m_closeType.getValue() == POLYLINE_CLOSE && ti) {
      closePolyline(pos);

      std::vector<TThickPoint> strokePoints;
      for (UINT i = 0; i < m_polyline.size() - 1; i++) {
        strokePoints.push_back(TThickPoint(m_polyline[i], 1));
        strokePoints.push_back(
            TThickPoint(0.5 * (m_polyline[i] + m_polyline[i + 1]), 1));
      }
      strokePoints.push_back(TThickPoint(m_polyline.back(), 1));
      m_polyline.clear();
      m_stroke = new TStroke(strokePoints);
      assert(m_stroke->getPoint(0) == m_stroke->getPoint(1));
      if (m_multi.getValue())
        multiAutocloseRegion(m_stroke, e);
      else
        applyAutoclose(ti, getCurrentFid(), TRectD(), m_stroke);
      invalidate();
    }
    if (m_stroke) {
      delete m_stroke;
      m_stroke = 0;
    }
  }

  //----------------------------------------------------------------------

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    if (m_closeType.getValue() == POLYLINE_CLOSE) {
      m_mousePosition = pos;
      invalidate();
    }
  }

  //----------------------------------------------------------------------

  void onEnter() override {
    //      getApplication()->editImage();
  }

  //----------------------------------------------------------------------

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  //----------------------------------------------------------------------

  void onActivate() override {
    if (m_firstTime) {
      m_closeType.setValue(::to_wstring(AutocloseVectorType.getValue()));
      m_distance.setValue(AutocloseDistance);
      m_angle.setValue(AutocloseAngle);
      m_opacity.setValue(AutocloseOpacity);
      m_multi.setValue(AutocloseRange ? 1 : 0);
      m_ignoreAP.setValue(AutocloseIgnoreAutoPaint ? 1 : 0);
      ToonzCheck::instance()->setAutocloseSettings(
          AutocloseDistance, AutocloseAngle, AutocloseOpacity,
          AutocloseIgnoreAutoPaint);
      m_firstTime = false;
    }

    //			getApplication()->editImage();
    resetMulti();
  }

  //----------------------------------------------------------------------

  void onDeactivate() override {}

  //----------------------------------------------------------------------

  int getCursorId() const override {
    int ret = ToolCursor::TapeCursor;

    if (m_closeType.getValue() == FREEHAND_CLOSE)
      ret = ret | ToolCursor::Ex_FreeHand;
    else if (m_closeType.getValue() == POLYLINE_CLOSE)
      ret = ret | ToolCursor::Ex_PolyLine;
    else if (m_closeType.getValue() == RECT_CLOSE)
      ret = ret | ToolCursor::Ex_Rectangle;

    if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
      ret = ret | ToolCursor::Ex_Negate;

    return ret;
  }

  //----------------------------------------------------------------------

  //! Adds \b pos to \b m_track and draws the first piece of the freehand.
  //! Initializes \b m_firstPos
  void startFreehand(const TPointD &pos) {
    m_track.clear();
    m_firstPos        = pos;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
  }

  //------------------------------------------------------------------

  //! Adds \b pos to \b m_track and draws another piece of the freehand.
  void freehandDrag(const TPointD &pos) {
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
  }

  //------------------------------------------------------------------

  //! Closes the freehand (adds the last point to m_track) and creates the
  //! stroke representing the freehand.
  void closeFreehand(const TPointD &pos) {
    if (m_track.isEmpty()) return;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
    m_track.filterPoints();
    double error = (30.0 / 11) * sqrt(pixelSize2);
    m_stroke     = m_track.makeStroke(error);
    m_stroke->setStyle(1);
  }

  //------------------------------------------------------------------

  //! Adds a point to the vector m_polyline.
  void addPointPolyline(const TPointD &pos) {
    m_firstPos = pos;
    m_polyline.push_back(pos);
  }

  //------------------------------------------------------------------

  //! Adds the last pos to \b m_polyline and closes the polyline (adds \b
  //! m_polyline.front() to the end of the vector)
  void closePolyline(const TPointD &pos) {
    if (m_polyline.size() <= 1) return;
    if (m_polyline.back() != pos) m_polyline.push_back(pos);
    if (m_polyline.back() != m_polyline.front())
      m_polyline.push_back(m_polyline.front());
    invalidate();
  }

  //-------------------------------------------------------------------

  //! Deletes segments that are not contained within the stroke!!!
  void checkSegments(std::vector<TAutocloser::Segment> &segments,
                     TStroke *stroke, const TRasterCM32P &ras,
                     const TPoint &delta) {
    TVectorImage vi;
    TStroke *app = new TStroke();
    *app         = *stroke;
    app->transform(TTranslation(convert(ras->getCenter())));
    vi.addStroke(app);
    vi.findRegions();
    std::vector<TAutocloser::Segment>::iterator it = segments.begin();
    while (it != segments.end()) {
      int i;
      bool isContained = false;
      for (i = 0; i < (int)vi.getRegionCount(); i++) {
        TRegion *reg = vi.getRegion(i);
        if (reg->contains(convert(it->first + delta)) &&
            reg->contains(convert(it->second + delta))) {
          isContained = true;
        }
      }
      if (!isContained)
        it = segments.erase(it);
      else
        ++it;
    }
  }

  void checkSegments(std::vector<TAutocloser::Segment> &segments, TRectD rect,
                     const TPoint &delta) {
    std::vector<TAutocloser::Segment>::iterator it = segments.begin();
    while (it != segments.end()) {
      int i;
      bool isContained = false;
      if (rect.contains(convert(it->first + delta)) &&
          rect.contains(convert(it->second + delta))) {
        isContained = true;
      }
      if (!isContained)
        it = segments.erase(it);
      else
        ++it;
    }
  }
  //-------------------------------------------------------------------

  void multiAutocloseRegion(TStroke *stroke, const TMouseEvent &e) {
    TTool::Application *app = TTool::getApplication();
    if (m_firstStroke) {
      multiApplyAutoclose(m_firstFrameId, getFrameId(), TRectD(), TRectD(),
                          m_firstStroke, stroke);
      invalidate();
      if (e.isShiftPressed()) {
        delete m_firstStroke;
        m_firstStroke  = new TStroke(*stroke);
        m_firstFrameId = getFrameId();
      } else {
        if (m_isXsheetCell) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
        resetMulti();
      }
    } else {
      m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
      // if (m_isXsheetCell)
      m_currCell    = std::pair<int, int>(getColumnIndex(), getFrame());
      m_firstStroke = new TStroke(*stroke);
    }
    return;
  }

  //------------------------------------------------------------------------

  void doClose(double t, const TFrameId id, const TImageP &img,
               const TVectorImageP &firstImage,
               const TVectorImageP &lastImage) {
    if (t == 0)
      applyAutoclose(img, id, TRectD(), firstImage->getStroke(0));
    else if (t == 1)
      applyAutoclose(img, id, TRectD(), lastImage->getStroke(0));
    else {
      assert(firstImage->getStrokeCount() == 1);
      assert(lastImage->getStrokeCount() == 1);
      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
      assert(vi->getStrokeCount() == 1);
      applyAutoclose(img, id, TRectD(), vi->getStroke(0));
    }
  }

  //-------------------------------------------------------------------

} rasterTapeTool;
