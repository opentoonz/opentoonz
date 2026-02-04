

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"

// TnzBase includes
#include "tcli.h"
#include "tenv.h"
#include "tpluginmanager.h"
#include "trasterfx.h"

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tconvert.h"
#include "tiio_std.h"
#include "timage_io.h"
#include "tnzimage.h"
#include "tlevel.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "tropcm.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tvectorrenderdata.h"
#include "tofflinegl.h"

#if defined(LINUX) || defined(FREEBSD)
#include <QGuiApplication>
#endif

// Add memory header for smart pointers
#include <memory>

using TCli::ArgumentT;
using TCli::QualifierT;

#define RENDER_LICENSE_NOT_FOUND 888

const char *rootVarName     = "TOONZROOT";
const char *systemVarPrefix = "TOONZ";

namespace {

//--------------------------------------------------------------------

/**
 * Check if file already exists and prompt user for replacement
 * @param fp File path to check
 */
void doesExist(const TFilePath &fp) {
  std::string msg;
  TFilePath path =
      fp.getParentDir() + (fp.getName() + "." + fp.getDottedType());

  if (TSystem::doesExistFileOrLevel(fp) ||
      TSystem::doesExistFileOrLevel(path)) {
    msg = "File " + fp.getLevelName() + " already exists:";
    std::cout << std::endl << msg << std::endl;

    char answer = ' ';
    while (answer != 'Y' && answer != 'N' && answer != 'y' && answer != 'n') {
      msg = "do you want to replace it? [Y/N] ";
      std::cout << msg;
      std::cin >> answer;

      if (answer == 'N' || answer == 'n') {
        msg = "Conversion aborted.";
        std::cout << std::endl << msg << std::endl;
        exit(1);
      }
    }
  }
}

//--------------------------------------------------------------------

/**
 * Get frame IDs based on user specified range
 * @param range Range qualifier from user input
 * @param level Input level
 * @return Vector of frame IDs to process
 */
std::vector<TFrameId> getFrameIds(const TCli::RangeQualifier &range,
                                  const TLevelP &level) {
  std::string msg;
  TFrameId r0, r1, lastFrame;
  TLevel::Iterator begin = level->begin();
  TLevel::Iterator end   = level->end();
  end--;
  lastFrame = end->first;

  std::vector<TFrameId> frames;

  if (range.isSelected()) {
    r0 = TFrameId(range.getFrom());
    r1 = TFrameId(range.getTo());
    if (r0 > r1) {
      std::swap(r0, r1);
    }
  } else {
    r0 = begin->first;
    r1 = end->first;
  }

  // Find first TFrameId
  TLevel::Iterator it = begin;
  if (r0 <= end->first) {
    while (it->first < r0) ++it;
  }

  // Fill vector up to last needed TFrameId
  while (it != level->end() && r1 >= it->first) {
    frames.push_back(it->first);
    ++it;
  }

  return frames;
}

//--------------------------------------------------------------------

/**
 * Convert from colored-mask (CM) format
 * @param lr Level reader
 * @param plt Palette
 * @param lw Level writer
 * @param frames Frame IDs to convert
 * @param aff Affine transformation
 * @param resType Resample filter type
 */
void convertFromCM(const TLevelReaderP &lr, const TPaletteP &plt,
                   const TLevelWriterP &lw, const std::vector<TFrameId> &frames,
                   const TAffine &aff,
                   const TRop::ResampleFilterType &resType) {
  TDimension dim(0, 0);  // Track dimensions to ensure consistent frame sizes

  for (size_t i = 0; i < frames.size(); i++) {
    try {
      TImageReaderP ir = lr->getFrameReader(frames[i]);
      TImageP img      = ir->load();
      TToonzImageP toonzImage(img);

      if (!toonzImage) continue;

      double xdpi, ydpi;
      toonzImage->getDpi(xdpi, ydpi);
      TRasterCM32P rasCMImage = toonzImage->getRaster();

      // Check frame dimensions consistency
      if (i == 0) {
        dim = rasCMImage->getSize();
      } else if (dim != rasCMImage->getSize()) {
        std::string msg = "Cannot continue to convert: not valid level!";
        std::cout << msg << std::endl;
        exit(1);
      }

      TRaster32P ras(convert(aff * convert(rasCMImage->getBounds())).getSize());

      if (!aff.isIdentity()) {
        TRop::resample(ras, rasCMImage, plt, aff, resType);
      } else {
        TRop::convert(ras, rasCMImage, plt);
      }

      TRasterImageP rasImage(ras);
      rasImage->setDpi(xdpi, ydpi);
      TImageWriterP iw = lw->getFrameWriter(frames[i]);
      iw->save(rasImage);

    } catch (...) {
      std::string msg = "Frame " + std::to_string(frames[i].getNumber()) +
                        ": conversion failed!";
      std::cout << msg << std::endl;
    }
  }
}

//--------------------------------------------------------------------

/**
 * Convert from vector image (VI) format
 * @param lr Level reader
 * @param plt Palette
 * @param lw Level writer
 * @param frames Frame IDs to convert
 * @param resType Resample filter type
 * @param width Output width
 */
void convertFromVI(const TLevelReaderP &lr, const TPaletteP &plt,
                   const TLevelWriterP &lw, const std::vector<TFrameId> &frames,
                   const TRop::ResampleFilterType &resType, int width) {
  std::vector<TVectorImageP> images;
  TRectD maxBbox;
  TAffine aff;

  // Find bounding box that contains all images
  for (size_t i = 0; i < frames.size(); i++) {
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TVectorImageP img = ir->load();
      images.push_back(img);
      maxBbox += img->getBBox();
    } catch (...) {
      std::string msg = "Frame " + std::to_string(frames[i].getNumber()) +
                        ": conversion failed!";
      std::cout << msg << std::endl;
    }
  }

  maxBbox = maxBbox.enlarge(2);

  // Calculate affine transformation for width
  if (width) {
    aff = TScale(static_cast<double>(width) / maxBbox.getLx());
  }

  maxBbox = aff * maxBbox;

  for (size_t i = 0; i < images.size(); i++) {
    try {
      TVectorImageP vectorImage = images[i];
      if (!vectorImage) continue;

      // Apply transformation and render image
      vectorImage->transform(aff, true);
      const TVectorRenderData rd(TTranslation(-maxBbox.getP00()), TRect(),
                                 plt.getPointer(), 0, true, true);

      std::unique_ptr<TOfflineGL> glContext(
          new TOfflineGL(convert(maxBbox).getSize()));
      glContext->clear(TPixel32::Transparent);
      glContext->draw(vectorImage, rd);

      TRaster32P rasImage = glContext->getRaster();
      TImageWriterP iw    = lw->getFrameWriter(frames[i]);
      iw->save(TRasterImageP(rasImage));

    } catch (...) {
      std::string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      std::cout << msg << std::endl;
    }
  }
}

//-----------------------------------------------------------------------

/**
 * Convert from full raster format
 * @param lr Level reader
 * @param lw Level writer
 * @param frames Frame IDs to convert
 * @param aff Affine transformation
 * @param resType Resample filter type
 */
void convertFromFullRaster(const TLevelReaderP &lr, const TLevelWriterP &lw,
                           const std::vector<TFrameId> &frames,
                           const TAffine &aff,
                           const TRop::ResampleFilterType &resType) {
  for (size_t i = 0; i < frames.size(); i++) {
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TRasterImageP img = ir->load();

      TRaster32P raster(convert(aff * img->getBBox()).getSize());

      if (!aff.isIdentity()) {
        TRop::resample(raster, img->getRaster(), aff, resType);
      } else {
        if (TRaster32P ras32 = img->getRaster()) {
          raster = ras32;
        } else {
          TRop::convert(raster, img->getRaster());
        }
      }

      TImageWriterP iw = lw->getFrameWriter(frames[i]);
      iw->save(TRasterImageP(raster));

    } catch (...) {
      std::string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      std::cout << msg << std::endl;
    }
  }
}

//-----------------------------------------------------------------------

/**
 * Convert from full raster to colored-mask format
 * @param lr Level reader
 * @param lw Level writer
 * @param frames Frame IDs to convert
 * @param aff Affine transformation
 * @param resType Resample filter type
 */
void convertFromFullRasterToCm(const TLevelReaderP &lr, const TLevelWriterP &lw,
                               const std::vector<TFrameId> &frames,
                               const TAffine &aff,
                               const TRop::ResampleFilterType &resType) {
  std::unique_ptr<TPalette> plt(new TPalette());

  for (size_t i = 0; i < frames.size(); i++) {
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TRasterImageP img = ir->load();

      double dpix, dpiy;
      img->getDpi(dpix, dpiy);
      if (dpix == 0 && dpiy == 0) {
        dpix = dpiy = Preferences::instance()->getDefLevelDpi();
      }

      TRasterCM32P raster(convert(aff * img->getBBox()).getSize());

      if (!aff.isIdentity()) {
        TRaster32P raux(raster->getSize());
        TRop::resample(raux, img->getRaster(), aff, resType);
        TRop::convert(raster, raux);
      } else {
        TRop::convert(raster, img->getRaster());
      }

      TImageWriterP iw = lw->getFrameWriter(frames[i]);
      TToonzImageP outimg(raster, raster->getBounds());
      outimg->setDpi(dpix, dpiy);
      outimg->setPalette(plt.get());
      iw->save(outimg);

    } catch (...) {
      std::string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      std::cout << msg << std::endl;
    }
  }

  // Save palette to file
  TFilePath pltPath = lw->getFilePath().withNoFrame().withType("tpl");
  if (TSystem::touchParentDir(pltPath)) {
    if (TSystem::doesExistFileOrLevel(pltPath)) {
      TSystem::removeFileOrLevel(pltPath);
    }

    try {
      TOStream os(pltPath);
      os << plt.get();
    } catch (...) {
      std::cout << "Warning: Could not save palette file" << std::endl;
    }
  }
}

//-----------------------------------------------------------------------

/**
 * Main conversion function
 * @param source Source file path
 * @param dest Destination file path
 * @param range Frame range qualifier
 * @param width Width qualifier
 * @param prop Output properties
 * @param resQuality Resample quality
 */
void convert(const TFilePath &source, const TFilePath &dest,
             const TCli::RangeQualifier &range, const TCli::IntQualifier &width,
             TPropertyGroup *prop,
             const TRenderSettings::ResampleQuality &resQuality) {
  std::string msg;

  // Load level information
  TLevelReaderP lr(source);
  TLevelP level = lr->loadInfo();

  // Get frame IDs based on range
  std::vector<TFrameId> frames = getFrameIds(range, level);

  doesExist(dest);

  msg = "Level loaded";
  std::cout << msg << std::endl;
  msg = "Conversion in progress: wait please...";
  std::cout << msg << std::endl;

  TAffine aff;
  if (width.isSelected()) {
    // Calculate affine for resampling
    int imgLx = lr->getImageInfo()->m_lx;
    aff       = TScale(static_cast<double>(width.getValue()) /
                       static_cast<double>(imgLx));
  }

  // Set resample filter type based on quality
  TRop::ResampleFilterType resType = TRop::Triangle;  // Default

  if (resQuality == TRenderSettings::StandardResampleQuality) {
    resType = TRop::Triangle;
  } else if (resQuality == TRenderSettings::ImprovedResampleQuality) {
    resType = TRop::Hann2;
  } else if (resQuality == TRenderSettings::HighResampleQuality) {
    resType = TRop::Hamming3;
  } else if (resQuality == TRenderSettings::Triangle_FilterResampleQuality) {
    resType = TRop::Triangle;
  } else if (resQuality == TRenderSettings::Mitchell_FilterResampleQuality) {
    resType = TRop::Mitchell;
  } else if (resQuality == TRenderSettings::Cubic5_FilterResampleQuality) {
    resType = TRop::Cubic5;
  } else if (resQuality == TRenderSettings::Cubic75_FilterResampleQuality) {
    resType = TRop::Cubic75;
  } else if (resQuality == TRenderSettings::Cubic1_FilterResampleQuality) {
    resType = TRop::Cubic1;
  } else if (resQuality == TRenderSettings::Hann2_FilterResampleQuality) {
    resType = TRop::Hann2;
  } else if (resQuality == TRenderSettings::Hann3_FilterResampleQuality) {
    resType = TRop::Hann3;
  } else if (resQuality == TRenderSettings::Hamming2_FilterResampleQuality) {
    resType = TRop::Hamming2;
  } else if (resQuality == TRenderSettings::Hamming3_FilterResampleQuality) {
    resType = TRop::Hamming3;
  } else if (resQuality == TRenderSettings::Lanczos2_FilterResampleQuality) {
    resType = TRop::Lanczos2;
  } else if (resQuality == TRenderSettings::Lanczos3_FilterResampleQuality) {
    resType = TRop::Lanczos3;
  } else if (resQuality == TRenderSettings::Gauss_FilterResampleQuality) {
    resType = TRop::Gauss;
  } else if (resQuality ==
             TRenderSettings::ClosestPixel_FilterResampleQuality) {
    resType = TRop::ClosestPixel;
  } else if (resQuality == TRenderSettings::Bilinear_FilterResampleQuality) {
    resType = TRop::Bilinear;
  }

  std::string ext = source.getType();
  TLevelWriterP lw(dest, prop);

  if (ext != "tlv" && ext != "pli") {
    if (dest.getType() == "tlv") {
      convertFromFullRasterToCm(lr, lw, frames, aff, resType);
    } else {
      convertFromFullRaster(lr, lw, frames, aff, resType);
    }
  } else if (ext == "tlv") {  // ToonzImage
    convertFromCM(lr, level->getPalette(), lw, frames, aff, resType);
  } else if (ext == "pli") {  // VectorImage
    convertFromVI(lr, level->getPalette(), lw, frames, resType,
                  width.getValue());
  }
}

}  // namespace

//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
#if defined(LINUX) || defined(FREEBSD)
  QGuiApplication app(argc, argv);
#endif

  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);
  TEnv::setApplicationFileName(argv[0]);

  std::string msg;

  // Initialize qualifiers
  TCli::FilePathArgument srcName("srcName", "Source file");
  TCli::FilePathArgument dstName("dstName", "Target file");
  TCli::QualifierT<TFilePath> tnzName("-s sceneName", "Scene file");
  TCli::RangeQualifier range;
  TCli::IntQualifier width("-w width", "Image width");

  TCli::Usage usage(argv[0]);
  usage.add(srcName + dstName + width + tnzName + range);

  if (!usage.parse(argc, argv)) {
    exit(1);
  }

  try {
    Tiio::defineStd();
    TSystem::hasMainLoop(false);

    TPropertyGroup *prop = nullptr;
    initImageIo();

    TRenderSettings::ResampleQuality resQuality =
        TRenderSettings::StandardResampleQuality;

    TFilePath dstFilePath = dstName.getValue();
    TFilePath srcFilePath = srcName.getValue();

    if (!TSystem::doesExistFileOrLevel(srcFilePath)) {
      msg = srcFilePath.getLevelName() + " level doesn't exist.";
      std::cout << std::endl << msg << std::endl;
      exit(1);
    }

    msg = "Loading " + srcFilePath.getLevelName();
    std::cout << std::endl << msg << std::endl;

    std::string ext = dstFilePath.getType();

    // Check for valid extension
    if (ext.empty()) {
      ext = dstFilePath.getName();
      if (ext.empty()) {
        msg = "Invalid extension!";
        std::cout << msg << std::endl;
        exit(1);
      }
    }

    // Handle destination path
    if (dstFilePath.getParentDir().isEmpty()) {
      dstFilePath =
          srcFilePath.getParentDir() + (srcFilePath.getName() + "." + ext);
    }

    // Scene-based conversion
    std::unique_ptr<ToonzScene> scenePtr;
    if (tnzName.isSelected()) {
      TFilePath tnzFilePath = tnzName.getValue();
      if (tnzFilePath.getType() != "tnz") {
        msg = "Invalid scene file: conversion terminated!";
        std::cout << msg << std::endl;
        exit(1);
      }

      if (!TSystem::doesExistFileOrLevel(tnzFilePath)) {
        return -1;
      }

      scenePtr = std::make_unique<ToonzScene>();
      try {
        scenePtr->loadTnzFile(tnzFilePath);
      } catch (...) {
        msg = "There were problems loading the scene " + srcFilePath.getName() +
              ".\n Some files may be missing.";
        std::cout << msg << std::endl;
      }

      if (scenePtr) {
        resQuality = scenePtr->getProperties()
                         ->getOutputProperties()
                         ->getRenderSettings()
                         .m_quality;
        prop = scenePtr->getProperties()
                   ->getOutputProperties()
                   ->getFileFormatProperties(ext);
      } else {
        msg = "Invalid scene file: conversion terminated!";
        std::cout << msg << std::endl;
        exit(1);
      }
    }

    // Perform conversion based on format
    if (ext != "3gp" && ext != "pli") {
      convert(srcFilePath, dstFilePath, range, width, prop, resQuality);
    } else {
      msg = "Cannot convert to ." + ext + " format.";
      std::cout << msg << std::endl;
      exit(1);
    }

  } catch (TException &e) {
    msg = "Untrapped exception: " + ::to_string(e.getMessage());
    std::cout << msg << std::endl;
    return -1;
  } catch (std::exception &e) {
    msg = "Standard exception: " + std::string(e.what());
    std::cout << msg << std::endl;
    return -1;
  } catch (...) {
    msg = "Unknown exception occurred!";
    std::cout << msg << std::endl;
    return -1;
  }

  msg = "Conversion terminated!";
  std::cout << std::endl << msg << std::endl;
  return 0;
}
