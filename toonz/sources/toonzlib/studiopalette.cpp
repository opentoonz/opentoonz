

#include "toonz/studiopalette.h"
#include "tfiletype.h"
#include "tstream.h"
#include "tconvert.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include "traster.h"
#include "tsystem.h"
#include "tcolorstyles.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "tpalette.h"

#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <string.h>

//===================================================================

//-------------------------------------------------------------------
namespace {
//-------------------------------------------------------------------

TFilePath makeUniqueName(TFilePath fp) {
  if (TFileStatus(fp).doesExist() == false) return fp;
  std::wstring name = fp.getWideName();
  int index         = 2;
  int j             = name.find_last_not_of(L"0123456789");
  if (j != (int)std::wstring::npos && j + 1 < (int)name.length()) {
    index = std::stoi(name.substr(j + 1)) + 1;
    name  = name.substr(0, j + 1);
  }
  for (;;) {
    fp = fp.withName(name + std::to_wstring(index));
    if (TFileStatus(fp).doesExist() == false) return fp;
    index++;
  }
}

//-------------------------------------------------------------------

TPalette *loadPliPalette(const TFilePath &fp) {
  TLevelReaderP lr(fp);
  TLevelP level  = lr->loadInfo();
  int frameCount = level->getFrameCount();
  if (frameCount < 1) return 0;
  TPalette *palette = level->getPalette();
  if (!palette) return 0;
  return palette->clone();
}

//-------------------------------------------------------------------

TPalette *loadTplPalette(const TFilePath &fp) {
  TPersist *p = 0;
  TIStream is(fp);
  is >> p;
  TPalette *palette = dynamic_cast<TPalette *>(p);
  return palette;
}

//-------------------------------------------------------------------

TPalette *loadToonz46Palette(const TFilePath &fp) {
  TImageP pltImg;
  TImageReader::load(fp, pltImg);
  if (!pltImg) return 0;
  TRasterImageP pltRasImg(pltImg);
  if (!pltRasImg) return 0;
  TRaster32P rasPlt = pltRasImg->getRaster();
  if (!rasPlt) return 0;
  TPalette *palette = new TPalette();
  const int offset  = 0;  // FirstUserStyle-1;
  assert(rasPlt->getLy() == 2);
  rasPlt->lock();
  TPixel32 *pixelRow = rasPlt->pixels(0);
  int x;
  for (x = 1; x < rasPlt->getLx(); ++x) {
    TPixel32 color = pixelRow[x];
    int styleId    = offset + x;
    if (styleId < palette->getStyleCount())
      palette->setStyle(styleId, color);
    else {
      int j = palette->addStyle(color);
      assert(j == styleId);
    }
  }

  // aggiungo solo i colori usati (salvo il BG)

  pixelRow             = rasPlt->pixels(1);
  TPalette::Page *page = palette->getPage(0);
  for (x = 1; x < rasPlt->getLx(); ++x) {
    if (pixelRow[x].r == 255)
      page->addStyle(offset +
                     x);  // palette->addStyleToPage(offset+x, L"colors");
  }
  rasPlt->unlock();
  return palette;
}

//-------------------------------------------------------------------

std::wstring readPaletteGlobalName(TFilePath path) {
  try {
    TIStream is(path);
    if (!is) return L"";
    std::string tagName;
    if (!is.matchTag(tagName) || tagName != "palette") return L"";
    std::string name;
    if (is.getTagParam("name", name)) return ::to_wstring(name);
  } catch (...) {
  }
  return L"";
}
//-------------------------------------------------------------------

TFilePath searchPalette(TFilePath path, std::wstring paletteId) {
  TFilePathSet q;
  try {
    TSystem::readDirectory(q, path);
  } catch (...) {
  }

  for (TFilePathSet::iterator i = q.begin(); i != q.end(); ++i) {
    TFilePath fp = *i;
    if (fp.getType() == "tpl") {
      std::wstring gname = readPaletteGlobalName(fp);
      if (gname == paletteId) return fp;
    } else if (TFileStatus(fp).isDirectory()) {
      TFilePath palettePath = searchPalette(fp, paletteId);
      if (palettePath != TFilePath()) return palettePath;
    }
  }
  return TFilePath();
}

bool studioPaletteHasBeenReferred = false;

static std::map<std::wstring, TFilePath> table;

//-------------------------------------------------------------------
}  // namespace
//-------------------------------------------------------------------

//===================================================================
//
// StudioPalette
//
//-------------------------------------------------------------------

StudioPalette::StudioPalette() {
  try {
    m_root = ToonzFolder::getStudioPaletteFolder();
  } catch (...) {
    return;
  }
  if (!TFileStatus(m_root).doesExist()) {
    try {
      TSystem::mkDir(m_root);
      FolderListenerManager::instance()->notifyFolderChanged(
          m_root.getParentDir());
    } catch (...) {
    }
    try {
      TSystem::mkDir(getLevelPalettesRoot());
      FolderListenerManager::instance()->notifyFolderChanged(
          getLevelPalettesRoot().getParentDir());
    } catch (...) {
    }
  }
}

//-------------------------------------------------------------------

StudioPalette::~StudioPalette() {}

//-------------------------------------------------------------------

bool StudioPalette::m_enabled = true;

//-------------------------------------------------------------------

void StudioPalette::enable(bool enabled) {
  assert(studioPaletteHasBeenReferred == false);
  m_enabled = enabled;
}

//-------------------------------------------------------------------

StudioPalette *StudioPalette::instance() {
  static StudioPalette _instance;
  studioPaletteHasBeenReferred = true;
  assert(m_enabled);
  return &_instance;
}

//-------------------------------------------------------------------

TFilePath StudioPalette::getLevelPalettesRoot() {
  return m_root + "Global Palettes";
}

//-------------------------------------------------------------------

TFilePath StudioPalette::getProjectPalettesRoot() {
  TProjectP p          = TProjectManager::instance()->getCurrentProject();
  TFilePath folderName = p->getFolder(TProject::Palettes);
  if (folderName.isEmpty()) return TFilePath();
  if (folderName.isAbsolute()) return folderName;
  return p->getProjectFolder() + folderName;
}

//-------------------------------------------------------------------

static bool loadRefImg(TPalette *palette, TFilePath dir) {
  assert(palette);
  TFilePath fp = palette->getRefImgPath();
  if (fp == TFilePath() || !TSystem::doesExistFileOrLevel(fp)) return false;
  if (!fp.isAbsolute()) fp = dir + fp;
  TLevelReaderP lr(fp);
  if (!lr) return false;
  TLevelP level = lr->loadInfo();
  if (!level || level->getFrameCount() == 0) return false;
  TLevel::Iterator it = level->begin();
  TImageP img         = lr->getFrameReader(it->first)->load();
  if (!img) return false;
  img->setPalette(0);
  palette->setRefImg(img);
  return true;
}

//-------------------------------------------------------------------

TPalette *StudioPalette::getPalette(const TFilePath &path,
                                    bool loadRefImgFlag) {
  try {
    if (path.getType() != "tpl") return 0;
    TPalette *palette = load(path);
    if (!palette) return 0;
    if (loadRefImgFlag) loadRefImg(palette, path.getParentDir());
    // palette->addRef(); // ci va??
    return palette;
  } catch (...) {
    return 0;
  }
}

//-------------------------------------------------------------------

void StudioPalette::movePalette(const TFilePath &dstPath,
                                const TFilePath &srcPath) {
  try {
    // do not allow overwrite palette
    TSystem::renameFile(dstPath, srcPath, false);
  } catch (...) {
    throw;
  }
  std::wstring id = readPaletteGlobalName(dstPath);
  table.erase(id);
  FolderListenerManager::instance()->notifyFolderChanged(
      dstPath.getParentDir());
  notifyMove(dstPath, srcPath);
}

//-------------------------------------------------------------------

int StudioPalette::getChildren(std::vector<TFilePath> &fps,
                               const TFilePath &folderPath) {
  TFilePathSet q;
  if (TFileStatus(folderPath).isDirectory()) {
    try {
      TSystem::readDirectory(q, folderPath, false, false);
    } catch (...) {
    }
  }

  // put the folders above the palette items
  std::vector<TFilePath> palettes;
  for (TFilePathSet::iterator i = q.begin(); i != q.end(); ++i) {
    if (isFolder(*i))
      fps.push_back(*i);
    else if (isPalette(*i))
      palettes.push_back(*i);
  }
  if (!palettes.empty()) {
    fps.reserve(fps.size() + palettes.size());
    std::copy(palettes.begin(), palettes.end(), std::back_inserter(fps));
  }
  //  fps.push_back(m_root+"butta.tpl");
  return fps.size();
}

//-------------------------------------------------------------------

int StudioPalette::getChildCount(const TFilePath &folderPath) {
  TFilePathSet q;
  try {
    TSystem::readDirectory(q, folderPath);
  } catch (...) {
  }
  return q.size();
}

//-------------------------------------------------------------------

bool StudioPalette::isFolder(const TFilePath &path) {
  return TFileStatus(path).isDirectory();
}

//-------------------------------------------------------------------

bool StudioPalette::isReadOnly(const TFilePath &path) {
  return !TFileStatus(path).isWritable();
}

//-------------------------------------------------------------------

bool StudioPalette::isPalette(const TFilePath &path) {
  return path.getType() == "tpl";
}

//-------------------------------------------------------------------
/*! check if the palette is studio palette or level palette in order to separate
 * icons in the StudioPaletteTree.
*/
bool StudioPalette::hasGlobalName(const TFilePath &path) {
  return (readPaletteGlobalName(path) != L"");
}

//-------------------------------------------------------------------

bool StudioPalette::isLevelPalette(const TFilePath &path) {
  TPalette *palette = getPalette(path);
  if (!palette) return false;
  bool ret = !palette->isCleanupPalette();
  delete palette;
  return ret;
}

//-------------------------------------------------------------------

TFilePath StudioPalette::createFolder(const TFilePath &parentFolderPath) {
  TFilePath path = makeUniqueName(parentFolderPath + "new folder");
  try {
    TSystem::mkDir(path);
  } catch (...) {
    return TFilePath();
  }
  FolderListenerManager::instance()->notifyFolderChanged(parentFolderPath);
  notifyTreeChange();
  return path;
}

//-------------------------------------------------------------------

void StudioPalette::createFolder(const TFilePath &parentFolderPath,
                                 std::wstring name) {
  TFilePath fp = parentFolderPath + name;
  if (TFileStatus(fp).doesExist()) return;
  try {
    TSystem::mkDir(fp);
  } catch (...) {
    return;
  }
  FolderListenerManager::instance()->notifyFolderChanged(parentFolderPath);
  notifyTreeChange();
}

//-------------------------------------------------------------------

TFilePath StudioPalette::createPalette(const TFilePath &folderPath,
                                       std::string name) {
  TPalette *palette    = 0;
  if (name == "") name = "new palette";
  palette              = new TPalette();
  TFilePath fp         = makeUniqueName(folderPath + (name + ".tpl"));
  time_t ltime;
  time(&ltime);
  std::wstring gname = std::to_wstring(ltime) + L"_" + std::to_wstring(rand());
  palette->setGlobalName(gname);
  setStylesGlobalNames(palette);
  save(fp, palette);
  delete palette;
  notifyTreeChange();
  return fp;
}

//-------------------------------------------------------------------

void StudioPalette::setPalette(const TFilePath &palettePath,
                               const TPalette *plt, bool notifyPaletteChanged) {
  assert(palettePath.getType() == "tpl");
  TPalette *palette = plt->clone();
  palette->setIsLocked(plt->isLocked());
  palette->addRef();
  std::wstring pgn = palette->getGlobalName();
  if (TFileStatus(palettePath).doesExist())
    pgn = readPaletteGlobalName(palettePath);
  palette->setGlobalName(pgn);
  setStylesGlobalNames(palette);
  save(palettePath, palette);
  palette->release();
  if (notifyPaletteChanged) notifyPaletteChange(palettePath);
}

//-------------------------------------------------------------------

void StudioPalette::deletePalette(const TFilePath &palettePath) {
  assert(palettePath.getType() == "tpl");
  try {
    TSystem::deleteFile(palettePath);
  } catch (...) {
    return;
  }
  notifyTreeChange();
}

//-------------------------------------------------------------------

void StudioPalette::deleteFolder(const TFilePath &path) {
  try {
    TSystem::rmDirTree(path);
  } catch (...) {
  }
  notifyTreeChange();
}

//-------------------------------------------------------------------

TFilePath StudioPalette::importPalette(const TFilePath &dstFolder,
                                       const TFilePath &srcPath) {
  TPaletteP palette;
  std::string ext = srcPath.getType();
  try {
    if (ext == "plt")
      palette = loadToonz46Palette(srcPath);
    else if (ext == "pli")
      palette = loadPliPalette(srcPath);
    else if (ext == "tpl")
      palette = loadTplPalette(srcPath);
  } catch (...) {
  }

  if (!palette) return TFilePath();
  std::wstring name = srcPath.getWideName();

  assert(!palette->isCleanupPalette());
  //    convertToLevelPalette(palette.getPointer());

  TFilePath fp = makeUniqueName(dstFolder + (name + L".tpl"));
  time_t ltime;
  time(&ltime);
  std::wstring gname = std::to_wstring(ltime) + L"_" + std::to_wstring(rand());
  palette->setGlobalName(gname);
  setStylesGlobalNames(palette.getPointer());
  TSystem::touchParentDir(fp);
  save(fp, palette.getPointer());
  notifyTreeChange();
  return fp;
}

//-------------------------------------------------------------------

// TFilePath StudioPalette::getRefImage(const TFilePath palette)
//{
//  return palette.withType("pli");
//}

//-------------------------------------------------------------------

static void foobar(std::wstring paletteId) { table.erase(paletteId); }

TFilePath StudioPalette::getPalettePath(std::wstring paletteId) {
  std::map<std::wstring, TFilePath>::iterator it = table.find(paletteId);
  if (it != table.end()) return it->second;
  TFilePath fp = searchPalette(m_root, paletteId);
  if (fp == TFilePath()) {
    fp = searchPalette(getProjectPalettesRoot(), paletteId);
  }
  table[paletteId] = fp;
  return fp;
}

//-------------------------------------------------------------------

TPalette *StudioPalette::getPalette(std::wstring paletteId) {
  TFilePath palettePath = getPalettePath(paletteId);
  if (palettePath != TFilePath())
    return getPalette(palettePath);
  else
    return 0;
}

//-------------------------------------------------------------------

TColorStyle *StudioPalette::getStyle(std::wstring styleId) { return 0; }

//-------------------------------------------------------------------

std::pair<TFilePath, int> StudioPalette::getSourceStyle(TColorStyle *cs) {
  std::pair<TFilePath, int> ret(TFilePath(), -1);
  if (!cs) return ret;
  std::wstring gname = cs->getGlobalName();
  if (gname == L"") return ret;
  int k = gname.find_first_of(L'-', 1);
  if (k == (int)std::wstring::npos) return ret;
  std::wstring paletteId = gname.substr(1, k - 1);
  ret.first              = getPalettePath(paletteId) - m_root;
  ret.second             = std::stoi(gname.substr(k + 1));
  return ret;
}

//-------------------------------------------------------------------
/*! return if any style in the palette is changed
*/
bool StudioPalette::updateLinkedColors(TPalette *palette) {
  bool paletteIsChanged = false;
  std::map<std::wstring, TPaletteP> table;

  for (int i = 0; i < palette->getStyleCount(); i++) {
    TColorStyle *cs    = palette->getStyle(i);
    std::wstring gname = cs->getGlobalName();
    if (gname == L"" || gname[0] != L'+') continue;
    int k = gname.find_first_of(L'-', 1);
    if (k == (int)std::wstring::npos) continue;
    std::wstring paletteId = gname.substr(1, k - 1);
    std::map<std::wstring, TPaletteP>::iterator it;
    it                  = table.find(paletteId);
    TPalette *spPalette = 0;
    if (it == table.end()) {
      spPalette = StudioPalette::instance()->getPalette(paletteId);
      if (!spPalette) continue;
      table[paletteId] = spPalette;
      // spPalette->release();
      assert(spPalette->getRefCount() == 1);
    } else
      spPalette = it->second.getPointer();

    int j = std::stoi(gname.substr(k + 1));
    if (spPalette && 0 <= j && j < spPalette->getStyleCount()) {
      TColorStyle *spStyle = spPalette->getStyle(j);
      assert(spStyle);
      spStyle = spStyle->clone();
      spStyle->setGlobalName(gname);

      // put the style name in the studio palette into the original name
      spStyle->setOriginalName(spStyle->getName());
      //.. and keep the style name unchanged
      spStyle->setName(cs->getName());

      palette->setStyle(i, spStyle);

      paletteIsChanged = true;
    }
  }
  return paletteIsChanged;
}

//-------------------------------------------------------------------

void StudioPalette::setStylesGlobalNames(TPalette *palette) {
  for (int i = 0; i < palette->getStyleCount(); i++) {
    TColorStyle *cs = palette->getStyle(i);
    // set global name only to the styles of which the global name is empty
    if (cs->getGlobalName() == L"") {
      std::wstring gname =
          L"-" + palette->getGlobalName() + L"-" + std::to_wstring(i);
      cs->setGlobalName(gname);
    }
  }
}

//-------------------------------------------------------------------

void StudioPalette::save(const TFilePath &path, TPalette *palette) {
  TOStream os(path);
  std::map<std::string, std::string> attr;
  attr["name"] = ::to_string(palette->getGlobalName());
  os.openChild("palette", attr);
  palette->saveData(os);
  os.closeChild();
}

//-------------------------------------------------------------------

TPalette *StudioPalette::load(const TFilePath &path) {
  try {
    TIStream is(path);
    if (!is) return 0;
    std::string tagName;
    if (!is.matchTag(tagName) || tagName != "palette") return 0;
    std::string gname;
    is.getTagParam("name", gname);
    TPalette *palette = new TPalette();
    palette->loadData(is);
    palette->setGlobalName(::to_wstring(gname));
    is.matchEndTag();
    palette->setPaletteName(path.getWideName());
    return palette;
  } catch (...) {
    return 0;
  }
}

//-------------------------------------------------------------------

void StudioPalette::addListener(Listener *listener) {
  if (std::find(m_listeners.begin(), m_listeners.end(), listener) ==
      m_listeners.end())
    m_listeners.push_back(listener);
}

//-------------------------------------------------------------------

void StudioPalette::removeListener(Listener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}

//-------------------------------------------------------------------

void StudioPalette::notifyTreeChange() {
  for (std::vector<Listener *>::iterator it = m_listeners.begin();
       it != m_listeners.end(); ++it)
    (*it)->onStudioPaletteTreeChange();
}

//-------------------------------------------------------------------

void StudioPalette::notifyMove(const TFilePath &dstPath,
                               const TFilePath &srcPath) {
  for (std::vector<Listener *>::iterator it = m_listeners.begin();
       it != m_listeners.end(); ++it)
    (*it)->onStudioPaletteMove(dstPath, srcPath);
}

//-------------------------------------------------------------------

void StudioPalette::notifyPaletteChange(const TFilePath &palette) {
  for (std::vector<Listener *>::iterator it = m_listeners.begin();
       it != m_listeners.end(); ++it)
    (*it)->onStudioPaletteChange(palette);
}
