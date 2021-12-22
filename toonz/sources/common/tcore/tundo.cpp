

#include "tundo.h"
#include <deque>

//-----------------------------------------------------------------------------

namespace {

void deleteUndo(const TUndo *undo) { delete undo; }
void callUndo(const TUndo *undo) { undo->undo(); }
void callRedo(const TUndo *undo) { undo->redo(); }
// void callRepeat(const TUndo* undo) {undo->repeat(); }

class TUndoBlock final : public TUndo {
  std::vector<TUndo *> m_undos;
  typedef std::vector<TUndo *>::const_iterator Iterator;
  typedef std::vector<TUndo *>::const_reverse_iterator ReverseIterator;
  mutable bool m_deleted, m_undoing;

public:
  TUndoBlock() : m_deleted(false), m_undoing(false) {}
  ~TUndoBlock() {
    assert(m_undoing == false);
    assert(m_deleted == false);
    m_deleted = true;
    std::for_each(m_undos.begin(), m_undos.end(), deleteUndo);
    m_undos.clear();
  }

  int getSize() const override {
    int size = sizeof(*this);
    for (Iterator cit = m_undos.begin(); cit != m_undos.end(); ++cit)
      size += (*cit)->getSize();
    size += (m_undos.capacity() - m_undos.size()) * sizeof(TUndo *);
    return size;
  }
  int getUndoCount() const { return (int)m_undos.size(); }
  void setLast() {
    for (UINT i = 0; i < m_undos.size(); i++) {
      m_undos[i]->m_isLastInBlock     = (i == 0);
      m_undos[i]->m_isLastInRedoBlock = (i == m_undos.size() - 1);
    }
  }

  void undo() const override {
    assert(!m_deleted);
    assert(!m_undoing);
    m_undoing = true;
    // VERSIONE CORRETTA
    std::for_each(m_undos.rbegin(), m_undos.rend(), callUndo);
    // VERSIONE SBAGLIATA
    // for_each(m_undos.begin(), m_undos.end(), callUndo);
    m_undoing = false;
  }
  void redo() const override {
    assert(!m_deleted);
    // VERSIONE CORRETTA
    std::for_each(m_undos.begin(), m_undos.end(), callRedo);
    // VERSIONE SBAGLIATA
    // for_each(m_undos.rbegin(), m_undos.rend(), callRedo);
  }

  // void repeat() const {
  //  for_each(m_undos.begin(), m_undos.end(), callRepeat);
  //}
  void onAdd() override {}
  void add(TUndo *undo) {
    undo->m_isLastInBlock     = true;
    undo->m_isLastInRedoBlock = true;
    m_undos.push_back(undo);
  }

  void popUndo(int n) {
    if (n == -1) n = m_undos.size();
    if (m_undos.empty() || n <= 0) return;
    while (n > 0 && !m_undos.empty()) {
      TUndo *undo = m_undos.back();
      m_undos.pop_back();
      delete undo;
      n--;
    }
  }

  QString getHistoryString() override {
    if (m_undos.empty())
      return TUndo::getHistoryString();
    else if ((int)m_undos.size() == 1)
      return m_undos.back()->getHistoryString();
    else {
      return QString("%1  etc..").arg(m_undos.back()->getHistoryString());
    }
  }

  int getHistoryType() override {
    if (m_undos.empty())
      return TUndo::getHistoryType();
    else
      return m_undos.back()->getHistoryType();
  }
};
}  // namespace

typedef std::deque<TUndo *> UndoList;
typedef UndoList::iterator UndoListIterator;
typedef UndoList::const_iterator UndoListConstIterator;

//-----------------------------------------------------------------------------

struct TUndoManager::TUndoManagerImp {
  UndoList m_undoList;
  UndoListIterator m_current;
  bool m_skipped;
  int m_undoMemorySize;  // in bytes

  std::vector<TUndoBlock *> m_blockStack;

public:
  TUndoManagerImp() : m_skipped(false), m_undoMemorySize(0) {
    m_current = m_undoList.end();
  }
  ~TUndoManagerImp() {}

  void add(TUndo *undo);

public:
  static struct ManagerPtr {
    TUndoManager *m_ptr;

  public:
    ManagerPtr() : m_ptr(0) {}
    ~ManagerPtr() {
      if (m_ptr) delete m_ptr;
      m_ptr = 0;
    }

  } theManager;

private:
  void doAdd(TUndo *undo);
};

//=============================================================================

TUndoManager::TUndoManagerImp::ManagerPtr
    TUndoManager::TUndoManagerImp::theManager;

//-----------------------------------------------------------------------------

TUndoManager *TUndoManager::manager() {
  if (!TUndoManagerImp::theManager.m_ptr)
    TUndoManagerImp::theManager.m_ptr = new TUndoManager;
  return TUndoManagerImp::theManager.m_ptr;
}

//=============================================================================

TUndoManager::TUndoManager() : m_imp(new TUndoManagerImp) {}

//-----------------------------------------------------------------------------

TUndoManager::~TUndoManager() {
  // cout << "Distrutto undo manager" << endl;
  assert(m_imp->m_blockStack.empty());
  reset();
}

//-----------------------------------------------------------------------------

void TUndoManager::TUndoManagerImp::add(TUndo *undo) {
  assert(undo);

  if (!m_blockStack.empty()) {
    assert(m_current == m_undoList.end());
    m_blockStack.back()->add(undo);
  } else
    doAdd(undo);
}

//-----------------------------------------------------------------------------

void TUndoManager::TUndoManagerImp::doAdd(TUndo *undo) {
  if (m_current != m_undoList.end()) {
    std::for_each(m_current, m_undoList.end(), deleteUndo);
    m_undoList.erase(m_current, m_undoList.end());
  }

  int i, memorySize = 0, count = m_undoList.size();
  for (i = 0; i < count; i++) memorySize += m_undoList[i]->getSize();

  while (count > 100 || (count != 0 && memorySize + undo->getSize() >
                                           m_undoMemorySize))  // 20MB
  {
    --count;
    TUndo *undo = m_undoList.front();
    m_undoList.pop_front();
    memorySize -= undo->getSize();
    delete undo;
  }

  undo->m_isLastInBlock     = true;
  undo->m_isLastInRedoBlock = true;
  m_undoList.push_back(undo);
  m_current = m_undoList.end();
}

//-----------------------------------------------------------------------------

void TUndoManager::beginBlock() {
  if (m_imp->m_current != m_imp->m_undoList.end()) {
    std::for_each(m_imp->m_current, m_imp->m_undoList.end(), deleteUndo);
    m_imp->m_undoList.erase(m_imp->m_current, m_imp->m_undoList.end());
  }

  TUndoBlock *undoBlock = new TUndoBlock;
  m_imp->m_blockStack.push_back(undoBlock);
  m_imp->m_current = m_imp->m_undoList.end();
}

//-----------------------------------------------------------------------------

void TUndoManager::endBlock() {
  // vogliamo fare anche resize del vector ???
  assert(m_imp->m_blockStack.empty() == false);
  TUndoBlock *undoBlock = m_imp->m_blockStack.back();
  m_imp->m_blockStack.pop_back();
  if (undoBlock->getUndoCount() > 0) {
    undoBlock->setLast();
    m_imp->add(undoBlock);
    emit historyChanged();
  } else {
    delete undoBlock;
    m_imp->m_current = m_imp->m_undoList.end();
  }
}

//-----------------------------------------------------------------------------

bool TUndoManager::undo() {
  assert(m_imp->m_blockStack.empty());
  UndoListIterator &it = m_imp->m_current;
  if (it != m_imp->m_undoList.begin()) {
    m_imp->m_skipped = false;
    --it;
    (*it)->undo();
    emit historyChanged();
    if (m_imp->m_skipped) {
      m_imp->m_skipped = false;
      return undo();
    }
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

bool TUndoManager::redo() {
  assert(m_imp->m_blockStack.empty());
  UndoListIterator &it = m_imp->m_current;
  if (it != m_imp->m_undoList.end()) {
    m_imp->m_skipped = false;
    (*it)->redo();
    ++it;
    emit historyChanged();
    if (m_imp->m_skipped) {
      m_imp->m_skipped = false;
      return redo();
    }
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------
// repeat e' come redo ma non sposta il puntatore al corrente
/*
void TUndoManager::repeat()
{
  assert(m_imp->m_blockStack.empty());
  UndoListIterator &it = m_imp->m_current;
  if (it != m_imp->m_undoList.end())
  {
    (*it)->repeat();
  }
}
*/

//-----------------------------------------------------------------------------

void TUndoManager::skip() { m_imp->m_skipped = true; }

//-----------------------------------------------------------------------------

void TUndoManager::setUndoMemorySize(int memorySyze) {
  m_imp->m_undoMemorySize = 1048576 * memorySyze;
}

//-----------------------------------------------------------------------------

void TUndoManager::add(TUndo *undo) {
  assert(undo);
  if (!undo) return;

  m_imp->add(undo);
  Q_EMIT historyChanged();

  undo->onAdd();
  Q_EMIT somethingChanged();
}

//-----------------------------------------------------------------------------

void TUndoManager::reset() {
  assert(m_imp->m_blockStack.empty());
  m_imp->m_blockStack.clear();
  UndoList &lst = m_imp->m_undoList;
  std::for_each(lst.begin(), lst.end(), deleteUndo);
  lst.clear();
  m_imp->m_current = m_imp->m_undoList.end();
  Q_EMIT historyChanged();
}

//-----------------------------------------------------------------------------

void TUndoManager::popUndo(int n, bool forward) {
  if (!forward) {
    if (m_imp->m_blockStack.empty()) {
      if (n == -1) {
        UndoListIterator start = m_imp->m_undoList.begin();
        n                      = 0;
        while (start != m_imp->m_current) ++n;
      }
      if (m_imp->m_undoList.empty() || n <= 0) return;
      if (m_imp->m_current == m_imp->m_undoList.end()) {
        int i;
        for (i = 0; i < n && m_imp->m_current != m_imp->m_undoList.begin();
             i++) {
          m_imp->m_current--;
          delete (*m_imp->m_current);
          m_imp->m_undoList.erase(m_imp->m_current);
          m_imp->m_current = m_imp->m_undoList.end();
        }
      } else {
        TUndo *undo = *m_imp->m_current;
        UndoListIterator start, end = m_imp->m_current;
        if (end == m_imp->m_undoList.begin()) return;

        int i;
        for (i = 0; i < n && m_imp->m_current != m_imp->m_undoList.begin(); i++)
          m_imp->m_current--;

        start                 = m_imp->m_current;
        UndoListIterator _end = end;
        while (_end != start) {
          _end--;
          delete (*_end);
        }
        m_imp->m_undoList.erase(start, end);

        m_imp->m_current = m_imp->m_undoList.begin();
        while (*m_imp->m_current != undo) m_imp->m_current++;
      }
    } else
      m_imp->m_blockStack.back()->popUndo(n);
    return;
  }

  if (m_imp->m_current == m_imp->m_undoList.end()) return;
  if (m_imp->m_blockStack.empty()) {
    UndoListIterator end, start = m_imp->m_current++;
    if (n == -1)
      end = m_imp->m_undoList.end();
    else {
      UndoListIterator it = start;
      end                 = it;
      int i               = 0;
      while (i != n && end != m_imp->m_undoList.end()) {
        ++end;
        i++;
      }
    }
    std::for_each(start, end, deleteUndo);
    m_imp->m_undoList.erase(start, end);
    m_imp->m_current = m_imp->m_undoList.end();
  } else
    m_imp->m_blockStack.back()->popUndo(n);
}

//-----------------------------------------------------------------------------

int TUndoManager::getHistoryCount() { return (int)m_imp->m_undoList.size(); }

//-----------------------------------------------------------------------------

int TUndoManager::getCurrentHistoryIndex() {
  int index           = 0;
  UndoListIterator it = m_imp->m_undoList.begin();
  while (1) {
    if (it == m_imp->m_current) return index;

    if (it == m_imp->m_undoList.end()) break;

    index++;
    it++;
  }
  return 0;
}

//-----------------------------------------------------------------------------

TUndo *TUndoManager::getUndoItem(int index) {
  if (index > (int)m_imp->m_undoList.size() || index <= 0) return 0;

  return m_imp->m_undoList.at(index - 1);
}
