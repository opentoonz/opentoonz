#pragma once

#ifndef TTHREADP_H
#define TTHREADP_H

#include <QObject>

#include "tthreadmessage.h"

//=====================================================================

//======================================
//    TThreadMessageDispatcher class
//--------------------------------------

// NOTE: This class should eventually be moved to tthreadmessagep.h...
class TThreadMessageDispatcher final : public QObject  // singleton
{
  Q_OBJECT

public:
  TThreadMessageDispatcher();
Q_SIGNALS:
  void signaled(TThread::Message *msg);
  void blockingSignaled(TThread::Message *msg);
protected Q_SLOTS:
  void onSignal(TThread::Message *msg);

public:
  void emitSignaled(TThread::Message *msg);
  void emitBlockingSignaled(TThread::Message *msg);
  static void init();
  static TThreadMessageDispatcher *instance();
};

//=====================================================================

namespace TThread {
// Forward declarations
class ExecutorId;
class ExecutorImpSlots;
}

//=====================================================================

//==============================
//    ExecutorImpSlots class
//------------------------------

class TThread::ExecutorImpSlots final : public QObject {
  Q_OBJECT

public:
  ExecutorImpSlots();
  ~ExecutorImpSlots();

  // The following is provided to ensure that point #3 in Qt reference in page
  // "Thread support in Qt"
  // is satisfied:

  //      "You must ensure that all objects created in
  //        a thread are deleted before you delete the QThread."

  // So, specifically, thread creation should happen only in the main thread,
  // not in worker threads.
  void emitRefreshAssignments();

Q_SIGNALS:
  void refreshAssignments();

public Q_SLOTS:
  void onTerminated();
  void onRefreshAssignments();
};

#endif
