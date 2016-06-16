

// TnzCore includes
#include "tconvert.h"
#include "tstream.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"

#include "toonz/levelset.h"

//=============================================================================

namespace {
const TFilePath defaultRootFolder("Cast");
const TFilePath defaultSoundRootFolder("Audio");
}

//=============================================================================
// TLevelSet

TLevelSet::TLevelSet() : m_defaultFolder(defaultRootFolder) {
  m_folders.push_back(defaultRootFolder);
  m_folders.push_back(defaultSoundRootFolder);
}

//-----------------------------------------------------------------------------

TLevelSet::~TLevelSet() { clear(); }

//-----------------------------------------------------------------------------

void TLevelSet::clear() {
  for (std::vector<TXshLevel *>::iterator it = m_levels.begin();
       it != m_levels.end(); ++it) {
    TXshLevel *level = *it;
    if (level->getSimpleLevel()) level->getSimpleLevel()->clearFrames();
    (*it)->release();
  }
  m_levelTable.clear();
  m_levels.clear();

  m_folderTable.clear();
  m_folders.clear();
  m_folders.push_back(defaultRootFolder);
  m_folders.push_back(defaultSoundRootFolder);
  m_defaultFolder = defaultRootFolder;
}

//-----------------------------------------------------------------------------

void TLevelSet::removeLevel(TXshLevel *level, bool deleteIt) {
  // if(level->getSimpleLevel())
  //  level->getSimpleLevel()->clearFrames();

  m_levels.erase(std::remove(m_levels.begin(), m_levels.end(), level),
                 m_levels.end());
  m_levelTable.erase(level->getName());
  if (deleteIt) level->release();

  m_folderTable.erase(level);
}

//-----------------------------------------------------------------------------

bool TLevelSet::renameLevel(TXshLevel *level, const std::wstring &newName) {
  if (level->getName() == newName) return true;
  if (m_levelTable.count(newName) > 0) return false;
  m_levelTable.erase(level->getName());
  level->setName(newName);
  m_levelTable[newName] = level;
  return true;
}

//-----------------------------------------------------------------------------

int TLevelSet::getLevelCount() const { return m_levels.size(); }

//-----------------------------------------------------------------------------

TXshLevel *TLevelSet::getLevel(int index) const {
  assert(0 <= index && index < getLevelCount());
  return m_levels[index];
}

//-----------------------------------------------------------------------------

TXshLevel *TLevelSet::getLevel(const std::wstring &levelName) const {
  std::map<std::wstring, TXshLevel *>::const_iterator it;
  it = m_levelTable.find(levelName);
  if (it == m_levelTable.end())
    return 0;
  else
    return it->second;
}

//-----------------------------------------------------------------------------

TXshLevel *TLevelSet::getLevel(const ToonzScene &scene,
                               const TFilePath &levelPath) const {
  const TFilePath &decodedPath = scene.decodeFilePath(levelPath);

  int l, lCount = getLevelCount();
  for (l = 0; l != lCount; ++l) {
    TXshLevel *level = getLevel(l);
    if (decodedPath == scene.decodeFilePath(level->getPath())) return level;
  }

  return 0;
}

//-----------------------------------------------------------------------------

bool TLevelSet::hasLevel(const std::wstring &levelName) const {
  std::vector<TXshLevel *>::const_iterator it = m_levels.begin();
  for (auto const level : m_levels) {
    if (levelName == level->getName()) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool TLevelSet::hasLevel(const ToonzScene &scene,
                         const TFilePath &levelPath) const {
  return getLevel(scene, levelPath);
}

//-----------------------------------------------------------------------------

void TLevelSet::listLevels(std::vector<TXshLevel *> &levels) const {
  levels = m_levels;
}

//-----------------------------------------------------------------------------

bool TLevelSet::insertLevel(TXshLevel *xl) {
  std::map<std::wstring, TXshLevel *>::const_iterator it;
  it = m_levelTable.find(xl->getName());
  if (it != m_levelTable.end() && it->second == xl)
    return it->second == xl;
  else {
    xl->addRef();
    m_levelTable[xl->getName()] = xl;
    m_levels.push_back(xl);

    TFilePath folder =
        (xl->getSoundLevel()) ? defaultSoundRootFolder : m_defaultFolder;
    m_folderTable[xl] = folder;
    assert(std::find(m_folders.begin(), m_folders.end(), folder) !=
           m_folders.end());
    return true;
  }
}

//-----------------------------------------------------------------------------

void TLevelSet::setDefaultFolder(TFilePath folder) {
  assert(std::find(m_folders.begin(), m_folders.end(), folder) !=
         m_folders.end());
  m_defaultFolder = folder;
}

//-----------------------------------------------------------------------------

TFilePath TLevelSet::getFolder(TXshLevel *xl) const {
  std::map<TXshLevel *, TFilePath>::const_iterator it;
  it = m_folderTable.find(xl);
  assert(it != m_folderTable.end());
  return it->second;
}

//-----------------------------------------------------------------------------

TFilePath TLevelSet::renameFolder(const TFilePath &folder,
                                  const std::wstring &newName) {
  // Impedisco la creazione di folder con nome "" che creano problemi
  // (praticamente Ecome se avessero infinite sottocartelle)
  if (newName == L"") return folder;

  TFilePath folder2 = folder.withName(newName);

  for (int i = 0; i < (int)m_folders.size(); i++) {
    if (folder == m_folders[i])
      m_folders[i] = folder2;
    else if (folder.isAncestorOf(m_folders[i]))
      m_folders[i] = folder2 + (m_folders[i] - folder);
  }
  if (m_defaultFolder == folder) m_defaultFolder = folder2;
  std::map<TXshLevel *, TFilePath>::iterator it;
  for (it = m_folderTable.begin(); it != m_folderTable.end(); ++it) {
    if (folder == it->second)
      it->second = folder2;
    else if (folder.isAncestorOf(it->second))
      it->second = folder2 + (it->second - folder);
  }
  return folder2;
}

//-----------------------------------------------------------------------------

TFilePath TLevelSet::createFolder(const TFilePath &parentFolder,
                                  const std::wstring &newName) {
  TFilePath child = parentFolder + newName;
  if (std::find(m_folders.begin(), m_folders.end(), child) == m_folders.end())
    m_folders.push_back(child);
  return child;
}

//-----------------------------------------------------------------------------

void TLevelSet::removeFolder(const TFilePath &folder) {
  if (folder == m_defaultFolder) return;
  std::vector<TFilePath> folders;
  for (int i = 0; i < (int)m_folders.size(); i++)
    if (!folder.isAncestorOf(m_folders[i])) folders.push_back(m_folders[i]);
  folders.swap(m_folders);
  std::map<TXshLevel *, TFilePath>::iterator it;
  for (it = m_folderTable.begin(); it != m_folderTable.end(); ++it) {
    if (folder.isAncestorOf(it->second)) it->second = m_defaultFolder;
  }
}

//-----------------------------------------------------------------------------

void TLevelSet::listLevels(std::vector<TXshLevel *> &levels,
                           const TFilePath &folder) const {
  std::map<TXshLevel *, TFilePath>::const_iterator it;
  for (it = m_folderTable.begin(); it != m_folderTable.end(); ++it)
    if (folder == it->second) levels.push_back(it->first);
}

//-----------------------------------------------------------------------------

void TLevelSet::listFolders(std::vector<TFilePath> &folders,
                            const TFilePath &folder) const {
  for (int i = 0; i < (int)m_folders.size(); i++)
    if (m_folders[i].getParentDir() == folder) folders.push_back(m_folders[i]);
}

//-----------------------------------------------------------------------------

void TLevelSet::moveLevelToFolder(const TFilePath &fp, TXshLevel *level) {
  TFilePath folder(fp);

  if (folder == TFilePath()) folder = m_defaultFolder;
  if (std::find(m_folders.begin(), m_folders.end(), folder) ==
      m_folders.end()) {
    assert(0);
    return;
  }
  std::map<TXshLevel *, TFilePath>::iterator it;
  it = m_folderTable.find(level);
  assert(it != m_folderTable.end());
  if (it == m_folderTable.end()) return;
  it->second = folder;
}

//-----------------------------------------------------------------------------

void TLevelSet::saveFolder(TOStream &os, TFilePath folder) {
  std::map<std::string, std::string> attr;
  attr["name"]                                   = folder.getName();
  if (folder == getDefaultFolder()) attr["type"] = "default";
  os.openChild("folder", attr);
  std::vector<TFilePath> folders;
  listFolders(folders, folder);
  if (!folders.empty()) {
    for (int i = 0; i < (int)folders.size(); i++) saveFolder(os, folders[i]);
  }
  std::vector<TXshLevel *> levels;
  listLevels(levels, folder);
  if (!levels.empty()) {
    os.openChild("levels");
    for (int i = 0; i < (int)levels.size(); i++)
      if (m_saveSet.empty() || m_saveSet.count(levels[i]) > 0) os << levels[i];
    os.closeChild();
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

void TLevelSet::loadFolder(TIStream &is, TFilePath folder) {
  std::string s;
  is.getTagParam("type", s);
  if (s == "default") setDefaultFolder(folder);
  while (!is.eos()) {
    std::string tagName;
    is.matchTag(tagName);
    if (tagName == "levels") {
      while (!is.eos()) {
        TPersist *p = 0;
        is >> p;
        TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
        if (xshLevel && !xshLevel->getChildLevel())
          moveLevelToFolder(folder, xshLevel);
      }
    } else if (tagName == "folder") {
      is.getTagParam("name", s);
      TFilePath child = createFolder(folder, ::to_wstring(s));
      loadFolder(is, child);
    } else
      throw TException("expected <levels> or <folder>");
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TLevelSet::loadData(TIStream &is) {
  int folderCount = 1;
  while (!is.eos()) {
    std::string tagName;
    if (is.matchTag(tagName)) {
      if (tagName == "levels") {
        while (!is.eos()) {
          TPersist *p = 0;
          is >> p;
          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          if (xshLevel) {
            insertLevel(xshLevel);
          }
        }
      } else if (tagName == "folder") {
        std::string name = ::to_string(defaultRootFolder.getWideString());
        is.getTagParam("name", name);
        TFilePath folder(name);
        if (folderCount == 1)
          m_defaultFolder = m_folders[0] = folder;
        else if (name != defaultSoundRootFolder.getName())
          m_folders.push_back(folder);
        folderCount++;
        loadFolder(is, folder);
      } else
        throw TException("expected <levels> or <folder>");
      is.closeChild();
    } else
      throw TException("expected tag");
  }
}

//-----------------------------------------------------------------------------

void TLevelSet::saveData(TOStream &os) {
  os.openChild("levels");
  int i;
  for (i = 0; i < getLevelCount(); i++) {
    TXshLevel *level = getLevel(i);
    if (m_saveSet.empty() || m_saveSet.count(level) > 0) os << level;
  }
  os.closeChild();
  std::vector<TFilePath> folders;
  listFolders(folders, TFilePath());
  assert(!folders.empty());
  for (i = 0; i < (int)folders.size(); i++) saveFolder(os, folders[i]);
}
