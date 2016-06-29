#pragma once

#ifndef TRENDERERP_INCLUDE
#define TRENDERERP_INCLUDE

#include <QObject>
#include <QMetaType>

#include "trenderer.h"

//=============================================================================================

//! This is a private class used to convey the TRenderer::startRendering
//! methods into Qt queued slots. This is necessary since these methods
//! implicitly
//! perform event processing, which could cause trouble in case they are invoked
//! from
//! events which must respect a strict ordering.
//! \n \n
//! For example, suppose that a render must be invoked upon a mousePressEvent,
//! and that such event must have been completely processed before the
//! corrispondant
//! mouseReleaseEvent is invoked - calling the startRendering method *directly*
//! by
//! the mousePressEvent may cause the mouseReleaseEvent to be processed before
//! the
//! former's end.
class TRendererStartInvoker final : public QObject {
  Q_OBJECT

public:
  struct StartInvokerRenderData {
    unsigned long m_renderId;
    const RenderDataVector *m_renderDataVector;
  };

public:
  TRendererStartInvoker() {
    qRegisterMetaType<StartInvokerRenderData>("StartInvokerRenderData");

    connect(this, SIGNAL(startRender(TRendererImp *, StartInvokerRenderData)),
            this, SLOT(doStartRender(TRendererImp *, StartInvokerRenderData)),
            Qt::QueuedConnection);
  }
  ~TRendererStartInvoker() {}

  static TRendererStartInvoker *instance() {
    static TRendererStartInvoker theInstance;
    return &theInstance;
  }

  void emitStartRender(TRendererImp *renderer, StartInvokerRenderData rd);

Q_SIGNALS:

  void startRender(TRendererImp *, StartInvokerRenderData);

public Q_SLOTS:

  void doStartRender(TRendererImp *, StartInvokerRenderData rd);
};

#endif  // TRENDERERP_INCLUDE
