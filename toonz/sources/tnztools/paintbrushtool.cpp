

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

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

TEnv::StringVar PaintBrushColorType("InknpaintPaintBrushColorType", "Areas");
TEnv::IntVar PaintBrushSelective("InknpaintPaintBrushSelective", 0);
TEnv::DoubleVar PaintBrushSize("InknpaintPaintBrushSize", 10);

//-----------------------------------------------------------------------------

const int BackgroundStyle = 0;

//-----------------------------------------------------------------------------

namespace {

class BrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  ColorType m_colorType;

public:
  BrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
            ColorType colorType, int styleId, bool selective,
            TXshSimpleLevel *level, const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_colorType(colorType) {}

  void redo() const override {
    TToonzImageP image = m_level->getFrame(m_frameId, true);
    TRasterCM32P ras   = image->getRaster();
    RasterStrokeGenerator m_rasterTrack(ras, PAINTBRUSH, m_colorType, m_styleId,
                                        m_points[0], m_selective, 0, false);
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

  QString getToolName() override { return QString("Paint Brush Tool"); }
  int getHistoryType() override { return HistoryType::PaintBrushTool; }
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

class PaintBrushTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(PaintBrushTool)

  RasterStrokeGenerator *m_rasterTrack;

  bool m_firstTime;

  double m_pointSize, m_distance2;

  bool m_selecting;
  TTileSaverCM32 *m_tileSaver;

  TPointD m_mousePos;

  TIntProperty m_toolSize;
  TBoolProperty m_onlyEmptyAreas;
  TEnumProperty m_colorType;
  TPropertyGroup m_prop;
  int m_cursor;
  ColorType m_colorTypeBrush;
  /*--
     描画開始時のFrameIdを保存し、マウスリリース時（Undoの登録時）に別のフレームに
          移動している場合に備える --*/
  TFrameId m_workingFrameId;

public:
  PaintBrushTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

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

  int getCursorId() const override { return m_cursor; }

  int getColorClass() const { return 2; }

  /*---
   * 描画中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ終了処理を行う
   * ---*/
  void finishBrush();
  /*--- Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す。
  　　　PaintBrushはピクセルのStyleIndexを入れ替えるツールのため、
     　 アンチエイリアスは存在しない、いわば常にPencilMode ---*/
  bool isPencilModeActive() override { return true; }
};

PaintBrushTool paintBrushTool;

//=============================================================================
//
//  InkPaintTool implemention
//
//-----------------------------------------------------------------------------

PaintBrushTool::PaintBrushTool()
    : TTool("T_PaintBrush")
    , m_rasterTrack(0)
    , m_pointSize(-1)
    , m_selecting(false)
    , m_tileSaver(0)
    , m_cursor(ToolCursor::EraserCursor)
    // sostituire i nomi con quelli del current, tipo W_ToolOptions...
    , m_toolSize("Size:", 1, 100, 10, false)  // W_ToolOptions_BrushToolSize
    , m_colorType("Mode:")                    // W_ToolOptions_InkOrPaint
    , m_onlyEmptyAreas("Selective", false)    // W_ToolOptions_Selective
    , m_firstTime(true)
    , m_workingFrameId(TFrameId()) {
  m_colorType.addValue(LINES);
  m_colorType.addValue(AREAS);
  m_colorType.addValue(ALL);

  bind(TTool::ToonzImage);

  m_prop.bind(m_toolSize);
  m_prop.bind(m_colorType);
  m_prop.bind(m_onlyEmptyAreas);

  m_onlyEmptyAreas.setId("Selective");
  m_colorType.setId("Mode");
}

//-----------------------------------------------------------------------------

void PaintBrushTool::updateTranslation() {
  m_toolSize.setQStringName(tr("Size:"));

  m_colorType.setQStringName(tr("Mode:"));
  m_colorType.setItemUIName(LINES, tr("Lines"));
  m_colorType.setItemUIName(AREAS, tr("Areas"));
  m_colorType.setItemUIName(ALL, tr("Lines & Areas"));

  m_onlyEmptyAreas.setQStringName(tr("Selective", NULL));
}

//-----------------------------------------------------------------------------

void PaintBrushTool::draw() {
  /*-- MouseLeave時にBrushTipが描かれるのを防止する --*/
  if (m_pointSize == -1) return;

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  TToonzImageP ti = (TToonzImageP)getImage(false);
  if (!ti) return;
  TRasterP ras = ti->getRaster();
  int lx       = ras->getLx();
  int ly       = ras->getLy();

  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
    glColor3d(0.5, 0.8, 0.8);
  else
    glColor3d(1.0, 0.0, 0.0);

  drawEmptyCircle(m_toolSize.getValue(), m_mousePos, true, lx % 2 == 0,
                  ly % 2 == 0);
}

//-----------------------------------------------------------------------------

const UINT pointCount = 20;

//-----------------------------------------------------------------------------

bool PaintBrushTool::onPropertyChanged(std::string propertyName) {
  /*-- Size ---*/
  if (propertyName == m_toolSize.getName()) {
    PaintBrushSize = m_toolSize.getValue();
    double x       = m_toolSize.getValue();

    double minRange = 1;
    double maxRange = 100;

    double minSize = 0.01;
    double maxSize = 100;

    m_pointSize =
        (x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize;
    invalidate();
  }

  // Selective
  else if (propertyName == m_onlyEmptyAreas.getName()) {
    if (m_onlyEmptyAreas.getValue() && m_colorType.getValue() == LINES) {
      m_colorType.setValue(AREAS);
      PaintBrushColorType = ::to_string(m_colorType.getValue());
    }
    PaintBrushSelective = (int)(m_onlyEmptyAreas.getValue());
  }

  // Areas, Lines etc.
  else if (propertyName == m_colorType.getName()) {
    if (m_colorType.getValue() == LINES) {
      PaintBrushSelective = (int)(m_onlyEmptyAreas.getValue());
    }
    PaintBrushColorType = ::to_string(m_colorType.getValue());
    /*--- ColorModelのCursor更新のためにSIGNALを出す ---*/
    TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  }

  return true;
}

//-----------------------------------------------------------------------------

void PaintBrushTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_selecting = true;
  TImageP image(getImage(true));
  if (m_colorType.getValue() == LINES) m_colorTypeBrush = INK;
  if (m_colorType.getValue() == AREAS) m_colorTypeBrush = PAINT;
  if (m_colorType.getValue() == ALL) m_colorTypeBrush   = INKNPAINT;

  if (TToonzImageP ti = image) {
    TRasterCM32P ras = ti->getRaster();
    if (ras) {
      int thickness = m_toolSize.getValue();
      int styleId   = TTool::getApplication()->getCurrentLevelStyleIndex();
      TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
      m_tileSaver           = new TTileSaverCM32(ras, tileSet);
      m_rasterTrack         = new RasterStrokeGenerator(
          ras, PAINTBRUSH, m_colorTypeBrush, styleId,
          TThickPoint(pos + convert(ras->getCenter()), thickness),
          m_onlyEmptyAreas.getValue(), 0, false);
      /*-- 現在のFidを記憶 --*/
      m_workingFrameId = getFrameId();
      m_tileSaver->save(m_rasterTrack->getLastRect());
      m_rasterTrack->generateLastPieceOfStroke(true);
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void PaintBrushTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_selecting) return;

  m_mousePos = pos;
  if (TToonzImageP ri = TImageP(getImage(true))) {
    /*--- マウスを動かしながらショートカットでこのツールに切り替わった場合、
            いきなりleftButtonDragから呼ばれることがあり、m_rasterTrackが無い可能性がある
　---*/
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

void PaintBrushTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_selecting) return;

  m_mousePos = pos;

  finishBrush();
}

//-----------------------------------------------------------------------------

void PaintBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_mousePos = pos;
  TPointD pp(tround(pos.x), tround(pos.y));
  m_mousePos = pp;
  invalidate();
}

//-----------------------------------------------------------------------------

void PaintBrushTool::onEnter() {
  if (m_firstTime) {
    m_onlyEmptyAreas.setValue(PaintBrushSelective ? 1 : 0);
    m_colorType.setValue(::to_wstring(PaintBrushColorType.getValue()));
    m_toolSize.setValue(PaintBrushSize);
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

void PaintBrushTool::onLeave() { m_pointSize = -1; }

//-----------------------------------------------------------------------------

void PaintBrushTool::onActivate() { onEnter(); }

//-----------------------------------------------------------------------------

void PaintBrushTool::onDeactivate() {
  /*--マウスドラッグ中(m_selecting=true)にツールが切り替わったときに描画の終了処理を行う---*/
  if (m_selecting) finishBrush();
}

//-----------------------------------------------------------------------------
/*!
 * 描画中にツールが切り替わった場合に備え、onDeactivateでもMouseReleaseと同じ終了処理を行う
*/
void PaintBrushTool::finishBrush() {
  if (TToonzImageP ti = (TToonzImageP)getImage(true)) {
    if (m_rasterTrack) {
      int thickness = m_toolSize.getValue();
      m_rasterTrack->add(TThickPoint(
          m_mousePos + convert(ti->getRaster()->getCenter()), thickness));
      m_tileSaver->save(m_rasterTrack->getLastRect());
      m_rasterTrack->generateLastPieceOfStroke(true, true);

      TTool::Application *app   = TTool::getApplication();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();

      /*-- 描画中にフレームが動いても、描画開始時のFidに対してUndoを記録する
       * --*/
      TFrameId frameId =
          m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

      TUndoManager::manager()->add(new BrushUndo(
          m_tileSaver->getTileSet(), m_rasterTrack->getPointsSequence(),
          m_colorTypeBrush, m_rasterTrack->getStyleId(),
          m_rasterTrack->isSelective(), simLevel.getPointer(), frameId));
      ToolUtils::updateSaveBox();

      /*--- FIdを指定して、描画中にフレームが変わっても、
              クリック時のFidのサムネイルが更新されるようにする。---*/
      notifyImageChanged(frameId);

      invalidate();
      delete m_rasterTrack;
      m_rasterTrack = 0;
      delete m_tileSaver;

      /*--- 作業中のフレームをリセット ---*/
      m_workingFrameId = TFrameId();
    }

    ToolUtils::updateSaveBox();
  }

  m_selecting = false;
}
