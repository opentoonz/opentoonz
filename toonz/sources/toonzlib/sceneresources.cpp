

#include "toonz/sceneresources.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/namebuilder.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/preferences.h"
#include "tpalette.h"

#include "tmsgcore.h"

#include "tconvert.h"
#include "tlogger.h"
#include "tsystem.h"

namespace {
//=============================================================================

// se path e' della forma +folder/<oldSavePath>/name.type
// allora sostituisce oldSavePath con newSavePath e ritorna true

bool changeSavePath(TFilePath &path, TFilePath oldSavePath,
                    TFilePath newSavePath) {
  if (oldSavePath == newSavePath) return false;
  TFilePath fp = path.getParentDir();
  std::wstring head;
  TFilePath tail;
  fp.split(head, tail);
  if (head != L"" && tail == oldSavePath) {
    path = path.withParentDir(TFilePath(head) + newSavePath);
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

// From ../../../filename#type.psd to ../../../filename.psd
TFilePath restorePsdPath(const TFilePath &fp) {
  QString path = QString::fromStdWString(fp.getWideString());
  if (fp.getType() != "psd" || !path.contains("#")) return fp;
  int from = path.indexOf("#");
  int to   = path.lastIndexOf(".");
  path.remove(from, to - from);
  return TFilePath(path.toStdWString());
}

//-----------------------------------------------------------------------------

bool makePathUnique(ToonzScene *scene, TFilePath &path) {
  std::wstring name = path.getWideName();
  int id            = 2;
  int i             = name.length() - 1;
  int num = 0, p = 1;
  while (i >= 0 && L'0' <= name[i] && name[i] <= L'9') {
    num += p * (name[i] - L'0');
    p *= 10;
    i--;
  }
  if (i >= 0 && name[i] == L'_') {
    id   = num + 1;
    name = name.substr(0, i);
  }

  bool ret = false;
  while (TSystem::doesExistFileOrLevel(scene->decodeFilePath(path))) {
    ret  = true;
    path = path.withName(name + L"_" + std::to_wstring(id));
    id++;
  }
  return ret;
}

//-----------------------------------------------------------------------------

bool getCollectedPath(ToonzScene *scene, TFilePath &path) {
  if (!path.isAbsolute() || path.getWideString()[0] == L'+') return false;

  TFilePath collectedPath = scene->getImportedLevelPath(path);
  if (path == collectedPath) return false;

  TFilePath actualCollectedPath = scene->decodeFilePath(collectedPath);

  if (makePathUnique(scene, actualCollectedPath))
    collectedPath = collectedPath.withName(actualCollectedPath.getName());

  path = collectedPath;
  return true;
}

}  // namespace

//=============================================================================
//
// ResourceImportStrategy
//
//-----------------------------------------------------------------------------

ResourceImportStrategy::ResourceImportStrategy(int strategy)
    : m_childFolderEnabled(false), m_strategy(strategy) {
  setChildFolderEnabled(Preferences::instance()->isSubsceneFolderEnabled());
}

//-----------------------------------------------------------------------------

TFilePath ResourceImportStrategy::process(ToonzScene *scene,
                                          ToonzScene *srcScene,
                                          TFilePath srcPath) {
  TFilePath srcActualPath = srcScene->decodeFilePath(srcPath);
  if (!scene->isExternPath(srcActualPath) || m_strategy == DONT_IMPORT)
    return srcPath;

  TFilePath dstPath;
  if (srcPath.getWideString().find(L'+') == 0)
    dstPath = srcPath;
  else
    dstPath = scene->getImportedLevelPath(srcPath);
  TFilePath actualDstPath = scene->decodeFilePath(dstPath);
  assert(actualDstPath != TFilePath());

  if (m_strategy == IMPORT_AND_OVERWRITE) {
    // bool overwritten = false;
    if (TSystem::doesExistFileOrLevel(actualDstPath)) {
      TSystem::removeFileOrLevel(actualDstPath);
      //  overwritten = true;
    }
    if (TSystem::doesExistFileOrLevel(srcPath))
      TXshSimpleLevel::copyFiles(actualDstPath, srcPath);

    return dstPath;
  } else if (m_strategy == IMPORT_AND_RENAME) {
    std::wstring levelName    = srcPath.getWideName();
    TLevelSet *parentLevelSet = scene->getLevelSet();
    NameModifier nm(levelName);
    std::wstring newName;
    for (;;) {
      newName = nm.getNext();
      if (!parentLevelSet->hasLevel(newName)) break;
    }

    dstPath       = dstPath.withName(newName);
    actualDstPath = scene->decodeFilePath(dstPath);

    if (TSystem::doesExistFileOrLevel(actualDstPath))
      TSystem::removeFileOrLevel(actualDstPath);

    if (TSystem::doesExistFileOrLevel(srcActualPath)) {
      TXshSimpleLevel::copyFiles(actualDstPath, srcActualPath);
    }
    return dstPath;
  }
  return srcPath;
}

//=============================================================================
//
// SceneResource
//
//-----------------------------------------------------------------------------

SceneResource::SceneResource(ToonzScene *scene)
    : m_scene(scene)
    , m_untitledScene(scene->isUntitled())
    , m_oldSavePath(scene->getSavePath()) {}

//-----------------------------------------------------------------------------

SceneResource::~SceneResource() {}

//-----------------------------------------------------------------------------

void SceneResource::updatePath(TFilePath &fp) {
  if (m_untitledScene)
    changeSavePath(fp, m_oldSavePath, m_scene->getSavePath());
}

//=============================================================================
//
// SceneLevel
//
//-----------------------------------------------------------------------------

SceneLevel::SceneLevel(ToonzScene *scene, TXshSimpleLevel *sl)
    : SceneResource(scene)
    , m_sl(sl)
    , m_oldPath(sl->getPath())
    , m_oldActualPath(scene->decodeFilePath(sl->getPath()))
    , m_oldScannedPath(sl->getScannedPath())
    , m_oldRefImgPath()
    , m_oldActualRefImgPath() {
  if (m_oldScannedPath != TFilePath())
    m_oldActualScannedPath = m_scene->decodeFilePath(m_oldScannedPath);
  if ((sl->getPath().getType() == "tlv" || sl->getPath().getType() == "pli") &&
      sl->getPalette()) {
    m_oldRefImgPath       = sl->getPalette()->getRefImgPath();
    m_oldActualRefImgPath = m_scene->decodeFilePath(m_oldRefImgPath);
  }
}

//-----------------------------------------------------------------------------

void SceneLevel::save() {
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  TFilePath actualFp      = m_scene->decodeFilePath(fp);
  actualFp                = restorePsdPath(actualFp);
  TFilePath oldActualPath = restorePsdPath(m_oldActualPath);
  assert(actualFp.getWideString() == L"" ||
         actualFp.getWideString()[0] != L'+');
  if (actualFp != oldActualPath ||
      !TSystem::doesExistFileOrLevel(oldActualPath) ||
      m_sl->getProperties()->getDirtyFlag() ||
      (m_sl->getPalette() && m_sl->getPalette()->getDirtyFlag())) {
    try {
      TSystem::touchParentDir(actualFp);
      if (actualFp != oldActualPath &&
          TSystem::doesExistFileOrLevel(oldActualPath) &&
          m_sl->getProperties()->getDirtyFlag() == false &&
          (!m_sl->getPalette() ||
           (m_sl->getPalette() &&
            m_sl->getPalette()->getDirtyFlag() == false))) {
        try {
          TXshSimpleLevel::copyFiles(actualFp, oldActualPath);
        } catch (...) {
        }
        // Must NOT KEEP FRAMES, it generate a level frames bind necessary to
        // imageBuilder path refresh.
        m_sl->setPath(fp, false);
      } else {
        try {
          m_sl->save(actualFp, oldActualPath);
        } catch (...) {
          throw;
        }
        if ((actualFp.getType() == "tlv" || actualFp.getType() == "pli") &&
            actualFp != oldActualPath && m_oldRefImgPath != TFilePath()) {
          // Devo preoccuparmi dell'eventuale livello colormodel
          TFilePath actualRefImagPath =
              m_scene->decodeFilePath(m_oldRefImgPath);
          TFilePath actualRefImagPathTpl = actualRefImagPath.withType("tpl");
          TFilePath oldRefImagPathTpl = m_oldActualRefImgPath.withType("tpl");
          TSystem::copyFile(actualRefImagPath, m_oldActualRefImgPath);
          if (actualRefImagPath.getType() == "tlv")
            TSystem::copyFile(actualRefImagPathTpl, oldRefImagPathTpl);
        }

        if (actualFp.getType() == "tif" || actualFp.getType() == "tiff" ||
            actualFp.getType() == "tga" || actualFp.getType() == "tzi") {
          TFilePath clnin = oldActualPath.withNoFrame().withType("cln");
          if (TSystem::doesExistFileOrLevel(clnin))
            TSystem::copyFile(actualFp.withNoFrame().withType("cln"), clnin);
        }
      }
      // Se il livello e' tlv verifico se esiste il corrispondente unpainted ed
      // in caso affermativo lo copio.
      // Questo controllo viene fatto qui e non nella copia o nel salvataggio
      // del livello perche' in generale
      // non si vuole che il livello unpainted venga copiato con il livello.
      if (actualFp.getType() == "tlv") {
        TFilePath oldUnpaintedLevelPath =
            oldActualPath.getParentDir() + "nopaint\\" +
            TFilePath(oldActualPath.getName() + "_np." +
                      oldActualPath.getType());
        TFilePath unpaintedLevelPath =
            actualFp.getParentDir() + "nopaint\\" +
            TFilePath(actualFp.getName() + "_np." + actualFp.getType());
        if (TSystem::doesExistFileOrLevel(oldUnpaintedLevelPath) &&
            !TSystem::doesExistFileOrLevel(unpaintedLevelPath) &&
            TSystem::touchParentDir(unpaintedLevelPath))
          TSystem::copyFile(unpaintedLevelPath, oldUnpaintedLevelPath);
        TFilePath oldUnpaintedPalettePath =
            oldUnpaintedLevelPath.withType("tpl");
        TFilePath unpaintedPalettePath = unpaintedLevelPath.withType("tpl");
        if (TSystem::doesExistFileOrLevel(oldUnpaintedPalettePath) &&
            !TSystem::doesExistFileOrLevel(unpaintedPalettePath) &&
            TSystem::touchParentDir(unpaintedPalettePath))
          TSystem::copyFile(unpaintedPalettePath, oldUnpaintedPalettePath);
      }
    } catch (...) {
    }
  }
  fp = m_oldScannedPath;
  if (fp != TFilePath()) {
    SceneResource::updatePath(fp);
    actualFp = m_scene->decodeFilePath(fp);
    if (actualFp != m_oldActualScannedPath &&
        TSystem::doesExistFileOrLevel(m_oldActualScannedPath)) {
      try {
        TSystem::touchParentDir(actualFp);
        TSystem::copyFileOrLevel_throw(actualFp, m_oldActualScannedPath);
        m_sl->clearFrames();
        m_sl->load();
      } catch (...) {
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SceneLevel::updatePath() {
  if (!m_untitledScene) return;
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  m_sl->setPath(fp, true);
  fp = m_oldScannedPath;
  SceneResource::updatePath(fp);
  m_sl->setScannedPath(fp);
}

//-----------------------------------------------------------------------------

void SceneLevel::rollbackPath() {
  if (!m_untitledScene) return;
  m_sl->setPath(m_oldPath, true);
  m_sl->setScannedPath(m_oldScannedPath);
}

//-----------------------------------------------------------------------------

bool SceneLevel::isDirty() {
  if (m_sl->getProperties()->getDirtyFlag() ||
      (m_sl->getPalette() && m_sl->getPalette()->getDirtyFlag()))
    return true;
  else
    return false;
}
//-----------------------------------------------------------------------------

QStringList SceneLevel::getResourceName() {
  QStringList ret;
  QString string;
  bool levelIsDirty = false;
  if (m_sl->getProperties()->getDirtyFlag()) {
    string += QString::fromStdString(m_sl->getPath().getLevelName());
    levelIsDirty = true;
  }
  if (m_sl->getPalette() && m_sl->getPalette()->getDirtyFlag()) {
    QString paletteName =
        QString::fromStdWString(m_sl->getPalette()->getPaletteName());
    if (m_sl->getType() & FULLCOLOR_TYPE) {
      if (levelIsDirty) ret << string;
      ret << paletteName + ".tpl";
    } else {
      if (levelIsDirty) string += " and ";
      if (m_sl->getPath().getType() == "pli")
        string += paletteName + ".pli (palette)";
      else
        string += paletteName + ".tpl";
      ret << string;
    }
  } else if (levelIsDirty)
    ret << string;

  return ret;
}

//=============================================================================
//
// ScenePalette
//
//-----------------------------------------------------------------------------

ScenePalette::ScenePalette(ToonzScene *scene, TXshPaletteLevel *pl)
    : SceneResource(scene)
    , m_pl(pl)
    , m_oldPath(pl->getPath())
    , m_oldActualPath(scene->decodeFilePath(pl->getPath())) {}

//-----------------------------------------------------------------------------

void ScenePalette::save() {
  assert(m_oldPath != TFilePath());
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  TFilePath actualFp = m_scene->decodeFilePath(fp);
  try {
    TSystem::touchParentDir(actualFp);
    if (actualFp != m_oldActualPath &&
        TSystem::doesExistFileOrLevel(m_oldActualPath)) {
      TSystem::copyFile(actualFp, m_oldActualPath);
    }
    m_pl->save();  // actualFp non so perche' era cosi'
  } catch (...) {
    TLogger::error() << "Can't save " << actualFp;
  }
}

//-----------------------------------------------------------------------------

void ScenePalette::updatePath() {
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  if (fp != m_oldPath) m_pl->setPath(fp);
}

//-----------------------------------------------------------------------------

void ScenePalette::rollbackPath() { m_pl->setPath(m_oldPath); }

//-----------------------------------------------------------------------------

bool ScenePalette::isDirty() { return m_pl->getPalette()->getDirtyFlag(); }

//-----------------------------------------------------------------------------

QStringList ScenePalette::getResourceName() {
  return QStringList(QString::fromStdString(m_pl->getPath().getLevelName()));
}

//=============================================================================
//
// SceneSound
//
//-----------------------------------------------------------------------------

SceneSound::SceneSound(ToonzScene *scene, TXshSoundLevel *sl)
    : SceneResource(scene)
    , m_sl(sl)
    , m_oldPath(sl->getPath())
    , m_oldActualPath(scene->decodeFilePath(sl->getPath())) {}

//-----------------------------------------------------------------------------

void SceneSound::save() {
  assert(m_oldPath != TFilePath());
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  TFilePath actualFp = m_scene->decodeFilePath(fp);
  try {
    TSystem::touchParentDir(actualFp);
    if (!TSystem::doesExistFileOrLevel(m_oldActualPath)) {
      m_sl->save(actualFp);
    } else if (actualFp != m_oldActualPath) {
      TSystem::copyFile(actualFp, m_oldActualPath);
    }
  } catch (...) {
    DVGui::warning(QObject::tr("Can't save") +
                   QString::fromStdWString(L": " + actualFp.getLevelNameW()));
  }
}

//-----------------------------------------------------------------------------

void SceneSound::updatePath() {
  TFilePath fp = m_oldPath;
  SceneResource::updatePath(fp);
  if (fp != m_oldPath) m_sl->setPath(fp);
}

//-----------------------------------------------------------------------------

void SceneSound::rollbackPath() { m_sl->setPath(m_oldPath); }

//=============================================================================
//
// SceneResources
//
//-----------------------------------------------------------------------------

SceneResources::SceneResources(ToonzScene *scene, TXsheet *subXsheet)
    : m_scene(scene)
    , m_commitDone(false)
    , m_wasUntitled(scene->isUntitled())
    , m_subXsheet(subXsheet) {
  getResources();
}

//-----------------------------------------------------------------------------

SceneResources::~SceneResources() {
  if (!m_commitDone) rollbackPaths();
  clearPointerContainer(m_resources);
}

//-----------------------------------------------------------------------------

void SceneResources::getResources() {
  ToonzScene *scene = m_scene;
  std::vector<TXshLevel *> levels;
  scene->getLevelSet()->listLevels(levels);
  std::vector<TXshLevel *>::iterator it;

  for (it = levels.begin(); it != levels.end(); ++it) {
    TXshSimpleLevel *sl = (*it)->getSimpleLevel();
    if (sl) m_resources.push_back(new SceneLevel(scene, sl));
    TXshPaletteLevel *pl = (*it)->getPaletteLevel();
    if (pl) m_resources.push_back(new ScenePalette(scene, pl));
    TXshSoundLevel *sdl = (*it)->getSoundLevel();
    if (sdl) m_resources.push_back(new SceneSound(scene, sdl));
  }
}

//-----------------------------------------------------------------------------

void SceneResources::save(const TFilePath newScenePath) {
  TFilePath oldScenePath = m_scene->getScenePath();
  m_scene->setScenePath(newScenePath);
  bool failedSave = false;
  for (int i = 0; i < (int)m_resources.size(); i++) {
    m_resources[i]->save();
  }

  QStringList failedList;
  getDirtyResources(failedList);

  if (!failedList.isEmpty()) {  // didn't save for some reason
    // show up to 5 items
    int extraCount = failedList.count() - 5;
    if (extraCount > 0) {
      failedList = failedList.mid(0, 5);
      failedList.append(QObject::tr("and %1 more item(s).").arg(extraCount));
    }

    DVGui::warning(QObject::tr("Failed to save the following resources:\n") +
                   "  " + failedList.join("\n  "));
  }
  m_scene->setScenePath(oldScenePath);
}

//-----------------------------------------------------------------------------

void SceneResources::updatePaths() {
  for (int i = 0; i < (int)m_resources.size(); i++)
    m_resources[i]->updatePath();
}

//-----------------------------------------------------------------------------

void SceneResources::rollbackPaths() {
  for (int i = 0; i < (int)m_resources.size(); i++)
    m_resources[i]->rollbackPath();
}

//-----------------------------------------------------------------------------

void SceneResources::accept(ResourceProcessor *processor, bool autoCommit) {
  for (int i = 0; i < (int)m_resources.size() && !processor->aborted(); i++)
    m_resources[i]->accept(processor);
  if (autoCommit) commit();
}

//-----------------------------------------------------------------------------
// return the name list of dirty resources
void SceneResources::getDirtyResources(QStringList &dirtyResources) {
  for (SceneResource *resource : m_resources)
    if (resource->isDirty()) {
      dirtyResources << resource->getResourceName();
    }
  dirtyResources.removeDuplicates();
}

//=============================================================================
//
// ResourceImporter
//
//-----------------------------------------------------------------------------

ResourceImporter::ResourceImporter(ToonzScene *scene, TProject *dstProject,
                                   ResourceImportStrategy &importStrategy)
    : m_scene(scene)
    , m_dstProject(dstProject)
    , m_dstScene(new ToonzScene())
    , m_importStrategy(importStrategy) {
  m_dstScene->setProject(dstProject);
  // scene file may not be in the "+scenes" path for the sandbox project.
  // in such case, try to save as "+scenes/filename.tnz" on import.
  TFilePath relativeScenePath =
      scene->getScenePath() - scene->getProject()->getScenesPath();
  if (relativeScenePath.isAbsolute())
    relativeScenePath = scene->getScenePath().withoutParentDir();
  TFilePath newFp = dstProject->getScenesPath() + relativeScenePath;
  makeUnique(newFp);
  m_dstScene->setScenePath(newFp);
}

//-----------------------------------------------------------------------------

ResourceImporter::~ResourceImporter() { delete m_dstScene; }

//-----------------------------------------------------------------------------

bool ResourceImporter::makeUnique(TFilePath &path) {
  return makePathUnique(m_dstScene, path);
}

//-----------------------------------------------------------------------------

TFilePath ResourceImporter::getImportedScenePath() const {
  return m_dstScene->getScenePath();
}

//-----------------------------------------------------------------------------

TFilePath ResourceImporter::codePath(const TFilePath &oldPath,
                                     const TFilePath &newActualPath) {
  return oldPath.withName(newActualPath.getName());
}

//-----------------------------------------------------------------------------

std::string ResourceImporter::extractPsdSuffix(TFilePath &path) {
  if (path.getType() != "psd") return "";
  std::string name = path.getName();
  int i            = name.find("#");
  if (i == std::string::npos) return "";
  std::string suffix = name.substr(i);
  path               = path.withName(name.substr(0, i));
  return suffix;
}

//-----------------------------------------------------------------------------

TFilePath ResourceImporter::buildPsd(const TFilePath &basePath,
                                     const std::string &suffix) {
  return basePath.withName(basePath.getName() + suffix);
}

//-----------------------------------------------------------------------------

void ResourceImporter::process(TXshSimpleLevel *sl) {
  if (sl->getPath().isAbsolute()) return;
  TFilePath newPath;

  TFilePath slPath   = sl->getPath();
  std::string suffix = extractPsdSuffix(slPath);

  TFilePath imgRefPath;
  if (sl->getPalette()) imgRefPath = sl->getPalette()->getRefImgPath();
  newPath = m_importStrategy.process(m_dstScene, m_scene, slPath);
  if (imgRefPath != TFilePath() &&
      !m_dstScene->isExternPath(m_dstScene->decodeFilePath(imgRefPath)))
    m_importStrategy.process(m_dstScene, m_scene, imgRefPath);

  if (suffix != "") newPath = buildPsd(newPath, suffix);

  sl->setPath(newPath);
  if (sl->getScannedPath() != TFilePath()) {
    newPath =
        m_importStrategy.process(m_dstScene, m_scene, sl->getScannedPath());
    sl->setScannedPath(newPath);
  }
  sl->setDirtyFlag(false);
}

//-----------------------------------------------------------------------------

void ResourceImporter::process(TXshPaletteLevel *pl) {
  if (pl->getPath().isAbsolute()) return;
  TFilePath newPath;
  newPath = m_importStrategy.process(m_dstScene, m_scene, pl->getPath());
  pl->setPath(newPath);
}

//-----------------------------------------------------------------------------

void ResourceImporter::process(TXshSoundLevel *sl) {
  if (sl->getPath().isAbsolute()) return;
  TFilePath newPath;
  newPath = m_importStrategy.process(m_dstScene, m_scene, sl->getPath());
  sl->setPath(newPath);
}

//=============================================================================
//
// ResourceCollector
//
//-----------------------------------------------------------------------------

ResourceCollector::ResourceCollector(ToonzScene *scene)
    : m_scene(scene), m_count(0) {}

//-----------------------------------------------------------------------------

ResourceCollector::~ResourceCollector() {}

//-----------------------------------------------------------------------------

bool ResourceCollector::makeUnique(TFilePath &path) {
  return makePathUnique(m_scene, path);
}

//-----------------------------------------------------------------------------

void ResourceCollector::process(TXshSimpleLevel *sl) {
  TFilePath path     = sl->getPath();
  std::string suffix = ResourceImporter::extractPsdSuffix(path);
  std::map<TFilePath, TFilePath>::iterator it = m_collectedFiles.find(path);
  if (it != m_collectedFiles.end()) {
    TFilePath destPath = it->second;
    if (suffix != "") destPath = ResourceImporter::buildPsd(destPath, suffix);
    sl->setPath(destPath);
  } else {
    TFilePath collectedPath = path;
    if (getCollectedPath(m_scene, collectedPath)) {
      TFilePath actualCollectedPath = m_scene->decodeFilePath(collectedPath);
      if (actualCollectedPath != path && TSystem::doesExistFileOrLevel(path) &&
          !TSystem::doesExistFileOrLevel(actualCollectedPath)) {
        try {
          TSystem::touchParentDir(actualCollectedPath);
          TXshSimpleLevel::copyFiles(actualCollectedPath, path);
        } catch (...) {
        }
      }
      ++m_count;
      TFilePath destPath = collectedPath;
      if (suffix != "") destPath = ResourceImporter::buildPsd(destPath, suffix);
      sl->setPath(destPath);
      m_collectedFiles[path] = collectedPath;
    }
  }

  if (sl->getScannedPath() != TFilePath()) {
    path                    = sl->getScannedPath();
    TFilePath collectedPath = path;
    if (getCollectedPath(m_scene, collectedPath)) {
      TFilePath actualCollectedPath = m_scene->decodeFilePath(collectedPath);
      if (actualCollectedPath != path && TSystem::doesExistFileOrLevel(path)) {
        try {
          TSystem::touchParentDir(actualCollectedPath);
          TXshSimpleLevel::copyFiles(actualCollectedPath, path);
        } catch (...) {
        }
      }
      sl->setScannedPath(collectedPath);
      m_count++;
    }
  }
  sl->setDirtyFlag(false);
}

//-----------------------------------------------------------------------------

void ResourceCollector::process(TXshSoundLevel *sl) {
  TFilePath path          = sl->getPath();
  TFilePath collectedPath = path;
  if (!getCollectedPath(m_scene, collectedPath)) return;

  TFilePath actualCollectedPath = m_scene->decodeFilePath(collectedPath);
  if (actualCollectedPath != path && TSystem::doesExistFileOrLevel(path)) {
    try {
      TSystem::touchParentDir(actualCollectedPath);
      TXshSimpleLevel::copyFiles(actualCollectedPath, path);

    } catch (...) {
    }
  }
  sl->setPath(collectedPath);
  m_count++;
}

//-----------------------------------------------------------------------------

void ResourceCollector::process(TXshPaletteLevel *pl) {
  TFilePath path          = pl->getPath();
  TFilePath collectedPath = path;
  if (!getCollectedPath(m_scene, collectedPath)) return;

  TFilePath actualCollectedPath = m_scene->decodeFilePath(collectedPath);
  if (actualCollectedPath != path && TSystem::doesExistFileOrLevel(path)) {
    try {
      TSystem::touchParentDir(actualCollectedPath);
      TXshSimpleLevel::copyFiles(actualCollectedPath, path);

    } catch (...) {
    }
  }
  pl->setPath(collectedPath);
  m_count++;
}
