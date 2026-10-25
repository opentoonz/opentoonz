

#include "toonz/fullcolorpalette.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "tsystem.h"
#include "tstream.h"
#include "tpalette.h"

#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>

namespace {

void writePaletteFile(const TFilePath &path, TPalette *palette) {
  if (!TSystem::touchParentDir(path))
    throw TSystemException(path, "Could not create the palette folder.");

  QTemporaryDir temporary(path.getQString() + ".XXXXXX");
  if (!temporary.isValid())
    throw TSystemException(path, temporary.errorString().toStdString());
  // QTemporaryFile keeps its native handle open even after close(), which
  // prevents the Windows native stream from reopening the snapshot for writing.
  const QString snapshot = temporary.filePath("palette.tpl");
  {
    TOStream os{TFilePath(snapshot)};
    if (!os)
      throw TSystemException(path, "Could not write the palette snapshot.");
    os << palette;
    if (!os.checkStatus())
      throw TSystemException(path, "Could not finish the palette snapshot.");
  }

  QFile input(snapshot);
  if (!input.open(QIODevice::ReadOnly))
    throw TSystemException(path, input.errorString().toStdString());
  const QByteArray data = input.readAll();
  if (input.error() != QFileDevice::NoError)
    throw TSystemException(path, input.errorString().toStdString());

  QSaveFile output(path.getQString());
  if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() ||
      !output.commit())
    throw TSystemException(path, output.errorString().toStdString());
}

}  // namespace

//==================================================
//
// FullColorPalette
//
//==================================================

FullColorPalette::FullColorPalette()
    : m_fullcolorPalettePath("+palettes\\Raster_Drawing_Palette.tpl")
    , m_palette(0) {}

//----------------------------------------------------

FullColorPalette *FullColorPalette::instance() {
  static FullColorPalette _instance;
  return &_instance;
}

//----------------------------------------------------

FullColorPalette::~FullColorPalette() { clear(); }

//----------------------------------------------------

void FullColorPalette::clear() {
  if (m_palette) m_palette->release();
  m_palette = 0;
}

//----------------------------------------------------

bool FullColorPalette::getDirtyFlag() const {
  return m_palette && m_palette->getDirtyFlag();
}

//----------------------------------------------------

TPalette *FullColorPalette::getPalette(ToonzScene *scene) {
  if (m_palette) return m_palette;
  m_palette = new TPalette();
  m_palette->addRef();
  TFilePath fullPath = getDefaultPalettePath();
  if (scene && scene->getProject() &&
      scene->getProject()->getFolderIndex("palettes") >= 0) {
    fullPath = scene->decodeFilePath(m_fullcolorPalettePath);
    if (!TSystem::doesExistFileOrLevel(fullPath)) {
      // Read the legacy project palette, but save under the current name.
      fullPath =
          scene->decodeFilePath(TFilePath("+palettes\\fullcolorPalette.tpl"));
    }
  }
  if (!TSystem::doesExistFileOrLevel(fullPath)) {
    fullPath = getDefaultPalettePath();
  }
  if (TSystem::doesExistFileOrLevel(fullPath)) {
    try {
      TIStream is(fullPath);
      std::string tagName;
      if (!is || !is.matchTag(tagName) || tagName != "palette")
        throw TException("Invalid raster palette.");
      TPaletteP loaded(new TPalette());
      loaded->loadData(is);
      if (!is.matchEndTag() || loaded->getStyleCount() < 2 ||
          !loaded->getStylePage(0) || !loaded->getStylePage(1))
        throw TException("The raster palette is missing its base styles.");
      loaded->addRef();
      m_palette->release();
      m_palette = loaded.getPointer();
    } catch (...) {
      qWarning() << "Could not load raster palette:" << fullPath.getQString();
    }
  }
  m_palette->setPaletteName(L"Raster Drawing Palette");

  return m_palette;
}

//----------------------------------------------------

void FullColorPalette::savePalette(ToonzScene *scene) {
  if (!getDirtyFlag()) return;
  if (!scene || !scene->getProject() ||
      scene->getProject()->getFolderIndex("palettes") < 0)
    throw TSystemException("The project has no palettes folder.");

  writePaletteFile(scene->decodeFilePath(m_fullcolorPalettePath), m_palette);
  m_palette->setDirtyFlag(false);
}

//----------------------------------------------------

TFilePath FullColorPalette::getDefaultPalettePath() const {
  return ToonzFolder::getStudioPaletteFolder() +
         "Global Palettes\\Default Palettes\\Raster_Drawing_Palette.tpl";
}

//----------------------------------------------------

void FullColorPalette::saveAsDefault(TPalette *palette) {
  if (!palette) return;
  writePaletteFile(getDefaultPalettePath(), palette);
}
