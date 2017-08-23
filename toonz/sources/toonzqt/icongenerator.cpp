

// TnzCore includes
#include "tfilepath.h"
#include "tfiletype.h"
#include "tstream.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tpixelutils.h"
#include "tropcm.h"
#include "timageinfo.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tgl.h"
#include "tvectorrenderdata.h"
#include "tstroke.h"
#include "tthreadmessage.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tmeshimage.h"

// TnzExt includes
#include "ext/meshutils.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/preferences.h"
#include "toonz/sceneresources.h"
#include "toonz/stage2.h"

// TnzQt includes
#include "toonzqt/gutil.h"

#include "toonzqt/icongenerator.h"

//=============================================================================

//===================================
//
//    Local namespace
//
//-----------------------------------

namespace {
const TDimension IconSize(80, 60);
TDimension FilmstripIconSize(0, 0);

// Access name-based storage
std::set<std::string> iconsMap;
typedef std::set<std::string>::iterator IconIterator;

//-----------------------------------------------------------------------------

// Returns true if the image request was already submitted.
bool getIcon(const std::string &iconName, QPixmap &pix,
             TXshSimpleLevel *simpleLevel = 0,
             TDimension standardSize      = TDimension(0, 0)) {
  IconIterator it;
  it = iconsMap.find(iconName);

  if (it != iconsMap.end()) {
    TImageP im         = TImageCache::instance()->get(iconName, false);
    TToonzImage *timgp = dynamic_cast<TToonzImage *>(im.getPointer());

    if (simpleLevel && timgp) {
      IconGenerator::Settings settings =
          IconGenerator::instance()->getSettings();

      TRaster32P icon(timgp->getSize());
      icon->clear();
      icon->fill((settings.m_blackBgCheck) ? TPixel::Black : TPixel::White);
      if (settings.m_transparencyCheck || settings.m_inkIndex != -1 ||
          settings.m_paintIndex != -1) {
        TRop::CmappedQuickputSettings s;
        s.m_globalColorScale  = TPixel32::Black;
        s.m_inksOnly          = false;
        s.m_transparencyCheck = settings.m_transparencyCheck;
        s.m_blackBgCheck      = settings.m_blackBgCheck;
        s.m_inkIndex          = settings.m_inkIndex;
        s.m_paintIndex        = settings.m_paintIndex;
        Preferences::instance()->getTranspCheckData(
            s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);

        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine(), s);
      } else
        TRop::quickPut(icon, timgp->getRaster(), simpleLevel->getPalette(),
                       TAffine());
      pix = rasterToQPixmap(icon, false);
      return true;
    }
    TRasterImageP img = static_cast<TRasterImageP>(im);

    if (!img) {
      pix = QPixmap();
      return true;
    }
    assert(!(TRasterGR8P)img->getRaster());
    const TRaster32P &ras = img->getRaster();
    bool isHighDpi        = false;
    // If the icon raster obtained in higher resolution than the standard
    // icon size, it may be icon displayed in high dpi monitors.
    // In such case set the device pixel ratio to the pixmap.
    // Note that the humbnails of regular levels are standardsize even if
    // they are displayed in high dpi monitors for now.
    if (standardSize != TDimension(0, 0) &&
        ras->getSize().lx > standardSize.lx &&
        ras->getSize().ly > standardSize.ly)
      isHighDpi = true;
    pix         = rasterToQPixmap(ras, false, isHighDpi);
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void setIcon(const std::string &iconName, const TRaster32P &icon) {
  if (iconsMap.find(iconName) != iconsMap.end())
    TImageCache::instance()->add(iconName, TRasterImageP(icon), true);
}

//-----------------------------------------------------------------------------
/*! Cache icon data in TToonzImage format if ToonzImageIconRenderer generates
 * them
*/
void setIcon_TnzImg(const std::string &iconName, const TRasterCM32P &icon) {
  if (iconsMap.find(iconName) != iconsMap.end())
    TImageCache::instance()->add(
        iconName, TToonzImageP(icon, TRect(icon->getSize())), true);
}

//-----------------------------------------------------------------------------

void removeIcon(const std::string &iconName) {
  IconIterator it;
  it = iconsMap.find(iconName);
  if (it != iconsMap.end()) {
    TImageCache::instance()->remove(iconName);
  }
  iconsMap.erase(iconName);
}

//-----------------------------------------------------------------------------

bool isUnpremultiplied(const TRaster32P &r) {
  int lx = r->getLx();
  int y  = r->getLy();
  r->lock();
  while (--y >= 0) {
    TPixel32 *pix    = r->pixels(y);
    TPixel32 *endPix = pix + lx;
    while (pix < endPix) {
      if (pix->r > pix->m || pix->g > pix->m || pix->b > pix->m) {
        r->unlock();
        return true;
      }
      ++pix;
    }
  }
  r->unlock();
  return false;
}

//-----------------------------------------------------------------------------

void makeChessBackground(const TRaster32P &ras) {
  ras->lock();

  const TPixel32 gray1(230, 230, 230, 255), gray2(180, 180, 180, 255);

  int lx = ras->getLx(), ly = ras->getLy();
  for (int y = 0; y != ly; ++y) {
    TPixel32 *pix = ras->pixels(y), *lineEnd = pix + lx;

    int yCol = (y & 4);

    for (int x                = 0; pix != lineEnd; ++x, ++pix)
      if (pix->m != 255) *pix = overPix((x & 4) == yCol ? gray1 : gray2, *pix);
  }

  ras->unlock();
}

}  // namespace

//=============================================================================

//==========================================
//
//    Image-to-Icon convertion methods
//
//------------------------------------------

namespace {
TRaster32P convertToIcon(TVectorImageP vimage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!vimage) return TRaster32P();

  TPalette *plt = vimage->getPalette()->clone();
  if (!plt) return TRaster32P();
  plt->setFrame(frame);

  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();

  // The image and contained within Imagebox
  // (add a small margin also to prevent problems with empty images)
  TRectD imageBox;
  {
    QMutexLocker sl(vimage->getMutex());
    imageBox = vimage->getBBox().enlarge(.1);
  }
  TPointD imageCenter = (imageBox.getP00() + imageBox.getP11()) * 0.5;

  // Calculate a transformation matrix that moves the image inside the icon.
  // The scale factor is chosen so that the image is entirely
  // contained in the icon (with a margin of 'margin' pixels)
  const int margin = 10;
  double scx       = (iconSize.lx - margin) / imageBox.getLx();
  double scy       = (iconSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);
  // Add the translation: the center point of the image is at the point
  // middle of the pixmap.
  TPointD iconCenter(iconSize.lx * 0.5, iconSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  // RenderData
  TVectorRenderData rd(aff, TRect(iconSize), plt, 0, true);

  rd.m_tcheckEnabled     = settings.m_transparencyCheck;
  rd.m_blackBgEnabled    = settings.m_blackBgCheck;
  rd.m_drawRegions       = !settings.m_inksOnly;
  rd.m_inkCheckEnabled   = settings.m_inkIndex != -1;
  rd.m_paintCheckEnabled = settings.m_paintIndex != -1;
  rd.m_colorCheckIndex =
      rd.m_inkCheckEnabled ? settings.m_inkIndex : settings.m_paintIndex;
  rd.m_isIcon = true;

  // Draw the image.
  glContext->makeCurrent();
  glContext->clear(rd.m_blackBgEnabled ? TPixel::Black : TPixel32::White);
  glContext->draw(vimage, rd);

  TRaster32P ras(iconSize);
  glContext->getRaster(ras);

  glContext->doneCurrent();

  delete plt;

  return ras;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TToonzImageP timage, int frame,
                         const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!timage) return TRaster32P();

  TPalette *plt = timage->getPalette();
  if (!plt) return TRaster32P();

  plt->setFrame(frame);

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer()) return TRaster32P();

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;
  if (std::max(double(lx) / iconSize.lx, double(ly) / iconSize.ly) ==
      double(ly) / iconSize.ly)
    iconLx = tround((double(lx) * iconSize.ly) / ly);
  else
    iconLy = tround((double(ly) * iconSize.lx) / lx);

  // icon size with correct aspect ratio
  TDimension iconSize2 = TDimension(iconLx, iconLy);

  TRaster32P icon(iconSize2);
  icon->clear();
  icon->fill(settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  TDimension dim = rasCM32->getSize();
  if (dim != iconSize2) {
    TRasterCM32P auxCM32(icon->getSize());
    auxCM32->clear();
    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (settings.m_transparencyCheck || settings.m_inksOnly ||
      settings.m_inkIndex != -1 || settings.m_paintIndex != -1) {
    TRop::CmappedQuickputSettings s;
    s.m_globalColorScale  = TPixel32::Black;
    s.m_inksOnly          = settings.m_inksOnly;
    s.m_transparencyCheck = settings.m_transparencyCheck;
    s.m_blackBgCheck      = settings.m_blackBgCheck;
    s.m_inkIndex          = settings.m_inkIndex;
    s.m_paintIndex        = settings.m_paintIndex;
    Preferences::instance()->getTranspCheckData(
        s.m_transpCheckBg, s.m_transpCheckInk, s.m_transpCheckPaint);

    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine(), s);
  } else
    TRop::quickPut(icon, rasCM32, timage->getPalette(), TAffine());

  assert(iconSize2.lx <= iconSize.lx && iconSize2.ly <= iconSize.ly);
  TRaster32P outIcon(iconSize);
  outIcon->clear();
  int dx = (outIcon->getLx() - icon->getLx()) / 2;
  int dy = (outIcon->getLy() - icon->getLy()) / 2;
  assert(dx >= 0 && dy >= 0);
  TRect box = outIcon->getBounds().enlarge(-dx, -dy);
  TRop::copy(outIcon->extract(box), icon);

  return outIcon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TRasterImageP rimage, const TDimension &iconSize) {
  if (!rimage) return TRaster32P();

  TRasterP ras = rimage->getRaster();

  if (!(TRaster32P)ras && !(TRasterGR8P)ras) return TRaster32P();

  if (ras->getSize() == iconSize) return ras;

  TRaster32P icon(iconSize);
  icon->fill(TPixel32(235, 235, 235));

  double sx = (double)icon->getLx() / ras->getLx();
  double sy = (double)icon->getLy() / ras->getLy();
  double sc = sx < sy ? sx : sy;

  TAffine aff = TScale(sc).place(ras->getCenterD(), icon->getCenterD());
  TRop::resample(icon, ras, aff, TRop::Bilinear);
  TRop::addBackground(icon, TPixel32::White);

  return icon;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TMeshImageP mi, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  if (!mi) return TRaster32P();

  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();

  // The image and contained within Imagebox
  // (add a small margin also to prevent problems with empty images)
  TRectD imageBox;
  imageBox = mi->getBBox().enlarge(.1);

  TPointD imageCenter(0.5 * (imageBox.getP00() + imageBox.getP11()));

  // Calculate a transformation matrix that moves the image inside the icon.
  // The scale factor is chosen so that the image is entirely
  // contained in the icon (with a margin of 'margin' pixels)
  const int margin = 10;
  double scx       = (iconSize.lx - margin) / imageBox.getLx();
  double scy       = (iconSize.ly - margin) / imageBox.getLy();
  double sc        = std::min(scx, scy);

  // Add the translation: the center point of the image is at the point
  // middle of the pixmap.
  TPointD iconCenter(iconSize.lx * 0.5, iconSize.ly * 0.5);
  TAffine aff = TScale(sc).place(imageCenter, iconCenter);

  // Draw the image.
  glContext->makeCurrent();
  glContext->clear(settings.m_blackBgCheck ? TPixel::Black : TPixel32::White);

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);

  glPushMatrix();
  tglMultMatrix(aff);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4f(0.0f, 1.0f, 0.0f, 0.7f);
  tglDrawEdges(*mi);

  glPopMatrix();

  glPopAttrib();

  TRaster32P ras(iconSize);
  glContext->getRaster(ras);

  glContext->doneCurrent();

  return ras;
}

//-------------------------------------------------------------------------

TRaster32P convertToIcon(TImageP image, int frame, const TDimension &iconSize,
                         const IconGenerator::Settings &settings) {
  TRasterImageP ri(image);
  if (ri) return convertToIcon(ri, iconSize);

  TToonzImageP ti(image);
  if (ti) return convertToIcon(ti, frame, iconSize, settings);

  TVectorImageP vi(image);
  if (vi) return convertToIcon(vi, frame, iconSize, settings);

  TMeshImageP mi(image);
  if (mi) return convertToIcon(mi, frame, iconSize, settings);

  return TRaster32P();
}

}  // namespace

//=============================================================================

//============================
//
//    IconRenderer class
//
//----------------------------

class IconRenderer : public TThread::Runnable {
  TRaster32P m_icon;
  TDimension m_iconSize;
  std::string m_id;

  bool m_started;
  bool m_terminated;

public:
  IconRenderer(const std::string &id, const TDimension &iconSize);
  virtual ~IconRenderer();

  void run() override = 0;

  void setIcon(const TRaster32P &icon) { m_icon = icon; }
  TRaster32P getIcon() const { return m_icon; }

  TDimension getIconSize() { return m_iconSize; }
  const std::string &getId() const { return m_id; }

  bool &hasStarted() { return m_started; }
  bool &wasTerminated() { return m_terminated; }
};

//-----------------------------------------------------------------------------

IconRenderer::IconRenderer(const std::string &id, const TDimension &iconSize)
    : m_icon()
    , m_iconSize(iconSize)
    , m_id(id)
    , m_started(false)
    , m_terminated(false) {
  connect(this, SIGNAL(started(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onStarted(TThread::RunnableP)));
  connect(this, SIGNAL(finished(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onFinished(TThread::RunnableP)));
  connect(this, SIGNAL(canceled(TThread::RunnableP)), IconGenerator::instance(),
          SLOT(onCanceled(TThread::RunnableP)), Qt::QueuedConnection);
  connect(this, SIGNAL(terminated(TThread::RunnableP)),
          IconGenerator::instance(), SLOT(onTerminated(TThread::RunnableP)),
          Qt::QueuedConnection);
}

//-----------------------------------------------------------------------------

IconRenderer::~IconRenderer() {}

//=============================================================================

//===================================
//
//    Specific icon renderers
//
//-----------------------------------

//=============================================================================

//======================================
//    VectorImageIconRenderer class
//--------------------------------------

class VectorImageIconRenderer final : public IconRenderer {
  TVectorImageP m_vimage;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  VectorImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TVectorImageP vimage,
                          const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_vimage(vimage)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P VectorImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TVectorImageP vimage;

  int frame = 0;
  if (!m_vimage) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();
    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();
    vimage = (TVectorImageP)image;
    if (!vimage) return TRaster32P();
    frame = m_sl->guessIndex(m_fid);
  } else
    vimage = m_vimage;
  assert(vimage);

  TRaster32P ras(convertToIcon(vimage, frame, iconSize, m_settings));

  return ras;
}

//-----------------------------------------------------------------------------

void VectorImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras) setIcon(ras);
  } catch (...) {
  }
}

//=============================================================================

//======================================
//    SplineImageIconRenderer class
//--------------------------------------

class SplineIconRenderer final : public IconRenderer {
  TStageObjectSpline *m_spline;

public:
  SplineIconRenderer(const std::string &id, const TDimension &iconSize,
                     TStageObjectSpline *spline)
      : IconRenderer(id, iconSize), m_spline(spline) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P SplineIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  // get the glContext
  TOfflineGL *glContext = IconGenerator::instance()->getOfflineGLContext();
  glContext->makeCurrent();
  glContext->clear(TPixel32::White);

  const TStroke *stroke = m_spline->getStroke();
  assert(stroke);
  if (!stroke) {
    glContext->doneCurrent();
    return TRaster32P();
  }
  TRectD sbbox = stroke->getBBox();

  glColor3d(0, 0, 0);
  double scaleX = 1, scaleY = 1;
  if (sbbox.getLx() > 0.0) scaleX = (double)iconSize.lx / sbbox.getLx();
  if (sbbox.getLy() > 0.0) scaleY = (double)iconSize.ly / sbbox.getLy();
  double scale                    = 0.8 * std::min(scaleX, scaleY);
  TPointD centerStroke            = 0.5 * (sbbox.getP00() + sbbox.getP11());
  TPointD centerPixmap(iconSize.lx * 0.5, iconSize.ly * 0.5);
  glPushMatrix();
  tglMultMatrix(TScale(scale).place(centerStroke, centerPixmap));
  int n = 50;
  glBegin(GL_LINE_STRIP);
  for (int i = 0; i < n; i++)
    tglVertex(stroke->getPoint((double)i / (double)(n - 1)));
  glEnd();
  glPopMatrix();

  TRaster32P ras(iconSize);
  glContext->getRaster(ras);
  glContext->doneCurrent();
  return ras;
}

//-----------------------------------------------------------------------------

void SplineIconRenderer::run() {
  TRaster32P raster = generateRaster(getIconSize());
  if (raster) setIcon(raster);
}

//=============================================================================

//======================================
//    RasterImageIconRenderer class
//--------------------------------------

class RasterImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;

public:
  RasterImageIconRenderer(const std::string &id, const TDimension &iconSize,
                          TXshSimpleLevelP sl, const TFrameId &fid)
      : IconRenderer(id, iconSize), m_sl(sl), m_fid(fid) {}

  void run() override;
};

//-----------------------------------------------------------------------------

void RasterImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage = (TRasterImageP)image;
  assert(rimage);

  TRaster32P icon(convertToIcon(rimage, getIconSize()));

  if (icon) setIcon(icon);
}

//=============================================================================

//======================================
//    ToonzImageIconRenderer class
//--------------------------------------

class ToonzImageIconRenderer final : public IconRenderer {
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

  TRasterCM32P m_tnzImgIcon;

public:
  ToonzImageIconRenderer(const std::string &id, const TDimension &iconSize,
                         TXshSimpleLevelP sl, const TFrameId &fid,
                         const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings)
      , m_tnzImgIcon(0) {}

  void run() override;

  void setIcon_TnzImg(const TRasterCM32P &timgp) { m_tnzImgIcon = timgp; }
  TRasterCM32P getIcon_TnzImg() const { return m_tnzImgIcon; }
};

//-----------------------------------------------------------------------------

void ToonzImageIconRenderer::run() {
  if (!m_sl->isFid(m_fid)) return;

  TImageP image = m_sl->getFrameIcon(m_fid);
  if (!image) return;

  TRasterImageP rimage(image);
  if (rimage) {
    TRaster32P icon(convertToIcon(rimage, getIconSize()));
    if (icon) setIcon(icon);

    return;
  }

  TToonzImageP timage = (TToonzImageP)image;

  TDimension iconSize(getIconSize());
  if (!timage) {
    TRaster32P p(iconSize.lx, iconSize.ly);
    p->fill(TPixelRGBM32::Yellow);
    setIcon(p);

    return;
  }

  TRasterCM32P rasCM32 = timage->getRaster();
  if (!rasCM32.getPointer()) return;

  int lx     = rasCM32->getSize().lx;
  int ly     = rasCM32->getSize().ly;
  int iconLx = iconSize.lx, iconLy = iconSize.ly;

  TRaster32P icon(iconSize);

  icon->fill(m_settings.m_blackBgCheck ? TPixel::Black : TPixel::White);

  if (lx != iconLx && ly != iconLy) {
    // The icons stored in the tlv file don't have the required size.
    // Fetch the original and iconize it.

    image = m_sl->getFrame(m_fid, ImageManager::dontPutInCache,
                           0);  // 0 uses the level properties' subsampling
    if (!image) return;

    timage = (TToonzImageP)image;
    if (!timage) {
      TRaster32P p(iconSize.lx, iconSize.ly);
      p->fill(TPixelRGBM32::Yellow);
      setIcon(p);

      return;
    }

    rasCM32 = timage->getRaster();
    if (!rasCM32.getPointer()) return;

    TRasterCM32P auxCM32(icon->getSize());
    auxCM32->clear();

    TRop::makeIcon(auxCM32, rasCM32);
    rasCM32 = auxCM32;
  }

  if (!m_sl->getPalette()) return;

  TPaletteP plt = m_sl->getPalette()->clone();
  if (!plt) return;

  int frame = m_sl->guessIndex(m_fid);
  plt->setFrame(frame);

  setIcon_TnzImg(rasCM32);
}

//=============================================================================

//======================================
//    MeshImageIconRenderer class
//--------------------------------------

class MeshImageIconRenderer final : public IconRenderer {
  TMeshImageP m_image;
  TXshSimpleLevelP m_sl;
  TFrameId m_fid;
  IconGenerator::Settings m_settings;

public:
  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TXshSimpleLevelP sl, const TFrameId &fid,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image()
      , m_sl(sl)
      , m_fid(fid)
      , m_settings(settings) {}

  MeshImageIconRenderer(const std::string &id, const TDimension &iconSize,
                        TMeshImageP image,
                        const IconGenerator::Settings &settings)
      : IconRenderer(id, iconSize)
      , m_image(image)
      , m_sl(0)
      , m_fid(-1)
      , m_settings(settings) {}

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P MeshImageIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  TMeshImageP mi;

  int frame = 0;
  if (!m_image) {
    assert(m_sl);
    if (!m_sl->isFid(m_fid)) return TRaster32P();

    TImageP image = m_sl->getFrameIcon(m_fid);
    if (!image) return TRaster32P();

    mi = (TMeshImageP)image;
    if (!mi) return TRaster32P();

    frame = m_sl->guessIndex(m_fid);
  } else
    mi = m_image;

  assert(mi);

  return convertToIcon(mi, frame, iconSize, m_settings);
}

//-----------------------------------------------------------------------------

void MeshImageIconRenderer::run() {
  try {
    TRaster32P ras(generateRaster(getIconSize()));

    if (ras) setIcon(ras);
  } catch (...) {
  }
}

//=============================================================================

//==================================
//    XsheetIconRenderer class
//----------------------------------

class XsheetIconRenderer final : public IconRenderer {
  TXsheet *m_xsheet;
  int m_row;

public:
  XsheetIconRenderer(const std::string &id, const TDimension &iconSize,
                     TXsheet *xsheet, int row = 0)
      : IconRenderer(id, iconSize), m_xsheet(xsheet), m_row(row) {
    if (m_xsheet) {
      assert(m_xsheet->getRefCount() > 0);
      m_xsheet->addRef();
    }
  }

  ~XsheetIconRenderer() {
    if (m_xsheet) m_xsheet->release();
  }

  static std::string getId(TXshChildLevel *level, int row) {
    return "sub:" + ::to_string(level->getName()) + std::to_string(row);
  }

  TRaster32P generateRaster(const TDimension &iconSize) const;
  void run() override;
};

//-----------------------------------------------------------------------------

TRaster32P XsheetIconRenderer::generateRaster(
    const TDimension &iconSize) const {
  ToonzScene *scene = m_xsheet->getScene();

  TRaster32P ras(iconSize);

  TPixel32 bgColor = scene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  TImageCache::instance()->setEnabled(false);

  // All checks are disabled
  scene->renderFrame(ras, m_row, m_xsheet, false);

  TImageCache::instance()->setEnabled(true);

  return ras;
}

//-----------------------------------------------------------------------------

void XsheetIconRenderer::run() {
  TRaster32P ras = generateRaster(getIconSize());
  if (ras) setIcon(ras);
}

//=============================================================================

//================================
//    FileIconRenderer class
//--------------------------------

class FileIconRenderer final : public IconRenderer {
  TFilePath m_path;
  TFrameId m_fid;

public:
  FileIconRenderer(const TDimension &iconSize, const TFilePath &path,
                   const TFrameId &fid)
      : IconRenderer(getId(path, fid), iconSize), m_path(path), m_fid(fid) {}

  static std::string getId(const TFilePath &path, const TFrameId &fid);

  void run() override;
};

//-----------------------------------------------------------------------------

std::string FileIconRenderer::getId(const TFilePath &path,
                                    const TFrameId &fid) {
  std::string type(path.getType());

  if (type == "tab" || type == "tnz" ||
      type == "mesh" ||  // meshes are not currently viewable
      TFileType::isViewable(TFileType::getInfo(path))) {
    std::string fidNumber;
    if (fid != TFrameId::NO_FRAME)
      fidNumber = "frame:" + fid.expand(TFrameId::NO_PAD);
    return "$:" + ::to_string(path) + fidNumber;
  }

  // All the other types whose icon is the same for file type, get the same id
  // per type.
  else if (type == "tpl")
    return "$:tpl";
  else if (type == "tzp")
    return "$:tzp";
  else if (type == "svg")
    return "$:svg";
  else if (type == "tzu")
    return "$:tzu";
  else if (TFileType::getInfo(path) == TFileType::AUDIO_LEVEL)
    return "$:audio";
  else if (type == "scr")
    return "$:scr";
  else if (type == "mpath")
    return "$:mpath";
  else if (type == "curve")
    return "$:curve";
  else if (type == "cln")
    return "$:cln";
  else if (type == "tnzbat")
    return "$:tnzbat";
  else
    return "$:unknown";
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateVectorFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid) {
  TLevelReaderP lr(path);
  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();
  TFrameId frameId                       = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;
  TImageP img                            = lr->getFrameReader(frameId)->load();
  TVectorImageP vi                       = img;
  if (!vi) return TRaster32P();
  vi->setPalette(level->getPalette());
  VectorImageIconRenderer vir("", iconSize, vi.getPointer(),
                              IconGenerator::Settings());
  return vir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateRasterFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize,
                                                 const TFrameId &fid) {
  TImageP img;

  try {
    // Attempt image reading
    TLevelReaderP lr(path);
    TLevelP level = lr->loadInfo();

    if (level->begin() == level->end()) return TRaster32P();

    TFrameId frameId = fid;
    if (fid == TFrameId::NO_FRAME)  // In case no frame was specified, pick the
      frameId = level->begin()->first;  // first level frame

    TImageReaderP ir = lr->getFrameReader(frameId);

    if (const TImageInfo *ii = ir->getImageInfo()) {
      int shrinkX = ii->m_lx / iconSize.lx;
      int shrinkY = ii->m_ly / iconSize.ly;
      int shrink  = shrinkX < shrinkY ? shrinkX : shrinkY;

      if (shrink > 1) ir->setShrink(shrink);
    }

    img = (toUpper(path.getType()) == "TLV") ? ir->loadIcon() : ir->load();
  } catch (...) {
  }

  // Extract a 32-bit fullcolor raster from img
  TRaster32P ras32;

  if (TRasterImageP ri = img) {
    ras32 = ri->getRaster();

    if (!ras32) {
      if (TRasterGR8P rasGR8 = ri->getRaster()) {
        TRaster32P raux(rasGR8->getSize());
        TRop::convert(raux, rasGR8);
        ras32 = raux;
      }
    }
  } else if (TToonzImageP ti = img) {
    TRasterCM32P auxRaster = ti->getRaster();
    TRaster32P dstRaster(auxRaster->getSize());

    if (TPaletteP plt = ti->getPalette())
      TRop::convert(dstRaster, auxRaster, plt, false);
    else
      dstRaster->fill(TPixel32::Magenta);

    ras32 = dstRaster;
  }

  if (!ras32) return TRaster32P();

  /*
// NOTE: The following was possible with the old Qt version 4.3.3 - but in the
new 4.5.0
// it's not: 'It is not safe to use QPixmaps outside the GUI thread'...
TRaster32P icon;
{
QPixmap p(rasterToQPixmap(ras32));
icon = rasterFromQPixmap(
  scalePixmapKeepingAspectRatio(p, QSize(iconSize.lx, iconSize.ly),
Qt::transparent)
  , false);
}
*/

  TRaster32P icon(iconSize);

  double sx = double(iconSize.lx) / ras32->getLx();
  double sy = double(iconSize.ly) / ras32->getLy();
  double sc = std::min(sx, sy);  // show all the image, possibly adding bands

  TAffine aff = TScale(sc).place(ras32->getCenterD(), icon->getCenterD());

  icon->fill(TPixel32(160, 160, 160));  // "bands" color
  TRop::resample(icon, ras32, aff, TRop::Triangle);

  if (icon) {
    if (::isUnpremultiplied(icon))  // APPALLING. I'm not touching this, but
      TRop::premultiply(
          icon);  // YOU JUST CAN'T TELL IF AN IMAGE IS PREMULTIPLIED
                  // OR NOT BY SCANNING ITS PIXELS.
                  // You either know it FOR A GIVEN, or you don't...      >_<
    TRectI srcBBoxI = ras32->getBounds();
    TRectD srcBBoxD = aff * TRectD(srcBBoxI.x0, srcBBoxI.y0, srcBBoxI.x1 + 1,
                                   srcBBoxI.y1 + 1);

    TRect bbox = TRect(tfloor(srcBBoxD.x0), tceil(srcBBoxD.y0) - 1,
                       tfloor(srcBBoxD.x1), tceil(srcBBoxD.y1) - 1);

    bbox = (bbox * icon->getBounds())
               .enlarge(-1);  // Add a 1 pixel transparent margin - this
    if (bbox.getLx() > 0 &&
        bbox.getLy() > 0)  // way the actual content doesn't look trimmed.
      ::makeChessBackground(icon->extract(bbox));
  } else
    icon->fill(TPixel32(255, 0, 0));

  return icon;
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSplineFileIcon(const TFilePath &path,
                                                 const TDimension &iconSize) {
  TStageObjectSpline *spline = new TStageObjectSpline();
  TIStream is(path);
  spline->loadData(is);
  SplineIconRenderer sr("", iconSize, spline);
  TRaster32P icon = sr.generateRaster(iconSize);
  delete spline;
  return icon;
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateMeshFileIcon(const TFilePath &path,
                                               const TDimension &iconSize,
                                               const TFrameId &fid) {
  TLevelReaderP lr(path);
  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return TRaster32P();

  TFrameId frameId                       = fid;
  if (fid == TFrameId::NO_FRAME) frameId = level->begin()->first;

  TMeshImageP mi = lr->getFrameReader(frameId)->load();
  if (!mi) return TRaster32P();

  MeshImageIconRenderer mir("", iconSize, mi.getPointer(),
                            IconGenerator::Settings());
  return mir.generateRaster(iconSize);
}

//-----------------------------------------------------------------------------

TRaster32P IconGenerator::generateSceneFileIcon(const TFilePath &path,
                                                const TDimension &iconSize,
                                                int row) {
  if (row == 0 || row == TFrameId::NO_FRAME - 1) {
    TFilePath iconPath =
        path.getParentDir() + "sceneIcons" + (path.getWideName() + L" .png");
    return generateRasterFileIcon(iconPath, iconSize, TFrameId::NO_FRAME);
  } else {
    // obsolete
    ToonzScene scene;
    scene.load(path);
    XsheetIconRenderer ir("", iconSize, scene.getXsheet(), row);
    return ir.generateRaster(iconSize);
  }
}

//-----------------------------------------------------------------------------

void FileIconRenderer::run() {
  TDimension iconSize(getIconSize());

  try {
    TRaster32P iconRaster;
    std::string type(m_path.getType());

    if (type == "tnz" || type == "tab")
      iconRaster = IconGenerator::generateSceneFileIcon(m_path, iconSize,
                                                        m_fid.getNumber() - 1);
    else if (type == "pli")
      iconRaster =
          IconGenerator::generateVectorFileIcon(m_path, iconSize, m_fid);
    else if (type == "tpl") {
      QImage palette(":Resources/paletteicon.svg");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (type == "tzp") {
      QImage palette(":Resources/tzpicon.png");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (type == "svg") {
      QPixmap palette(svgToPixmap(":Resources/svg.svg",
                                  QSize(iconSize.lx, iconSize.ly),
                                  Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(palette));
      return;
    } else if (type == "tzu") {
      QImage palette(":Resources/tzuicon.png");
      setIcon(rasterFromQImage(palette));
      return;
    } else if (TFileType::getInfo(m_path) == TFileType::AUDIO_LEVEL) {
      QPixmap loudspeaker(svgToPixmap(":Resources/audio.svg",
                                      QSize(iconSize.lx, iconSize.ly),
                                      Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(loudspeaker));
      return;
    } else if (type == "scr") {
      QImage screensaver(":Resources/savescreen.png");
      setIcon(rasterFromQImage(screensaver));
      return;
    } else if (type == "psd") {
      QPixmap psdPath(svgToPixmap(":Resources/psd.svg",
                                  QSize(iconSize.lx, iconSize.ly),
                                  Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(psdPath));
      return;
    } else if (type == "mesh")
      iconRaster = IconGenerator::generateMeshFileIcon(m_path, iconSize, m_fid);
    else if (TFileType::isViewable(TFileType::getInfo(m_path)) || type == "tlv")
      iconRaster =
          IconGenerator::generateRasterFileIcon(m_path, iconSize, m_fid);
    else if (type == "mpath") {
      QPixmap motionPath(svgToPixmap(":Resources/motionpath_fileicon.svg",
                                     QSize(iconSize.lx, iconSize.ly),
                                     Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(motionPath));
      return;
    } else if (type == "curve") {
      QPixmap motionPath(svgToPixmap(":Resources/curve.svg",
                                     QSize(iconSize.lx, iconSize.ly),
                                     Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(motionPath));
      return;
    } else if (type == "cln") {
      QPixmap motionPath(svgToPixmap(":Resources/cleanup.svg",
                                     QSize(iconSize.lx, iconSize.ly),
                                     Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(motionPath));
      return;
    } else if (type == "tnzbat") {
      QPixmap motionPath(svgToPixmap(":Resources/tasklist.svg",
                                     QSize(iconSize.lx, iconSize.ly),
                                     Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(motionPath));
      return;
    } else if (type == "tls") {
      QPixmap magpie(svgToPixmap(":Resources/magpie.svg",
                                 QSize(iconSize.lx, iconSize.ly),
                                 Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(magpie));
      return;
    } else if (type == "js") {
      QImage script(":Resources/scripticon.png");
      setIcon(rasterFromQImage(script));
      return;
    }

    else {
      QPixmap unknown(svgToPixmap(":Resources/unknown.svg",
                                  QSize(iconSize.lx, iconSize.ly),
                                  Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(unknown));
      return;
    }
    if (!iconRaster) {
      QPixmap broken(svgToPixmap(":Resources/broken.svg",
                                 QSize(iconSize.lx, iconSize.ly),
                                 Qt::KeepAspectRatio));
      setIcon(rasterFromQPixmap(broken));
      return;
    }
    setIcon(iconRaster);
  } catch (const TImageVersionException &) {
    QPixmap unknown(svgToPixmap(":Resources/unknown.svg",
                                QSize(iconSize.lx, iconSize.ly),
                                Qt::KeepAspectRatio));
    setIcon(rasterFromQPixmap(unknown));
  } catch (...) {
    QPixmap broken(svgToPixmap(":Resources/broken.svg",
                               QSize(iconSize.lx, iconSize.ly),
                               Qt::KeepAspectRatio));
    setIcon(rasterFromQPixmap(broken));
  }
}

//=============================================================================

//================================
//    SceneIconRenderer class
//--------------------------------

class SceneIconRenderer final : public IconRenderer {
  ToonzScene *m_toonzScene;

public:
  SceneIconRenderer(const TDimension &iconSize, ToonzScene *scene)
      : IconRenderer(getId(), iconSize), m_toonzScene(scene) {}

  static std::string getId() { return "currentScene"; }

  void run() override;
  TRaster32P generateIcon(const TDimension &iconSize) const;
};

//-----------------------------------------------------------------------------

TRaster32P SceneIconRenderer::generateIcon(const TDimension &iconSize) const {
  TRaster32P ras(iconSize);

  TPixel32 bgColor = m_toonzScene->getProperties()->getBgColor();
  bgColor.m        = 255;
  ras->fill(bgColor);

  m_toonzScene->renderFrame(ras, 0, 0, false);

  return ras;
}

//-----------------------------------------------------------------------------

void SceneIconRenderer::run() { setIcon(generateIcon(getIconSize())); }

//=============================================================================

//===================================
//
//    IconGenerator class
//
//-----------------------------------

IconGenerator::IconGenerator() : m_iconSize(FilmstripIconSize) {
  m_executor.setMaxActiveTasks(1);  // Only one thread to render icons...
  m_executor.setDedicatedThreads(true);
}

//-----------------------------------------------------------------------------

IconGenerator::~IconGenerator() {}

//-----------------------------------------------------------------------------

IconGenerator *IconGenerator::instance() {
  static IconGenerator _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void IconGenerator::setFilmstripIconSize(const TDimension &dim) {
  FilmstripIconSize = dim;
}

//-----------------------------------------------------------------------------

TDimension IconGenerator::getIconSize() const { return FilmstripIconSize; }

//-----------------------------------------------------------------------------

TOfflineGL *IconGenerator::getOfflineGLContext() {
  // One context per rendering thread
  if (!m_contexts.hasLocalData()) {
    TDimension contextSize(std::max(FilmstripIconSize.lx, IconSize.lx),
                           std::max(FilmstripIconSize.ly, IconSize.ly));
    m_contexts.setLocalData(new TOfflineGL(contextSize));
  }
  return m_contexts.localData();
}

//-----------------------------------------------------------------------------

void IconGenerator::addTask(const std::string &id,
                            TThread::RunnableP iconRenderer) {
  iconsMap.insert(id);
  m_executor.addTask(iconRenderer);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TXshLevel *xl, const TFrameId &fid,
                               bool filmStrip, bool onDemand) {
  if (!xl) return QPixmap();

  if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (filmStrip) return QPixmap();

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    QPixmap pix;
    if (::getIcon(id, pix)) return pix;

    if (onDemand) return pix;

    TDimension iconSize = TDimension(80, 60);

    // The icon must be calculated - add an IconRenderer task.
    // storeIcon(id, QPixmap());   //It was automatically added by the former
    // access
    addTask(id, new XsheetIconRenderer(id, iconSize, cl->getXsheet()));
  }

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    // make thumbnails for cleanup preview and cameratest to be the same as
    // normal TLV
    std::string id;
    int status = sl->getFrameStatus(fid);
    if (sl->getType() == TZP_XSHLEVEL &&
        status & TXshSimpleLevel::CleanupPreview) {
      sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      id = sl->getIconId(fid);
      sl->setFrameStatus(fid, status);
    } else
      id = sl->getIconId(fid);

    if (!filmStrip) id += "_small";

    QPixmap pix;
    if (::getIcon(id, pix, xl->getSimpleLevel())) return pix;

    if (onDemand) return pix;

    IconGenerator::Settings oldSettings = m_settings;

    // Disable transparency check for cast and xsheet icons
    if (!filmStrip) m_settings = IconGenerator::Settings();

    TDimension iconSize = filmStrip ? m_iconSize : TDimension(80, 60);

    // storeIcon(id, QPixmap());

    int type = sl->getType();
    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id,
              new VectorImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case TZP_XSHLEVEL:
      // Yep, we could have rasters, due to a cleanupping process
      if (status == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, iconSize, sl, fid));
      else
        addTask(id,
                new ToonzImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, iconSize, sl, fid, m_settings));
      break;
    default:
      assert(false);
      break;
    }

    m_settings = oldSettings;
  }

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TXshLevel *xl, const TFrameId &fid,
                               bool onlyFilmStrip) {
  if (!xl) return;

  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id = sl->getIconId(fid);

    int type = sl->getType();

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      break;
    case PLI_XSHLEVEL:
      removeIcon(id);
      addTask(id, new VectorImageIconRenderer(id, getIconSize(), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id, new RasterImageIconRenderer(id, getIconSize(), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, getIconSize(), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, getIconSize(), sl, fid,
                                            m_settings));
      break;
    default:
      assert(false);
      break;
    }

    if (onlyFilmStrip) return;

    id += "_small";
    if (iconsMap.find(id) == iconsMap.end()) return;

    // Not-filmstrip icons diable all checks
    IconGenerator::Settings oldSettings = m_settings;
    m_settings.m_transparencyCheck      = false;
    m_settings.m_inkIndex               = -1;
    m_settings.m_paintIndex             = -1;
    m_settings.m_blackBgCheck           = false;

    switch (type) {
    case OVL_XSHLEVEL:
    case TZI_XSHLEVEL:
      addTask(id, new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      break;
    case PLI_XSHLEVEL:
      addTask(id, new VectorImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                              m_settings));
      break;
    case TZP_XSHLEVEL:
      if (sl->getFrameStatus(fid) == TXshSimpleLevel::Scanned)
        addTask(id,
                new RasterImageIconRenderer(id, TDimension(80, 60), sl, fid));
      else
        addTask(id, new ToonzImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                               m_settings));
      break;
    case MESH_XSHLEVEL:
      addTask(id, new MeshImageIconRenderer(id, TDimension(80, 60), sl, fid,
                                            m_settings));
      break;
    default:
      assert(false);
      break;
    }

    m_settings = oldSettings;
  } else if (TXshChildLevel *cl = xl->getChildLevel()) {
    if (onlyFilmStrip) return;

    std::string id = XsheetIconRenderer::getId(cl, fid.getNumber() - 1);
    removeIcon(id);

    getIcon(xl, fid);
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TXshLevel *xl, const TFrameId &fid,
                           bool onlyFilmStrip) {
  if (!xl) return;
  if (TXshSimpleLevel *sl = xl->getSimpleLevel()) {
    std::string id(sl->getIconId(fid));

    removeIcon(id);
    if (!onlyFilmStrip) removeIcon(id + "_small");
  } else {
    TXshChildLevel *cl = xl->getChildLevel();
    if (cl && !onlyFilmStrip)
      removeIcon(XsheetIconRenderer::getId(cl, fid.getNumber() - 1));
  }
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(TStageObjectSpline *spline) {
  if (!spline) return QPixmap();
  std::string iconName = spline->getIconId();

  QPixmap pix;
  if (::getIcon(iconName, pix)) return pix;

  // storeIcon(id, QPixmap());
  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);

  addTask(iconName, new SplineIconRenderer(iconName, getIconSize(), spline));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(TStageObjectSpline *spline) {
  if (!spline) return;
  std::string iconName = spline->getIconId();
  removeIcon(iconName);
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getIcon(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);

  QPixmap pix;
  TDimension fileIconSize(80, 60);
  // Here the fileIconSize is input in order to check if the icon is obtained
  // with high-dpi (i.e. devPixRatio > 1.0).
  if (::getIcon(id, pix, 0, fileIconSize)) return pix;

  addTask(id, new FileIconRenderer(fileIconSize, path, fid));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidate(const TFilePath &path, const TFrameId &fid) {
  std::string id = FileIconRenderer::getId(path, fid);
  removeIcon(id);
  addTask(id, new FileIconRenderer(TDimension(80, 60), path, fid));
}

//-----------------------------------------------------------------------------

void IconGenerator::remove(const TFilePath &path, const TFrameId &fid) {
  removeIcon(FileIconRenderer::getId(path, fid));
}

//-----------------------------------------------------------------------------

QPixmap IconGenerator::getSceneIcon(ToonzScene *scene) {
  std::string id(SceneIconRenderer::getId());

  QPixmap pix;
  if (::getIcon(id, pix)) return pix;

  // storeIcon(id, QPixmap());
  addTask(id, new SceneIconRenderer(getIconSize(), scene));

  return QPixmap();
}

//-----------------------------------------------------------------------------

void IconGenerator::invalidateSceneIcon() {
  removeIcon(SceneIconRenderer::getId());
}

//-----------------------------------------------------------------------------

void IconGenerator::remap(const std::string &newIconId,
                          const std::string &oldIconId) {
  IconIterator it = iconsMap.find(oldIconId);
  if (it == iconsMap.end()) return;

  iconsMap.erase(it);
  iconsMap.insert(newIconId);

  TImageCache::instance()->remap(newIconId, oldIconId);
}

//-----------------------------------------------------------------------------

void IconGenerator::clearRequests() { m_executor.cancelAll(); }

//-----------------------------------------------------------------------------

void IconGenerator::clearSceneIcons() {
  // Eliminate all icons whose prefix is not "$:" (that is, scene-independent
  // images).
  // The abovementioned prefix is internally recognized by the image cache when
  // the scene
  // changes to avoid clearing file browser's icons.

  // Observe that image cache's clear function invoked during scene changes is
  // called through
  // the ImageManager::clear() method, including FilmStrip icons.

  // note the ';' - which follows ':' in the ascii table
  iconsMap.erase(iconsMap.begin(), iconsMap.lower_bound("$:"));
  iconsMap.erase(iconsMap.lower_bound("$;"), iconsMap.end());
}

//-----------------------------------------------------------------------------

void IconGenerator::onStarted(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  ir->hasStarted() = true;
}

//-----------------------------------------------------------------------------

void IconGenerator::onCanceled(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  if (!ir->hasStarted()) {
    removeIcon(ir->getId());
  }
}

//-----------------------------------------------------------------------------

void IconGenerator::onFinished(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  // if the icon was generated in TToonzImage format, cache it instead
  ToonzImageIconRenderer *tir = dynamic_cast<ToonzImageIconRenderer *>(ir);
  if (tir) {
    TRasterCM32P timgp = tir->getIcon_TnzImg();
    if (timgp) {
      ::setIcon_TnzImg(ir->getId(), timgp);
      emit iconGenerated();
      if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
      return;
    }
  }

  // Update the icons map
  if (ir->getIcon()) {
    ::setIcon(ir->getId(), ir->getIcon());
    emit iconGenerated();
  }

  if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
}

//-----------------------------------------------------------------------------

void IconGenerator::onException(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  if (ir->wasTerminated()) m_iconsTerminationLoop.quit();
}

//-----------------------------------------------------------------------------

void IconGenerator::onTerminated(TThread::RunnableP iconRenderer) {
  IconRenderer *ir = static_cast<IconRenderer *>(iconRenderer.getPointer());

  ir->wasTerminated() = true;
  m_iconsTerminationLoop.exec();
}
