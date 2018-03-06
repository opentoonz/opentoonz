

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

using namespace std;
using namespace TCli;

typedef ArgumentT<TFilePath> FilePathArgument;
typedef QualifierT<TFilePath> FilePathQualifier;

#define RENDER_LICENSE_NOT_FOUND 888

const char *applicationVersion = "1.2";
const char *applicationName    = "OpenToonz";
const char *rootVarName        = "TOONZROOT";
const char *systemVarPrefix    = "TOONZ";

namespace {

//--------------------------------------------------------------------

void doesExist(const TFilePath &fp) {
  string msg;
  TFilePath path =
      fp.getParentDir() + (fp.getName() + "." + fp.getDottedType());
  if (TSystem::doesExistFileOrLevel(fp) ||
      TSystem::doesExistFileOrLevel(path)) {
    msg = "File " + fp.getLevelName() + " already exists:";
    cout << endl << msg << endl;
    char answer = ' ';
    while (answer != 'Y' && answer != 'N' && answer != 'y' && answer != 'n') {
      msg = "do you want to replace it? [Y/N] ";
      cout << msg;
      cin >> answer;
      if (answer == 'N' || answer == 'n') {
        msg = "Conversion aborted.";
        cout << endl << msg << endl;
        exit(1);
      }
    }
  }
}

//--------------------------------------------------------------------

// Ritorna un vettore contenete i soli TFrameId corrispondenti al range inserito
// dall'utente
vector<TFrameId> getFrameIds(const RangeQualifier &range,
                             const TLevelP &level) {
  string msg;
  TFrameId r0, r1, lastFrame;
  TLevel::Iterator begin = level->begin();
  TLevel::Iterator end   = level->end();
  end--;
  lastFrame = end->first;
  vector<TFrameId> frames;
  if (range.isSelected()) {
    r0 = TFrameId(range.getFrom());
    r1 = TFrameId(range.getTo());
    if (r0 > r1) {
      TFrameId app = r0;
      r0           = r1;
      r1           = app;
    }
  } else {
    r0 = begin->first;
    r1 = end->first;
  }
  // cerco il primo TFrameId
  TLevel::Iterator it = begin;
  if (r0 <= end->first)
    while (it->first < r0) ++it;
  while (it != level->end() && r1 >= it->first) {
    // Riempio il vettore fino all'ultimo TFrameId che mi serve
    frames.push_back(it->first);
    ++it;
  }
  return frames;
}

//--------------------------------------------------------------------

void convertFromCM(const TLevelReaderP &lr, const TPaletteP &plt,
                   const TLevelWriterP &lw, const vector<TFrameId> &frames,
                   const TAffine &aff,
                   const TRop::ResampleFilterType &resType) {
  TDimension dim(
      0,
      0);  // Serve per controllare che non ci siano frame di diverse dimensioni
  for (int i = 0; i < (int)frames.size(); i++) {
    try {
      TImageReaderP ir = lr->getFrameReader(frames[i]);
      TImageP img      = ir->load();
      TToonzImageP toonzImage(img);
      double xdpi, ydpi;
      toonzImage->getDpi(xdpi, ydpi);
      assert(toonzImage);
      if (toonzImage) {
        TRasterCM32P rasCMImage = toonzImage->getRaster();
        if (i == 0)
          dim = rasCMImage->getSize();
        else if (dim != rasCMImage->getSize()) {
          // dimensioni diverse dei frame
          string msg = "Cannot continue to convert: not valid level!";
          cout << msg << endl;
          exit(1);
        }
        TRaster32P ras(
            convert(aff * convert(rasCMImage->getBounds())).getSize());
        if (!aff.isIdentity())
          TRop::resample(ras, rasCMImage, plt, aff, resType);
        else
          TRop::convert(ras, rasCMImage, plt);
        TRasterImageP rasImage(ras);
        rasImage->setDpi(xdpi, ydpi);
        TImageWriterP iw = lw->getFrameWriter(frames[i]);
        iw->save(rasImage);
      }
    } catch (...) {
      string msg = "Frame " + std::to_string(frames[i].getNumber()) +
                   ": conversion failed!";
      cout << msg << endl;
    }
  }
}

//--------------------------------------------------------------------

void convertFromVI(const TLevelReaderP &lr, const TPaletteP &plt,
                   const TLevelWriterP &lw, const vector<TFrameId> &frames,
                   const TRop::ResampleFilterType &resType, int width) {
  int i;
  vector<TVectorImageP> images;
  TRectD maxBbox;
  TAffine aff;
  for (i = 0; i < (int)frames.size();
       i++) {  // trovo la bbox che possa contenere tutte le immagini
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TVectorImageP img = ir->load();
      images.push_back(img);
      maxBbox += img->getBBox();
    } catch (...) {
      string msg = "Frame " + std::to_string(frames[i].getNumber()) +
                   ": conversion failed!";
      cout << msg << endl;
    }
  }
  maxBbox = maxBbox.enlarge(2);
  if (width)  // calcolo l'affine
    aff   = TScale((double)width / maxBbox.getLx());
  maxBbox = aff * maxBbox;

  for (i = 0; i < (int)images.size(); i++) {
    try {
      TVectorImageP vectorImage = images[i];
      assert(vectorImage);
      if (vectorImage) {
        // faccio il render dell'immagine
        vectorImage->transform(aff, true);
        const TVectorRenderData rd(TTranslation(-maxBbox.getP00()), TRect(),
                                   plt.getPointer(), 0, true, true);
        TOfflineGL *glContext = new TOfflineGL(convert(maxBbox).getSize());
        glContext->clear(TPixel32::Transparent);
        glContext->draw(vectorImage, rd);
        TRaster32P rasImage = (glContext->getRaster());
        TImageWriterP iw    = lw->getFrameWriter(frames[i]);
        iw->save(TRasterImageP(rasImage));
      }
    } catch (...) {
      string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      cout << msg << endl;
    }
  }
}

//-----------------------------------------------------------------------

void convertFromFullRaster(const TLevelReaderP &lr, const TLevelWriterP &lw,
                           const vector<TFrameId> &frames, const TAffine &aff,
                           const TRop::ResampleFilterType &resType) {
  for (int i = 0; i < (int)frames.size(); i++) {
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TRasterImageP img = ir->load();
      TRaster32P raster(convert(aff * img->getBBox()).getSize());
      if (!aff.isIdentity())
        TRop::resample(raster, img->getRaster(), aff, resType);
      else {
        if ((TRaster32P)img->getRaster())
          raster = img->getRaster();
        else
          TRop::convert(raster, img->getRaster());
      }
      TImageWriterP iw = lw->getFrameWriter(frames[i]);
      iw->save(TRasterImageP(raster));
    } catch (...) {
      string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      cout << msg << endl;
    }
  }
}

//-----------------------------------------------------------------------

void convertFromFullRasterToCm(const TLevelReaderP &lr, const TLevelWriterP &lw,
                               const vector<TFrameId> &frames,
                               const TAffine &aff,
                               const TRop::ResampleFilterType &resType) {
  TPalette *plt = new TPalette();

  for (int i = 0; i < (int)frames.size(); i++) {
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TRasterImageP img = ir->load();
      double dpix, dpiy;
      img->getDpi(dpix, dpiy);
      if (dpix == 0 && dpiy == 0)
        dpix = dpiy = Preferences::instance()->getDefLevelDpi();
      TRasterCM32P raster(convert(aff * img->getBBox()).getSize());

      if (!aff.isIdentity()) {
        TRaster32P raux(raster->getSize());
        TRop::resample(raux, img->getRaster(), aff, resType);
        TRop::convert(raster, raux);
      } else
        TRop::convert(raster, img->getRaster());

      TImageWriterP iw = lw->getFrameWriter(frames[i]);
      TToonzImageP outimg(raster, raster->getBounds());
      outimg->setDpi(dpix, dpiy);
      outimg->setPalette(plt);
      iw->save(outimg);

    } catch (...) {
      string msg = "Frame " + frames[i].expand() + ": conversion failed!";
      cout << msg << endl;
    }
  }

  TFilePath pltPath = lw->getFilePath().withNoFrame().withType("tpl");
  if (TSystem::touchParentDir(pltPath)) {
    if (TSystem::doesExistFileOrLevel(pltPath))
      TSystem::removeFileOrLevel(pltPath);
    TOStream os(pltPath);
    os << plt;
  }
}
//-----------------------------------------------------------------------

void convert(const TFilePath &source, const TFilePath &dest,
             const RangeQualifier &range, const IntQualifier &width,
             TPropertyGroup *prop,
             const TRenderSettings::ResampleQuality &resQuality) {
  string msg;
  // Carico le informazione del livello
  TLevelReaderP lr(source);
  TLevelP level = lr->loadInfo();

  // Trovo i TFrameId corrispondenti al range
  vector<TFrameId> frames = getFrameIds(range, level);

  doesExist(dest);

  msg = "Level loaded";
  cout << msg << endl;
  msg = "Conversion in progress: wait please...";
  cout << msg << endl;

  TAffine aff;
  if (width.isSelected()) {
    // calcolo un affine per fare la resample
    int imgLx = lr->getImageInfo()->m_lx;
    aff       = TScale((double)width / (double)imgLx);
  }

  // setto il FilterResempleType giusto
  TRop::ResampleFilterType resType;
  if (resQuality == TRenderSettings::StandardResampleQuality)
    resType = TRop::Triangle;
  else if (resQuality == TRenderSettings::ImprovedResampleQuality)
    resType = TRop::Hann2;
  else if (resQuality == TRenderSettings::HighResampleQuality)
    resType = TRop::Hamming3;
  else if (resQuality == TRenderSettings::Triangle_FilterResampleQuality)
    resType = TRop::Triangle;
  else if (resQuality == TRenderSettings::Mitchell_FilterResampleQuality)
    resType = TRop::Mitchell;
  else if (resQuality == TRenderSettings::Cubic5_FilterResampleQuality)
    resType = TRop::Cubic5;
  else if (resQuality == TRenderSettings::Cubic75_FilterResampleQuality)
    resType = TRop::Cubic75;
  else if (resQuality == TRenderSettings::Cubic1_FilterResampleQuality)
    resType = TRop::Cubic1;
  else if (resQuality == TRenderSettings::Hann2_FilterResampleQuality)
    resType = TRop::Hann2;
  else if (resQuality == TRenderSettings::Hann3_FilterResampleQuality)
    resType = TRop::Hann3;
  else if (resQuality == TRenderSettings::Hamming2_FilterResampleQuality)
    resType = TRop::Hamming2;
  else if (resQuality == TRenderSettings::Hamming3_FilterResampleQuality)
    resType = TRop::Hamming3;
  else if (resQuality == TRenderSettings::Lanczos2_FilterResampleQuality)
    resType = TRop::Lanczos2;
  else if (resQuality == TRenderSettings::Lanczos3_FilterResampleQuality)
    resType = TRop::Lanczos3;
  else if (resQuality == TRenderSettings::Gauss_FilterResampleQuality)
    resType = TRop::Gauss;
  else if (resQuality == TRenderSettings::ClosestPixel_FilterResampleQuality)
    resType = TRop::ClosestPixel;
  else if (resQuality == TRenderSettings::Bilinear_FilterResampleQuality)
    resType = TRop::Bilinear;

  string ext = source.getType();
  TLevelWriterP lw(dest, prop);
  if (ext != "tlv" && ext != "pli") {
    if (dest.getType() == "tlv")
      convertFromFullRasterToCm(lr, lw, frames, aff, resType);
    else
      convertFromFullRaster(lr, lw, frames, aff, resType);
  } else if (ext == "tlv")  // ToonzImage
    convertFromCM(lr, level->getPalette(), lw, frames, aff, resType);
  else if (ext == "pli")  // VectorImage
    convertFromVI(lr, level->getPalette(), lw, frames, resType,
                  width.getValue());
}

}  // namespace

//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  TEnv::setApplication(applicationName, applicationVersion);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);
  TFilePath fp = TEnv::getStuffDir();

  string msg;
  // Inizializzo i qualificatori
  TCli::FilePathArgument srcName("srcName", "Source file");
  TCli::FilePathArgument dstName("dstName", "Target file");
  FilePathQualifier tnzName("-s sceneName", "Scene file");
  RangeQualifier range;
  IntQualifier width("-w width", "Image width");

  Usage usage(argv[0]);
  usage.add(srcName + dstName + width + tnzName + range);
  if (!usage.parse(argc, argv)) exit(1);

  try {
    Tiio::defineStd();
    // TPluginManager::instance()->loadStandardPlugins();

    TSystem::hasMainLoop(false);
    TPropertyGroup *prop = 0;
    initImageIo();
    TRenderSettings::ResampleQuality resQuality =
        TRenderSettings::StandardResampleQuality;

    TFilePath dstFilePath = dstName.getValue();
    TFilePath srcFilePath = srcName.getValue();
    if (!TSystem::doesExistFileOrLevel(srcFilePath)) {
      msg = srcFilePath.getLevelName() + " level doesn't exist.";
      cout << endl << msg << endl;
      exit(1);
    }
    msg = "Loading " + srcFilePath.getLevelName();
    cout << endl << msg << endl;

    string ext = dstFilePath.getType();
    // controllo che ci sia un'estensione
    if (ext == "") {
      ext = ::to_string(dstFilePath);
      if (ext == "") {
        msg = "Invalid extension!";
        cout << msg << endl;
        exit(1);
      }
    }
    if (dstFilePath.getParentDir()
            .isEmpty())  // ho specificato solo l'estensione
      dstFilePath =
          srcFilePath.getParentDir() + (srcFilePath.getName() + "." + ext);
    if (tnzName.isSelected()) {
      // Devo prendermi i settaggi degli "output setting" dalla scena!
      TFilePath tnzFilePath = tnzName.getValue();
      if (tnzFilePath.getType() != "tnz") {
        msg = "Invalid scene file: conversion terminated!";
        cout << msg << endl;
        exit(1);
      }

      if (!TSystem::doesExistFileOrLevel(tnzFilePath)) return false;
      ToonzScene *scene = new ToonzScene();
      try {
        scene->loadTnzFile(tnzFilePath);
      } catch (...) {
        string msg;
        msg = "There were problems loading the scene " +
              ::to_string(srcFilePath) + ".\n Some files may be missing.";
        cout << msg << endl;
        // return false;
      }

      if (scene) {
        resQuality = scene->getProperties()
                         ->getOutputProperties()
                         ->getRenderSettings()
                         .m_quality;
        prop = scene->getProperties()
                   ->getOutputProperties()
                   ->getFileFormatProperties(ext);
      } else {
        msg = "Invalid scene file: conversion terminated!";
        cout << msg << endl;
        exit(1);
      }
    }
    if (ext != "3gp" && ext != "pli") {
      // assert(ext!="3gp" && ext!="pli" && ext!="tlv");
      convert(srcFilePath, dstFilePath, range, width, prop, resQuality);
    } else {
      msg = "Cannot convert to ." + ext + " format.";
      cout << msg << endl;
      exit(1);
    }
  } catch (TException &e) {
    msg = "Untrapped exception: " + ::to_string(e.getMessage());
    cout << msg << endl;
    return -1;
  }
  msg = "Conversion terminated!";
  cout << endl << msg << endl;
  return 0;
}
