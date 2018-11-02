

#include "toonz/scriptbinding_level.h"
#include "toonz/scriptbinding_files.h"
#include <QScriptEngine>
#include "timage_io.h"
#include "tlevel_io.h"
#include "tlevel.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/tcamera.h"
#include "trop.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "tpalette.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tgeometry.h"
#include "toonz/stage.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelset.h"
#include "tfiletype.h"
#include "tsystem.h"
#include <QRegExp>
#include <QColor>

namespace TScriptBinding {

Level::Level()
    : m_sl(0)
    , m_type(NO_XSHLEVEL)
    , m_scene(new ToonzScene())
    , m_sceneOwner(true) {}

Level::Level(TXshSimpleLevel *sl)
    : m_sl(sl)
    , m_type(sl->getType())
    , m_scene(sl->getScene())
    , m_sceneOwner(false) {
  sl->addRef();
}

Level::~Level() {
  if (m_sceneOwner) delete m_scene;
  if (m_sl) m_sl->release();
}

QScriptValue Level::ctor(QScriptContext *context, QScriptEngine *engine) {
  Level *level     = new Level();
  QScriptValue obj = engine->newQObject(level, QScriptEngine::AutoOwnership);
  if (context->argumentCount() == 1) {
    return obj.property("load").call(obj, context->argumentsObject());
  }
  return obj;
}

QScriptValue Level::toString() {
  QString info  = "(";
  QString comma = "";
  if (getName() != "") {
    info.append(comma).append(getName());
    comma = ", ";
  }
  info.append(comma).append(tr("%1 frames").arg(getFrameCount()));
  info.append(")");
  if (m_type == PLI_XSHLEVEL)
    return QString("Vector level %1").arg(info);
  else if (m_type == TZP_XSHLEVEL)
    return QString("Toonz level %1").arg(info);
  else if (m_type == NO_XSHLEVEL)
    return QString("Empty level");
  else if (m_type == OVL_XSHLEVEL)
    return QString("Raster level %1").arg(info);
  else
    return QString("Level %1").arg(info);
}

QString Level::getType() const {
  if (m_type == NO_XSHLEVEL)
    return "Empty";
  else if (m_type == PLI_XSHLEVEL)
    return "Vector";
  else if (m_type == TZP_XSHLEVEL)
    return "ToonzRaster";
  else if (m_type == OVL_XSHLEVEL)
    return "Raster";
  else
    return "Unknown";
}

int Level::getFrameCount() const { return m_sl ? m_sl->getFrameCount() : 0; }

QString Level::getName() const {
  return m_sl ? QString::fromStdWString(m_sl->getName()) : "";
}

void Level::setName(const QString &name) {
  if (m_sl) m_sl->setName(name.toStdWString());
}

QScriptValue Level::getPath() const {
  if (m_sl) {
    FilePath *result = new FilePath(m_sl->getPath());
    return result->create<FilePath>(engine());
  } else
    return QScriptValue();
  return m_sl ? QString::fromStdWString(m_sl->getName()) : "";
}

void Level::setPath(const QScriptValue &pathArg) {
  TFilePath fp;
  FilePath *filePath = qscriptvalue_cast<FilePath *>(pathArg);
  if (filePath)
    fp = filePath->getToonzFilePath();
  else if (pathArg.isString())
    fp = TFilePath(pathArg.toString().toStdString());
  else
    context()->throwError(
        tr("Bad argument (%1). It should be FilePath or string")
            .arg(pathArg.toString()));
  if (m_sl) {
    m_sl->setPath(fp);
    try {
      m_sl->load();
    } catch (...) {
      context()->throwError(
          tr("Exception loading level (%1)")
              .arg(QString::fromStdWString(fp.getWideString())));
    }
  }
}

QScriptValue Level::load(const QScriptValue &fpArg) {
  if (m_sl) {
    m_scene->getLevelSet()->removeLevel(m_sl, true);
    m_sl->release();
    m_sl = 0;
  }

  // get the path
  TFilePath fp;
  QScriptValue err = checkFilePath(context(), fpArg, fp);
  if (err.isError()) return err;
  QString fpStr = fpArg.toString();

  try {
    if (!TSystem::doesExistFileOrLevel(fp)) {
      return context()->throwError(tr("File %1 doesn't exist").arg(fpStr));
    }
    TFileType::Type fileType = TFileType::getInfo(fp);
    if (TFileType::isVector(fileType))
      m_type = PLI_XSHLEVEL;
    else if (0 != (fileType & TFileType::CMAPPED_IMAGE))
      m_type = TZP_XSHLEVEL;
    else if (0 != (fileType & TFileType::RASTER_IMAGE))
      m_type = OVL_XSHLEVEL;
    else {
      return context()->throwError(tr("File %1 is unsupported").arg(fpStr));
    }
    TXshLevel *xl = m_scene->loadLevel(fp);
    if (xl) {
      m_sl = xl->getSimpleLevel();
      m_sl->addRef();
    }
    return context()->thisObject();
  } catch (...) {
    return context()->throwError(tr("Exception reading %1").arg(fpStr));
  }
}

QScriptValue Level::save(const QScriptValue &fpArg) {
  if (getFrameCount() == 0) {
    return context()->throwError(tr("Can't save an empty level"));
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
    if (m_sl->getType() == OVL_XSHLEVEL) isCompatible = true;
  } else if (TFileType::isVector(fileType)) {
    if (m_sl->getType() == PLI_XSHLEVEL) isCompatible = true;
  } else if (fileType & TFileType::CMAPPED_IMAGE) {
    if (m_sl->getType() == TZP_XSHLEVEL) isCompatible = true;
  } else {
    return context()->throwError(tr("Unrecognized file type :").arg(fpStr));
  }
  if (!isCompatible) {
    return context()->throwError(
        tr("Can't save a %1 level to this file type : %2")
            .arg(getType())
            .arg(fpStr));
  }

  try {
    m_sl->save(fp);
  } catch (TSystemException se) {
    return context()->throwError(
        tr("Exception writing %1")
            .arg(QString::fromStdWString(se.getMessage())));
  }
  return context()->thisObject();
}

TFrameId Level::getFid(const QScriptValue &arg, QString &err) {
  if (arg.isNumber() || arg.isString()) {
    QString s = arg.toString();
    QRegExp re("(-?\\d+)(\\w?)");
    if (re.exactMatch(s)) {
      int d     = re.cap(1).toInt();
      QString c = re.cap(2);
      TFrameId fid;
      if (c.length() == 1)
#if QT_VERSION >= 0x050500
        fid = TFrameId(d, c[0].unicode());
#else
        fid = TFrameId(d, c[0].toAscii());
#endif
      else
        fid = TFrameId(d);
      err   = "";
      return fid;
    }
  }
  err = QObject::tr("Argument '%1' does not look like a FrameId")
            .arg(arg.toString());
  return TFrameId();
}

TImageP Level::getImg(const TFrameId &fid) {
  if (m_sl)
    return m_sl->getFrame(fid, false);
  else
    return TImageP();
}

QScriptValue Level::getFrame(const QScriptValue &fidArg) {
  if (getFrameCount() == 0)
    return context()->throwError("An empty level has no frames");
  QString err;
  TFrameId fid = getFid(fidArg, err);
  if (err != "") return context()->throwError(err);

  TImageP content = m_sl->getFrame(fid, false);
  if (content) {
    Image *img = new Image(content.getPointer());
    return create(img);
  } else {
    return QScriptValue();
  }
}

QScriptValue Level::getFrameByIndex(const QScriptValue &indexArg) {
  if (getFrameCount() == 0)
    return context()->throwError("An empty level has no frames");
  if (!indexArg.isNumber()) {
    return context()->throwError(
        tr("frame index (%1) must be a number").arg(indexArg.toString()));
  }
  int index = indexArg.toInteger();
  if (index < 0 || index >= getFrameCount()) {
    return context()->throwError(tr("frame index (%1) is out of range (0-%2)")
                                     .arg(index)
                                     .arg(getFrameCount() - 1));
  }
  TFrameId fid    = m_sl->index2fid(index);
  TImageP content = m_sl->getFrame(fid, false);
  if (content) {
    Image *img = new Image(content.getPointer());
    return create(img);
  } else {
    return QScriptValue();
  }
}

// TODO: chiamare setFrame(const TFrameId &fid, const TImageP &img)
QScriptValue Level::setFrame(const QScriptValue &fidArg,
                             const QScriptValue &imageArg) {
  QString err;
  TFrameId fid = getFid(fidArg, err);
  if (err != "") return context()->throwError(err);
  Image *img = qscriptvalue_cast<Image *>(imageArg);
  if (!img) {
    return context()->throwError(
        tr("second argument (%1) is not an image").arg(imageArg.toString()));
  }

  QString imgType = img->getType();
  int levelType   = NO_XSHLEVEL;
  if (imgType == "ToonzRaster")
    levelType = TZP_XSHLEVEL;
  else if (imgType == "Raster")
    levelType = OVL_XSHLEVEL;
  else if (imgType == "Vector")
    levelType = PLI_XSHLEVEL;
  else {
    return context()->throwError(
        tr("can not insert a %1 image into a level").arg(imgType));
  }

  if (m_type == NO_XSHLEVEL) {
    m_type        = levelType;
    TXshLevel *xl = m_scene->createNewLevel(levelType);
    m_sl          = xl->getSimpleLevel();
    m_sl->addRef();
    m_sl->setPalette(img->getImg()->getPalette());
    if (levelType != PLI_XSHLEVEL) {
      LevelProperties *lprop = m_sl->getProperties();
      lprop->setDpiPolicy(LevelProperties::DP_ImageDpi);
      int xres   = img->getWidth();
      int yres   = img->getHeight();
      double dpi = img->getDpi();
      lprop->setDpi(dpi);
      lprop->setImageDpi(TPointD(dpi, dpi));
      lprop->setImageRes(TDimension(xres, yres));
      // lprop->setHasAlpha(true);
    }
  } else if (m_type != levelType) {
    return context()->throwError(tr("can not insert a %1 image to a %2 level")
                                     .arg(imgType)
                                     .arg(getType()));
  }
  if (m_sl->getFrameCount() == 0) m_sl->setPalette(img->getImg()->getPalette());

  m_sl->setFrame(fid, img->getImg());
  m_sl->setDirtyFlag(true);
  return context()->thisObject();
}

QScriptValue Level::getFrameIds() {
  QList<TFrameId> fids;
  getFrameIds(fids);
  QScriptValue result = engine()->newArray();
  quint32 index       = 0;
  foreach (TFrameId fid, fids) {
    QString fidStr = QString::fromStdString(fid.expand());
    result.setProperty(index++, fidStr);
  }
  return result;
}

void Level::getFrameIds(QList<TFrameId> &result) {
  if (getFrameCount() > 0) {
    std::vector<TFrameId> fids;
    m_sl->getFids(fids);
    for (std::vector<TFrameId>::iterator it = fids.begin(); it != fids.end();
         ++it) {
      result.append(*it);
    }
  }
}

int Level::setFrame(const TFrameId &fid, const TImageP &img) {
  TImage::Type imgType = img->getType();
  int levelType        = NO_XSHLEVEL;
  if (imgType == TImage::TOONZ_RASTER)
    levelType = TZP_XSHLEVEL;
  else if (imgType == TImage::RASTER)
    levelType = OVL_XSHLEVEL;
  else if (imgType == TImage::VECTOR)
    levelType = PLI_XSHLEVEL;
  else {
    return -1;
  }

  if (m_type == NO_XSHLEVEL) {
    m_type        = levelType;
    TXshLevel *xl = m_scene->createNewLevel(levelType);
    m_sl          = xl->getSimpleLevel();
    m_sl->addRef();
    m_sl->setPalette(img->getPalette());
    if (levelType != PLI_XSHLEVEL) {
      LevelProperties *lprop = m_sl->getProperties();
      lprop->setDpiPolicy(LevelProperties::DP_ImageDpi);
      int xres = 0, yres = 0;
      double dpix = 0, dpiy = 0;
      if (TRasterImageP ri = img) {
        if (ri->getRaster()) {
          TDimension size = ri->getRaster()->getSize();
          xres            = size.lx;
          yres            = size.ly;
          ri->getDpi(dpix, dpiy);
        }
      } else if (TToonzImageP ti = img) {
        if (ti->getRaster()) {
          TDimension size = ri->getRaster()->getSize();
          xres            = size.lx;
          yres            = size.ly;
          ri->getDpi(dpix, dpiy);
        }
      }
      lprop->setDpi(dpix);
      lprop->setImageDpi(TPointD(dpix, dpiy));
      lprop->setImageRes(TDimension(xres, yres));
      // lprop->setHasAlpha(true);
    }
  } else if (m_type != levelType) {
    return -2;
  }
  if (m_sl->getFrameCount() == 0) m_sl->setPalette(img->getPalette());
  m_sl->setFrame(fid, img);
  m_sl->setDirtyFlag(true);
  return 1;
}

}  // namespace TScriptBinding
