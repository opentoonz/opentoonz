#pragma once

#ifndef MULTIMEDIARENDERER_INCLUDED
#define MULTIMEDIARENDERER_INCLUDED

#include <QObject>
#include "toonz/movierenderer.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//  Forward declarations
class TFilePath;
class ToonzScene;

//=========================================================
//
// MultimediaRenderer
//
//---------------------------------------------------------

//! High-level class that performs overlay-separated rendering.
/*!The MultimediaRenderer is typically used to export in a movie
   file format all layers contributing to a scene render - that is,
   each input node for the xsheet node is rendered in a separate level
   file whose name will be:
     <moviepath>_<column name>.<extension>
   This class uses the MovieRenderer class to render each xsheet input
   node on separate files.
   \b NOTE: Observe that the special feature for which some layering fx
   nodes may act on all lower xsheet levels (e.g. AddFx) is ignored in
   this process, meaning that these nodes are considered part of the
   xsheet's implicit overlaying system.

   \sa MovieRenderer class.
*/

class ToonzScene;

class DVAPI MultimediaRenderer final : public QObject {
  Q_OBJECT

  class Imp;
  Imp *m_imp;

public:
  //----------------------------------------------------------------

  class Listener {
  public:
    virtual bool onFrameCompleted(int frame, int column) = 0;
    virtual bool onFrameFailed(int frame, int column, TException &e) = 0;
    virtual void onSequenceCompleted(int column) = 0;
    virtual void onRenderCompleted()             = 0;
    virtual ~Listener() {}
  };

  //----------------------------------------------------------------

  MultimediaRenderer(ToonzScene *scene, const TFilePath &moviePath,
                     int multimediaMode, int threadCount = 1,
                     bool cacheResults = true);

  ~MultimediaRenderer();

  const TFilePath &getFilePath();
  int getFrameCount();
  int getColumnsCount();

  void setRenderSettings(const TRenderSettings &renderData);
  void setDpi(double xDpi, double yDpi);
  void addListener(Listener *listener);
  void enablePrecomputing(bool on);
  bool isPrecomputingEnabled() const;

  enum { COLUMNS = 1, LAYERS = 2 };
  int getMultimediaMode() const;

  //! Returns the currently active TRenderer.
  TRenderer *getTRenderer();

  //! Add a frame among the ones to be rendered.
  void addFrame(double frame);

  //! Starts the scene rendering.
  void start();

  //! Return true if the vector containing the frames to render is empty, false
  //! otherwise.
  // bool done() const;

public slots:

  void onCanceled();

private:
  // not implemented
  MultimediaRenderer(const MultimediaRenderer &);
  MultimediaRenderer &operator=(const MultimediaRenderer &);
};

#endif
