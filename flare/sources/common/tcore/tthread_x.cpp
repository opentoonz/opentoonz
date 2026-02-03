

#include "tthread.h"
#include <pthread.h>

//---------------------------------------------------------------------------
//    TMutex & TMutexImp
//---------------------------------------------------------------------------

class TMutexImp {
  pthread_mutex_t id;

public:
  TMutexImp();
  ~TMutexImp();
  void lock();
  void unlock();
};

//---------------------------------------------------------------------------
TMutex lockForTheList;

class TThreadGroupImp {
  list<TThread *> threads;

public:
  TThreadGroupImp();
  ~TThreadGroupImp();
  void add(TThread *);
  void remove(TThread *);
  void wait();
};

//---------------------------------------------------------------------------

TMutexImp::TMutexImp() { pthread_mutex_init(&id, 0); }

//---------------------------------------------------------------------------

TMutexImp::~TMutexImp() { pthread_mutex_destroy(&id); }

//---------------------------------------------------------------------------

void TMutexImp::lock() { pthread_mutex_lock(&id); }

//---------------------------------------------------------------------------

void TMutexImp::unlock() { pthread_mutex_unlock(&id); }

//---------------------------------------------------------------------------

TMutex::TMutex() : m_imp(new TMutexImp) {}

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
  pthread_t threadId;

public:
  TThreadImp();
  ~TThreadImp();

  TThread *thread;

  void start();

  bool setThreadPriority(TThread::TThreadPriority p);
  bool setPreferredProcessor(int processorId);

  TMutex secureLock;
  bool isRunning;

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

  // some static stuff
  static TUINT32 nThreads;
  static TMutex mutex;

  friend class TThreadGroupImp;
  void setOwner(TThreadGroupImp *_owner) { owner = _owner; }
  TThreadGroupImp *owner;
};

TUINT32 TThreadImp::nThreads = 0;
TMutex TThreadImp::mutex     = TMutex();
//---------------------------------------------------------------------------

TThreadImp::TThreadImp() : isRunning(false), owner(0), thread(0) {}

//---------------------------------------------------------------------------

TThreadImp::~TThreadImp() {
  // CloseHandle(threadId);
}
//---------------------------------------------------------------------------

static void * /*__stdcall*/ fun(void *data) {
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

  return 0;
}

//---------------------------------------------------------------------------

void TThreadImp::start() {
  incNThreads();
  pthread_create(&threadId, 0, fun, (void *)this);
}

//---------------------------------------------------------------------------

bool TThreadImp::setThreadPriority(TThread::TThreadPriority) {
  assert(!"not implemented");
  return false;
}

//---------------------------------------------------------------------------

bool TThreadImp::setPreferredProcessor(int processorId) {
#ifdef __sgi
#if (OP_RELEASE == rel_2)
  assert(!"Not implemented");
  return false;
#else
  int rc = pthread_setrunon_np(processorId);
  return (rc != -1);
#endif
#else
  assert(0);
  return false;
#endif
}

//---------------------------------------------------------------------------

TThread::TThread() : m_imp(new TThreadImp()) { m_imp->thread = this; }

//---------------------------------------------------------------------------

TThread::~TThread() { delete m_imp; }

//---------------------------------------------------------------------------

void TThread::start() { m_imp->start(); }

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

TThreadGroupImp::TThreadGroupImp() {}

//---------------------------------------------------------------------------

TThreadGroupImp::~TThreadGroupImp() {}

//---------------------------------------------------------------------------

void TThreadGroupImp::add(TThread *t) {
  lockForTheList.lock();
  threads.push_back(t);
  lockForTheList.unlock();
  t->m_imp->setOwner(this);
}
//---------------------------------------------------------------------------

void TThreadGroupImp::remove(TThread *t) {
  lockForTheList.lock();
  threads.remove(t);
  lockForTheList.unlock();
  t->m_imp->setOwner(0);
}
//---------------------------------------------------------------------------

static void * /*__stdcall*/ mainFun(void *data) {
  // cout << "mainfun" << endl;
  list<TThread *> *threads = (list<TThread *> *)data;
  // lockForTheList.lock();
  ULONG s = threads->size();
  // lockForTheList.unlock();
  // cout <<"ci sono " << s << "thread in ballo..." << endl;

  while (s != 0) {
    lockForTheList.lock();
    s = threads->size();
    lockForTheList.unlock();
  }
  return 0;
}

//---------------------------------------------------------------------------
void TThreadGroupImp::wait() {
  // cout << "wait()" << endl;
  lockForTheList.lock();
  ULONG count = threads.size();

  for (list<TThread *>::iterator it = threads.begin(); it != threads.end();
       it++) {
    TThread *t = *it;
    t->start();
  }
  lockForTheList.unlock();

  if (count == 0) return;
  void *mainRet = 0;
  pthread_t mainThread;
  // cout << "creo il main" << endl;
  pthread_create(&mainThread, 0, mainFun, &threads);
  // cout << mainThread << endl;
  pthread_join(mainThread, &mainRet);
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
