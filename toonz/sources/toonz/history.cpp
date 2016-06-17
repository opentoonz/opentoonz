

#include "history.h"
#include "tsystem.h"
#include "tenv.h"
//#include "tutil.h"
#include "tfilepath_io.h"
#include "toonz/toonzfolders.h"

//#include <fstream.h>

inline TFilePath getHistoryFile() {
  return TEnv::getConfigDir() +
         (TSystem::getUserName().toStdString() + "_history.txt");
}

std::string History::Day::getDate() const {
  QDate today     = QDate::currentDate();
  QDate yesterday = today.addDays(-1);
  if (m_timeStamp == today)
    return "today";
  else if (m_timeStamp == yesterday)
    return "yesterday";
  else
    return m_timeStamp.toString(Qt::SystemLocaleDate)
        .toStdString();  // "%d %b %A %x");
}

void History::Day::getFiles(std::vector<TFilePath> &files) const {
  std::set<Item>::const_iterator it;
  for (it = m_items.begin(); it != m_items.end(); ++it)
    files.push_back(it->m_filePath);
}

History::History() { load(); }

History::~History() { clearPointerContainer(m_days); }

void History::addItem(const TFilePath &fp) {
  bool changed = false;
  QDate now    = QDate::currentDate();
  Day *day     = 0;
  int i        = 0;
  for (; i < (int)m_days.size(); i++)
    if (now == m_days[i]->m_timeStamp) {
      day = m_days[i];
      break;
    }
  if (i == (int)m_days.size()) {
    changed = true;
    m_days.push_back(new Day(now));
    day = m_days.back();
    if (m_days.size() > 7) {
      delete m_days[0];
      m_days.erase(m_days.begin());
    }
  }
  assert(day);
  Item fpItem(fp);
  std::set<Item>::iterator j = day->m_items.find(fpItem);
  if (j == day->m_items.end()) {
    day->m_items.insert(fpItem);
    changed = true;
  }
  if (changed) save();
}

History *History::instance() {
  static History theHistory;
  return &theHistory;
}

const History::Day *History::getDay(int i) const {
  assert(0 <= i && i < (int)m_days.size());
  assert(m_days[i]);
  return m_days[i];
}

const History::Day *History::getDay(const QDate &time) const {
  for (int i = 0; i < (int)m_days.size(); i++)
    if (m_days[i]->m_timeStamp == time) return m_days[i];
  return 0;
}

const History::Day *History::getDay(std::string dateString) const {
  for (int i = 0; i < (int)m_days.size(); i++)
    if (m_days[i]->getDate() == dateString) return m_days[i];
  return 0;
}

void History::save() {
  TFilePath fp = getHistoryFile();
  Tofstream os(fp);
  for (int i = 0; i < (int)m_days.size(); i++) {
    Day &day = *m_days[i];
    int d    = day.m_timeStamp.day();
    int m    = day.m_timeStamp.month();
    int y    = day.m_timeStamp.year();
    std::set<Item>::iterator j;
    for (j = day.m_items.begin(); j != day.m_items.end(); ++j) {
      TFilePath fp = j->m_filePath;
      os << d << " " << m << " " << y << " " << fp << std::endl;
    }
  }
}

void History::load() {
  clearPointerContainer(m_days);

  TFilePath fp = getHistoryFile();
  Tifstream is(fp);
  if (!is) return;
  std::map<int, Day *> table;
  std::map<int, Day *>::iterator it;
  for (;;) {
    char buffer[2048];
    int d, m, y;
    is >> d >> m >> y;
    if (is.eof()) break;
    is.get(buffer, sizeof(buffer), '\n');
    char *s = buffer;
    char *t = buffer + strlen(buffer) - 1;
    while (t >= s && (*t == ' ' || *t == '\t' || t[0] == '\n' || t[0] == '\r'))
      t--;
    t[1] = '\0';
    while (*s == ' ' || *s == '\t') s++;
    TFilePath fp(s);

    int id = 500 * y + 50 * m + d;
    Day *day;
    it = table.find(id);
    if (it == table.end())
      table[id] = day = new Day(QDate(y, m, d));
    else
      day = it->second;
    day->m_items.insert(Item(fp));
  }
  for (it = table.begin(); it != table.end(); ++it)
    m_days.push_back(it->second);
}
