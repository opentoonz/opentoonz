#pragma once

#ifndef TRENDERER_INCLUDED
#define TRENDERER_INCLUDED

#include "trasterfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//  Forward declarations

class TRenderer;
class TRendererImp;
class TException;
class TRenderSettings;
class TRasterFxP;
class TFxCacheManager;
class TRenderResourceManager;
class TRenderResourceManagerGenerator;

class TFxPair {
public:
  TRasterFxP m_frameA, m_frameB;
  TFxPair() : m_frameA(), m_frameB() {}
  TFxPair(const TRasterFxP &frameA, TRasterFxP &frameB)
      : m_frameA(frameA), m_frameB(frameB) {}
};

// typedef std::pair<TRasterFxP, TRasterFxP> TFxPair;

//=========================================================

//====================
//    TRenderPort
//--------------------

//! The TRenderPort class acts as a spatially-declared listener to a TRenderer
//! instance.
//! TRenderPorts are the formally supported receivers of TRenderer's activity
//! notifications;
//! they provide some basic pure virtual members that are appropriately invoked
//! by the TRenderer
//! when an associated interesting render event takes place.

class DVAPI TRenderPort {
  TRectD m_renderArea;

public:
  struct RenderData;

  TRenderPort();
  virtual ~TRenderPort();

  void setRenderArea(const TRectD &area);
  TRectD &getRenderArea();

  virtual void onRenderRasterStarted(const RenderData &renderData) {}
  virtual void onRenderRasterCompleted(const RenderData &renderData) {}
  virtual void onRenderFailure(const RenderData &renderData, TException &e) {}
  virtual void onRenderFinished(bool isCanceled = false) {}
};

//=========================================================

//================================
//    TRenderPort::RenderData
//--------------------------------

//! This storage struct contains the elementary data necessary to identify a
//! cluster of identical
//! rendered frames from TRenderer. In order to avoid recalculation of identical
//! frames during a
//! render process, they are merged in one 'equivalence cluster' of which only
//! one representant is
//! rendered; the m_frames member stores each frame of such cluster. The
//! associated raster is stored
//! in the m_ras member. Additional returned infos include the TRenderSettings
//! under which the frames
//! were rendered, and unique identification vars.

struct TRenderPort::RenderData {
  std::vector<double> m_frames;  //!< Frames this output represents
  TRenderSettings m_info;        //!< Output settings description
  TRasterP m_rasA, m_rasB;  //!< The output images; m_rasB is not empty only for
                            //! interlacacing and stereoscopic.
  unsigned long m_renderId;  //!< Identifier of the rendering session this
                             //! output belongs to
  unsigned long m_taskId;  //!< Task identifier in the rendering session. Starts
                           //! at 0, preserves
  //!< the original submission order except for cluster equivalence.

  RenderData() : m_renderId((unsigned long)-1), m_taskId((unsigned long)-1) {}
  RenderData(const std::vector<double> &frames, const TRenderSettings &info,
             const TRasterP &rasA, const TRasterP &rasB, unsigned long renderId,
             unsigned long taskId)
      : m_frames(frames)
      , m_info(info)
      , m_rasA(rasA)
      , m_rasB(rasB)
      , m_renderId(renderId)
      , m_taskId(taskId) {}
};

//=================================================================================

//=========================================================
//
//    TRenderer
//
//---------------------------------------------------------

//! The TRenderer class provides the core operative interface to the Toonz
//! rendering library.

//! The TRenderer's main purpose is that of invoking a render process on a group
//! of separate threads through the \b startRendering() methods. After this is
//! done,
//! the activated process communicates with any TRenderPort previously set
//! through
//! the \b addPort() method in order to notify the render status.
//! The rendering process can be interrupted with the stopRendering() method,
//! which
//! makes all rendering threads quit safely at the most appropriate time; the
//! more
//! specific abortRendering() method may be used to terminate a render process
//! id in case
//! more than one process is running.
//! \n \n
//! Technically, this class is an \a interface to the actual rendering class,
//! which remains in background. This means that users have no necessity of
//! maintaining the
//! TRenderer instance alive as a render process is running. We may, for
//! example,
//! create a TRenderer, set its properties, launch the render, and release the
//! TRenderer
//! instance immediately after as the internal rendering class will continue
//! working on.
//! TRenderers may therefore be copied and assigned freely to refer an already
//! existing
//! internal rendering instance. These are created anew only using the default
//! TRenderer(int) constructor.
//! \n \n
//! On the other hand, TRenderer's lifespan may last longer than that of a
//! single rendering
//! process. This is especially useful when it is known that multiple rendering
//! processes share
//! some common resources (likewise, intermediate cache results), which can be
//! maintained
//! in the same internal TRenderer representation for later use.
//! \n \n
//! Some final notes about methods dealing with Toonz's lower-level API. Direct
//! invocation
//! of rendering functions, such as the TRasterFx::compute() method, is
//! discouraged. Further
//! improvements of the TRenderer's API will eventually make the need for these
//! calls obsolete. However,
//! we've implemented the possibilty to rely on TRenderer's management support
//! even in those cases -
//! it can be done by declaring the render process' interesting events through
//! the apposite <a> declare..() <\a>
//! methods and invoking install() and uninstall() on each rendering thread
//! you'll raise
//! for the process.

class DVAPI TRenderer {
  TRendererImp *m_imp;

  friend class TRenderResourceManagerGenerator;
  TRenderResourceManager *getManager(unsigned int id) const;

public:
  //! The RenderData struct contains the frame specifics to supply for a
  //! TRenderer::startRendering() call.
  struct RenderData {
    double m_frame;
    TRenderSettings m_info;
    TFxPair m_fxRoot;  // The second of pair is used for field interlacing or
                       // stereoscopic render.

    RenderData(double frame, const TRenderSettings &info, const TFxPair &fxRoot)
        : m_frame(frame), m_info(info), m_fxRoot(fxRoot) {}
  };

public:
  TRenderer(int nThread = 1);
  TRenderer(TRendererImp *);
  ~TRenderer();

  TRenderer(const TRenderer &);
  void operator=(const TRenderer &);

  operator bool() const { return m_imp; }

  void addPort(TRenderPort *port);
  void removePort(TRenderPort *port);

  unsigned long startRendering(
      const std::vector<TRenderer::RenderData> *renderDatas);
  unsigned long startRendering(double f, const TRenderSettings &info,
                               const TFxPair &actualRoot);

  void abortRendering(unsigned long renderId);
  void stopRendering(bool waitForCompleteStop = false);

  void enablePrecomputing(bool on);
  bool isPrecomputingEnabled() const;

  void setThreadsCount(int nThreads);

  static TRenderer instance();

  unsigned long rendererId();
  static unsigned long renderId();
  static unsigned long nextRenderId();

  //-----------------------------------------

  // Render instance properties

  enum RenderStatus {
    IDLE      = 0x0,
    FIRSTRUN  = 0x1,
    TESTRUN   = 0x2,
    COMPUTING = 0x4
  };
  int getRenderStatus(unsigned long renderId) const;
  bool isAborted(unsigned long renderId) const;

  //-----------------------------------------

  //  To install the TRenderer on lower-level rendering invocations
  static unsigned long buildRenderId();

  void declareRenderStart(unsigned long renderId);
  void declareRenderEnd(unsigned long renderId);

  void declareFrameStart(double frame);
  void declareFrameEnd(double frame);

  void install(unsigned long renderId);
  void uninstall();

  // Be sure that this method is called at least once before a rendering driven
  // by a working thread
  // (the TRendererStartInvoker singleton must be referred first by a thread
  // that survives, e.g. the main thread)
  static void initialize();
};

//-------------------------------------------------------------------------------

typedef std::vector<TRenderer::RenderData> RenderDataVector;

#endif
