

// TnzCore includes
#include "tsystem.h"
#include "tstopwatch.h"
#include "tthreadmessage.h"
#include "timagecache.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "trop.h"
#include "tsop.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/trasterimageutils.h"
#include "toonz/levelupdater.h"
#include "toutputproperties.h"
#include "toonz/boardsettings.h"

// tcg includes
#include "tcg/tcg_macros.h"

// Qt includes
#include <QCoreApplication>

#include "toonz/movierenderer.h"

//**************************************************************************
//    Local Namespace  stuff
//**************************************************************************

namespace {

int RenderSessionId = 0;

//---------------------------------------------------------

void addMark(const TRasterP &mark, TRasterImageP img) {
  TRasterP raster = img->getRaster();

  if (raster->getLx() >= mark->getLx() && raster->getLy() >= mark->getLy()) {
    TRasterP ras = raster->clone();

    int borderx = troundp(0.035 * (ras->getLx() - mark->getLx()));
    int bordery = troundp(0.035 * (ras->getLy() - mark->getLy()));

    TRect rect = TRect(borderx, bordery, borderx + mark->getLx() - 1,
                       bordery + mark->getLy() - 1);
    TRop::over(ras->extract(rect), mark);

    img->setRaster(ras);
  }
}

//---------------------------------------------------------

void getRange(ToonzScene *scene, bool isPreview, int &from, int &to) {
  TSceneProperties *sprop = scene->getProperties();

  int step;
  if (isPreview)
    sprop->getPreviewProperties()->getRange(from, to, step);
  else
    sprop->getOutputProperties()->getRange(from, to, step);

  if (to < 0) {
    TXsheet *xs = scene->getXsheet();

    // NOTE: Use of numeric_limits::min is justified since the type is
    // *INTERGRAL*.
    from = (std::numeric_limits<int>::max)(),
    to   = (std::numeric_limits<int>::min)();

    for (int k = 0; k < xs->getColumnCount(); ++k) {
      int r0, r1;
      xs->getCellRange(k, r0, r1);

      from = std::min(from, r0), to = std::max(to, r1);
    }
  }
}

//---------------------------------------------------------

QString getPreviewName(unsigned long renderSessionId) {
  return "previewed" + QString::number(renderSessionId) + ".noext";
}

}  // namespace

//**************************************************************************
//    MovieRenderer::Imp  definition
//**************************************************************************

class MovieRenderer::Imp final : public TRenderPort, public TSmartObject {
public:
  ToonzScene *m_scene;
  TRenderer m_renderer;
  TFilePath m_fp;

  TRenderSettings m_renderSettings;
  TDimension m_frameSize;
  double m_xDpi, m_yDpi;

  std::set<MovieRenderer::Listener *> m_listeners;
  std::unique_ptr<LevelUpdater> m_levelUpdaterA, m_levelUpdaterB;
  TSoundTrackP m_st;

  std::map<double, std::pair<TRasterP, TRasterP>> m_toBeSaved;
  std::vector<std::pair<double, TFxPair>> m_framesToBeRendered;
  std::string m_renderCacheId;
  /*--- 同じラスタのキャッシュを使いまわすとき、
          最初のものだけガンマをかけ、以降はそれを使いまわすようにする。
  ---*/
  std::map<double, bool> m_toBeAppliedGamma;

  TThread::Mutex m_mutex;

  int m_renderSessionId;
  long m_whiteSample;

  int m_nextFrameIdxToSave;
  int m_savingThreadsCount;
  bool m_firstCompletedRaster;
  bool m_failure;
  bool m_cacheResults;
  bool m_preview;
  bool m_movieType;

public:
  Imp(ToonzScene *scene, const TFilePath &moviePath, int threadCount,
      bool cacheResults);
  ~Imp();

  // TRenderPort methods

  void onRenderRasterCompleted(const RenderData &renderData) override;
  void onRenderFailure(const RenderData &renderData, TException &e) override;

  /*-- キャンセル時にはm_overallRenderedRegionを更新しない --*/
  void onRenderFinished(bool isCanceled = false) override;

  void doRenderRasterCompleted(const RenderData &renderData);
  void doPreviewRasterCompleted(const RenderData &renderData);

  // Helper methods

  void prepareForStart();
  void addSoundtrack(int r0, int r1, double fps);
  void postProcessImage(const TRasterImageP &img, bool has64bitOutputSupport,
                        const TRasterP &mark, int frame);

  //! Saves the specified rasters at the specified time; returns whether the
  //! frames were successfully saved, and
  //! the associated time-adjusted level frame.
  std::pair<bool, int> saveFrame(double frame,
                                 const std::pair<TRasterP, TRasterP> &rasters);
  std::string getRenderCacheId();

  void addBoard();
};

//---------------------------------------------------------

MovieRenderer::Imp::Imp(ToonzScene *scene, const TFilePath &moviePath,
                        int threadCount, bool cacheResults)
    : m_scene(scene)
    , m_renderer(threadCount)
    , m_fp(moviePath)
    , m_frameSize(scene->getCurrentCamera()->getRes())
    , m_xDpi(72)
    , m_yDpi(72)
    , m_renderSessionId(RenderSessionId++)
    , m_nextFrameIdxToSave(0)
    , m_savingThreadsCount(0)
    , m_whiteSample(0)
    , m_firstCompletedRaster(
          true)         //< I know, sounds weird - it's just set to false
    , m_failure(false)  //  AFTER the first completed raster gets processed
    , m_cacheResults(cacheResults)
    , m_preview(moviePath.isEmpty())
    , m_movieType(isMovieType(moviePath)) {
  m_renderCacheId =
      m_fp.withName(m_fp.getName() + "#RENDERID" +
                    QString::number(m_renderSessionId).toStdString())
          .getLevelName();

  m_renderer.addPort(this);
}

//---------------------------------------------------------

MovieRenderer::Imp::~Imp() {
  m_renderer.removePort(this);  // Please, note: a TRenderer instance is
                                // currently a shared-pointer-like
}  // object to a private worker. *That* object may outlive the TRenderer
   // instance.

//---------------------------------------------------------

void MovieRenderer::Imp::prepareForStart() {
  struct locals {
    static void eraseUncompatibleExistingLevel(
        const TFilePath &fp, const TDimension &imageSize)  // nothrow
    {
      assert(!fp.isEmpty());

      if (TSystem::doesExistFileOrLevel(fp)) {
        bool remove = false;
        // In case the raster specifics are different from those of a currently
        // existing movie, erase it
        try {
          if (fp.isFfmpegType()) {
            TSystem::removeFileOrLevel(fp);
          } else {
            TLevelReaderP lr(fp);
            lr->loadInfo();

            const TImageInfo *info = lr->getImageInfo();
            if (!info || info->m_lx != imageSize.lx ||
                info->m_ly != imageSize.ly)
              TSystem::removeFileOrLevel(fp);  // nothrow
          }
        } catch (...) {
          // Same if the level could not be read/opened
          TSystem::removeFileOrLevel(fp);  // nothrow
        }

        // NOTE: The level removal procedure could still fail.
        // In this case, no signaling takes place. The level readers will throw
        // when the time to write on the file comes, leading to a render
        // failure.
      }
    }
  };

  TOutputProperties *oprop = m_scene->getProperties()->getOutputProperties();
  double frameRate         = (double)oprop->getFrameRate();

  /*-- Frame rate の stretch --*/
  double stretchFactor = oprop->getRenderSettings().m_timeStretchTo /
                         oprop->getRenderSettings().m_timeStretchFrom;
  frameRate *= stretchFactor;

  // Get the shrink
  int shrinkX = m_renderSettings.m_shrinkX,
      shrinkY = m_renderSettings.m_shrinkY;

  // Build the render area
  TPointD cameraPos(-0.5 * m_frameSize.lx, -0.5 * m_frameSize.ly);
  TDimensionD cameraRes(double(m_frameSize.lx) / shrinkX,
                        double(m_frameSize.ly) / shrinkY);
  TDimension cameraResI(cameraRes.lx, cameraRes.ly);

  TRectD renderArea(cameraPos.x, cameraPos.y, cameraPos.x + cameraRes.lx,
                    cameraPos.y + cameraRes.ly);
  setRenderArea(renderArea);

  if (!m_fp.isEmpty()) {
    try  // Construction of a LevelUpdater may throw (well, almost ANY operation
         // on a LevelUpdater
    {    // could throw). But due to backward compatibility this function is
         // assumed to be non-throwing.
      if (!m_renderSettings.m_stereoscopic) {
        locals::eraseUncompatibleExistingLevel(m_fp, cameraResI);

        m_levelUpdaterA.reset(new LevelUpdater(
            m_fp, oprop->getFileFormatProperties(m_fp.getType())));
        m_levelUpdaterA->getLevelWriter()->setFrameRate(frameRate);
      } else {
        TFilePath leftFp  = m_fp.withName(m_fp.getName() + "_l");
        TFilePath rightFp = m_fp.withName(m_fp.getName() + "_r");

        locals::eraseUncompatibleExistingLevel(leftFp, cameraResI);
        locals::eraseUncompatibleExistingLevel(rightFp, cameraResI);

        m_levelUpdaterA.reset(new LevelUpdater(
            leftFp, oprop->getFileFormatProperties(leftFp.getType())));
        m_levelUpdaterA->getLevelWriter()->setFrameRate(frameRate);

        m_levelUpdaterB.reset(new LevelUpdater(
            rightFp, oprop->getFileFormatProperties(rightFp.getType())));
        m_levelUpdaterB->getLevelWriter()->setFrameRate(frameRate);
      }
    } catch (...) {
      // If we get here, it's because one of the LevelUpdaters could not be
      // created. So, let's say
      // that if one could not be created, then ALL OF THEM couldn't (ie saving
      // is not possible at all).
      m_levelUpdaterA.reset();
      m_levelUpdaterB.reset();
    }
  }
}

//---------------------------------------------------------

void MovieRenderer::Imp::addSoundtrack(int r0, int r1, double fps) {
  TCG_ASSERT(r0 <= r1, return );

  TXsheet::SoundProperties *prop =
      new TXsheet::SoundProperties();  // Ownership will be surrendered ...
  prop->m_frameRate = fps;

  TSoundTrack *snd = m_scene->getXsheet()->makeSound(prop);  // ... here
  if (!snd) {
    // No soundtrack
    m_whiteSample = (r1 - r0 + 1) * 918;  // 918?? wtf... I don't think it has
    return;  // any arcane meaning - but i'm not touching it.
  }          // My impression would be that... no sound implies
             // no access to m_whiteSample, no?

  double samplePerFrame = snd->getSampleRate() / fps;

  // Extract the useful part of scene soundtrack
  TSoundTrackP snd1 = snd->extract((TINT32)(r0 * samplePerFrame),
                                   (TINT32)(r1 * samplePerFrame));
  assert(!m_st);
  if (!m_st) {
    // First, add white sound before the 'from' instant
    m_st          = TSoundTrack::create(snd1->getFormat(), m_whiteSample);
    m_whiteSample = 0;  // Why?  Probably being pedantic here... I guess
  }

  // Then, add the rest
  TINT32 fromSample = m_st->getSampleCount();
  TINT32 numSample =
      std::max(TINT32((r1 - r0 + 1) * samplePerFrame), snd1->getSampleCount());

  m_st = TSop::insertBlank(m_st, fromSample, numSample + m_whiteSample);
  m_st->copy(snd1, TINT32(fromSample + m_whiteSample));

  m_whiteSample = 0;
}

//---------------------------------------------------------

void MovieRenderer::Imp::onRenderRasterCompleted(const RenderData &renderData) {
  if (m_preview)
    doPreviewRasterCompleted(renderData);
  else
    doRenderRasterCompleted(renderData);
}

//---------------------------------------------------------

void MovieRenderer::Imp::postProcessImage(const TRasterImageP &img,
                                          bool has64bitOutputSupport,
                                          const TRasterP &mark, int frame) {
  img->setDpi(m_xDpi, m_yDpi);

  if (img->getRaster()->getPixelSize() == 8 && !has64bitOutputSupport) {
    TRaster32P aux(img->getRaster()->getLx(), img->getRaster()->getLy());
    TRop::convert(aux, img->getRaster());
    img->setRaster(aux);
  }

  if (mark) addMark(mark, img);

  if (Preferences::instance()->isSceneNumberingEnabled())
    TRasterImageUtils::addGlobalNumbering(img, m_scene->getSceneName(), frame);
}

//---------------------------------------------------------------------

std::pair<bool, int> MovieRenderer::Imp::saveFrame(
    double frame, const std::pair<TRasterP, TRasterP> &rasters) {
  bool success = false;

  // Build the frame number to write to
  double stretchFac = double(m_renderSettings.m_timeStretchTo) /
                      m_renderSettings.m_timeStretchFrom;

  int fr = (stretchFac != 1) ? tround(frame * stretchFac) : int(frame);

  int boardDuration = 0;
  if (m_movieType) {
    BoardSettings *bs =
        m_scene->getProperties()->getOutputProperties()->getBoardSettings();
    boardDuration = (bs->isActive()) ? bs->getDuration() : 0;
  }

  TFrameId fid(fr + 1 + boardDuration);

  if (m_levelUpdaterA.get()) {
    assert(m_levelUpdaterB.get() || !rasters.second);

    // Analyze writer
    bool has64bitOutputSupport = false;
    {
      if (TImageWriterP writerA =
              m_levelUpdaterA->getLevelWriter()->getFrameWriter(fid))
        has64bitOutputSupport = writerA->is64bitOutputSupported();

      // NOTE: If the writer could not be retrieved, the updater will throw.
      // Failure will be catched then.
    }

    // Prepare the images to be flushed
    TRasterP rasterA = rasters.first, rasterB = rasters.second;
    assert(rasterA);

    /*--- 同じラスタのキャッシュを使いまわすとき、
    最初のものだけガンマをかけ、以降はそれを使いまわすようにする。
---*/
    if (m_renderSettings.m_gamma != 1.0 && m_toBeAppliedGamma[frame]) {
      TRop::gammaCorrect(rasterA, m_renderSettings.m_gamma);
      if (rasterB) TRop::gammaCorrect(rasterB, m_renderSettings.m_gamma);
    }

    // Flush images
    try {
      TRasterImageP imgA(rasterA);
      postProcessImage(imgA, has64bitOutputSupport, m_renderSettings.m_mark,
                       fid.getNumber());

      m_levelUpdaterA->update(fid, imgA);

      if (rasterB) {
        TRasterImageP imgB(rasterB);
        postProcessImage(imgB, has64bitOutputSupport, m_renderSettings.m_mark,
                         fid.getNumber());

        m_levelUpdaterB->update(fid, imgB);
      }

      // Should no more throw from here on

      if (m_cacheResults) {
        if (imgA->getRaster()->getPixelSize() == 8) {
          // Convert 64-bit images to 32 - cached images are supposed to be
          // 32-bit
          TRaster32P aux(imgA->getRaster()->getLx(),
                         imgA->getRaster()->getLy());

          TRop::convert(aux, imgA->getRaster());
          imgA->setRaster(aux);
        }

        TImageCache::instance()->add(
            m_renderCacheId + std::to_string(fid.getNumber()), imgA);
      }

      success = true;
    } catch (...) {
      // Nothing. The images could not be saved for whatever reason.
      // Failure is reported.
    }
  }

  return std::make_pair(success, fr);
}

//---------------------------------------------------------------------

void MovieRenderer::Imp::doRenderRasterCompleted(const RenderData &renderData) {
  assert(!(m_cacheResults &&
           m_levelUpdaterB.get()));  // Cannot cache results on stereoscopy

  QMutexLocker locker(&m_mutex);

  // Build soundtrack at the first time a frame is completed - and the filetype
  // is that of a movie.
  if (m_firstCompletedRaster && m_movieType && !m_st) {
    int from, to;
    getRange(m_scene, false, from, to);

    TLevelP oldLevel(m_levelUpdaterA->getInputLevel());
    if (oldLevel) {
      from = std::min(from, oldLevel->begin()->first.getNumber() - 1);
      to   = std::max(to, (--oldLevel->end())->first.getNumber() - 1);
    }

    addSoundtrack(
        from, to,
        m_scene->getProperties()->getOutputProperties()->getFrameRate());

    if (m_st) {
      m_levelUpdaterA->getLevelWriter()->saveSoundTrack(m_st.getPointer());
      if (m_levelUpdaterB.get())
        m_levelUpdaterB->getLevelWriter()->saveSoundTrack(m_st.getPointer());
    }

    addBoard();
  }

  // Output frames must be *cloned*, since the supplied rasters will be
  // overwritten by m_renderer
  TRasterP toBeSavedRasA = renderData.m_rasA->clone();
  TRasterP toBeSavedRasB =
      renderData.m_rasB ? renderData.m_rasB->clone() : TRasterP();

  m_toBeSaved[renderData.m_frames[0]] =
      std::make_pair(toBeSavedRasA, toBeSavedRasB);

  m_toBeAppliedGamma[renderData.m_frames[0]] = true;

  // Prepare the cluster's frames to be saved (possibly in the future)
  std::vector<double>::const_iterator jt;
  for (jt = renderData.m_frames.begin(), ++jt; jt != renderData.m_frames.end();
       ++jt) {
    m_toBeSaved[*jt]        = std::make_pair(toBeSavedRasA, toBeSavedRasB);
    m_toBeAppliedGamma[*jt] = false;
  }

  // Attempt flushing as many frames as possible to the level updater(s)
  while (!m_toBeSaved.empty()) {
    std::map<double, std::pair<TRasterP, TRasterP>>::iterator ft =
        m_toBeSaved.begin();

    // In the *movie type* case, frames must be saved sequentially.
    // If the frame is not the next one in the sequence, wait until *that* frame
    // is available.
    if (m_movieType &&
        (ft->first != m_framesToBeRendered[m_nextFrameIdxToSave].first))
      break;

    // This thread will be the one processing ft - remove it from the map to
    // prevent another
    // thread from interfering
    double frame = ft->first;
    std::pair<TRasterP, TRasterP> rasters = ft->second;

    ++m_nextFrameIdxToSave;
    m_toBeSaved.erase(ft);

    // Save current frame
    std::pair<bool, int> savedFrame;
    {
      // Time the saving procedure
      struct SaveTimer {
        int &m_count;
        SaveTimer(int &count) : m_count(count) {
          if (m_count++ == 0) TStopWatch::global(0).start();
        }
        ~SaveTimer() {
          if (--m_count == 0) TStopWatch::global(0).stop();
        }
      } saveTimer(m_savingThreadsCount);

      // Unlock the mutex only in case this is NOT a movie type. Single images
      // can be saved concurrently.
      struct MutexUnlocker {
        QMutexLocker *m_locker;
        ~MutexUnlocker() {
          if (m_locker) m_locker->relock();
        }
      } unlocker = {m_movieType ? (QMutexLocker *)0
                                : (locker.unlock(), &locker)};

      savedFrame = saveFrame(frame, rasters);
    }

    // Report status and deal with responses
    bool okToContinue = true;

    std::set<MovieRenderer::Listener *>::iterator lt = m_listeners.begin();

    if (savedFrame.first) {
      for (; lt != m_listeners.end(); ++lt)
        okToContinue &= (*lt)->onFrameCompleted(savedFrame.second);
    } else {
      for (; lt != m_listeners.end(); ++lt) {
        TException e;
        okToContinue &= (*lt)->onFrameFailed(savedFrame.second, e);
      }
    }

    if (!okToContinue) {
      // Some listener invoked termination of the render procedure. It seems
      // it's their right
      // to do so. I wonder what happens if two listeners would disagree on the
      // matter...
      // BTW stop the rendering, alright.

      {
        int from, to;
        getRange(m_scene, false, from,
                 to);  // It's ok since cancels can only happen from Toonz...

        for (int i = from; i < to; i++)
          TImageCache::instance()->remove(m_renderCacheId +
                                          std::to_string(i + 1));
      }

      m_renderer.stopRendering();

      m_levelUpdaterA
          .reset();  // No more saving. Further attempts to save images
      m_levelUpdaterB.reset();  // will be rejected and treated as failures.
    }
  }

  m_firstCompletedRaster = false;
}

//---------------------------------------------------------

void MovieRenderer::Imp::doPreviewRasterCompleted(
    const RenderData &renderData) {
  // Most probably unused now. I'm not reviewing this.

  assert(!m_levelUpdaterA.get());

  QMutexLocker sl(&m_mutex);

  QString name = getPreviewName(m_renderSessionId);

  TRasterP ras = renderData.m_rasA->clone();
  if (renderData.m_rasB) {
    assert(m_renderSettings.m_stereoscopic);
    TRop::makeStereoRaster(ras, renderData.m_rasB);
  }

  TRasterImageP img(ras);
  img->setDpi(m_xDpi, m_yDpi);

  try {
    if (renderData.m_info.m_mark != TRasterP())
      addMark(renderData.m_info.m_mark, img);

    if (img->getRaster()->getPixelSize() == 8) {
      TRaster32P aux(img->getRaster()->getLx(), img->getRaster()->getLy());
      TRop::convert(aux, img->getRaster());
      img->setRaster(aux);
    }

    QString frameName = name + QString::number(renderData.m_frames[0] + 1);

    TImageCache::instance()->add(frameName.toStdString(), img);

    // controlla se ci sono frame(uguali ad altri) da mettere in cache

    std::vector<double>::const_iterator jt;
    for (jt = renderData.m_frames.begin(), ++jt;
         jt != renderData.m_frames.end(); ++jt) {
      frameName = name + QString::number(*jt + 1);
      TImageCache::instance()->add(frameName.toStdString(), img);
    }
  } catch (...) {
  }

  std::set<MovieRenderer::Listener *>::iterator listenerIt =
      m_listeners.begin();

  bool okToContinue = true;

  for (; listenerIt != m_listeners.end(); ++listenerIt)
    okToContinue &= (*listenerIt)->onFrameCompleted(renderData.m_frames[0]);

  if (!okToContinue) {
    // Svuoto la cache nel caso in cui si esce dal render prima della fine
    int from, to;
    getRange(m_scene, true, from, to);
    for (int i = from; i < to; i++) {
      QString frameName = name + QString::number(i + 1);
      TImageCache::instance()->remove(frameName.toStdString());
    }

    m_renderer.stopRendering();
  }

  m_firstCompletedRaster = false;
}

//---------------------------------------------------------

void MovieRenderer::Imp::onRenderFailure(const RenderData &renderData,
                                         TException &e) {
  QMutexLocker sl(&m_mutex);  // Lock as soon as possible.
                              // No sense making it later in this case!
  m_failure = true;

  // If the saver object has already been destroyed - or it was never
  // created to begin with, nothing to be done
  if (!m_levelUpdaterA.get()) return;  // The preview case would fall here

  // Flush out as much as we can of the frames that were already rendered
  m_toBeSaved[0.0] =
      std::make_pair(TRasterP(), TRasterP());  // ?? Why is this ??

  std::map<double, std::pair<TRasterP, TRasterP>>::iterator it =
      m_toBeSaved.begin();
  while (it != m_toBeSaved.end()) {
    if (m_movieType &&
        (it->first != m_framesToBeRendered[m_nextFrameIdxToSave].first))
      break;

    // o_o!
    // I would have expected that at least those frames that were computed could
    // attempt saving! Why is this not addressed? They're even marked as
    // 'failed'!

    double stretchFac = (double)renderData.m_info.m_timeStretchTo /
                        renderData.m_info.m_timeStretchFrom;

    int fr;
    if (stretchFac != 1)
      fr = tround(it->first * stretchFac);
    else
      fr = (int)it->first;

    // No saving? Really?

    std::set<MovieRenderer::Listener *>::iterator lt = m_listeners.begin();
    bool okToContinue                                = true;

    for (; lt != m_listeners.end(); ++lt)
      okToContinue &= (*lt)->onFrameFailed((int)it->first, e);

    if (!okToContinue) m_renderer.stopRendering();

    ++m_nextFrameIdxToSave;
    m_toBeSaved.erase(it++);
  }
}

//---------------------------------------------------------

void MovieRenderer::Imp::onRenderFinished(bool isCanceled) {
  TFilePath levelName(
      m_levelUpdaterA.get()
          ? m_fp
          : TFilePath(getPreviewName(m_renderSessionId).toStdWString()));

  // Close updaters. After this, the output levels should be finalized on disk.
  m_levelUpdaterA.reset();
  m_levelUpdaterB.reset();

  if (!m_failure) {
    // Inform listeners of the render completion
    std::set<MovieRenderer::Listener *>::iterator it;
    for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
      (*it)->onSequenceCompleted(levelName);

    // I wonder why listeners are not informed of a failed sequence, btw...
  }

  release();  // The movieRenderer is released by the render process. It could
              // eventually be deleted.
}

//---------------------------------------------------------

void MovieRenderer::Imp::addBoard() {
  BoardSettings *boardSettings =
      m_scene->getProperties()->getOutputProperties()->getBoardSettings();
  if (!boardSettings->isActive()) return;
  int duration = boardSettings->getDuration();
  if (duration == 0) return;
  // Get the image size
  int shrinkX = m_renderSettings.m_shrinkX,
      shrinkY = m_renderSettings.m_shrinkY;
  TDimensionD cameraRes(double(m_frameSize.lx) / shrinkX,
                        double(m_frameSize.ly) / shrinkY);
  TDimension cameraResI(cameraRes.lx, cameraRes.ly);

  TRaster32P boardRas =
      boardSettings->getBoardRaster(cameraResI, shrinkX, m_scene);

  if (m_levelUpdaterA.get()) {
    // Flush images
    try {
      TRasterImageP img(boardRas);
      for (int f = 0; f < duration; f++) {
        m_levelUpdaterA->update(TFrameId(f + 1), img);
        if (m_levelUpdaterB.get())
          m_levelUpdaterB->update(TFrameId(f + 1), img);
      }
    } catch (...) {
      // Nothing. The images could not be saved for whatever reason.
      // Failure is reported.
    }
  }
}

//======================================================================================

//======================
//    MovieRenderer
//----------------------

MovieRenderer::MovieRenderer(ToonzScene *scene, const TFilePath &moviePath,
                             int threadCount, bool cacheResults)
    : m_imp(new Imp(scene, moviePath, threadCount, cacheResults)) {
  m_imp->addRef();  // See MovieRenderer::start(). Can't just delete it in the
                    // dtor.
}

//---------------------------------------------------------

MovieRenderer::~MovieRenderer() { m_imp->release(); }

//---------------------------------------------------------

void MovieRenderer::setRenderSettings(const TRenderSettings &renderSettings) {
  m_imp->m_renderSettings = renderSettings;
}

//---------------------------------------------------------

void MovieRenderer::setDpi(double xDpi, double yDpi) {
  m_imp->m_xDpi = xDpi;
  m_imp->m_yDpi = yDpi;
}

//---------------------------------------------------------

void MovieRenderer::addListener(Listener *listener) {
  m_imp->m_listeners.insert(listener);
}

//---------------------------------------------------------

void MovieRenderer::addFrame(double frame, const TFxPair &fxPair) {
  m_imp->m_framesToBeRendered.push_back(std::make_pair(frame, fxPair));
}

//---------------------------------------------------------

void MovieRenderer::enablePrecomputing(bool on) {
  m_imp->m_renderer.enablePrecomputing(on);
}

//---------------------------------------------------------

bool MovieRenderer::isPrecomputingEnabled() const {
  return m_imp->m_renderer.isPrecomputingEnabled();
}

//---------------------------------------------------------

void MovieRenderer::start() {
  m_imp->prepareForStart();

  // Add a reference to MovieRenderer's Imp. The reference is 'owned' by
  // TRenderer's render process - when it
  // ends (that is, when notifies onRenderFinished), the reference is released.
  // As to TRenderer's specifics,
  // this is ensured to happen only after all the other port notifications for
  // each frame have been invoked.
  m_imp->addRef();

  // Prepare the TRenderer::RenderDatas to render
  RenderDataVector *datasToBeRendered = new RenderDataVector;
  size_t i, size = m_imp->m_framesToBeRendered.size();
  for (i = 0; i < size; ++i)
    datasToBeRendered->push_back(TRenderer::RenderData(
        m_imp->m_framesToBeRendered[i].first, m_imp->m_renderSettings,
        m_imp->m_framesToBeRendered[i].second));

  m_imp->m_renderer.startRendering(datasToBeRendered);
}

//---------------------------------------------------------

void MovieRenderer::onCanceled() { m_imp->m_renderer.stopRendering(true); }

//---------------------------------------------------------

TRenderer *MovieRenderer::getTRenderer() {
  // Again, this is somewhat BAD. The pointed-to object dies together with the
  // MovieRenderer instance.
  // Since a TRenderer is already smart-pointer-like, we could just return a
  // copy - however, it really
  // shouln't be that way. Maybe one day we'll revert that and actually use a
  // smart pointer class.

  // For now, no use of this function seems to access the returned pointer
  // beyond the lifespan of the
  // associated MovieRenderer instance - so I'm not gonna touch the class
  // interface.
  return &m_imp->m_renderer;
}
