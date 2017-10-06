

#include "toonz/tproject.h"

// TnzLib includes
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/observer.h"
#include "toonz/toonzfolders.h"
#include "toonz/cleanupparameters.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tconvert.h"

// Qt includes
#include <QFileInfo>
#include <QDir>

// STD includes
#include <fstream>
#include <stdlib.h>

using namespace std;

//===================================================================

/* Version-related strings added to project files, in reversed chronological
 * order */
const std::wstring prjSuffix[4] = {L"_otprj", L"_prj63ml", L"_prj6", L"_prj"};
const std::wstring xmlExt       = L".xml";
const int prjSuffixCount        = 4;

//===================================================================
/*! Default inputs folder: is used to save all scanned immage.*/
const std::string
    TProject::Inputs = "inputs",
    /*! Default drawings folder: is used to save all tlv and pli levels.*/
    TProject::Drawings = "drawings",
    /*! Default scenes folder: is used to save all scenes.*/
    TProject::Scenes = "scenes",
    /*! Default scripts folder: is used to save all the script.*/
    TProject::Scripts = "scripts",
    /*! Default extras folder: is used to save all imported images and levels
       not pli or tlv.*/
    TProject::Extras = "extras",
    /*! Default outputs folder: is used to save all rendered scenes.*/
    TProject::Outputs = "outputs",
    /*! Default palettes folder: is used for color design (色指定)*/
    TProject::Palettes = "palettes";
//! Default project name
const TFilePath TProject::SandboxProjectName("sandbox");

TProjectP currentProject;

//===================================================================

namespace {

//===================================================================
//
// helper functions
//
//===================================================================

TFilePath makeRelative(TFilePath ref, TFilePath fp) {
  if (!fp.isAbsolute()) return fp;
  TFilePath dots;
  for (;;) {
    if (ref.isAncestorOf(fp)) {
      TFilePath relativePath = dots + (fp - ref);
      return relativePath;
    }
    if (ref.isRoot()) return fp;
    ref  = ref.getParentDir();
    dots = dots + "..";
  }
}

//-------------------------------------------------------------------

TFilePath makeAbsolute(TFilePath ref, TFilePath fp) {
  if (fp.isAbsolute()) return fp;
  const TFilePath twoDots("..");
  while (twoDots.isAncestorOf(fp)) {
    TFilePath refParent = ref.getParentDir();
    if (refParent == TFilePath()) break;  // non dovrebbe succedere
    ref = refParent;
    fp  = fp - twoDots;
  }
  fp = ref + fp;

  return fp;
}

//===================================================================

TEnv::StringVar currentProjectPath("CurrentProject", "");

//===================================================================

//! Returns the project suffix (ie anything beyond the last '_' included)
//! of the passed file path.
std::wstring getProjectSuffix(const TFilePath &path) {
  const std::wstring &name = path.getWideName();
  int idx                  = name.find_last_of(L'_');
  if (idx == string::npos) return L"";

  return name.substr(idx);
}

//-------------------------------------------------------------------

//! In case the supplied path has an old version suffix,
//! this function updates it to the most recent; otherwise,
//! it is left untouched.
TFilePath getLatestVersionProjectPath(const TFilePath &path) {
  const std::wstring &suffix = getProjectSuffix(path);
  for (int i = 1; i < prjSuffixCount; ++i)
    if (suffix == prjSuffix[i]) {
      const std::wstring &name = path.getWideName();
      int pos                  = name.size() - suffix.size();
      return path.withName(path.getWideName().substr(0, pos) + prjSuffix[0]);
    }

  return path;
}

//===================================================================

/*! Return project path with right suffix.
                VERSION 6.0: _prj6.xml;
                PRECEDENT VERSION: _prj.xml.
                If in \b folder exists ONLY old version project path return old
   version path;
                otherwise return 6.0 version project path.
*/
TFilePath searchProjectPath(TFilePath folder) {
  assert(folder.isAbsolute());
  wstring projectName = folder.getWideName();

  // Search for the first available project file, starting from the most recent.
  TFilePath projectPath;
  for (int i = 0; i < prjSuffixCount; ++i) {
    projectPath = folder + TFilePath(projectName + prjSuffix[i] + xmlExt);
    if (TFileStatus(projectPath).doesExist()) return projectPath;
  }

  // If none exist in the folder, build the name with the most recent suffix
  return folder + TFilePath(projectName + prjSuffix[0] + xmlExt);
}

//===================================================================

bool isFolderUnderVersionControl(const TFilePath &folderPath) {
  QDir dir(QString::fromStdWString(folderPath.getWideString()));
  return dir.entryList(QDir::AllDirs | QDir::Hidden).contains(".svn");
}

//===================================================================

void hideOlderProjectFiles(const TFilePath &folderPath) {
  const std::wstring &name = folderPath.getWideName();

  TFilePath path;
  for (int i = 1; i < prjSuffixCount; ++i) {
    path = folderPath + (name + prjSuffix[i] + xmlExt);
    if (TFileStatus(path).doesExist())
      TSystem::renameFile(path.withType("xml_"), path);
  }
}

}  // namespace

//===================================================================

//===================================================================
//
// TProject
//

//-------------------------------------------------------------------

/*! \class TProject tproject.h
        \brief Define and handle a toonz project.

    A toonz project is identified by a project name that matches a folder with
   the same name in the project root.\n
        The project folder can contains saveral other folders.
        By default, five folders are created: Inputs, Drawings, Scenes, Extras
   and Outputs.
        Each of this folders can be renamed using the setFolder(string name,
   TFilePath path) method.
        Usually, the \b name parameter is choosen from inputs, drawings, scenes,
   extras and outputs;
        the \b path parameter contains the folder that can have a different name
   from them.
        All association between names and folder are kept in a mapping.\n
        A folder of a project can be constant or scene dependent. A constant
   folder is used by
        every scene created in the project to save or load data. A scene
   dependent folder is used only by the scene
        from wich the folder depends. A scene dependent folder contains the
   string "$scene" in its path.

        \code
        e.g.
        Scene path: "...\\prodA\\episode1\\scenes\\sceneA.tzn"
        Drawings scene dependent folder:
   "...\\prodA\\episode1\\$scene\\drawings"
        Drawings folder path: "...\\prodA\\episode1\\SceneA\\drawings"
        \endcode
        \n\n
        By default, from the toonz installation, exist allways a toonz project
   called "sandbox".
        \see TProjectManager, TSceneProperties.
*/

/*! \fn TFilePath TProject::getName() const
        Returns the name of the project.
        \code
        e.g. "prodA\\episode1"
        \endcode
*/

/*! \fn TFilePath TProject::getProjectPath() const
        Returns the path of the project. It is an absolute path.
        \code
        e.g. "prodA\\episode1" -> "...\\prodA\\episode1\\episode1_prj.xml"
        \endcode
  */

/*! \fn TFilePath TProject::getProjectFolder() const
        Returns the project folder path. It is an absolute path.
        \code
        e.g. "prodA\\episode1" -> "...\\prodA\\episode1\\"
        \endcode
  */

/*! \fn const TSceneProperties &TProject::getSceneProperties() const
    Returns the scene properties of the project.
    \see TSceneProperties
        */

/*! \fn void TProject::save()
        Saves the project.
        Is equvalent to save(getProjectPath()).
        The project is saved as a xml file.\n
        Uses TProjectManager and TOStream.
        \note Exceptions can be thrown.
        \see TProjectManager and TOStream.
  */

TProject::TProject() : m_name(), m_path(), m_sprop(new TSceneProperties()) {}

//-------------------------------------------------------------------

TProject::~TProject() { delete m_sprop; }

//-------------------------------------------------------------------
/*! Associates the \b name to the specified \b path.
        \code
        e.g. setFolder(TProject::Drawings, TFilePath("C:\\temp\\drawings"))
        \endcode
        Usually, the \b name parameter is choosen from inputs, drawings, scenes,
   extras and outputs;
        the \b path contains the folder that can have a different name from
   them.
        Every association between names and paths is contained in a mapping.
        \note Not absolute path are thought relative to the project folder.
*/
void TProject::setFolder(string name, TFilePath path) {
  std::map<std::string, TFilePath>::iterator it;
  it = m_folders.find(name);
  if (it == m_folders.end()) {
    m_folderNames.push_back(name);
    m_folders[name] = path;
  } else {
    it->second = path;
  }
}

//-------------------------------------------------------------------
/*! Create a folder named with \b name.
        Call the setFolder(name,TFilePath(name)) method.\n
        e.g. setFolder(TProject::Drawings) is equivalent to
   setFolder(TProject::Drawings, TFilePath("drawings"))\n
        The resulting is "..\\projectFolder\\drawings"
*/
void TProject::setFolder(string name) { setFolder(name, TFilePath(name)); }

//-------------------------------------------------------------------
/*! Returns the path of the folder named with \b name.\n
        Returns TFilePath() if there isn't a folder named with \b name.
        \note The returned path could be a relative path if \b absolute is
   false.
*/
TFilePath TProject::getFolder(string name, bool absolute) const {
  std::map<std::string, TFilePath>::const_iterator it;
  it = m_folders.find(name);
  if (it != m_folders.end())
    return (absolute) ? makeAbsolute(getProjectFolder(), it->second)
                      : it->second;
  else
    return TFilePath();
}

//-------------------------------------------------------------------
/*! Returns the path of the Scene folder.\n
        The Scene folder contains all the saved scene. The returned path is an
   absolute path.
*/
TFilePath TProject::getScenesPath() const {
  TFilePath scenes = getFolder(Scenes);
  return makeAbsolute(getProjectFolder(), scenes);
}

//-------------------------------------------------------------------
/*! Returns the path of the folder indexed with \b index.\n
        Returns TFilePath() if there isn't a folder indexed with \b index.
        \note The returned path could be a relative path.
*/
TFilePath TProject::getFolder(int index) const {
  if (0 <= index && index < (int)m_folderNames.size())
    return getFolder(m_folderNames[index]);
  else
    return TFilePath();
}

//-------------------------------------------------------------------
/*! Returns true if the folder indexed with \b index isn't scene dependent.\n
        A scene dependent folder is a folder containing "$scene" in its path.
*/
bool TProject::isConstantFolder(int index) const {
  TFilePath fp = getFolder(index);
  return fp.getWideString().find(L"$scene") == wstring::npos;
}

//-------------------------------------------------------------------
//! Returns the number of the folders contained in the project folder.
int TProject::getFolderCount() const { return m_folders.size(); }

//-------------------------------------------------------------------
//! Returns the name of the folder indexed with \b index.
string TProject::getFolderName(int index) const {
  if (0 <= index && index < (int)m_folderNames.size())
    return m_folderNames[index];
  else
    return "";
}

//-------------------------------------------------------------------

/*! Returns the index of the folder named with \b folderName.\n
        If a folder named with \b folderName doesn't exist, return -1.
*/
int TProject::getFolderIndex(string name) const {
  std::vector<std::string>::const_iterator it;
  it = std::find(m_folderNames.begin(), m_folderNames.end(), name);
  if (it == m_folderNames.end()) return -1;
  return std::distance(it, m_folderNames.begin());
}

//-------------------------------------------------------------------
/*! Returns true if this project is the current project.
        Uses \b TProjectManager.
        \see TProjectManager
*/
bool TProject::isCurrent() const {
  TFilePath currentProjectPath =
      TProjectManager::instance()->getCurrentProjectPath();
  if (getProjectPath() == currentProjectPath)
    return true;
  else
    return getLatestVersionProjectPath(currentProjectPath) ==
           getLatestVersionProjectPath(getProjectPath());
}

//-------------------------------------------------------------------
/*! Set the scene properties \b sprop to the project.
    \see TSceneProperties*/
void TProject::setSceneProperties(const TSceneProperties &sprop) {
  m_sprop->assign(&sprop);
}

//-------------------------------------------------------------------
/*! Returns the absolute path of \b fp.
        If \b fp contains "$project", replaces it with the name of the project.
        \note the returned path can contain "$scene"
*/
TFilePath TProject::decode(TFilePath fp) const {
  for (;;) {
    wstring fpstr = fp.getWideString();
    int j         = fpstr.find(L"$project");
    if (j == (int)wstring::npos) break;
    fpstr.replace(j, 8, getName().getWideString());
    fp = TFilePath(fpstr);
  }
  return makeAbsolute(getProjectFolder(), fp);
}

//-------------------------------------------------------------------

void TProject::setUseScenePath(string folderName, bool on) {
  m_useScenePathFlags[folderName] = on;
}

//-------------------------------------------------------------------

bool TProject::getUseScenePath(string folderName) const {
  std::map<std::string, bool>::const_iterator it;
  it = m_useScenePathFlags.find(folderName);
  return it != m_useScenePathFlags.end() ? it->second : false;
}

//-------------------------------------------------------------------
/*! Returns the index of the folder specified in the path \b folderDir.
        Returns -1 if \b folderDir isn't a folder of the project.
*/
int TProject::getFolderIndexFromPath(const TFilePath &folderDir) {
  TFilePath scenePath          = decode(getFolder(Scenes));
  bool sceneDependentScenePath = false;
  if (scenePath.getName().find("$scene") != string::npos) {
    scenePath               = scenePath.getParentDir();
    sceneDependentScenePath = true;
  }
  int folderIndex;
  for (folderIndex = 0; folderIndex < getFolderCount(); folderIndex++)
    if (isConstantFolder(folderIndex)) {
      TFilePath fp = decode(getFolder(folderIndex));
      if (fp == folderDir) return folderIndex;
    } else {
      TFilePath fp = decode(getFolder(folderIndex));
      wstring a    = fp.getWideString();
      wstring b    = folderDir.getWideString();
      int alen     = a.length();
      int blen     = b.length();
      int i        = a.find(L"$scene");
      assert(i != (int)wstring::npos);
      if (i == (int)wstring::npos) continue;
      int j = i + 1;
      while (j < alen && isalnum(a[j])) j++;
      // a.substr(i,j-i) == "$scenexxxx"
      int k = j + blen - alen;
      if (!(0 <= i && i < k && k <= blen)) continue;
      assert(i < blen);
      if (i > 0 && a.substr(0, i) != b.substr(0, i)) continue;
      if (k < blen && (j >= alen || a.substr(j) != b.substr(k))) continue;
      wstring v = b.substr(i, k - i);
      TFilePath scene(v + L".tnz");
      if (sceneDependentScenePath)
        scene = scenePath + scene.getWideName() + scene;
      else
        scene = scenePath + scene;
      if (TFileStatus(scene).doesExist()) return folderIndex;
    }
  return -1;
}

//-------------------------------------------------------------------
/*! Returns the folder's name of the specified TFilePath \b folderDir.\n
        Returns the empty string if \b folderDir isn't a folder of the
   project.*/
wstring TProject::getFolderNameFromPath(const TFilePath &folderDir) {
  int index = getFolderIndexFromPath(folderDir);
  if (index < 0) return L"";
  if (getFolder(index).isAbsolute())
    return ::to_wstring("+" + getFolderName(index));
  else
    return folderDir.getWideName();
}

//-------------------------------------------------------------------
/*! Saves the project in the specified path.
        The TfilePath fp must be an absolute path. The project is saved as a xml
   file.\n
        Uses TProjectManager and TOStream.
        \note Exceptions can be thrown.
        \see TProjectManager and TOStream.
*/
bool TProject::save(const TFilePath &projectPath) {
  assert(isAProjectPath(projectPath));

  TProjectManager *pm     = TProjectManager::instance();
  m_name                  = pm->projectPathToProjectName(projectPath);
  m_path                  = getLatestVersionProjectPath(projectPath);
  TFilePath projectFolder = projectPath.getParentDir();

  if (!TFileStatus(projectFolder).doesExist()) {
    try {
      TSystem::mkDir(projectFolder);
    } catch (...) {
      return false;
    }
  }

  TFilePath sceneFolder    = decode(getFolder(TProject::Scenes));
  TFilePath scenesDescPath = sceneFolder + "scenes.xml";

  TFileStatus fs(projectPath);
  if (fs.doesExist() && !fs.isWritable()) {
    throw TSystemException(
        projectPath,
        "Cannot save the project settings. The file is read-only.");
    return false;
  }
  TFileStatus fs2(scenesDescPath);
  if (fs2.doesExist() && !fs2.isWritable()) {
    throw TSystemException(
        projectPath,
        "Cannot save the project settings. The scenes file is read-only.");
    return false;
  }

  TOStream os(m_path);
  os.openChild("project");
  os.openChild("version");
  os << 70 << 1;    // Standard version signature:
  os.closeChild();  //   <Major Toonz version number * 10>.<Major version
                    //   advancement>
  os.openChild("folders");
  int i = 0;
  for (i = 0; i < getFolderCount(); i++) {
    TFilePath folderRelativePath = getFolder(i);
    if (folderRelativePath == TFilePath()) continue;
    std::map<std::string, string> attr;
    string folderName = getFolderName(i);
    attr["name"]      = folderName;
    attr["path"]      = ::to_string(folderRelativePath);  // escape()
    if (getUseScenePath(folderName)) attr["useScenePath"] = "yes";
    os.openCloseChild("folder", attr);
  }
  os.closeChild();

  os.openChild("sceneProperties");
  getSceneProperties().saveData(os);
  os.closeChild();
  os.closeChild();

  // crea (se necessario) le directory relative ai vari folder
  for (i = 0; i < getFolderCount(); i++)
    if (isConstantFolder(i)) {
      TFilePath fp = getFolder(i);
      if (fp == TFilePath()) continue;
      fp = decode(fp);
      // if(!fp.isAbsolute()) fp = projectFolder + fp;
      if (!TFileStatus(fp).doesExist()) {
        try {
          TSystem::mkDir(fp);
        } catch (...) {
        }
      }
    }

  /*-- +scenes だけでなく、全てのProject Folderにscenes.xmlを生成する --*/
  std::vector<std::string> foldernames;
  pm->getFolderNames(foldernames);
  for (int f = 0; f < foldernames.size(); f++) {
    TFilePath folderpath = decode(getFolder(foldernames.at(f)));
    if (folderpath.isEmpty() || !isConstantFolder(f)) continue;

    TFilePath xmlPath = folderpath + "scenes.xml";
    TFileStatus xmlfs(xmlPath);
    if (xmlfs.doesExist() && !xmlfs.isWritable()) continue;

    TFilePath relativeProjectFolder =
        makeRelative(folderpath, m_path.getParentDir());

    TOStream os2(xmlPath);
    std::map<std::string, string> attr;
    attr["type"] = "projectFolder";
    os2.openChild("parentProject", attr);
    os2 << relativeProjectFolder;
    os2.closeChild();
  }

  // The project has been successfully saved. In case there are other
  // project files from older Toonz project versions, those files are
  // renamed so that older Toonz versions can no longer 'see' it.
  if (!isFolderUnderVersionControl(projectFolder))
    hideOlderProjectFiles(projectFolder);

  return true;
}

//-------------------------------------------------------------------

bool TProject::save() { return save(m_path); }

//-------------------------------------------------------------------
/*! Loads the project specified in \b projectPath.\n
        \b projectPath must be an absolute path.
*/
void TProject::load(const TFilePath &projectPath) {
  assert(isAProjectPath(projectPath));

  TFilePath latestProjectPath = getLatestVersionProjectPath(projectPath);
  TFilePath inputProjectPath  = searchProjectPath(projectPath.getParentDir());

  TProjectManager *pm = TProjectManager::instance();
  m_name              = pm->projectPathToProjectName(latestProjectPath);
  m_path              = latestProjectPath;

  m_folderNames.clear();
  m_folders.clear();
  m_useScenePathFlags.clear();
  delete m_sprop;
  m_sprop = new TSceneProperties();

  // Read the project
  TIStream is(inputProjectPath);
  if (!is) return;

  string tagName;
  if (!is.matchTag(tagName) || tagName != "project") return;

  while (is.matchTag(tagName)) {
    if (tagName == "folders") {
      while (is.matchTag(tagName)) {
        if (tagName == "folder") {
          string name = is.getTagAttribute("name");
          TFilePath path(is.getTagAttribute("path"));
          setFolder(name, path);
          string useScenePath = is.getTagAttribute("useScenePath");
          setUseScenePath(name, useScenePath == "yes");
        } else
          throw TException("expected <folder>");
      }
      is.matchEndTag();
    } else if (tagName == "version") {
      int major, minor;
      is >> major >> minor;
      is.setVersion(VersionNumber(major, minor));
      is.matchEndTag();
    } else if (tagName == "sceneProperties") {
      TSceneProperties sprop;
      try {
        sprop.loadData(is, true);
      } catch (...) {
      }
      setSceneProperties(sprop);
      is.matchEndTag();
    }
  }
}

//-------------------------------------------------------------------
/*! Returns true if the specified path is a project path.\n
        A project path must be absolute, must be an xml file and must have the
   name of the
        parent root with either one of the version-dependent suffixes "_prj*".\n
        \code
        e.g. "C:\\Toonz 5.2 stuff\\projects\\prodA\\episode1\\episode1_prj.xml"
   is a project path.
        \endcode
*/
bool TProject::isAProjectPath(const TFilePath &fp) {
  if (fp.isAbsolute() && fp.getType() == "xml") {
    const std::wstring &fpName     = fp.getWideName();
    const std::wstring &folderName = fp.getParentDir().getWideName();
    for (int i = 0; i < prjSuffixCount; ++i)
      if (fpName == (folderName + prjSuffix[i])) return true;
  }

  return false;
}

//-------------------------------------------------------------------

namespace {

/*
class SimpleProject final : public TProject {
public:
  SimpleProject() : TProject(TFilePath("___simpleProject")) {
  }

};
*/
}  // namespace

//===================================================================
//
// TProjectManager
//
//-------------------------------------------------------------------

/*! \class TProjectManager tproject.h
        \brief Manages all toonz projects. The class provides all needed method
   to retrieve projects paths, names
        and folders.

        It is possible to handle more than one project root.
        The class mantains a container this purpose. All the projects roots must
   be setted by hand in the windows
        registery. By default, only one project root is created when toonz is
   installed.\n
        The project root container can be updated using addProjectsRoot(const
   TFilePath &root), addDefaultProjectsRoot()
        methods.

        The class mantains also information about the current project. The class
   provides all needed method to retrieve
        the current project path, name and folder.
        \see TProject

*/

/*! \fn bool TProjectManager::isTabModeEnabled() const
        Returns the tab mode.
        \note the tab mode is used for Tab Application
*/

/*! \fn void TProjectManager::enableTabMode(bool tabMode)
        Set the tab mode to the passed \b tabMode.
        \note the tab mode is used for Tab Application
*/

TProjectManager::TProjectManager() : m_tabMode(false), m_tabKidsMode(false) {}

//-------------------------------------------------------------------

TProjectManager::~TProjectManager() {}

//-------------------------------------------------------------------
/*! Returns the instance to the TProjectManager.\n
        If an instance doesn't exist, creates one.*/
TProjectManager *TProjectManager::instance() {
  static TProjectManager _instance;
  return &_instance;
}

//-------------------------------------------------------------------
/*! Adds the specified folder \b fp in the projecs roots container.\n
        If \b fp is already contained in the container, the method does nothing.
        \note \b fp must be a folder and not a file path.*/
void TProjectManager::addProjectsRoot(const TFilePath &root) {
  // assert(TFileStatus(root).isDirectory());
  if (std::find(m_projectsRoots.begin(), m_projectsRoots.end(), root) ==
      m_projectsRoots.end())
    m_projectsRoots.push_back(root);
}

//-------------------------------------------------------------------

/*! Adds the specified folder \b fp in the version control projecs roots
   container.\n
        If \b fp is already contained in the container, the method does nothing.
        \note \b fp must be a folder and not a file path.*/
void TProjectManager::addSVNProjectsRoot(const TFilePath &root) {
  assert(TFileStatus(root).isDirectory());
  if (std::find(m_svnProjectsRoots.begin(), m_svnProjectsRoots.end(), root) ==
      m_svnProjectsRoots.end())
    m_svnProjectsRoots.push_back(root);
}

//-------------------------------------------------------------------

void TProjectManager::addDefaultProjectsRoot() {
  addProjectsRoot(TEnv::getStuffDir() + "projects");
}

//-------------------------------------------------------------------

TFilePath TProjectManager::getCurrentProjectRoot() {
  TFilePath currentProjectPath = getCurrentProjectPath();
  int i;
  for (i = 0; i < (int)m_projectsRoots.size(); i++)
    if (m_projectsRoots[i].isAncestorOf(currentProjectPath))
      return m_projectsRoots[i];
  for (i = 0; i < (int)m_svnProjectsRoots.size(); i++)
    if (m_svnProjectsRoots[i].isAncestorOf(currentProjectPath))
      return m_svnProjectsRoots[i];
  if (m_projectsRoots.empty())
    addDefaultProjectsRoot();  // shouldn't be necessary
  return m_projectsRoots[0];
}

//-------------------------------------------------------------------
/*! Returns the name of the specified \b projectPath.
        \note projectPath must be an absolute path.
*/
TFilePath TProjectManager::projectPathToProjectName(
    const TFilePath &projectPath) {
  assert(projectPath.isAbsolute());
  TFilePath projectFolder = projectPath.getParentDir();
  if (m_projectsRoots.empty()) addDefaultProjectsRoot();
  int i;
  for (i = 0; i < (int)m_projectsRoots.size(); i++) {
    if (m_projectsRoots[i].isAncestorOf(projectFolder))
      return projectFolder - m_projectsRoots[i];
  }
  for (i = 0; i < (int)m_svnProjectsRoots.size(); i++) {
    if (m_svnProjectsRoots[i].isAncestorOf(projectFolder))
      return projectFolder - m_svnProjectsRoots[i];
  }
  // non dovrei mai arrivare qui: il progetto non sta sotto un project root
  return projectFolder.withoutParentDir();
}

//-------------------------------------------------------------------
/*! Returns an absolute path of the specified \b projectName.\n
        \note The returned project path is allways computed used the first
   project root in the container.*/
TFilePath TProjectManager::projectNameToProjectPath(
    const TFilePath &projectName) {
  assert(!TProject::isAProjectPath(projectName));
  assert(!projectName.isAbsolute());
  if (m_projectsRoots.empty()) addDefaultProjectsRoot();
  if (projectName == TProject::SandboxProjectName)
    return searchProjectPath(TEnv::getStuffDir() + projectName);
  return searchProjectPath(m_projectsRoots[0] + projectName);
}

//-------------------------------------------------------------------
/*! Returns the absolute path of the project file respect to the specified \b
   projectFolder.\n
        \note \b projectName must be an absolute path.*/
TFilePath TProjectManager::projectFolderToProjectPath(
    const TFilePath &projectFolder) {
  assert(projectFolder.isAbsolute());
  return searchProjectPath(projectFolder);
}

//-------------------------------------------------------------------
/*! Returns the absolute path of the specified \b projectName only if the
   project already exist.\n
        Returns TFilePath() if a project with the specified \b projectName
   doesn't exsist.\n
        \note \b projectName must be a relative path.*/
TFilePath TProjectManager::getProjectPathByName(const TFilePath &projectName) {
  assert(!TProject::isAProjectPath(projectName));
  assert(!projectName.isAbsolute());
  // TFilePath relativeProjectPath = projectName + (projectName.getName() +
  // projectPathSuffix);
  if (m_projectsRoots.empty()) addDefaultProjectsRoot();
  if (projectName == TProject::SandboxProjectName)
    return searchProjectPath(TEnv::getStuffDir() + projectName);
  int i, n = (int)m_projectsRoots.size();
  for (i = 0; i < n; i++) {
    TFilePath projectPath = searchProjectPath(m_projectsRoots[i] + projectName);
    assert(TProject::isAProjectPath(projectPath));
    if (TFileStatus(projectPath).doesExist()) return projectPath;
  }
  for (i = 0; i < (int)m_svnProjectsRoots.size(); i++) {
    TFilePath projectPath =
        searchProjectPath(m_svnProjectsRoots[i] + projectName);
    assert(TProject::isAProjectPath(projectPath));
    if (TFileStatus(projectPath).doesExist()) return projectPath;
  }
  return TFilePath();
}

//-------------------------------------------------------------------
/*! Gets all project folder names and put them in the passed vector \b names.
        \note All previous data contained in \b names are lost.*/

void TProjectManager::getFolderNames(std::vector<std::string> &names) {
  names.clear();
  TFilePath fp = ToonzFolder::getProfileFolder() + "project_folders.txt";
  try {
    Tifstream is(fp);
    if (is)
      for (;;) {
        char buffer[1024];
        is.getline(buffer, sizeof(buffer));
        if (is.eof()) break;
        char *s = buffer;
        while (*s == ' ' || *s == '\t') s++;  // skips blanks
        char *t = s;
        while (*t && *t != '\r' && *t != '\n') t++;  // reads up to end of line
        while (t > s && (t[-1] == ' ' || t[-1] == '\t'))
          t--;  // remove trailing blanks
        t[0] = '\0';
        if (s[0]) names.push_back(string(s));
      }
  } catch (...) {
  }
  const std::string stdNames[] = {TProject::Inputs,  TProject::Drawings,
                                  TProject::Scenes,  TProject::Extras,
                                  TProject::Outputs, TProject::Scripts};
  for (int i = 0; i < (int)tArrayCount(stdNames); i++) {
    string name = stdNames[i];
    // se il nome non e' gia' stato inserito lo aggiungo
    if (std::find(names.begin(), names.end(), name) == names.end())
      names.push_back(name);
  }
}

//-------------------------------------------------------------------
/*! Set the the path \b fp as current project path.\n
        \b fp must be an absolute path.*/
void TProjectManager::setCurrentProjectPath(const TFilePath &fp) {
  assert(TProject::isAProjectPath(fp));
  currentProjectPath = ::to_string(fp.getWideString());
  currentProject     = TProjectP();
  notifyListeners();
}

//-------------------------------------------------------------------
/*! Returns the current project path.\n
        The project path, usually, is setted in key registry. If a current
   project path isn't setted,
        TProject::SandboxProjectName is setted as current project.
*/
TFilePath TProjectManager::getCurrentProjectPath() {
  TFilePath fp(currentProjectPath);
  if (fp == TFilePath())
    fp = projectNameToProjectPath(TProject::SandboxProjectName);
  if (!TProject::isAProjectPath(fp)) {
    // in Toonz 5.1 e precedenti era un project name
    if (!fp.isAbsolute()) fp = getProjectPathByName(fp);
  }
  fp = searchProjectPath(fp.getParentDir());
  if (!TFileStatus(fp).doesExist())
    fp     = projectNameToProjectPath(TProject::SandboxProjectName);
  fp       = getLatestVersionProjectPath(fp);
  string s = ::to_string(fp);
  if (s != (string)currentProjectPath) currentProjectPath = s;
  return fp;
}

//-------------------------------------------------------------------
/*! Returns the current TProject.\n
        If a current TProject() doesn't exist, load the project in the the
   current project path.
*/
TProjectP TProjectManager::getCurrentProject() {
  if (currentProject.getPointer() == 0) {
    TFilePath fp = getCurrentProjectPath();
    assert(TProject::isAProjectPath(fp));
    currentProject = new TProject();
    currentProject->load(fp);
  }
  return currentProject;
}

//-------------------------------------------------------------------
/*! Returns the TProjectP in which the specified \b scenePath is saved.\n
        Returns 0 if \b scenePath isn't a valid scene, or isn't saved in a valid
   folder of a project root.
        \note \b scenePath must be an absolute path.\n
        Creates a new TProject. The caller gets ownership.*/
TProjectP TProjectManager::loadSceneProject(const TFilePath &scenePath) {
  // cerca il file scenes.xml nella stessa directory della scena
  // oppure in una
  // directory superiore

  TFilePath folder = scenePath.getParentDir();
  TFilePath sceneDesc;
  bool found = true;
  for (;;) {
    sceneDesc = folder + "scenes.xml";
    if (TFileStatus(sceneDesc).doesExist()) break;
    if (folder.isRoot()) {
      found = false;
      break;
    }
    folder = folder.getParentDir();
  }

  // legge il path (o il nome) del progetto
  TFilePath projectPath;
  if (found) {
    try {
      TIStream is(sceneDesc);
      string tagName;
      is.matchTag(tagName);
      string type = is.getTagAttribute("type");
      TFilePath projectFolderPath;
      is >> projectFolderPath;
      if (type == "") {
        projectFolderPath = TFilePath("..");
      }
      is.matchEndTag();
      projectPath = makeAbsolute(folder, projectFolderPath);

      TFilePath path;
      for (int i = 0; i < prjSuffixCount; ++i) {
        path =
            projectPath + (projectPath.getWideName() + prjSuffix[i] + xmlExt);
        if (TFileStatus(path).doesExist()) break;
      }

      projectPath = path;

    } catch (...) {
    }
    if (projectPath == TFilePath()) return 0;
  } else
    projectPath = getSandboxProjectPath();

  if (!TProject::isAProjectPath(projectPath)) {
    // in Toonz 5.1 e precedenti era un project name
    if (!projectPath.isAbsolute())
      projectPath = getProjectPathByName(projectPath);
    else
      return 0;
  }
  if (!TFileStatus(projectPath).doesExist()) return 0;

  TProject *project = new TProject();
  project->load(projectPath);
  return project;
}

//-------------------------------------------------------------------

void TProjectManager::notifyListeners() {
  for (std::set<Listener *>::iterator i = m_listeners.begin();
       i != m_listeners.end(); ++i)
    (*i)->onProjectSwitched();
}

//-------------------------------------------------------------------

void TProjectManager::notifyProjectChanged() {
  for (std::set<Listener *>::iterator i = m_listeners.begin();
       i != m_listeners.end(); ++i)
    (*i)->onProjectChanged();
}

//-------------------------------------------------------------------
/*! Adds \b listener to the listners container.*/
void TProjectManager::addListener(Listener *listener) {
  m_listeners.insert(listener);
}

//-------------------------------------------------------------------
/*! Removes \b listener from the listners container.*/
void TProjectManager::removeListener(Listener *listener) {
  m_listeners.erase(listener);
}

//-------------------------------------------------------------------
/*! Initializes the specified \b scene using the TSceneProperties of the current
   project.\n
        \see TSceneProperties
*/
void TProjectManager::initializeScene(ToonzScene *scene) {
  TProject *project       = scene->getProject();
  TSceneProperties *sprop = scene->getProperties();

  TFilePath currentProjectPath = getCurrentProjectPath();
  project->load(currentProjectPath);

  sprop->assign(&project->getSceneProperties());
  CleanupParameters::GlobalParameters.assign(
      project->getSceneProperties().getCleanupParameters());

  // scene->setProject(this);
  scene->setUntitled();
  sprop->cloneCamerasTo(scene->getTopXsheet()->getStageObjectTree());
  sprop->onInitialize();
  // scene->save(scene->getScenePath());
}

//-------------------------------------------------------------------
/*! Saves the TSceneProperties of the specified scene in the current project.*/
void TProjectManager::saveTemplate(ToonzScene *scene) {
  TSceneProperties props;
  props.assign(scene->getProperties());
  props.cloneCamerasFrom(scene->getXsheet()->getStageObjectTree());

  // camera capture's "save in" path is saved in env, not in the project
  props.setCameraCaptureSaveInPath(TFilePath());

  TProjectP currentProject = getCurrentProject();
  currentProject->setSceneProperties(props);
  currentProject->save();
}

//-------------------------------------------------------------------
/*! Creates the standard project folder "sandbox" if it doesn't exist.*/
void TProjectManager::createSandboxIfNeeded() {
  TFilePath path = getSandboxProjectPath();
  if (!TFileStatus(path).doesExist()) {
    TProjectP project = createStandardProject();
    try {
      project->save(path);
    } catch (...) {
    }
  }
}

//-------------------------------------------------------------------
/*! Create a standard project.\n
        A standard project is a project containing the standard named and
   constant folder.
        \see TProject. */
TProjectP TProjectManager::createStandardProject() {
  TProject *project = new TProject();
  // set default folders (+drawings, ecc.)
  std::vector<std::string> names;
  getFolderNames(names);
  std::vector<std::string>::iterator it;
  for (it = names.begin(); it != names.end(); ++it) project->setFolder(*it);
  return project;
}

//! Return the absolute path of the standard folder "sandbox".
TFilePath TProjectManager::getSandboxProjectFolder() {
  return getSandboxProjectPath().getParentDir();
}
//! Return the absolute path of the standard project "sandbox_prj6.xml" file.
TFilePath TProjectManager::getSandboxProjectPath() {
  return getProjectPathByName(TProject::SandboxProjectName);
}

bool TProjectManager::isProject(const TFilePath &projectFolder) {
  TFilePath projectPath = projectFolderToProjectPath(projectFolder);
  return TFileStatus(projectPath).doesExist();
}
