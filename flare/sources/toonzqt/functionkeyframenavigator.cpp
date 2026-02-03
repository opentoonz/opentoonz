

#include "toonzqt/functionkeyframenavigator.h"

// TnzQt includes
#include "toonzqt/framenavigator.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"

// TnzBase includes
#include "tdoubleparam.h"

// Qt includes
#include <QPushButton>
#include <QBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QAction>
#include <QIntValidator>

FunctionKeyframeNavigator::FunctionKeyframeNavigator(QWidget *parent)
    : KeyframeNavigator(parent) {}

void FunctionKeyframeNavigator::setCurve(TDoubleParam *curve) {
  if (m_curve.getPointer() == curve) return;
  m_curve = curve;
  if (isVisible()) update();
}

bool FunctionKeyframeNavigator::hasNext() const {
  return m_curve && m_curve->getNextKeyframe(getCurrentFrame()) >= 0;
}

bool FunctionKeyframeNavigator::hasPrev() const {
  return m_curve && m_curve->getPrevKeyframe(getCurrentFrame()) >= 0;
}

bool FunctionKeyframeNavigator::hasKeyframes() const {
  return m_curve && m_curve->hasKeyframes();
}

bool FunctionKeyframeNavigator::isKeyframe() const {
  return m_curve && m_curve->isKeyframe(getCurrentFrame());
}

void FunctionKeyframeNavigator::toggle() {
  if (!m_curve) return;
  int frame    = getCurrentFrame();
  double value = m_curve->getValue(frame);
  if (m_curve->isKeyframe(frame))
    KeyframeSetter::removeKeyframeAt(m_curve.getPointer(), frame);
  else {
    KeyframeSetter setter(m_curve.getPointer());
    setter.createKeyframe(frame);
  }
}

void FunctionKeyframeNavigator::goNext() {
  if (!m_curve) return;
  int k = m_curve->getNextKeyframe(getCurrentFrame());
  if (k < 0) return;
  setCurrentFrame(m_curve->keyframeIndexToFrame(k));
}

void FunctionKeyframeNavigator::goPrev() {
  if (!m_curve) return;
  int k = m_curve->getPrevKeyframe(getCurrentFrame());
  if (k < 0) return;
  setCurrentFrame(m_curve->keyframeIndexToFrame(k));
}

void FunctionKeyframeNavigator::showEvent(QShowEvent *e) {
  KeyframeNavigator::showEvent(e);
}

void FunctionKeyframeNavigator::hideEvent(QHideEvent *e) {
  KeyframeNavigator::hideEvent(e);
}
