

// Toonz includes
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tropcm.h"
#include "tvectorrenderdata.h"
#include "tsystem.h"
#include "tvectorgl.h"
#include "tcolorstyles.h"

// TnzCore includes
#include "tfiletype.h"
#include "tvectorbrushstyle.h"

// TnzLib includes
#include "toonz/imagestyles.h"
#include "toonz/toonzfolders.h"

// Qt includes
#include <QDir>
#include <QImage>
#include <QOffscreenSurface>
#include <QThread>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include "toonz/stylemanager.h"

#include <QVector>

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

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

//-----------------------------------------------------------------------------

QImage rasterToQImage(const TRasterP &ras, bool premultiplied = true,
                      bool mirrored = true) {
  if (TRaster32P ras32 = ras) {
    QImage image(ras->getRawData(), ras->getLx(), ras->getLy(),
                 premultiplied ? QImage::Format_ARGB32_Premultiplied
                               : QImage::Format_ARGB32);
    if (mirrored) return image.mirrored();
    return image;
  } else if (TRasterGR8P ras8 = ras) {
    QImage image(ras->getRawData(), ras->getLx(), ras->getLy(), ras->getWrap(),
                 QImage::Format_Indexed8);
    static QVector<QRgb> colorTable;
    if (colorTable.size() == 0) {
      int i;
      for (i = 0; i < 256; i++) colorTable.append(QColor(i, i, i).rgb());
    }
    image.setColorTable(colorTable);
    if (mirrored) return image.mirrored();
    return image;
  }
  return QImage();
}

}  // namespace

//********************************************************************************
//    FavoritesManager implementation
//********************************************************************************

FavoritesManager::FavoritesManager() {
  m_fpPinsToTop = ToonzFolder::getMyModuleDir() + "pintotopbrushes.txt";
  m_xxPinsToTop = false;
  loadPinsToTop();
}

//-----------------------------------------------------------------------------

FavoritesManager *FavoritesManager::instance() {
  static FavoritesManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

bool FavoritesManager::loadPinsToTop() {
  if (!TFileStatus(m_fpPinsToTop).doesExist()) return false;

  TIStream is(m_fpPinsToTop);
  if (!is) throw TException("Can't read XML");
  std::string tagName;

  if (!is.matchTag(tagName) || tagName != "PinsToTop") return false;

  m_pinsToTop.clear();
  while (!is.matchEndTag()) {
    if (!is.matchTag(tagName)) throw TException("Expected tag");
    if (tagName == "BrushIdName") {
      std::string brushname;
      is >> brushname;
      m_pinsToTop.push_back(brushname);
      if (!is.matchEndTag()) throw TException("Expected end tag");
    }
  }
  m_xxPinsToTop = false;

  return true;
}

//-----------------------------------------------------------------------------

void FavoritesManager::savePinsToTop() {
  if (!m_xxPinsToTop) return;

  TOStream os(m_fpPinsToTop);
  if (!os) throw TException("Can't write XML");

  os.openChild("PinsToTop");
  for (auto &idname : m_pinsToTop) {
    os.openChild("BrushIdName", {});
    os << idname;
    os.closeChild();
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

bool FavoritesManager::getPinToTop(std::string idname) const {
  return m_pinsToTop.contains(idname);
}

//-----------------------------------------------------------------------------

void FavoritesManager::setPinToTop(std::string idname, bool state) {
  int index = m_pinsToTop.indexOf(idname);
  if (state && index == -1) {
    m_xxPinsToTop = true;
    m_pinsToTop.append(idname);
  } else if (!state && index != -1) {
    m_xxPinsToTop = true;
    m_pinsToTop.removeAll(idname);
  }
}

//-----------------------------------------------------------------------------

void FavoritesManager::togglePinToTop(std::string idname) {
  int index = m_pinsToTop.indexOf(idname);
  if (index != -1)
    m_pinsToTop.removeAt(index);
  else
    m_pinsToTop.append(idname);
  m_xxPinsToTop = true;
}

//********************************************************************************
//    BaseStyleManager implementation
//********************************************************************************

TFilePath BaseStyleManager::s_rootPath;
BaseStyleManager::ChipData BaseStyleManager::s_emptyChipData;

BaseStyleManager::BaseStyleManager(const TFilePath &stylesFolder,
                                   QString filters, QSize chipSize)
    : m_stylesFolder(stylesFolder)
    , m_filters(filters)
    , m_chipSize(chipSize)
    , m_loaded(false)
    , m_isIndexed(false) {}

//-----------------------------------------------------------------------------

const BaseStyleManager::ChipData &BaseStyleManager::getData(int index) const {
  if (m_isIndexed) {
    return (index < 0 || index >= m_indexes.count())
               ? s_emptyChipData
               : m_chips[m_indexes[index]];
  } else {
    return (index < 0 || index >= m_chips.count()) ? s_emptyChipData
                                                   : m_chips[index];
  }
}

//-----------------------------------------------------------------------------

void BaseStyleManager::applyFilter() {
  FavoritesManager *favMan = FavoritesManager::instance();
  QList<int> indexes;

  m_indexes.clear();
  int len = m_chips.count();
  for (int i = 0; i < len; i++) {
    auto &chip = m_chips[i];
    if (chip.desc.indexOf(m_searchText, 0, Qt::CaseInsensitive) >= 0) {
      if (favMan->getPinToTop(chip.idname)) {
        chip.markPinToTop = true;
        m_indexes.append(i);
      } else {
        chip.markPinToTop = false;
        indexes.append(i);
      }
    }
  }

  bool hasPinsToTop = m_indexes.count() > 0;
  m_indexes.append(indexes);
  m_isIndexed = (m_indexes.count() != len) || hasPinsToTop;
}

//-----------------------------------------------------------------------------

TFilePath BaseStyleManager::getRootPath() { return s_rootPath; }

//-----------------------------------------------------------------------------

void BaseStyleManager::setRootPath(const TFilePath &rootPath) {
  s_rootPath = rootPath;
}

//********************************************************************************
//    StyleLoaderTask definition
//********************************************************************************

class CustomStyleManager::StyleLoaderTask final : public TThread::Runnable {
  CustomStyleManager *m_manager;
  TFilePath m_fp;
  ChipData m_data;
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
  m_data = m_manager->createPattern(m_fp, m_offScreenSurface);
}

//-----------------------------------------------------------------------------

void CustomStyleManager::StyleLoaderTask::onFinished(
    TThread::RunnableP sender) {
  // On the main thread...
  if (!m_data.image.isNull())  // Everything went ok
  {
    m_manager->m_chips.append(m_data);
    emit m_manager->patternAdded();
  }
}

//********************************************************************************
//    CustomStyleManager implementation
//********************************************************************************

CustomStyleManager::CustomStyleManager(std::string rasterIdName,
                                       std::string vectorIdName,
                                       const TFilePath &stylesFolder,
                                       QString filters, QSize chipSize)
    : BaseStyleManager(stylesFolder, filters, chipSize)
    , m_started(false)
    , m_rasterIdName(rasterIdName)
    , m_vectorIdName(vectorIdName) {
  m_executor.setMaxActiveTasks(1);
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
  for (i = 0; i < m_chips.size(); i++) {
    ChipData data = m_chips.at(i);
    for (it = fps.begin(); it != fps.end(); ++it) {
      bool isVector = (it->getType() == "pli");
      QString name  = QString::fromStdWString(it->getWideName());
      if (data.name == name && data.isVector == isVector) break;
    }

    if (it == fps.end()) {
      m_chips.removeAt(i);
      i--;
    } else
      fps.erase(it);  // The style is not new, so don't generate tasks for it
  }

  // For each (now new) file entry, generate a fetching task
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++)
    m_executor.addTask(new StyleLoaderTask(this, *it));
}

QImage CustomStyleManager::makeIcon(
    const TFilePath &path, const QSize &qChipSize,
    std::shared_ptr<QOffscreenSurface> offsurf) {
  try {
    // Fetch the level
    TLevelReaderP lr(path);
    TLevelP level = lr->loadInfo();
    if (!level || level->getFrameCount() == 0) return QImage();

    // Fetch the image of the first frame in the level
    TLevel::Iterator frameIt = level->begin();
    if (frameIt == level->end()) return QImage();
    TImageP img = lr->getFrameReader(frameIt->first)->load();

    // Process the image
    TDimension chipSize(qChipSize.width(), qChipSize.height());

    TVectorImageP vimg = img;
    TRasterImageP rimg = img;

    TRaster32P ras;

    QImage image;

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
      glContext->makeCurrent(offsurf.get());
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

      image = QImage(fb.toImage().scaled(QSize(chipSize.lx, chipSize.ly),
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
      // image = QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
      // convertRaster32ToImage(ras, &image);
      image = rasterToQImage(ras);
#endif
    } else
      assert(!"unsupported type for custom styles!");

#ifdef LINUX
    // image = QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
    // convertRaster32ToImage(ras, &image);
    image = rasterToQImage(ras);
#endif

    return image;
  } catch (...) {
  }

  return QImage();
}

CustomStyleManager::ChipData CustomStyleManager::createPattern(
    const TFilePath &path, std::shared_ptr<QOffscreenSurface> offsurf) {
  ChipData data;

  bool isVector = (path.getType() == "pli" || path.getType() == "svg");

  // Generate preview
  try {
    data.image = makeIcon(path, getChipSize(), offsurf);
  } catch (...) {
  }

  if (!data.image.isNull()) {
    data.name     = QString::fromStdWString(path.getWideName());
    data.desc     = data.name;
    data.isVector = isVector;
    if (isVector)
      data.idname = m_vectorIdName + data.name.toStdString();
    else
      data.idname = m_rasterIdName + data.name.toStdString();
    data.hash = TColorStyle::generateHash(data.idname);
  }
  return data;
}

//********************************************************************************
//    TextureStyleManager implementation
//********************************************************************************

TextureStyleManager::TextureStyleManager(const TFilePath &stylesFolder,
                                         QSize chipSize)
    : BaseStyleManager(stylesFolder, QString(), chipSize) {}

//-----------------------------------------------------------------------------

void TextureStyleManager::loadTexture(const TFilePath &fp) {
  if (fp == TFilePath()) {
    TRaster32P ras(25, 25);
    TTextureStyle::fillCustomTextureIcon(ras);
    // ras->fill(TPixel::Blue);
    ChipData customText(
        QString(""), QObject::tr("Custom Texture", "TextureStyleChooserPage"),
        rasterToQImage(ras), 4, false, ras,
        TTextureStyle::staticBrushIdName(L""));
    customText.hash = TTextureStyle::generateHash(customText.idname);
    m_chips.append(customText);
    return;
  }

  TRasterP ras;
  TImageReader::load(fp, ras);
  if (!ras || ras->getLx() < 2 || ras->getLy() < 2) return;

  TRaster32P ras32 = ras;
  if (!ras32) return;

  TDimension d(2, 2);
  while (d.lx < 256 && d.lx * 2 <= ras32->getLx()) d.lx *= 2;
  while (d.ly < 256 && d.ly * 2 <= ras32->getLy()) d.ly *= 2;

  TRaster32P texture;
  if (d == ras32->getSize())
    texture = ras32;
  else {
    texture = TRaster32P(d);
    TScale sc((double)texture->getLx() / ras32->getLx(),
              (double)texture->getLy() / ras32->getLy());
    TRop::resample(texture, ras32, sc);
  }

  QString name = QString::fromStdWString(fp.getLevelNameW());
  ChipData text(name, name, rasterToQImage(ras), 4, false, texture,
                TTextureStyle::staticBrushIdName(fp.getLevelNameW()));
  text.hash = TTextureStyle::generateHash(text.idname);

  m_chips.append(text);
}

//-----------------------------------------------------------------------------

void TextureStyleManager::loadItems() {
  m_chips.clear();
  if (getRootPath() == TFilePath()) return;

  TFilePath texturePath = getRootPath() + "textures";
  TFilePathSet fps;
  try {
    fps = TSystem::readDirectory(texturePath);
  } catch (...) {
    return;
  }
  if (fps.empty()) return;
  int count = 0;
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++)
    if (TFileType::getInfo(*it) == TFileType::RASTER_IMAGE) {
      try {
        loadTexture(*it);
        ++count;
      } catch (...) {
      }
    }
  loadTexture(TFilePath());  // custom texture

  m_loaded = true;
}

//********************************************************************************
//    MyPaintBrushStyleManager  implementation
//********************************************************************************

MyPaintBrushStyleManager::MyPaintBrushStyleManager(QSize chipSize)
    : BaseStyleManager(TFilePath(), QString(), chipSize) {}

//-----------------------------------------------------------------------------

void MyPaintBrushStyleManager::loadItems() {
  m_brushes.clear();
  m_chips.clear();
  std::set<TFilePath> brushFiles;

  TFilePathSet dirs = TMyPaintBrushStyle::getBrushesDirs();
  for (TFilePathSet::iterator i = dirs.begin(); i != dirs.end(); ++i) {
    TFileStatus fs(*i);
    if (fs.doesExist() && fs.isDirectory()) {
      TFilePathSet files = TSystem::readDirectoryTree(*i, false, true);
      for (TFilePathSet::iterator j = files.begin(); j != files.end(); ++j)
        if (j->getType() == TMyPaintBrushStyle::getBrushType())
          brushFiles.insert(*j - *i);
    }
  }

  // reserve memory to avoid reallocation
  m_brushes.reserve(brushFiles.size());
  for (std::set<TFilePath>::iterator i = brushFiles.begin();
       i != brushFiles.end(); ++i) {
    TMyPaintBrushStyle style = TMyPaintBrushStyle(*i);
    m_brushes.push_back(style);

    // Generate a QImage preview to draw the chip faster at cost of few memory
    QImage previewQImage = rasterToQImage(style.getPreview());
    QString stylePath    = style.getPath().getQString();
    m_chips.append(ChipData(stylePath, stylePath, previewQImage, 4001, false,
                            TRasterP(), style.getBrushIdName(),
                            style.getBrushIdHash()));
  }

  m_loaded = true;
}

//********************************************************************************
//    SpecialStyleManager  implementation
//********************************************************************************

SpecialStyleManager::SpecialStyleManager(QSize chipSize)
    : BaseStyleManager(TFilePath(), QString(), chipSize) {}

//-----------------------------------------------------------------------------

void SpecialStyleManager::loadItems() {
  m_chips.clear();

  std::vector<int> tags;
  TColorStyle::getAllTags(tags);

  int chipCount = 0;

  for (int j = 0; j < (int)tags.size(); j++) {
    int tagId = tags[j];
    if (tagId == 3 ||     // solid color
        tagId == 4 ||     // texture
        tagId == 100 ||   // obsolete imagepattern id
        tagId == 2000 ||  // imagepattern
        tagId == 2800 ||  // imagepattern
        tagId == 2001 ||  // cleanup
        tagId == 2002 ||  // black cleanup
        tagId == 3000 ||  // vector brush
        tagId == 4001     // mypaint brush
    )
      continue;

    TColorStyle *style = TColorStyle::create(tagId);
    if (style->isRasterStyle()) {
      delete style;
      continue;
    }
    TDimension chipSize(getChipSize().width(), getChipSize().height());
    // QImage *image = new QImage(chipSize.lx, chipSize.ly,
    // QImage::Format_RGB32); convertRaster32ToImage(style->getIcon(chipSize),
    // image);
    TRaster32P raster = style->getIcon(chipSize);
    ChipData chip(style->getDescription(), style->getDescription(),
                  rasterToQImage(raster), tagId, true, raster,
                  style->getBrushIdName(), style->getBrushIdHash());
    m_chips.append(chip);
    delete style;
  }

  m_loaded = true;
}
