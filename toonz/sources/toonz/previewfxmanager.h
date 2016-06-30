#pragma once

#ifndef PREVIEW_FX_MANAGER
#define PREVIEW_FX_MANAGER

#include <QObject>
#include <QMap>
#include "trenderer.h"
#include "tfx.h"

//=====================================================================================

//  Forward declarations

class FlipBook;
class PreviewFxInstance;
class TXsheet;

//=====================================================================================

//===============================
//    PreviewFxManager class
//-------------------------------

/*!
Manages the 'Preview Fx' requests and updates all contextually opened FlipBooks
when needed.
The class provides the medium between the 'Preview Fx' command and management of
both Toonz
rendering classes and Flipbooks.
\n \n
The showNewPreview() method invocation takes place when users activate the
'Preview' command from the
right-click menu of fxs in the schematic. It inizializes a TRenderer object and
starts the
render of passed fx over the preview properties range; at the same time, a new
Flipbook is opened to
visualize the images produced by the render process as frames are finalized.
\n \n
The refreshView() command is available to update an fx's view upon request (this
is typically handled
by flipbooks).
The manager always renders the area strictly necessary to correctly update all
flipbooks' current view;
in particular, previously rendered portions of flipbook views are retained, and
excluded from further
calculations.
\n
Observe that invalidation signals coming from TXshLevelHandle, TFxHandle and
TXsheetHandle classes are
automatically intercepted to update all FlipBooks created under the manager.
\n \n
Additional reset() methods are available to clear the manager about a single fx
or the whole scene's fxs.

\sa FlipBook, TXshLevelHandle, TFxHandle, TXsheetHandle classes.
*/
class PreviewFxManager final : public QObject {
  Q_OBJECT

  TThread::Mutex m_mutex;
  QMap<unsigned long, PreviewFxInstance *> m_previewInstances;

private:
  PreviewFxManager();
  ~PreviewFxManager();

  // Not implemented
  PreviewFxManager(const PreviewFxManager &);
  void operator=(const PreviewFxManager &);

public:
  //! Returns the instance of the PreviewFxManager object.
  static PreviewFxManager *instance();

  static void suspendRendering(bool suspend);

  //! Starts a preview instance for passed fx. If explicitly requested, a new
  //! flipbook is created to
  //! host the preview, otherwise the Preference settings are used. In the
  //! former case, the view refresh
  //! must be explicitly invoked using the refreshView() command, in order to
  //! allow changes of the flipbook's
  //! view affine before it.
  //! The associated flipbook is returned, in case it has been created.
  FlipBook *showNewPreview(TFxP fx, bool forceFlipbook = false);

  //! Updates the fx's preview on the associated flipbooks' current views.
  void refreshView(TFxP fx);

  //! Detaches the specified flipbook from the manager's control. Optionally,
  //! the flipbooks may keep the already
  //! previewed images instead of discarding them - however, observe that these
  //! images may be \b cloned from the
  //! preview originals, in case some flipbook is still actively previewing the
  //! same fx.
  void detach(FlipBook *flipbook);

  //! Freezes the specified flipbook.
  void freeze(FlipBook *flipbook);
  void unfreeze(FlipBook *flipbook);

  //! Resets the PreviewFxManager for input fx. All previously rendered previews
  //! for it are cleared.
  void reset(TFxP fx);
  //! Resets the PreviewFxManager together with all internal rendering objects.
  void reset(bool detachFlipbooks = true);
  //! Resets the PreviewFxManager for input fx at frame. The previous rendered
  //! preview is cleared.
  void reset(TFxP fx, int frame);

  // return true if the preview fx instance for specified fx is with sub-camera
  // activated
  bool isSubCameraActive(TFxP fx);

private:
  friend class PreviewFxInstance;
  void emitRenderedFrame(unsigned long fxId,
                         const TRenderPort::RenderData &renderData);
  void emitStartedFrame(unsigned long fxId,
                        const TRenderPort::RenderData &renderData);

signals:

  void startedFrame(unsigned long fxId, TRenderPort::RenderData renderData);
  void renderedFrame(unsigned long fxId, TRenderPort::RenderData renderData);
  void refreshViewRects(unsigned long fxId);

protected slots:

  void onLevelChanged();
  void onFxChanged();
  void onXsheetChanged();
  void onObjectChanged(bool isDragging);

  void onStartedFrame(unsigned long fxId, TRenderPort::RenderData renderData);
  void onRenderedFrame(unsigned long fxId, TRenderPort::RenderData renderData);

  void onRefreshViewRects(unsigned long id);
};

#endif
