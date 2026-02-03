

#include "tlogger.h"
#include "tsystem.h"
#include "tthreadmessage.h"
#include "tconvert.h"

#include <fstream>
#include <set>
#include <vector>
#include <QTimer>

class TLogger::Imp {
public:
  std::vector<TLogger::Message> m_messages;
  std::set<TLogger::Listener *> m_listeners;
  TThread::Mutex m_mutex;

  class ListenerNotifier final : public TThread::Message {
    Imp *m_imp;

  public:
    ListenerNotifier(Imp *imp) : m_imp(imp) {}
    void onDeliver() override {
      QMutexLocker sl(&m_imp->m_mutex);
      std::set<TLogger::Listener *>::iterator it;
      for (it = m_imp->m_listeners.begin(); it != m_imp->m_listeners.end();
           ++it)
        (*it)->onLogChanged();
    }
    TThread::Message *clone() const override {
      return new ListenerNotifier(*this);
    }
  };

  void notify() {
    // ListenerNotifier(this).send();//onDeliver()
    std::set<TLogger::Listener *>::iterator it;
    for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
      (*it)->onLogChanged();
  }
};

TLogger::TLogger() : m_imp(new Imp()) {}

TLogger::~TLogger() {}

TLogger *TLogger::instance() {
  static TLogger _instance;
  return &_instance;
}

TLogger::Message::Message(MessageType type, std::string text)
    : m_type(type), m_text(text) {
  QTime t     = QTime::currentTime();
  m_timestamp = t.toString("hh:mm:ss.zzz").toStdString();
}

void TLogger::addMessage(const Message &msg) {
  QMutexLocker sl(&m_imp->m_mutex);
  m_imp->m_messages.push_back(msg);
  m_imp->notify();
}

void TLogger::clearMessages() {
  QMutexLocker sl(&m_imp->m_mutex);
  m_imp->m_messages.clear();
  m_imp->notify();
}

int TLogger::getMessageCount() const {
  QMutexLocker sl(&m_imp->m_mutex);
  return m_imp->m_messages.size();
}

TLogger::Message TLogger::getMessage(int index) const {
  QMutexLocker sl(&m_imp->m_mutex);
  assert(0 <= index && index < getMessageCount());
  return m_imp->m_messages[index];
}

void TLogger::addListener(TLogger::Listener *listener) {
  m_imp->m_listeners.insert(listener);
}

void TLogger::removeListener(TLogger::Listener *listener) {
  m_imp->m_listeners.erase(listener);
}

TLogger::Stream::Stream(MessageType type) : m_type(type), m_text() {}

TLogger::Stream::~Stream() {
  try {
    TLogger::Message msg(m_type, m_text);
    TLogger::instance()->addMessage(msg);
  } catch (...) {
  }
}

TLogger::Stream &TLogger::Stream::operator<<(std::string v) {
  m_text += v;
  return *this;
}

TLogger::Stream &TLogger::Stream::operator<<(int v) {
  m_text += std::to_string(v);
  return *this;
}

TLogger::Stream &TLogger::Stream::operator<<(double v) {
  m_text += std::to_string(v);
  return *this;
}

TLogger::Stream &TLogger::Stream::operator<<(const TFilePath &v) {
  m_text += v.getQString().toStdString();
  return *this;
}
