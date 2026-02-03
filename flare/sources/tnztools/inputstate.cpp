

#include <tools/inputstate.h>

#include <iomanip>
#include <QKeySequence>


//*****************************************************************************************
//    TKey static members
//*****************************************************************************************

const TKey TKey::shift   ( Qt::Key_Shift   , true );
const TKey TKey::control ( Qt::Key_Control , true );
const TKey TKey::alt     ( Qt::Key_Alt     , true );
const TKey TKey::meta    ( Qt::Key_Meta    , true );


Qt::Key
TKey::mapKey(Qt::Key key) {
  switch(key) {
  case Qt::Key_AltGr: return Qt::Key_Alt;
  default: break;
  }
  return key;
}


bool
TKey::isModifier(Qt::Key key) {
  key = mapKey(key);
  return key == Qt::Key_Shift
      || key == Qt::Key_Control
      || key == Qt::Key_Alt
      || key == Qt::Key_AltGr
      || key == Qt::Key_Meta;
}


bool
TKey::isNumber(Qt::Key key) {
  key = mapKey(key);
  return key >= Qt::Key_0 && key <= Qt::Key_9;
}


//*****************************************************************************************
//    TInputState implementation
//*****************************************************************************************

TInputState::TInputState():
  m_ticks(),
  m_keyHistory(new KeyHistory())
  { }

TInputState::~TInputState()
  { }

void
TInputState::touch(TTimerTicks ticks) {
  if (m_ticks < ticks)
    m_ticks = ticks;
  else
    ++m_ticks;
}

TInputState::ButtonHistory::Pointer
TInputState::buttonHistory(DeviceId device) const {
  ButtonHistory::Pointer &history = m_buttonHistories[device];
  if (!history) history = new ButtonHistory();
  return history;
}

TInputState::ButtonState::Pointer
TInputState::buttonFindAny(Button button, DeviceId &outDevice) {
  for(ButtonHistoryMap::const_iterator i = m_buttonHistories.begin(); i != m_buttonHistories.end(); ++i) {
    ButtonState::Pointer state = i->second->current()->find(button);
    if (state) {
      outDevice = i->first;
      return state;
    }
  }
  outDevice = DeviceId();
  return ButtonState::Pointer();
}


namespace {
  
template<typename T>
void printKey(const T &k, std::ostream &stream)
  { stream << k; }

template<>
void printKey<TKey>(const TKey &k, std::ostream &stream) {
  stream << QKeySequence(k.key).toString().toStdString() << "[" << std::hex << k.key << std::dec;
  if (k.generic) stream << "g";
  if (k.numPad) stream << "n";
  stream << "]";
}

template<typename T>
class Print {
public:
  typedef T Type;
  typedef TKeyHistoryT<Type> History;
  typedef typename History::StatePointer StatePointer;
  typedef typename History::StateMap StateMap;
  typedef typename History::LockSet LockSet;

  static void print(const History &history, std::ostream &stream, const std::string &tab) {
    const StateMap &states = history.getStates();
    stream << tab << "states: " << std::endl;
    for(typename StateMap::const_iterator i = states.begin(); i != states.end(); ++i) {
      stream << tab << "- " << i->first << std::endl;
      for(StatePointer p = i->second; p; p = p->previous) {
        stream << tab << "- - " << p->ticks << ": ";
        printKey(p->value, stream);
        stream << std::endl;
      }
    }
    
    const LockSet &locks = history.getLocks();
    stream << tab << "locks: ";
    for(typename LockSet::const_iterator i = locks.begin(); i != locks.end(); ++i) {
      if (i != locks.begin()) stream << ", ";
      stream << *i;
    }
    stream << std::endl;
  }
};
}


void TInputState::print(std::ostream &stream, const std::string &tab) const {
  stream << tab << "keys:" << std::endl;
  Print<TKey>::print(*m_keyHistory, stream, tab + "  ");
  for(ButtonHistoryMap::const_iterator i = m_buttonHistories.begin(); i != m_buttonHistories.end(); ++i) {
    stream << tab << "buttons[" << i->first << "]:" << std::endl;
    Print<TKey>::print(*m_keyHistory, stream, tab + "  ");
  }
}

