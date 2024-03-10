#pragma once

#ifndef PREVIEWER_INCLUDED
#define PREVIEWER_INCLUDED

#include <memory>

#include "traster.h"
#include "tfx.h"

#include "trenderer.h"

#include <QObject>
#include <QTimer>

// forward declarations
class TRenderSettings;
class TXshLevel;
class TFrameId;
class TFilePath;
// class TRenderPort
// {
//     class RenderData;
// };
// class TRenderPort::RenderData;

//=============================================================================
// Previewer
//-----------------------------------------------------------------------------

class Previewer final : public QObject, public TFxObserver {
  Q_OBJECT

  class Imp;
  std::unique_ptr<Imp> m_imp;

  Previewer(bool subcamera);
  ~Previewer();

  // not implemented
  Previewer(const Previewer &);
  void operator=(const Previewer &);

public:
  class Listener {
  public:
    QTimer m_refreshTimer;

    Listener();
    virtual ~Listener() {}

    void requestTimedRefresh();
    virtual TRectD getPreviewRect() const = 0;

    virtual void onRenderStarted(int frame){};
    virtual void onRenderCompleted(int frame){};
    virtual void onRenderFailed(int frame){};

    virtual void onPreviewUpdate() {}
  };

public:
  static Previewer *instance(bool subcameraPreview = false);

  static void clearAll();

  static void suspendRendering(bool suspend);

  void addListener(Listener *);
  void removeListener(Listener *);

  TRasterP getRaster(int frame, bool renderIfNeeded = true) const;
  void addFramesToRenderQueue(const std::vector<int> frames) const;
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
