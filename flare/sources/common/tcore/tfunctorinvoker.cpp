

// Qt includes
#include <QMetaObject>

#include "tfunctorinvoker.h"

//********************************************************************************
//    TFunctorInvoker  definition
//********************************************************************************

TFunctorInvoker *TFunctorInvoker::instance() {
  static TFunctorInvoker theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------

void TFunctorInvoker::invokeQueued(BaseFunctor *functor) {
  QMetaObject::invokeMethod(this, "invoke", Qt::QueuedConnection,
                            Q_ARG(void *, functor));
}
