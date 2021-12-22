#pragma once

#ifndef ICON_GENERATOR_H
#define ICON_GENERATOR_H

// TnzCore includes
#include "tthread.h"
#include "tgeometry.h"
#include "tfilepath.h"
#include "traster.h"
#include "timage.h"
#include "tpixel.h"

// Qt includes
#include <QPixmap>
#include <QThreadStorage>
#include <QEventLoop>

// STD includes
#include <map>
#include <vector>
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================================================

//    Forward declarations

class TFilePath;
class TXshLevel;
class TStageObjectSpline;
class ToonzScene;
class TOfflineGL;

//==============================================================

//**********************************************************************************
//    IconGenerator  definition
//**********************************************************************************

/*!
  \brief    The class responsible for icons management in Toonz.

  \details  It's a singleton class - in particular, rendered icons are stored in
            a shared map container for fast retrieval upon repeated icon
  requests.

            IconGenerator provides methods to submit icon requests, and to
  invalidate or
            remove icons from the internal database. In order to keep outer
  entities
            informed of the icon generation status, an iconGenerated() signal is
  emitted
            once an icon has been generated.
*/

class DVAPI IconGenerator final : public QObject {
  Q_OBJECT

public:
  class Settings {
  public:
    bool m_blackBgCheck;
    bool m_transparencyCheck;
    bool m_inksOnly;
    int m_inkIndex;
    int m_paintIndex;

    Settings()
        : m_transparencyCheck(false)
        , m_blackBgCheck(false)
        , m_inksOnly(false)
        , m_inkIndex(-1)
        , m_paintIndex(-1) {}
  };

public:
  IconGenerator();
  ~IconGenerator();

  static IconGenerator *instance();

  void setSettings(const Settings &settings) { m_settings = settings; }
  const Settings getSettings() const { return m_settings; }
  static void setFilmstripIconSize(const TDimension &dim);

  TDimension getIconSize() const;

  TOfflineGL *getOfflineGLContext();

  // icons from splines
  QPixmap getIcon(TStageObjectSpline *spline);
  void invalidate(TStageObjectSpline *spline);
  void remove(TStageObjectSpline *spline);

  // icons from toonz levels
  QPixmap getIcon(TXshLevel *sl, const TFrameId &fid, bool filmStrip = true,
                  bool onDemand = false);
  QPixmap getSizedIcon(TXshLevel *sl, const TFrameId &fid, std::string newId,
                       TDimension dim = TDimension(0, 0));
  void invalidate(TXshLevel *sl, const TFrameId &fid,
                  bool onlyFilmStrip = false);
  void remove(TXshLevel *sl, const TFrameId &fid, bool onlyFilmStrip = false);

  // icons from files
  QPixmap getIcon(const TFilePath &path,
                  const TFrameId &fid = TFrameId::NO_FRAME);
  void invalidate(const TFilePath &path,
                  const TFrameId &fid = TFrameId::NO_FRAME);
  void remove(const TFilePath &path, const TFrameId &fid = TFrameId::NO_FRAME);

  QPixmap getSceneIcon(ToonzScene *scene);  // Unused in Toonz
  void invalidateSceneIcon();

  void remap(const std::string &newIconId, const std::string &oldIconId);

  void clearRequests();
  void clearSceneIcons();

  static TRaster32P generateVectorFileIcon(const TFilePath &path,
                                           const TDimension &iconSize,
                                           const TFrameId &fid);
  static TRaster32P generateRasterFileIcon(const TFilePath &path,
                                           const TDimension &iconSize,
                                           const TFrameId &fid);
  static TRaster32P generateSceneFileIcon(const TFilePath &path,
                                          const TDimension &iconSize, int row);
  static TRaster32P generateSplineFileIcon(const TFilePath &path,
                                           const TDimension &iconSize);
  static TRaster32P generateMeshFileIcon(const TFilePath &path,
                                         const TDimension &iconSize,
                                         const TFrameId &fid);

  // This function is called when only colors of styles are changed in toonz
  // raster levels. In such case it doesn't need to re-compute icons but needs
  // to let panels to update. See TApp::onLevelColorStyleChanged() for details.
  void notifyIconGenerated() { emit iconGenerated(); }
signals:

  void iconGenerated();

public slots:

  void onStarted(TThread::RunnableP iconRenderer);
  void onCanceled(TThread::RunnableP iconRenderer);
  void onFinished(TThread::RunnableP iconRenderer);
  void onException(TThread::RunnableP iconRenderer);
  void onTerminated(TThread::RunnableP iconRenderer);

private:
  TThread::Executor m_executor;
  QThreadStorage<TOfflineGL *> m_contexts;
  TDimension m_iconSize;

  QEventLoop m_iconsTerminationLoop;  //!< Event loop used to wait for icons
                                      //! termination.

  Settings m_settings;

private:
  void addTask(const std::string &id, TThread::RunnableP iconRenderer);
};

//**********************************************************************************
//    Related non-member functions
//**********************************************************************************

template <class It>
inline void invalidateIcons(TXshLevel *sl, It fBegin, It fEnd,
                            bool onlyFilmStrip = false) {
  for (It ft = fBegin; ft != fEnd; ++ft)
    IconGenerator::instance()->invalidate(sl, *ft, onlyFilmStrip);
}

//------------------------------------------------------------------------

template <class C>
inline void invalidateIcons(TXshLevel *sl, const C &fids,
                            bool onlyFilmStrip = false) {
  invalidateIcons(sl, fids.begin(), fids.end(), onlyFilmStrip);
}

//------------------------------------------------------------------------

template <typename It>
inline void removeIcons(TXshLevel *sl, It fBegin, It fEnd,
                        bool onlyFilmStrip = false) {
  for (It ft = fBegin; ft != fEnd; ++ft)
    IconGenerator::instance()->remove(sl, *ft, onlyFilmStrip);
}

//------------------------------------------------------------------------

template <typename C>
inline void removeIcons(TXshLevel *sl, const C &fids,
                        bool onlyFilmStrip = false) {
  removeIcons(sl, fids.begin(), fids.end(), onlyFilmStrip);
}

#endif  // ICON_GENERATOR_H
