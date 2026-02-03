

#include "tthread.h"
#include "tthreadP.h"

#include "boost/thread/thread.hpp"
#include "boost/thread/condition.hpp"
#include "boost/thread/tss.hpp"
#include "boost/thread/xtime.hpp"
#include <queue>

#ifdef WIN32
#include <windows.h>
#define WM_THREAD_NOTIFICATION (WM_USER + 10)
#else

#endif

DEFINE_CLASS_CODE(TThread::Runnable, 21)

//==============================================================================

namespace {
#ifdef WIN32
HWND MainHandle;
#else
Display *TheDisplay;
Window TheMainWindow;
#endif

//------------------------------------------------------------------------------

class Thread {
public:
  Thread();
  Thread(const TThread::RunnableP &runnable);

  ~Thread();

  void join();
  void cancel();

#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(disable : 4290)
#endif

  static void milestone() throw(TThread::Interrupt);

#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(default : 4290)
#endif

  class Imp;

private:
  friend class ThreadGroup;
  Imp *m_imp;
};

//------------------------------------------------------------------------------

class ThreadGroup {
public:
  ThreadGroup();
  ~ThreadGroup();

  void add(Thread *thread);
  void joinAll();

private:
  class Imp;
  Imp *m_imp;
};

//------------------------------------------------------------------------------

template <class T>
class QueueT {
public:
  QueueT(int slotCount)
      : m_items()
      , m_slotCount(slotCount)
      , m_notEmpty()
      , m_notFull()
      , m_mutex() {}

  ~QueueT() {}

  void put(const T &item) {
    TThread::ScopedLock sl(m_mutex);
    while (m_items.size() == m_slotCount) m_notFull.wait(sl);
    m_items.push(item);
    m_notEmpty.notify_one();
  }

  T get() {
    TThread::ScopedLock sl(m_mutex);

    while (m_items.size() == 0) m_notEmpty.wait(sl);

    m_notFull.notify_one();
    T item = m_items.front();
    m_items.pop();
    return item;
  }

  int size() {
    TThread::ScopedLock sl(m_mutex);
    int size = m_items.size();
    return size;
  }

private:
  std::queue<T> m_items;
  TThread::Condition m_notEmpty;
  TThread::Condition m_notFull;
  TThread::Mutex m_mutex;
  int m_slotCount;
};

}  // anonymous namespace

//------------------------------------------------------------------------------

void TThread::setMainThreadId(TThread::ThreadInfo *info) {
  assert(info);
#ifdef WIN32
  MainHandle = info->mainHandle;
#else
  TheDisplay    = info->dpy;
  TheMainWindow = info->win;
#endif
}

//------------------------------------------------------------------------------

#ifdef WIN32
ULONG TThread::getMainShellHandle() { return ULONG(MainHandle); }
#endif

//==============================================================================

class BoostRunnable {
public:
  BoostRunnable(const TThread::RunnableP &runnable, Thread::Imp *threadImp)
      : m_runnable(runnable), m_threadImp(threadImp) {}

  void operator()();

  TThread::RunnableP m_runnable;
  Thread::Imp *m_threadImp;
};

//==============================================================================

class Thread::Imp {
public:
  Imp() : m_boostThread(0), m_isCanceled(false), m_stateMutex() {}

  Imp(const TThread::RunnableP &runnable)
      : m_isCanceled(false), m_stateMutex() {
    m_boostThread = new boost::thread(BoostRunnable(runnable, this));
  }

  ~Imp() {
    if (m_boostThread) delete m_boostThread;
  }

  boost::thread *m_boostThread;
  boost::mutex m_stateMutex;
  bool m_isCanceled;
  long m_kkkk;

  enum State { Running, Canceled };

  static boost::mutex m_mutex;
  static std::map<long, State> m_state;

  class Key {
  public:
    Key(long id) : m_id(id) {}
    long m_id;
  };

  static boost::thread_specific_ptr<Key> m_key;
};

boost::mutex Thread::Imp::m_mutex;
std::map<long, Thread::Imp::State> Thread::Imp::m_state;

boost::thread_specific_ptr<Thread::Imp::Key> Thread::Imp::m_key;

void BoostRunnable::operator()() {
  Thread::Imp::m_key.reset(new Thread::Imp::Key(reinterpret_cast<long>(this)));
  m_threadImp->m_kkkk = reinterpret_cast<long>(this);

  {
    boost::mutex::scoped_lock sl(Thread::Imp::m_mutex);
    Thread::Imp::m_state[m_threadImp->m_kkkk] = Thread::Imp::Running;
  }

  assert(m_runnable);
  m_runnable->run();

  {
    boost::mutex::scoped_lock sl(Thread::Imp::m_mutex);
    Thread::Imp::m_state.erase(reinterpret_cast<long>(this));
  }
}

//==============================================================================

Thread::Thread() : m_imp(new Thread::Imp) {}

//------------------------------------------------------------------------------

Thread::Thread(const TThread::RunnableP &runnable) : m_imp(new Imp(runnable)) {}

//------------------------------------------------------------------------------

Thread::~Thread() { delete m_imp; }

//------------------------------------------------------------------------------

void Thread::join() {
  assert(m_imp->m_boostThread);
  m_imp->m_boostThread->join();
}

//------------------------------------------------------------------------------

void Thread::cancel() {
  boost::mutex::scoped_lock sl(Thread::Imp::m_mutex);

  std::map<long, Thread::Imp::State>::iterator it =
      Thread::Imp::m_state.find(m_imp->m_kkkk);
  if (it != Thread::Imp::m_state.end())
    Thread::Imp::m_state[m_imp->m_kkkk] = Thread::Imp::Canceled;
}

//------------------------------------------------------------------------------
// static member function
#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(disable : 4290)
#endif

void Thread::milestone() throw(TThread::Interrupt) {
  boost::mutex::scoped_lock sl(Thread::Imp::m_mutex);
  Thread::Imp::Key key = *Thread::Imp::m_key.get();
  Thread::Imp::m_state.find(key.m_id);

  if (Thread::Imp::m_state[key.m_id]) throw TThread::Interrupt();
}

#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(default : 4290)
#endif

//==============================================================================

class ThreadGroup::Imp {
public:
  Imp() : m_boostThreadGroup() {}
  ~Imp() {}

  boost::thread_group m_boostThreadGroup;
};

ThreadGroup::ThreadGroup() : m_imp(new Imp) {}

ThreadGroup::~ThreadGroup() { delete m_imp; }

void ThreadGroup::add(Thread *thread) {
  m_imp->m_boostThreadGroup.add_thread(thread->m_imp->m_boostThread);
}

void ThreadGroup::joinAll() { m_imp->m_boostThreadGroup.join_all(); }

//==============================================================================

#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(disable : 4290)
#endif

void TThread::milestone() throw(TThread::Interrupt) { Thread::milestone(); }

#if defined(_MSC_VER) && (_MSC_VER == 1200)
#pragma warning(default : 4290)
#endif

//==============================================================================

class TThread::Mutex::Imp {
public:
  boost::mutex m_mutex;

  Imp() : m_mutex() {}
  ~Imp() {}
};

TThread::Mutex::Mutex() : m_imp(new Imp) {}

TThread::Mutex::~Mutex() { delete m_imp; }

//==============================================================================

class TThread::ScopedLock::Imp {
public:
  boost::mutex::scoped_lock *m_sl;

  Imp(boost::mutex &mutex) : m_sl(new boost::mutex::scoped_lock(mutex)) {}

  ~Imp() {
    m_sl->unlock();
    delete m_sl;
  }
};

TThread::ScopedLock::ScopedLock(Mutex &mutex)
    : m_imp(new Imp(mutex.m_imp->m_mutex)) {}

TThread::ScopedLock::~ScopedLock() { delete m_imp; }

//==============================================================================

class TThread::Condition::Imp {
public:
  boost::condition m_condition;

  Imp() : m_condition() {}
  ~Imp() {}
};

TThread::Condition::Condition() : m_imp(new Imp()) {}

TThread::Condition::~Condition() { delete m_imp; }

void TThread::Condition::wait(ScopedLock &lock) {
  m_imp->m_condition.wait(*(lock.m_imp->m_sl));
}

bool TThread::Condition::wait(ScopedLock &lock, long timeout) {
  boost::xtime xt;
  boost::xtime_get(&xt, boost::TIME_UTC);
  xt.nsec += timeout * 1000;
  xt.sec += timeout / 1000;
  return m_imp->m_condition.timed_wait(*(lock.m_imp->m_sl), xt);
}

void TThread::Condition::notifyOne() { m_imp->m_condition.notify_one(); }

void TThread::Condition::notifyAll() { m_imp->m_condition.notify_all(); }

//==============================================================================

TThread::Msg::Msg() {}

//------------------------------------------------------------------------------

void TThread::Msg::send() {
  Msg *msg = clone();

#ifdef WIN32
  /*
Non viene utilizzato PostThreadMessage perche' se l'applicazione
si trova in un modal loop (esempio MessageBox) oppure si sta
facendo move o resize di una finestra i messaggi non giungono al
message loop.
http://support.microsoft.com/default.aspx?scid=KB;EN-US;q183116&
*/

  /*
BOOL rc = PostThreadMessage(
getMainThreadId(),      // thread identifier
WM_THREAD_NOTIFICATION, // message
WPARAM(msg),            // first message parameter
0);                     // second message parameter
*/
  PostMessage(HWND(getMainShellHandle()), WM_THREAD_NOTIFICATION, WPARAM(msg),
              0);
#else
  XClientMessageEvent clientMsg;
  clientMsg.type         = ClientMessage;
  clientMsg.window       = TheMainWindow;
  clientMsg.format       = 32;
  clientMsg.message_type = Msg::MsgId();
  clientMsg.data.l[0]    = (long)msg;
  // Status status =
  XSendEvent(TheDisplay, TheMainWindow, 0, NoEventMask, (XEvent *)&clientMsg);
  XFlush(TheDisplay);
#endif
}

//------------------------------------------------------------------------------

// statica
UINT TThread::Msg::MsgId() {
#ifdef WIN32
  return WM_THREAD_NOTIFICATION;
#else
  static Atom atom = 0;
  if (!atom) {
    atom = XInternAtom(TheDisplay, "ThreadMessage", false);
    assert(atom);
  }
  return atom;
#endif
}

//==============================================================================

class TThread::Executor::Imp : public TSmartObject {
public:
  typedef TSmartPointerT<TThread::Executor::Imp> ImpP;

  //---------------------------------------------------

  class Worker : public Runnable {
  public:
    Worker(ImpP owner) : Runnable(), m_owner(owner) {}
    ~Worker() {}

    void run();
    void doCleanup();

    ImpP m_owner;
  };

  //---------------------------------------------------

  bool m_suspend;
  bool m_threadHasToDie;

  UINT m_threadCount;
  Mutex m_mutex;

  Condition m_cond;
  Condition m_taskQueueEmpty;

  //------
  Condition m_taskQueueNotEmpty;
  //------

  std::queue<TThread::RunnableP> m_tasks;
  std::map<long, Thread *> m_workerThreads;

  Imp(int threadsCount, bool suspend)
      : TSmartObject()
      , m_suspend(suspend)
      , m_threadHasToDie(false)
      , m_threadCount(threadsCount)
      , m_tasks()
      , m_workerThreads()
      , m_mutex()
      , m_cond(){};

  ~Imp() {}
};

//------------------------------------------------------------------------------

TThread::Executor::Executor(int threadsCount, bool suspend)
    : m_imp(new Imp(threadsCount, suspend)) {
  m_imp->addRef();
}

//------------------------------------------------------------------------------

TThread::Executor::~Executor() {
  {
    TThread::ScopedLock sl(m_imp->m_mutex);

    if (m_imp->m_suspend) {
      m_imp->m_threadHasToDie = true;
      m_imp->m_taskQueueNotEmpty.notifyAll();
    }
  }

  m_imp->release();
}

//------------------------------------------------------------------------------

void TThread::Executor::Imp::Worker::run() {
  try {
    while (true) {
      // check if thread has been canceled
      Thread::milestone();

      // get the next task
      RunnableP task = 0;

      {
        ScopedLock sl(m_owner->m_mutex);
        if (m_owner->m_tasks.empty()) {
          // la lista di task e' stata esaurita
          if (m_owner->m_suspend) {
            // il thread deve sospendersi
            m_owner->m_taskQueueNotEmpty.wait(sl);

            // a questo punto il thread e' stato risvegliato
            if (m_owner->m_threadHasToDie) {
              doCleanup();
              return;
            }
          } else {
            // il thread sta per morire -> bisogna eliminarlo dalla lista dei
            // worker thread
            doCleanup();
            return;
          }
        }

        if (!m_owner->m_tasks.empty()) {
          task = m_owner->m_tasks.front();
          m_owner->m_tasks.pop();
        }
      }

      if (task) task->run();

      // check if thread has been canceled
      Thread::milestone();
    }
  } catch (TThread::Interrupt &) {
    // m_owner->m_cond.notifyOne();
  } catch (...) {
    // eccezione non prevista --> bisogna eliminare il thread
    // dalla lista dei worker thread

    ScopedLock sl(m_owner->m_mutex);
    doCleanup();
  }
}

//------------------------------------------------------------------------------

void TThread::Executor::Imp::Worker::doCleanup() {
  std::map<long, Thread *>::iterator it =
      m_owner->m_workerThreads.find(reinterpret_cast<long>(this));

  if (it != m_owner->m_workerThreads.end()) {
    Thread *thread = it->second;
    delete thread;
    m_owner->m_workerThreads.erase(it);
  }

  if (m_owner->m_workerThreads.size() == 0)
    m_owner->m_taskQueueEmpty.notifyAll();
}

//------------------------------------------------------------------------------

void TThread::Executor::addTask(const RunnableP &task) {
  TThread::ScopedLock sl(m_imp->m_mutex);
  m_imp->m_tasks.push(task);
  if (m_imp->m_workerThreads.size() < m_imp->m_threadCount) {
    TThread::Executor::Imp::Worker *worker =
        new TThread::Executor::Imp::Worker(m_imp);

    m_imp->m_workerThreads[reinterpret_cast<long>(worker)] = new Thread(worker);
  } else {
    if (m_imp->m_suspend)
      // risveglia uno dei thread in attesa
      m_imp->m_taskQueueNotEmpty.notifyOne();
  }
}

//------------------------------------------------------------------------------

void TThread::Executor::clear() {
  ScopedLock sl(m_imp->m_mutex);

  while (!m_imp->m_tasks.empty()) m_imp->m_tasks.pop();
}

//------------------------------------------------------------------------------

void TThread::Executor::cancel() {
  {
    ScopedLock sl(m_imp->m_mutex);

    while (!m_imp->m_tasks.empty()) m_imp->m_tasks.pop();
  }

  while (true) {
    Thread *thread = 0;
    {
      ScopedLock sl(m_imp->m_mutex);
      if (m_imp->m_workerThreads.empty())
        break;
      else {
        std::map<long, Thread *>::iterator it = m_imp->m_workerThreads.begin();
        thread = it->second;
        m_imp->m_workerThreads.erase(it);

        if (thread) thread->cancel();
      }
    }
  }
}

//------------------------------------------------------------------------------

void TThread::Executor::wait() {
  TThread::ScopedLock sl(m_imp->m_mutex);

  while (m_imp->m_workerThreads.size()) m_imp->m_taskQueueEmpty.wait(sl);
}

//------------------------------------------------------------------------------

bool TThread::Executor::wait(long timeout) {
  TThread::ScopedLock sl(m_imp->m_mutex);

  bool expired = false;
  while (m_imp->m_workerThreads.size())
    expired = m_imp->m_taskQueueEmpty.wait(sl, timeout);

  return expired;
}

//------------------------------------------------------------------------------

int TThread::Executor::getThreadCount() {
  TThread::ScopedLock sl(m_imp->m_mutex);
  return m_imp->m_workerThreads.size();
}

//------------------------------------------------------------------------------

int TThread::Executor::getTaskCount() {
  TThread::ScopedLock sl(m_imp->m_mutex);
  return m_imp->m_tasks.size();
}
