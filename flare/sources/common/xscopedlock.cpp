

#include "xscopedlock.h"
#include "tthread.h"

using namespace TThread;

class XScopedLock::Imp {
  ScopedLock *m_scopedLock;
  static Mutex m_mutex;

public:
  Imp() : m_scopedLock(new ScopedLock(m_mutex)) {}

  ~Imp() { delete m_scopedLock; }
};

//---------------------------------------------------------------------------

Mutex XScopedLock::Imp::m_mutex;

//---------------------------------------------------------------------------

XScopedLock::XScopedLock() : m_imp(new Imp) {}

//---------------------------------------------------------------------------

XScopedLock::~XScopedLock() { delete m_imp; }

//---------------------------------------------------------------------------
