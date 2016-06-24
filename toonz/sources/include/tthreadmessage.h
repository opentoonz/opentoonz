#pragma once

#ifndef TTHREADMESSAGE_H
#define TTHREADMESSAGE_H

//! HOW TO USE: subclass TThread::Message (MyMessage, in example) defining what
//! to execute in onDeliver method and than, to send a message to be executed in
//! the main thread:
//  MyMessage().send();

#ifndef TNZCORE_LIGHT
#include <QMutex>
#else
#include <windows.h>
#endif

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "tcommon.h"

namespace TThread {

bool DVAPI isMainThread();

// This class is used for communication between different threads. Calling the
// 'send' method in a thread, the user defined method 'onDeliver' is executed in
// the mainThread.
// using 'sendblocking', the calling thread will block until the main thread has
// executed the onDeliver function.
// WARNING!! the sendblocking method will cause a deadlock if used in a
// executable without main loop! (such as composer, cleanupper, etc.)
// NOTE: if the 'send' is called in the main thread, ther onDeliver is executed
// immediately.

class DVAPI Message {
public:
  Message();
  virtual ~Message(){};
  virtual Message *clone() const = 0;
  virtual void onDeliver()       = 0;

  void send();
  void sendBlocking();
};

#ifdef TNZCORE_LIGHT

class DVAPI Mutex {
public:
  HANDLE m_mutex;
  Mutex() { m_mutex = CreateMutex(NULL, FALSE, NULL); }
  ~Mutex() { CloseHandle(m_mutex); }
  void lock() {
    WaitForSingleObject(m_mutex,  // handle to mutex
                        INFINITE);
  }
  void unlock() { ReleaseMutex(m_mutex); }

private:
  // not implemented
  Mutex(const Mutex &);
  Mutex &operator=(const Mutex &);
};

class DVAPI MutexLocker {
  HANDLE m_mutex;

public:
  MutexLocker(Mutex *mutex) : m_mutex(mutex->m_mutex) {
    WaitForSingleObject(m_mutex, INFINITE);
  }
  ~MutexLocker() { ReleaseMutex(m_mutex); }
};

#else

class DVAPI Mutex final : public QMutex {
public:
  Mutex() : QMutex(QMutex::Recursive) {}

private:
  // not implemented
  Mutex(const Mutex &);
  Mutex &operator=(const Mutex &);
};

typedef QMutexLocker MutexLocker;

#endif

}  // namespace  TThread

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#endif
