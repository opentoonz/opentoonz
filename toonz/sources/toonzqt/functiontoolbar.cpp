

#include "toonzqt/functiontoolbar.h"

// TnzQt includes
#include "toonzqt/doublefield.h"
#include "toonzqt/functionkeyframenavigator.h"
#include "toonzqt/functionselection.h"
#include "toonzqt/framenavigator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"

// Qt includes
#include <QLabel>
#include <QApplication>

//=========================================================================

/*
   * two states: curve_defined / curve_not_defined

   * three widgets: value_fld, frame_nav, keyframe_nav
                    (if curve_not_defined only keyframe_nav is visible)

   * user actions

     - value_fld changed / keyframe_nav changed (key icon) => modify current
   curve
     - frame_nav changed / keyframe_nav changed (arrows) => modify the 'extern'
   frame

   * 'extern' events:

     - 'extern' frame changed => update fields
     - curve selected => update fields (and possibly change status)
*/

//=========================================================================

FunctionToolbar::FunctionToolbar(QWidget *parent)
    : DVGui::ToolBar(parent), m_frameHandle(0), m_curve(0), m_selection(0) {
  setFixedHeight(28);

  m_valueToolbar    = new DVGui::ToolBar();
  m_keyframeToolbar = new DVGui::ToolBar();

  // value field
  m_valueToolbar->addWidget(new QLabel(tr("Value")));
  m_valueFld = new DVGui::MeasuredDoubleLineEdit();
  // frame navigator
  m_frameNavigator = new FrameNavigator(this);
  // keyframe navigator
  m_keyframeNavigator          = new FunctionKeyframeNavigator(this);
  QWidget *space               = new QWidget(this);
  DVGui::ToolBar *spaceToolBar = new DVGui::ToolBar();
  spaceToolBar->addWidget(space);

  m_valueFld->setStyleSheet("height:14px;margin-right:5px;margin-top:2px;");
  space->setMinimumHeight(22);
  space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  QIcon toggleIcon      = createQIconOnOff("swap", false);
  QAction *toggleAction = new QAction(tr("&Open Function Curve Editor"), this);
  assert(toggleAction);
  toggleAction->setIcon(toggleIcon);

  //-----layout
  m_valueFldAction          = addWidget(m_valueToolbar);
  m_keyframeNavigatorAction = addWidget(m_keyframeToolbar);

  m_valueToolbar->addWidget(m_valueFld);
  m_valueToolbar->addSeparator();

  m_keyframeToolbar->addWidget(m_keyframeNavigator);
  m_keyframeToolbar->addSeparator();

  addWidget(m_frameNavigator);
  addSeparator();
  addWidget(spaceToolBar);
  addSeparator();
  addAction(toggleAction);

  bool ret = connect(m_valueFld, SIGNAL(valueChanged()), this,
                     SLOT(onValueFieldChanged()));
  ret = ret && connect(m_frameNavigator, SIGNAL(frameSwitched()), this,
                       SLOT(onNavFrameSwitched()));
  ret = ret && connect(toggleAction, SIGNAL(triggered()), this,
                       SIGNAL(numericalColumnToggled()));

  m_valueFldAction->setVisible(false);
  m_keyframeNavigatorAction->setVisible(false);
  assert(ret);
}

//-------------------------------------------------------------------

FunctionToolbar::~FunctionToolbar() {
  if (m_curve) {
    m_curve->removeObserver(this);
    m_curve->release();
  }
}

//-------------------------------------------------------------------

bool FunctionToolbar::anyWidgetHasFocus() {
  return hasFocus() || m_valueToolbar->hasFocus() ||
         m_keyframeToolbar->hasFocus() || m_valueFld->hasFocus() ||
         m_frameNavigator->anyWidgetHasFocus() ||
         m_keyframeNavigator->hasFocus();
}

//-------------------------------------------------------------------

void FunctionToolbar::setCurve(TDoubleParam *curve) {
  if (curve == m_curve) return;
  bool curveDefined = curve != 0;
  m_valueFldAction->setVisible(curveDefined);
  m_keyframeNavigatorAction->setVisible(curveDefined);
  m_keyframeNavigator->setCurve(curve);
  if (curve) {
    curve->addObserver(this);
    curve->addRef();
  }
  if (m_curve) {
    m_curve->removeObserver(this);
    m_curve->release();
  }
  m_curve = curve;
  if (m_curve) {
    m_valueFld->setMeasure(m_curve->getMeasureName());
    setFrame(m_frameNavigator->getFrame());
  } else {
    m_valueFld->setMeasure("");
    m_valueFld->setValue(0);
  }
}

//-------------------------------------------------------------------

void FunctionToolbar::setFrame(double frame) {
  m_frameNavigator->setFrame(tround(frame), false);
  if (m_curve)
    m_valueFld->setValue(m_curve->getValue(frame));
  else
    m_valueFld->setValue(0);
}

//-------------------------------------------------------------------

void FunctionToolbar::onValueFieldChanged() {
  if (!m_curve) return;
  int frame     = m_frameNavigator->getFrame();
  double value  = m_valueFld->getValue();
  double fValue = m_curve->getValue(frame);
  if (value == fValue) return;
  KeyframeSetter::setValue(m_curve, frame, value);
}

//-------------------------------------------------------------------

void FunctionToolbar::onFrameSwitched() { setFrame(m_frameHandle->getFrame()); }

//-------------------------------------------------------------------

void FunctionToolbar::onNavFrameSwitched() {
  if (m_frameHandle) m_frameHandle->setFrame(m_frameNavigator->getFrame());
}

//-------------------------------------------------------------------

void FunctionToolbar::onSelectionChanged() {
  if (m_selection) {
    if (m_selection->getSelectedKeyframeCount() == 1) {
      QPair<TDoubleParam *, int> k = m_selection->getSelectedKeyframe(0);
      if (k.first == m_curve) {
        int frame = m_curve->keyframeIndexToFrame(k.second);
        setFrame(frame);
        return;
      }
    } else if (m_selection->getSelectedKeyframeCount() > 1) {
      setFrame(m_frameHandle->getFrame());
      m_valueFld->setText("");
      return;
    }
  }

  if (m_frameHandle && m_curve)
    setFrame(m_frameHandle->getFrame());
  else
    m_valueFld->setText("");
}

//-------------------------------------------------------------------

void FunctionToolbar::setFrameHandle(TFrameHandle *frameHandle) {
  if (m_frameHandle) m_frameHandle->disconnect(this);

  m_frameHandle = frameHandle;

  if (m_frameHandle)
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));

  m_keyframeNavigator->setFrameHandle(frameHandle);
}

//-------------------------------------------------------------------

void FunctionToolbar::setSelection(FunctionSelection *selection) {
  if (m_selection != selection) {
    if (m_selection)
      disconnect(m_selection, SIGNAL(selectionChanged()), this,
                 SLOT(onSelectionChanged()));
    m_selection = selection;
    if (m_selection)
      connect(m_selection, SIGNAL(selectionChanged()), this,
              SLOT(onSelectionChanged()));
  }
}

//-------------------------------------------------------------------

void FunctionToolbar::onChange(const TParamChange &) {
  m_keyframeNavigator->update();
}
