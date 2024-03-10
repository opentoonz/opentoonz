

#include "toonzrasterbrushtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tooloptions.h"
#include "tools/replicator.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/ttileset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzimageutils.h"
#include "toonz/palettecontroller.h"
#include "toonz/stage2.h"
#include "toonz/preferences.h"
#include "toonz/tpalettehandle.h"
#include "toonz/mypaintbrushstyle.h"

// TnzCore includes
#include "tstream.h"
#include "tcolorstyles.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "tregion.h"
#include "tinbetween.h"

#include "tgl.h"
#include "trop.h"

// Qt includes
#include <QPainter>

using namespace ToolUtils;

TEnv::DoubleVar RasterBrushMinSize("InknpaintRasterBrushMinSize", 1);
TEnv::DoubleVar RasterBrushMaxSize("InknpaintRasterBrushMaxSize", 5);
TEnv::DoubleVar BrushSmooth("InknpaintBrushSmooth", 0);
TEnv::IntVar BrushDrawOrder("InknpaintBrushDrawOrder", 0);
TEnv::IntVar RasterBrushPencilMode("InknpaintRasterBrushPencilMode", 0);
TEnv::IntVar BrushPressureSensitivity("InknpaintBrushPressureSensitivity", 1);
TEnv::DoubleVar RasterBrushHardness("RasterBrushHardness", 100);
TEnv::DoubleVar RasterBrushModifierSize("RasterBrushModifierSize", 0);
TEnv::StringVar RasterBrushPreset("RasterBrushPreset", "<custom>");
TEnv::IntVar BrushLockAlpha("InknpaintBrushLockAlpha", 0);
TEnv::IntVar RasterBrushAssistants("RasterBrushAssistants", 1);

//-------------------------------------------------------------------
#define CUSTOM_WSTR L"<custom>"
//-------------------------------------------------------------------
//
// (Da mettere in libreria) : funzioni che spezzano una stroke
// nei suoi punti angolosi. Lo facciamo specialmente per limitare
// i problemi di fill.
//
//-------------------------------------------------------------------

//
// Split a stroke in n+1 parts, according to n parameter values
// Input:
//      stroke            = stroke to split
//      parameterValues[] = vector of parameters where I want to split the
//      stroke
//                          assert: 0<a[0]<a[1]<...<a[n-1]<1
// Output:
//      strokes[]         = the split strokes
//
// note: stroke is unchanged
//

static void split(TStroke *stroke, const std::vector<double> &parameterValues,
                  std::vector<TStroke *> &strokes) {
  TThickPoint p2;
  std::vector<TThickPoint> points;
  TThickPoint lastPoint = stroke->getControlPoint(0);
  int n                 = parameterValues.size();
  int chunk;
  double t;
  int last_chunk = -1, startPoint = 0;
  double lastLocT = 0;

  for (int i = 0; i < n; i++) {
    points.push_back(lastPoint);  // Add first point of the stroke
    double w =
        parameterValues[i];  // Global parameter. along the stroke 0<=w<=1
    stroke->getChunkAndT(w, chunk,
                         t);  // t: local parameter in the chunk-th quadratic

    if (i == 0)
      startPoint = 1;
    else {
      int indexAfterLastT =
          stroke->getControlPointIndexAfterParameter(parameterValues[i - 1]);
      startPoint = indexAfterLastT;
      if ((indexAfterLastT & 1) && lastLocT != 1) startPoint++;
    }
    int endPoint = 2 * chunk + 1;
    if (lastLocT != 1 && i > 0) {
      if (last_chunk != chunk || t == 1)
        points.push_back(p2);  // If the last local t is not an extreme
                               // add the point p2
    }

    for (int j = startPoint; j < endPoint; j++)
      points.push_back(stroke->getControlPoint(j));

    TThickPoint p, A, B, C;
    p       = stroke->getPoint(w);
    C       = stroke->getControlPoint(2 * chunk + 2);
    B       = stroke->getControlPoint(2 * chunk + 1);
    A       = stroke->getControlPoint(2 * chunk);
    p.thick = A.thick;

    if (last_chunk != chunk) {
      TThickPoint p1 = (1 - t) * A + t * B;
      points.push_back(p1);
      p.thick = p1.thick;
    } else {
      if (t != 1) {
        // If the i-th cut point belong to the same chunk of the (i-1)-th cut
        // point.
        double tInters  = lastLocT / t;
        TThickPoint p11 = (1 - t) * A + t * B;
        TThickPoint p1  = (1 - tInters) * p11 + tInters * p;
        points.push_back(p1);
        p.thick = p1.thick;
      }
    }

    points.push_back(p);

    if (t != 1) p2 = (1 - t) * B + t * C;

    assert(points.size() & 1);

    // Add new stroke
    TStroke *strokeAdd = new TStroke(points);
    strokeAdd->setStyle(stroke->getStyle());
    strokeAdd->outlineOptions() = stroke->outlineOptions();
    strokes.push_back(strokeAdd);

    lastPoint  = p;
    last_chunk = chunk;
    lastLocT   = t;
    points.clear();
  }
  // Add end stroke
  points.push_back(lastPoint);

  if (lastLocT != 1) points.push_back(p2);

  startPoint =
      stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]);
  if ((stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]) &
       1) &&
      lastLocT != 1)
    startPoint++;
  for (int j = startPoint; j < stroke->getControlPointCount(); j++)
    points.push_back(stroke->getControlPoint(j));

  assert(points.size() & 1);
  TStroke *strokeAdd = new TStroke(points);
  strokeAdd->setStyle(stroke->getStyle());
  strokeAdd->outlineOptions() = stroke->outlineOptions();
  strokes.push_back(strokeAdd);
  points.clear();
}

// Compute Parametric Curve Curvature
// By Formula:
// k(t)=(|p'(t) x p''(t)|)/Norm2(p')^3
// p(t) is parametric curve
// Input:
//      dp  = First Derivate.
//      ddp = Second Derivate
// Output:
//      return curvature value.
//      Note: if the curve is a single point (that's dp=0) or it is a straight
//      line (that's ddp=0) return 0

static double curvature(TPointD dp, TPointD ddp) {
  if (dp == TPointD(0, 0))
    return 0;
  else
    return fabs(cross(dp, ddp) / pow(norm2(dp), 1.5));
}

// Find the max curvature points of a stroke.
// Input:
//      stroke.
//      angoloLim =  Value (radians) of the Corner between two tangent vector.
//                   Up this value the two corner can be considered angular.
//      curvMaxLim = Value of the max curvature.
//                   Up this value the point can be considered a max curvature
//                   point.
// Output:
//      parameterValues = vector of max curvature parameter points

static void findMaxCurvPoints(TStroke *stroke, const float &angoloLim,
                              const float &curvMaxLim,
                              std::vector<double> &parameterValues) {
  TPointD tg1, tg2;  // Tangent vectors

  TPointD dp, ddp;  // First and Second derivate.

  parameterValues.clear();
  int cpn = stroke ? stroke->getControlPointCount() : 0;
  for (int j = 2; j < cpn; j += 2) {
    TPointD p0 = stroke->getControlPoint(j - 2);
    TPointD p1 = stroke->getControlPoint(j - 1);
    TPointD p2 = stroke->getControlPoint(j);

    TPointD q = p1 - (p0 + p2) * 0.5;

    // Search corner point
    if (j > 2) {
      tg2 = -p0 + p2 + 2 * q;  // Tangent vector to this chunk at t=0
      double prod_scal =
          tg2 * tg1;  // Inner product between tangent vectors at t=0.
      assert(tg1 != TPointD(0, 0) || tg2 != TPointD(0, 0));
      // Compute corner between two tangent vectors
      double angolo =
          acos(prod_scal / (pow(norm2(tg2), 0.5) * pow(norm2(tg1), 0.5)));

      // Add corner point
      if (angolo > angoloLim) {
        double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)),
                                     0);  //  transform lacal t to global t
        parameterValues.push_back(w);
      }
    }
    tg1 = -p0 + p2 - 2 * q;  // Tangent vector to this chunk at t=1

    // End search corner point

    // Search max curvature point
    // Value of t where the curvature function has got an extreme.
    // (Point where first derivate is null)
    double estremo_int = 0;
    double t           = -1;
    if (q != TPointD(0, 0)) {
      t = 0.25 *
          (2 * q.x * q.x + 2 * q.y * q.y - q.x * p0.x + q.x * p2.x -
           q.y * p0.y + q.y * p2.y) /
          (q.x * q.x + q.y * q.y);

      dp  = -p0 + p2 + 2 * q - 4 * t * q;  // First derivate of the curve
      ddp = -4 * q;                        // Second derivate of the curve
      estremo_int = curvature(dp, ddp);

      double h    = 0.01;
      dp          = -p0 + p2 + 2 * q - 4 * (t + h) * q;
      double c_dx = curvature(dp, ddp);
      dp          = -p0 + p2 + 2 * q - 4 * (t - h) * q;
      double c_sx = curvature(dp, ddp);
      // Check the point is a max and not a minimum
      if (estremo_int < c_dx && estremo_int < c_sx) {
        estremo_int = 0;
      }
    }
    double curv_max = estremo_int;

    // Compute curvature at the extreme of interval [0,1]
    // Compute curvature at t=0 (Left extreme)
    dp                = -p0 + p2 + 2 * q;
    double estremo_sx = curvature(dp, ddp);

    // Compute curvature at t=1 (Right extreme)
    dp                = -p0 + p2 - 2 * q;
    double estremo_dx = curvature(dp, ddp);

    // Compare curvature at the extreme of interval [0,1] with the internal
    // value
    double t_ext;
    if (estremo_sx >= estremo_dx)
      t_ext = 0;
    else
      t_ext = 1;
    double maxEstremi = std::max(estremo_dx, estremo_sx);
    if (maxEstremi > estremo_int) {
      t        = t_ext;
      curv_max = maxEstremi;
    }

    // Add max curvature point
    if (t >= 0 && t <= 1 && curv_max > curvMaxLim) {
      double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)),
                                   t);  // transform local t to global t
      parameterValues.push_back(w);
    }
    // End search max curvature point
  }
  // Delete duplicate of parameterValues
  // Because some max cuvature point can coincide with the corner point
  if ((int)parameterValues.size() > 1) {
    std::sort(parameterValues.begin(), parameterValues.end());
    parameterValues.erase(
        std::unique(parameterValues.begin(), parameterValues.end()),
        parameterValues.end());
  }
}

static void addStroke(TTool::Application *application, const TVectorImageP &vi,
                      TStroke *stroke, bool breakAngles, bool frameCreated,
                      bool levelCreated, TXshSimpleLevel *sLevel = NULL,
                      TFrameId fid = TFrameId::NO_FRAME) {
  QMutexLocker lock(vi->getMutex());

  if (application->getCurrentObject()->isSpline()) {
    application->getCurrentXsheet()->notifyXsheetChanged();
    return;
  }

  std::vector<double> corners;
  std::vector<TStroke *> strokes;

  const float angoloLim =
      1;  // Value (radians) of the Corner between two tangent vector.
          // Up this value the two corner can be considered angular.
  const float curvMaxLim = 0.8;  // Value of the max curvature.
  // Up this value the point can be considered a max curvature point.

  findMaxCurvPoints(stroke, angoloLim, curvMaxLim, corners);
  TXshSimpleLevel *sl;
  if (!sLevel) {
    sl = application->getCurrentLevel()->getSimpleLevel();
  } else {
    sl = sLevel;
  }
  TFrameId id = application->getCurrentTool()->getTool()->getCurrentFid();
  if (id == TFrameId::NO_FRAME && fid != TFrameId::NO_FRAME) id = fid;
  if (!corners.empty()) {
    if (breakAngles)
      split(stroke, corners, strokes);
    else
      strokes.push_back(new TStroke(*stroke));

    int n = strokes.size();

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < n; i++) {
      std::vector<TFilledRegionInf> *fillInformation =
          new std::vector<TFilledRegionInf>;
      ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                       stroke->getBBox());
      TStroke *str = new TStroke(*strokes[i]);
      vi->addStroke(str);
      TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id,
                                                  frameCreated, levelCreated));
    }
    TUndoManager::manager()->endBlock();
  } else {
    std::vector<TFilledRegionInf> *fillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                     stroke->getBBox());
    TStroke *str = new TStroke(*stroke);
    vi->addStroke(str);
    TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id,
                                                frameCreated, levelCreated));
  }

  // Update regions. It will call roundStroke() in
  // TVectorImage::Imp::findIntersections().
  // roundStroke() will slightly modify all the stroke positions.
  // It is needed to update information for Fill Check.
  vi->findRegions();

  for (int k = 0; k < (int)strokes.size(); k++) delete strokes[k];
  strokes.clear();

  application->getCurrentTool()->getTool()->notifyImageChanged();
}

//-------------------------------------------------------------------
//
// Gennaro: end
//
//-------------------------------------------------------------------

//===================================================================
//
// Helper functions and classes
//
//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

enum DrawOrder { OverAll = 0, UnderAll, PaletteOrder };

void getAboveStyleIdSet(int styleId, TPaletteP palette,
                        QSet<int> &aboveStyles) {
  if (!palette) return;
  for (int p = 0; p < palette->getPageCount(); p++) {
    TPalette::Page *page = palette->getPage(p);
    for (int s = 0; s < page->getStyleCount(); s++) {
      int tmpId = page->getStyleId(s);
      if (tmpId == styleId) return;
      if (tmpId != 0) aboveStyles.insert(tmpId);
    }
  }
}

//=========================================================================================================

class RasterBrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  bool m_isPaletteOrder;
  bool m_isPencil;
  bool m_isStraight;
  bool m_modifierLockAlpha;

public:
  RasterBrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
                  int styleId, bool selective, TXshSimpleLevel *level,
                  const TFrameId &frameId, bool isPencil, bool isFrameCreated,
                  bool isLevelCreated, bool isPaletteOrder, bool lockAlpha,
                  bool isStraight = false)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_isPaletteOrder(isPaletteOrder)
      , m_isPencil(isPencil)
      , m_isStraight(isStraight)
      , m_modifierLockAlpha(lockAlpha) {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP image = getImage();
    TRasterCM32P ras   = image->getRaster();
    RasterStrokeGenerator rasterTrack(
        ras, BRUSH, NONE, m_styleId, m_points[0], m_selective, 0,
        m_modifierLockAlpha, !m_isPencil, m_isPaletteOrder);
    if (m_isPaletteOrder) {
      QSet<int> aboveStyleIds;
      getAboveStyleIdSet(m_styleId, image->getPalette(), aboveStyleIds);
      rasterTrack.setAboveStyleIds(aboveStyleIds);
    }
    rasterTrack.setPointsSequence(m_points);
    rasterTrack.generateStroke(m_isPencil, m_isStraight);
    image->setSavebox(image->getSavebox() +
                      rasterTrack.getBBox(rasterTrack.getPointsSequence()));
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }
  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

class RasterBluredBrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  DrawOrder m_drawOrder;
  int m_maxThick;
  double m_hardness;
  bool m_isStraight;
  bool m_modifierLockAlpha;

public:
  RasterBluredBrushUndo(TTileSetCM32 *tileSet,
                        const std::vector<TThickPoint> &points, int styleId,
                        DrawOrder drawOrder, bool lockAlpha,
                        TXshSimpleLevel *level, const TFrameId &frameId,
                        int maxThick, double hardness, bool isFrameCreated,
                        bool isLevelCreated, bool isStraight = false)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_drawOrder(drawOrder)
      , m_maxThick(maxThick)
      , m_hardness(hardness)
      , m_isStraight(isStraight)
      , m_modifierLockAlpha(lockAlpha) {}

  void redo() const override {
    if (m_points.size() == 0) return;
    insertLevelAndFrameIfNeeded();
    TToonzImageP image     = getImage();
    TRasterCM32P ras       = image->getRaster();
    TRasterCM32P backupRas = ras->clone();
    TRaster32P workRaster(ras->getSize());
    QRadialGradient brushPad = ToolUtils::getBrushPad(m_maxThick, m_hardness);
    workRaster->clear();
    BluredBrush brush(workRaster, m_maxThick, brushPad, false);

    if (m_drawOrder == PaletteOrder) {
      QSet<int> aboveStyleIds;
      getAboveStyleIdSet(m_styleId, image->getPalette(), aboveStyleIds);
      brush.setAboveStyleIds(aboveStyleIds);
    }

    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.updateDrawing(ras, ras, bbox, m_styleId, (int)m_drawOrder,
                        m_modifierLockAlpha);
    if (m_isStraight) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      points.push_back(m_points[2]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], m_points[1], m_points[2], 1, 1);
      brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                          m_modifierLockAlpha);
    } else if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1],
                   1, 1);
      brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                          m_modifierLockAlpha);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                            m_modifierLockAlpha);
      }
    }
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

class MyPaintBrushUndo final : public TRasterUndo {
  TPoint m_offset;
  QString m_id;

public:
  MyPaintBrushUndo(TTileSetCM32 *tileSet, TXshSimpleLevel *level,
                   const TFrameId &frameId, bool isFrameCreated,
                   bool isLevelCreated, const TRasterCM32P &ras,
                   const TPoint &offset)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_offset(offset) {
    static int counter = 0;
    m_id = QString("MyPaintBrushUndo") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(),
                                 TToonzImageP(ras, TRect(ras->getSize())));
  }

  ~MyPaintBrushUndo() { TImageCache::instance()->remove(m_id); }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TToonzImageP image = getImage();
    TRasterCM32P ras   = image->getRaster();

    TImageP srcImg =
        TImageCache::instance()->get(m_id.toStdString(), false)->cloneImage();
    TToonzImageP tSrcImg = srcImg;
    assert(tSrcImg);
    ras->copy(tSrcImg->getRaster(), m_offset);
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

double computeThickness(double pressure, const TDoublePairProperty &property) {
  double t      = pressure * pressure * pressure;
  double thick0 = property.getValue().first;
  double thick1 = property.getValue().second;

  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t) * 0.5;
}

}  // namespace

//--------------------------------------------------------------------------------------------------

static void CatmullRomInterpolate(const TThickPoint &P0, const TThickPoint &P1,
                                  const TThickPoint &P2, const TThickPoint &P3,
                                  int samples,
                                  std::vector<TThickPoint> &points) {
  double x0 = P1.x;
  double x1 = (-P0.x + P2.x) * 0.5f;
  double x2 = P0.x - 2.5f * P1.x + 2.0f * P2.x - 0.5f * P3.x;
  double x3 = -0.5f * P0.x + 1.5f * P1.x - 1.5f * P2.x + 0.5f * P3.x;

  double y0 = P1.y;
  double y1 = (-P0.y + P2.y) * 0.5f;
  double y2 = P0.y - 2.5f * P1.y + 2.0f * P2.y - 0.5f * P3.y;
  double y3 = -0.5f * P0.y + 1.5f * P1.y - 1.5f * P2.y + 0.5f * P3.y;

  double z0 = P1.thick;
  double z1 = (-P0.thick + P2.thick) * 0.5f;
  double z2 = P0.thick - 2.5f * P1.thick + 2.0f * P2.thick - 0.5f * P3.thick;
  double z3 =
      -0.5f * P0.thick + 1.5f * P1.thick - 1.5f * P2.thick + 0.5f * P3.thick;

  for (int i = 1; i <= samples; ++i) {
    double t  = i / (double)(samples + 1);
    double t2 = t * t;
    double t3 = t2 * t;
    TThickPoint p;
    p.x     = x0 + x1 * t + x2 * t2 + x3 * t3;
    p.y     = y0 + y1 * t + y2 * t2 + y3 * t3;
    p.thick = z0 + z1 * t + z2 * t2 + z3 * t3;
    points.push_back(p);
  }
}

//--------------------------------------------------------------------------------------------------

static void Smooth(std::vector<TThickPoint> &points, const int radius,
                   const int readIndex, const int level) {
  int n = (int)points.size();
  if (radius < 1 || n < 3) {
    return;
  }

  std::vector<TThickPoint> result;

  float d = 1.0f / (radius * 2 + 1);

  int endSamples = 10;
  int startId    = std::max(readIndex - endSamples * 3 - radius * level, 1);

  for (int i = startId; i < n - 1; ++i) {
    int lower = i - radius;
    int upper = i + radius;

    TThickPoint total;
    total.x     = 0;
    total.y     = 0;
    total.thick = 0;

    for (int j = lower; j <= upper; ++j) {
      int idx = j;
      if (idx < 0) {
        idx = 0;
      } else if (idx >= n) {
        idx = n - 1;
      }
      total.x += points[idx].x;
      total.y += points[idx].y;
      total.thick += points[idx].thick;
    }

    total.x *= d;
    total.y *= d;
    total.thick *= d;
    result.push_back(total);
  }

  auto result_itr = result.begin();
  for (int i = startId; i < n - 1; ++i, ++result_itr) {
    points[i].x     = (*result_itr).x;
    points[i].y     = (*result_itr).y;
    points[i].thick = (*result_itr).thick;
  }

  if (points.size() >= 3) {
    std::vector<TThickPoint> pts;
    CatmullRomInterpolate(points[0], points[0], points[1], points[2],
                          endSamples, pts);
    std::vector<TThickPoint>::iterator it = points.begin() + 1;
    points.insert(it, pts.begin(), pts.end());

    pts.clear();
    CatmullRomInterpolate(points[n - 3], points[n - 2], points[n - 1],
                          points[n - 1], endSamples, pts);
    it = points.begin();
    it += n - 1;
    points.insert(it, pts.begin(), pts.end());
  }
}


//===================================================================
//
// ToonzRasterBrushTool
//
//-----------------------------------------------------------------------------

ToonzRasterBrushTool::ToonzRasterBrushTool(std::string name, int targetType)
    : TTool(name)
    , m_rasThickness("Size", 1, 1000, 1, 5)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_preset("Preset:")
    , m_drawOrder("Draw Order:")
    , m_pencil("Pencil", false)
    , m_pressure("Pressure", true)
    , m_modifierSize("ModifierSize", -3, 3, 0, true)
    , m_modifierLockAlpha("Lock Alpha", false)
    , m_assistants("Assistants", true)
    , m_targetType(targetType)
    , m_enabled(false)
    , m_isPrompting(false)
    , m_firstTime(true)
    , m_presetsLoaded(false)
    , m_notifier(0)
{
  bind(targetType);

  m_rasThickness.setNonLinearSlider();

  m_prop[0].bind(m_rasThickness);
  m_prop[0].bind(m_hardness);
  m_prop[0].bind(m_smooth);
  m_prop[0].bind(m_drawOrder);
  m_prop[0].bind(m_modifierSize);
  m_prop[0].bind(m_modifierLockAlpha);
  m_prop[0].bind(m_pencil);
  m_prop[0].bind(m_assistants);
  m_pencil.setId("PencilMode");

  m_drawOrder.addValue(L"Over All");
  m_drawOrder.addValue(L"Under All");
  m_drawOrder.addValue(L"Palette Order");
  m_drawOrder.setId("DrawOrder");

  m_prop[0].bind(m_pressure);

  m_prop[0].bind(m_preset);
  m_preset.setId("BrushPreset");
  m_preset.addValue(CUSTOM_WSTR);
  m_pressure.setId("PressureSensitivity");
  m_modifierLockAlpha.setId("LockAlpha");

  m_inputmanager.setHandler(this);
  m_modifierLine               = new TModifierLine();
  m_modifierTangents           = new TModifierTangents();
  m_modifierAssistants         = new TModifierAssistants();
  m_modifierSegmentation       = new TModifierSegmentation();
  m_modifierSmoothSegmentation = new TModifierSegmentation(TPointD(1, 1), 3);
  for(int i = 0; i < 3; ++i)
    m_modifierSmooth[i]        = new TModifierSmooth();
#ifndef NDEBUG
  m_modifierTest = new TModifierTest();
#endif

  m_inputmanager.addModifier(
      TInputModifierP(m_modifierAssistants.getPointer()));
}

//-------------------------------------------------------------------------------------------------------

unsigned int ToonzRasterBrushTool::getToolHints() const {
  unsigned int h = TTool::getToolHints() & ~HintAssistantsAll;
  if (m_assistants.getValue()) {
    h |= HintReplicators;
    h |= HintReplicatorsPoints;
    h |= HintReplicatorsEnabled;
  }
  return h;
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *ToonzRasterBrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//-------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::drawLine(const TPointD &point, const TPointD &centre,
                                    bool horizontal, bool isDecimal) {
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

void ToonzRasterBrushTool::drawEmptyCircle(TPointD pos, int thick,
                                           bool isLxEven, bool isLyEven,
                                           bool isPencil) {
  if (isLxEven) pos.x += 0.5;
  if (isLyEven) pos.y += 0.5;

  if (!isPencil)
    tglDrawCircle(pos, (thick + 1) * 0.5);
  else {
    pos.x = floor(pos.x) + 0.5;
    pos.y = floor(pos.y) + 0.5;
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

//-------------------------------------------------------------------------------------------------------

TPointD ToonzRasterBrushTool::getCenteredCursorPos(
    const TPointD &originalCursorPos) {
  if (m_isMyPaintStyleSelected) return originalCursorPos;
  TXshLevelHandle *levelHandle = m_application->getCurrentLevel();
  TXshSimpleLevel *level = levelHandle ? levelHandle->getSimpleLevel() : 0;
  TDimension resolution =
      level ? level->getProperties()->getImageRes() : TDimension(0, 0);

  bool xEven = (resolution.lx % 2 == 0);
  bool yEven = (resolution.ly % 2 == 0);

  TPointD centeredCursorPos = originalCursorPos;

  if (xEven) centeredCursorPos.x -= 0.5;
  if (yEven) centeredCursorPos.y -= 0.5;

  return centeredCursorPos;
}

//-------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::updateTranslation() {
  m_rasThickness.setQStringName(tr("Size"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_smooth.setQStringName(tr("Smooth:"));
  m_drawOrder.setQStringName(tr("Draw Order:"));
  m_drawOrder.setItemUIName(L"Over All", tr("Over All"));
  m_drawOrder.setItemUIName(L"Under All", tr("Under All"));
  m_drawOrder.setItemUIName(L"Palette Order", tr("Palette Order"));
  m_modifierSize.setQStringName(tr("Size"));

  // m_filled.setQStringName(tr("Filled"));
  m_preset.setQStringName(tr("Preset:"));
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));
  m_pencil.setQStringName(tr("Pencil"));
  m_pressure.setQStringName(tr("Pressure"));
  m_modifierLockAlpha.setQStringName(tr("Lock Alpha"));
  m_assistants.setQStringName(tr("Assistants"));
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onActivate() {
  if (!m_notifier) m_notifier = new ToonzRasterBrushToolNotifier(this);

  if (m_firstTime) {
    m_firstTime = false;

    std::wstring wpreset =
        QString::fromStdString(RasterBrushPreset.getValue()).toStdWString();
    if (wpreset != CUSTOM_WSTR) {
      initPresets();
      if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
      m_preset.setValue(wpreset);
      RasterBrushPreset = m_preset.getValueAsString();
      loadPreset();
    } else
      loadLastBrush();
  }
  m_brushPad = ToolUtils::getBrushPad(m_rasThickness.getValue().second,
                                      m_hardness.getValue() * 0.01);
  setWorkAndBackupImages();

  updateModifiers();
  m_brushTimer.start();
  // TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onDeactivate() {
  m_inputmanager.finishTracks();
  m_enabled   = false;
  m_workRas   = TRaster32P();
  m_backupRas = TRasterCM32P();
}

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::askRead(const TRect &rect) { return askWrite(rect); }

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::askWrite(const TRect &rect) {
  if (rect.isEmpty()) return true;
  m_painting.myPaint.strokeSegmentRect += rect;
  updateWorkAndBackupRasters(rect);
  m_painting.tileSaver->save(rect);
  return true;
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::updateModifiers() {
  int smoothRadius = (int)round(m_smooth.getValue());
  m_modifierAssistants->magnetism = m_assistants.getValue() ? 1 : 0;
  m_inputmanager.drawPreview      = false; //!m_modifierAssistants->drawOnly;

  m_modifierReplicate.clear();
  if (m_assistants.getValue())
    TReplicator::scanReplicators(this, nullptr, &m_modifierReplicate, false, true, false, false, nullptr);
  
  m_inputmanager.clearModifiers();
  m_inputmanager.addModifier(TInputModifierP(m_modifierTangents.getPointer()));
  if (smoothRadius > 0) {
    m_inputmanager.addModifier(TInputModifierP(m_modifierSmoothSegmentation.getPointer()));
    for(int i = 0; i < 3; ++i) {
      m_modifierSmooth[i]->radius = smoothRadius;
      m_inputmanager.addModifier(TInputModifierP(m_modifierSmooth[i].getPointer()));
    }
  }
  m_inputmanager.addModifier(TInputModifierP(m_modifierAssistants.getPointer()));
#ifndef NDEBUG
  m_inputmanager.addModifier(TInputModifierP(m_modifierTest.getPointer()));
#endif
  m_inputmanager.addModifiers(m_modifierReplicate);
  m_inputmanager.addModifier(TInputModifierP(m_modifierSegmentation.getPointer()));
}

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::preLeftButtonDown() {
  updateModifiers();
  touchImage();
  if (m_isFrameCreated) {
    setWorkAndBackupImages();
    // When the xsheet frame is selected, whole viewer will be updated from
    // SceneViewer::onXsheetChanged() on adding a new frame.
    // We need to take care of a case when the level frame is selected.
    if (m_application->getCurrentFrame()->isEditingLevel()) invalidate();
  }
  return true;
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::handleMouseEvent(MouseEventType type,
                                          const TPointD &pos,
                                          const TMouseEvent &e) {
  TTimerTicks t = TToolTimer::ticks();
  bool alt      = e.getModifiersMask() & TMouseEvent::ALT_KEY;
  bool shift    = e.getModifiersMask() & TMouseEvent::SHIFT_KEY;
  bool control  = e.getModifiersMask() & TMouseEvent::CTRL_KEY;

  if (shift && type == ME_DOWN && e.button() == Qt::LeftButton && !m_painting.active) {
    m_modifierAssistants->magnetism = 0;
    m_inputmanager.clearModifiers();
    m_inputmanager.addModifier(TInputModifierP(m_modifierLine.getPointer()));
    m_inputmanager.addModifier(TInputModifierP(m_modifierAssistants.getPointer()));
    m_inputmanager.addModifiers(m_modifierReplicate);
    m_inputmanager.addModifier(TInputModifierP(m_modifierSegmentation.getPointer()));
    m_inputmanager.drawPreview = true;
  }

  if (alt != m_inputmanager.state.isKeyPressed(TKey::alt))
    m_inputmanager.keyEvent(alt, TKey::alt, t, nullptr);
  if (shift != m_inputmanager.state.isKeyPressed(TKey::shift))
    m_inputmanager.keyEvent(shift, TKey::shift, t, nullptr);
  if (control != m_inputmanager.state.isKeyPressed(TKey::control))
    m_inputmanager.keyEvent(control, TKey::control, t, nullptr);

  if (type == ME_MOVE) {
    THoverList hovers(1, pos);
    m_inputmanager.hoverEvent(hovers);
  } else {
    int    deviceId    = e.isTablet() ? 1 : 0;
    double defPressure = m_isMyPaintStyleSelected ? 0.5 : 1.0;
    bool   hasPressure = e.isTablet();
    double pressure    = hasPressure ? e.m_pressure : defPressure;
    bool   final       = type == ME_UP;
    m_inputmanager.trackEvent(
      deviceId, 0, pos, pressure, TPointD(), hasPressure, false, final, t);
    m_inputmanager.processTracks();
  }
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DOWN, pos, e);
}
void ToonzRasterBrushTool::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DRAG, pos, e);
}
void ToonzRasterBrushTool::leftButtonUp(const TPointD &pos,
                                      const TMouseEvent &e) {
  handleMouseEvent(ME_UP, pos, e);
}
void ToonzRasterBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  handleMouseEvent(ME_MOVE, pos, e);
}

//--------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::inputSetBusy(bool busy) {
  if (m_painting.active == busy)
    return;
  
  if (busy) {
    // begin painting
    
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    int col   = app->getCurrentColumn()->getColumnIndex();
    m_enabled = col >= 0 || app->getCurrentFrame()->isEditingLevel();
    // todo: gestire autoenable
    if (!m_enabled) return;

    m_painting.active = !!getImage(true);
    if (!m_painting.active)
      m_painting.active = !!touchImage();
    if (!m_painting.active)
      return;
    
    // nel caso che il colore corrente sia un cleanup/studiopalette color
    // oppure il colore di un colorfield
    updateCurrentStyle();
    if (TColorStyle *cs = app->getCurrentLevelStyle()) {
      m_painting.styleId = app->getCurrentLevelStyleIndex();
      TRasterStyleFx *rfx = cs->getRasterStyleFx();
      if (!cs->isStrokeStyle() && (!rfx || !rfx->isInkStyle()))
          { m_painting.active = false; return; }
    } else {
      m_painting.styleId = 1;
    }
    
    m_painting.frameId = getFrameId();

    TImageP img = getImage(true);
    TToonzImageP ri(img);
    TRasterCM32P ras = ri->getRaster();
    if (!ras)
      { m_painting.active = false; return; }
      
    m_painting.tileSet   = new TTileSetCM32(ras->getSize());
    m_painting.tileSaver = new TTileSaverCM32(ras, m_painting.tileSet);
    m_painting.affectedRect.empty();
    setWorkAndBackupImages();
    
    if (m_isMyPaintStyleSelected) {
      // init myPaint drawing
      
      m_painting.myPaint.isActive = true;
      m_painting.myPaint.strokeSegmentRect.empty();
      
      m_workRas->lock();
      
      // prepare base brush
      if ( TMyPaintBrushStyle *mypaintStyle = dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle()) )
        m_painting.myPaint.baseBrush.fromBrush( mypaintStyle->getBrush() ); else
          m_painting.myPaint.baseBrush.fromDefaults();
      double modifierSize = m_modifierSize.getValue() * log(2.0);
      float baseSize = m_painting.myPaint.baseBrush.getBaseValue( MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC );
      m_painting.myPaint.baseBrush.setBaseValue( MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, baseSize + modifierSize );
    } else
    if (m_hardness.getValue() == 100 || m_pencil.getValue()) {
      // init pencil drawing
      
      m_painting.pencil.isActive = true;
      m_painting.pencil.realPencil = m_pencil.getValue();
    } else {
      // init blured brush drawing (regular drawing)
      
      m_painting.blured.isActive = true;
    }
    
  } else {
    // finish painting
    
    if (m_painting.myPaint.isActive) {
      // finish myPaint drawing
      m_workRas->unlock();
    }

    delete m_painting.tileSaver;
    m_painting.tileSaver = nullptr;

    // add undo record
    TFrameId frameId = m_painting.frameId.isEmptyFrame() ? getCurrentFid() : m_painting.frameId;
    if (m_painting.tileSet->getTileCount() > 0) {
      TTool::Application *app   = TTool::getApplication();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();
      TRasterCM32P ras          = TToonzImageP( getImage(true) )->getRaster();
      TRasterCM32P subras       = ras->extract(m_painting.affectedRect)->clone();
      TUndoManager::manager()->add(new MyPaintBrushUndo(
          m_painting.tileSet, simLevel.getPointer(), frameId, m_isFrameCreated,
          m_isLevelCreated, subras, m_painting.affectedRect.getP00()));
      // MyPaintBrushUndo will delete tileSet by it self
    } else {
      // delete tileSet here because MyPaintBrushUndo will not do it
      delete m_painting.tileSet;
    }
    m_painting.tileSet = nullptr;

    /*-- 作業中のフレームをリセット --*/
    m_painting.frameId = TFrameId();


    m_painting.myPaint.isActive = false;
    m_painting.pencil.isActive = false;
    m_painting.blured.isActive = false;
    m_painting.active = false;
    
    /*-- FIdを指定して、描画中にフレームが動いても、
      描画開始時のFidのサムネイルが更新されるようにする。--*/
    notifyImageChanged(frameId);
    ToolUtils::updateSaveBox();
  }
}

//--------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::inputPaintTrackPoint(const TTrackPoint &point, const TTrack &track, bool firstTrack, bool preview) {
  if (!m_painting.active || preview)
    return;
  
  TImageP img = getImage(true);
  TToonzImageP ri(img);
  TRasterCM32P ras = ri->getRaster();
  if (!ras)
    return;
  TPointD rasCenter = ras->getCenterD();
  TPointD fixedPosition = getCenteredCursorPos(point.position);
  

  TRectD invalidateRect;
  bool firstPoint = track.size() == track.pointsAdded;
  bool lastPoint  = track.pointsAdded == 1 && track.finished();
  
  // first point must be without handler, following points must be with handler
  // other behaviour is possible bug and must be ignored
  assert(firstPoint == !track.handler);
  if (firstPoint != !track.handler)
    return;
  
  double defPressure = m_painting.myPaint.isActive ? 0.5 : 1.0;
  double pressure    = m_pressure.getValue() ? point.pressure : defPressure;
  
  if (m_painting.myPaint.isActive) {
    // mypaint case
    
    // init brush
    MyPaintStroke *handler;
    if (firstPoint) {
      handler = new MyPaintStroke(m_workRas, *this, m_painting.myPaint.baseBrush, false);
      handler->brush.beginStroke();
      track.handler = handler;
    }
    handler = dynamic_cast<MyPaintStroke*>(track.handler.getPointer());
    if (!handler) return;
    
    // paint stroke
    m_painting.myPaint.strokeSegmentRect.empty();
    handler->brush.strokeTo( fixedPosition + rasCenter, pressure,
                             point.tilt, point.time - track.previous().time );
    if (lastPoint)
      handler->brush.endStroke();
    
    // update affected area
    TRect updateRect = m_painting.myPaint.strokeSegmentRect * ras->getBounds();
    m_painting.affectedRect += updateRect;
    if (!updateRect.isEmpty())
      handler->brush.updateDrawing( ras, m_backupRas, m_painting.myPaint.strokeSegmentRect,
                                    m_painting.styleId, m_modifierLockAlpha.getValue() );
    
    // determine invalidate rect
    invalidateRect += convert(m_painting.myPaint.strokeSegmentRect) - rasCenter;
  } else
  if (m_painting.pencil.isActive) {
    // pencil case
    
    // Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる
    double thickness = computeThickness(pressure, m_rasThickness)*2;
    //if (!m_painting.pencil.realPencil && !m_modifierLine->getManager())
    //  thickness -= 1.0;
    TThickPoint thickPoint(fixedPosition + rasCenter, thickness);

    // init brush
    PencilStroke *handler;
    if (firstPoint) {
      DrawOrder drawOrder = (DrawOrder)m_drawOrder.getIndex();
      handler = new PencilStroke( ras, BRUSH, NONE, m_painting.styleId, thickPoint, drawOrder != OverAll, 0,
                                  m_modifierLockAlpha.getValue(), !m_painting.pencil.realPencil,
                                  drawOrder == PaletteOrder );
      
      // if the drawOrder mode = "Palette Order",
      // get styleId list which is above the current style in the palette
      if (drawOrder == PaletteOrder) {
        QSet<int> aboveStyleIds;
        getAboveStyleIdSet(m_painting.styleId, ri->getPalette(), aboveStyleIds);
        handler->brush.setAboveStyleIds(aboveStyleIds);
      }
      track.handler = handler;
    }
    handler = dynamic_cast<PencilStroke*>(track.handler.getPointer());
    if (!handler) return;

    // paint stroke
    if (!firstPoint)
      handler->brush.add(thickPoint);
    
    // update affected area
    TRect strokeRect = handler->brush.getLastRect() * ras->getBounds();
    m_painting.tileSaver->save( strokeRect );
    m_painting.affectedRect += strokeRect;
    handler->brush.generateLastPieceOfStroke( m_painting.pencil.realPencil );
    
    // determine invalidate rect
    invalidateRect += convert(strokeRect) - rasCenter;
  } else
  if (m_painting.blured.isActive) {
    // blured brush case (aka regular brush)

    double maxThick = m_rasThickness.getValue().second;
    DrawOrder drawOrder = (DrawOrder)m_drawOrder.getIndex();
    
    // init brush
    BluredStroke *handler;
    if (firstPoint) {
      handler = new BluredStroke(m_workRas, maxThick, m_brushPad, false);
      // if the drawOrder mode = "Palette Order",
      // get styleId list which is above the current style in the palette
      if (drawOrder == PaletteOrder) {
        QSet<int> aboveStyleIds;
        getAboveStyleIdSet(m_painting.styleId, ri->getPalette(), aboveStyleIds);
        handler->brush.setAboveStyleIds(aboveStyleIds);
      }
      track.handler = handler;
    }
    handler = dynamic_cast<BluredStroke*>(track.handler.getPointer());
    if (!handler) return;

    // paint stroke
    double radius = computeThickness(pressure, m_rasThickness);
    TThickPoint thickPoint(fixedPosition + rasCenter, radius*2);
    TRect strokeRect( tfloor(thickPoint.x - maxThick - 0.999),
                      tfloor(thickPoint.y - maxThick - 0.999),
                      tceil(thickPoint.x + maxThick + 0.999),
                      tceil(thickPoint.y + maxThick + 0.999) );
    strokeRect *= ras->getBounds();
    m_painting.affectedRect += strokeRect;
    updateWorkAndBackupRasters(m_painting.affectedRect);
    m_painting.tileSaver->save(strokeRect);
    handler->brush.addPoint(thickPoint, 1, !lastPoint);
    handler->brush.updateDrawing( ras, m_backupRas, strokeRect,
                                  m_painting.styleId, drawOrder,
                                  m_modifierLockAlpha.getValue() );
    
    invalidateRect += convert(strokeRect) - rasCenter;
  }

  // invalidate rect
  if (firstTrack) {
    // for the first track we will paint cursor circle
    // here we will invalidate rects for it
    double thickness = m_isMyPaintStyleSelected ? m_maxCursorThick : m_maxThick*0.5;
    TPointD thickOffset(thickness + 1, thickness + 1);
    invalidateRect += TRectD(m_brushPos    - thickOffset, m_brushPos    + thickOffset);
    invalidateRect += TRectD(fixedPosition - thickOffset, fixedPosition + thickOffset);
    m_mousePos = point.position;
    m_brushPos = fixedPosition;
  }
 
  if (!invalidateRect.isEmpty())
    invalidate(invalidateRect.enlarge(2));
}

//---------------------------------------------------------------------------------------------------------------

// 明日はここをMyPaintのときにカーソルを消せるように修正する！！！！！！
void ToonzRasterBrushTool::inputMouseMove(const TPointD &position, const TInputState &state) {
  struct Locals {
    ToonzRasterBrushTool *m_this;

    void setValue(TDoublePairProperty &prop,
                  const TDoublePairProperty::Value &value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TDoublePairProperty &prop, double min, double max) {
      if (min == 0.0 && max == 0.0) return;
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.first += min;
      value.second += max;
      if (value.first > value.second) value.first = value.second;
      value.first  = tcrop(value.first, range.first, range.second);
      value.second = tcrop(value.second, range.first, range.second);

      setValue(prop, value);
    }
  } locals = {this};

  double thickness =
      (m_isMyPaintStyleSelected) ? (double)(m_maxCursorThick + 1) : m_maxThick;
  TPointD halfThick(thickness * 0.5, thickness * 0.5);
  TRectD invalidateRect(m_brushPos - halfThick, m_brushPos + halfThick);

  if ( Preferences::instance()->useCtrlAltToResizeBrushEnabled()
    && state.isKeyPressed(TKey::control)
    && state.isKeyPressed(TKey::alt)
    && !state.isKeyPressed(TKey::shift) )
  {
    // Resize the brush if CTRL+ALT is pressed and the preference is enabled.
    const TPointD &diff = position - m_mousePos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMax(m_rasThickness, min, max);

    double radius = m_rasThickness.getValue().second * 0.5;
    invalidateRect += TRectD(m_brushPos - TPointD(radius, radius),
                             m_brushPos + TPointD(radius, radius));

  } else {
    m_mousePos = position;
    m_brushPos = getCenteredCursorPos(position);

    invalidateRect += TRectD(position - halfThick, position + halfThick);
  }

  invalidate(invalidateRect.enlarge(2));

  if (m_minThick == 0 && m_maxThick == 0) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  }
}

//-------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::draw() {
  m_inputmanager.draw();

  /*--ショートカットでのツール切り替え時に赤点が描かれるのを防止する--*/
  if (m_minThick == 0 && m_maxThick == 0 &&
      !Preferences::instance()->getShow0ThickLines())
    return;

  TImageP img = getImage(false, 1);

  if (getApplication()->getCurrentObject()->isSpline()) return;

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  // Draw the brush outline - change color when the Ink / Paint check is
  // activated
  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
    glColor3d(0.5, 0.8, 0.8);
  // normally draw in red
  else
    glColor3d(1.0, 0.0, 0.0);

  if (m_isMyPaintStyleSelected) {
    tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5);
    tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5);
    return;
  }

  if (TToonzImageP ti = img) {
    TRasterP ras = ti->getRaster();
    int lx       = ras->getLx();
    int ly       = ras->getLy();
    drawEmptyCircle(m_brushPos, tround(m_minThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
    drawEmptyCircle(m_brushPos, tround(m_maxThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
  } else {
    drawEmptyCircle(m_brushPos, tround(m_minThick), true, true,
                    m_pencil.getValue());
    drawEmptyCircle(m_brushPos, tround(m_maxThick), true, true,
                    m_pencil.getValue());
  }
}

//--------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onEnter() {
  m_minThick = m_rasThickness.getValue().first;
  m_maxThick = m_rasThickness.getValue().second;
  updateCurrentStyle();
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onLeave() {
  m_minThick       = 0;
  m_maxThick       = 0;
  m_minCursorThick = 0;
  m_maxCursorThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *ToonzRasterBrushTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();

  return &m_prop[idx];
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onImageChanged() {
  if (!isEnabled()) return;
  setWorkAndBackupImages();
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::setWorkAndBackupImages() {
  TToonzImageP ti = (TToonzImageP)getImage(false, 1);
  if (!ti) return;
  TRasterP ras   = ti->getRaster();
  TDimension dim = ras->getSize();

  if (!m_workRas || m_workRas->getLx() < dim.lx || m_workRas->getLy() < dim.ly)
    m_workRas = TRaster32P(dim);
  if (!m_backupRas || m_backupRas->getLx() < dim.lx || m_backupRas->getLy() < dim.ly)
    m_backupRas = TRasterCM32P(dim);

  m_workBackupRect.empty();
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::updateWorkAndBackupRasters(const TRect &rect) {
  TToonzImageP ti = TImageP(getImage(false, 1));
  if (!ti) return;
  TRasterCM32P ras = ti->getRaster();

  // work and backup rect will additionaly enlarged to 1/8 of it size
  // in each affected direction to predict future possible enlargements in this direction
  const int denominator = 8;
  TRect enlargedRect    = rect + m_workBackupRect;
  int dx                = (enlargedRect.getLx() - 1) / denominator + 1;
  int dy                = (enlargedRect.getLy() - 1) / denominator + 1;

  if (m_workBackupRect.isEmpty()) {
    enlargedRect.x0 -= dx;
    enlargedRect.y0 -= dy;
    enlargedRect.x1 += dx;
    enlargedRect.y1 += dy;

    enlargedRect *= ras->getBounds();
    if (enlargedRect.isEmpty()) return;

    m_workRas->extract(enlargedRect)->copy(ras->extract(enlargedRect));
    m_backupRas->extract(enlargedRect)->copy(ras->extract(enlargedRect));
  } else {
    if (enlargedRect.x0 < m_workBackupRect.x0) enlargedRect.x0 -= dx;
    if (enlargedRect.y0 < m_workBackupRect.y0) enlargedRect.y0 -= dy;
    if (enlargedRect.x1 > m_workBackupRect.x1) enlargedRect.x1 += dx;
    if (enlargedRect.y1 > m_workBackupRect.y1) enlargedRect.y1 += dy;

    enlargedRect *= ras->getBounds();
    if (enlargedRect.isEmpty()) return;

    TRect lastRect     = m_workBackupRect * ras->getBounds();
    QList<TRect> rects = ToolUtils::splitRect(enlargedRect, lastRect);
    for (int i = 0; i < rects.size(); i++) {
      m_workRas->extract(rects[i])->copy(ras->extract(rects[i]));
      m_backupRas->extract(rects[i])->copy(ras->extract(rects[i]));
    }
  }

  m_workBackupRect = enlargedRect;
}

//------------------------------------------------------------------

bool ToonzRasterBrushTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  if (propertyName == m_preset.getName()) {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastBrush();

    RasterBrushPreset  = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  RasterBrushMinSize       = m_rasThickness.getValue().first;
  RasterBrushMaxSize       = m_rasThickness.getValue().second;
  BrushSmooth              = m_smooth.getValue();
  BrushDrawOrder           = m_drawOrder.getIndex();
  RasterBrushPencilMode    = m_pencil.getValue();
  BrushPressureSensitivity = m_pressure.getValue();
  RasterBrushHardness      = m_hardness.getValue();
  RasterBrushModifierSize  = m_modifierSize.getValue();
  BrushLockAlpha           = m_modifierLockAlpha.getValue();
  RasterBrushAssistants    = m_assistants.getValue();

  // Recalculate/reset based on changed settings
  if (propertyName == m_rasThickness.getName()) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  }

  if (propertyName == m_hardness.getName() ||
      propertyName == m_rasThickness.getName()) {
    m_brushPad = getBrushPad(m_rasThickness.getValue().second,
                             m_hardness.getValue() * 0.01);
    TRectD rect(m_mousePos - TPointD(m_maxThick + 2, m_maxThick + 2),
                m_mousePos + TPointD(m_maxThick + 2, m_maxThick + 2));
    invalidate(rect);
  }

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    RasterBrushPreset  = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  return true;
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(TEnv::getConfigDir() + "brush_toonzraster.txt");
  }

  // Rebuild the presets property entries
  const std::set<BrushData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<BrushData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::loadPreset() {
  const std::set<BrushData> &presets = m_presetsManager.presets();
  std::set<BrushData>::const_iterator it;

  it = presets.find(BrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const BrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_rasThickness.setValue(
        TDoublePairProperty::Value(std::max(preset.m_min, 1.0), preset.m_max));
    m_hardness.setValue(preset.m_hardness, true);
    m_smooth.setValue(preset.m_smooth, true);
    m_drawOrder.setIndex(preset.m_drawOrder);
    m_pencil.setValue(preset.m_pencil);
    m_pressure.setValue(preset.m_pressure);
    m_modifierSize.setValue(preset.m_modifierSize);
    m_modifierLockAlpha.setValue(preset.m_modifierLockAlpha);
    m_assistants.setValue(preset.m_assistants);

    // Recalculate based on updated presets
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;

    m_brushPad = ToolUtils::getBrushPad(preset.m_max, preset.m_hardness * 0.01);
  } catch (...) {
  }
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::addPreset(QString name) {
  // Build the preset
  BrushData preset(name.toStdWString());

  preset.m_min = m_rasThickness.getValue().first;
  preset.m_max = m_rasThickness.getValue().second;

  preset.m_smooth            = m_smooth.getValue();
  preset.m_hardness          = m_hardness.getValue();
  preset.m_drawOrder         = m_drawOrder.getIndex();
  preset.m_pencil            = m_pencil.getValue();
  preset.m_pressure          = m_pressure.getValue();
  preset.m_modifierSize      = m_modifierSize.getValue();
  preset.m_modifierLockAlpha = m_modifierLockAlpha.getValue();
  preset.m_assistants        = m_assistants.getValue();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
  RasterBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  RasterBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::loadLastBrush() {
  m_rasThickness.setValue(
      TDoublePairProperty::Value(RasterBrushMinSize, RasterBrushMaxSize));
  m_drawOrder.setIndex(BrushDrawOrder);
  m_pencil.setValue(RasterBrushPencilMode ? 1 : 0);
  m_hardness.setValue(RasterBrushHardness);
  m_pressure.setValue(BrushPressureSensitivity ? 1 : 0);
  m_smooth.setValue(BrushSmooth);
  m_modifierSize.setValue(RasterBrushModifierSize);
  m_modifierLockAlpha.setValue(BrushLockAlpha ? 1 : 0);
  m_assistants.setValue(RasterBrushAssistants ? 1 : 0);

  // Recalculate based on prior values
  m_minThick = m_rasThickness.getValue().first;
  m_maxThick = m_rasThickness.getValue().second;

  m_brushPad = getBrushPad(m_rasThickness.getValue().second,
                           m_hardness.getValue() * 0.01);
}

//------------------------------------------------------------------
/*!	Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す
 */
bool ToonzRasterBrushTool::isPencilModeActive() {
  return getTargetType() == TTool::ToonzImage && m_pencil.getValue();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::onColorStyleChanged() {
  m_inputmanager.finishTracks();
  m_enabled = false;

  TTool::Application *app = getApplication();
  TMyPaintBrushStyle *mpbs =
      dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
  m_isMyPaintStyleSelected = (mpbs) ? true : false;
  getApplication()->getCurrentTool()->notifyToolChanged();
}

//------------------------------------------------------------------

double ToonzRasterBrushTool::restartBrushTimer() {
  double dtime = m_brushTimer.nsecsElapsed() * 1e-9;
  m_brushTimer.restart();
  return dtime;
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::updateCurrentStyle() {
  if (m_isMyPaintStyleSelected) {
    TTool::Application *app = TTool::getApplication();
    TMyPaintBrushStyle *brushStyle =
        dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
    if (!brushStyle) {
      // brush changed to normal abnormally. Complete color style change.
      onColorStyleChanged();
      return;
    }
    double radiusLog = brushStyle->getBrush().getBaseValue(
                           MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC) +
                       m_modifierSize.getValue() * log(2.0);
    double radius    = exp(radiusLog);
    m_minCursorThick = m_maxCursorThick = (int)std::round(2.0 * radius);
  }
}
//==========================================================================================================

ToonzRasterBrushToolNotifier::ToonzRasterBrushToolNotifier(
    ToonzRasterBrushTool *tool)
    : m_tool(tool) {
  if (TTool::Application *app = m_tool->getApplication()) {
    if (TPaletteHandle *paletteHandle = app->getCurrentPalette()) {
      bool ret;
      ret = connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
                    SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                           SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(paletteSwitched()), this,
                           SLOT(onColorStyleChanged()));
      assert(ret);
    }
  }
  onColorStyleChanged();
}

//==========================================================================================================

// Tools instantiation

ToonzRasterBrushTool toonzPencil("T_Brush",
                                 TTool::ToonzImage | TTool::EmptyTarget);

//*******************************************************************************
//    Brush Data implementation
//*******************************************************************************

BrushData::BrushData()
    : m_name()
    , m_min(0.0)
    , m_max(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_pencil(false)
    , m_pressure(false)
    , m_drawOrder(0)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0)
    , m_modifierEraser(0.0)
    , m_modifierLockAlpha(0.0)
    , m_assistants(false) {}

//----------------------------------------------------------------------------------------------------------

BrushData::BrushData(const std::wstring &name)
    : m_name(name)
    , m_min(0.0)
    , m_max(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_pencil(false)
    , m_pressure(false)
    , m_drawOrder(0)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0)
    , m_modifierEraser(0.0)
    , m_modifierLockAlpha(0.0)
    , m_assistants(false) {}

//----------------------------------------------------------------------------------------------------------

void BrushData::saveData(TOStream &os) {
  os.openChild("Name");
  os << m_name;
  os.closeChild();
  os.openChild("Thickness");
  os << m_min << m_max;
  os.closeChild();
  os.openChild("Smooth");
  os << m_smooth;
  os.closeChild();
  os.openChild("Hardness");
  os << m_hardness;
  os.closeChild();
  os.openChild("Opacity");
  os << m_opacityMin << m_opacityMax;
  os.closeChild();
  os.openChild("Draw_Order");
  os << m_drawOrder;
  os.closeChild();
  os.openChild("Pencil");
  os << (int)m_pencil;
  os.closeChild();
  os.openChild("Pressure_Sensitivity");
  os << (int)m_pressure;
  os.closeChild();
  os.openChild("Modifier_Size");
  os << m_modifierSize;
  os.closeChild();
  os.openChild("Modifier_Opacity");
  os << m_modifierOpacity;
  os.closeChild();
  os.openChild("Modifier_Eraser");
  os << (int)m_modifierEraser;
  os.closeChild();
  os.openChild("Modifier_LockAlpha");
  os << (int)m_modifierLockAlpha;
  os.closeChild();
  os.openChild("Assistants");
  os << (int)m_assistants;
  os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void BrushData::loadData(TIStream &is) {
  std::string tagName;
  int val;

  while (is.matchTag(tagName)) {
    if (tagName == "Name")
      is >> m_name, is.matchEndTag();
    else if (tagName == "Thickness")
      is >> m_min >> m_max, is.matchEndTag();
    else if (tagName == "Smooth")
      is >> m_smooth, is.matchEndTag();
    else if (tagName == "Hardness")
      is >> m_hardness, is.matchEndTag();
    else if (tagName == "Opacity")
      is >> m_opacityMin >> m_opacityMax, is.matchEndTag();
    else if (tagName == "Selective" ||
             tagName == "Draw_Order")  // "Selective" is left to keep backward
                                       // compatibility
      is >> m_drawOrder, is.matchEndTag();
    else if (tagName == "Pencil")
      is >> val, m_pencil = val, is.matchEndTag();
    else if (tagName == "Pressure_Sensitivity")
      is >> val, m_pressure = val, is.matchEndTag();
    else if (tagName == "Modifier_Size")
      is >> m_modifierSize, is.matchEndTag();
    else if (tagName == "Modifier_Opacity")
      is >> m_modifierOpacity, is.matchEndTag();
    else if (tagName == "Modifier_Eraser")
      is >> val, m_modifierEraser = val, is.matchEndTag();
    else if (tagName == "Modifier_LockAlpha")
      is >> val, m_modifierLockAlpha = val, is.matchEndTag();
    else if (tagName == "Assistants")
      is >> val, m_assistants = val, is.matchEndTag();
    else
      is.skipCurrentTag();
  }
}

//----------------------------------------------------------------------------------------------------------

PERSIST_IDENTIFIER(BrushData, "BrushData");

//*******************************************************************************
//    Brush Preset Manager implementation
//*******************************************************************************

void BrushPresetManager::load(const TFilePath &fp) {
  m_fp = fp;

  std::string tagName;
  BrushData data;

  TIStream is(m_fp);
  try {
    while (is.matchTag(tagName)) {
      if (tagName == "version") {
        VersionNumber version;
        is >> version.first >> version.second;

        is.setVersion(version);
        is.matchEndTag();
      } else if (tagName == "brushes") {
        while (is.matchTag(tagName)) {
          if (tagName == "brush") {
            is >> data, m_presets.insert(data);
            is.matchEndTag();
          } else
            is.skipCurrentTag();
        }

        is.matchEndTag();
      } else
        is.skipCurrentTag();
    }
  } catch (...) {
  }
}

//------------------------------------------------------------------

void BrushPresetManager::save() {
  TOStream os(m_fp);

  os.openChild("version");
  os << 1 << 19;
  os.closeChild();

  os.openChild("brushes");

  std::set<BrushData>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    os.openChild("brush");
    os << (TPersist &)*it;
    os.closeChild();
  }

  os.closeChild();
}

//------------------------------------------------------------------

void BrushPresetManager::addPreset(const BrushData &data) {
  m_presets.erase(data);  // Overwriting insertion
  m_presets.insert(data);
  save();
}

//------------------------------------------------------------------

void BrushPresetManager::removePreset(const std::wstring &name) {
  m_presets.erase(BrushData(name));
  save();
}
