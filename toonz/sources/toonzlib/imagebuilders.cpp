

// TnzCore includes
#include "tcommon.h"

#include "timageinfo.h"
#include "timage_io.h"
#include "tlevel_io.h"

#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "trastercm.h"
#include "tvectorrenderdata.h"
#include "tpalette.h"

#include "tgl.h"
#include "tvectorgl.h"
#include "tofflinegl.h"

#include "timagecache.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/fill.h"
#include "toonz/dpiscale.h"
#include "toonz/stage.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"

// Qt includes
#include <QImage>
#include <QThread>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>

#include "imagebuilders.h"

//***************************************************************************************
//    Global stuff
//***************************************************************************************

extern TOfflineGL *currentOfflineGL;

//***************************************************************************************
//    ImageLoader  implementation
//***************************************************************************************

ImageLoader::ImageLoader(const TFilePath &path, const TFrameId &fid)
    : m_path(path)
    , m_fid(fid)
    , m_subsampling(0)
    , m_64bitCompatible(false)
    , m_colorSpaceGamma(LevelOptions::DefaultColorSpaceGamma) {}

//-------------------------------------------------------------------------

bool ImageLoader::getInfo(TImageInfo &info, int imFlags, void *extData) {
  try {
    TLevelReaderP lr(m_path);
    TImageReaderP fr = lr->getFrameReader(m_fid);

    // NOTE: Currently not changing imageInfo's bpp stuff...
    //       Ignoring subsampling too...

    return ImageBuilder::setImageInfo(info, fr.getPointer());
  } catch (TException &e) {
    QString msg = QString::fromStdWString(e.getMessage());
    if (msg == QString("Old 4.1 Palette")) throw;

    return false;
  }
}

//-------------------------------------------------------------------------

inline int ImageLoader::buildSubsampling(int imFlags, BuildExtData *data) {
  return (imFlags & ImageManager::toBeModified) ? 1
         : (data->m_subs > 0)                   ? data->m_subs
         : (m_subsampling > 0)                  ? m_subsampling
                               : data->m_sl->getProperties()->getSubsampling();
}

//-------------------------------------------------------------------------

TImageP ImageLoader::build(int imFlags, void *extData) {
  assert(extData);

  // Extract external data
  BuildExtData *data = static_cast<BuildExtData *>(extData);

  int subsampling = buildSubsampling(imFlags, data);

  try {
    // Initialize level reader
    TLevelReaderP lr(m_path);
    if (!lr) return TImageP();

    // Load info in cases where it's required first
    lr->doReadPalette(false);

    if ((m_path.getType() == "pli") || (m_path.getType() == "svg") ||
        (m_path.getType() == "psd") || (m_path.getType() == "tzm"))
      lr->loadInfo();

    bool isTlvIcon = data->m_icon && m_path.getType() == "tlv";

    // for TLV icons, palettes will be applied in IconGenerator later
    if (!isTlvIcon) lr->doReadPalette(true);  // Allow palette loading

    TImageReaderP ir = lr->getFrameReader(m_fid);

    bool enable64bit = (imFlags & ImageManager::is64bitEnabled);
    ir->enable16BitRead(enable64bit);  // Set 64-bit loading if required
    bool enableFloat = (imFlags & ImageManager::isFloatEnabled);
    ir->enableFloatRead(enableFloat);  // Set float loading if required

    double colorSpaceGamma = LevelOptions::DefaultColorSpaceGamma;
    if (m_path.getType() == "exr") {
      // gamma value to be used for converting linear-based image file to
      // nonlinear raster. Curretly only used in EXR image levels.
      colorSpaceGamma = data->m_sl->getProperties()->colorSpaceGamma();
      ir->setColorSpaceGamma(colorSpaceGamma);
    }

    // Load the image
    TImageP img;

    if (isTlvIcon)
      img = ir->loadIcon();  // TODO: Why just in the tlv case??
    else {
      ir->setShrink(subsampling);
      img = ir->load();
    }

    ir->enable16BitRead(false);
    ir->enableFloatRead(false);

    if (!img) return img;  // There was an error loading the image.

    TPalette *palette = data->m_sl->getPalette();
    if (palette) img->setPalette(palette);

    if (subsampling > 1) {
      // Store the subsampling info in the image
      if (TRasterImageP ri = img)
        ri->setSubsampling(subsampling);
      else if (TToonzImageP ti = img)
        ti->setSubsampling(subsampling);
    }

    // In case the image will be cached, store its subsampling and 64 bit
    // compatibility
    if (!(imFlags & ImageManager::dontPutInCache)) {
      m_subsampling = subsampling;
      m_64bitCompatible =
          data->m_sl->is16BitChannelLevel() ? enable64bit : true;
      m_floatCompatible =
          data->m_sl->isFloatChannelLevel() ? enableFloat : true;
      if (m_path.getType() == "exr") m_colorSpaceGamma = colorSpaceGamma;
    }

    return img;
  } catch (...) {
    return TImageP();
  }
}

//-------------------------------------------------------------------------

bool ImageLoader::isImageCompatible(int imFlags, void *extData) {
  assert(extData);

  BuildExtData *data        = static_cast<BuildExtData *>(extData);
  const TXshSimpleLevel *sl = data->m_sl;

  // NOTE: Vector and Mesh dont care about sub sampling rate and bit depth
  // compatibility.
  //       They are property of Raster.
  if (sl->getType() == PLI_XSHLEVEL || sl->getType() == MESH_XSHLEVEL)
    return true;

  int subsampling = buildSubsampling(imFlags, data);

  if (m_subsampling <= 0 || subsampling != m_subsampling) return false;

  if (m_path.getType() == "exr" &&
      !areAlmostEqual(m_colorSpaceGamma,
                      sl->getProperties()->colorSpaceGamma()))
    return false;

  if (!m_floatCompatible && (imFlags & ImageManager::isFloatEnabled))
    return false;
  else if (!m_64bitCompatible && (imFlags & ImageManager::is64bitEnabled))
    return false;
  else
    return true;

  // if (m_64bitCompatible || !(imFlags & ImageManager::is64bitEnabled)) {
  //   return true;
  // } else {
  //   return false;
  // }
}

//-------------------------------------------------------------------------

void ImageLoader::invalidate() {
  ImageBuilder::invalidate();
  m_subsampling     = 0;
  m_64bitCompatible = false;
  m_colorSpaceGamma = LevelOptions::DefaultColorSpaceGamma;
}

//-------------------------------------------------------------------------
/*--
 * Implement ImageBuilder virtual functions. All icons and images are stored in
 * the cache on Loading.
 * --*/

void ImageLoader::buildAllIconsAndPutInCache(TXshSimpleLevel *level,
                                             std::vector<TFrameId> fids,
                                             std::vector<std::string> iconIds,
                                             bool cacheImagesAsWell) {
  // if (m_path.getType() != "tlv") return;
  if (level->getType() != TZP_XSHLEVEL && level->getType() != OVL_XSHLEVEL)
    return;

  if (fids.empty() || iconIds.empty()) return;
  /*- The number of fid and icon id should be the same -*/
  if ((int)fids.size() != (int)iconIds.size()) return;

  try {
    TLevelReaderP lr(m_path);
    if (!lr) return;

    for (int i = 0; i < (int)fids.size(); i++) {
      lr->doReadPalette(false);
      TImageReaderP ir = lr->getFrameReader(fids[i]);
      lr->doReadPalette(true);

      TImageInfo info;
      TPalette *palette     = level->getPalette();
      std::string fullImgId = level->getImageId(fids[i]);

      /*- When image data is also cached together -*/
      if (cacheImagesAsWell) {
        ir->enable16BitRead(m_64bitCompatible);
        ir->setShrink(1);
        TImageP fullImg = ir->load();
        if (fullImg) {
          if (palette) fullImg->setPalette(palette);
          TImageCache::instance()->add(fullImgId, fullImg, true);
          setImageInfo(info, fullImg.getPointer());
        }
      }

      /*- load icons -*/
      TImageP img = ir->loadIcon();
      ir->enable16BitRead(false);
      if (img) {
        if (palette) img->setPalette(palette);
        TImageCache::instance()->add(iconIds[i], img, true);
      }
    }
  } catch (...) {
    return;
  }
}

void ImageLoader::setFid(const TFrameId &fid) { m_fid = fid; }

//***************************************************************************************
//    ImageRasterizer  implementation
//***************************************************************************************

bool ImageRasterizer::getInfo(TImageInfo &info, int imFlags, void *extData) {
  assert(false);  // None should get these... in case, TODO
  return false;
}

bool ImageRasterizer::isImageCompatible(int imFlags, void* extData) { 
  ImageLoader::BuildExtData *data = (ImageLoader::BuildExtData *)extData;
  
  if (m_cameraDPI == data->m_cameraDPI)
    if (m_antiAliasing == Preferences::instance()->getRasterizeAntialias())
        return true;
  return false;
}

//-------------------------------------------------------------------------

TImageP ImageRasterizer::build(int imFlags, void *extData) {
  assert(!(imFlags &
           ~(ImageManager::dontPutInCache | ImageManager::forceRebuild)));

  // Fetch image
  assert(extData);
  ImageLoader::BuildExtData *data = (ImageLoader::BuildExtData *)extData;

  const std::string &srcImgId = data->m_sl->getImageId(data->m_fid);

  TImageP img = ImageManager::instance()->getImage(srcImgId, imFlags, extData);
  if (img) {
    TVectorImageP vi = img;
    if (vi) {
      TRectD bbox = vi->getBBox();
      m_cameraDPI       = data->m_cameraDPI;

      double sx, sy;
      sx           = m_cameraDPI.x / Stage::inch;
      sy           = m_cameraDPI.y / Stage::inch;
      bbox.x0     = std::floor(bbox.x0 * sx);
      bbox.y0     = std::floor(bbox.y0 * sy);
      bbox.x1     = std::ceil(bbox.x1 * sx);
      bbox.y1     = std::ceil(bbox.y1 * sy);

      bbox.x1 += static_cast<int>(std::ceil(bbox.x1 - bbox.x0)) % 2;
      bbox.y1 += static_cast<int>(std::ceil(bbox.y1 - bbox.y0)) % 2;

      if (!bbox.isEmpty()) {
          m_antiAliasing = Preferences::instance()->getRasterizeAntialias();
          TDimension d(bbox.getLx(),bbox.getLy());
          TVectorRenderData rd(TVectorRenderData::ProductionSettings(),
                               TScale(sx,sy),
              TRect(), vi->getPalette());
          rd.m_antiAliasing = m_antiAliasing;
          rd.m_show0ThickStrokes = Preferences::instance()->getShow0ThickLines();

          //Is this too slow?
          {
            QSurfaceFormat format;
            format.setProfile(QSurfaceFormat::CompatibilityProfile);

            static QOffscreenSurface *surface = nullptr;
            if (!surface) {
              surface = new QOffscreenSurface();
              surface->setFormat(format);
              surface->create();
            }

            TRaster32P ras(d);

            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glMatrixMode(GL_MODELVIEW), glPushMatrix();
            glMatrixMode(GL_PROJECTION), glPushMatrix();
            {
              static std::unique_ptr<QOpenGLFramebufferObject> fb;
              static int lastLx = 0, lastLy = 0;
              static bool bboxChanged;

              if (!fb || d.lx != lastLx || d.ly != lastLy) {
                bboxChanged = true;
              }
              if (bboxChanged){
                fb.reset(new QOpenGLFramebufferObject(d.lx, d.ly));
                lastLx = d.lx;
                lastLy = d.ly;
              }
              fb->bind();

              if (bboxChanged) {
                glViewport(0, 0, d.lx, d.ly);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                gluOrtho2D(bbox.x0, bbox.x1, bbox.y0, bbox.y1);

                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
              }
              
              tglDraw(rd, vi.getPointer());

              glFlush();

              glReadPixels(0, 0, d.lx, d.ly, GL_BGRA, GL_UNSIGNED_BYTE,
                           ras->getRawData());

              fb->release();
            }
            glMatrixMode(GL_MODELVIEW), glPopMatrix();
            glMatrixMode(GL_PROJECTION), glPopMatrix();

            glPopAttrib();

            TRasterImageP ri = TRasterImageP(ras);

            int offsetX      = bbox.x1 - (d.lx / 2);
            int offsetY      = bbox.y1 - (d.ly / 2);
            ri->setOffset(TPoint(offsetX, offsetY));

            assert(glGetError() == 0);

            return ri;
          }
      }
    }
  }
  return TRasterImageP();
  // Error case: return a dummy image (is it really required?)

  TRaster32P ras(10,10);
  ras->fill(TPixel32(127, 0, 127, 127));

  return TRasterImageP(ras);
}

//***************************************************************************************
//    ImageFiller  implementation
//***************************************************************************************

bool ImageFiller::getInfo(TImageInfo &info, int imFlags, void *extData) {
  assert(false);  // None should get these... in case, TODO
  return false;
}

//-------------------------------------------------------------------------

TImageP ImageFiller::build(int imFlags, void *extData) {
  assert(imFlags == ImageManager::none);

  // Fetch image
  assert(extData);

  ImageLoader::BuildExtData *data = (ImageLoader::BuildExtData *)extData;
  assert(data->m_subs == 0);

  const std::string &srcImgId = data->m_sl->getImageId(data->m_fid);

  TImageP img = ImageManager::instance()->getImage(srcImgId, imFlags, extData);
  if (img) {
    TRasterImageP ri = img;
    if (ri) {
      TRaster32P ras = ri->getRaster();
      if (ras) {
        TRaster32P newRas = ras->clone();
        FullColorAreaFiller filler(newRas);

        TPaletteP palette = new TPalette();
        int styleId       = palette->getPage(0)->addStyle(TPixel32::White);

        FillParameters params;
        params.m_palette      = palette.getPointer();
        params.m_styleId      = styleId;
        params.m_minFillDepth = 0;
        params.m_maxFillDepth = 15;
        filler.rectFill(newRas->getBounds(), params, false);

        TRasterImageP ri = TRasterImageP(newRas);
        return ri;
      }
    }
  }

  // Error case: return a dummy image (is it really required?)

  TRaster32P ras(10, 10);
  ras->fill(TPixel32(127, 0, 127, 127));

  return TRasterImageP(ras);
}
