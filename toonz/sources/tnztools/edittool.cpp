

#include "tools/tool.h"
#include "tools/cursors.h"
#include "tproperty.h"
#include "toonz/stageobjectutil.h"
#include "tstroke.h"
#include "tgl.h"
#include "tenv.h"
#include "toonzqt/gutil.h"

#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/toonzscene.h"
#include "toonz/txshcolumn.h"
#include "toonz/stage.h"
#include "toonz/tcamera.h"

#include "tools/toolhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tstageobjectcmd.h"

#include "edittoolgadgets.h"

// For Qt translation support
#include <QCoreApplication>

#include <QDebug>
#include <QApplication>
#include <QDesktopWidget>

//=============================================================================
// Scale Constraints
//-----------------------------------------------------------------------------

namespace ScaleConstraints {
enum { None = 0, AspectRatio, Mass };
}

TEnv::IntVar LockCenterX("EditToolLockCenterX", 0);
TEnv::IntVar LockCenterY("EditToolLockCenterY", 0);
TEnv::IntVar LockPositionX("EditToolLockPositionX", 0);
TEnv::IntVar LockPositionY("EditToolLockPositionY", 0);
TEnv::IntVar LockRotation("EditToolLockRotation", 0);
TEnv::IntVar LockShearH("EditToolLockShearH", 0);
TEnv::IntVar LockShearV("EditToolLockShearV", 0);
TEnv::IntVar LockScaleH("EditToolLockScaleH", 0);
TEnv::IntVar LockScaleV("EditToolLockScaleV", 0);
TEnv::IntVar LockGlobalScale("EditToolLockGlobalScale", 0);

TEnv::IntVar ShowEWNSposition("EditToolShowEWNSposition", 1);
TEnv::IntVar ShowZposition("EditToolShowZposition", 1);
TEnv::IntVar ShowSOposition("EditToolShowSOposition", 1);
TEnv::IntVar ShowRotation("EditToolShowRotation", 1);
TEnv::IntVar ShowGlobalScale("EditToolShowGlobalScale", 1);
TEnv::IntVar ShowHVscale("EditToolShowHVscale", 1);
TEnv::IntVar ShowShear("EditToolShowShear", 0);
TEnv::IntVar ShowCenterPosition("EditToolShowCenterPosition", 0);
TEnv::StringVar Active("EditToolActiveAxis", "Position");

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

using EditToolGadgets::DragTool;

//=============================================================================
// DragCenterTool
//-----------------------------------------------------------------------------

class DragCenterTool final : public DragTool {
  TStageObjectId m_objId;
  int m_frame;

  bool m_lockCenterX;
  bool m_lockCenterY;

  TPointD m_firstPos;
  TPointD m_oldCenter;
  TPointD m_center;
  TAffine m_affine;

public:
  DragCenterTool(bool lockCenterX, bool lockCenterY)
      : m_objId(
            TTool::getApplication()->getCurrentTool()->getTool()->getObjectId())
      , m_frame(
            TTool::getApplication()->getCurrentTool()->getTool()->getFrame())
      , m_lockCenterX(lockCenterX)
      , m_lockCenterY(lockCenterY) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockCenterX && m_lockCenterY) return;
    TXsheet *xsh =
        TTool::getApplication()->getCurrentTool()->getTool()->getXsheet();
    m_center = m_oldCenter = xsh->getCenter(m_objId, m_frame);
    m_firstPos             = pos;
    m_affine               = xsh->getPlacement(m_objId, m_frame).inv() *
               xsh->getParentPlacement(m_objId, m_frame);
    m_affine.a13 = m_affine.a23 = 0;
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockCenterX && m_lockCenterY) return;
    double factor = 1.0 / Stage::inch;
    TPointD delta = pos - m_firstPos;
    if (m_lockCenterX)
      delta = TPointD(0.0, delta.y);
    else if (m_lockCenterY)
      delta  = TPointD(delta.x, 0.0);
    m_center = m_oldCenter + (m_affine * delta) * factor;
    TTool::getApplication()
        ->getCurrentTool()
        ->getTool()
        ->getXsheet()
        ->setCenter(m_objId, m_frame, m_center);
  }
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {
    if ((m_lockCenterX && m_lockCenterY) || m_firstPos == pos) return;
    UndoStageObjectCenterMove *undo =
        new UndoStageObjectCenterMove(m_objId, m_frame, m_oldCenter, m_center);
    TTool::Application *app = TTool::getApplication();
    undo->setObjectHandle(app->getCurrentObject());
    undo->setXsheetHandle(app->getCurrentXsheet());
    TUndoManager::manager()->add(undo);
  }
};

//=============================================================================
// DragChannelTool
//-----------------------------------------------------------------------------

class DragChannelTool : public DragTool {
protected:
  TStageObjectValues m_before, m_after;
  bool m_globalKeyframesEnabled;

  bool m_isStarted;
  TPointD m_firstPos;

public:
  DragChannelTool(TStageObject::Channel a0, bool globalKeyframesEnabled)
      : m_globalKeyframesEnabled(globalKeyframesEnabled)
      , m_isStarted(false)
      , m_firstPos() {
    TTool::Application *app = TTool::getApplication();
    m_before.setFrameHandle(app->getCurrentFrame());
    m_before.setObjectHandle(app->getCurrentObject());
    m_before.setXsheetHandle(app->getCurrentXsheet());
    m_before.add(a0);
    if (m_globalKeyframesEnabled) {
      m_before.add(TStageObject::T_Angle);
      m_before.add(TStageObject::T_X);
      m_before.add(TStageObject::T_Y);
      m_before.add(TStageObject::T_Z);
      m_before.add(TStageObject::T_SO);
      m_before.add(TStageObject::T_ScaleX);
      m_before.add(TStageObject::T_ScaleY);
      m_before.add(TStageObject::T_Scale);
      m_before.add(TStageObject::T_Path);
      m_before.add(TStageObject::T_ShearX);
      m_before.add(TStageObject::T_ShearY);
    }
    m_after = m_before;
  }
  DragChannelTool(TStageObject::Channel a0, TStageObject::Channel a1,
                  bool globalKeyframesEnabled)
      : m_globalKeyframesEnabled(globalKeyframesEnabled)
      , m_isStarted(false)
      , m_firstPos() {
    TTool::Application *app = TTool::getApplication();
    m_before.setFrameHandle(app->getCurrentFrame());
    m_before.setObjectHandle(app->getCurrentObject());
    m_before.setXsheetHandle(app->getCurrentXsheet());
    m_before.add(a0);
    m_before.add(a1);
    if (m_globalKeyframesEnabled) {
      m_before.add(TStageObject::T_Angle);
      m_before.add(TStageObject::T_X);
      m_before.add(TStageObject::T_Y);
      m_before.add(TStageObject::T_Z);
      m_before.add(TStageObject::T_SO);
      m_before.add(TStageObject::T_ScaleX);
      m_before.add(TStageObject::T_ScaleY);
      m_before.add(TStageObject::T_Scale);
      m_before.add(TStageObject::T_Path);
      m_before.add(TStageObject::T_ShearX);
      m_before.add(TStageObject::T_ShearY);
    }
    m_after = m_before;
  }

  void enableGlobalKeyframes(bool enabled) override {
    m_globalKeyframesEnabled = enabled;
  }

  void start() {
    m_isStarted = true;
    m_before.updateValues();
    m_after = m_before;
  }

  TPointD getCenter() {
    TTool *tool          = TTool::getApplication()->getCurrentTool()->getTool();
    TStageObjectId objId = tool->getObjectId();
    int frame            = tool->getFrame();
    TXsheet *xsh         = tool->getXsheet();
    return xsh->getParentPlacement(objId, frame).inv() *
           xsh->getPlacement(objId, frame) *
           (Stage::inch * xsh->getCenter(objId, frame));
  }

  double getOldValue(int index) const { return m_before.getValue(index); }
  double getValue(int index) const { return m_after.getValue(index); }
  void setValue(double v) {
    m_after.setValue(v);
    m_after.applyValues();
  }
  void setValues(double v0, double v1) {
    m_after.setValues(v0, v1);
    m_after.applyValues();
  }

  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override {
    if (!m_isStarted || m_firstPos == pos)
      return;
    else
      m_isStarted = false;

    TTool::Application *app   = TTool::getApplication();
    UndoStageObjectMove *undo = new UndoStageObjectMove(m_before, m_after);
    undo->setObjectHandle(app->getCurrentObject());
    TUndoManager::manager()->add(undo);
    app->getCurrentScene()->setDirtyFlag(true);
  }
};

//=============================================================================
// DragPositionTool
//-----------------------------------------------------------------------------

class DragPositionTool final : public DragChannelTool {
  bool m_lockPositionX;
  bool m_lockPositionY;

public:
  DragPositionTool(bool lockPositionX, bool lockPositionY,
                   bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_X, TStageObject::T_Y,
                        globalKeyframesEnabled)
      , m_lockPositionX(lockPositionX)
      , m_lockPositionY(lockPositionY) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockPositionX && m_lockPositionY) return;
    start();
    m_firstPos = pos;
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_lockPositionX && m_lockPositionY) return;
    TPointD delta = pos - m_firstPos;
    if (m_lockPositionX)
      delta = TPointD(0, delta.y);
    else if (m_lockPositionY)
      delta = TPointD(delta.x, 0);

    if (e.isShiftPressed()) {
      if (fabs(delta.x) > fabs(delta.y))
        delta.y = 0;
      else
        delta.x = 0;
    }
    double factor = 1.0 / Stage::inch;
    setValues(getOldValue(0) + delta.x * factor,
              getOldValue(1) + delta.y * factor);
  }
};

//=============================================================================
// DragSplinePositionTool
//-----------------------------------------------------------------------------

class DragSplinePositionTool final : public DragChannelTool {
  const TStroke *m_spline;
  std::vector<double> m_lengthAtCPs;
  double m_offset;
  double m_splineLength;
  double m_tolerance;

public:
  DragSplinePositionTool(const TStroke *spline, bool globalKeyframesEnabled,
                         double pixelSize)
      : DragChannelTool(TStageObject::T_Path, globalKeyframesEnabled)
      , m_spline(spline)
      , m_offset(0.0)
      , m_splineLength(0)
      , m_tolerance(pixelSize * 10.0) {}

  double getLengthAtPos(const TPointD &pos) const {
    assert(m_spline);
    double t = m_spline->getW(pos);
    return m_spline->getLength(t);
    /*
double length = m_spline->getLength();
if(length>0) return 100 * m_spline->getLength(t) / length;
else return 0.0;
*/
  }

  double paramValueToLength(double s) const {
    return s * m_splineLength * 0.01;
  }
  double lengthToParamValue(double len) const {
    if (m_splineLength > 0)
      return 100 * len / m_splineLength;
    else
      return 0.0;
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    m_firstPos = pos;
    start();
    assert(m_spline);
    m_splineLength = m_spline->getLength();
    m_lengthAtCPs.clear();
    int n = m_spline->getControlPointCount();
    for (int i = 0; i < n; i += 4)
      m_lengthAtCPs.push_back(m_spline->getLengthAtControlPoint(i));

    m_offset = paramValueToLength(getOldValue(0)) - getLengthAtPos(pos);
  }

  bool snapLengthToControlPoint(double &len) const {
    int n = (int)m_lengthAtCPs.size();
    if (n <= 0) return false;
    double cpLen = len;
    int k        = 0;
    while (k < n && m_lengthAtCPs[k] <= len) k++;
    if (k >= n) {
      // len is >= all CP lengths. The closest is the last
      cpLen = m_lengthAtCPs.back();
    } else if (k == 0) {
      // len is < all CP lengths. the closest is the first
      cpLen = m_lengthAtCPs.front();
    } else {
      // m_lengthAtCPs[k-1]<=len && len < m_lengthAtCPs[k]
      double cpLen0 = m_lengthAtCPs[k - 1];
      double cpLen1 = m_lengthAtCPs[k];
      assert(cpLen0 <= len && len < cpLen1);
      cpLen = (len - cpLen0 < cpLen1 - len) ? cpLen0 : cpLen1;
    }
    if (fabs(cpLen - len) < m_tolerance) {
      len = cpLen;
      return true;
    } else
      return false;
  }

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    double len = tcrop(getLengthAtPos(pos) + m_offset, 0.0, m_splineLength);
    snapLengthToControlPoint(len);
    setValue(lengthToParamValue(len));
  }
};

//=============================================================================
// DragRotationTool
//-----------------------------------------------------------------------------

class DragRotationTool final : public DragChannelTool {
  TPointD m_lastPos;
  TPointD m_center;
  bool m_lockRotation;

public:
  DragRotationTool(bool lockRotation, bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_Angle, globalKeyframesEnabled)
      , m_lockRotation(lockRotation) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockRotation) return;
    m_firstPos = pos;
    m_lastPos  = pos;
    m_center   = getCenter();
    start();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockRotation) return;
    TPointD a = m_lastPos - m_center;
    TPointD b = pos - m_center;
    m_lastPos = pos;
    /*
if(cameraFlag)
{
// m_viewer->updateViewMatrix();
b = m_viewer->mouseToTool(gb) - m_curCenter;
a = m_viewer->mouseToTool(gc) - m_curCenter;
}
*/

    double a2 = norm2(a), b2 = norm2(b);
    const double eps = 1e-8;
    if (a2 < eps || b2 < eps) return;

    double dang = asin(cross(a, b) / sqrt(a2 * b2)) * M_180_PI;
    setValue(getValue(0) + dang);
  }
};

//=============================================================================
// DragIsotropicScaleTool
//-----------------------------------------------------------------------------

class DragIsotropicScaleTool final : public DragChannelTool {
  TPointD m_center;
  double m_r0;
  bool m_lockGlobalScale;

public:
  DragIsotropicScaleTool(bool lockGlobalScale, bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_Scale, globalKeyframesEnabled)
      , m_lockGlobalScale(lockGlobalScale)
      , m_r0(0) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockGlobalScale) return;
    m_firstPos = pos;
    m_center   = getCenter();
    start();
    m_r0 = norm(m_firstPos - m_center);
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_lockGlobalScale) return;
    if (m_r0 < 0.001) return;

    // TPointD a = m_firstPos - m_center;
    double r1 = norm(pos - m_center);
    if (r1 < 0.0001) return;
    setValue(getOldValue(0) * r1 / m_r0);
  }
};

//=============================================================================
// DragScaleTool
//-----------------------------------------------------------------------------

class DragScaleTool final : public DragChannelTool {
  TPointD m_center;
  int m_constraint;

  bool m_lockScaleH;
  bool m_lockScaleV;

public:
  DragScaleTool(int constraint, bool lockScaleH, bool lockScaleV,
                bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_ScaleX, TStageObject::T_ScaleY,
                        globalKeyframesEnabled)
      , m_constraint(constraint)
      , m_lockScaleH(lockScaleH)
      , m_lockScaleV(lockScaleV) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockScaleH && m_lockScaleV) return;
    m_firstPos = pos;
    m_center   = getCenter();
    start();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_lockScaleH && m_lockScaleV) return;
    TPointD center = m_center + TPointD(40, 40);
    TPointD a      = m_firstPos - center;
    TPointD b      = pos - center;

    const double eps = 1e-8;
    if (norm2(a) < eps || norm2(b) < eps) return;

    double fx            = b.x / a.x;
    if (fabs(fx) > 1) fx = tsign(fx) * sqrt(abs(fx));
    double fy            = b.y / a.y;
    if (fabs(fy) > 1) fy = tsign(fy) * sqrt(abs(fy));

    int constraint = m_constraint;
    if (constraint == ScaleConstraints::None && e.isShiftPressed())
      constraint = ScaleConstraints::AspectRatio;

    if (constraint == ScaleConstraints::AspectRatio) {
      TPointD c = pos - m_firstPos;
      if (fabs(c.x) > fabs(c.y))
        fy = fx;
      else
        fx = fy;
    } else if (constraint == ScaleConstraints::Mass) {
      double bxay = b.x * a.y;
      double byax = b.y * a.x;
      if (fabs(bxay) < eps || fabs(byax) < eps) return;
      fx = bxay / byax;
      fy = byax / bxay;
    }
    if (fabs(fx) > eps && fabs(fy) > eps) {
      double oldv0                   = getOldValue(0);
      double oldv1                   = getOldValue(1);
      if (fabs(oldv0) < 0.001) oldv0 = 0.001;
      if (fabs(oldv1) < 0.001) oldv1 = 0.001;

      double valueX = oldv0 * fx;
      double valueY = oldv1 * fy;
      if (m_lockScaleH)
        valueX = oldv0;
      else if (m_lockScaleV)
        valueY = oldv1;

      setValues(valueX, valueY);
    }
  }
};

//=============================================================================
// DragShearTool
//-----------------------------------------------------------------------------

class DragShearTool final : public DragChannelTool {
  TPointD m_center;
  bool m_lockShearH;
  bool m_lockShearV;

public:
  DragShearTool(bool lockShearH, bool lockShearV, bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_ShearX, TStageObject::T_ShearY,
                        globalKeyframesEnabled)
      , m_lockShearH(lockShearH)
      , m_lockShearV(lockShearV) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (m_lockShearH && m_lockShearV) return;
    m_firstPos = pos;
    m_center   = getCenter();
    start();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_lockShearH && m_lockShearV) return;
    TPointD a = m_firstPos - m_center;
    TPointD b = pos - m_center;
    double fx = a.x - b.x;
    double fy = b.y - a.y;

    if (m_lockShearH)
      fx = 0;
    else if (m_lockShearV)
      fy = 0;

    if (e.isShiftPressed()) {
      if (fabs(fx) > fabs(fy))
        fy = 0;
      else
        fx = 0;
    }
    setValues(getOldValue(0) + 0.01 * fx, getOldValue(1) + 0.01 * fy);
  }
};

//=============================================================================
// DragZTool
//-----------------------------------------------------------------------------

class DragZTool final : public DragChannelTool {
  TPointD m_lastPos;
  TTool::Viewer *m_viewer;
  double m_dz;

public:
  DragZTool(TTool::Viewer *viewer, bool globalKeyframesEnabled)
      : DragChannelTool(TStageObject::T_Z, globalKeyframesEnabled)
      , m_viewer(viewer) {}

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_lastPos  = e.m_pos;
    m_firstPos = pos;
    m_dz       = 0;
    start();
  }
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    double dz = m_viewer->projectToZ(e.m_pos - m_lastPos);
    m_lastPos = e.m_pos;
    if (dz != 0.0) {
      m_dz += dz;
      setValue(getOldValue(0) + m_dz);
    }
  }
};

//=============================================================================
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// EditTool
//-----------------------------------------------------------------------------

class EditTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(EditTool)

  DragTool *m_dragTool;

  bool m_firstTime;

  enum {
    None        = -1,
    Translation = 1,
    Rotation,
    Scale,
    ScaleX,
    ScaleY,
    ScaleXY,
    Center,
    ZTranslation,
    Shear,
  };

  // DragInfo m_dragInfo;

  TPointD m_lastPos;
  TPointD m_curPos;
  TPointD m_firstPos;
  TPointD m_curCenter;

  bool m_active;
  bool m_keyFrameAdded;
  int m_what;
  int m_highlightedDevice;

  double m_oldValues[2];

  double m_currentScaleFactor;
  FxGadgetController *m_fxGadgetController;

  TEnumProperty m_scaleConstraint;
  TEnumProperty m_autoSelect;
  TBoolProperty m_globalKeyframes;

  TBoolProperty m_lockCenterX;
  TBoolProperty m_lockCenterY;
  TBoolProperty m_lockPositionX;
  TBoolProperty m_lockPositionY;
  TBoolProperty m_lockRotation;
  TBoolProperty m_lockShearH;
  TBoolProperty m_lockShearV;
  TBoolProperty m_lockScaleH;
  TBoolProperty m_lockScaleV;
  TBoolProperty m_lockGlobalScale;

  TBoolProperty m_showEWNSposition;
  TBoolProperty m_showZposition;
  TBoolProperty m_showSOposition;
  TBoolProperty m_showRotation;
  TBoolProperty m_showGlobalScale;
  TBoolProperty m_showHVscale;
  TBoolProperty m_showShear;
  TBoolProperty m_showCenterPosition;

  TEnumProperty m_activeAxis;

  TPropertyGroup m_prop;

  void drawMainHandle();
  void onEditAllLeftButtonDown(TPointD &pos, const TMouseEvent &e);

public:
  EditTool();
  ~EditTool();

  ToolType getToolType() const override { return TTool::ColumnTool; }

  bool doesApply() const;  // ritorna vero se posso deformare l'oggetto corrente
  void saveOldValues();

  const TStroke *getSpline() const;

  void rotate();
  void move();
  void moveCenter();
  void scale();
  void isoScale();
  void squeeze();
  void shear(const TPointD &pos, bool single);

  void updateTranslation() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  void mouseMove(const TPointD &, const TMouseEvent &e) override;

  void draw() override;

  void transform(const TAffine &aff);

  void onActivate() override;
  void onDeactivate() override;
  bool onPropertyChanged(std::string propertyName) override;

  void computeBBox();

  int getCursorId() const override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void updateMatrix() override {
    setMatrix(
        getCurrentObjectParentMatrix2());  // getCurrentObjectParentMatrix());
  }

  void drawText(const TPointD &p, double unit, std::string text);
};

//-----------------------------------------------------------------------------

EditTool::EditTool()
    : TTool("T_Edit")
    , m_active(false)
    , m_what(Translation)
    , m_highlightedDevice(None)
    , m_currentScaleFactor(1)
    , m_fxGadgetController(0)
    , m_keyFrameAdded(false)
    , m_scaleConstraint("Scale Constraint:")  // W_ToolOptions_ScaleConstraint
    , m_autoSelect("Auto Select Column")      // W_ToolOptions_AutoSelect
    , m_globalKeyframes("Global Key", false)  // W_ToolsOptions_GlobalKeyframes
    , m_lockCenterX("Lock Center E/W", false)
    , m_lockCenterY("Lock Center N/S", false)
    , m_lockPositionX("Lock Position E/W", false)
    , m_lockPositionY("Lock Position N/S", false)
    , m_lockRotation("Lock Rotation", false)
    , m_lockShearH("Lock Shear H", false)
    , m_lockShearV("Lock Shear V", false)
    , m_lockScaleH("Lock Scale H", false)
    , m_lockScaleV("Lock Scale V", false)
    , m_lockGlobalScale("Lock Global Scale", false)
    , m_showEWNSposition("E/W and N/S Positions", true)
    , m_showZposition("Z Position", true)
    , m_showSOposition("SO", true)
    , m_showRotation("Rotation", true)
    , m_showGlobalScale("Global Scale", true)
    , m_showHVscale("Horizontal and Vertical Scale", true)
    , m_showShear("Shear", true)
    , m_showCenterPosition("Center Position", true)
    , m_dragTool(0)
    , m_firstTime(true)
    , m_activeAxis("Active Axis") {
  bind(TTool::AllTargets);
  m_prop.bind(m_scaleConstraint);
  m_prop.bind(m_autoSelect);
  m_prop.bind(m_globalKeyframes);

  m_prop.bind(m_lockCenterX);
  m_prop.bind(m_lockCenterY);
  m_prop.bind(m_lockPositionX);
  m_prop.bind(m_lockPositionY);
  m_prop.bind(m_lockRotation);
  m_prop.bind(m_lockShearH);
  m_prop.bind(m_lockShearV);
  m_prop.bind(m_lockScaleH);
  m_prop.bind(m_lockScaleV);
  m_prop.bind(m_lockGlobalScale);

  m_prop.bind(m_showEWNSposition);
  m_prop.bind(m_showZposition);
  m_prop.bind(m_showSOposition);
  m_prop.bind(m_showRotation);
  m_prop.bind(m_showGlobalScale);
  m_prop.bind(m_showHVscale);
  m_prop.bind(m_showShear);
  m_prop.bind(m_showCenterPosition);

  m_scaleConstraint.addValue(L"None");
  m_scaleConstraint.addValue(L"A/R");
  m_scaleConstraint.addValue(L"Mass");
  m_scaleConstraint.setValue(L"None");

  m_autoSelect.addValue(L"None");
  m_autoSelect.addValue(L"Column");
  m_autoSelect.addValue(L"Pegbar");
  m_autoSelect.setValue(L"None");

  m_globalKeyframes.setId("GlobalKey");
  m_autoSelect.setId("AutoSelect");

  m_prop.bind(m_activeAxis);
  m_activeAxis.addValue(L"Position", "edit_position");
  m_activeAxis.addValue(L"Rotation", "edit_rotation");
  m_activeAxis.addValue(L"Scale", "edit_scale");
  m_activeAxis.addValue(L"Shear", "edit_shear");
  m_activeAxis.addValue(L"Center", "edit_center");
  m_activeAxis.addValue(L"All", "edit_all");
  m_activeAxis.setValue(L"Position");

  m_activeAxis.setId("EditToolActiveAxis");
}

//-----------------------------------------------------------------------------

EditTool::~EditTool() {
  delete m_dragTool;
  delete m_fxGadgetController;
}

//-----------------------------------------------------------------------------

void EditTool::updateTranslation() {
  m_scaleConstraint.setQStringName(tr("Scale Constraint:"));
  m_scaleConstraint.setItemUIName(L"None", tr("None"));
  m_scaleConstraint.setItemUIName(L"A/R", tr("A/R"));
  m_scaleConstraint.setItemUIName(L"Mass", tr("Mass"));

  m_autoSelect.setQStringName(tr("Auto Select Column"));
  m_autoSelect.setItemUIName(L"None", tr("None"));
  m_autoSelect.setItemUIName(L"Column", tr("Column"));
  m_autoSelect.setItemUIName(L"Pegbar", tr("Pegbar"));

  m_globalKeyframes.setQStringName(tr("Global Key"));
  m_lockCenterX.setQStringName(tr("Lock Center E/W"));
  m_lockCenterY.setQStringName(tr("Lock Center N/S"));
  m_lockPositionX.setQStringName(tr("Lock Position E/W"));
  m_lockPositionY.setQStringName(tr("Lock Position N/S"));
  m_lockRotation.setQStringName(tr("Lock Rotation"));
  m_lockShearH.setQStringName(tr("Lock Shear H"));
  m_lockShearV.setQStringName(tr("Lock Shear V"));
  m_lockScaleH.setQStringName(tr("Lock Scale H"));
  m_lockScaleV.setQStringName(tr("Lock Scale V"));
  m_lockGlobalScale.setQStringName(tr("Lock Global Scale"));
  m_showEWNSposition.setQStringName(tr("E/W and N/S Positions"));
  m_showZposition.setQStringName(tr("Z Position"));
  m_showSOposition.setQStringName(tr("SO"));
  m_showRotation.setQStringName(tr("Rotation"));
  m_showGlobalScale.setQStringName(tr("Global Scale"));
  m_showHVscale.setQStringName(tr("Horizontal and Vertical Scale"));
  m_showShear.setQStringName(tr("Shear"));
  m_showCenterPosition.setQStringName(tr("Center Position"));

  m_activeAxis.setQStringName(tr("Active Axis"));
  m_activeAxis.setItemUIName(L"Position", tr("Position"));
  m_activeAxis.setItemUIName(L"Rotation", tr("Rotation"));
  m_activeAxis.setItemUIName(L"Scale", tr("Scale"));
  m_activeAxis.setItemUIName(L"Shear", tr("Shear"));
  m_activeAxis.setItemUIName(L"Center", tr("Center"));
  m_activeAxis.setItemUIName(L"All", tr("All"));
}

//-----------------------------------------------------------------------------

bool EditTool::doesApply() const {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  assert(xsh);
  TStageObjectId objId =
      TTool::getApplication()->getCurrentObject()->getObjectId();
  if (objId.isColumn()) {
    TXshColumn *column = xsh->getColumn(objId.getIndex());
    if (column && column->getSoundColumn()) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

const TStroke *EditTool::getSpline() const {
  TTool::Application *app    = TTool::getApplication();
  TXsheet *xsh               = app->getCurrentXsheet()->getXsheet();
  TStageObjectId objId       = app->getCurrentObject()->getObjectId();
  TStageObject *pegbar       = xsh->getStageObject(objId);
  TStageObjectSpline *spline = pegbar ? pegbar->getSpline() : 0;
  return spline ? spline->getStroke() : 0;
}

//-----------------------------------------------------------------------------

void EditTool::mouseMove(const TPointD &, const TMouseEvent &e) {
  /*-- return while left dragging --*/
  if (e.isLeftButtonPressed()) return;

  /*-- Pick screen only when the FxGadget is displayed or
       when the "All" axis is selected. --*/
  int selectedDevice = -1;
  if (m_fxGadgetController->hasGadget() || m_activeAxis.getValue() == L"All")
    selectedDevice = pick(e.m_pos);

  if (selectedDevice <= 0) {
    selectedDevice = m_what;
    if (m_what == Translation && e.isCtrlPressed())
      selectedDevice = ZTranslation;
    else if (
        m_what == ZTranslation &&
        !e.isCtrlPressed()) /*--ここには、一度Z移動をした後に入る可能性がある--*/
      selectedDevice = Translation;
    else if (m_what == Scale && e.isCtrlPressed())
      selectedDevice = ScaleXY;
    else if (m_what == ScaleXY && !e.isCtrlPressed())
      selectedDevice = Scale;
  }
  if (selectedDevice != m_highlightedDevice) {
    m_highlightedDevice = selectedDevice;
    m_fxGadgetController->selectById(selectedDevice);
    invalidate();
  }
}

//-----------------------------------------------------------------------------

TPoint ga, gb, gc;
TPoint lastScreenPos;

//-----------------------------------------------------------------------------

void EditTool::leftButtonDown(const TPointD &ppos, const TMouseEvent &e) {
  TPointD pos = ppos;
  /*-- Soundカラムの場合は何もしない --*/
  if (!doesApply()) return;

  if (m_activeAxis.getValue() == L"Position")
    if (e.isCtrlPressed())
      m_what = ZTranslation;
    else
      m_what = Translation;
  else if (m_activeAxis.getValue() == L"Scale")
    if (e.isCtrlPressed())
      m_what = ScaleXY;
    else
      m_what = Scale;
  else if (m_activeAxis.getValue() == L"All")
    onEditAllLeftButtonDown(pos, e);

  int scaleConstraint = 0;
  if (m_scaleConstraint.getValue() == L"A/R")
    scaleConstraint = ScaleConstraints::AspectRatio;
  else if (m_scaleConstraint.getValue() == L"Mass")
    scaleConstraint = ScaleConstraints::Mass;

  assert(m_dragTool == 0);

  if (m_highlightedDevice >= 1000) {
    m_dragTool = m_fxGadgetController->createDragTool(m_highlightedDevice);
  }

  if (!m_dragTool) {
    switch (m_what) {
    case Center:
      m_dragTool = new DragCenterTool(m_lockCenterX.getValue(),
                                      m_lockCenterY.getValue());
      break;

    case Translation:
      if (const TStroke *spline = getSpline())
        m_dragTool = new DragSplinePositionTool(
            spline, m_globalKeyframes.getValue(), getPixelSize());
      else
        m_dragTool = new DragPositionTool(m_lockPositionX.getValue(),
                                          m_lockPositionY.getValue(),
                                          m_globalKeyframes.getValue());
      break;

    case Rotation:
      m_dragTool = new DragRotationTool(m_lockRotation.getValue(),
                                        m_globalKeyframes.getValue());
      break;

    case Scale:
      m_dragTool = new DragIsotropicScaleTool(m_lockGlobalScale.getValue(),
                                              m_globalKeyframes.getValue());
      break;

    case ScaleXY:
      m_dragTool = new DragScaleTool(scaleConstraint, m_lockScaleH.getValue(),
                                     m_lockScaleV.getValue(),
                                     m_globalKeyframes.getValue());
      break;

    case Shear:
      m_dragTool =
          new DragShearTool(m_lockShearH.getValue(), m_lockShearV.getValue(),
                            m_globalKeyframes.getValue());
      break;
    case ZTranslation:
      m_dragTool = new DragZTool(m_viewer, m_globalKeyframes.getValue());
      break;
    }
  }
  if (m_dragTool) {
    m_dragTool->enableGlobalKeyframes(m_globalKeyframes.getValue());
    TUndoManager::manager()->beginBlock();
    m_dragTool->leftButtonDown(pos, e);
  }
  invalidate();
}

//-----------------------------------------------------------------------------

void EditTool::onEditAllLeftButtonDown(TPointD &pos, const TMouseEvent &e) {
  int selectedDevice = pick(e.m_pos);
  m_what             = selectedDevice >= 0 ? selectedDevice : Translation;

  if (selectedDevice < 0 && m_autoSelect.getValue() != L"None") {
    pos = getMatrix() * pos;
    int columnIndex =
        getViewer()->posToColumnIndex(e.m_pos, 5 * getPixelSize(), false);
    if (columnIndex >= 0) {
      TStageObjectId id      = TStageObjectId::ColumnId(columnIndex);
      int currentColumnIndex = getColumnIndex();
      TXsheet *xsh           = getXsheet();

      if (m_autoSelect.getValue() == L"Pegbar") {
        TStageObjectId id2 = id;
        while (!id2.isPegbar()) {
          id2 = xsh->getStageObjectParent(id2);
          if (!id2.isColumn() && !id2.isPegbar()) break;
        }
        if (id2.isPegbar()) id = id2;
      }
      if (id.isColumn()) {
        if (columnIndex >= 0 && columnIndex != currentColumnIndex) {
          if (e.isShiftPressed()) {
            TXsheetHandle *xshHandle =
                TTool::getApplication()->getCurrentXsheet();
            TStageObjectId curColId =
                TStageObjectId::ColumnId(currentColumnIndex);
            TStageObjectId colId = TStageObjectId::ColumnId(columnIndex);
            TStageObjectCmd::setParent(curColId, colId, "", xshHandle);
            m_what = None;
            xshHandle->notifyXsheetChanged();
          } else {
            TXshColumn *column = xsh->getColumn(columnIndex);
            if (!column || !column->isLocked()) {
              TTool::getApplication()->getCurrentColumn()->setColumnIndex(
                  columnIndex);
              updateMatrix();
            }
          }
        }
      } else {
        TTool::getApplication()->getCurrentObject()->setObjectId(id);
        updateMatrix();
      }
    }
    pos = getMatrix().inv() * pos;
  }
}

//-----------------------------------------------------------------------------

void EditTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_dragTool) return;
  m_dragTool->leftButtonDrag(pos, e);
  TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(true);
  invalidate();
}

//-----------------------------------------------------------------------------

void EditTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (m_dragTool) {
    m_dragTool->leftButtonUp(pos, e);
    TUndoManager::manager()->endBlock();
    delete m_dragTool;
    m_dragTool = 0;
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
  }
  m_keyFrameAdded = false;
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void drawArrow(double r, bool filled) {
  double z0 = 0.25 * r;
  double z1 = 2 * r;
  double z2 = 4 * r;
  double x0 = r;
  double x1 = 2 * r;

  if (filled) {
    glBegin(GL_POLYGON);
    glVertex3d(x1, 0, z2);
    glVertex3d(-x1, 0, z2);
    glVertex3d(-x1, 0, -z2);
    glVertex3d(x1, 0, -z2);
    glVertex3d(x1, 0, z2);
    glEnd();
  } else {
    glBegin(GL_LINE_STRIP);
    glVertex3d(x0, 0, z0);
    glVertex3d(x0, 0, z1);
    glVertex3d(x1, 0, z1);
    glVertex3d(0, 0, z2);
    glVertex3d(-x1, 0, z1);
    glVertex3d(-x0, 0, z1);
    glVertex3d(-x0, 0, z0);
    glVertex3d(-x0, 0, z0);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex3d(x0, 0, -z0);
    glVertex3d(x0, 0, -z1);
    glVertex3d(x1, 0, -z1);
    glVertex3d(0, 0, -z2);
    glVertex3d(-x1, 0, -z1);
    glVertex3d(-x0, 0, -z1);
    glVertex3d(-x0, 0, -z0);
    glVertex3d(-x0, 0, -z0);
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void drawCameraIcon() {
  glBegin(GL_LINE_STRIP);
  glVertex2i(5, 0);
  glVertex2i(16, 0);
  glVertex2i(16, 3);
  glVertex2i(16, 3);
  glVertex2i(22, 0);
  glVertex2i(22, 9);
  glVertex2i(16, 6);
  glVertex2i(16, 9);
  glVertex2i(14, 9);
  glVertex2i(16, 11);
  glVertex2i(16, 14);
  glVertex2i(14, 16);
  glVertex2i(11, 16);
  glVertex2i(9, 14);
  glVertex2i(9, 11);
  glVertex2i(11, 9);
  glVertex2i(7, 9);
  glVertex2i(7, 11);
  glVertex2i(5, 13);
  glVertex2i(2, 13);
  glVertex2i(0, 11);
  glVertex2i(0, 8);
  glVertex2i(2, 6);
  glVertex2i(5, 6);
  glVertex2i(5, 0);
  glEnd();
}

void drawZArrow() {
  /*--矢印--*/
  glBegin(GL_LINE_LOOP);
  glVertex2i(0, 3);
  glVertex2i(2, 2);
  glVertex2i(1, 2);
  glVertex2i(2, -3);
  glVertex2i(4, -3);
  glVertex2i(0, -6);
  glVertex2i(-4, -3);
  glVertex2i(-2, -3);
  glVertex2i(-1, 2);
  glVertex2i(-2, 2);
  glEnd();
  /*--Zの文字--*/
  glBegin(GL_LINE_STRIP);
  glVertex2i(3, 4);
  glVertex2i(5, 4);
  glVertex2i(3, 1);
  glVertex2i(5, 1);
  glEnd();
}

//-----------------------------------------------------------------------------
}  // namespace

//-----------------------------------------------------------------------------

void EditTool::drawText(const TPointD &p, double unit, std::string text) {
  glPushMatrix();
  glTranslated(p.x, p.y, 0.0);
  double sc = unit * 1.6;
  /*
double w = sc * tglGetTextWidth(text, GLUT_STROKE_ROMAN);
double h = sc;
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glColor4d(0.9,0.9,0.1,0.5);
glRectd(0,0,w,h);
glDisable(GL_BLEND);
glColor3d(0,0,0);
*/

  glScaled(sc, sc, 1);
  tglDrawText(TPointD(8, -3), text);
  glPopMatrix();
}

//-----------------------------------------------------------------------------

void EditTool::drawMainHandle() {
  const TPixel32 normalColor(250, 127, 240);
  const TPixel32 highlightedColor(150, 255, 140);

  // collect information
  TXsheet *xsh         = getXsheet();
  TStageObjectId objId = getObjectId();
  int frame            = getFrame();
  TAffine parentAff    = xsh->getParentPlacement(objId, frame);
  TAffine aff          = xsh->getPlacement(objId, frame);
  TPointD center       = Stage::inch * xsh->getCenter(objId, frame);
  int devPixRatio      = getDevPixRatio();
  // the gadget appears on the center of the level. orientation and dimension
  // are independent of the movement of the level
  glPushMatrix();

  tglMultMatrix(parentAff.inv() * TTranslation(aff * center));

  // so in the system of ref. of the gadget the center is always in the origin
  center = TPointD();

  double unit = sqrt(tglGetPixelSize2());
  unit *= devPixRatio;
  bool dragging = m_dragTool != 0;

  // draw center
  tglColor(m_highlightedDevice == Center ? highlightedColor : normalColor);
  glPushName(Center);
  if (isPicking())
    tglDrawDisk(center, unit * 12);
  else {
    tglDrawCircle(center, unit * 10);
    tglDrawCircle(center, unit * 8);
    if (m_highlightedDevice == Center && !dragging)
      drawText(center + TPointD(4 * unit, 0), unit, "Move center");
  }
  glPopName();

  // draw label (column/pegbar name; possibly camera icon)
  tglColor(normalColor);
  glPushMatrix();
  glTranslated(center.x + unit * 10, center.y - unit * 20, 0);

  if (objId.isColumn() || objId.isPegbar()) {
    TStageObject *pegbar = xsh->getStageObject(objId);
    std::string name     = pegbar->getFullName();
    glScaled(unit * 2, unit * 1.5, 1);
    tglDrawText(TPointD(0, 0), name);
  } else if (objId.isCamera()) {
    glScaled(unit, unit, 1);
    drawCameraIcon();
  }
  glPopMatrix();

  // draw rotation handle
  const double delta = 30;
  tglColor(m_highlightedDevice == Rotation ? highlightedColor : normalColor);
  glPushName(Rotation);
  TPointD p = center + unit * TPointD(0, delta);
  if (isPicking())
    tglDrawDisk(p, unit * 10);
  else
    tglDrawDisk(p, unit * 5);
  glPopName();
  if (m_highlightedDevice == Rotation && !dragging && !isPicking())
    drawText(p, unit, "Rotate");
  tglColor(normalColor);
  tglDrawSegment(p, center);

  // draw scale handle
  p        = center + m_currentScaleFactor * unit * delta * TPointD(-1, -1);
  double r = unit * 3;
  double f = 5;
  TRectD hitRect;

  tglColor(m_highlightedDevice == Scale ? highlightedColor : normalColor);
  glPushName(Scale);
  hitRect =
      TRectD(p.x - (f - 2) * r, p.y - (f - 2) * r, p.x + r * 2, p.y + r * 2);
  // tglDrawRect(hitRect);
  if (isPicking())
    tglFillRect(hitRect);
  else
    tglDrawRect(p.x - r, p.y - r, p.x + r, p.y + r);
  glPopName();
  TPointD scaleTooltipPos = p + unit * TPointD(-16, -16);
  if (m_highlightedDevice == Scale && !dragging && !isPicking())
    drawText(scaleTooltipPos, unit, "Scale");

  tglColor(normalColor);
  tglDrawSegment(p, center);

  TPointD q;
  double dd = unit * 10;

  q = p + TPointD(dd, dd);
  tglColor(m_highlightedDevice == ScaleXY ? highlightedColor : normalColor);
  glPushName(ScaleXY);
  hitRect =
      TRectD(q.x - 2 * r, q.y - 2 * r, q.x + r * (f - 2), q.y + r * (f - 2));
  // tglDrawRect(hitRect);
  if (isPicking())
    tglFillRect(hitRect);
  else
    tglDrawRect(q.x - r, q.y - r, q.x + r, q.y + r);
  glPopName();
  if (m_highlightedDevice == ScaleXY && !dragging && !isPicking())
    drawText(scaleTooltipPos, unit, "Horizontal/Vertical scale");

  // draw shear handle
  p = center + m_currentScaleFactor * unit * delta * TPointD(1, -1);
  tglColor(m_highlightedDevice == Shear ? highlightedColor : normalColor);
  glPushName(Shear);
  if (isPicking()) {
    glBegin(GL_POLYGON);
    glVertex2d(p.x - unit * 6, p.y - unit * 3);
    glVertex2d(p.x - unit * 3, p.y - unit * 3);
    glVertex2d(p.x + unit * 6, p.y + unit * 3);
    glVertex2d(p.x + unit * 3, p.y + unit * 3);
    glVertex2d(p.x - unit * 6, p.y - unit * 3);
    glEnd();
  } else {
    glBegin(GL_LINE_STRIP);
    glVertex2d(p.x - unit * 6, p.y - unit * 3);
    glVertex2d(p.x - unit * 3, p.y - unit * 3);
    glVertex2d(p.x + unit * 6, p.y + unit * 3);
    glVertex2d(p.x + unit * 3, p.y + unit * 3);
    glVertex2d(p.x - unit * 6, p.y - unit * 3);
    glEnd();
  }
  glPopName();
  if (m_highlightedDevice == Shear && !dragging)
    drawText(p + TPointD(0, -unit * 10), unit, "Shear");
  tglColor(normalColor);
  tglDrawSegment(p, center);

  //
  if (objId.isCamera()) {
    if (xsh->getStageObjectTree()->getCurrentCameraId() != objId) {
      glEnable(GL_LINE_STIPPLE);
      glColor3d(1.0, 0.0, 1.0);
      glLineStipple(1, 0x1111);
      TRectD cameraRect = TTool::getApplication()
                              ->getCurrentScene()
                              ->getScene()
                              ->getCurrentCamera()
                              ->getStageRect();

      glPushMatrix();
      // tglMultMatrix(mat);
      tglDrawRect(cameraRect);
      glPopMatrix();
      glDisable(GL_LINE_STIPPLE);
    }
  }

  glPopMatrix();
}
//-----------------------------------------------------------------------------

void EditTool::draw() {
  // the tool is using the coordinate system of the parent object
  // glColor3d(1,0,1);
  // tglDrawCircle(crossHair,50);

  /*-- Show nothing on Level Editing mode --*/
  if (TTool::getApplication()->getCurrentFrame()->isEditingLevel()) return;
  const TPixel32 normalColor(250, 127, 240);
  const TPixel32 highlightedColor(150, 255, 140);

  // collect information
  TXsheet *xsh = getXsheet();
  /*-- Obtain ID of the current editing stage object --*/
  TStageObjectId objId = getObjectId();
  int frame            = getFrame();
  TAffine parentAff    = xsh->getParentPlacement(objId, frame);
  TAffine aff          = xsh->getPlacement(objId, frame);
  TPointD center       = Stage::inch * xsh->getCenter(objId, frame);

  /*-- Enable Z translation on 3D view --*/
  if (getViewer()->is3DView()) {
    glPushMatrix();
    glPushName(ZTranslation);

    tglColor(m_highlightedDevice == ZTranslation ? highlightedColor
                                                 : normalColor);

    glPushMatrix();
    double z = xsh->getZ(objId, frame);
    glTranslated(0, -1, z);
    drawArrow(50, isPicking());
    glPopName();
    glPopMatrix();

    glPopMatrix();
    return;
  }

  // Edit-all
  if (m_activeAxis.getValue() == L"All") {
    if (!m_fxGadgetController->isEditingNonZeraryFx()) drawMainHandle();
    m_fxGadgetController->draw(isPicking());
    return;
  }

  double unit = getPixelSize();

  /*-- Obtain object's center position --*/
  glPushMatrix();
  tglMultMatrix(parentAff.inv() * TTranslation(aff * TPointD(0.0, 0.0)));
  tglColor(normalColor);
  tglDrawDisk(TPointD(0.0, 0.0), unit * 4);
  glPopMatrix();

  /*-- Z translation : Draw arrow mark (placed at the camera center) --*/
  if (m_activeAxis.getValue() == L"Position" &&
      m_highlightedDevice == ZTranslation) {
    tglColor(normalColor);
    glPushMatrix();
    TStageObjectId currentCamId =
        xsh->getStageObjectTree()->getCurrentCameraId();
    TAffine camParentAff = xsh->getParentPlacement(currentCamId, frame);
    TAffine camAff       = xsh->getPlacement(currentCamId, frame);
    tglMultMatrix(camParentAff.inv() *
                  TTranslation(camAff * TPointD(0.0, 0.0)));
    glScaled(unit * 8, unit * 8, 1);
    drawZArrow();
    glPopMatrix();
  }
  /*-- Rotation, Position : Draw vertical and horizontal lines --*/
  else if (m_activeAxis.getValue() == L"Rotation" ||
           m_activeAxis.getValue() == L"Position") {
    glPushMatrix();
    tglMultMatrix(parentAff.inv() * aff * TTranslation(center));
    glScaled(unit, unit, 1);
    tglColor(normalColor);
    glBegin(GL_LINE_STRIP);
    glVertex2i(-800, 0);
    glVertex2i(800, 0);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2i(0, -100);
    glVertex2i(0, 100);
    glEnd();
    glPopMatrix();
  }
  glPushMatrix();

  tglMultMatrix(parentAff.inv() * TTranslation(aff * center));
  center = TPointD();

  bool dragging = m_dragTool != 0;

  // draw center
  tglColor(normalColor);
  glPushName(Center);
  {
    tglDrawCircle(center, unit * 10);
    tglDrawCircle(center, unit * 8);

    /*-- Draw crossed lines in the circle. It's already translated to the center
     * position. --*/
    glBegin(GL_LINE_STRIP);
    glVertex2d(-unit * 8, 0.0);
    glVertex2d(unit * 8, 0.0);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2d(0.0, -unit * 8);
    glVertex2d(0.0, unit * 8);
    glEnd();
  }
  glPopName();

  // draw label (column/pegbar name; possibly camera icon)
  tglColor(normalColor);
  glPushMatrix();
  glTranslated(center.x + unit * 10, center.y - unit * 20, 0);

  /*-- Object name --*/
  TStageObject *pegbar = xsh->getStageObject(objId);
  std::string name     = pegbar->getFullName();
  if (objId.isColumn() || objId.isPegbar() || objId.isTable()) {
    glScaled(unit * 2, unit * 1.5, 1);
    tglDrawText(TPointD(0, 0), name);
  } else if (objId.isCamera()) {
    glPushMatrix();
    glScaled(unit * 2, unit * 1.5, 1);
    tglDrawText(TPointD(12, 0), name);
    glPopMatrix();
    glScaled(unit, unit, 1);
    drawCameraIcon();
  }
  glPopMatrix();

  /*--- When editing non-active camera, draw its camera frame ---*/
  if (objId.isCamera()) {
    if (xsh->getStageObjectTree()->getCurrentCameraId() != objId) {
      // TODO : glLineStipple has been deprecated in the OpenGL APIs. Need to be
      // replaced. 2016/1/20 shun_iwasawa
      glEnable(GL_LINE_STIPPLE);
      glColor3d(1.0, 0.0, 1.0);
      glLineStipple(1, 0x1111);
      TRectD cameraRect = TTool::getApplication()
                              ->getCurrentScene()
                              ->getScene()
                              ->getCurrentCamera()
                              ->getStageRect();

      glPushMatrix();
      tglDrawRect(cameraRect);
      glPopMatrix();
      glDisable(GL_LINE_STIPPLE);
    }
  }

  glPopMatrix();

  m_fxGadgetController->draw(isPicking());
}

//=============================================================================

void EditTool::onActivate() {
  if (m_firstTime) {
    m_lockCenterX.setValue(LockCenterX ? 1 : 0);
    m_lockCenterY.setValue(LockCenterY ? 1 : 0);
    m_lockPositionX.setValue(LockPositionX ? 1 : 0);
    m_lockPositionY.setValue(LockPositionY ? 1 : 0);
    m_lockRotation.setValue(LockRotation ? 1 : 0);
    m_lockShearH.setValue(LockShearH ? 1 : 0);
    m_lockShearV.setValue(LockShearV ? 1 : 0);
    m_lockScaleH.setValue(LockScaleH ? 1 : 0);
    m_lockScaleV.setValue(LockScaleV ? 1 : 0);
    m_lockGlobalScale.setValue(LockGlobalScale ? 1 : 0);

    m_showEWNSposition.setValue(ShowEWNSposition ? 1 : 0);
    m_showZposition.setValue(ShowZposition ? 1 : 0);
    m_showSOposition.setValue(ShowSOposition ? 1 : 0);
    m_showRotation.setValue(ShowRotation ? 1 : 0);
    m_showGlobalScale.setValue(ShowGlobalScale ? 1 : 0);
    m_showHVscale.setValue(ShowHVscale ? 1 : 0);
    m_showShear.setValue(ShowShear ? 1 : 0);
    m_showCenterPosition.setValue(ShowCenterPosition ? 1 : 0);

    m_fxGadgetController = new FxGadgetController(this);

    /*
m_foo.setTool(this);
m_foo.setFxHandle(getApplication()->getCurrentFx());
*/

    m_firstTime = false;
  }

  TStageObjectId objId = getObjectId();
  if (objId == TStageObjectId::NoneId) {
    int index              = getColumnIndex();
    if (index == -1) objId = TStageObjectId::CameraId(0);
    objId                  = TStageObjectId::ColumnId(index);
  }
  TTool::getApplication()->getCurrentObject()->setObjectId(objId);
}

//=============================================================================

void EditTool::onDeactivate() {}

//-----------------------------------------------------------------------------

bool EditTool::onPropertyChanged(std::string propertyName) {
  if (propertyName == m_lockCenterX.getName())
    LockCenterX = (int)m_lockCenterX.getValue();

  else if (propertyName == m_lockCenterY.getName())
    LockCenterY = (int)m_lockCenterY.getValue();

  else if (propertyName == m_lockPositionX.getName())
    LockPositionX = (int)m_lockPositionX.getValue();

  else if (propertyName == m_lockPositionY.getName())
    LockPositionY = (int)m_lockPositionY.getValue();

  else if (propertyName == m_lockRotation.getName())
    LockRotation = (int)m_lockRotation.getValue();

  else if (propertyName == m_lockShearH.getName())
    LockShearH = (int)m_lockShearH.getValue();

  else if (propertyName == m_lockShearV.getName())
    LockShearV = (int)m_lockShearV.getValue();

  else if (propertyName == m_lockScaleH.getName())
    LockScaleH = (int)m_lockScaleH.getValue();

  else if (propertyName == m_lockScaleV.getName())
    LockScaleV = (int)m_lockScaleV.getValue();

  else if (propertyName == m_lockGlobalScale.getName())
    LockGlobalScale = (int)m_lockGlobalScale.getValue();

  else if (propertyName == m_showEWNSposition.getName())
    ShowEWNSposition = (int)m_showEWNSposition.getValue();

  else if (propertyName == m_showZposition.getName())
    ShowZposition = (int)m_showZposition.getValue();

  else if (propertyName == m_showSOposition.getName())
    ShowSOposition = (int)m_showSOposition.getValue();

  else if (propertyName == m_showRotation.getName())
    ShowRotation = (int)m_showRotation.getValue();

  else if (propertyName == m_showGlobalScale.getName())
    ShowGlobalScale = (int)m_showGlobalScale.getValue();

  else if (propertyName == m_showHVscale.getName())
    ShowHVscale = (int)m_showHVscale.getValue();

  else if (propertyName == m_showShear.getName())
    ShowShear = (int)m_showShear.getValue();

  else if (propertyName == m_showCenterPosition.getName())
    ShowCenterPosition = (int)m_showCenterPosition.getValue();

  /*-- Active Axis の変更 --*/
  else if (propertyName == m_activeAxis.getName()) {
    std::wstring activeAxis = m_activeAxis.getValue();
    if (activeAxis == L"Position")
      m_what = Translation;
    else if (activeAxis == L"Rotation")
      m_what = Rotation;
    else if (activeAxis == L"Scale")
      m_what = Scale;
    else if (activeAxis == L"Shear")
      m_what = Shear;
    else if (activeAxis == L"Center")
      m_what = Center;
    else if (activeAxis == L"All")
      m_what = None;
  }

  return true;
}

//-----------------------------------------------------------------------------

int EditTool::getCursorId() const {
  /*--- FxParameter操作中のカーソル ---*/
  if (m_highlightedDevice >= 1000) return ToolCursor::FxGadgetCursor;

  /*--- カーソルをアクティブな軸に応じて選ぶ --*/
  std::wstring activeAxis = m_activeAxis.getValue();
  if (activeAxis == L"Position") {
    if (m_highlightedDevice == ZTranslation)
      return ToolCursor::MoveZCursor;
    else if (LockPositionX && LockPositionY)
      return ToolCursor::DisableCursor;
    else if (LockPositionX)
      return ToolCursor::MoveNSCursor;
    else if (LockPositionY)
      return ToolCursor::MoveEWCursor;
    else
      return ToolCursor::MoveCursor;
  } else if (activeAxis == L"Rotation") {
    return ToolCursor::RotCursor;
  } else if (activeAxis == L"Scale") {
    if (m_highlightedDevice == ScaleXY) {
      if (LockScaleH && LockScaleV)
        return ToolCursor::DisableCursor;
      else if (LockScaleH)
        return ToolCursor::ScaleVCursor;
      else if (LockScaleV)
        return ToolCursor::ScaleHCursor;
      else
        return ToolCursor::ScaleHVCursor;
    } else
      return ToolCursor::ScaleGlobalCursor;
  } else if (activeAxis == L"Shear") {
    if (LockShearH && LockShearV)
      return ToolCursor::DisableCursor;
    else if (LockShearH)
      return ToolCursor::ScaleVCursor;
    else if (LockShearV)
      return ToolCursor::ScaleHCursor;
    else
      return ToolCursor::ScaleCursor;
  } else if (activeAxis == L"Center") {
    if (LockCenterX && LockCenterY)
      return ToolCursor::DisableCursor;
    else if (LockCenterX)
      return ToolCursor::MoveNSCursor;
    else if (LockCenterY)
      return ToolCursor::MoveEWCursor;
    else
      return ToolCursor::MoveCursor;
  } else
    return ToolCursor::StrokeSelectCursor;
}

//=============================================================================

EditTool arrowTool;
