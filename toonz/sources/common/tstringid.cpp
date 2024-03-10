

#include <tstringid.h>

#include <QMutex>
#include <QMutexLocker>

//---------------------------------------------------------

struct TStringId::StaticData {
  Map map;
  std::vector<Iterator> iterators;
  Iterator none;
  QMutex mutex;
  StaticData() {
    map[std::string()] = 0;
    none = map.begin();
    iterators.push_back(none);
  }

  static StaticData& instance() {
    static StaticData data;
    return data;
  }
};

//---------------------------------------------------------

const TStringId::Iterator&
TStringId::none()
  { return StaticData::instance().none; }

//---------------------------------------------------------

TStringId::Iterator
TStringId::genIter(const std::string &str) {
  StaticData &data = StaticData::instance();
  if (str.empty()) return data.none;

  QMutexLocker lock(&data.mutex);
  Iterator i = data.map.find(str);
  if (i == data.map.end()) {
    i = data.map.insert(std::pair<std::string, int>(str, (int)data.iterators.size())).first;
    data.iterators.push_back(i);
  }
  return i;
}

//---------------------------------------------------------

TStringId::Iterator
TStringId::findIter(int id) {
  StaticData &data = StaticData::instance();
  if (id <= 0) return data.none;

  QMutexLocker lock(&data.mutex);
  return id < (int)data.iterators.size() ? data.iterators[id] : data.none;
}

//---------------------------------------------------------

TStringId::Iterator
TStringId::findIter(const std::string &str) {
  StaticData &data = StaticData::instance();
  if (str.empty()) return data.none;

  QMutexLocker lock(&data.mutex);
  Iterator i = data.map.find(str);
  return i == data.map.end() ? data.none : i;
}

//---------------------------------------------------------
