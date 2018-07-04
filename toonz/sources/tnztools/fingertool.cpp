//------------------------------------------------------
/*! Finger Tool : 線のノイズを埋めるツール
*/
#include "tstroke.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "drawutil.h"
#include "tcolorstyles.h"
#include "tundo.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "toonz/strokegenerator.h"
#include "toonz/ttilesaver.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/observer.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"
#include "toonz/ttileset.h"
#include "toonz/rasterstrokegenerator.h"
#include "toonz/preferences.h"
#include "tgl.h"
#include "tenv.h"

#include "trop.h"

#include "tinbetween.h"
#include "ttile.h"

#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "tools/toolhandle.h"
#include "tools/toolutils.h"

// For Qt translation support
#include <QCoreApplication>

#include "tools/stylepicker.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"
#include "historytypes.h"

using namespace ToolUtils;

TEnv::IntVar FingerInvert("InknpaintFingerInvert", 0);
TEnv::DoubleVar FingerSize("InknpaintFingerSize", 10);

//-----------------------------------------------------------------------------

const int BackgroundStyle = 0;

//-----------------------------------------------------------------------------

namespace {

class FingerUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_invert;

public:
  FingerUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
             int styleId, bool invert, TXshSimpleLevel *level,
             const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_invert(invert) {}

  void redo() const override {
    TToonzImageP image = m_level->getFrame(m_frameId, true);
    TRasterCM32P ras   = image->getRaster();
    RasterStrokeGenerator m_rasterTrack(ras, FINGER, INK, m_styleId,
                                        m_points[0], m_invert, 0, false);
    m_rasterTrack.setPointsSequence(m_points);
    m_rasterTrack.generateStroke(true);
    image->setSavebox(image->getSavebox() +
                      m_rasterTrack.getBBox(m_rasterTrack.getPointsSequence()));

    ToolUtils::updateSaveBox();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Finger Tool"); }
  int getHistoryType() override { return HistoryType::FingerTool; }
};

//-------------------------------------------------------------------------------------------

void drawLine(const TPointD &point, const TPointD &centre, bool horizontal,
              bool isDecimal) {
  if (!isDecimal) {
    if (horizontal) {
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);

      tglDrawSegment(TPointD(point.y - 0.5, point.x + 0.5) + centre,
                     TPointD(point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y + 0.5) + centre,
                     TPointD(point.x - 1.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(-point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
    } else {
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 1.5) + centre,
                     TPointD(point.x - 1.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x + 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);

      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, -point.y - 0.5) + centre,
                     TPointD(point.x - 1.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, -point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 1.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    }
  } else {
    if (horizontal) {
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.x - 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    } else {
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 1.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 1.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 1.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y - 1.5) + centre,
                     TPointD(point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y - 0.5) + centre,
                     TPointD(point.x + 0.5, -point.y - 0.5) + centre);

      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 1.5) + centre,
                     TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    }
  }
}

//-------------------------------------------------------------------------------------------------------

void drawEmptyCircle(int thick, const TPointD &mousePos, bool isPencil,
                     bool isLxEven, bool isLyEven) {
  TPointD pos = mousePos;
  if (isLxEven) pos.x += 0.5;
  if (isLyEven) pos.y += 0.5;
  if (!isPencil)
    tglDrawCircle(pos, (thick + 1) * 0.5);
  else {
    int x = 0, y = tround((thick * 0.5) - 0.5);
    int d           = 3 - 2 * (int)(thick * 0.5);
    bool horizontal = true, isDecimal = thick % 2 != 0;
    drawLine(TPointD(x, y), pos, horizontal, isDecimal);
    while (y > x) {
      if (d < 0) {
        d          = d + 4 * x + 6;
        horizontal = true;
      } else {
        d          = d + 4 * (x - y) + 10;
        horizontal = false;
        y--;
      }
      x++;
      drawLine(TPointD(x, y), pos, horizontal, isDecimal);
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

class FingerTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(FingerTool)

  RasterStrokeGenerator *m_rasterTrack;

  bool m_firstTime;

  double m_pointSize, m_distance2;

  bool m_selecting;
  TTileSaverCM32 *m_tileSaver;

  TPointD m_mousePos;

  TIntProperty m_toolSize;
  TBoolProperty m_invert;
  TPropertyGroup m_prop;
  int m_cursor;

  /*---	作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）
                  に別のフレームに移動している場合があるため ---*/
  TFrameId m_workingFrameId;

  /*-- 最初のクリックでStyleを切り替える --*/
  void pick(const TPointD &pos);

public:
  FingerTool();

  void draw() override;
  void update(TToonzImageP ti, TRectD area);

  void updateTranslation() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;
  void onDeactivate() override;
  bool onPropertyChanged(std::string propertyName) override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }
  ToolType getToolType() const override { return TTool::LevelWriteTool; }
  int getCursorId() const override { return m_cursor; }

  int getColorClass() const { return 2; }

  /*--
   * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
   * --*/
  void finishBrush();
};

FingerTool fingerTool;

//=============================================================================
//
//  InkPaintTool implemention
//
//-----------------------------------------------------------------------------

FingerTool::FingerTool()
    : TTool("T_Finger")
    , m_rasterTrack(0)
    , m_pointSize(-1)
    , m_selecting(false)
    , m_tileSaver(0)
    , m_cursor(ToolCursor::EraserCursor)
    , m_toolSize("Size:", 1, 100, 10, false)
    , m_invert("Invert", false)
    , m_firstTime(true)
    , m_workingFrameId(TFrameId()) {
  bind(TTool::ToonzImage);

  m_prop.bind(m_toolSize);
  m_prop.bind(m_invert);

  m_invert.setId("Invert");
}

//-----------------------------------------------------------------------------

void FingerTool::updateTranslation() {
  m_toolSize.setQStringName(tr("Size:"));
  m_invert.setQStringName(tr("Invert", NULL));
}

//-----------------------------------------------------------------------------

void FingerTool::draw() {
  if (m_pointSize == -1) {
    return;
  }

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  TToonzImageP ti = (TToonzImageP)getImage(false);
  if (!ti) return;
  TRasterP ras = ti->getRaster();
  int lx       = ras->getLx();
  int ly       = ras->getLy();

  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint))
    glColor3d(0.5, 0.8, 0.8);
  else
    glColor3d(1.0, 0.0, 0.0);

  drawEmptyCircle(m_toolSize.getValue(), m_mousePos, true, lx % 2 == 0,
                  ly % 2 == 0);
}

//-----------------------------------------------------------------------------

const UINT pointCount = 20;

//-----------------------------------------------------------------------------

bool FingerTool::onPropertyChanged(std::string propertyName) {
  /*-- サイズ --*/
  if (propertyName == m_toolSize.getName()) {
    FingerSize = m_toolSize.getValue();
    double x   = m_toolSize.getValue();

    double minRange = 1;
    double maxRange = 100;

    double minSize = 0.01;
    double maxSize = 100;

    m_pointSize =
        (x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize;
    invalidate();
  }

  // Invert
  else if (propertyName == m_invert.getName()) {
    FingerInvert = (int)(m_invert.getValue());
  }

  return true;
}

//-----------------------------------------------------------------------------

void FingerTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  pick(pos);

  m_selecting = true;
  TImageP image(getImage(true));

  if (TToonzImageP ti = image) {
    TRasterCM32P ras = ti->getRaster();
    if (ras) {
      int thickness = m_toolSize.getValue();
      int styleId   = TTool::getApplication()->getCurrentLevelStyleIndex();
      TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
      m_tileSaver           = new TTileSaverCM32(ras, tileSet);
      m_rasterTrack         = new RasterStrokeGenerator(
          ras, FINGER, INK, styleId,
          TThickPoint(pos + convert(ras->getCenter()), thickness),
          m_invert.getValue(), 0, false);

      /*-- 作業中Fidを現在のFIDにする --*/
      m_workingFrameId = getFrameId();

      m_tileSaver->save(m_rasterTrack->getLastRect());
      TRect modifiedBbox = m_rasterTrack->generateLastPieceOfStroke(true);
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void FingerTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_selecting) return;

  m_mousePos = pos;
  if (TToonzImageP ri = TImageP(getImage(true))) {
    /*---	マウスを動かしながらショートカットで切り替わった場合、
                    いきなりleftButtonDragから呼ばれることがあり、
                    m_rasterTrackが無くて落ちることがある。 ---*/
    if (m_rasterTrack) {
      int thickness = m_toolSize.getValue();
      m_rasterTrack->add(
          TThickPoint(pos + convert(ri->getRaster()->getCenter()), thickness));
      m_tileSaver->save(m_rasterTrack->getLastRect());
      TRect modifiedBbox = m_rasterTrack->generateLastPieceOfStroke(true);
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void FingerTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_selecting) return;

  m_mousePos = pos;

  finishBrush();
}

//-----------------------------------------------------------------------------

void FingerTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_mousePos = pos;
  TPointD pp(tround(pos.x), tround(pos.y));
  m_mousePos = pp;
  invalidate();
}

//-----------------------------------------------------------------------------

void FingerTool::onEnter() {
  if (m_firstTime) {
    m_invert.setValue(FingerInvert ? 1 : 0);
    m_toolSize.setValue(FingerSize);
    m_firstTime = false;
  }
  double x = m_toolSize.getValue();

  double minRange = 1;
  double maxRange = 100;

  double minSize = 0.01;
  double maxSize = 100;

  m_pointSize =
      (x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize;

  if ((TToonzImageP)getImage(false))
    m_cursor = ToolCursor::PenCursor;
  else
    m_cursor = ToolCursor::CURSOR_NO;
}

//-----------------------------------------------------------------------------

void FingerTool::onLeave() { m_pointSize = -1; }

//-----------------------------------------------------------------------------

void FingerTool::onActivate() { onEnter(); }

//-----------------------------------------------------------------------------

void FingerTool::onDeactivate() {
  /*---
   * マウスドラッグ中(m_selecting=true)にツールが切り替わったときに線を終わらせる
   * ---*/
  if (m_selecting) finishBrush();
}

//-----------------------------------------------------------------------------
/*!
 * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
*/
void FingerTool::finishBrush() {
  if (TToonzImageP ti = (TToonzImageP)getImage(true)) {
    if (m_rasterTrack) {
      int thickness = m_toolSize.getValue();
      m_rasterTrack->add(TThickPoint(
          m_mousePos + convert(ti->getRaster()->getCenter()), thickness));
      m_tileSaver->save(m_rasterTrack->getLastRect());
      TRect modifiedBbox = m_rasterTrack->generateLastPieceOfStroke(true, true);

      TTool::Application *app   = TTool::getApplication();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();

      TFrameId frameId =
          m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

      TUndoManager::manager()->add(new FingerUndo(
          m_tileSaver->getTileSet(), m_rasterTrack->getPointsSequence(),
          m_rasterTrack->getStyleId(), m_rasterTrack->isSelective(),
          simLevel.getPointer(), frameId));
      ToolUtils::updateSaveBox();

      /*! FIdを指定して、作業中にフレームが動いても、
              クリック時のFidのサムネイルが更新されるようにする。
      */
      notifyImageChanged(frameId);

      invalidate();
      delete m_rasterTrack;
      m_rasterTrack = 0;
      delete m_tileSaver;

      /*-- 作業中fIdをリセット --*/
      m_workingFrameId = TFrameId();
    }
  }
  m_selecting = false;
}

void FingerTool::pick(const TPointD &pos) {
  int modeValue = 2;  // LINES

  TImageP image    = getImage(false);
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  TXshSimpleLevel *level =
      getApplication()->getCurrentLevel()->getSimpleLevel();
  if (!ti || !level) return;

  /*--- 画面外をpickしても拾えないようにする ---*/
  if (!m_viewer->getGeometry().contains(pos)) return;

  int subsampling = level->getImageSubsampling(getCurrentFid());

  StylePicker picker(image);

  int styleId =
      picker.pickStyleId(TScale(1.0 / subsampling) * pos + TPointD(-0.5, -0.5),
                         getPixelSize() * getPixelSize(), modeValue);

  if (styleId < 0) return;

  if (modeValue == 2)  // LINES
  {
    /*--- pickLineモードのとき、取得Styleが0の場合はカレントStyleを変えない。
     * ---*/
    if (styleId == 0) return;

    /*---
     * pickLineモードのとき、PurePaintの部分をクリックしてもカレントStyleを変えない
     * ---*/
    if (ti &&
        picker.pickTone(TScale(1.0 / subsampling) * pos +
                        TPointD(-0.5, -0.5)) == 255)
      return;
  }

  /*--- Styleを選択している場合は選択を解除する ---*/
  TSelection *selection =
      TTool::getApplication()->getCurrentSelection()->getSelection();
  if (selection) {
    TStyleSelection *styleSelection =
        dynamic_cast<TStyleSelection *>(selection);
    if (styleSelection) styleSelection->selectNone();
  }

  getApplication()->setCurrentLevelStyleIndex(styleId);
}
