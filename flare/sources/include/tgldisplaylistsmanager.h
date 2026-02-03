#pragma once

#ifndef TGLDISPLAYLISTSMANAGER_H
#define TGLDISPLAYLISTSMANAGER_H

// TnzCore includes
#include "tgl.h"

// tcg includes
#include "tcg/tcg_observer_notifier.h"

// Qt includes
#include <QMutex>

#undef DVAPI
#undef DVVAR

#ifdef TGL_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**************************************************************************************************
//    TGLDisplayListsProxy  declaration
//**************************************************************************************************

//! TGLDisplayListsProxy is a wrapper to a dummy OpenGL context attached to a
//! specific display lists space.
/*!
  TGLDisplayListsProxy implements the basic functionalities necessary to address
a display lists
  space without having to access any actual associated OpenGL context. This is
equivalent to
  making a hidden OpenGL context (the display lists proxy) the \a current one.

\note Implementations of the TGLDisplayListsProxy must take ownership of the
proxy.
*/

class TGLDisplayListsProxy {
  QMutex m_mutex;

public:
  virtual ~TGLDisplayListsProxy() {}

  virtual void makeCurrent() = 0;
  virtual void doneCurrent() = 0;

  QMutex *mutex() { return &m_mutex; }
};

//**************************************************************************************************
//    TGLDisplayListsProxy  template specializations
//**************************************************************************************************

template <typename Context>
class TGLDisplayListsProxyT final : public TGLDisplayListsProxy {
  Context *m_proxy;

public:
  TGLDisplayListsProxyT(Context *proxy) : m_proxy(proxy) {}
  ~TGLDisplayListsProxyT() { delete m_proxy; }

  void makeCurrent() override { m_proxy->makeCurrent(); }
  void doneCurrent() override { m_proxy->doneCurrent(); }
};

//**************************************************************************************************
//    TGLDisplayListsManager  declaration
//**************************************************************************************************

//! TGLDisplayListsManager is a singleton class used to track OpenGL shared
//! display lists spaces.
/*!
  OpenGL contexts can share their display lists space (in particular, this
includes texture
  objects) with other contexts. Typically, sharing specification happens when a
new context
  is created - at that point, the new context is allowed to share the lists
space of another
  known context, pretty much the same way a shared smart pointer does.
\n\n
  However, OpenGL provides access to the display lists space \a only through
their attached
  OpenGL contexts; meaning that an external object has to know at least one
associated context
  to operate on a display lists space.
\n\n
  We'll call such an associated context  a \a proxy of the display lists space.
It is a dummy
  OpenGL context that shares the display lists spaces with those that are
attached to it.
\n\n
  Observe that the use of one such dummy context, rather than one of the
originally
  attached ones, is strictly necessary in a multithreaded environment, since <I>
an OpenGL
  context can be active in exactly one thread at a given time <\I> - and any of
the attached
  contexts could always be current in another thread.
\n\n
  However, the use of a single proxy per display lists space means that multiple
threads
  could try to access it at the same time. Synchronization in this case must be
handled by
  the user by accessing the proxy's built-in mutex.

  \warning TGLDisplayListsManager relies on the user to attach a context to the
\b correct
           display lists id.
*/

class DVAPI TGLDisplayListsManager final : public tcg::notifier<> {
public:
  struct Observer : public tcg::observer<TGLDisplayListsManager> {
    virtual void onDisplayListDestroyed(int dlSpaceId) = 0;
  };

public:
  static TGLDisplayListsManager *instance();

  int storeProxy(TGLDisplayListsProxy *proxy);  //!< Stores the specified proxy,
                                                //! returning its associated
  //! display
  //!< lists id. Context attaches should follow.
  void attachContext(int dlSpaceId, TGlContext context);  //!< Attaches the
                                                          //! specified context
  //! to a display lists
  //! space
  void releaseContext(TGlContext context);  //!< Releases a context reference to
                                            //! its display lists space
  int displayListsSpaceId(TGlContext context);  //!< Returns the display lists
                                                //! space id of a known context,
  //! or
  //!< -1 if it did not attach to any known space.
  TGLDisplayListsProxy *dlProxy(int dlSpaceId);  //!< Returns the display lists
                                                 //! space proxy associated to
  //! the
  //!< specified id.
};

#endif  // TGLDISPLAYLISTSMANAGER_H
