

#include "brushtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tooloptions.h"
#include "bluredbrush.h"

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
#include "toonz/rasterstrokegenerator.h"
#include "toonz/ttileset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzimageutils.h"
#include "toonz/palettecontroller.h"
#include "toonz/stage2.h"
#include "tw/keycodes.h"
#include "toonz/preferences.h"

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

TEnv::DoubleVar VectorBrushMinSize("InknpaintVectorBrushMinSize", 1);
TEnv::DoubleVar VectorBrushMaxSize("InknpaintVectorBrushMaxSize", 5);
TEnv::IntVar VectorCapStyle("InknpaintVectorCapStyle", 1);
TEnv::IntVar VectorJoinStyle("InknpaintVectorJoinStyle", 1);
TEnv::IntVar VectorMiterValue("InknpaintVectorMiterValue", 4);
TEnv::DoubleVar RasterBrushMinSize("InknpaintRasterBrushMinSize", 1);
TEnv::DoubleVar RasterBrushMaxSize("InknpaintRasterBrushMaxSize", 5);
TEnv::DoubleVar BrushAccuracy("InknpaintBrushAccuracy", 20);
TEnv::DoubleVar BrushSmooth("InknpaintBrushSmooth", 0);
TEnv::IntVar BrushSelective("InknpaintBrushSelective", 0);
TEnv::IntVar BrushBreakSharpAngles("InknpaintBrushBreakSharpAngles", 0);
TEnv::IntVar RasterBrushPencilMode("InknpaintRasterBrushPencilMode", 0);
TEnv::IntVar BrushPressureSensitivity("InknpaintBrushPressureSensitivity", 1);
TEnv::DoubleVar RasterBrushHardness("RasterBrushHardness", 100);
TEnv::IntVar VectorBrushFrameRange("VectorBrushFrameRange", 0);
TEnv::IntVar VectorBrushSnap("VectorBrushSnap", 0);
TEnv::IntVar VectorBrushSnapSensitivity("VectorBrushSnapSensitivity", 0);

//-------------------------------------------------------------------

#define ROUNDC_WSTR L"round_cap"
#define BUTT_WSTR L"butt_cap"
#define PROJECTING_WSTR L"projecting_cap"
#define ROUNDJ_WSTR L"round_join"
#define BEVEL_WSTR L"bevel_join"
#define MITER_WSTR L"miter_join"
#define CUSTOM_WSTR L"<custom>"

#define LINEAR_WSTR L"Linear"
#define EASEIN_WSTR L"In"
#define EASEOUT_WSTR L"Out"
#define EASEINOUT_WSTR L"In&Out"

#define LOW_WSTR L"Low"
#define MEDIUM_WSTR L"Med"
#define HIGH_WSTR L"High"

const double SNAPPING_LOW    = 5.0;
const double SNAPPING_MEDIUM = 25.0;
const double SNAPPING_HIGH   = 100.0;
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
      t = 0.25 * (2 * q.x * q.x + 2 * q.y * q.y - q.x * p0.x + q.x * p2.x -
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
      t_ext           = 1;
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
                      bool levelCreated) {
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
  TXshSimpleLevel *sl = application->getCurrentLevel()->getSimpleLevel();
  TFrameId id = application->getCurrentTool()->getTool()->getCurrentFid();

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

void addStrokeToImage(TTool::Application *application, const TVectorImageP &vi,
                      TStroke *stroke, bool breakAngles, bool frameCreated,
                      bool levelCreated) {
  QMutexLocker lock(vi->getMutex());
  addStroke(application, vi.getPointer(), stroke, breakAngles, frameCreated,
            levelCreated);
  // la notifica viene gia fatta da addStroke!
  // getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//=========================================================================================================

class RasterBrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  bool m_isPencil;

public:
  RasterBrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
                  int styleId, bool selective, TXshSimpleLevel *level,
                  const TFrameId &frameId, bool isPencil, bool isFrameCreated,
                  bool isLevelCreated)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_isPencil(isPencil) {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP image = getImage();
    TRasterCM32P ras   = image->getRaster();
    RasterStrokeGenerator m_rasterTrack(
        ras, BRUSH, NONE, m_styleId, m_points[0], m_selective, 0, !m_isPencil);
    m_rasterTrack.setPointsSequence(m_points);
    m_rasterTrack.generateStroke(m_isPencil);
    image->setSavebox(image->getSavebox() +
                      m_rasterTrack.getBBox(m_rasterTrack.getPointsSequence()));
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
  bool m_selective;
  int m_maxThick;
  double m_hardness;

public:
  RasterBluredBrushUndo(TTileSetCM32 *tileSet,
                        const std::vector<TThickPoint> &points, int styleId,
                        bool selective, TXshSimpleLevel *level,
                        const TFrameId &frameId, int maxThick, double hardness,
                        bool isFrameCreated, bool isLevelCreated)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_maxThick(maxThick)
      , m_hardness(hardness) {}

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

    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.updateDrawing(ras, ras, bbox, m_styleId, m_selective);
    if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1],
                   1, 1);
      brush.updateDrawing(ras, backupRas, bbox, m_styleId, m_selective);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.updateDrawing(ras, backupRas, bbox, m_styleId, m_selective);
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

double computeThickness(int pressure, const TDoublePairProperty &property,
                        bool isPath) {
  if (isPath) return 0.0;
  double p                    = pressure / 255.0;
  double t                    = p * p * p;
  double thick0               = property.getValue().first;
  double thick1               = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t) * 0.5;
}

//---------------------------------------------------------------------------------------------------------

int computeThickness(int pressure, const TIntPairProperty &property,
                     bool isPath) {
  if (isPath) return 0.0;
  double p   = pressure / 255.0;
  double t   = p * p * p;
  int thick0 = property.getValue().first;
  int thick1 = property.getValue().second;
  return tround(thick0 + (thick1 - thick0) * t);
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

static void Smooth(std::vector<TThickPoint> &points, int radius) {
  int n = (int)points.size();
  if (radius < 1 || n < 3) {
    return;
  }

  std::vector<TThickPoint> result;

  float d = 1.0f / (radius * 2 + 1);

  for (int i = 1; i < n - 1; ++i) {
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

  for (int i = 1; i < n - 1; ++i) {
    points[i].x     = result[i - 1].x;
    points[i].y     = result[i - 1].y;
    points[i].thick = result[i - 1].thick;
  }

  if (points.size() >= 3) {
    std::vector<TThickPoint> pts;
    CatmullRomInterpolate(points[0], points[0], points[1], points[2], 10, pts);
    std::vector<TThickPoint>::iterator it = points.begin();
    points.insert(it, pts.begin(), pts.end());

    pts.clear();
    CatmullRomInterpolate(points[n - 3], points[n - 2], points[n - 1],
                          points[n - 1], 10, pts);
    it = points.begin();
    it += n - 1;
    points.insert(it, pts.begin(), pts.end());
  }
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::beginStroke(int smooth) {
  m_smooth      = smooth;
  m_outputIndex = 0;
  m_readIndex   = -1;
  m_rawPoints.clear();
  m_outputPoints.clear();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::addPoint(const TThickPoint &point) {
  if (m_rawPoints.size() > 0 && m_rawPoints.back().x == point.x &&
      m_rawPoints.back().y == point.y) {
    return;
  }
  m_rawPoints.push_back(point);
  generatePoints();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::endStroke() {
  generatePoints();
  // force enable the output all segments
  m_outputIndex = m_outputPoints.size() - 1;
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::clearPoints() {
  m_outputIndex = 0;
  m_readIndex   = -1;
  m_outputPoints.clear();
  m_rawPoints.clear();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::getSmoothPoints(std::vector<TThickPoint> &smoothPoints) {
  int n = m_outputPoints.size();
  for (int i = m_readIndex + 1; i <= m_outputIndex && i < n; ++i) {
    smoothPoints.push_back(m_outputPoints[i]);
  }
  m_readIndex = m_outputIndex;
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::generatePoints() {
  int n = (int)m_rawPoints.size();
  if (n == 0) {
    return;
  }
  std::vector<TThickPoint> smoothedPoints;
  // Add more stroke samples before applying the smoothing
  // This is because the raw inputs points are too few to support smooth result,
  // especially on stroke ends
  smoothedPoints.push_back(m_rawPoints.front());
  for (int i = 1; i < n; ++i) {
    const TThickPoint &p1 = m_rawPoints[i - 1];
    const TThickPoint &p2 = m_rawPoints[i];
    const TThickPoint &p0 = i - 2 >= 0 ? m_rawPoints[i - 2] : p1;
    const TThickPoint &p3 = i + 1 < n ? m_rawPoints[i + 1] : p2;

    int samples = 8;
    CatmullRomInterpolate(p0, p1, p2, p3, samples, smoothedPoints);
    smoothedPoints.push_back(p2);
  }
  // Apply the 1D box filter
  // Multiple passes result in better quality and fix the stroke ends break
  // issue
  for (int i = 0; i < 3; ++i) {
    Smooth(smoothedPoints, m_smooth);
  }
  // Compare the new smoothed stroke with old one
  // Enable the output for unchanged parts
  int outputNum = (int)m_outputPoints.size();
  for (int i = m_outputIndex; i < outputNum; ++i) {
    if (m_outputPoints[i] != smoothedPoints[i]) {
      break;
    }
    ++m_outputIndex;
  }
  m_outputPoints = smoothedPoints;
}

//===================================================================
//
// BrushTool
//
//-----------------------------------------------------------------------------

BrushTool::BrushTool(std::string name, int targetType)
    : TTool(name)
    , m_thickness("Size", 0, 100, 0, 5)
    , m_rasThickness("Size", 1, 100, 1, 5)
    , m_accuracy("Accuracy:", 1, 100, 20)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_preset("Preset:")
    , m_selective("Selective", false)
    , m_breakAngles("Break", true)
    , m_pencil("Pencil", false)
    , m_pressure("Pressure", true)
    , m_capStyle("Cap")
    , m_joinStyle("Join")
    , m_miterJoinLimit("Miter:", 0, 100, 4)
    , m_rasterTrack(0)
    , m_styleId(0)
    , m_modifiedRegion()
    , m_bluredBrush(0)
    , m_active(false)
    , m_enabled(false)
    , m_isPrompting(false)
    , m_firstTime(true)
    , m_firstFrameRange(true)
    , m_presetsLoaded(false)
    , m_frameRange("Range:")
    , m_snap("Snap", false)
    , m_snapSensitivity("Sensitivity:")
    , m_targetType(targetType)
    , m_workingFrameId(TFrameId()) {
  bind(targetType);

  if (targetType & TTool::Vectors) {
    m_prop[0].bind(m_thickness);
    m_prop[0].bind(m_accuracy);
    m_prop[0].bind(m_smooth);
    m_prop[0].bind(m_breakAngles);
    m_breakAngles.setId("BreakSharpAngles");
  }

  if (targetType & TTool::ToonzImage) {
    m_prop[0].bind(m_rasThickness);
    m_prop[0].bind(m_hardness);
    m_prop[0].bind(m_smooth);
    m_prop[0].bind(m_selective);
    m_prop[0].bind(m_pencil);
    m_pencil.setId("PencilMode");
    m_selective.setId("Selective");
  }

  m_prop[0].bind(m_pressure);

  if (targetType & TTool::Vectors) {
    m_frameRange.addValue(L"Off");
    m_frameRange.addValue(LINEAR_WSTR);
    m_frameRange.addValue(EASEIN_WSTR);
    m_frameRange.addValue(EASEOUT_WSTR);
    m_frameRange.addValue(EASEINOUT_WSTR);
    m_prop[0].bind(m_frameRange);
    m_frameRange.setId("FrameRange");
    m_prop[0].bind(m_snap);
    m_snap.setId("Snap");
    m_snapSensitivity.addValue(LOW_WSTR);
    m_snapSensitivity.addValue(MEDIUM_WSTR);
    m_snapSensitivity.addValue(HIGH_WSTR);
    m_prop[0].bind(m_snapSensitivity);
    m_snapSensitivity.setId("SnapSensitivity");
  }

  m_prop[0].bind(m_preset);
  m_preset.setId("BrushPreset");
  m_preset.addValue(CUSTOM_WSTR);
  m_pressure.setId("PressureSensitivity");

  m_capStyle.addValue(BUTT_WSTR);
  m_capStyle.addValue(ROUNDC_WSTR);
  m_capStyle.addValue(PROJECTING_WSTR);
  m_capStyle.setId("Cap");

  m_joinStyle.addValue(MITER_WSTR);
  m_joinStyle.addValue(ROUNDJ_WSTR);
  m_joinStyle.addValue(BEVEL_WSTR);
  m_joinStyle.setId("Join");

  m_miterJoinLimit.setId("Miter");

  if (targetType & TTool::Vectors) {
    m_prop[1].bind(m_capStyle);
    m_prop[1].bind(m_joinStyle);
    m_prop[1].bind(m_miterJoinLimit);
  }
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *BrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//-------------------------------------------------------------------------------------------------------

void BrushTool::drawLine(const TPointD &point, const TPointD &centre,
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

void BrushTool::drawEmptyCircle(TPointD pos, int thick, bool isLxEven,
                                bool isLyEven, bool isPencil) {
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

//-------------------------------------------------------------------------------------------------------

void BrushTool::updateTranslation() {
  m_thickness.setQStringName(tr("Size"));
  m_rasThickness.setQStringName(tr("Size"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_accuracy.setQStringName(tr("Accuracy:"));
  m_smooth.setQStringName(tr("Smooth:"));
  m_selective.setQStringName(tr("Selective"));
  // m_filled.setQStringName(tr("Filled"));
  m_preset.setQStringName(tr("Preset:"));
  m_breakAngles.setQStringName(tr("Break"));
  m_pencil.setQStringName(tr("Pencil"));
  m_pressure.setQStringName(tr("Pressure"));
  m_capStyle.setQStringName(tr("Cap"));
  m_joinStyle.setQStringName(tr("Join"));
  m_miterJoinLimit.setQStringName(tr("Miter:"));
  m_frameRange.setQStringName(tr("Range:"));
  m_snap.setQStringName(tr("Snap"));
  m_snapSensitivity.setQStringName("");
}

//---------------------------------------------------------------------------------------------------

void BrushTool::updateWorkAndBackupRasters(const TRect &rect) {
  TToonzImageP ti = TImageP(getImage(false, 1));
  if (!ti) return;

  TRasterCM32P ras = ti->getRaster();

  TRect _rect     = rect * ras->getBounds();
  TRect _lastRect = m_lastRect * ras->getBounds();

  if (_rect.isEmpty()) return;

  if (m_lastRect.isEmpty()) {
    m_workRas->extract(_rect)->clear();
    m_backupRas->extract(_rect)->copy(ras->extract(_rect));
    return;
  }

  QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
  for (int i = 0; i < rects.size(); i++) {
    m_workRas->extract(rects[i])->clear();
    m_backupRas->extract(rects[i])->copy(ras->extract(rects[i]));
  }
}

//---------------------------------------------------------------------------------------------------

void BrushTool::onActivate() {
  if (m_firstTime) {
    m_thickness.setValue(
        TDoublePairProperty::Value(VectorBrushMinSize, VectorBrushMaxSize));
    m_rasThickness.setValue(
        TDoublePairProperty::Value(RasterBrushMinSize, RasterBrushMaxSize));
    m_capStyle.setIndex(VectorCapStyle);
    m_joinStyle.setIndex(VectorJoinStyle);
    m_miterJoinLimit.setValue(VectorMiterValue);
    m_selective.setValue(BrushSelective ? 1 : 0);
    m_breakAngles.setValue(BrushBreakSharpAngles ? 1 : 0);
    m_pencil.setValue(RasterBrushPencilMode ? 1 : 0);
    m_pressure.setValue(BrushPressureSensitivity ? 1 : 0);
    m_firstTime = false;
    m_accuracy.setValue(BrushAccuracy);
    m_smooth.setValue(BrushSmooth);
    m_hardness.setValue(RasterBrushHardness);
    if (m_targetType & TTool::Vectors) {
      m_frameRange.setIndex(VectorBrushFrameRange);
      m_snap.setValue(VectorBrushSnap);
      m_snapSensitivity.setIndex(VectorBrushSnapSensitivity);
      switch (VectorBrushSnapSensitivity) {
      case 0:
        m_minDistance2 = SNAPPING_LOW;
        break;
      case 1:
        m_minDistance2 = SNAPPING_MEDIUM;
        break;
      case 2:
        m_minDistance2 = SNAPPING_HIGH;
        break;
      }
    }
  }
  if (m_targetType & TTool::ToonzImage) {
    m_brushPad = ToolUtils::getBrushPad(m_rasThickness.getValue().second,
                                        m_hardness.getValue() * 0.01);
    setWorkAndBackupImages();
  }
  resetFrameRange();
  // TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void BrushTool::onDeactivate() {
  /*---
   * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
   * ---*/
  if (m_tileSaver && !m_isPath) {
    bool isValid = m_enabled && m_active;
    m_enabled    = false;
    if (isValid) {
      TImageP image = getImage(true);
      if (TToonzImageP ti = image)
        finishRasterBrush(m_mousePos,
                          1); /*-- 最後のストロークの筆圧は1とする --*/
    }
  }
  m_workRas   = TRaster32P();
  m_backupRas = TRasterCM32P();
  resetFrameRange();
}

//--------------------------------------------------------------------------------------------------

bool BrushTool::preLeftButtonDown() {
  touchImage();
  if (m_isFrameCreated) setWorkAndBackupImages();
  return true;
}

//--------------------------------------------------------------------------------------------------

void BrushTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  int col   = app->getCurrentColumn()->getColumnIndex();
  m_isPath  = app->getCurrentObject()->isSpline();
  m_enabled = col >= 0 || m_isPath;
  // todo: gestire autoenable
  if (!m_enabled) return;
  if (!m_isPath) {
    m_currentColor = TPixel32::Black;
    m_active       = !!getImage(true);
    if (!m_active) {
      m_active = !!touchImage();
    }
    if (!m_active) return;

    if (m_active) {
      // nel caso che il colore corrente sia un cleanup/studiopalette color
      // oppure il colore di un colorfield
      m_styleId       = app->getCurrentLevelStyleIndex();
      TColorStyle *cs = app->getCurrentLevelStyle();
      if (cs) {
        TRasterStyleFx *rfx = cs ? cs->getRasterStyleFx() : 0;
        m_active =
            cs != 0 && (cs->isStrokeStyle() || (rfx && rfx->isInkStyle()));
        m_currentColor   = cs->getAverageColor();
        m_currentColor.m = 255;
      } else {
        m_styleId      = 1;
        m_currentColor = TPixel32::Black;
      }
    }
  } else {
    m_currentColor = TPixel32::Red;
    m_active       = true;
  }
  // assert(0<=m_styleId && m_styleId<2);
  TImageP img = getImage(true);
  TToonzImageP ri(img);
  if (ri) {
    TRasterCM32P ras = ri->getRaster();
    if (ras) {
      TPointD rasCenter = ras->getCenterD();
      m_tileSet         = new TTileSetCM32(ras->getSize());
      m_tileSaver       = new TTileSaverCM32(ras, m_tileSet);
      double maxThick   = m_rasThickness.getValue().second;
      double thickness =
          (m_pressure.getValue() || m_isPath)
              ? computeThickness(e.m_pressure, m_rasThickness, m_isPath) * 2
              : maxThick;

      /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する
       * ---*/
      if (m_pressure.getValue() && e.m_pressure == 255)
        thickness = m_rasThickness.getValue().first;

      TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
      TRectD invalidateRect(pos - halfThick, pos + halfThick);
      if (m_hardness.getValue() == 100 || m_pencil.getValue()) {
        /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる
         * --*/
        if (!m_pencil.getValue()) thickness -= 1.0;

        TThickPoint thickPoint(pos + convert(ras->getCenter()), thickness);
        m_rasterTrack = new RasterStrokeGenerator(
            ras, BRUSH, NONE, m_styleId, thickPoint, m_selective.getValue(), 0,
            !m_pencil.getValue());
        m_tileSaver->save(m_rasterTrack->getLastRect());
        m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue());

        m_smoothStroke.beginStroke(m_smooth.getValue());
        m_smoothStroke.addPoint(thickPoint);
        std::vector<TThickPoint> pts;
        m_smoothStroke.getSmoothPoints(
            pts);  // skip first point because it has been outputted
      } else {
        m_points.clear();
        TThickPoint point(pos + rasCenter, thickness);
        m_points.push_back(point);
        m_bluredBrush = new BluredBrush(m_workRas, maxThick, m_brushPad, false);

        m_strokeRect = m_bluredBrush->getBoundFromPoints(m_points);
        updateWorkAndBackupRasters(m_strokeRect);
        m_tileSaver->save(m_strokeRect);
        m_bluredBrush->addPoint(point, 1);
        m_bluredBrush->updateDrawing(ri->getRaster(), m_backupRas, m_strokeRect,
                                     m_styleId, m_selective.getValue());
        m_lastRect = m_strokeRect;

        m_smoothStroke.beginStroke(m_smooth.getValue());
        m_smoothStroke.addPoint(point);
        std::vector<TThickPoint> pts;
        m_smoothStroke.getSmoothPoints(
            pts);  // skip first point because it has been outputted
      }
      /*-- 作業中のFidを登録 --*/
      m_workingFrameId = getFrameId();

      invalidate(invalidateRect);
    }
  } else {  // vector happens here
    m_track.clear();
    double thickness =
        (m_pressure.getValue() || m_isPath)
            ? computeThickness(e.m_pressure, m_thickness, m_isPath)
            : m_thickness.getValue().second * 0.5;

    /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
    if (m_pressure.getValue() && e.m_pressure == 255)
      thickness     = m_rasThickness.getValue().first;
    m_currThickness = thickness;
    m_smoothStroke.beginStroke(m_smooth.getValue());

    if ((m_targetType & TTool::Vectors) && m_foundFirstSnap) {
      addTrackPoint(TThickPoint(m_firstSnapPoint, thickness),
                    getPixelSize() * getPixelSize());
    } else
      addTrackPoint(TThickPoint(pos, thickness),
                    getPixelSize() * getPixelSize());
  }
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;

  if (!m_enabled || !m_active) return;

  bool isAdded;

  if (TToonzImageP ti = TImageP(getImage(true))) {
    TPointD rasCenter = ti->getRaster()->getCenterD();
    int maxThickness  = m_rasThickness.getValue().second;
    double thickness =
        (m_pressure.getValue() || m_isPath)
            ? computeThickness(e.m_pressure, m_rasThickness, m_isPath) * 2
            : maxThickness;
    TRectD invalidateRect;
    if (m_rasterTrack &&
        (m_hardness.getValue() == 100 || m_pencil.getValue())) {
      /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる
       * --*/
      if (!m_pencil.getValue()) thickness -= 1.0;

      TThickPoint thickPoint(pos + rasCenter, thickness);
      m_smoothStroke.addPoint(thickPoint);
      std::vector<TThickPoint> pts;
      m_smoothStroke.getSmoothPoints(pts);
      for (size_t i = 0; i < pts.size(); ++i) {
        const TThickPoint &thickPoint = pts[i];
        isAdded                       = m_rasterTrack->add(thickPoint);
        if (isAdded) {
          m_tileSaver->save(m_rasterTrack->getLastRect());
          m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue());
          std::vector<TThickPoint> brushPoints =
              m_rasterTrack->getPointsSequence();
          int m = (int)brushPoints.size();
          std::vector<TThickPoint> points;
          if (m == 3) {
            points.push_back(brushPoints[0]);
            points.push_back(brushPoints[1]);
          } else {
            points.push_back(brushPoints[m - 4]);
            points.push_back(brushPoints[m - 3]);
            points.push_back(brushPoints[m - 2]);
          }
          if (i == 0) {
            invalidateRect =
                ToolUtils::getBounds(points, maxThickness) - rasCenter;
          } else {
            invalidateRect +=
                ToolUtils::getBounds(points, maxThickness) - rasCenter;
          }
        }
      }
    } else {
      // antialiased brush
      assert(m_workRas.getPointer() && m_backupRas.getPointer());
      TThickPoint thickPoint(pos + rasCenter, thickness);
      m_smoothStroke.addPoint(thickPoint);
      std::vector<TThickPoint> pts;
      m_smoothStroke.getSmoothPoints(pts);
      bool rectUpdated = false;
      for (size_t i = 0; i < pts.size(); ++i) {
        TThickPoint old = m_points.back();
        if (norm2(pos - old) < 4) continue;

        const TThickPoint &point = pts[i];
        TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
        m_points.push_back(mid);
        m_points.push_back(point);

        TRect bbox;
        int m = (int)m_points.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          // ho appena cominciato. devo disegnare un segmento
          TThickPoint pa = m_points.front();
          points.push_back(pa);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
          m_lastRect += bbox;
        } else {
          points.push_back(m_points[m - 4]);
          points.push_back(old);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(m_points[m - 4], old, mid, 1, 1);
          m_lastRect += bbox;
        }
        if (!rectUpdated) {
          invalidateRect =
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
          rectUpdated = true;
        } else {
          invalidateRect +=
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        }

        m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                     m_styleId, m_selective.getValue());
        m_strokeRect += bbox;
      }
    }
    invalidate(invalidateRect.enlarge(2));
  } else {
    double thickness =
        (m_pressure.getValue() || m_isPath)
            ? computeThickness(e.m_pressure, m_thickness, m_isPath)
            : m_thickness.getValue().second * 0.5;

    m_currThickness = thickness;

    m_mousePos      = pos;
    m_lastSnapPoint = pos;
    m_foundLastSnap = false;
    checkStrokeSnapping(false);
    checkGuideSnapping(false);
    m_brushPos = m_lastSnapPoint;

    if (e.isShiftPressed()) {
      m_smoothStroke.clearPoints();
      m_track.add(TThickPoint(m_brushPos, thickness),
                  getPixelSize() * getPixelSize());
      m_track.removeMiddlePoints();
    }

    else if (m_dragDraw)
      addTrackPoint(TThickPoint(pos, thickness),
                    getPixelSize() * getPixelSize());

    invalidate();
  }
}

//---------------------------------------------------------------------------------------------------------------

void BrushTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  bool isValid = m_enabled && m_active;
  m_enabled    = false;

  if (!isValid) return;

  if (m_isPath) {
    double error = 20.0 * getPixelSize();

    TStroke *stroke;
    if (e.isShiftPressed()) {
      m_track.removeMiddlePoints();
      stroke = m_track.makeStroke(0);
    } else {
      flushTrackPoint();
      stroke = m_track.makeStroke(error);
    }
    int points = stroke->getControlPointCount();

    if (TVectorImageP vi = getImage(true)) {
      struct Cleanup {
        BrushTool *m_this;
        ~Cleanup() { m_this->m_track.clear(), m_this->invalidate(); }
      } cleanup = {this};

      if (!isJustCreatedSpline(vi.getPointer())) {
        m_isPrompting = true;

        QString question("Are you sure you want to replace the motion path?");
        int ret =
            DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);

        m_isPrompting = false;

        if (ret == 2 || ret == 0) return;
      }

      QMutexLocker lock(vi->getMutex());

      TUndo *undo =
          new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());

      while (vi->getStrokeCount() > 0) vi->deleteStroke(0);
      vi->addStroke(stroke, false);

      notifyImageChanged();
      TUndoManager::manager()->add(undo);
    }

    return;
  }

  TImageP image = getImage(true);
  if (TVectorImageP vi = image) {
    if (m_track.isEmpty()) {
      m_styleId = 0;
      m_track.clear();
      return;
    }

    if (vi && m_snap.getValue() && m_foundLastSnap) {
      addTrackPoint(TThickPoint(m_lastSnapPoint, m_currThickness),
                    getPixelSize() * getPixelSize());
    }
    m_strokeIndex1   = -1;
    m_strokeIndex2   = -1;
    m_w1             = -1;
    m_w2             = -2;
    m_foundFirstSnap = false;
    m_foundLastSnap  = false;

    m_track.filterPoints();
    double error = 30.0 / (1 + 0.5 * m_accuracy.getValue());
    error *= getPixelSize();

    TStroke *stroke;
    if (e.isShiftPressed()) {
      m_track.removeMiddlePoints();
      stroke = m_track.makeStroke(0);
    } else {
      flushTrackPoint();
      stroke = m_track.makeStroke(error);
    }
    stroke->setStyle(m_styleId);
    {
      TStroke::OutlineOptions &options = stroke->outlineOptions();
      options.m_capStyle               = m_capStyle.getIndex();
      options.m_joinStyle              = m_joinStyle.getIndex();
      options.m_miterUpper             = m_miterJoinLimit.getValue();
    }
    m_styleId = 0;

    QMutexLocker lock(vi->getMutex());
    if (stroke->getControlPointCount() == 3 &&
        stroke->getControlPoint(0) !=
            stroke->getControlPoint(2))  // gli stroke con solo 1 chunk vengono
                                         // fatti dal tape tool...e devono venir
                                         // riconosciuti come speciali di
                                         // autoclose proprio dal fatto che
                                         // hanno 1 solo chunk.
      stroke->insertControlPoints(0.5);
    if (m_frameRange.getIndex()) {
      if (m_firstFrameId == -1) {
        m_firstStroke                   = new TStroke(*stroke);
        m_firstFrameId                  = getFrameId();
        TTool::Application *application = TTool::getApplication();
        if (application) {
          m_col        = application->getCurrentColumn()->getColumnIndex();
          m_firstFrame = application->getCurrentFrame()->getFrame();
        }
        m_rangeTrack = m_track;
        if (m_firstFrameRange) {
          m_veryFirstCol     = m_col;
          m_veryFirstFrame   = m_firstFrame;
          m_veryFirstFrameId = m_firstFrameId;
        }
      } else if (m_firstFrameId == getFrameId()) {
        if (m_firstStroke) {
          delete m_firstStroke;
          m_firstStroke = 0;
        }
        m_firstStroke = new TStroke(*stroke);
        m_rangeTrack  = m_track;
      } else {
        TFrameId currentId = getFrameId();
        int curCol = 0, curFrame = 0;
        TTool::Application *application = TTool::getApplication();
        if (application) {
          curCol   = application->getCurrentColumn()->getColumnIndex();
          curFrame = application->getCurrentFrame()->getFrame();
        }
        bool success =
            doFrameRangeStrokes(m_firstFrameId, m_firstStroke, getFrameId(),
                                stroke, m_firstFrameRange);
        if (e.isCtrlPressed()) {
          if (application) {
            if (m_firstFrameId > currentId) {
              if (application->getCurrentFrame()->isEditingScene()) {
                application->getCurrentColumn()->setColumnIndex(curCol);
                application->getCurrentFrame()->setFrame(curFrame);
              } else
                application->getCurrentFrame()->setFid(currentId);
            }
          }
          resetFrameRange();
          m_firstStroke     = new TStroke(*stroke);
          m_rangeTrack      = m_track;
          m_firstFrameId    = currentId;
          m_firstFrameRange = false;
        }

        if (application && !e.isCtrlPressed()) {
          if (application->getCurrentFrame()->isEditingScene()) {
            application->getCurrentColumn()->setColumnIndex(m_veryFirstCol);
            application->getCurrentFrame()->setFrame(m_veryFirstFrame);
          } else
            application->getCurrentFrame()->setFid(m_veryFirstFrameId);
        }

        if (!e.isCtrlPressed()) {
          resetFrameRange();
        }
      }
    } else {
      addStrokeToImage(getApplication(), vi, stroke, m_breakAngles.getValue(),
                       m_isFrameCreated, m_isLevelCreated);
      TRectD bbox = stroke->getBBox().enlarge(2) + m_track.getModifiedRegion();
      invalidate();
    }
    assert(stroke);
    m_track.clear();

  } else if (TToonzImageP ti = image) {
    finishRasterBrush(pos, e.m_pressure);
  }
}

//--------------------------------------------------------------------------------------------------

bool BrushTool::keyDown(int key, TUINT32 b, const TPoint &point) {
  if (key == TwConsts::TK_Esc) {
    resetFrameRange();
  }
  return false;
}

//--------------------------------------------------------------------------------------------------

bool BrushTool::doFrameRangeStrokes(TFrameId firstFrameId, TStroke *firstStroke,
                                    TFrameId lastFrameId, TStroke *lastStroke,
                                    bool drawFirstStroke) {
  TXshSimpleLevel *sl =
      TTool::getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel();
  TStroke *first           = new TStroke();
  TStroke *last            = new TStroke();
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();

  *first       = *firstStroke;
  *last        = *lastStroke;
  bool swapped = false;
  if (firstFrameId > lastFrameId) {
    tswap(firstFrameId, lastFrameId);
    *first  = *lastStroke;
    *last   = *firstStroke;
    swapped = true;
  }

  firstImage->addStroke(first);
  lastImage->addStroke(last);
  assert(firstFrameId <= lastFrameId);

  std::vector<TFrameId> allFids;
  sl->getFids(allFids);
  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
  if (i0 == allFids.end()) return false;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFrameId <= fid && fid <= lastFrameId);

    // This is an attempt to divide the tween evenly
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    double s = t;
    switch (m_frameRange.getIndex()) {
    case 1:  // LINEAR_WSTR
      break;
    case 2:  // EASEIN_WSTR
      s = t * t;
      break;  // s'(0) = 0
    case 3:   // EASEOUT_WSTR
      s = t * (2 - t);
      break;  // s'(1) = 0
    case 4:   // EASEINOUT_WSTR:
      s = t * t * (3 - 2 * t);
      break;  // s'(0) = s'(1) = 0
    }

    TTool::Application *app = TTool::getApplication();
    if (app) {
      if (app->getCurrentFrame()->isEditingScene())
        app->getCurrentFrame()->setFrame(fid.getNumber() - 1);
      else
        app->getCurrentFrame()->setFid(fid);
    }

    TVectorImageP img = sl->getFrame(fid, true);
    if (t == 0) {
      if (!swapped && !drawFirstStroke) {
      } else
        addStrokeToImage(getApplication(), img, firstImage->getStroke(0),
                         m_breakAngles.getValue(), m_isFrameCreated,
                         m_isLevelCreated);
    } else if (t == 1) {
      if (swapped && !drawFirstStroke) {
      } else
        addStrokeToImage(getApplication(), img, lastImage->getStroke(0),
                         m_breakAngles.getValue(), m_isFrameCreated,
                         m_isLevelCreated);
    } else {
      assert(firstImage->getStrokeCount() == 1);
      assert(lastImage->getStrokeCount() == 1);
      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(s);
      assert(vi->getStrokeCount() == 1);
      addStrokeToImage(getApplication(), img, vi->getStroke(0),
                       m_breakAngles.getValue(), m_isFrameCreated,
                       m_isLevelCreated);
    }
  }
  TUndoManager::manager()->endBlock();
  notifyImageChanged();
  return true;
}

//--------------------------------------------------------------------------------------------------

void BrushTool::addTrackPoint(const TThickPoint &point, double pixelSize2) {
  m_smoothStroke.addPoint(point);
  std::vector<TThickPoint> pts;
  m_smoothStroke.getSmoothPoints(pts);
  for (size_t i = 0; i < pts.size(); ++i) {
    m_track.add(pts[i], pixelSize2);
  }
}

//--------------------------------------------------------------------------------------------------

void BrushTool::flushTrackPoint() {
  m_smoothStroke.endStroke();
  std::vector<TThickPoint> pts;
  m_smoothStroke.getSmoothPoints(pts);
  double pixelSize2 = getPixelSize() * getPixelSize();
  for (size_t i = 0; i < pts.size(); ++i) {
    m_track.add(pts[i], pixelSize2);
  }
}

//---------------------------------------------------------------------------------------------------------------
/*!
 * ドラッグ中にツールが切り替わった場合に備え、onDeactivate時とMouseRelease時にと同じ終了処理を行う
*/
void BrushTool::finishRasterBrush(const TPointD &pos, int pressureVal) {
  TImageP image   = getImage(true);
  TToonzImageP ti = image;
  if (!ti) return;

  TPointD rasCenter         = ti->getRaster()->getCenterD();
  TTool::Application *app   = TTool::getApplication();
  TXshLevel *level          = app->getCurrentLevel()->getLevel();
  TXshSimpleLevelP simLevel = level->getSimpleLevel();

  /*--
   * 描画中にカレントフレームが変わっても、描画開始時のFidに対してUndoを記録する
   * --*/
  TFrameId frameId =
      m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

  if (m_rasterTrack && (m_hardness.getValue() == 100 || m_pencil.getValue())) {
    double thickness =
        m_pressure.getValue()
            ? computeThickness(pressureVal, m_rasThickness, m_isPath)
            : m_rasThickness.getValue().second;

    /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
    if (m_pressure.getValue() && pressureVal == 255)
      thickness = m_rasThickness.getValue().first;

    /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる --*/
    if (!m_pencil.getValue()) thickness -= 1.0;

    TRectD invalidateRect;
    TThickPoint thickPoint(pos + rasCenter, thickness);
    m_smoothStroke.addPoint(thickPoint);
    m_smoothStroke.endStroke();
    std::vector<TThickPoint> pts;
    m_smoothStroke.getSmoothPoints(pts);
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint = pts[i];
      bool isAdded                  = m_rasterTrack->add(thickPoint);
      if (isAdded) {
        m_tileSaver->save(m_rasterTrack->getLastRect());
        m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue(), true);

        std::vector<TThickPoint> brushPoints =
            m_rasterTrack->getPointsSequence();
        int m = (int)brushPoints.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          points.push_back(brushPoints[0]);
          points.push_back(brushPoints[1]);
        } else {
          points.push_back(brushPoints[m - 4]);
          points.push_back(brushPoints[m - 3]);
          points.push_back(brushPoints[m - 2]);
        }
        int maxThickness = m_rasThickness.getValue().second;
        if (i == 0) {
          invalidateRect =
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        } else {
          invalidateRect +=
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        }
      }
    }
    invalidate(invalidateRect.enlarge(2));

    if (m_tileSet->getTileCount() > 0) {
      TUndoManager::manager()->add(new RasterBrushUndo(
          m_tileSet, m_rasterTrack->getPointsSequence(),
          m_rasterTrack->getStyleId(), m_rasterTrack->isSelective(),
          simLevel.getPointer(), frameId, m_pencil.getValue(), m_isFrameCreated,
          m_isLevelCreated));
    }
    delete m_rasterTrack;
    m_rasterTrack = 0;
  } else {
    if (m_points.size() != 1) {
      double maxThickness = m_rasThickness.getValue().second;
      double thickness =
          (m_pressure.getValue() || m_isPath)
              ? computeThickness(pressureVal, m_rasThickness, m_isPath)
              : maxThickness;
      TPointD rasCenter = ti->getRaster()->getCenterD();
      TRectD invalidateRect;
      bool rectUpdated = false;
      TThickPoint thickPoint(pos + rasCenter, thickness);
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.endStroke();
      std::vector<TThickPoint> pts;
      m_smoothStroke.getSmoothPoints(pts);
      for (size_t i = 0; i < pts.size() - 1; ++i) {
        TThickPoint old = m_points.back();
        if (norm2(pos - old) < 4) continue;

        const TThickPoint &point = pts[i];
        TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
        m_points.push_back(mid);
        m_points.push_back(point);

        TRect bbox;
        int m = (int)m_points.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          // ho appena cominciato. devo disegnare un segmento
          TThickPoint pa = m_points.front();
          points.push_back(pa);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
          m_lastRect += bbox;
        } else {
          points.push_back(m_points[m - 4]);
          points.push_back(old);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(m_points[m - 4], old, mid, 1, 1);
          m_lastRect += bbox;
        }
        if (!rectUpdated) {
          invalidateRect =
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
          rectUpdated = true;
        } else {
          invalidateRect +=
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        }

        m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                     m_styleId, m_selective.getValue());
        m_strokeRect += bbox;
      }
      if (pts.size() > 0) {
        TThickPoint point = pts.back();
        m_points.push_back(point);
        int m = m_points.size();
        std::vector<TThickPoint> points;
        points.push_back(m_points[m - 3]);
        points.push_back(m_points[m - 2]);
        points.push_back(m_points[m - 1]);
        TRect bbox = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(points[0], points[1], points[2], 1, 1);
        m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                     m_styleId, m_selective.getValue());

        if (!rectUpdated) {
          invalidateRect =
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        } else {
          invalidateRect +=
              ToolUtils::getBounds(points, maxThickness) - rasCenter;
        }
        m_lastRect += bbox;
        m_strokeRect += bbox;
      }
      invalidate(invalidateRect.enlarge(2));
      m_lastRect.empty();
    }
    delete m_bluredBrush;
    m_bluredBrush = 0;

    if (m_tileSet->getTileCount() > 0) {
      TUndoManager::manager()->add(new RasterBluredBrushUndo(
          m_tileSet, m_points, m_styleId, m_selective.getValue(),
          simLevel.getPointer(), frameId, m_rasThickness.getValue().second,
          m_hardness.getValue() * 0.01, m_isFrameCreated, m_isLevelCreated));
    }
  }
  delete m_tileSaver;

  m_tileSaver = 0;

  /*-- FIdを指定して、描画中にフレームが動いても、
  　　描画開始時のFidのサムネイルが更新されるようにする。--*/
  notifyImageChanged(frameId);

  m_strokeRect.empty();

  ToolUtils::updateSaveBox();

  /*-- 作業中のフレームをリセット --*/
  m_workingFrameId = TFrameId();
}
//---------------------------------------------------------------------------------------------------------------

void BrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

  struct Locals {
    BrushTool *m_this;

    void setValue(TDoublePairProperty &prop,
                  const TDoublePairProperty::Value &value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TDoublePairProperty &prop, double add) {
      if (add == 0.0) return;
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.first  = tcrop(value.first + add, range.first, range.second);
      value.second = tcrop(value.second + add, range.first, range.second);

      setValue(prop, value);
    }

    void addMinMaxSeparate(TDoublePairProperty &prop, double min, double max) {
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

  // if (e.isAltPressed() && !e.isCtrlPressed()) {
  // const TPointD &diff = pos - m_mousePos;
  // double add = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

  // locals.addMinMax(
  //  TToonzImageP(getImage(false, 1)) ? m_rasThickness : m_thickness, add);
  //} else
  if (e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed()) {
    const TPointD &diff = pos - m_mousePos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(
        (m_targetType & TTool::ToonzImage) ? m_rasThickness : m_thickness, min,
        max);
  } else {
    m_brushPos = pos;

    m_mousePos       = pos;
    m_firstSnapPoint = pos;
    m_foundFirstSnap = false;

    checkStrokeSnapping(true);
    checkGuideSnapping(true);
    m_brushPos = m_firstSnapPoint;
  }
  invalidate();

  if (m_minThick == 0 && m_maxThick == 0) {
    if (m_targetType & TTool::ToonzImage) {
      m_minThick = m_rasThickness.getValue().first;
      m_maxThick = m_rasThickness.getValue().second;
    } else {
      m_minThick = m_thickness.getValue().first;
      m_maxThick = m_thickness.getValue().second;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::checkStrokeSnapping(bool beforeMousePress) {
  if (Preferences::instance()->getVectorSnappingTarget() == 1) return;

  TVectorImageP vi(getImage(false));
  if (vi && m_snap.getValue()) {
    m_dragDraw          = true;
    double minDistance2 = m_minDistance2;
    if (beforeMousePress)
      m_strokeIndex1 = -1;
    else
      m_strokeIndex2 = -1;
    int i, strokeNumber = vi->getStrokeCount();
    TStroke *stroke;
    double distance2, outW;

    for (i = 0; i < strokeNumber; i++) {
      stroke = vi->getStroke(i);
      if (stroke->getNearestW(m_mousePos, outW, distance2) &&
          distance2 < minDistance2) {
        minDistance2                      = distance2;
        beforeMousePress ? m_strokeIndex1 = i : m_strokeIndex2 = i;
        if (areAlmostEqual(outW, 0.0, 1e-3))
          beforeMousePress ? m_w1 = 0.0 : m_w2 = 0.0;
        else if (areAlmostEqual(outW, 1.0, 1e-3))
          beforeMousePress ? m_w1 = 1.0 : m_w2 = 1.0;
        else
          beforeMousePress ? m_w1 = outW : m_w2 = outW;
        TThickPoint point1;
        beforeMousePress ? point1 = stroke->getPoint(m_w1)
                         : point1 = stroke->getPoint(m_w2);
        if (beforeMousePress) {
          m_firstSnapPoint = TPointD(point1.x, point1.y);
          m_foundFirstSnap = true;
        } else {
          m_lastSnapPoint                 = TPointD(point1.x, point1.y);
          m_foundLastSnap                 = true;
          if (distance2 < 2.0) m_dragDraw = false;
        }
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::checkGuideSnapping(bool beforeMousePress) {
  if (Preferences::instance()->getVectorSnappingTarget() == 0) return;
  bool foundSnap;
  TPointD snapPoint;
  beforeMousePress ? foundSnap = m_foundFirstSnap : foundSnap = m_foundLastSnap;
  beforeMousePress ? snapPoint = m_firstSnapPoint : snapPoint = m_lastSnapPoint;
  if ((m_targetType & TTool::Vectors) && m_snap.getValue()) {
    // check guide snapping
    int vGuideCount = 0, hGuideCount = 0;
    double guideDistance  = sqrt(m_minDistance2);
    TTool::Viewer *viewer = getViewer();
    if (viewer) {
      vGuideCount = viewer->getVGuideCount();
      hGuideCount = viewer->getHGuideCount();
    }
    double distanceToVGuide = -1.0, distanceToHGuide = -1.0;
    double vGuide, hGuide;
    bool useGuides = false;
    if (vGuideCount) {
      for (int j = 0; j < vGuideCount; j++) {
        double guide        = viewer->getVGuide(j);
        double tempDistance = abs(guide - m_mousePos.y);
        if (tempDistance < guideDistance &&
            (distanceToVGuide < 0 || tempDistance < distanceToVGuide)) {
          distanceToVGuide = tempDistance;
          vGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (hGuideCount) {
      for (int j = 0; j < hGuideCount; j++) {
        double guide        = viewer->getHGuide(j);
        double tempDistance = abs(guide - m_mousePos.x);
        if (tempDistance < guideDistance &&
            (distanceToHGuide < 0 || tempDistance < distanceToHGuide)) {
          distanceToHGuide = tempDistance;
          hGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (useGuides && foundSnap) {
      double currYDistance = abs(snapPoint.y - m_mousePos.y);
      double currXDistance = abs(snapPoint.x - m_mousePos.x);
      double hypotenuse =
          sqrt(pow(currYDistance, 2.0) + pow(currXDistance, 2.0));
      if ((distanceToVGuide >= 0 && distanceToVGuide < hypotenuse) ||
          (distanceToHGuide >= 0 && distanceToHGuide < hypotenuse))
        useGuides = true;
      else
        useGuides = false;
    }
    if (useGuides) {
      assert(distanceToHGuide >= 0 || distanceToVGuide >= 0);
      if (distanceToHGuide < 0 ||
          (distanceToVGuide <= distanceToHGuide && distanceToVGuide >= 0)) {
        snapPoint.y = vGuide;
        snapPoint.x = m_mousePos.x;

      } else {
        snapPoint.y = m_mousePos.y;
        snapPoint.x = hGuide;
      }
      beforeMousePress ? m_foundFirstSnap = true : m_foundLastSnap = true;
      beforeMousePress ? m_firstSnapPoint = snapPoint : m_lastSnapPoint =
                                                            snapPoint;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::draw() {
  /*--ショートカットでのツール切り替え時に赤点が描かれるのを防止する--*/
  if (m_minThick == 0 && m_maxThick == 0 &&
      !Preferences::instance()->getShow0ThickLines())
    return;

  TImageP img = getImage(false, 1);

  // Draw track
  tglColor(m_isPrompting ? TPixel32::Green : m_currentColor);
  m_track.drawAllFragments();

  // snapping
  TVectorImageP vi = img;
  if ((m_targetType & TTool::Vectors) && m_snap.getValue()) {
    m_pixelSize  = getPixelSize();
    double thick = 6.0 * m_pixelSize;
    if (m_foundFirstSnap) {
      tglColor(TPixelD(0.1, 0.9, 0.1));
      tglDrawCircle(m_firstSnapPoint, thick);
    }

    TThickPoint point2;

    if (m_foundLastSnap) {
      tglColor(TPixelD(0.1, 0.9, 0.1));
      tglDrawCircle(m_lastSnapPoint, thick);
    }
  }

  // frame range
  if (m_firstStroke) {
    glColor3d(1.0, 0.0, 0.0);
    m_rangeTrack.drawAllFragments();
    glColor3d(0.0, 0.6, 0.0);
    TPointD firstPoint        = m_rangeTrack.getFirstPoint();
    TPointD topLeftCorner     = TPointD(firstPoint.x - 5, firstPoint.y - 5);
    TPointD topRightCorner    = TPointD(firstPoint.x + 5, firstPoint.y - 5);
    TPointD bottomLeftCorner  = TPointD(firstPoint.x - 5, firstPoint.y + 5);
    TPointD bottomRightCorner = TPointD(firstPoint.x + 5, firstPoint.y + 5);
    tglDrawSegment(topLeftCorner, bottomRightCorner);
    tglDrawSegment(topRightCorner, bottomLeftCorner);
  }

  if (getApplication()->getCurrentObject()->isSpline()) return;

  // Draw the brush outline - change color when the Ink / Paint check is
  // activated
  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
    glColor3d(0.5, 0.8, 0.8);
  // normally draw in red
  else
    glColor3d(1.0, 0.0, 0.0);

  if (TToonzImageP ti = img) {
    TRasterP ras = ti->getRaster();
    int lx       = ras->getLx();
    int ly       = ras->getLy();

    drawEmptyCircle(m_brushPos, tround(m_minThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
    drawEmptyCircle(m_brushPos, tround(m_maxThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
  } else {
    tglDrawCircle(m_brushPos, 0.5 * m_minThick);
    tglDrawCircle(m_brushPos, 0.5 * m_maxThick);
  }
}

//--------------------------------------------------------------------------------------------------------------

void BrushTool::onEnter() {
  TImageP img = getImage(false);

  if (TToonzImageP(img)) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  } else {
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
  }

  Application *app = getApplication();

  m_styleId       = app->getCurrentLevelStyleIndex();
  TColorStyle *cs = app->getCurrentLevelStyle();
  if (cs) {
    TRasterStyleFx *rfx = cs->getRasterStyleFx();
    m_active            = cs->isStrokeStyle() || (rfx && rfx->isInkStyle());
    m_currentColor      = cs->getAverageColor();
    m_currentColor.m    = 255;
  } else {
    m_currentColor = TPixel32::Black;
  }
  m_active = img;
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::onLeave() {
  m_minThick = 0;
  m_maxThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *BrushTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();

  return &m_prop[idx];
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::onImageChanged() {
  TToonzImageP ti = (TToonzImageP)getImage(false, 1);
  if (!ti || !isEnabled()) return;

  setWorkAndBackupImages();
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::setWorkAndBackupImages() {
  TToonzImageP ti = (TToonzImageP)getImage(false, 1);
  if (!ti) return;

  TRasterP ras   = ti->getRaster();
  TDimension dim = ras->getSize();

  double hardness = m_hardness.getValue() * 0.01;
  if (hardness == 1.0 && ras->getPixelSize() == 4) {
    m_workRas   = TRaster32P();
    m_backupRas = TRasterCM32P();
  } else {
    if (!m_workRas || m_workRas->getLx() > dim.lx ||
        m_workRas->getLy() > dim.ly)
      m_workRas = TRaster32P(dim);
    if (!m_backupRas || m_backupRas->getLx() > dim.lx ||
        m_backupRas->getLy() > dim.ly)
      m_backupRas = TRasterCM32P(dim);

    m_strokeRect.empty();
    m_lastRect.empty();
  }
}

//------------------------------------------------------------------

void BrushTool::resetFrameRange() {
  m_rangeTrack.clear();
  m_firstFrameId = -1;
  if (m_firstStroke) {
    delete m_firstStroke;
    m_firstStroke = 0;
  }
  m_firstFrameRange = true;
}

//------------------------------------------------------------------

bool BrushTool::onPropertyChanged(std::string propertyName) {
  // Set the following to true whenever a different piece of interface must
  // be refreshed - done once at the end.
  bool notifyTool = false;

  /*--- 変更されたPropertyに合わせて処理を分ける ---*/

  /*--- m_thicknessとm_rasThicknessは同じName(="Size:")なので、
          扱っている画像がラスタかどうかで区別する---*/
  if (propertyName == m_thickness.getName()) {
    TImageP img = getImage(false);
    if (TToonzImageP(img))  // raster
    {
      RasterBrushMinSize = m_rasThickness.getValue().first;
      RasterBrushMaxSize = m_rasThickness.getValue().second;

      m_minThick = m_rasThickness.getValue().first;
      m_maxThick = m_rasThickness.getValue().second;
    } else  // vector
    {
      VectorBrushMinSize = m_thickness.getValue().first;
      VectorBrushMaxSize = m_thickness.getValue().second;

      m_minThick = m_thickness.getValue().first;
      m_maxThick = m_thickness.getValue().second;
    }
  } else if (propertyName == m_accuracy.getName()) {
    BrushAccuracy = m_accuracy.getValue();
  } else if (propertyName == m_smooth.getName()) {
    BrushSmooth = m_smooth.getValue();
  } else if (propertyName == m_preset.getName()) {
    loadPreset();
    notifyTool = true;
  } else if (propertyName == m_selective.getName()) {
    BrushSelective = m_selective.getValue();
  } else if (propertyName == m_breakAngles.getName()) {
    BrushBreakSharpAngles = m_breakAngles.getValue();
  } else if (propertyName == m_pencil.getName()) {
    RasterBrushPencilMode = m_pencil.getValue();
  } else if (propertyName == m_pressure.getName()) {
    BrushPressureSensitivity = m_pressure.getValue();
  } else if (propertyName == m_capStyle.getName()) {
    VectorCapStyle = m_capStyle.getIndex();
  } else if (propertyName == m_joinStyle.getName()) {
    VectorJoinStyle = m_joinStyle.getIndex();
  } else if (propertyName == m_miterJoinLimit.getName()) {
    VectorMiterValue = m_miterJoinLimit.getValue();
  } else if (propertyName == m_frameRange.getName()) {
    int index             = m_frameRange.getIndex();
    VectorBrushFrameRange = index;
    if (index == 0) resetFrameRange();
  } else if (propertyName == m_snap.getName()) {
    VectorBrushSnap = m_snap.getValue();
  } else if (propertyName == m_snapSensitivity.getName()) {
    int index                  = m_snapSensitivity.getIndex();
    VectorBrushSnapSensitivity = index;
    switch (index) {
    case 0:
      m_minDistance2 = SNAPPING_LOW;
      break;
    case 1:
      m_minDistance2 = SNAPPING_MEDIUM;
      break;
    case 2:
      m_minDistance2 = SNAPPING_HIGH;
      break;
    }
  }

  if (m_targetType & TTool::Vectors) {
    if (propertyName == m_joinStyle.getName()) notifyTool = true;
  }
  if (m_targetType & TTool::ToonzImage) {
    if (propertyName == m_hardness.getName()) setWorkAndBackupImages();
    if (propertyName == m_hardness.getName() ||
        propertyName == m_thickness.getName()) {
      m_brushPad = getBrushPad(m_rasThickness.getValue().second,
                               m_hardness.getValue() * 0.01);
      TRectD rect(m_mousePos - TPointD(m_maxThick + 2, m_maxThick + 2),
                  m_mousePos + TPointD(m_maxThick + 2, m_maxThick + 2));
      invalidate(rect);
    }
  }

  if (propertyName != m_preset.getName() &&
      m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    notifyTool = true;
  }

  if (notifyTool) getApplication()->getCurrentTool()->notifyToolChanged();

  return true;
}

//------------------------------------------------------------------

void BrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    if (getTargetType() & TTool::Vectors)
      m_presetsManager.load(TEnv::getConfigDir() + "brush_vector.txt");
    else
      m_presetsManager.load(TEnv::getConfigDir() + "brush_toonzraster.txt");
  }

  // Rebuild the presets property entries
  const std::set<BrushData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);

  std::set<BrushData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::loadPreset() {
  const std::set<BrushData> &presets = m_presetsManager.presets();
  std::set<BrushData>::const_iterator it;

  it = presets.find(BrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const BrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    if (getTargetType() & TTool::Vectors) {
      m_thickness.setValue(
          TDoublePairProperty::Value(preset.m_min, preset.m_max));
      m_accuracy.setValue(preset.m_acc, true);
      m_smooth.setValue(preset.m_smooth, true);
      m_breakAngles.setValue(preset.m_breakAngles);
      m_pressure.setValue(preset.m_pressure);
      m_capStyle.setIndex(preset.m_cap);
      m_joinStyle.setIndex(preset.m_join);
      m_miterJoinLimit.setValue(preset.m_miter);
    } else {
      m_rasThickness.setValue(TDoublePairProperty::Value(
          std::max(preset.m_min, 1.0), preset.m_max));
      m_brushPad =
          ToolUtils::getBrushPad(preset.m_max, preset.m_hardness * 0.01);
      m_smooth.setValue(preset.m_smooth, true);
      m_hardness.setValue(preset.m_hardness, true);
      m_selective.setValue(preset.m_selective);
      m_pencil.setValue(preset.m_pencil);
      m_pressure.setValue(preset.m_pressure);
    }
  } catch (...) {
  }
}

//------------------------------------------------------------------

void BrushTool::addPreset(QString name) {
  // Build the preset
  BrushData preset(name.toStdWString());

  if (getTargetType() & TTool::Vectors) {
    preset.m_min = m_thickness.getValue().first;
    preset.m_max = m_thickness.getValue().second;
  } else {
    preset.m_min = m_rasThickness.getValue().first;
    preset.m_max = m_rasThickness.getValue().second;
  }

  preset.m_acc             = m_accuracy.getValue();
  preset.m_smooth          = m_smooth.getValue();
  preset.m_hardness        = m_hardness.getValue();
  preset.m_selective       = m_selective.getValue();
  preset.m_pencil          = m_pencil.getValue();
  preset.m_breakAngles     = m_breakAngles.getValue();
  preset.m_pressure        = m_pressure.getValue();
  preset.m_cap             = m_capStyle.getIndex();
  preset.m_join            = m_joinStyle.getIndex();
  preset.m_miter           = m_miterJoinLimit.getValue();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
}

//------------------------------------------------------------------

void BrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
}

//------------------------------------------------------------------
/*!	Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す
*/
bool BrushTool::isPencilModeActive() {
  return getTargetType() == TTool::ToonzImage && m_pencil.getValue();
}

//==========================================================================================================

// Tools instantiation

BrushTool vectorPencil("T_Brush", TTool::Vectors | TTool::EmptyTarget);
BrushTool toonzPencil("T_Brush", TTool::ToonzImage | TTool::EmptyTarget);

//*******************************************************************************
//    Brush Data implementation
//*******************************************************************************

BrushData::BrushData()
    : m_name()
    , m_min(0.0)
    , m_max(0.0)
    , m_acc(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_selective(false)
    , m_pencil(false)
    , m_breakAngles(false)
    , m_pressure(false)
    , m_cap(0)
    , m_join(0)
    , m_miter(0)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0) {}

//----------------------------------------------------------------------------------------------------------

BrushData::BrushData(const std::wstring &name)
    : m_name(name)
    , m_min(0.0)
    , m_max(0.0)
    , m_acc(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_selective(false)
    , m_pencil(false)
    , m_breakAngles(false)
    , m_pressure(false)
    , m_cap(0)
    , m_join(0)
    , m_miter(0)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0) {}

//----------------------------------------------------------------------------------------------------------

void BrushData::saveData(TOStream &os) {
  os.openChild("Name");
  os << m_name;
  os.closeChild();
  os.openChild("Thickness");
  os << m_min << m_max;
  os.closeChild();
  os.openChild("Accuracy");
  os << m_acc;
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
  os.openChild("Selective");
  os << (int)m_selective;
  os.closeChild();
  os.openChild("Pencil");
  os << (int)m_pencil;
  os.closeChild();
  os.openChild("Break_Sharp_Angles");
  os << (int)m_breakAngles;
  os.closeChild();
  os.openChild("Pressure_Sensitivity");
  os << (int)m_pressure;
  os.closeChild();
  os.openChild("Cap");
  os << m_cap;
  os.closeChild();
  os.openChild("Join");
  os << m_join;
  os.closeChild();
  os.openChild("Miter");
  os << m_miter;
  os.closeChild();
  os.openChild("Modifier_Size");
  os << m_modifierSize;
  os.closeChild();
  os.openChild("Modifier_Opacity");
  os << m_modifierOpacity;
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
    else if (tagName == "Accuracy")
      is >> m_acc, is.matchEndTag();
    else if (tagName == "Smooth")
      is >> m_smooth, is.matchEndTag();
    else if (tagName == "Hardness")
      is >> m_hardness, is.matchEndTag();
    else if (tagName == "Opacity")
      is >> m_opacityMin >> m_opacityMax, is.matchEndTag();
    else if (tagName == "Selective")
      is >> val, m_selective = val, is.matchEndTag();
    else if (tagName == "Pencil")
      is >> val, m_pencil = val, is.matchEndTag();
    else if (tagName == "Break_Sharp_Angles")
      is >> val, m_breakAngles = val, is.matchEndTag();
    else if (tagName == "Pressure_Sensitivity")
      is >> val, m_pressure = val, is.matchEndTag();
    else if (tagName == "Cap")
      is >> m_cap, is.matchEndTag();
    else if (tagName == "Join")
      is >> m_join, is.matchEndTag();
    else if (tagName == "Miter")
      is >> m_miter, is.matchEndTag();
    else if (tagName == "Modifier_Size")
      is >> m_modifierSize, is.matchEndTag();
    else if (tagName == "Modifier_Opacity")
      is >> m_modifierOpacity, is.matchEndTag();
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
