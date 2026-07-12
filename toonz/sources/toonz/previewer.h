#pragma once

#ifndef PREVIEWER_INCLUDED
#define PREVIEWER_INCLUDED

#include <memory>
#include <vector>

#include "traster.h"
#include "tfx.h"
#include "trenderer.h"

#include <QObject>
#include <QTimer>

// Forward declarations
class TRenderSettings;
class TXshLevel;
class TFrameId;
class TFilePath;

//=============================================================================
// Previewer
//-----------------------------------------------------------------------------

class Previewer final : public QObject, public TFxObserver {
  Q_OBJECT

  class Imp;
  std::unique_ptr<Imp> m_imp;

  Previewer(bool subcamera);
  ~Previewer();

  // Prohibit copying and moving
  Q_DISABLE_COPY_MOVE(Previewer)

public:
  class Listener {
  public:
    QTimer m_refreshTimer;

    Listener();
    virtual ~Listener() = default;

    void requestTimedRefresh();
    virtual TRectD getPreviewRect() const = 0;

    virtual void onRenderStarted(int frame) {}
    virtual void onRenderCompleted(int frame) {}
    virtual void onRenderFailed(int frame) {}

    virtual void onPreviewUpdate() {}
  };

public:
  static Previewer *instance(bool subcameraPreview = false);
  static void clearAll();
  static void suspendRendering(bool suspend);

  void addListener(Listener *listener);
  void removeListener(Listener *listener);

  TRasterP getRaster(int frame, bool renderIfNeeded = true) const;
  void addFramesToRenderQueue(const std::vector<int> &frames) const;
  bool isFrameReady(int frame) const;

  bool doSaveRenderedFrames(TFilePath fp);
  bool isActive() const;
  bool isBusy() const;

  void onChange(const TFxChange &change) override;

  void onImageChange(TXshLevel *xl, const TFrameId &fid);
  void onLevelChange(TXshLevel *xl);

  void clear(int frame);
  void clear();

  std::vector<UCHAR> &getProgressBarStatus() const;
  void clearAllUnfinishedFrames();

private:
  friend class Imp;
  void emitStartedFrame(const TRenderPort::RenderData &renderData);
  void emitRenderedFrame(const TRenderPort::RenderData &renderData);
  void emitFailedFrame(const TRenderPort::RenderData &renderData);

signals:
  void activedChanged();
  void startedFrame(TRenderPort::RenderData renderData);
  void renderedFrame(TRenderPort::RenderData renderData);
  void failedFrame(TRenderPort::RenderData renderData);

public slots:
  void saveFrame();
  void saveRenderedFrames();

  void update();
  void updateView();

  void onLevelChanged();
  void onFxChanged();
  void onXsheetChanged();
  void onObjectChanged();

protected slots:
  void onStartedFrame(TRenderPort::RenderData renderData);
  void onRenderedFrame(TRenderPort::RenderData renderData);
  void onFailedFrame(TRenderPort::RenderData renderData);
};

#endif
