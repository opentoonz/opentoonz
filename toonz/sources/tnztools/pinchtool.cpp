

//-----------------------------------------------------------------------------
//  pinchtool.cpp
//
//  ChangeLog:
//
//    24/04/2005  Fab
//                  Refactoring:
//                    - removed unused headers
//                    - changed zero finder algorithm (secant vs bisection)
//    22/05/2005  Fab
//                  Added header file.
//                  Added adapting dragger.
//
//    12/06/2005  Fab
//                  Changed deformator API.
//                  Added designer for dragger.
//
//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif

#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"
#include "tdebugmessage.h"
#include "tgeometry.h"
#include "tgl.h"
#include "tproperty.h"
#include "tstream.h"
#include "tstrokeutil.h"
#include "tthreadmessage.h"
#include "tools/pinchtool.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

#include "ext/StrokeDeformation.h"
#include "ext/SmoothDeformation.h"
#include "ext/OverallDesigner.h"
#include "ext/ExtUtil.h"
#include "ext/Selector.h"
#include "ext/CornerDeformation.h"
#include "ext/StraightCornerDeformation.h"
#include "ext/StrokeDeformation.h"

#include <memory>
#include <algorithm>

using namespace ToolUtils;
using namespace ToonzExt;

// viene usato??
class TGlobalChange;

namespace {

struct GLMatrixGuard {
  GLMatrixGuard() { glPushMatrix(); }
  ~GLMatrixGuard() { glPopMatrix(); }
};

}  // namespace

//-----------------------------------------------------------------------------

PinchTool::PinchTool()
    : TTool("T_Pinch")
    , m_active(false)
    , m_cursorEnabled(false)
    , m_draw(false)
    , m_undo(0)
    , m_showSelector(true)
    , m_toolRange("Size:", 1.0, 1000.0, 500.0)  // W_ToolOptions_PinchTool
    , m_toolCornerSize("Corner:", 1.0, 180.0,
                       160.0)          // W_ToolOptions_PinchCorner
    , m_autoOrManual("Manual", false)  // W_ToolOptions_PinchManual
    , m_deformation(new ToonzExt::StrokeDeformation)
    , m_selector(500, 10, 1000) {
  bind(TTool::Vectors);

  m_prop.bind(m_toolCornerSize);
  m_prop.bind(m_autoOrManual);
  m_prop.bind(m_toolRange);

  CornerDeformation::instance()->setCursorId(ToolCursor::PinchWaveCursor);
  SmoothDeformation::instance()->setCursorId(ToolCursor::PinchCursor);
  StraightCornerDeformation::instance()->setCursorId(
      ToolCursor::PinchAngleCursor);
  assert(m_deformation && "Can not create a deformation CATASTROFIC!!!");

  TMouseEvent dummy;
  updateInterfaceStatus(dummy);
  m_autoOrManual.setId("Manual");
}

//-----------------------------------------------------------------------------

PinchTool::~PinchTool() {
  delete m_deformation;
  m_deformation = 0;
}

//-----------------------------------------------------------------------------

void PinchTool::updateTranslation() {
  m_toolRange.setQStringName(tr("Size:"));
  m_toolCornerSize.setQStringName(tr("Corner:"));
  m_autoOrManual.setQStringName(tr("Manual"));
}

//-----------------------------------------------------------------------------

TStroke *PinchTool::getClosestStroke(const TPointD &pos, double &w) const {
  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return 0;
  double dist = 0;
  UINT index;
  if (vi->getNearestStroke(pos, w, index, dist, true))
    return vi->getStroke(index);
  else
    return 0;
}

//-----------------------------------------------------------------------------

void PinchTool::updateInterfaceStatus(const TMouseEvent &event) {
  assert(getPixelSize() > 0 && "Pixel size is lower than 0!!!");

  m_status.isManual_            = m_autoOrManual.getValue();
  m_status.pixelSize_           = getPixelSize();
  m_status.cornerSize_          = (int)m_toolCornerSize.getValue();
  m_status.lengthOfAction_      = m_toolRange.getValue();
  m_status.deformerSensitivity_ = 0.01 * getPixelSize();

  m_status.key_event_ = ContextStatus::NONE;

  // mutual exclusive
  if (event.isCtrlPressed()) m_status.key_event_  = ContextStatus::CTRL;
  if (event.isShiftPressed()) m_status.key_event_ = ContextStatus::SHIFT;

  // TODO:  **DEVE** essere fatto dentro la costruzione di TMouseEvent
  // nel codice di Toonz/Tab/ecc. **NON** ci devono essere ifdef MACOSX se e'
  // possibile
  // evitarlo. Qua sotto ci deve essere solo if(event.isShiftPressed)
  /*#ifdef MACOSX
if(event.isLockPressed() )
#else*/
  if (event.isAltPressed())
    //#endif
    m_status.key_event_ = ContextStatus::ALT;

  m_selector.setStroke(0);
  m_selector.setVisibility(m_status.isManual_ && m_showSelector);
  m_selector.setLength(m_status.lengthOfAction_);
}

//-----------------------------------------------------------------------------

void PinchTool::updateStrokeStatus(TStroke *stroke, double w) {
  assert(stroke && "Stroke is null!!!");
  assert(0.0 <= w && w <= 1.0 && "Stroke's parameter is out of range [0,1]!!!");

  if (!stroke || w < 0.0 || w > 1.0) return;

  // start update the status
  m_status.stroke2change_ = stroke;
  m_status.w_             = w;

  assert(stroke->getLength() >= 0.0 && "Wrong length in stroke!!!");
}

//-----------------------------------------------------------------------------

int PinchTool::updateCursor() const {
  if (!(TVectorImageP)getImage(false)) return ToolCursor::CURSOR_NO;

  return m_deformation->getCursorId();
}

//---------------------------------------------------------------------------

void PinchTool::leftButtonDown(const TPointD &pos, const TMouseEvent &event) {
  m_curr = m_down = pos;

  if (!m_active && !m_selector.isSelected()) {
    m_active = false;

    assert(m_undo == 0);

    StrokeDeformation *deformation = m_deformation;

    TVectorImageP vi = TImageP(getImage(true));

    if (!vi) return;

    m_active = true;

    ContextStatus *status = &m_status;

    // reset status
    status->init();

    double w, dist2;

    // find nearest stroke
    if (vi->getNearestStroke(m_down, w, m_n, dist2, true)) {
      TStroke *stroke = vi->getStroke(m_n);
      assert(stroke && "Not valid stroke found!!!");
      if (!stroke) return;

      updateStrokeStatus(stroke, w);

      // set parameters from sliders
      updateInterfaceStatus(event);

      deformation->activate(status);

      // stroke can be changed (replaced by another) during deformation activate
      if (TTool::getApplication()->getCurrentObject()->isSpline())
        m_undo = new ToolUtils::UndoPath(
            getXsheet()->getStageObject(getObjectId())->getSpline());
      else {
        TXshSimpleLevel *sl =
            TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
        assert(sl);
        TFrameId id = getCurrentFid();
        m_undo      = new UndoModifyStrokeAndPaint(sl, id, m_n);
      }
    }
  }

  m_selector.mouseDown(m_curr);
  m_prev = m_curr;

  invalidate();
}

//-----------------------------------------------------------------------------

void PinchTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  m_curr = pos;

  if (m_selector.isSelected()) {
    m_selector.mouseDrag(m_curr);
    TDoubleProperty::Range prop_range = m_toolRange.getRange();
    double val_in_range               = m_selector.getLength();

    // set value in range
    val_in_range =
        std::max(std::min(val_in_range, prop_range.second), prop_range.first);
    try {
      m_toolRange.setValue(val_in_range);
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    } catch (TDoubleProperty::RangeError &) {
      m_toolRange.setValue((prop_range.first + prop_range.second) * 0.5);
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    m_selector.setLength(m_toolRange.getValue());
  } else {
    TVectorImageP vi(getImage(true));

    ContextStatus *status = &m_status;
    if (!vi || !status->stroke2change_ || !m_active) return;

    QMutexLocker lock(vi->getMutex());

    // assert( status->stroke2change_->getLength() >= 0.0 );

    StrokeDeformation *deformation = m_deformation;

    TPointD delta = m_curr - m_prev;

    deformation->update(delta);
  }

  m_prev = m_curr;

  // moveCursor(pos);
  invalidate();
}

//-----------------------------------------------------------------------------

void PinchTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (!m_active || m_selector.isSelected()) return;

  m_active = false;

  TVectorImageP vi(getImage(true));

  ContextStatus *status = &m_status;

  if (!vi || !status->stroke2change_) {
    delete m_undo;
    m_undo = 0;
    return;
  }

  // if mouse position is unchanged doesn't modify current stroke
  if (areAlmostEqual(m_down, pos, PickRadius * status->pixelSize_)) {
    assert(m_undo);
    delete m_undo;
    m_undo = 0;

// to avoid red line tool on stroke
#ifdef _WIN32
    invalidate(
        status->stroke2change_->getBBox().enlarge(status->pixelSize_ * 13));
#else
    invalidate();
#endif
    m_deformation->deactivate();
    status->stroke2change_ = 0;
    return;
  }

  QMutexLocker lock(vi->getMutex());

  TStroke *deactivateStroke          = m_deformation->deactivate();
  deactivateStroke->outlineOptions() = status->stroke2change_->outlineOptions();
  replaceStroke(status->stroke2change_, deactivateStroke, m_n, vi);

  status->stroke2change_ = 0;

  vi->notifyChangedStrokes(m_n);

#ifdef _DEBUG
  vi->checkIntersections();
#endif
  invalidate();
#ifdef _DEBUG
  vi->checkIntersections();
#endif

  moveCursor(pos);

  notifyImageChanged();

  assert(m_undo);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
}

//-----------------------------------------------------------------------------

void PinchTool::onEnter() {
  m_draw = true;
  // per sicurezza
  // m_status.stroke2change_ = 0;
  // m_selector.setStroke(0);
}

//-----------------------------------------------------------------------------

void PinchTool::onLeave() {
  if (!m_active) m_draw   = false;
  m_status.stroke2change_ = 0;

  // Abbiamo dovuto commentarlo perche' stranamente
  // l'onLeave viene chiamata anche quando viene premuto il tasto del mouse
  // per iniziare a fare drag utilizzando il Selector
  // setStroke al suo interno resetta lo status del Selector (lo mette a NONE)

  //  m_selector.setStroke(0);

  m_deformation->reset();
}

//-----------------------------------------------------------------------------

void PinchTool::onImageChanged() {
  m_status.stroke2change_ = 0;
  m_deformation->reset();

  double w        = 0;
  TStroke *stroke = getClosestStroke(m_lastMouseEvent.m_pos, w);
  if (stroke) {
    // set parameters from sliders
    updateInterfaceStatus(m_lastMouseEvent);

    // update information about current stroke
    updateStrokeStatus(stroke, w);
  }

  m_selector.setStroke(stroke);
  invalidate();
}

//-----------------------------------------------------------------------------

void PinchTool::draw() {
  GLMatrixGuard guard;

  TVectorImageP img(getImage(true));
  if (!img ||
      img->getStrokeCount() ==
          0)  // Controllo che il numero degli stroke nell'immagine sia != 0
    return;
  if (!m_draw) return;

  ContextStatus *status = &m_status;
  if (!status) return;

  StrokeDeformation *deformation = m_deformation;

  OverallDesigner designer((int)m_curr.x, (int)m_curr.y);

  // m_active == true means that a button down is done (drag)
  if (!m_active) {
    if (m_cursorEnabled) {
      glColor3d(1, 0, 1);
      if (m_cursor.thick > 0) tglDrawCircle(m_cursor, m_cursor.thick);
      tglDrawCircle(m_cursor, m_cursor.thick + 4 * status->pixelSize_);
    }
  }

  // internal deformer can be changed during draw
  if (!m_selector.isSelected()) deformation->draw(&designer);

  m_selector.draw(&designer);
  CHECK_ERRORS_BY_GL;
}

//-----------------------------------------------------------------------------

void PinchTool::invalidateCursorArea() {
  double r = m_cursor.thick + 6;
  TPointD d(r, r);
  invalidate(TRectD(m_cursor - d, m_cursor + d));
}

//-----------------------------------------------------------------------------

void PinchTool::mouseMove(const TPointD &pos, const TMouseEvent &event) {
  if (m_active) return;

  if (!m_draw) m_draw = true;

  ContextStatus *status = &m_status;
  m_curr                = pos;

  const int pixelRange = 3;
  if (abs(m_lastMouseEvent.m_pos.x - event.m_pos.x) < pixelRange &&
      abs(m_lastMouseEvent.m_pos.y - event.m_pos.y) < pixelRange &&
      m_lastMouseEvent.getModifiersMask() == event.getModifiersMask())
    return;

  m_lastMouseEvent = event;
  double w         = 0;
  TStroke *stroke  = getClosestStroke(pos, w);
  if (stroke) {
    // set parameters from sliders
    updateInterfaceStatus(event);

    // update information about current stroke
    updateStrokeStatus(stroke, w);

    // retrieve the currect m_deformation and
    // prepare to design and modify
    if (m_deformation) m_deformation->check(status);

    m_selector.setStroke(stroke);
    m_selector.mouseMove(m_curr);
  } else {
    m_status.stroke2change_ = 0;
    m_selector.setStroke(0);
    return;
  }

  m_prev          = m_curr;
  m_cursorEnabled = moveCursor(pos);

  if (m_cursorEnabled) invalidate();

  //  TNotifier::instance()->notify(TToolChange());
}

//-----------------------------------------------------------------------------

bool PinchTool::keyDown(QKeyEvent *event) {
  if (!m_active) m_deformation->reset();

#if 0
  char c = (char)key;
  if(c == 'p' ||
     c == 'P' )
  {
    TVectorImageP vimage(getImage());      
    if(!vimage)
      return false;
    
    char  fileName[] = {"c:\\toonz_input.sdd"};
    ofstream  out_file(fileName);
    if(!out_file)
    {
      cerr<<"Error on opening: "<<fileName<<endl;
      return false;
    }
    
    out_file<<"# VImage info\n";
    out_file<<"# Number of stroke:\n";
    const int max_number_of_strokes = vimage->getStrokeCount();
    out_file<<max_number_of_strokes<<endl;
    int number_of_strokes = 0;
    const int cp_for_row=3;
    while( number_of_strokes<max_number_of_strokes)
    {
      TStroke* ref = vimage->getStroke(number_of_strokes);
      out_file<<endl;
      out_file<<"# Number of control points for stroke:\n";
      const int max_number_of_cp = ref->getControlPointCount();
      out_file<<max_number_of_cp <<endl;
      out_file<<"# Control Points:\n";
      int number_of_cp=0;
      while( number_of_cp<max_number_of_cp )
      {
        out_file<<ref->getControlPoint(number_of_cp)<<" ";
        if(!((number_of_cp+1)%cp_for_row)) // add a new line after ten points
          out_file<<endl;
        number_of_cp++;
      }
      out_file<<endl;
      number_of_strokes++;
    }
  }
#endif
  return true;
}

//-----------------------------------------------------------------------------

bool PinchTool::moveCursor(const TPointD &pos) {
  double w        = 0.0;
  TStroke *stroke = getClosestStroke(pos, w);
  if (!stroke) return false;

  m_cursor = stroke->getThickPoint(w);
  return true;
}

//-----------------------------------------------------------------------------

void PinchTool::onActivate() {
  //  getApplication()->editImageOrSpline();
  //  TNotifier::instance()->attach(this);
  // per sicurezza
  m_status.stroke2change_ = 0;
  m_selector.setStroke(0);
}

//-----------------------------------------------------------------------------

void PinchTool::onDeactivate() {
  m_draw = false;
  delete m_undo;
  m_undo   = 0;
  m_active = false;
  m_deformation->reset();
  //  TNotifier::instance()->detach(this);
}

//-----------------------------------------------------------------------------

void PinchTool::update(const TGlobalChange &) {
  m_cursor = TConsts::natp;
  m_selector.setStroke(0);
  m_selector.setVisibility(m_autoOrManual.getValue() && m_showSelector);
  if (m_deformation) delete m_deformation->deactivate();
  // per sicurezza
  m_status.stroke2change_ = 0;
}

//-----------------------------------------------------------------------------

static PinchTool pinchTool;
