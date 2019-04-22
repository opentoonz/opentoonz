

#include "toonzvectorbrushtool.h"

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

TEnv::DoubleVar V_VectorBrushMinSize("InknpaintVectorBrushMinSize", 1);
TEnv::DoubleVar V_VectorBrushMaxSize("InknpaintVectorBrushMaxSize", 5);
TEnv::IntVar V_VectorCapStyle("InknpaintVectorCapStyle", 1);
TEnv::IntVar V_VectorJoinStyle("InknpaintVectorJoinStyle", 1);
TEnv::IntVar V_VectorMiterValue("InknpaintVectorMiterValue", 4);
TEnv::DoubleVar V_BrushAccuracy("InknpaintBrushAccuracy", 20);
TEnv::DoubleVar V_BrushSmooth("InknpaintBrushSmooth", 0);
TEnv::IntVar V_BrushBreakSharpAngles("InknpaintBrushBreakSharpAngles", 0);
TEnv::IntVar V_BrushPressureSensitivity("InknpaintBrushPressureSensitivity", 1);
TEnv::IntVar V_VectorBrushFrameRange("VectorBrushFrameRange", 0);
TEnv::IntVar V_VectorBrushSnap("VectorBrushSnap", 0);
TEnv::IntVar V_VectorBrushSnapSensitivity("VectorBrushSnapSensitivity", 0);

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

void addStrokeToImage(TTool::Application *application, const TVectorImageP &vi,
                      TStroke *stroke, bool breakAngles, bool frameCreated,
                      bool levelCreated, TXshSimpleLevel *sLevel = NULL,
                      TFrameId id = TFrameId::NO_FRAME) {
  QMutexLocker lock(vi->getMutex());
  addStroke(application, vi.getPointer(), stroke, breakAngles, frameCreated,
            levelCreated, sLevel, id);
  // la notifica viene gia fatta da addStroke!
  // getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//---------------------------------------------------------------------------------------------------------

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

double computeThickness(double pressure, const TDoublePairProperty &property,
                        bool isPath) {
  if (isPath) return 0.0;
  double t                    = pressure * pressure * pressure;
  double thick0               = property.getValue().first;
  double thick1               = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t) * 0.5;
}

}  // namespace

//===================================================================
//
// ToonzVectorBrushTool
//
//-----------------------------------------------------------------------------

ToonzVectorBrushTool::ToonzVectorBrushTool(std::string name, int targetType)
    : TTool(name)
    , m_thickness("Size", 0, 100, 0, 5)
    , m_accuracy("Accuracy:", 1, 100, 20)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_preset("Preset:")
    , m_breakAngles("Break", true)
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
  m_prop[0].bind(m_thickness);
  m_prop[0].bind(m_accuracy);
  m_prop[0].bind(m_smooth);
  m_prop[0].bind(m_breakAngles);
  m_prop[0].bind(m_pressure);

  m_prop[0].bind(m_frameRange);
  m_frameRange.addValue(L"Off");
  m_frameRange.addValue(LINEAR_WSTR);
  m_frameRange.addValue(EASEIN_WSTR);
  m_frameRange.addValue(EASEOUT_WSTR);
  m_frameRange.addValue(EASEINOUT_WSTR);

  m_prop[0].bind(m_snap);

  m_prop[0].bind(m_snapSensitivity);
  m_snapSensitivity.addValue(LOW_WSTR);
  m_snapSensitivity.addValue(MEDIUM_WSTR);
  m_snapSensitivity.addValue(HIGH_WSTR);

  m_prop[0].bind(m_preset);
  m_preset.addValue(CUSTOM_WSTR);

  m_prop[1].bind(m_capStyle);
  m_capStyle.addValue(BUTT_WSTR, QString::fromStdWString(BUTT_WSTR));
  m_capStyle.addValue(ROUNDC_WSTR, QString::fromStdWString(ROUNDC_WSTR));
  m_capStyle.addValue(PROJECTING_WSTR,
                      QString::fromStdWString(PROJECTING_WSTR));

  m_prop[1].bind(m_joinStyle);
  m_joinStyle.addValue(MITER_WSTR, QString::fromStdWString(MITER_WSTR));
  m_joinStyle.addValue(ROUNDJ_WSTR, QString::fromStdWString(ROUNDJ_WSTR));
  m_joinStyle.addValue(BEVEL_WSTR, QString::fromStdWString(BEVEL_WSTR));

  m_prop[1].bind(m_miterJoinLimit);

  m_breakAngles.setId("BreakSharpAngles");
  m_frameRange.setId("FrameRange");
  m_snap.setId("Snap");
  m_snapSensitivity.setId("SnapSensitivity");
  m_preset.setId("BrushPreset");
  m_pressure.setId("PressureSensitivity");
  m_capStyle.setId("Cap");
  m_joinStyle.setId("Join");
  m_miterJoinLimit.setId("Miter");
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *ToonzVectorBrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//-------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::updateTranslation() {
  m_thickness.setQStringName(tr("Size"));
  m_accuracy.setQStringName(tr("Accuracy:"));
  m_smooth.setQStringName(tr("Smooth:"));
  m_preset.setQStringName(tr("Preset:"));
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));
  m_breakAngles.setQStringName(tr("Break"));
  m_pressure.setQStringName(tr("Pressure"));
  m_capStyle.setQStringName(tr("Cap"));
  m_joinStyle.setQStringName(tr("Join"));
  m_miterJoinLimit.setQStringName(tr("Miter:"));
  m_frameRange.setQStringName(tr("Range:"));
  m_snap.setQStringName(tr("Snap"));
  m_snapSensitivity.setQStringName("");
  m_frameRange.setItemUIName(L"Off", tr("Off"));
  m_frameRange.setItemUIName(LINEAR_WSTR, tr("Linear"));
  m_frameRange.setItemUIName(EASEIN_WSTR, tr("In"));
  m_frameRange.setItemUIName(EASEOUT_WSTR, tr("Out"));
  m_frameRange.setItemUIName(EASEINOUT_WSTR, tr("In&Out"));
  m_snapSensitivity.setItemUIName(LOW_WSTR, tr("Low"));
  m_snapSensitivity.setItemUIName(MEDIUM_WSTR, tr("Med"));
  m_snapSensitivity.setItemUIName(HIGH_WSTR, tr("High"));
  m_capStyle.setItemUIName(BUTT_WSTR, tr("Butt cap"));
  m_capStyle.setItemUIName(ROUNDC_WSTR, tr("Round cap"));
  m_capStyle.setItemUIName(PROJECTING_WSTR, tr("Projecting cap"));
  m_joinStyle.setItemUIName(MITER_WSTR, tr("Miter join"));
  m_joinStyle.setItemUIName(ROUNDJ_WSTR, tr("Round join"));
  m_joinStyle.setItemUIName(BEVEL_WSTR, tr("Bevel join"));
}

//---------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::onActivate() {
  if (m_firstTime) {
    m_thickness.setValue(
        TDoublePairProperty::Value(V_VectorBrushMinSize, V_VectorBrushMaxSize));

    m_capStyle.setIndex(V_VectorCapStyle);
    m_joinStyle.setIndex(V_VectorJoinStyle);
    m_miterJoinLimit.setValue(V_VectorMiterValue);
    m_breakAngles.setValue(V_BrushBreakSharpAngles ? 1 : 0);
    m_accuracy.setValue(V_BrushAccuracy);

    m_pressure.setValue(V_BrushPressureSensitivity ? 1 : 0);
    m_firstTime = false;
    m_smooth.setValue(V_BrushSmooth);

    m_frameRange.setIndex(V_VectorBrushFrameRange);
    m_snap.setValue(V_VectorBrushSnap);
    m_snapSensitivity.setIndex(V_VectorBrushSnapSensitivity);
    switch (V_VectorBrushSnapSensitivity) {
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
  resetFrameRange();
  // TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::onDeactivate() {
  /*---
  * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
  * ---*/
  if (m_tileSaver && !m_isPath) {
    m_enabled = false;
  }
  m_workRas   = TRaster32P();
  m_backupRas = TRasterCM32P();
  resetFrameRange();
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::preLeftButtonDown() {
  touchImage();
  if (m_isFrameCreated) {
    // When the xsheet frame is selected, whole viewer will be updated from
    // SceneViewer::onXsheetChanged() on adding a new frame.
    // We need to take care of a case when the level frame is selected.
    if (m_application->getCurrentFrame()->isEditingLevel()) invalidate();
  }
  return true;
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::leftButtonDown(const TPointD &pos,
                                          const TMouseEvent &e) {
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
  m_track.clear();
  double thickness = (m_pressure.getValue() || m_isPath)
                         ? computeThickness(e.m_pressure, m_thickness, m_isPath)
                         : m_thickness.getValue().second * 0.5;

  /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
  if (m_pressure.getValue() && e.m_pressure == 1.0)
    thickness     = m_thickness.getValue().first * 0.5;
  m_currThickness = thickness;
  m_smoothStroke.beginStroke(m_smooth.getValue());

  if (m_foundFirstSnap) {
    addTrackPoint(TThickPoint(m_firstSnapPoint, thickness),
                  getPixelSize() * getPixelSize());
  } else
    addTrackPoint(TThickPoint(pos, thickness), getPixelSize() * getPixelSize());
  TRectD invalidateRect = m_track.getLastModifiedRegion();
  invalidate(invalidateRect.enlarge(2));

  // updating m_brushPos is needed to refresh viewer properly
  m_brushPos = m_mousePos = pos;
}

//-------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::leftButtonDrag(const TPointD &pos,
                                          const TMouseEvent &e) {
  if (!m_enabled || !m_active) {
    m_brushPos = m_mousePos = pos;
    return;
  }
  double thickness = (m_pressure.getValue() || m_isPath)
                         ? computeThickness(e.m_pressure, m_thickness, m_isPath)
                         : m_thickness.getValue().second * 0.5;

  TRectD invalidateRect;
  TPointD halfThick(m_maxThick * 0.5, m_maxThick * 0.5);
  TPointD snapThick(6.0 * m_pixelSize, 6.0 * m_pixelSize);

  // In order to clear the previous brush tip
  invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);

  // In order to clear the previous snap indicator
  if (m_foundLastSnap)
    invalidateRect +=
        TRectD(m_lastSnapPoint - snapThick, m_lastSnapPoint + snapThick);

  m_currThickness = thickness;

  m_mousePos       = pos;
  m_lastSnapPoint  = pos;
  m_foundLastSnap  = false;
  m_foundFirstSnap = false;
  m_snapSelf       = false;
  m_altPressed     = e.isAltPressed() && !e.isCtrlPressed();

  checkStrokeSnapping(false, m_altPressed);
  checkGuideSnapping(false, m_altPressed);
  m_brushPos = m_lastSnapPoint;

  if (m_foundLastSnap)
    invalidateRect +=
        TRectD(m_lastSnapPoint - snapThick, m_lastSnapPoint + snapThick);

  if (e.isShiftPressed()) {
    m_smoothStroke.clearPoints();
    m_track.add(TThickPoint(m_brushPos, thickness),
                getPixelSize() * getPixelSize());
    m_track.removeMiddlePoints();
    invalidateRect += m_track.getModifiedRegion();
  } else if (m_dragDraw) {
    addTrackPoint(TThickPoint(pos, thickness), getPixelSize() * getPixelSize());
    invalidateRect += m_track.getLastModifiedRegion();
  }

  // In order to draw the current brush tip
  invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);

  if (!invalidateRect.isEmpty()) {
    // for motion path, call the invalidate function directry to ignore dpi of
    // the current level
    if (m_isPath)
      m_viewer->GLInvalidateRect(invalidateRect.enlarge(2));
    else
      invalidate(invalidateRect.enlarge(2));
  }
}

//---------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::leftButtonUp(const TPointD &pos,
                                        const TMouseEvent &e) {
  bool isValid = m_enabled && m_active;
  m_enabled    = false;

  if (!isValid) {
    // in case the current frame is moved to empty cell while dragging
    if (!m_track.isEmpty()) {
      m_track.clear();
      invalidate();
    }
    return;
  }

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

    TVectorImageP vi = getImage(true);
    struct Cleanup {
      ToonzVectorBrushTool *m_this;
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

    return;
  }

  TVectorImageP vi = getImage(true);
  if (m_track.isEmpty()) {
    m_styleId = 0;
    m_track.clear();
    return;
  }

  if (vi && (m_snap.getValue() != m_altPressed) && m_foundLastSnap) {
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
    invalidate();
  } else {
    if (m_snapSelf) {
      stroke->setSelfLoop(true);
      m_snapSelf = false;
    }
    addStrokeToImage(getApplication(), vi, stroke, m_breakAngles.getValue(),
                     m_isFrameCreated, m_isLevelCreated);
    TRectD bbox = stroke->getBBox().enlarge(2) + m_track.getModifiedRegion();
    invalidate();  // should use bbox?
  }
  assert(stroke);
  m_track.clear();
  m_altPressed = false;
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    resetFrameRange();
  }
  return false;
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::doFrameRangeStrokes(TFrameId firstFrameId,
                                               TStroke *firstStroke,
                                               TFrameId lastFrameId,
                                               TStroke *lastStroke,
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
    std::swap(firstFrameId, lastFrameId);
    *first  = *lastStroke;
    *last   = *firstStroke;
    swapped = true;
  }

  firstImage->addStroke(first, false);
  lastImage->addStroke(last, false);
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
                         m_isLevelCreated, sl, fid);
    } else if (t == 1) {
      if (swapped && !drawFirstStroke) {
      } else
        addStrokeToImage(getApplication(), img, lastImage->getStroke(0),
                         m_breakAngles.getValue(), m_isFrameCreated,
                         m_isLevelCreated, sl, fid);
    } else {
      assert(firstImage->getStrokeCount() == 1);
      assert(lastImage->getStrokeCount() == 1);
      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(s);
      assert(vi->getStrokeCount() == 1);
      addStrokeToImage(getApplication(), img, vi->getStroke(0),
                       m_breakAngles.getValue(), m_isFrameCreated,
                       m_isLevelCreated, sl, fid);
    }
  }
  TUndoManager::manager()->endBlock();
  notifyImageChanged();
  return true;
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::addTrackPoint(const TThickPoint &point,
                                         double pixelSize2) {
  m_smoothStroke.addPoint(point);
  std::vector<TThickPoint> pts;
  m_smoothStroke.getSmoothPoints(pts);
  for (size_t i = 0; i < pts.size(); ++i) {
    m_track.add(pts[i], pixelSize2);
  }
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::flushTrackPoint() {
  m_smoothStroke.endStroke();
  std::vector<TThickPoint> pts;
  m_smoothStroke.getSmoothPoints(pts);
  double pixelSize2 = getPixelSize() * getPixelSize();
  for (size_t i = 0; i < pts.size(); ++i) {
    m_track.add(pts[i], pixelSize2);
  }
}

//---------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

  struct Locals {
    ToonzVectorBrushTool *m_this;

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

  TPointD halfThick(m_maxThick * 0.5, m_maxThick * 0.5);
  TRectD invalidateRect(m_brushPos - halfThick, m_brushPos + halfThick);

  if (e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed()) {
    const TPointD &diff = pos - m_mousePos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(m_thickness, min, max);

    double radius = m_thickness.getValue().second * 0.5;
    invalidateRect += TRectD(m_brushPos - TPointD(radius, radius),
                             m_brushPos + TPointD(radius, radius));

  } else {
    m_mousePos = pos;
    m_brushPos = pos;

    TPointD snapThick(6.0 * m_pixelSize, 6.0 * m_pixelSize);
    // In order to clear the previous snap indicator
    if (m_foundFirstSnap)
      invalidateRect +=
          TRectD(m_firstSnapPoint - snapThick, m_firstSnapPoint + snapThick);

    m_firstSnapPoint = pos;
    m_foundFirstSnap = false;
    m_altPressed     = e.isAltPressed() && !e.isCtrlPressed();
    checkStrokeSnapping(true, m_altPressed);
    checkGuideSnapping(true, m_altPressed);
    m_brushPos = m_firstSnapPoint;
    // In order to draw the snap indicator
    if (m_foundFirstSnap)
      invalidateRect +=
          TRectD(m_firstSnapPoint - snapThick, m_firstSnapPoint + snapThick);

    invalidateRect += TRectD(pos - halfThick, pos + halfThick);
  }

  invalidate(invalidateRect.enlarge(2));

  if (m_minThick == 0 && m_maxThick == 0) {
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
  }
}

//-------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::checkStrokeSnapping(bool beforeMousePress,
                                               bool invertCheck) {
  if (Preferences::instance()->getVectorSnappingTarget() == 1) return;

  TVectorImageP vi(getImage(false));
  bool checkSnap             = m_snap.getValue();
  if (invertCheck) checkSnap = !checkSnap;
  if (vi && checkSnap) {
    m_dragDraw          = true;
    double minDistance2 = m_minDistance2;
    if (beforeMousePress)
      m_strokeIndex1 = -1;
    else
      m_strokeIndex2 = -1;
    int i, strokeNumber = vi->getStrokeCount();
    TStroke *stroke;
    double distance2, outW;
    bool snapFound = false;
    TThickPoint point1;

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

        beforeMousePress ? point1 = stroke->getPoint(m_w1)
                         : point1 = stroke->getPoint(m_w2);
        snapFound                 = true;
      }
    }
    // compare to first point of current stroke
    if (beforeMousePress && snapFound) {
      m_firstSnapPoint = TPointD(point1.x, point1.y);
      m_foundFirstSnap = true;
    } else if (!beforeMousePress) {
      if (!snapFound) {
        TPointD tempPoint        = m_track.getFirstPoint();
        double distanceFromStart = tdistance2(m_mousePos, tempPoint);

        if (distanceFromStart < m_minDistance2) {
          point1     = tempPoint;
          distance2  = distanceFromStart;
          snapFound  = true;
          m_snapSelf = true;
        }
      }
      if (snapFound) {
        m_lastSnapPoint                 = TPointD(point1.x, point1.y);
        m_foundLastSnap                 = true;
        if (distance2 < 2.0) m_dragDraw = false;
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::checkGuideSnapping(bool beforeMousePress,
                                              bool invertCheck) {
  if (Preferences::instance()->getVectorSnappingTarget() == 0) return;
  bool foundSnap;
  TPointD snapPoint;
  beforeMousePress ? foundSnap = m_foundFirstSnap : foundSnap = m_foundLastSnap;
  beforeMousePress ? snapPoint = m_firstSnapPoint : snapPoint = m_lastSnapPoint;

  bool checkSnap             = m_snap.getValue();
  if (invertCheck) checkSnap = !checkSnap;

  if (checkSnap) {
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
          (distanceToHGuide >= 0 && distanceToHGuide < hypotenuse)) {
        useGuides  = true;
        m_snapSelf = false;
      } else
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
      beforeMousePress ? m_firstSnapPoint                          = snapPoint
                       : m_lastSnapPoint                           = snapPoint;
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::draw() {
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
  if (m_snap.getValue() != m_altPressed) {
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

  tglDrawCircle(m_brushPos, 0.5 * m_minThick);
  tglDrawCircle(m_brushPos, 0.5 * m_maxThick);
}

//--------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::onEnter() {
  TImageP img = getImage(false);

  m_minThick = m_thickness.getValue().first;
  m_maxThick = m_thickness.getValue().second;

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

void ToonzVectorBrushTool::onLeave() {
  m_minThick = 0;
  m_maxThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *ToonzVectorBrushTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();

  return &m_prop[idx];
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::resetFrameRange() {
  m_rangeTrack.clear();
  m_firstFrameId = -1;
  if (m_firstStroke) {
    delete m_firstStroke;
    m_firstStroke = 0;
  }
  m_firstFrameRange = true;
}

//------------------------------------------------------------------

bool ToonzVectorBrushTool::onPropertyChanged(std::string propertyName) {
  // Set the following to true whenever a different piece of interface must
  // be refreshed - done once at the end.
  bool notifyTool = false;

  /*--- 変更されたPropertyに合わせて処理を分ける ---*/

  /*--- determine which type of brush to be modified ---*/
  if (propertyName == m_thickness.getName()) {
    V_VectorBrushMinSize = m_thickness.getValue().first;
    V_VectorBrushMaxSize = m_thickness.getValue().second;
    m_minThick           = m_thickness.getValue().first;
    m_maxThick           = m_thickness.getValue().second;
  } else if (propertyName == m_accuracy.getName()) {
    V_BrushAccuracy = m_accuracy.getValue();
  } else if (propertyName == m_smooth.getName()) {
    V_BrushSmooth = m_smooth.getValue();
  } else if (propertyName == m_preset.getName()) {
    loadPreset();
    notifyTool = true;
  } else if (propertyName == m_breakAngles.getName()) {
    V_BrushBreakSharpAngles = m_breakAngles.getValue();
  } else if (propertyName == m_pressure.getName()) {
    V_BrushPressureSensitivity = m_pressure.getValue();
  } else if (propertyName == m_capStyle.getName()) {
    V_VectorCapStyle = m_capStyle.getIndex();
  } else if (propertyName == m_joinStyle.getName()) {
    V_VectorJoinStyle = m_joinStyle.getIndex();
  } else if (propertyName == m_miterJoinLimit.getName()) {
    V_VectorMiterValue = m_miterJoinLimit.getValue();
  } else if (propertyName == m_frameRange.getName()) {
    int index               = m_frameRange.getIndex();
    V_VectorBrushFrameRange = index;
    if (index == 0) resetFrameRange();
  } else if (propertyName == m_snap.getName()) {
    V_VectorBrushSnap = m_snap.getValue();
  } else if (propertyName == m_snapSensitivity.getName()) {
    int index                    = m_snapSensitivity.getIndex();
    V_VectorBrushSnapSensitivity = index;
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

  if (propertyName == m_joinStyle.getName()) notifyTool = true;

  if (propertyName != m_preset.getName() &&
      m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    notifyTool = true;
  }

  if (notifyTool) getApplication()->getCurrentTool()->notifyToolChanged();

  return true;
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(TEnv::getConfigDir() + "brush_vector.txt");
  }

  // Rebuild the presets property entries
  const std::set<VectorBrushData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<VectorBrushData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::loadPreset() {
  const std::set<VectorBrushData> &presets = m_presetsManager.presets();
  std::set<VectorBrushData>::const_iterator it;

  it = presets.find(VectorBrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const VectorBrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_thickness.setValue(
        TDoublePairProperty::Value(preset.m_min, preset.m_max));
    m_accuracy.setValue(preset.m_acc, true);
    m_smooth.setValue(preset.m_smooth, true);
    m_breakAngles.setValue(preset.m_breakAngles);
    m_pressure.setValue(preset.m_pressure);
    m_capStyle.setIndex(preset.m_cap);
    m_joinStyle.setIndex(preset.m_join);
    m_miterJoinLimit.setValue(preset.m_miter);

  } catch (...) {
  }
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::addPreset(QString name) {
  // Build the preset
  VectorBrushData preset(name.toStdWString());

  preset.m_min = m_thickness.getValue().first;
  preset.m_max = m_thickness.getValue().second;

  preset.m_acc         = m_accuracy.getValue();
  preset.m_smooth      = m_smooth.getValue();
  preset.m_breakAngles = m_breakAngles.getValue();
  preset.m_pressure    = m_pressure.getValue();
  preset.m_cap         = m_capStyle.getIndex();
  preset.m_join        = m_joinStyle.getIndex();
  preset.m_miter       = m_miterJoinLimit.getValue();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::removePreset() {
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
bool ToonzVectorBrushTool::isPencilModeActive() { return false; }

//==========================================================================================================

// Tools instantiation

ToonzVectorBrushTool vectorPencil("T_Brush",
                                  TTool::Vectors | TTool::EmptyTarget);

//*******************************************************************************
//    Brush Data implementation
//*******************************************************************************

VectorBrushData::VectorBrushData()
    : m_name()
    , m_min(0.0)
    , m_max(0.0)
    , m_acc(0.0)
    , m_smooth(0.0)
    , m_breakAngles(false)
    , m_pressure(false)
    , m_cap(0)
    , m_join(0)
    , m_miter(0) {}

//----------------------------------------------------------------------------------------------------------

VectorBrushData::VectorBrushData(const std::wstring &name)
    : m_name(name)
    , m_min(0.0)
    , m_max(0.0)
    , m_acc(0.0)
    , m_smooth(0.0)
    , m_breakAngles(false)
    , m_pressure(false)
    , m_cap(0)
    , m_join(0)
    , m_miter(0) {}

//----------------------------------------------------------------------------------------------------------

void VectorBrushData::saveData(TOStream &os) {
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
}

//----------------------------------------------------------------------------------------------------------

void VectorBrushData::loadData(TIStream &is) {
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
    else
      is.skipCurrentTag();
  }
}

//----------------------------------------------------------------------------------------------------------

PERSIST_IDENTIFIER(VectorBrushData, "VectorBrushData");

//*******************************************************************************
//    Brush Preset Manager implementation
//*******************************************************************************

void VectorBrushPresetManager::load(const TFilePath &fp) {
  m_fp = fp;

  std::string tagName;
  VectorBrushData data;

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

void VectorBrushPresetManager::save() {
  TOStream os(m_fp);

  os.openChild("version");
  os << 1 << 20;
  os.closeChild();

  os.openChild("brushes");

  std::set<VectorBrushData>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    os.openChild("brush");
    os << (TPersist &)*it;
    os.closeChild();
  }

  os.closeChild();
}

//------------------------------------------------------------------

void VectorBrushPresetManager::addPreset(const VectorBrushData &data) {
  m_presets.erase(data);  // Overwriting insertion
  m_presets.insert(data);
  save();
}

//------------------------------------------------------------------

void VectorBrushPresetManager::removePreset(const std::wstring &name) {
  m_presets.erase(VectorBrushData(name));
  save();
}
