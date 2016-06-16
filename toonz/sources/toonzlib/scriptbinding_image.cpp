

#include "toonz/scriptbinding_image.h"
#include "toonz/scriptbinding_level.h"
#include "toonz/scriptbinding_files.h"
#include "tsystem.h"
#include "ttoonzimage.h"
#include "tfiletype.h"
#include "timage_io.h"
#include "tlevel_io.h"

namespace TScriptBinding {

Image::Image() {}

Image::Image(const TImageP img) : m_img(img) {}

Image::Image(TImage *img) : m_img(img) {}

Image::~Image() {}

QScriptValue Image::ctor(QScriptContext *context, QScriptEngine *engine) {
  Image *img       = new Image();
  QScriptValue obj = create(engine, img);
  QScriptValue err = checkArgumentCount(context, "the Image constructor", 0, 1);
  if (err.isError()) return err;
  if (context->argumentCount() == 1) {
    return obj.property("load").call(obj, context->argumentsObject());
  }
  return obj;
}

QScriptValue Image::toString() {
  if (m_img) {
    TImage::Type type = m_img->getType();
    if (type == TImage::RASTER)
      return QString("Raster image ( %1 x %2 )")
          .arg(getWidth())
          .arg(getHeight());
    else if (type == TImage::TOONZ_RASTER)
      return QString("Toonz raster image ( %1 x %2 )")
          .arg(getWidth())
          .arg(getHeight());
    else if (type == TImage::VECTOR)
      return QString("Vector image");
    else
      return QString("Image");
  } else {
    return "Empty image";
  }
}

int Image::getWidth() {
  return !!m_img && !!m_img->raster() ? m_img->raster()->getSize().lx : 0;
}

int Image::getHeight() {
  return !!m_img && !!m_img->raster() ? m_img->raster()->getSize().ly : 0;
}

double Image::getDpi() {
  if (TRasterImageP ri = m_img) {
    double dpix = 0, dpiy = 0;
    ri->getDpi(dpix, dpiy);
    return dpix;
  } else if (TToonzImageP ti = m_img) {
    double dpix = 0, dpiy = 0;
    ti->getDpi(dpix, dpiy);
    return dpix;
  } else
    return 0;
}

QString Image::getType() const {
  if (m_img) {
    TImage::Type type = m_img->getType();
    if (type == TImage::RASTER)
      return "Raster";
    else if (type == TImage::TOONZ_RASTER)
      return "ToonzRaster";
    else if (type == TImage::VECTOR)
      return "Vector";
    else
      return "Unknown";
  } else
    return "Empty";
}

QScriptValue Image::load(const QScriptValue &fpArg) {
  // clear the old image (if any)
  m_img = TImageP();

  // get the path
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  QString fpStr = fpArg.toString();

  try {
    // check if the file/level does exist
    if (!TSystem::doesExistFileOrLevel(fp)) {
      return context()->throwError(tr("File %1 doesn't exist").arg(fpStr));
    }

    // the file could be a level
    TFileType::Type fileType = TFileType::getInfo(fp);
    if (TFileType::isLevel(fileType)) {
      // file is a level: read first frame
      TLevelReaderP lr(fp);
      TLevelP level = lr->loadInfo();
      int n         = level->getFrameCount();
      if (n > 0) {
        // there are some frames
        TFrameId fid = fp.getFrame();
        if (fid == TFrameId::NO_FRAME || fid == TFrameId::EMPTY_FRAME)
          fid = level->begin()->first;

        m_img = lr->getFrameReader(fid)->load();
        if (!m_img) {
          return context()->throwError(QString("Could not read %1").arg(fpStr));
        }
        m_img->setPalette(level->getPalette());
        if (n > 1 && (fp.getFrame() == TFrameId::EMPTY_FRAME ||
                      fp.getFrame() == TFrameId::NO_FRAME)) {
          // warning: a multi-frame level read into an Image
          warning(tr("Loaded first frame of %1").arg(n));
        }
      } else {
        // level contains no frame (not sure it can even happen)
        return context()->throwError(
            QString("%1 contains no frames").arg(fpStr));
      }
    } else {
      // plain image: try to read it
      if (!TImageReader::load(fp, m_img)) {
        return context()->throwError(
            QString("File %1 not found or not readable").arg(fpStr));
      }
    }
    // return a reference to the Image object
    return context()->thisObject();
  } catch (...) {
    return context()->throwError(
        tr("Unexpected error while reading image").arg(fpStr));
  }
}

QScriptValue Image::save(const QScriptValue &fpArg) {
  // clear the old image (if any)
  if (!m_img) {
    return context()->throwError("Can't save an empty image");
  }

  // get the path
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  QString fpStr = fpArg.toString();

  // handle conversion (if it is needed and possible)
  TFileType::Type fileType = TFileType::getInfo(fp);

  bool isCompatible = false;
  if (TFileType::isFullColor(fileType)) {
    if (m_img->getType() == TImage::RASTER) isCompatible = true;
  } else if (TFileType::isVector(fileType)) {
    if (m_img->getType() == TImage::VECTOR) isCompatible = true;
  } else if (fileType & TFileType::CMAPPED_IMAGE) {
    if (m_img->getType() == TImage::TOONZ_RASTER) isCompatible = true;
  } else {
    return context()->throwError(tr("Unrecognized file type :").arg(fpStr));
  }
  if (!isCompatible) {
    return context()->throwError(
        tr("Can't save a %1 image to this file type : %2")
            .arg(getType())
            .arg(fpStr));
  }

  try {
    if (TFileType::isLevel(fileType)) {
      TLevelP level = new TLevel();
      level->setPalette(m_img->getPalette());
      level->setFrame(TFrameId(1), m_img);
      TLevelWriterP lw(fp);
      if (m_img->getPalette()) lw->setPalette(m_img->getPalette());
      lw->save(level);
    } else {
      TImageWriterP iw(fp);
      iw->save(fp, m_img);
    }
    // return a reference to the Image object
    return context()->thisObject();
  } catch (...) {
    return context()->throwError(
        tr("Unexpected error while writing image").arg(fpStr));
  }
}

QScriptValue checkImage(QScriptContext *context, const QScriptValue &value,
                        Image *&img) {
  img = qscriptvalue_cast<Image *>(value);
  if (!img || !img->getImg())
    return context->throwError(
        QObject::tr("Bad argument (%1): should be an Image (not empty)")
            .arg(value.toString()));
  else
    return QScriptValue();
}

}  // namespace TScriptBinding
