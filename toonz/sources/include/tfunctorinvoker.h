#pragma once

#ifndef TFUNCTORINVOKER_H
#define TFUNCTORINVOKER_H

// TnzCore includes
#include "tcommon.h"

// Qt includes
#include <QObject>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//********************************************************************************
//    TFunctorInvoker  declaration
//********************************************************************************

/*!
  \brief    The TFunctorInvoker is a singleton class that can be used to invoke
functions
            as Qt slots without having to inherit from QObject.

  \details  \par Rationale
              Signal and slots are an important part of the Qt framework -
however, using them
              requires to explicitly inherit from QObject. This singleton object
is provided
              to avoid the memory overhead of inheriting from QObject in the
case of simple
              function calls - notably functions that need to synchronize with
Qt's main event
              loop.

            \par Usage
              Inherit from TFunctorInvoker::BaseFunctor and allocate a new
instance on the heap,
              to be supplied to the TFunctorInvoker::invoke() slot.
\n
              The following code exemplifies a correct usage for this class:
\n
              \code
              class MyFunctor final : public TFunctorInvoker::BaseFunctor
              {
                MyParams m_params;                                // Function
parameters

              public:

                MyFunctor(const MyParams& params) : m_params(params) {}

                void operator()()
                {
                  // The function body, operating on m_params
                }
              };

              //------------------------------------------------------------------

              void invokeMyFunctor_queued(const MyParams& params)
              {
                QMetaObject::invokeMethod(TFunctorInvoker::instance(), "invoke",
Qt::QueuedConnection,
                  Q_ARG(void*, new MyFunctor(params))
                );
              }
              \endcode
*/

class DVAPI TFunctorInvoker final : public QObject {
  Q_OBJECT

public:
  /*!
\brief    Polymorphic base \a zerary functor for TFunctorInvoker-compatible
        functors.
*/

  class BaseFunctor {
  public:
    virtual ~BaseFunctor() {}

    virtual void operator()() = 0;
  };

public:
  static TFunctorInvoker *instance();

  void invokeQueued(BaseFunctor *functor);

public Q_SLOTS:

  void invoke(void *vPtr) {
    BaseFunctor *func = (BaseFunctor *)vPtr;

    (*func)();    // Invoke functor ...
    delete func;  // ... and then delete it
  }

private:
  TFunctorInvoker() {}
};

#endif  // TFUNCTORINVOKER_H
