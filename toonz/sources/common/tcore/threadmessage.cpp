

#include "tthreadmessage.h"
#include "tthreadp.h"
#include <QThread>

QThread *MainThread = QThread::currentThread();

TThreadMessageDispatcher
    *Dispatcher;  // MUST BE CREATED  in the main thread!!!!!!

//------------------------------------------------------------------------------

bool TThread::isMainThread() { return MainThread == QThread::currentThread(); }

//------------------------------------------------------------------------------

TThreadMessageDispatcher::TThreadMessageDispatcher() {
  QObject::connect(this, &TThreadMessageDispatcher::signaled, this,
                   &TThreadMessageDispatcher::onSignal);
  QObject::connect(this, &TThreadMessageDispatcher::blockingSignaled, this,
                   &TThreadMessageDispatcher::onSignal,
                   Qt::BlockingQueuedConnection);
}

//------------------------------------------------------------------------------

void TThreadMessageDispatcher::init() {
  if (!TThread::isMainThread()) return;
  if (Dispatcher == 0) Dispatcher = new TThreadMessageDispatcher();
}

//------------------------------------------------------------------------------

TThreadMessageDispatcher *TThreadMessageDispatcher::instance() {
  assert(Dispatcher);
  return Dispatcher;
}

//------------------------------------------------------------------------------

void TThreadMessageDispatcher::emitSignaled(TThread::Message *msg) {
  Q_EMIT signaled(msg);
}

//------------------------------------------------------------------------------

void TThreadMessageDispatcher::emitBlockingSignaled(TThread::Message *msg) {
  Q_EMIT blockingSignaled(msg);
}

//------------------------------------------------------------------------------

void TThreadMessageDispatcher::onSignal(TThread::Message *msg) {
  msg->onDeliver();
  delete msg;
}

//------------------------------------------------------------------------------

TThread::Message::Message() {}

//------------------------------------------------------------------------------
void TThread::Message::send() {
  if (isMainThread())
    onDeliver();
  else
    TThreadMessageDispatcher::instance()->emitSignaled(clone());
}

//------------------------------------------------------------------------------

void TThread::Message::sendBlocking() {
  if (isMainThread())
    onDeliver();
  else
    TThreadMessageDispatcher::instance()->emitBlockingSignaled(clone());
}

//------------------------------------------------------------------------------
