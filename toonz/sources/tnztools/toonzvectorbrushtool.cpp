

#include "toonzvectorbrushtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tooloptions.h"
#include "tools/replicator.h"
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
#include "toonz/tonionskinmaskhandle.h"

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
TEnv::IntVar V_VectorBrushAssistants("VectorBrushAssistants", 1);
TEnv::StringVar V_VectorBrushPreset("VectorBrushPreset", "<custom>");

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
                      TStroke *stroke, bool breakAngles, bool autoGroup,
                      bool autoFill, bool frameCreated, bool levelCreated,
                      TXshSimpleLevel *sLevel = NULL,
                      TFrameId fid            = TFrameId::NO_FRAME) {
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
                                                  frameCreated, levelCreated,
                                                  autoGroup, autoFill));
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
                                                frameCreated, levelCreated,
                                                autoGroup, autoFill));
  }

  if (autoGroup && stroke->isSelfLoop()) {
    int index = vi->getStrokeCount() - 1;
    vi->group(index, 1);
    if (autoFill) {
      // to avoid filling other strokes, I enter into the new stroke group
      int currentGroup = vi->exitGroup();
      vi->enterGroup(index);
      vi->selectFill(stroke->getBBox().enlarge(1, 1), 0, stroke->getStyle(),
                     false, true, false);
      if (currentGroup != -1)
        vi->enterGroup(currentGroup);
      else
        vi->exitGroup();
    }
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
                      TStroke *stroke, bool breakAngles, bool autoGroup,
                      bool autoFill, bool frameCreated, bool levelCreated,
                      TXshSimpleLevel *sLevel = NULL,
                      TFrameId id             = TFrameId::NO_FRAME) {
  QMutexLocker lock(vi->getMutex());
  addStroke(application, vi.getPointer(), stroke, breakAngles, autoGroup,
            autoFill, frameCreated, levelCreated, sLevel, id);
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
                        bool enablePressure, bool isPath ) {
  if (isPath) return 0.0;
  if (!enablePressure) return property.getValue().second*0.5;
  double t      = pressure * pressure * pressure;
  double thick0 = property.getValue().first;
  double thick1 = property.getValue().second;
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
    , m_thickness("Size", 0, 1000, 0, 5)
    , m_accuracy("Accuracy:", 1, 100, 20)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_preset("Preset:")
    , m_breakAngles("Break", true)
    , m_pressure("Pressure", true)
    , m_snap("Snap", false)
    , m_frameRange("Range:")
    , m_snapSensitivity("Sensitivity:")
    , m_capStyle("Cap")
    , m_joinStyle("Join")
    , m_miterJoinLimit("Miter:", 0, 100, 4)
    , m_assistants("Assistants", true)
    , m_styleId()
    , m_minThick()
    , m_maxThick()
    , m_col()
    , m_firstFrame()
    , m_veryFirstFrame()
    , m_veryFirstCol()
    , m_targetType(targetType)
    , m_pixelSize()
    , m_minDistance2()
    , m_snapped()
    , m_snappedSelf()
    , m_active()
    , m_firstTime(true)
    , m_isPath()
    , m_presetsLoaded()
    , m_firstFrameRange(true)
    , m_propertyUpdating()
{
  bind(targetType);

  m_thickness.setNonLinearSlider();

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

  m_prop[0].bind(m_assistants);

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
  m_assistants.setId("Assistants");

  m_inputmanager.setHandler(this);
  m_modifierLine               = new TModifierLine();
  m_modifierTangents           = new TModifierTangents();
  m_modifierAssistants         = new TModifierAssistants();
  m_modifierSegmentation       = new TModifierSegmentation();
  m_modifierSmoothSegmentation = new TModifierSegmentation(TPointD(1, 1), 3);
  for(int i = 0; i < 3; ++i)
    m_modifierSmooth[i]        = new TModifierSmooth();
  m_modifierSimplify           = new TModifierSimplify();
#ifndef NDEBUG
  m_modifierTest = new TModifierTest();
#endif

  m_inputmanager.addModifier(
      TInputModifierP(m_modifierAssistants.getPointer()));
}

//-------------------------------------------------------------------------------------------------------

unsigned int ToonzVectorBrushTool::getToolHints() const {
  unsigned int h = TTool::getToolHints() & ~HintAssistantsAll;
  if (m_assistants.getValue()) {
    h |= HintReplicators;
    h |= HintReplicatorsPoints;
    h |= HintReplicatorsEnabled;
  }
  return h;
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
  m_assistants.setQStringName(tr("Assistants"));
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
    m_firstTime = false;

    std::wstring wpreset =
        QString::fromStdString(V_VectorBrushPreset.getValue()).toStdWString();
    if (wpreset != CUSTOM_WSTR) {
      initPresets();
      if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
      m_preset.setValue(wpreset);
      V_VectorBrushPreset = m_preset.getValueAsString();
      loadPreset();
    } else
      loadLastBrush();
  }
  resetFrameRange();
  updateModifiers();
  // TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::onDeactivate() {
  /*---
   * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
   * ---*/

  // End current stroke.
  m_inputmanager.finishTracks();
  resetFrameRange();
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::inputMouseMove(
  const TPointD &position, const TInputState &state )
{
  struct Locals {
    ToonzVectorBrushTool *m_this;

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

  TPointD halfThick(m_maxThick * 0.5, m_maxThick * 0.5);
  TRectD invalidateRect(m_brushPos - halfThick, m_brushPos + halfThick);

  bool alt     = state.isKeyPressed(TInputState::Key::alt);
  bool shift   = state.isKeyPressed(TInputState::Key::shift);
  bool control = state.isKeyPressed(TInputState::Key::control);
  
  if ( alt && control && !shift
    && Preferences::instance()->useCtrlAltToResizeBrushEnabled() )
  {
    // Resize the brush if CTRL+ALT is pressed and the preference is enabled.
    const TPointD &diff = position - m_mousePos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMax(m_thickness, min, max);

    double radius = m_thickness.getValue().second * 0.5;
    halfThick = TPointD(radius, radius);
  } else {
    m_brushPos = m_mousePos = position;
  }
  
  invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);

  if (m_minThick == 0 && m_maxThick == 0) {
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
  }

  invalidate(invalidateRect.enlarge(2));
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::deleteStrokes(StrokeList &strokes) {
  for(StrokeList::iterator i = strokes.begin(); i != strokes.end(); ++i)
    delete *i;
  strokes.clear();
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::copyStrokes(StrokeList &dst, const StrokeList &src) {
  deleteStrokes(dst);
  dst.reserve(src.size());
  for(StrokeList::const_iterator i = src.begin(); i != src.end(); ++i)
    dst.push_back(new TStroke(**i));
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::inputSetBusy(bool busy) {
  if (m_active == busy) return;
  
  if (busy) {
    
    // begin painting //////////////////////////
    
    m_styleId = 0;
    m_tracks.clear();
    
    TTool::Application *app = TTool::getApplication();
    if (!app)
      return;

    m_isPath = app->getCurrentObject()->isSpline();
    if (m_isPath) {
      m_currentColor = TPixel32::Red;
      m_active = true;
      return;
    }
    
    // todo: gestire autoenable
    if ( app->getCurrentColumn()->getColumnIndex() < 0
      && !app->getCurrentFrame()->isEditingLevel() )
      return;
    if (!getImage(true) || !touchImage())
      return;
    if (!app->getCurrentLevel()->getLevel())
      return;

    // nel caso che il colore corrente sia un cleanup/studiopalette color
    // oppure il colore di un colorfield
    if (TColorStyle *cs = app->getCurrentLevelStyle()) {
      TRasterStyleFx *rfx = cs->getRasterStyleFx();
      if (!cs->isStrokeStyle() && (!rfx || !rfx->isInkStyle()))
        return;
      m_styleId = app->getCurrentLevelStyleIndex();
      m_currentColor = cs->getAverageColor();
      m_currentColor.m = 255;
    } else {
      m_styleId = 1;
      m_currentColor = TPixel32::Black;
    }
    
    m_active = true;
    
    return; // painting has begun
  }
  
  
  // end painting //////////////////////////
  
  m_active = false;
  
  // clear tracks automatically when return from this function
  struct Cleanup {
    ToonzVectorBrushTool &owner;
    inline ~Cleanup() { owner.m_tracks.clear(); owner.invalidate(); }
  } cleanup = {*this};

  // remove empty tracks
  for(TrackList::iterator i = m_tracks.begin(); i != m_tracks.end(); )
    if (i->isEmpty()) i = m_tracks.erase(i); else ++i;
  
  if (m_tracks.empty())
    return;
  
  // make motion path (if need)
    
  if (m_isPath) {
    double error = 20.0 * m_pixelSize;

    m_tracks.resize(1);
    TStroke *stroke = m_tracks.front().makeStroke(error);

    TVectorImageP vi = getImage(true);

    if (!isJustCreatedSpline(vi.getPointer())) {
      m_currentColor = TPixel32::Green;
      invalidate();
      int ret = DVGui::MsgBox(
        QString("Are you sure you want to replace the motion path?"),
        QObject::tr("Yes"), QObject::tr("No"), 0 );
      if (ret != 1)
        return; // 1 here means "Yes" button
    }

    QMutexLocker lock(vi->getMutex());

    TUndo *undo = new UndoPath(
      getXsheet()->getStageObject(getObjectId())->getSpline() );

    while(vi->getStrokeCount() > 0) vi->deleteStroke(0);
    vi->addStroke(stroke, false);

    notifyImageChanged();
    TUndoManager::manager()->add(undo);

    return; // done with motion path
  }
  
  // paint regular strokes
  
  TVectorImageP vi = getImage(true);
  QMutexLocker lock(vi->getMutex());
  TTool::Application *app = TTool::getApplication();
  
  // prepare strokes
    
  StrokeList strokes;
  strokes.reserve(m_tracks.size());
  for(TrackList::iterator i = m_tracks.begin(); i != m_tracks.end(); ++i) {
    StrokeGenerator &track = *i;
    
    track.filterPoints();
    double error = 30.0/(1 + 0.5 * m_accuracy.getValue())*m_pixelSize;
    TStroke *stroke = track.makeStroke(error);
    
    stroke->setStyle(m_styleId);
    
    TStroke::OutlineOptions &options = stroke->outlineOptions();
    options.m_capStyle   = m_capStyle.getIndex();
    options.m_joinStyle  = m_joinStyle.getIndex();
    options.m_miterUpper = m_miterJoinLimit.getValue();

    if ( stroke->getControlPointCount() == 3
      && stroke->getControlPoint(0) != stroke->getControlPoint(2) )
                                        // gli stroke con solo 1 chunk vengono
                                        // fatti dal tape tool...e devono venir
                                        // riconosciuti come speciali di
                                        // autoclose proprio dal fatto che
                                        // hanno 1 solo chunk.
      stroke->insertControlPoints(0.5);
    
    if (!m_frameRange.getIndex() && track.getLoop())
      stroke->setSelfLoop(true);
    
    strokes.push_back(stroke);
  }

  // add stroke to image
  
  if (m_frameRange.getIndex()) {
    // frame range stroke
    if (m_firstFrameId == -1) {
      // remember strokes for first srame
      copyStrokes(m_firstStrokes, strokes);
      m_firstFrameId = getFrameId();
      m_rangeTracks  = m_tracks;
      
      if (app) {
        m_col        = app->getCurrentColumn()->getColumnIndex();
        m_firstFrame = app->getCurrentFrame()->getFrame();
      }
      
      if (m_firstFrameRange) {
        m_veryFirstCol     = m_col;
        m_veryFirstFrame   = m_firstFrame;
        m_veryFirstFrameId = m_firstFrameId;
      }
    } else
    if (m_firstFrameId == getFrameId()) {
      // painted of first frame agein, so
      // just replace the remembered strokes for first frame
      copyStrokes(m_firstStrokes, strokes);
      m_rangeTracks = m_tracks;
    } else {
      // paint frame range strokes
      TFrameId currentId = getFrameId();
      int curCol   = app ? app->getCurrentColumn()->getColumnIndex() : 0;
      int curFrame = app ? app->getCurrentFrame()->getFrame()        : 0;
      
      if (size_t count = std::min(m_firstStrokes.size(), strokes.size())) {
        TUndoManager::manager()->beginBlock();
        for(size_t i = 0; i < count; ++i)
          doFrameRangeStrokes(
              m_firstFrameId, m_firstStrokes[i], getFrameId(), strokes[i],
              m_frameRange.getIndex(), m_breakAngles.getValue(), false, false,
              m_firstFrameRange );
        TUndoManager::manager()->endBlock();
      }
      
      if (m_inputmanager.state.isKeyPressed(TInputState::Key::control)) {
        if (app && m_firstFrameId > currentId) {
          if (app->getCurrentFrame()->isEditingScene()) {
            app->getCurrentColumn()->setColumnIndex(curCol);
            app->getCurrentFrame()->setFrame(curFrame);
          } else {
            app->getCurrentFrame()->setFid(currentId);
          }
        }
        
        resetFrameRange();
        copyStrokes(m_firstStrokes, strokes);
        m_rangeTracks     = m_tracks;
        m_firstFrameId    = currentId;
        m_firstFrameRange = false;
      } else {
        if (app) {
          if (app->getCurrentFrame()->isEditingScene()) {
            app->getCurrentColumn()->setColumnIndex(m_veryFirstCol);
            app->getCurrentFrame()->setFrame(m_veryFirstFrame);
          } else {
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
          }
        }
        resetFrameRange();
      }
    }
  } else {
    // regular paint strokes
    TUndoManager::manager()->beginBlock();
    for(StrokeList::iterator i = strokes.begin(); i != strokes.end(); ++i) {
      TStroke *stroke = *i;
      addStrokeToImage(app, vi, stroke, m_breakAngles.getValue(),
                      false, false, m_isFrameCreated, m_isLevelCreated);

      if ((Preferences::instance()->getGuidedDrawingType() == 1 ||
          Preferences::instance()->getGuidedDrawingType() == 2) &&
          Preferences::instance()->getGuidedAutoInbetween())
      {
        TFrameId fId = getFrameId();
        doGuidedAutoInbetween(fId, vi, stroke, m_breakAngles.getValue(), false,
                              false, false);
        if (app->getCurrentFrame()->isEditingScene())
          app->getCurrentFrame()->setFrame( app->getCurrentFrame()->getFrameIndex() );
        else
          app->getCurrentFrame()->setFid(fId);
      }
    }
    TUndoManager::manager()->endBlock();
  }
  
  deleteStrokes(strokes);
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::inputPaintTracks(const TTrackList &tracks) {
  if (tracks.empty()) return;

  TRectD invalidateRect;

  size_t count = m_isPath ? 1 : tracks.size();
  m_tracks.resize(count);
  for(size_t i = 0; i < count; ++i) {
    const TTrack &track = *tracks[i];
    StrokeGenerator &gen = m_tracks[i];
    
    while(track.pointsRemoved) {
      gen.pop();
      --track.pointsRemoved;
    }
    
    while(track.pointsAdded) {
      const TTrackPoint &p = track.current();
      double t = computeThickness(p.pressure, m_thickness, m_pressure.getValue(), m_isPath);
      gen.add(TThickPoint(p.position, t), 0);
      --track.pointsAdded;
    }
    
    bool loop = m_snappedSelf
            && track.fixedFinished()
            && !track.empty()
            && areAlmostEqual(track.front().position, track.back().position);
    gen.setLoop(loop);
    
    invalidateRect += gen.getLastModifiedRegion();
    if (!i) {
      TPointD halfThick(m_maxThick * 0.5, m_maxThick * 0.5);
      invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);
      m_brushPos = m_mousePos = track.current().position;
      invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);
    }
  }
  
  if (!invalidateRect.isEmpty()) {
    if (m_isPath) {
      if (getViewer()) getViewer()->GLInvalidateRect(invalidateRect);
    } else {
      invalidate(invalidateRect.enlarge(2));
    }
  }
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::updateModifiers() {
  m_pixelSize = getPixelSize();
  int smoothRadius = (int)round(m_smooth.getValue());
  m_modifierAssistants->magnetism = m_assistants.getValue() ? 1 : 0;
  m_modifierSegmentation->setStep(TPointD(m_pixelSize, m_pixelSize));
  m_modifierSmoothSegmentation->setStep(TPointD(2*m_pixelSize, 2*m_pixelSize));
  m_modifierSimplify->step = 2*m_pixelSize;
  m_inputmanager.drawPreview = false;

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
  m_inputmanager.addModifier(TInputModifierP(m_modifierSimplify.getPointer()));
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::preLeftButtonDown() {
  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) return false;
  updateModifiers();
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

void ToonzVectorBrushTool::handleMouseEvent(MouseEventType type,
                                          const TPointD &pos,
                                          const TMouseEvent &e)
{
  TTimerTicks t = TToolTimer::ticks();
  bool alt      = e.getModifiersMask() & TMouseEvent::ALT_KEY;
  bool shift    = e.getModifiersMask() & TMouseEvent::SHIFT_KEY;
  bool control  = e.getModifiersMask() & TMouseEvent::CTRL_KEY;

  if (shift && type == ME_DOWN && e.button() == Qt::LeftButton && !m_active) {
    m_modifierAssistants->magnetism = 0;
    m_inputmanager.clearModifiers();
    m_inputmanager.addModifier(TInputModifierP(m_modifierLine.getPointer()));
    m_inputmanager.addModifier(TInputModifierP(m_modifierAssistants.getPointer()));
    m_inputmanager.addModifiers(m_modifierReplicate);
    m_inputmanager.addModifier(TInputModifierP(m_modifierSegmentation.getPointer()));
    m_inputmanager.addModifier(TInputModifierP(m_modifierSimplify.getPointer()));
    m_inputmanager.drawPreview = true;
  }

  if (alt != m_inputmanager.state.isKeyPressed(TKey::alt))
    m_inputmanager.keyEvent(alt, TKey::alt, t, nullptr);
  if (shift != m_inputmanager.state.isKeyPressed(TKey::shift))
    m_inputmanager.keyEvent(shift, TKey::shift, t, nullptr);
  if (control != m_inputmanager.state.isKeyPressed(TKey::control))
    m_inputmanager.keyEvent(control, TKey::control, t, nullptr);

  TPointD snappedPos = pos;
  bool pickerMode = getViewer() && getViewer()->getGuidedStrokePickerMode();
  bool snapInvert = alt && (!control || type == ME_MOVE || type == ME_DOWN);
  bool snapEnabled = !pickerMode && (snapInvert != m_snap.getValue());
  snap(pos, snapEnabled, m_active);
  if (m_snapped)
    snappedPos = m_snapPoint;
  if (m_snappedSelf && type == ME_UP)
    snappedPos = m_snapPointSelf;
  
  if (type == ME_MOVE) {
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    THoverList hovers(1, snappedPos);
    m_inputmanager.hoverEvent(hovers);
  } else
  if (pickerMode) {
    if (type == ME_DOWN) getViewer()->doPickGuideStroke(pos);
  } else {
    int    deviceId    = e.isTablet() ? 1 : 0;
    bool   hasPressure = e.isTablet();
    double pressure    = hasPressure ? e.m_pressure : 1.0;
    bool   final       = type == ME_UP;
    m_inputmanager.trackEvent(
      deviceId, 0, snappedPos, pressure, TPointD(), hasPressure, false, final, t);
    m_inputmanager.processTracks();
  }
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DOWN, pos, e);
}
void ToonzVectorBrushTool::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  handleMouseEvent(ME_DRAG, pos, e);
}
void ToonzVectorBrushTool::leftButtonUp(const TPointD &pos,
                                      const TMouseEvent &e) {
  handleMouseEvent(ME_UP, pos, e);
}
void ToonzVectorBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  handleMouseEvent(ME_MOVE, pos, e);
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape)
    resetFrameRange();
  return false;
}

//--------------------------------------------------------------------------------------------------

bool ToonzVectorBrushTool::doFrameRangeStrokes(
    TFrameId firstFrameId, TStroke *firstStroke, TFrameId lastFrameId,
    TStroke *lastStroke, int interpolationType, bool breakAngles,
    bool autoGroup, bool autoFill, bool drawFirstStroke, bool drawLastStroke,
    bool withUndo) {
  TXshSimpleLevel *sl =
      TTool::getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel();
  TStroke *first           = new TStroke();
  TStroke *last            = new TStroke();
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();

  bool swapped = firstFrameId > lastFrameId;
  if (swapped) {
    std::swap(firstFrameId, lastFrameId);
    *first = *lastStroke;
    *last  = *firstStroke;
  } else {
    *first = *firstStroke;
    *last  = *lastStroke;
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

  if (withUndo) TUndoManager::manager()->beginBlock();
  int row       = getApplication()->getCurrentFrame()->isEditingScene()
                      ? getApplication()->getCurrentFrame()->getFrameIndex()
                      : -1;
  TFrameId cFid = getApplication()->getCurrentFrame()->getFid();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFrameId <= fid && fid <= lastFrameId);

    // This is an attempt to divide the tween evenly
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    double s = t;
    switch (interpolationType) {
    case 1:  // LINEAR_WSTR
      break;
    case 2:  // EASEIN_WSTR
      s = t * (2 - t);
      break;  // s'(1) = 0
    case 3:   // EASEOUT_WSTR
      s = t * t;
      break;  // s'(0) = 0
    case 4:   // EASEINOUT_WSTR:
      s = t * t * (3 - 2 * t);
      break;  // s'(0) = s'(1) = 0
    }

    TTool::Application *app = TTool::getApplication();
    if (app) app->getCurrentFrame()->setFid(fid);

    TVectorImageP img = sl->getFrame(fid, true);
    if (t == 0) {
      if (!swapped && !drawFirstStroke) {
      } else
        addStrokeToImage(getApplication(), img, firstImage->getStroke(0),
                         breakAngles, autoGroup, autoFill, m_isFrameCreated,
                         m_isLevelCreated, sl, fid);
    } else if (t == 1) {
      if (swapped && !drawFirstStroke) {
      } else if (drawLastStroke)
        addStrokeToImage(getApplication(), img, lastImage->getStroke(0),
                         breakAngles, autoGroup, autoFill, m_isFrameCreated,
                         m_isLevelCreated, sl, fid);
    } else {
      assert(firstImage->getStrokeCount() == 1);
      assert(lastImage->getStrokeCount() == 1);
      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(s);
      assert(vi->getStrokeCount() == 1);
      addStrokeToImage(getApplication(), img, vi->getStroke(0), breakAngles,
                       autoGroup, autoFill, m_isFrameCreated, m_isLevelCreated,
                       sl, fid);
    }
  }
  if (row != -1)
    getApplication()->getCurrentFrame()->setFrame(row);
  else
    getApplication()->getCurrentFrame()->setFid(cFid);

  if (withUndo) TUndoManager::manager()->endBlock();
  notifyImageChanged();
  return true;
}

//--------------------------------------------------------------------------------------------------
bool ToonzVectorBrushTool::doGuidedAutoInbetween(
    TFrameId cFid, const TVectorImageP &cvi, TStroke *cStroke, bool breakAngles,
    bool autoGroup, bool autoFill, bool drawStroke) {
  TApplication *app = TTool::getApplication();

  if (cFid.isEmptyFrame() || cFid.isNoFrame() || !cvi || !cStroke) return false;

  TXshSimpleLevel *sl = app->getCurrentLevel()->getLevel()->getSimpleLevel();
  if (!sl) return false;

  int osBack  = -1;
  int osFront = -1;

  getViewer()->getGuidedFrameIdx(&osBack, &osFront);

  TFrameHandle *currentFrame = getApplication()->getCurrentFrame();
  bool resultBack            = false;
  bool resultFront           = false;
  TFrameId oFid;
  int cStrokeIdx = cvi->getStrokeCount();
  if (!drawStroke) cStrokeIdx--;

  TUndoManager::manager()->beginBlock();
  if (osBack != -1) {
    if (currentFrame->isEditingScene()) {
      TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
      int col      = app->getCurrentColumn()->getColumnIndex();
      if (xsh && col >= 0) {
        TXshCell cell = xsh->getCell(osBack, col);
        if (!cell.isEmpty()) oFid = cell.getFrameId();
      }
    } else
      oFid = sl->getFrameId(osBack);

    TVectorImageP fvi = sl->getFrame(oFid, false);
    int fStrokeCount  = fvi ? fvi->getStrokeCount() : 0;

    int strokeIdx = getViewer()->getGuidedBackStroke() != -1
                        ? getViewer()->getGuidedBackStroke()
                        : cStrokeIdx;

    if (!oFid.isEmptyFrame() && oFid != cFid && fvi && fStrokeCount &&
        strokeIdx < fStrokeCount) {
      TStroke *fStroke = fvi->getStroke(strokeIdx);

      bool frameCreated = m_isFrameCreated;
      m_isFrameCreated  = false;
      touchImage();
      resultBack = doFrameRangeStrokes(
          oFid, fStroke, cFid, cStroke,
          Preferences::instance()->getGuidedInterpolation(), breakAngles,
          autoGroup, autoFill, false, drawStroke, false);
      m_isFrameCreated = frameCreated;
    }
  }

  if (osFront != -1) {
    bool drawFirstStroke = (osBack != -1 && resultBack) ? false : true;

    if (currentFrame->isEditingScene()) {
      TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
      int col      = app->getCurrentColumn()->getColumnIndex();
      if (xsh && col >= 0) {
        TXshCell cell = xsh->getCell(osFront, col);
        if (!cell.isEmpty()) oFid = cell.getFrameId();
      }
    } else
      oFid = sl->getFrameId(osFront);

    TVectorImageP fvi = sl->getFrame(oFid, false);
    int fStrokeCount  = fvi ? fvi->getStrokeCount() : 0;

    int strokeIdx = getViewer()->getGuidedFrontStroke() != -1
                        ? getViewer()->getGuidedFrontStroke()
                        : cStrokeIdx;

    if (!oFid.isEmptyFrame() && oFid != cFid && fvi && fStrokeCount &&
        strokeIdx < fStrokeCount) {
      TStroke *fStroke = fvi->getStroke(strokeIdx);

      bool frameCreated = m_isFrameCreated;
      m_isFrameCreated  = false;
      touchImage();
      resultFront = doFrameRangeStrokes(
          cFid, cStroke, oFid, fStroke,
          Preferences::instance()->getGuidedInterpolation(), breakAngles,
          autoGroup, autoFill, drawFirstStroke, false, false);
      m_isFrameCreated = frameCreated;
    }
  }
  TUndoManager::manager()->endBlock();

  return resultBack || resultFront;
}

//--------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::snap(const TPointD &pos, bool snapEnabled, bool withSelfSnap) {
  bool oldSnapped      = m_snapped;
  bool oldSnappedSelf  = m_snappedSelf;
  TPointD oldPoint     = m_snapPoint;
  TPointD oldPointSelf = m_snapPointSelf;
  
  m_snapped = m_snappedSelf = false;
  
  if (snapEnabled) { // snapping is active
    double minDistance2 = m_minDistance2;
    
    // 0 - strokes, 1 - guides, 2 - all
    int target = Preferences::instance()->getVectorSnappingTarget();

    // snap to guides
    if (target != 0) {
      if (TToolViewer *viewer = getViewer()) {
        // find nearest vertical guide
        int cnt = viewer->getVGuideCount();
        for(int i = 0; i < cnt; ++i) {
          double guide = viewer->getVGuide(i);
          double d2 = guide - pos.y;
          d2 *= d2; // we work with square of the distance
          if (d2 < minDistance2) {
            m_snapped = true;
            m_snapPoint.x = pos.x;
            m_snapPoint.y = guide;
            minDistance2 = d2;
          }
        }

        // find nearest horizontal guide
        cnt = viewer->getHGuideCount();
        for(int i = 0; i < cnt; ++i) {
          double guide = viewer->getHGuide(i);
          double d2 = guide - pos.x;
          d2 *= d2; // we work with square of the distance
          if (d2 < minDistance2) {
            m_snapped = true;
            m_snapPoint.x = guide;
            m_snapPoint.y = pos.y;
            minDistance2 = d2;
          }
        }
      }
    }

    // snap to strokes
    if (target != 1) {
      if (TVectorImageP vi = getImage(false)) {
        int count = vi->getStrokeCount();
        for(int i = 0; i < count; ++i) {
          double w, d2;
          TStroke *stroke = vi->getStroke(i);
          if (!stroke->getNearestW(pos, w, d2) || d2 >= minDistance2)
            continue;
          minDistance2 = d2;
          w = w > 0.001 ? (w < 0.999 ? w : 1.0) : 0.0;
          m_snapped = true;
          m_snapPoint = stroke->getPoint(w);
        }
      }
    
      // finally snap to first point of track (self snap)
      if (withSelfSnap && !m_tracks.empty() && !m_tracks.front().isEmpty()) {
        TPointD p = m_tracks.front().getFirstPoint();
        double d2 = tdistance2(pos, p);
        if (d2 < minDistance2) {
          m_snappedSelf = true;
          m_snapPointSelf = p;
        }
      }
    }
  } // snapping is active
  
  // invalidate rect
  TRectD invalidateRect;
  double radius = 8.0*m_pixelSize;
  TPointD halfSize(radius, radius);

  if ( oldSnapped != m_snapped
    || !areAlmostEqual(oldPoint, m_snapPoint) )
  {
    if (oldSnapped) invalidateRect += TRectD(oldPoint - halfSize, oldPoint + halfSize);
    if (m_snapped)  invalidateRect += TRectD(m_snapPoint - halfSize, m_snapPoint + halfSize);
  }
  
  if ( oldSnappedSelf != m_snappedSelf
    || !areAlmostEqual(oldPointSelf, m_snapPointSelf) )
  {
    if (oldSnappedSelf) invalidateRect += TRectD(oldPointSelf - halfSize, oldPointSelf + halfSize);
    if (m_snappedSelf)  invalidateRect += TRectD(m_snapPointSelf - halfSize, m_snapPointSelf + halfSize);
  }
  
  if (!invalidateRect.isEmpty())
    invalidate(invalidateRect);
}

//-------------------------------------------------------------------------------------------------------------

void ToonzVectorBrushTool::draw() {
  m_pixelSize = getPixelSize();
  m_inputmanager.draw();
  
  /*--ショートカットでのツール切り替え時に赤点が描かれるのを防止する--*/
  if (m_minThick == 0 && m_maxThick == 0 &&
      !Preferences::instance()->getShow0ThickLines())
    return;

  // draw track
  tglColor(m_currentColor);
  for(TrackList::iterator i = m_tracks.begin(); i != m_tracks.end(); ++i)
    i->drawAllFragments();
  
  // draw snapping
  double snapMarkRadius = 6.0 * m_pixelSize;
  if (m_snapped) {
    tglColor(TPixelD(0.1, 0.9, 0.1));
    tglDrawCircle(m_snapPoint, snapMarkRadius);
  }
  if (m_snappedSelf) {
    tglColor(TPixelD(0.9, 0.9, 0.1));
    tglDrawCircle(m_snapPointSelf, snapMarkRadius);
  }

  // frame range
  for(TrackList::iterator i = m_rangeTracks.begin(); i != m_rangeTracks.end(); ++i) {
    if (i->isEmpty()) continue;
    TPointD offset1 = TPointD(5, 5);
    TPointD offset2 = TPointD(-offset1.x, offset1.y);
    TPointD point = i->getFirstPoint();
    glColor3d(1.0, 0.0, 0.0);
    i->drawAllFragments();
    glColor3d(0.0, 0.6, 0.0);
    tglDrawSegment(point - offset1, point + offset1);
    tglDrawSegment(point - offset2, point + offset2);
  }

  if (getApplication()->getCurrentObject()->isSpline()) return;

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  // Don't draw brush outline if picking guiding stroke
  if (getViewer()->getGuidedStrokePickerMode()) return;

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
  m_minThick = m_thickness.getValue().first;
  m_maxThick = m_thickness.getValue().second;
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
  m_rangeTracks.clear();
  m_firstFrameId = -1;
  deleteStrokes(m_firstStrokes);
  m_firstFrameRange = true;
}

//------------------------------------------------------------------

bool ToonzVectorBrushTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  // Set the following to true whenever a different piece of interface must
  // be refreshed - done once at the end.
  bool notifyTool = false;

  if (propertyName == m_preset.getName()) {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastBrush();

    V_VectorBrushPreset = m_preset.getValueAsString();
    m_propertyUpdating  = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  // Switch to <custom> only if it's a preset property change
  if (m_preset.getValue() != CUSTOM_WSTR &&
      (propertyName == m_thickness.getName() ||
       propertyName == m_accuracy.getName() ||
       propertyName == m_smooth.getName() ||
       propertyName == m_breakAngles.getName() ||
       propertyName == m_pressure.getName() ||
       propertyName == m_capStyle.getName() ||
       propertyName == m_joinStyle.getName() ||
       propertyName == m_miterJoinLimit.getName())) {
    m_preset.setValue(CUSTOM_WSTR);
    V_VectorBrushPreset = m_preset.getValueAsString();
    notifyTool          = true;
  }

  // Properties tracked with preset. Update only on <custom>
  if (m_preset.getValue() == CUSTOM_WSTR) {
    V_VectorBrushMinSize       = m_thickness.getValue().first;
    V_VectorBrushMaxSize       = m_thickness.getValue().second;
    V_BrushAccuracy            = m_accuracy.getValue();
    V_BrushSmooth              = m_smooth.getValue();
    V_BrushBreakSharpAngles    = m_breakAngles.getValue();
    V_BrushPressureSensitivity = m_pressure.getValue();
    V_VectorCapStyle           = m_capStyle.getIndex();
    V_VectorJoinStyle          = m_joinStyle.getIndex();
    V_VectorMiterValue         = m_miterJoinLimit.getValue();
  }

  // Properties not tracked with preset
  int frameIndex               = m_frameRange.getIndex();
  V_VectorBrushFrameRange      = frameIndex;
  V_VectorBrushSnap            = m_snap.getValue();
  int snapSensitivityIndex     = m_snapSensitivity.getIndex();
  V_VectorBrushSnapSensitivity = snapSensitivityIndex;
  V_VectorBrushAssistants      = m_assistants.getValue();

  // Recalculate/reset based on changed settings
  m_minThick = m_thickness.getValue().first;
  m_maxThick = m_thickness.getValue().second;

  if (frameIndex == 0) resetFrameRange();

  switch (snapSensitivityIndex) {
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

  if (propertyName == m_joinStyle.getName()) notifyTool = true;

  if (notifyTool) {
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

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

    // Recalculate based on updated presets
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
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
  V_VectorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  V_VectorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzVectorBrushTool::loadLastBrush() {
  // Properties tracked with preset
  m_thickness.setValue(
      TDoublePairProperty::Value(V_VectorBrushMinSize, V_VectorBrushMaxSize));
  m_capStyle.setIndex(V_VectorCapStyle);
  m_joinStyle.setIndex(V_VectorJoinStyle);
  m_miterJoinLimit.setValue(V_VectorMiterValue);
  m_breakAngles.setValue(V_BrushBreakSharpAngles ? 1 : 0);
  m_accuracy.setValue(V_BrushAccuracy);
  m_pressure.setValue(V_BrushPressureSensitivity ? 1 : 0);
  m_smooth.setValue(V_BrushSmooth);

  // Properties not tracked with preset
  m_frameRange.setIndex(V_VectorBrushFrameRange);
  m_snap.setValue(V_VectorBrushSnap ? 1 : 0);
  m_snapSensitivity.setIndex(V_VectorBrushSnapSensitivity);
  m_assistants.setValue(V_VectorBrushAssistants ? 1 : 0);

  // Recalculate based on prior values
  m_minThick = m_thickness.getValue().first;
  m_maxThick = m_thickness.getValue().second;

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
