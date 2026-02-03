

#include "toonz/logger.h"

Logger::Logger() {}

Logger *Logger::instance() {
  static Logger _instance;
  return &_instance;
}

void Logger::add(std::wstring s) {
  m_rows.push_back(s);
  for (int i = 0; i < (int)m_listeners.size(); i++) m_listeners[i]->onAdd();
}

void Logger::clear() { m_rows.clear(); }

int Logger::getRowCount() const { return m_rows.size(); }

std::wstring Logger::getRow(int i) const {
  if (0 <= i && i < (int)m_rows.size())
    return m_rows[i];
  else
    return L"";
}

void Logger::addListener(Listener *listener) {
  if (std::find(m_listeners.begin(), m_listeners.end(), listener) ==
      m_listeners.end())
    m_listeners.push_back(listener);
}

void Logger::removeListener(Listener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}
