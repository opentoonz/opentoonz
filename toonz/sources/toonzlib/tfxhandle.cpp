

#include "toonz/tfxhandle.h"
#include "tfx.h"

TFxHandle::TFxHandle() : m_fx(0), m_previousActionString() {}

TFxHandle::~TFxHandle() {
  if (m_fx) m_fx->release();
  m_fx = 0;
}

// do not switch fx settings when single-clicking the fx node in the schematic
void TFxHandle::setFx(TFx *fx, bool doSwitchFxSettings) {
  if (m_fx == fx) return;
  if (fx) fx->addRef();
  if (m_fx) m_fx->release();
  m_fx = fx;
  emit fxSwitched();
  if (doSwitchFxSettings) emit fxSettingsShouldBeSwitched();
}

void TFxHandle::onColumnChanged() {}
