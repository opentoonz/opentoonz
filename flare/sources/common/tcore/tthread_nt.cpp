

#include "tthread.h"
#define _WIN32_WINNT 0x0400
#include <windows.h>

class TThreadGroupImp;
//---------------------------------------------------------------------------
//    TMutex & TMutexImp
//---------------------------------------------------------------------------

class TMutexImp {
  HANDLE id;

public:
  TMutexImp();
  ~TMutexImp();
  void lock();
  void unlock();
};

//---------------------------------------------------------------------------

class TThreadGroupImp {
  std::list<TThread *> threads;

public:
  TThreadGroupImp();
  ~TThreadGroupImp();
  void add(TThread *);
  void remove(TThread *);
  void wait();
};
//---------------------------------------------------------------------------

TMutexImp::TMutexImp() { id = CreateMutex(NULL, FALSE, NULL); }

//---------------------------------------------------------------------------

TMutexImp::~TMutexImp() { BOOL rc = CloseHandle(id); }
//---------------------------------------------------------------------------

void TMutexImp::lock() { DWORD rc = WaitForSingleObject(id, INFINITE); }

//---------------------------------------------------------------------------

void TMutexImp::unlock() { BOOL rc = ReleaseMutex(id); }

//---------------------------------------------------------------------------

TMutex::TMutex() : m_imp(new TMutexImp()) {}

//---------------------------------------------------------------------------

TMutex::~TMutex() { delete m_imp; }
//---------------------------------------------------------------------------

void TMutex::lock() { m_imp->lock(); }

//---------------------------------------------------------------------------

void TMutex::unlock() { m_imp->unlock(); }

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//    TThread & TThreadImp
//---------------------------------------------------------------------------

class TThreadImp {
public:
  friend class TThreadGroupImp;

  HANDLE threadId;
  HANDLE mainThread;
  int m_refCount;
  TMutex secureLock;
  bool isRunning;
  TThreadGroupImp *owner;
  TThread *thread;

  // some static stuff
  static TUINT32 nThreads;
  static TMutex mutex;

  TThreadImp();
  ~TThreadImp();

  void start();

  bool setThreadPriority(TThread::TThreadPriority p);
  bool setPreferredProcessor(int processorId);

  static void incNThreads() {
    mutex.lock();
    nThreads++;
    mutex.unlock();
  }
  static void decNThreads() {
    mutex.lock();
    nThreads--;
    mutex.unlock();
  }

  void setOwner(TThreadGroupImp *_owner) { owner = _owner; }
};

TUINT32 TThreadImp::nThreads = 0;
TMutex TThreadImp::mutex     = TMutex();

//---------------------------------------------------------------------------

TThreadImp::TThreadImp()
    : isRunning(false)
    , threadId(0)
    , owner(0)
    , mainThread(0)
    , thread(0)
    , secureLock()
    , m_refCount(0) {}

//---------------------------------------------------------------------------

TThreadImp::~TThreadImp() {
  if (threadId) CloseHandle(threadId);
}

//---------------------------------------------------------------------------

static TUINT32 __stdcall fun(void *data) {
  TThreadImp *t = (TThreadImp *)data;

  t->secureLock.lock();
  if (t->isRunning) {
    t->secureLock.unlock();
    assert(!"thread is already running");
    return 0;
  }
  t->isRunning = true;

  t->secureLock.unlock();

  t->thread->run();

  t->decNThreads();
  if (t->owner) t->owner->remove(t->thread);
  t->thread->release();
  return 0;
}

//---------------------------------------------------------------------------

void TThreadImp::start() {
  TThreadImp::incNThreads();
  threadId = CreateThread(0, 0, fun, this, 0, 0);
}
//---------------------------------------------------------------------------

bool TThreadImp::setThreadPriority(TThread::TThreadPriority p) {
  int priority;
  switch (p) {
  case TThread::TIME_CRITICAL:
    priority = THREAD_PRIORITY_TIME_CRITICAL;
    break;
  case TThread::HIGHEST:
    priority = THREAD_PRIORITY_HIGHEST;
    break;
  case TThread::ABOVE_NORMAL:
    priority = THREAD_PRIORITY_ABOVE_NORMAL;
    break;
  case TThread::NORMAL:
    priority = THREAD_PRIORITY_NORMAL;
    break;
  case TThread::BELOW_NORMAL:
    priority = THREAD_PRIORITY_BELOW_NORMAL;
    break;
  case TThread::LOWEST:
    priority = THREAD_PRIORITY_LOWEST;
    break;
  case TThread::IDLE:
    priority = THREAD_PRIORITY_IDLE;
    break;
  default:
    priority = THREAD_PRIORITY_NORMAL;
    break;
  }
  return !!SetThreadPriority(threadId, priority);
}

//---------------------------------------------------------------------------

bool TThreadImp::setPreferredProcessor(int processorId) {
  DWORD rc = SetThreadIdealProcessor(threadId, processorId);
  return (rc != -1);
}

//---------------------------------------------------------------------------

TThread::TThread() : m_imp(new TThreadImp()) { m_imp->thread = this; }

//---------------------------------------------------------------------------

TThread::~TThread() { delete m_imp; }

//---------------------------------------------------------------------------

void TThread::start() {
  addRef();
  m_imp->start();
}

//---------------------------------------------------------------------------

void TThread::addRef() {
  m_imp->mutex.lock();
  m_imp->m_refCount++;
  m_imp->mutex.unlock();
}

//---------------------------------------------------------------------------

void TThread::release() {
  bool kill = false;
  m_imp->mutex.lock();
  m_imp->m_refCount--;
  if (m_imp->m_refCount <= 0) kill = true;
  m_imp->mutex.unlock();
  if (kill) delete this;
}

//---------------------------------------------------------------------------

int TThread::getRefCount() { return m_imp->m_refCount; }

//---------------------------------------------------------------------------

bool TThread::setPreferredProcessor(int processorId) {
  return m_imp->setPreferredProcessor(processorId);
}

//---------------------------------------------------------------------------

bool TThread::setThreadPriority(TThread::TThreadPriority p) {
  return m_imp->setThreadPriority(p);
}

//=======================
//    TThreadGroupImp

//---------------------------------------------------------------------------

TThreadGroupImp::TThreadGroupImp() {}

//---------------------------------------------------------------------------

TThreadGroupImp::~TThreadGroupImp() {}

//---------------------------------------------------------------------------
void TThreadGroupImp::add(TThread *t) {
  threads.push_back(t);
  t->m_imp->setOwner(this);
}
//---------------------------------------------------------------------------

void TThreadGroupImp::remove(TThread *t) {
  threads.remove(t);
  t->m_imp->setOwner(0);
}
//---------------------------------------------------------------------------

void TThreadGroupImp::wait() {
  DWORD count = threads.size();
  if (count == 0) return;
  HANDLE *hThreads = new HANDLE[count];
  int id           = 0;
  for (std::list<TThread *>::iterator it = threads.begin(); it != threads.end();
       it++, id++) {
    TThread *t = *it;
    if (t->m_imp->threadId == 0) t->start();
    hThreads[id] = t->m_imp->threadId;
  }

  DWORD rc = WaitForMultipleObjects(count, hThreads, FALSE, INFINITE);
  if (rc >= WAIT_OBJECT_0 && rc <= (WAIT_OBJECT_0 + count - 1)) {
    // cout << "obj #" << rc << endl;
  }
  if (rc >= WAIT_ABANDONED_0 && rc <= (WAIT_ABANDONED_0 + count - 1)) {
    // cout << "obj #" << rc << " abandoned" << endl;
  }
  if (rc == WAIT_TIMEOUT) {
    // cout << "timeout" << endl;
  }

  if (rc == WAIT_FAILED) {
    // cout << "failed" << endl;
  }

  delete[] hThreads;
  wait();
}

//---------------------------------------------------------------------------

TThreadGroup::TThreadGroup()
    : m_imp(new TThreadGroupImp())

{}

//---------------------------------------------------------------------------

TThreadGroup::~TThreadGroup() { delete m_imp; }

//---------------------------------------------------------------------------

void TThreadGroup::add(TThread *t) { m_imp->add(t); }

//---------------------------------------------------------------------------

void TThreadGroup::wait() { m_imp->wait(); }
