#pragma once

#ifndef MOVIERENDERER_INCLUDED
#define MOVIERENDERER_INCLUDED

#include <QObject>
#include "trenderer.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================================================

//  Forward declarations

class TFilePath;
class TRenderer;
class ToonzScene;

//=================================================================================

//=========================================================
//
//    MovieRenderer
//
//---------------------------------------------------------

/*!
MovieRenderer is the high-level API class responsible for rendering
Toonz scenes into movies.
In a more generic view, the term 'movie' represents here a generic sequence
of images, which may even be kept in memory rather than written to file.
*/

class DVAPI MovieRenderer final : public QObject {
  Q_OBJECT

  class Imp;
  Imp *m_imp;

public:
  class Listener {
  public:
    virtual bool onFrameCompleted(int frame) = 0;
    virtual bool onFrameFailed(int frame, TException &e) = 0;
    virtual void onSequenceCompleted(const TFilePath &fp) = 0;
    virtual ~Listener() {}
  };

public:
  MovieRenderer(ToonzScene *scene, const TFilePath &moviePath,
                int threadCount = 1, bool cacheResults = true);

  ~MovieRenderer();

  void setRenderSettings(const TRenderSettings &renderData);
  void setDpi(double xDpi, double yDpi);

  void addListener(Listener *listener);

  void enablePrecomputing(bool on);
  bool isPrecomputingEnabled() const;

  TRenderer *getTRenderer();

  void addFrame(double frame, const TFxPair &fx);

  void start();

public slots:

  void onCanceled();

private:
  // not implemented
  MovieRenderer(const MovieRenderer &);
  MovieRenderer &operator=(const MovieRenderer &);
};

#endif
