

#include <tools/inputstate.h>

//*****************************************************************************************
//    TKey static members
//*****************************************************************************************

const TKey TKey::shift(Qt::Key_Shift, true);
const TKey TKey::control(Qt::Key_Control, true);
const TKey TKey::alt(Qt::Key_Alt, true);
const TKey TKey::meta(Qt::Key_Meta, true);

Qt::Key TKey::mapKey(Qt::Key key) {
  switch (key) {
  case Qt::Key_AltGr:
    return Qt::Key_Alt;
  default:
    break;
  }
  return key;
}

bool TKey::isModifier(Qt::Key key) {
  key = mapKey(key);
  return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
         key == Qt::Key_AltGr || key == Qt::Key_Meta;
}

bool TKey::isNumber(Qt::Key key) {
  key = mapKey(key);
  return key >= Qt::Key_0 && key <= Qt::Key_9;
}

//*****************************************************************************************
//    TInputState implementation
//*****************************************************************************************

TInputState::TInputState() : m_ticks(), m_keyHistory(new KeyHistory()) {}

TInputState::~TInputState() {}

void TInputState::touch(TTimerTicks ticks) {
  if (m_ticks < ticks)
    m_ticks = ticks;
  else
    ++m_ticks;
}

TInputState::ButtonHistory::Pointer TInputState::buttonHistory(
    DeviceId device) const {
  ButtonHistory::Pointer &history = m_buttonHistories[device];
  if (!history) history           = new ButtonHistory();
  return history;
}

TInputState::ButtonState::Pointer TInputState::buttonFindAny(
    Button button, DeviceId &outDevice) {
  for (ButtonHistoryMap::const_iterator i = m_buttonHistories.begin();
       i != m_buttonHistories.end(); ++i) {
    ButtonState::Pointer state = i->second->current()->find(button);
    if (state) {
      outDevice = i->first;
      return state;
    }
  }
  outDevice = DeviceId();
  return ButtonState::Pointer();
}
