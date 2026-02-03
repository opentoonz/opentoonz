#pragma once

#ifndef FXHISTOGRAMRENDER_H
#define FXHISTOGRAMRENDER_H

#include "tcommon.h"
#include <QWidget>

#include "tfx.h"
#include "trenderer.h"
#include "tthreadmessage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class Histograms;
class TFxHandle;
class TFrameHandle;
class TXsheetHandle;
class TSceneHandle;
class TXshLevelHandle;
class ToonzScene;

//=========================================================
// FxHistogramRenderPort
//---------------------------------------------------------
//! Implements the TRenderPort virtual class
//! This class receives and handles signals from the TThread::Runnable class
//! that make the render of frame.
class FxHistogramRenderPort final : public QObject, public TRenderPort {
  Q_OBJECT

public:
  FxHistogramRenderPort();
  ~FxHistogramRenderPort();

  void onRenderRasterCompleted(const RenderData &renderData) override;
  void onRenderFailure(const RenderData &renderData, TException &e) override{};
  void onRenderFinished(){};

signals:

  void renderCompleted(const TRasterP &, UINT);
  void renderFailure(double);
};

//=========================================================
// FxHistogramRender
//---------------------------------------------------------

class DVAPI FxHistogramRender final : public QObject {
  Q_OBJECT

private:
  //! Stores some information about the frame to render needed to the
  //! PreviewFxManager class
  class FrameInfo {
  public:
    FrameInfo() : m_frame(-1), m_renderId((UINT)-1) {}

    TFxP m_fx;
    std::string m_fxAlias;
    int m_frame;
    //! renderId given from renderer
    UINT m_renderId;
  };

  FxHistogramRenderPort *m_renderPort;
  TRenderer m_renderer;
  FrameInfo m_lastFrameInfo;
  TThread::Mutex m_mutex;
  QList<UINT> m_abortedRendering;
  ToonzScene *m_scene;
  Histograms *m_histograms;
  bool m_isCameraViewMode;

public:
  FxHistogramRender();
  ~FxHistogramRender();

  static FxHistogramRender *instance();

  void setScene(ToonzScene *scene);
  void setHistograms(Histograms *histograms);

  void setIsCameraViewMode(bool isCameraViewMode) {
    m_isCameraViewMode = isCameraViewMode;
  }
  bool isCameraViewMode() { return m_isCameraViewMode; }

  void computeHistogram(TFxP fx, int frame);
  void invalidateFrame(int frame);

private:
  void remakeRender();
  void updateRenderer(int frame);

protected slots:
  void onRenderCompleted(const TRasterP &, UINT);
};

#endif  // FXHISTOGRAMRENDER_H
