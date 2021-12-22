

// Toonz includes
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tropcm.h"
#include "tvectorrenderdata.h"
#include "tsystem.h"
#include "tvectorgl.h"

// Qt includes
#include <QDir>
#include <QImage>
#include <QOffscreenSurface>
#include <QThread>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include "toonz/stylemanager.h"

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

TFilePath rootPath;

//-----------------------------------------------------------------------------

void convertRaster32ToImage(TRaster32P ras, QImage *image) {
  int lx = ras->getLx();
  int ly = ras->getLy();
  int i, j;
  ras->lock();
  for (i = 0; i < lx; i++)
    for (j = 0; j < ly; j++) {
      TPixel32 pix = ras->pixels(ly - 1 - j)[i];
      QRgb value;
      value = qRgba(pix.r, pix.g, pix.b, pix.m);
      image->setPixel(i, j, value);
    }
  ras->unlock();
}

}  // namespace

//********************************************************************************
//    StyleLoaderTask definition
//********************************************************************************

class CustomStyleManager::StyleLoaderTask final : public TThread::Runnable {
  CustomStyleManager *m_manager;
  TFilePath m_fp;
  PatternData m_data;
  std::shared_ptr<QOffscreenSurface> m_offScreenSurface;

public:
  StyleLoaderTask(CustomStyleManager *manager, const TFilePath &fp);

  void run() override;

  void onFinished(TThread::RunnableP sender) override;
};

//-----------------------------------------------------------------------------

CustomStyleManager::StyleLoaderTask::StyleLoaderTask(
    CustomStyleManager *manager, const TFilePath &fp)
    : m_manager(manager), m_fp(fp) {
  connect(this, SIGNAL(finished(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));

  if (QThread::currentThread() == qGuiApp->thread()) {
    m_offScreenSurface.reset(new QOffscreenSurface());
    m_offScreenSurface->setFormat(QSurfaceFormat::defaultFormat());
    m_offScreenSurface->create();
  }
}

//-----------------------------------------------------------------------------

void CustomStyleManager::StyleLoaderTask::run() {
  try {
    // Fetch the level
    TLevelReaderP lr(m_fp);
    TLevelP level = lr->loadInfo();
    if (!level || level->getFrameCount() == 0) return;

    // Fetch the image of the first frame in the level
    TLevel::Iterator frameIt = level->begin();
    if (frameIt == level->end()) return;
    TImageP img = lr->getFrameReader(frameIt->first)->load();

    // Process the image
    const QSize &qChipSize = m_manager->getChipSize();
    TDimension chipSize(qChipSize.width(), qChipSize.height());

    TVectorImageP vimg = img;
    TRasterImageP rimg = img;

    TRaster32P ras;

    QImage *image = nullptr;

    if (vimg) {
      assert(level->getPalette());
      TPalette *vPalette = level->getPalette();
      assert(vPalette);
      vimg->setPalette(vPalette);

#ifdef LINUX
      TOfflineGL *glContext = 0;
      glContext             = TOfflineGL::getStock(chipSize);
      glContext->clear(TPixel32::White);
#else
      QOpenGLContext *glContext = new QOpenGLContext();
      if (QOpenGLContext::currentContext())
        glContext->setShareContext(QOpenGLContext::currentContext());
      glContext->setFormat(QSurfaceFormat::defaultFormat());
      glContext->create();
      glContext->makeCurrent(m_offScreenSurface.get());
      // attaching stencil buffer here as some styles use it
      QOpenGLFramebufferObject fb(
          chipSize.lx, chipSize.ly,
          QOpenGLFramebufferObject::CombinedDepthStencil);

      fb.bind();
      // Draw
      glViewport(0, 0, chipSize.lx, chipSize.ly);
      glClearColor(1.0, 1.0, 1.0, 1.0);  // clear with white color
      glClear(GL_COLOR_BUFFER_BIT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluOrtho2D(0, chipSize.lx, 0, chipSize.ly);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
#endif

      TRectD bbox = img->getBBox();
      double scx  = 0.8 * chipSize.lx / bbox.getLx();
      double scy  = 0.8 * chipSize.ly / bbox.getLy();
      double sc   = std::min(scx, scy);
      double dx   = 0.5 * chipSize.lx;
      double dy   = 0.5 * chipSize.ly;

      TAffine aff =
          TTranslation(dx, dy) * TScale(sc) *
          TTranslation(-0.5 * (bbox.x0 + bbox.x1), -0.5 * (bbox.y0 + bbox.y1));
      TVectorRenderData rd(aff, chipSize, vPalette, 0, true);

#ifdef LINUX
      glContext->draw(img, rd);
      // No need to clone! The received raster already is a copy of the
      // context's buffer
      ras = glContext->getRaster();  //->clone();
#else
      tglDraw(rd, vimg.getPointer());

      image = new QImage(fb.toImage().scaled(QSize(chipSize.lx, chipSize.ly),
                                             Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation));
      fb.release();
      glContext->deleteLater();
#endif
    } else if (rimg) {
      TDimension size = rimg->getRaster()->getSize();
      if (size == chipSize)
        ras = rimg->getRaster()->clone();  // Yep, this may be necessary
      else {
        TRaster32P rout(chipSize);

        TRop::resample(rout, rimg->getRaster(),
                       TScale((double)chipSize.lx / size.lx,
                              (double)chipSize.ly / size.ly));

        TRop::addBackground(rout, TPixel::White);
        ras = rout;
      }
#ifndef LINUX
      image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
      convertRaster32ToImage(ras, image);
#endif
    } else
      assert(!"unsupported type for custom styles!");

#ifdef LINUX
    image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
    convertRaster32ToImage(ras, image);
#endif

    m_data.m_patternName = m_fp.getName();
    m_data.m_isVector    = (m_fp.getType() == "pli" || m_fp.getType() == "svg");
    m_data.m_image       = image;
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void CustomStyleManager::StyleLoaderTask::onFinished(
    TThread::RunnableP sender) {
  // On the main thread...
  if (m_data.m_image)  // Everything went ok
  {
    m_manager->m_patterns.push_back(m_data);
    emit m_manager->patternAdded();
  }
}

//********************************************************************************
//    CustomStyleManager implementation
//********************************************************************************

CustomStyleManager::CustomStyleManager(const TFilePath &stylesFolder,
                                       QString filters, QSize chipSize)
    : m_stylesFolder(stylesFolder), m_filters(filters), m_chipSize(chipSize) {
  m_executor.setMaxActiveTasks(1);
}

//-----------------------------------------------------------------------------

int CustomStyleManager::getPatternCount() { return m_patterns.size(); }

//-----------------------------------------------------------------------------

CustomStyleManager::PatternData CustomStyleManager::getPattern(int index) {
  return (index < 0 || index >= m_patterns.size()) ? PatternData()
                                                   : m_patterns[index];
}

//-----------------------------------------------------------------------------

TFilePath CustomStyleManager::getRootPath() { return ::rootPath; }

//-----------------------------------------------------------------------------

void CustomStyleManager::setRootPath(const TFilePath &rootPath) {
  ::rootPath = rootPath;
}

//-----------------------------------------------------------------------------

void CustomStyleManager::loadItems() {
  // Build the folder to be read
  const TFilePath &rootFP(getRootPath());

  assert(rootFP != TFilePath());
  if (rootFP == TFilePath()) return;

  QDir patternDir(
      QString::fromStdWString((rootFP + m_stylesFolder).getWideString()));
  patternDir.setNameFilters(m_filters.split(' '));

  // Read the said folder
  TFilePathSet fps;
  try {
    TSystem::readDirectory(fps, patternDir);
  } catch (...) {
    return;
  }

  // Delete patterns no longer in the folder
  TFilePathSet newFps;
  TFilePathSet::iterator it;
  int i;
  for (i = 0; i < m_patterns.size(); i++) {
    PatternData data = m_patterns.at(i);
    for (it = fps.begin(); it != fps.end(); ++it) {
      if (data.m_patternName == it->getName() &&
          data.m_isVector == (it->getType() == "pli"))
        break;
    }

    if (it == fps.end()) {
      m_patterns.removeAt(i);
      i--;
    } else
      fps.erase(it);  // The style is not new, so don't generate tasks for it
  }

  // For each (now new) file entry, generate a fetching task
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++)
    m_executor.addTask(new StyleLoaderTask(this, *it));
}
