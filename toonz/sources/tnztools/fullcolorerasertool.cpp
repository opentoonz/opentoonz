

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolcommandids.h"
#include "tools/toolutils.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"

#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/trasterimageutils.h"
#include "toonz/stage2.h"
#include "toonz/levelproperties.h"
#include "toonz/strokegenerator.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tproperty.h"
#include "trasterimage.h"
#include "bluredbrush.h"
#include "trop.h"
#include "tgl.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tinbetween.h"
#include "tpixelutils.h"

// Qt includes
#include <QCoreApplication>

using namespace ToolUtils;

#define NORMALERASE L"Normal"
#define RECTERASE L"Rectangular"
#define FREEHANDERASE L"Freehand"
#define POLYLINEERASE L"Polyline"

TEnv::DoubleVar FullcolorEraseSize("FullcolorEraseSize", 5);
TEnv::DoubleVar FullcolorEraseHardness("FullcolorEraseHardness", 100);
TEnv::DoubleVar FullcolorEraserOpacity("FullcolorEraserOpacity", 100);
TEnv::StringVar FullcolorEraserType("FullcolorEraseType", "Normal");
TEnv::IntVar FullcolorEraserInvert("FullcolorEraseInvert", 0);
TEnv::IntVar FullcolorEraserRange("FullcolorEraseRange", 0);

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

int computeThickness(int pressure, const TIntPairProperty &property) {
  double p   = pressure / 255.0;
  double t   = p * p * p;
  int thick0 = property.getValue().first;
  int thick1 = property.getValue().second;

  return tround(thick0 + (thick1 - thick0) * t);
}

//----------------------------------------------------------------------------------

void eraseImage(const TRasterImageP &ri, const TRaster32P &image,
                const TPoint &pos, bool invert) {
  TRect rasBounds   = ri->getRaster()->getBounds();
  TRect imageBounds = image->getBounds() + pos;

  if (invert) {
    TRect rect(imageBounds.x0, rasBounds.y0, rasBounds.x1, imageBounds.y0 - 1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(imageBounds.x1 + 1, imageBounds.y0, rasBounds.x1, rasBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(rasBounds.x0, imageBounds.y1 + 1, imageBounds.x1, rasBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(rasBounds.x0, rasBounds.y0, imageBounds.x0 - 1, imageBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));
  }

  TRaster32P workRas;
  if (TRasterGR8P ras = ri->getRaster()) {
    workRas = TRaster32P(imageBounds.getSize());
    TRop::convert(workRas, ras->extract(imageBounds));
  } else
    workRas = ri->getRaster()->extract(imageBounds);

  int y;
  for (y = 0; y < workRas->getLy(); y++) {
    TPixel32 *outPix    = workRas->pixels(y);
    TPixel32 *outEndPix = outPix + workRas->getLx();
    TPixel32 *inPix     = image->pixels(y);
    for (outPix; outPix != outEndPix; outPix++, inPix++) {
      if (outPix->m == 0) continue;
      TPixel32 pix = depremultiply(*outPix);
      pix.m =
          invert ? pix.m * (inPix->m) / 255 : pix.m * (255 - inPix->m) / 255;
      *outPix = premultiply(pix);
    }
  }

  if (TRasterGR8P ras = ri->getRaster()) {
    TRop::addBackground(workRas, TPixel32::White);
    TRop::over(ras, workRas, pos);
  }
}

//----------------------------------------------------------------------------------

class RectFullColorUndo final : public TFullColorRasterUndo {
  TRectD m_modifyArea;
  TStroke *m_stroke;
  std::wstring m_eraseType;
  bool m_invert;

public:
  RectFullColorUndo(TTileSetFullColor *tileSet, const TRectD &modifyArea,
                    TStroke stroke, std::wstring eraseType,
                    TXshSimpleLevel *level, bool invert,
                    const TFrameId &frameId)
      : TFullColorRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_modifyArea(modifyArea)
      , m_eraseType(eraseType)
      , m_invert(invert) {
    m_stroke = new TStroke(stroke);
  }

  ~RectFullColorUndo() { delete m_stroke; }

  void redo() const override {
    TRasterImageP ri = getImage();
    if (!ri) return;

    if (m_eraseType == RECTERASE)
      TRect rect = TRasterImageUtils::eraseRect(ri, m_modifyArea);
    else if (m_eraseType == FREEHANDERASE || m_eraseType == POLYLINEERASE) {
      TPoint pos;

      TRaster32P image =
          convertStrokeToImage(m_stroke, ri->getRaster()->getBounds(), pos);
      if (!image) return;

      eraseImage(ri, image, pos, m_invert);
    }

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return TFullColorRasterUndo::getSize() +
           m_stroke->getControlPointCount() * sizeof(TThickPoint) + 100 +
           sizeof(this);
  }

  QString getToolName() override {
    return QString("Raster Eraser Tool (Rect)");
  }

  int getHistoryType() override { return HistoryType::EraserTool; }
};

//----------------------------------------------------------------------------------

class FullColorEraserUndo final : public TFullColorRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_size;
  double m_hardness;
  double m_opacity;

public:
  FullColorEraserUndo(TTileSetFullColor *tileSet,
                      const std::vector<TThickPoint> &points,
                      TXshSimpleLevel *level, const TFrameId &frameId, int size,
                      double hardness, double opacity)
      : TFullColorRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_size(size)
      , m_hardness(hardness)
      , m_opacity(opacity) {}

  void redo() const override {
    if (m_points.size() == 0) return;
    TRasterImageP image      = getImage();
    TRasterP ras             = image->getRaster();
    QRadialGradient brushPad = getBrushPad(m_size, m_hardness);
    TRaster32P workRaster    = TRaster32P(ras->getSize());
    TRasterP backUpRas       = ras->clone();
    workRaster->clear();

    BluredBrush brush(workRaster, m_size, brushPad, false);
    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
    if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1],
                   1, 1);
      brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
      }
    }

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TFullColorRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Raster Eraser Tool"); }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//----------------------------------------------------------------------------------

void eraseStroke(const TRasterImageP &ri, TStroke *stroke,
                 std::wstring eraseType, bool invert,
                 const TXshSimpleLevelP &level, const TFrameId &frameId) {
  assert(stroke);
  TPoint pos;
  TRasterP ras = ri->getRaster();

  TRaster32P image = convertStrokeToImage(stroke, ras->getBounds(), pos);
  if (!image) return;

  TRect rasterErasedArea = image->getBounds() + pos;
  TRect area;
  if (!invert)
    area = rasterErasedArea.enlarge(2);
  else
    area = ras->getBounds();

  TTileSetFullColor *tileSet = new TTileSetFullColor(ras->getSize());
  tileSet->add(ras, area);

  TUndoManager::manager()->add(
      new RectFullColorUndo(tileSet, convert(area), *stroke, eraseType,
                            level.getPointer(), invert, frameId));

  eraseImage(ri, image, pos, invert);
}

}  // namespace

//**********************************************************************************
//    FullColorEraserTool  definition
//**********************************************************************************

class FullColorEraserTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(FullColorEraserTool)

public:
  FullColorEraserTool(std::string name);
  ~FullColorEraserTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;
  void onEnter() override;

  void update(const TRasterImageP &ri, TRectD selArea,
              const TXshSimpleLevelP &level, bool multi = false,
              const TFrameId &frameId = -1);
  void multiUpdate(const TRectD firstRect, const TRectD lastRect);

  void multiAreaEraser(TFrameId &firstFid, TFrameId &lastFid,
                       TStroke *firstStroke, TStroke *lastStroke);

  void doMultiEraser(const TImageP &img, double t, const TFrameId &fid,
                     const TVectorImageP &firstImage,
                     const TVectorImageP &lastImage);

  void resetMulti();

private:
  TPropertyGroup m_prop;

  TIntProperty m_size;
  TDoubleProperty m_opacity;
  TDoubleProperty m_hardness;
  TEnumProperty m_eraseType;
  TBoolProperty m_invertOption;
  TBoolProperty m_multi;

  TXshSimpleLevelP m_level;
  std::pair<int, int> m_currCell;

  TFrameId m_firstFrameId, m_veryFirstFrameId;

  TRaster32P m_workRaster;
  TRasterP m_backUpRas;

  QRadialGradient m_brushPad;

  std::vector<TThickPoint> m_points;
  BluredBrush *m_brush;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  StrokeGenerator m_track;
  std::vector<TPointD> m_polyline;
  TStroke *m_firstStroke;

  TRectD m_selectingRect, m_firstRect;

  TPointD m_mousePos, m_brushPos, m_firstPos;
  TMouseEvent m_mouseEvent;
  double m_thick;

  bool m_firstTime, m_selecting, m_firstFrameSelected, m_isXsheetCell;
  bool m_mousePressed = false;

} fullColorEraser(T_Eraser);  // Tools are statically instantiated

//===================================================================================================

FullColorEraserTool::FullColorEraserTool(std::string name)
    : TTool(name)
    , m_size("Size:", 1, 100, 5, false)
    , m_opacity("Opacity:", 0, 100, 100)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_eraseType("Type:")
    , m_invertOption("Invert", false)
    , m_multi("Frame Range", false)
    , m_currCell(-1, -1)
    , m_brush(0)
    , m_tileSet(0)
    , m_tileSaver(0)
    , m_thick(0.5)
    , m_firstTime(true)
    , m_selecting(false)
    , m_firstFrameSelected(false)
    , m_isXsheetCell(false) {
  bind(TTool::RasterImage);

  m_prop.bind(m_size);
  m_prop.bind(m_hardness);
  m_prop.bind(m_opacity);
  m_prop.bind(m_eraseType);
  m_prop.bind(m_invertOption);
  m_prop.bind(m_multi);

  m_eraseType.addValue(NORMALERASE);
  m_eraseType.addValue(RECTERASE);
  m_eraseType.addValue(FREEHANDERASE);
  m_eraseType.addValue(POLYLINEERASE);
}

//---------------------------------------------------------------------------------------------------

FullColorEraserTool::~FullColorEraserTool() { delete m_firstStroke; }

//---------------------------------------------------------------------------------------------------

void FullColorEraserTool::updateTranslation() {
  m_size.setQStringName(tr("Size:"));
  m_opacity.setQStringName(tr("Opacity:"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_eraseType.setQStringName(tr("Type:"));
  m_invertOption.setQStringName(tr("Invert"));
  m_multi.setQStringName(tr("Frame Range"));
}

//---------------------------------------------------------------------------------------------------

void FullColorEraserTool::onActivate() {
  if (m_firstTime) {
    m_firstTime = false;
    m_size.setValue(FullcolorEraseSize);
    m_opacity.setValue(FullcolorEraserOpacity);
    m_hardness.setValue(FullcolorEraseHardness);
    m_eraseType.setValue(::to_wstring(FullcolorEraserType.getValue()));
    m_invertOption.setValue((bool)FullcolorEraserInvert);
    m_multi.setValue((bool)FullcolorEraserRange);
    m_firstTime = false;
  }

  m_brushPad = getBrushPad(m_size.getValue(), m_hardness.getValue() * 0.01);
}

//--------------------------------------------------------------------------------------------------

void FullColorEraserTool::onDeactivate() {
  if (m_mousePressed) leftButtonUp(m_mousePos, m_mouseEvent);
}

//--------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;
  m_mouseEvent            = e;
  m_mousePressed          = true;
  TRasterImageP ri        = (TRasterImageP)getImage(true);
  if (!ri) return;
  TRectD invalidateRect;
  TRasterP ras = ri->getRaster();
  if (m_eraseType.getValue() == NORMALERASE) {
    TDimension dim  = ras->getSize();
    double opacity  = m_opacity.getValue() * 0.01;
    double hardness = m_hardness.getValue() * 0.01;
    m_workRaster    = TRaster32P(dim);
    m_workRaster->clear();
    m_backUpRas = ras->clone();

    int maxThick      = m_size.getValue();
    TPointD rasCenter = ras->getCenterD();
    TThickPoint point(pos + rasCenter, maxThick);
    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    invalidateRect = TRectD(pos - halfThick, pos + halfThick);

    m_points.clear();
    m_points.push_back(point);

    m_tileSet   = new TTileSetFullColor(ras->getSize());
    m_tileSaver = new TTileSaverFullColor(ras, m_tileSet);
    m_brush =
        new BluredBrush(m_workRaster, m_size.getValue(), m_brushPad, false);
    TRect bbox = m_brush->getBoundFromPoints(m_points);
    m_tileSaver->save(bbox);
    m_brush->addPoint(point, 1);
    m_brush->eraseDrawing(ras, m_backUpRas, bbox, opacity);
  } else if (m_eraseType.getValue() == RECTERASE) {
    if (m_multi.getValue() && m_firstRect.isEmpty()) {
      invalidateRect = m_selectingRect;
      m_selectingRect.empty();
      invalidate(invalidateRect.enlarge(2));
    }
    m_selecting        = true;
    m_selectingRect.x0 = pos.x;
    m_selectingRect.y0 = pos.y;
    m_selectingRect.x1 = pos.x + 1;
    m_selectingRect.y1 = pos.y + 1;
    invalidateRect     = m_selectingRect;
  } else if (m_eraseType.getValue() == FREEHANDERASE ||
             m_eraseType.getValue() == POLYLINEERASE) {
    if (m_multi.getValue() && m_firstStroke && !m_firstFrameSelected) {
      invalidateRect = m_firstStroke->getBBox();
      delete m_firstStroke;
      m_firstStroke = 0;
      invalidate(invalidateRect.enlarge(2));
    }

    m_track.clear();
    m_firstPos        = pos;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);

    if (m_eraseType.getValue() == POLYLINEERASE) {
      if (m_polyline.empty() || m_polyline.back() != pos)
        m_polyline.push_back(pos);
    }

    int maxThick = 2 * m_thick;
    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    invalidateRect = TRectD(pos - halfThick, pos + halfThick);
  }
  invalidate(invalidateRect.enlarge(2));
}

//-------------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;
  m_mouseEvent            = e;
  double pixelSize2       = getPixelSize() * getPixelSize();

  TRasterImageP ri = (TRasterImageP)getImage(true);
  if (!ri) return;
  if (m_eraseType.getValue() == NORMALERASE) {
    double thickness  = m_size.getValue();
    TDimension size   = m_workRaster->getSize();
    TPointD rasCenter = ri->getRaster()->getCenterD();
    TThickPoint point(pos + rasCenter, thickness);

    TThickPoint old = m_points.back();
    if (norm2(point - old) < 4) return;

    TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
    m_points.push_back(mid);
    m_points.push_back(point);

    int m          = m_points.size();
    double opacity = m_opacity.getValue() * 0.01;

    TThickPoint pa = m_points.front();
    std::vector<TThickPoint> points;
    points.push_back(pa);
    points.push_back(mid);
    TRect bbox;
    TRectD invalidateRect;
    if (m == 3) {
      TThickPoint pa = m_points.front();
      std::vector<TThickPoint> points;
      points.push_back(pa);
      points.push_back(mid);
      invalidateRect = ToolUtils::getBounds(points, thickness);
      bbox           = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
    } else {
      std::vector<TThickPoint> points;
      points.push_back(m_points[m - 4]);
      points.push_back(old);
      points.push_back(mid);
      invalidateRect = ToolUtils::getBounds(points, thickness);
      bbox           = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(m_points[m - 4], old, mid, 1, 1);
    }
    m_brush->eraseDrawing(ri->getRaster(), m_backUpRas, bbox, opacity);
    invalidate(invalidateRect.enlarge(2) - rasCenter);
  }
  if (m_eraseType.getValue() == RECTERASE) {
    assert(m_selecting);
    TRectD oldRect = m_selectingRect;
    if (oldRect.x0 > oldRect.x1) tswap(oldRect.x1, oldRect.x0);
    if (oldRect.y0 > oldRect.y1) tswap(oldRect.y1, oldRect.y0);
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;
    TRectD invalidateRect(m_selectingRect);
    if (invalidateRect.x0 > invalidateRect.x1)
      tswap(invalidateRect.x1, invalidateRect.x0);
    if (invalidateRect.y0 > invalidateRect.y1)
      tswap(invalidateRect.y1, invalidateRect.y0);
    invalidateRect += oldRect;
    invalidate(invalidateRect.enlarge(2));
  }
  if (m_eraseType.getValue() == FREEHANDERASE) {
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
    invalidate(m_track.getModifiedRegion());
  }
}

//---------------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonUp(const TPointD &pos,
                                       const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;
  TRasterImageP ri        = (TRasterImageP)getImage(true);
  if (!ri) return;
  if (m_eraseType.getValue() == NORMALERASE) {
    if (m_points.size() != 1) {
      TPointD rasCenter = ri->getRaster()->getCenterD();
      TThickPoint point(pos + rasCenter, m_size.getValue());
      m_points.push_back(point);
      int m = m_points.size();
      std::vector<TThickPoint> points;
      points.push_back(m_points[m - 3]);
      points.push_back(m_points[m - 2]);
      points.push_back(m_points[m - 1]);
      TRect bbox = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(points[0], points[1], points[2], 1, 1);
      double opacity = m_opacity.getValue() * 0.01;
      m_brush->eraseDrawing(ri->getRaster(), m_backUpRas, bbox, opacity);
      TRectD invalidateRect = ToolUtils::getBounds(points, m_size.getValue());
      invalidate(invalidateRect.enlarge(2) - rasCenter);
    }

    if (m_brush) {
      delete m_brush;
      m_brush = 0;
    }

    m_workRaster->unlock();
    double opacity  = m_opacity.getValue() * 0.01;
    double hardness = m_hardness.getValue() * 0.01;

    m_workRaster = TRaster32P();
    m_backUpRas  = TRaster32P();

    delete m_tileSaver;
    TTool::Application *app   = TTool::getApplication();
    TXshLevel *level          = app->getCurrentLevel()->getLevel();
    TXshSimpleLevelP simLevel = level->getSimpleLevel();
    TFrameId frameId          = getCurrentFid();
    TUndoManager::manager()->add(new FullColorEraserUndo(
        m_tileSet, m_points, simLevel.getPointer(), frameId, m_size.getValue(),
        m_hardness.getValue() * 0.01, opacity));
    notifyImageChanged();
  } else if (m_eraseType.getValue() == RECTERASE) {
    if (m_selectingRect.x0 > m_selectingRect.x1)
      tswap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      tswap(m_selectingRect.y1, m_selectingRect.y0);

    if (m_multi.getValue()) {
      TTool::Application *app = TTool::getApplication();
      if (m_firstFrameSelected) {
        multiUpdate(m_firstRect, m_selectingRect);
        notifyImageChanged();
        if (e.isShiftPressed()) {
          m_firstRect    = m_selectingRect;
          m_firstFrameId = getFrameId();
          invalidate();
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
        m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
      }
    } else {
      TTool::Application *app   = TTool::getApplication();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();
      TFrameId frameId          = getCurrentFid();
      if (m_invertOption.getValue()) {
        TUndoManager::manager()->beginBlock();
        TRect rect =
            TRasterImageUtils::convertWorldToRaster(m_selectingRect, ri);
        rect *= ri->getSavebox();
        TDimension dim = ri->getRaster()->getSize();
        TRectD rect01 =
            TRectD(TPointD(0., 0.), TPointD((double)rect.x0, (double)dim.ly));
        if (rect01.getLx() > 0 && rect01.getLy() > 0) {
          rect01 = TRasterImageUtils::convertRasterToWorld(convert(rect01), ri);
          update(ri, rect01, simLevel, false, frameId);
        }
        TRectD rect02 = TRectD(convert(rect.getP01()),
                               TPointD((double)rect.x1, (double)dim.ly));
        if (rect02.getLx() > 0 && rect02.getLy() > 0) {
          rect02 = TRasterImageUtils::convertRasterToWorld(convert(rect02), ri);
          update(ri, rect02, simLevel, false, frameId);
        }
        TRectD rect03 =
            TRectD(TPointD((double)rect.x0, 0.), convert(rect.getP10()));
        if (rect03.getLx() > 0 && rect03.getLy() > 0) {
          rect03 = TRasterImageUtils::convertRasterToWorld(convert(rect03), ri);
          update(ri, rect03, simLevel, false, frameId);
        }
        TRectD rect04 = TRectD(TPointD((double)rect.x1, 0.),
                               TPointD((double)dim.lx, (double)dim.ly));
        if (rect04.getLx() > 0 && rect04.getLy() > 0) {
          rect04 = TRasterImageUtils::convertRasterToWorld(convert(rect04), ri);
          update(ri, rect04, simLevel, false, frameId);
        }
        TUndoManager::manager()->endBlock();
        invalidate();
      } else {
        update(ri, m_selectingRect, simLevel, false, frameId);
        invalidate(m_selectingRect.enlarge(2));
      }
      m_selectingRect.empty();
      TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
      m_selecting = false;
      notifyImageChanged();
    }
  } else if (m_eraseType.getValue() == FREEHANDERASE) {
    if (m_track.isEmpty()) return;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
    m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
    m_track.filterPoints();
    double error    = (30.0 / 11) * sqrt(pixelSize2);
    TStroke *stroke = m_track.makeStroke(error);

    stroke->setStyle(1);
    m_track.clear();

    TTool::Application *app = TTool::getApplication();
    if (m_multi.getValue())  // stroke multi
    {
      if (m_firstFrameSelected) {
        TFrameId tmp = getCurrentFid();
        if (m_firstStroke && stroke)
          multiAreaEraser(m_firstFrameId, tmp, m_firstStroke, stroke);
        notifyImageChanged();
        if (e.isShiftPressed()) {
          TRectD invalidateRect = m_firstStroke->getBBox();
          delete m_firstStroke;
          m_firstStroke = 0;
          invalidate(invalidateRect.enlarge(2));
          m_firstStroke  = stroke;
          invalidateRect = m_firstStroke->getBBox();
          invalidate(invalidateRect.enlarge(2));
          m_firstFrameId = getFrameId();
        } else {
          if (m_isXsheetCell) {
            app->getCurrentColumn()->setColumnIndex(m_currCell.first);
            app->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
          delete stroke;
        }
      } else  // primo frame
      {
        m_firstStroke  = stroke;
        m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
        m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
        invalidate(m_firstStroke->getBBox().enlarge(2));
      }
    } else  // stroke non multi
    {
      if (!getImage(true)) return;
      TFrameId frameId = getCurrentFid();
      eraseStroke(ri, stroke, m_eraseType.getValue(), m_invertOption.getValue(),
                  /*m_multi.getValue(),*/ m_level, frameId);
      notifyImageChanged();
      if (m_invertOption.getValue())
        invalidate();
      else
        invalidate(stroke->getBBox().enlarge(2));
    }
  }
  m_mousePressed = false;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDoubleClick(const TPointD &pos,
                                                const TMouseEvent &e) {
  TRasterImageP ri(getImage(true));
  if (!ri) return;
  TStroke *stroke;
  TTool::Application *app = TTool::getApplication();
  if (m_polyline.size() <= 1) {
    resetMulti();
    return;
  }
  if (m_polyline.back() != pos) m_polyline.push_back(pos);
  if (m_polyline.back() != m_polyline.front())
    m_polyline.push_back(m_polyline.front());
  std::vector<TThickPoint> strokePoints;
  for (UINT i = 0; i < m_polyline.size() - 1; i++) {
    strokePoints.push_back(TThickPoint(m_polyline[i], 1));
    strokePoints.push_back(
        TThickPoint(0.5 * (m_polyline[i] + m_polyline[i + 1]), 1));
  }
  strokePoints.push_back(TThickPoint(m_polyline.back(), 1));
  m_polyline.clear();
  stroke = new TStroke(strokePoints);
  assert(stroke->getPoint(0) == stroke->getPoint(1));

  if (m_multi.getValue())  // stroke multi
  {
    if (m_firstFrameSelected) {
      TFrameId tmp = getFrameId();
      if (m_firstStroke && stroke)
        multiAreaEraser(m_firstFrameId, tmp, m_firstStroke, stroke);
      if (e.isShiftPressed()) {
        TRectD invalidateRect = m_firstStroke->getBBox();
        delete m_firstStroke;
        m_firstStroke = 0;
        invalidate(invalidateRect.enlarge(2));
        m_firstStroke  = stroke;
        invalidateRect = m_firstStroke->getBBox();
        invalidate(invalidateRect.enlarge(2));
        m_firstFrameId = getFrameId();
      } else {
        if (m_isXsheetCell) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
        resetMulti();
        delete stroke;
      }
    } else  // primo frame
    {
      m_firstStroke  = stroke;
      m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
      m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
      invalidate(m_firstStroke->getBBox().enlarge(2));
    }
  } else {
    if (!getImage(true)) return;
    TXshLevel *level          = app->getCurrentLevel()->getLevel();
    TXshSimpleLevelP simLevel = level->getSimpleLevel();
    TFrameId frameId          = getFrameId();
    eraseStroke(ri, stroke, m_eraseType.getValue(), m_invertOption.getValue(),
                /*m_multi.getValue(),*/ m_level, frameId);
    notifyImageChanged();
    if (m_invertOption.getValue())
      invalidate();
    else
      invalidate(stroke->getBBox().enlarge(2));
  }
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  struct Locals {
    FullColorEraserTool *m_this;

    void setValue(TIntProperty &prop, int value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addValue(TIntProperty &prop, double add) {
      const TIntProperty::Range &range = prop.getRange();
      setValue(prop,
               tcrop<double>(prop.getValue() + add, range.first, range.second));
    }

  } locals = {this};

  switch (e.getModifiersMask()) {
  case TMouseEvent::ALT_KEY: {
    // User wants to alter the maximum brush size
    const TPointD &diff = pos - m_mousePos;
    double add          = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

    locals.addValue(m_size, add);
    break;
  }

  default:
    m_brushPos = pos;
    break;
  }

  m_mousePos = pos;
  invalidate();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::draw() {
  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = sqrt(pixelSize2) / 2.0;
  TRasterImageP img = (TRasterImageP)getImage(false);
  if (!img) return;
  if (m_eraseType.getValue() == NORMALERASE) {
    glColor3d(1.0, 0.0, 0.0);
    tglDrawCircle(m_brushPos, (m_size.getValue() + 1) * 0.5);
  } else if (m_eraseType.getValue() == RECTERASE) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    if (m_multi.getValue() && m_firstFrameSelected)
      drawRect(m_firstRect, color, 0x3F33, true);

    if (m_selecting || (m_multi.getValue() && !m_firstFrameSelected))
      drawRect(m_selectingRect, color, 0x3F33, true);
  }
  if ((m_eraseType.getValue() == FREEHANDERASE ||
       m_eraseType.getValue() == POLYLINEERASE) &&
      m_multi.getValue()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    if (m_firstStroke) drawStrokeCenterline(*m_firstStroke, 1);
  }
  if (m_eraseType.getValue() == POLYLINEERASE && !m_polyline.empty()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    tglDrawCircle(m_polyline[0], 2 * m_thick);
    glBegin(GL_LINE_STRIP);
    for (UINT i = 0; i < m_polyline.size(); i++) tglVertex(m_polyline[i]);
    tglVertex(m_mousePos);
    glEnd();
  } else if (m_eraseType.getValue() == FREEHANDERASE && !m_track.isEmpty()) {
    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
                       ? TPixel32::White
                       : TPixel32::Black;
    tglColor(color);
    m_track.drawAllFragments();
  }
}

//----------------------------------------------------------------------------------------------------------

bool FullColorEraserTool::onPropertyChanged(std::string propertyName) {
  FullcolorEraseSize     = m_size.getValue();
  FullcolorEraseHardness = m_hardness.getValue();
  FullcolorEraserOpacity = m_opacity.getValue();
  FullcolorEraserType    = ::to_string(m_eraseType.getValue());
  FullcolorEraserInvert  = (int)m_invertOption.getValue();
  FullcolorEraserRange   = (int)m_multi.getValue();
  if (propertyName == "Hardness:" || propertyName == "Size:") {
    m_brushPad = getBrushPad(m_size.getValue(), m_hardness.getValue() * 0.01);
    TRectD rect(
        m_brushPos - TPointD(FullcolorEraseSize + 2, FullcolorEraseSize + 2),
        m_brushPos + TPointD(FullcolorEraseSize + 2, FullcolorEraseSize + 2));
    invalidate(rect);
  }

  return true;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::update(const TRasterImageP &ri, TRectD selArea,
                                 const TXshSimpleLevelP &level, bool multi,
                                 const TFrameId &frameId) {
  if (m_selectingRect.x0 > m_selectingRect.x1) {
    selArea.x1 = m_selectingRect.x0;
    selArea.x0 = m_selectingRect.x1;
  }
  if (m_selectingRect.y0 > m_selectingRect.y1) {
    selArea.y1 = m_selectingRect.y0;
    selArea.y0 = m_selectingRect.y1;
  }
  if (selArea.getLx() < 1 || selArea.getLy() < 1) return;

  TRasterP raster            = ri->getRaster();
  TTileSetFullColor *tileSet = new TTileSetFullColor(raster->getSize());
  tileSet->add(raster, TRasterImageUtils::convertWorldToRaster(selArea, ri));
  TUndo *undo;

  undo = new RectFullColorUndo(tileSet, selArea, TStroke(),
                               m_eraseType.getValue(), level.getPointer(),
                               m_invertOption.getValue(), frameId);
  TRect rect = TRasterImageUtils::eraseRect(ri, selArea);

  TUndoManager::manager()->add(undo);
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::resetMulti() {
  m_isXsheetCell       = false;
  m_firstFrameSelected = false;
  m_firstRect.empty();
  m_selectingRect.empty();
  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
  m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
  if (m_firstStroke) {
    delete m_firstStroke;
    m_firstStroke = 0;
  }
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::onImageChanged() {
  if (!m_multi.getValue()) return;
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *xshl   = 0;
  if (app->getCurrentLevel()->getLevel())
    xshl = app->getCurrentLevel()->getSimpleLevel();

  if (!xshl || m_level.getPointer() != xshl ||
      (m_selectingRect.isEmpty() && !m_firstStroke))
    resetMulti();
  else if (m_firstFrameId == getCurrentFid())
    m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e torno
                                   // all'immagine iniziale, torno allo stato
                                   // iniziale
  else {                           // cambio stato.
    m_firstFrameSelected = true;
    if (m_eraseType.getValue() != FREEHANDERASE &&
        m_eraseType.getValue() != POLYLINEERASE) {
      assert(!m_selectingRect.isEmpty());
      m_firstRect = m_selectingRect;
    }
  }
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiUpdate(const TRectD firstRect,
                                      const TRectD lastRect) {
  bool backward     = false;
  TFrameId firstFid = m_firstFrameId;
  TFrameId lastFid  = getCurrentFid();
  if (firstFid > lastFid) {
    tswap(firstFid, lastFid);
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

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TRasterImageP ri = m_level->getFrame(fid, true);
    assert(ri);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    TRectD rect = interpolateRect(firstRect, lastRect, backward ? 1 - t : t);
    if (m_invertOption.getValue()) {
      TRectD rect01 =
          TRectD(TPointD(-100000., -100000.), TPointD(rect.x0, 100000.));
      update(ri, rect01, m_level, true, fid);
      TRectD rect02 = TRectD(rect.getP01(), TPointD(rect.x1, 100000.));
      update(ri, rect02, m_level, true, fid);
      TRectD rect03 = TRectD(TPointD(rect.x0, -100000.), rect.getP10());
      update(ri, rect03, m_level, true, fid);
      TRectD rect04 =
          TRectD(TPointD(rect.x1, -100000.), TPointD(100000., 100000.));
      update(ri, rect04, m_level, true, fid);
    } else
      update(ri, rect, m_level, true, fid);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::onEnter() {
  TRasterImageP ti(getImage(false));
  if (!ti) return;

  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiAreaEraser(TFrameId &firstFid, TFrameId &lastFid,
                                          TStroke *firstStroke,
                                          TStroke *lastStroke) {
  TStroke *first           = new TStroke();
  TStroke *last            = new TStroke();
  *first                   = *firstStroke;
  *last                    = *lastStroke;
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();
  firstImage->addStroke(first);
  lastImage->addStroke(last);

  bool backward = false;
  if (firstFid > lastFid) {
    tswap(firstFid, lastFid);
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
  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TImageP img = m_level->getFrame(fid, true);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    doMultiEraser(img, backward ? 1 - t : t, fid, firstImage, lastImage);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::doMultiEraser(const TImageP &img, double t,
                                        const TFrameId &fid,
                                        const TVectorImageP &firstImage,
                                        const TVectorImageP &lastImage) {
  int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
  if (t == 0)
    eraseStroke(img, firstImage->getStroke(0), m_eraseType.getValue(),
                m_invertOption.getValue(), /*m_multi.getValue(),*/ m_level,
                fid);
  else if (t == 1)
    eraseStroke(img, lastImage->getStroke(0), m_eraseType.getValue(),
                m_invertOption.getValue(), /*m_multi.getValue(),*/ m_level,
                fid);
  else {
    assert(firstImage->getStrokeCount() == 1);
    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
    assert(vi->getStrokeCount() == 1);
    eraseStroke(img, vi->getStroke(0), m_eraseType.getValue(),
                m_invertOption.getValue(), /*m_multi.getValue(),*/ m_level,
                fid);
  }
}
