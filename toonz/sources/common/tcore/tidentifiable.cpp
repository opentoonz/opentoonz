

#include "tidentifiable.h"
#include <set>

namespace {

class IdentifierTable {  // singleton

  unsigned long m_lastId;
  std::map<unsigned long, TIdentifiable *> m_table;
  std::set<TIdentifiable *> m_objects;

  IdentifierTable() : m_lastId(0) {}

public:
  static IdentifierTable *instance() {
    // NON DEVE MORIRE
    // static IdentifierTable _instance;
    // return &_instance;
    static IdentifierTable *_instance = 0;
    if (!_instance) _instance         = new IdentifierTable;
    return _instance;
  }

  unsigned long getNextId() { return ++m_lastId; }

  void insert(TIdentifiable *o) {
    unsigned long id = o->getIdentifier();
    std::map<unsigned long, TIdentifiable *>::iterator it = m_table.find(id);
    if (it != m_table.end()) {
      if (it->second == o) return;
      m_objects.erase(it->second);
      it->second = o;
    } else {
      m_table[id] = o;
    }
    m_objects.insert(o);
  }

  void erase(TIdentifiable *o) {
    unsigned long id = o->getIdentifier();
    m_table.erase(id);
    m_objects.erase(o);
  }

  TIdentifiable *fetch(unsigned long id) {
    std::map<unsigned long, TIdentifiable *>::iterator it = m_table.find(id);
    return it == m_table.end() ? 0 : it->second;
  }
};

}  // namespace

TIdentifiable::TIdentifiable() : m_id(0) {}

TIdentifiable::~TIdentifiable() {
  if (m_id != 0) IdentifierTable::instance()->erase(this);
}

TIdentifiable::TIdentifiable(const TIdentifiable &src) : m_id(src.m_id) {}

const TIdentifiable &TIdentifiable::operator=(const TIdentifiable &src) {
  if (src.m_id != m_id && m_id != 0) IdentifierTable::instance()->erase(this);
  m_id = src.m_id;
  return *this;
}

void TIdentifiable::setIdentifier(unsigned long id) {
  bool wasStored = m_id > 0 && IdentifierTable::instance()->fetch(m_id) == this;
  if (m_id != id && m_id != 0) IdentifierTable::instance()->erase(this);
  m_id = id;
  if (wasStored) IdentifierTable::instance()->insert(this);
}

void TIdentifiable::setNewIdentifier() {
  setIdentifier(IdentifierTable::instance()->getNextId());
}

void TIdentifiable::storeByIdentifier() {
  assert(getIdentifier() >= 1);
  IdentifierTable::instance()->insert(this);
}

TIdentifiable *TIdentifiable::fetchByIdentifier(unsigned long id) {
  return IdentifierTable::instance()->fetch(id);
}
