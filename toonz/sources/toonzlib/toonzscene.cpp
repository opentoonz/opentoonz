

#include "toonz/toonzscene.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/sceneproperties.h"
#include "toonz/stage.h"
#include "toonz/stagevisitor.h"
#include "toonz/levelset.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/observer.h"
#include "toonz/namebuilder.h"
#include "toonz/tproject.h"
#include "toonz/toonzimageutils.h"
#include "toonz/childstack.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/sceneresources.h"
#include "toonz/preferences.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "timagecache.h"
#include "tstream.h"
#include "tstreamexception.h"
#include "tofflinegl.h"
#include "tenv.h"
#include "tsystem.h"
#include "tfiletype.h"
#include "tlevel_io.h"
#include "ttoonzimage.h"
#include "tlogger.h"
#include "tvectorimage.h"
#include "tcontenthistory.h"
#include "toutputproperties.h"
#include "trop.h"

TOfflineGL *currentOfflineGL = 0;

#include <QProgressDialog>
//=============================================================================
// Utility functions
//=============================================================================
namespace {

const VersionNumber l_currentVersion(71, 0);

//-----------------------------------------------------------------------------

std::string getFolderName(int levelType) {
  switch (levelType) {
  case TZI_XSHLEVEL:
    return TProject::Inputs;
  case PLI_XSHLEVEL:
    return TProject::Drawings;
  case TZP_XSHLEVEL:
    return TProject::Drawings;
  case OVL_XSHLEVEL:
    return TProject::Extras;
  default:
    return TProject::Extras;
  }
}

//-----------------------------------------------------------------------------

/*
TFilePath adjustTypeAndFrame(int levelType, TFilePath fp)
{
  switch(levelType)
  {
    case TZI_XSHLEVEL:
      if(fp.getType()=="") fp=fp.withType("tif");
      return fp.withFrame(TFrameId::EMPTY_FRAME);
    case PLI_XSHLEVEL:
      return fp.withFrame(TFrameId::NO_FRAME)
               .withType("pli");
    case TZP_XSHLEVEL:
      return fp.withFrame(TFrameId::NO_FRAME)
               .withType("tlv");
    case OVL_XSHLEVEL:
    default:
      if(fp.getType()=="") fp=fp.withType("png");
      return fp;
  }
}
*/

//-----------------------------------------------------------------------------

TFilePath getUntitledScenesDir() {
  return ToonzFolder::getCacheRootFolder() + "temp";
}

//-----------------------------------------------------------------------------

// se path = head + tail ritorna true e assegna head
bool checkTail(TFilePath path, TFilePath tail, TFilePath &head) {
  for (;;) {
    if (tail == TFilePath()) {
      head = path;
      return true;
    }
    if (path == TFilePath()) return false;
    if (path.withoutParentDir() != tail.withoutParentDir()) return false;
    path = path.getParentDir();
    tail = tail.getParentDir();
  }
}

//-----------------------------------------------------------------------------

void makeSceneIcon(ToonzScene *scene) {
  TDimension cameraSize = scene->getCurrentCamera()->getRes();

  int maxSize               = 128;
  TDimension iconCameraSize = cameraSize;
  if (cameraSize.lx > cameraSize.ly) {
    iconCameraSize.ly = maxSize * cameraSize.ly / cameraSize.lx;
    iconCameraSize.lx = maxSize;
  } else {
    iconCameraSize.lx = maxSize * cameraSize.lx / cameraSize.ly;
    iconCameraSize.ly = maxSize;
  }

  TRaster32P ras(iconCameraSize);
  TPixel32 bgColor = scene->getProperties()->getBgColor();
  ras->fill(bgColor);
  scene->renderFrame(ras, 0);

  TFilePath iconPath = scene->getIconPath();
  if (TSystem::touchParentDir(iconPath)) TImageWriter::save(iconPath, ras);
}

//-----------------------------------------------------------------------------

void deleteUntitledScene(const TFilePath &fp) {
  if (TFileStatus(fp).isDirectory()) {
    TFilePath tempDir = getUntitledScenesDir();
    if (TFileStatus(tempDir).isDirectory() &&
        tempDir.isAncestorOf(fp))  // per sicurezza
    {
      try {
        TSystem::rmDirTree(fp);
      } catch (...) {
        assert(0);
      }
    } else
      assert(0);
  }
}

//-----------------------------------------------------------------------------
/*-- TODO: オプション化して復活させるか、検討のこと --*/
/*
void saveBackup(const TFilePath &fp)
{
  if(!TFileStatus(fp).doesExist()) return;
        wstring sceneName = fp.getWideName();
  TFilePath bckDir = fp.getParentDir() + "backups" + sceneName;
  if(!TFileStatus(bckDir).doesExist())
  {
    try {TSystem::mkDir(bckDir);}
    catch(...) {return;}
  }

  std::map<int, TFilePath> oldBackups;
  TFilePathSet lst = TSystem::readDirectory(bckDir);
  for(TFilePathSet::iterator it = lst.begin(); it != lst.end(); ++it)
  {
    TFilePath fp2 = *it;
    if(fp2.getType() != "tnz" && fp2.getType() != "tab") continue;
    wstring name = fp2.getWideName();
    if(name.find_first_of(L"0123456789") == wstring::npos) continue;
                int i = name.find(sceneName);
                if(i != wstring::npos)
                        name = name.substr(sceneName.size()+1);
    if(name == L"" || name.find_first_not_of(L"0123456789") != wstring::npos)
      continue;
    int index = toInt(name);
    assert(0<index);
    assert(oldBackups.count(index)==0);
    oldBackups[index] = fp2;
  }

  int m = 3;
  if((int)oldBackups.size()>m)
  {
    std::map<int, TFilePath>::iterator it = oldBackups.begin();
    for(int i=0;i+m<(int)oldBackups.size();i++)
    {
      assert(it != oldBackups.end());
      TFilePath toKill = it->second;
      try {TSystem::deleteFile(toKill); } catch(...) {}
      ++it;
    }
  }

  TFilePath bckFp;
  if(oldBackups.empty())
  {
    if(fp.getType() == "tnz")
      bckFp = bckDir + TFilePath(sceneName + L"_1.tnz");
    else if(fp.getType() == "tab")
      bckFp = bckDir + TFilePath(sceneName + L"_1.tab");
  }
  else
  {
    int id = oldBackups.rbegin()->first + 1;
    if(fp.getType() == "tnz")
      bckFp = bckDir + TFilePath(sceneName + L"_" + ::to_wstring(id) + L".tnz");
    else if(fp.getType() == "tab")
      bckFp = bckDir + TFilePath(sceneName + L"_" + ::to_wstring(id) + L".tab");
  }

  TSystem::renameFile(bckFp, fp);
}
*/

//-----------------------------------------------------------------------------

// serve per correggere un problema verificatosi da Bianco (con la beta3)
// vengono create coppie di livelli con lo stesso nome e in genere path diverso

void fixBiancoProblem(ToonzScene *scene, TXsheet *xsh) {
  TLevelSet *levelSet = scene->getLevelSet();
  std::set<TXsheet *> visited, tovisit;
  tovisit.insert(xsh);
  while (!tovisit.empty()) {
    xsh = *tovisit.begin();
    xsh->setScene(scene);  // sound problem
    visited.insert(xsh);
    tovisit.erase(xsh);
    int c0 = 0, c1 = xsh->getColumnCount() - 1;
    for (int c = c0; c <= c1; c++) {
      if (xsh->isColumnEmpty(c)) continue;
      TXshColumn *column = xsh->getColumn(c);
      if (!column || !column->getLevelColumn()) continue;
      TXshLevelColumn *lcolumn = column->getLevelColumn();
      int r0 = 0, r1 = -1;
      lcolumn->getRange(r0, r1);
      for (int r = r0; r <= r1; r++) {
        TXshCell cell = lcolumn->getCell(r);
        if (cell.isEmpty()) continue;
        TXshLevel *xl = cell.m_level.getPointer();
        scene->getLevelSet()->insertLevel(xl);  // per sicurezza
        xl->setScene(scene);
        if (TXshChildLevel *childLevel = xl->getChildLevel()) {
          TXsheet *childXsh = childLevel->getXsheet();
          if (visited.count(childXsh) > 0) continue;
          tovisit.insert(childXsh);
        } else {
          TXshLevel *xl2 = levelSet->getLevel(xl->getName());
          if (xl2) {
            if (xl2 != xl) {
              cell.m_level = xl2;
              lcolumn->setCell(r, cell);
            }
          } else {
            xl->setScene(scene);
            levelSet->insertLevel(xl);
          }
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================

static void deleteAllUntitledScenes() {
  TFilePath tempDir = getUntitledScenesDir();
  try {
    if (TFileStatus(tempDir).isDirectory()) {
      TFilePathSet fps;
      TSystem::readDirectory(fps, tempDir);
      TFilePathSet::iterator fpsIt;
      for (fpsIt = fps.begin(); fpsIt != fps.end(); ++fpsIt) {
        TFilePath fp = *fpsIt;
        if (TFileStatus(fp).isDirectory() &&
            fp.getName().find("untitled") != -1)
          TSystem::rmDirTree(fp);
      }
    }
  } catch (...) {
  }
}

//=============================================================================
// ToonzScene

ToonzScene::ToonzScene() : m_contentHistory(0), m_isUntitled(true) {
  m_childStack = new ChildStack(this);
  m_properties = new TSceneProperties();
  m_levelSet   = new TLevelSet();
  m_project    = new TProject();
  m_project->addRef();
}

//-----------------------------------------------------------------------------

ToonzScene::~ToonzScene() {
  delete m_properties;
  delete m_levelSet;
  delete m_childStack;
  delete m_contentHistory;

  assert(m_project);
  if (m_project) m_project->release();
}

//-----------------------------------------------------------------------------

void ToonzScene::setSceneName(std::wstring name) {
  m_scenePath = m_scenePath.withName(name);
}

//-----------------------------------------------------------------------------

void ToonzScene::clear() {
  if (isUntitled()) deleteUntitledScene(getScenePath().getParentDir());

  m_childStack->clear();

  m_scenePath                  = TFilePath();
  TSceneProperties *properties = m_properties;
  m_properties                 = new TSceneProperties();
  delete properties;
  m_levelSet->clear();
}

//-----------------------------------------------------------------------------

void ToonzScene::setProject(TProject *project) {
  assert(project);

  if (project != m_project) {
    if (project) project->addRef();
    if (m_project) m_project->release();
    m_project = project;
  }
}

//-----------------------------------------------------------------------------

TProject *ToonzScene::getProject() const { return m_project; }

//-----------------------------------------------------------------------------

void ToonzScene::setScenePath(const TFilePath &fp) {
  m_scenePath  = fp;
  m_isUntitled = false;
}

//-----------------------------------------------------------------------------

bool ToonzScene::isUntitled() const {
  return m_scenePath == TFilePath() || m_isUntitled;
}

//-----------------------------------------------------------------------------

void ToonzScene::load(const TFilePath &path, bool withProgressDialog) {
  loadNoResources(path);              // This loads a version number ..
  loadResources(withProgressDialog);  // .. this uses the version number ..

  setVersionNumber(
      VersionNumber());  // .. but scene instances in memory do not retain
}  // a version number beyond resource loading

//-----------------------------------------------------------------------------

// Funzioncina veloce per trovare il numero di frame di una scena senza caricare
// nulla. (implementato per Toonz 6.0 beta 1)
int ToonzScene::loadFrameCount(const TFilePath &fp) {
  TIStream is(fp);
  if (!is) throw TException(fp.getWideString() + L": Can't open file");
  try {
    // Leggo il primo tag (<tnz/tab>) ed estraggo il framecount (se c'e')
    std::string tagName = "";
    if (!is.matchTag(tagName)) throw TException("Bad file format");

    if (tagName == "tab" || tagName == "tnz") {
      int frameCount;
      if (is.getTagParam("framecount", frameCount))
        return frameCount;
      else
        return 0;
    } else
      throw TException("Bad file format");
  } catch (TException &e) {
    throw TIStreamException(is, e);
  } catch (...) {
    throw TIStreamException(is);
  }

  return 0;
}

//-----------------------------------------------------------------------------

void ToonzScene::loadNoResources(const TFilePath &fp) {
  clear();

  TProjectManager *pm    = TProjectManager::instance();
  TProjectP sceneProject = pm->loadSceneProject(fp);
  if (!sceneProject) return;

  setProject(sceneProject.getPointer());

  loadTnzFile(fp);
  getXsheet()->updateFrameCount();
}

//-----------------------------------------------------------------------------
/*--
 * プログレスダイアログをGUIからの実行時でのみ表示させる。tcomposerから実行の場合は表示させない
 * --*/
void ToonzScene::loadResources(bool withProgressDialog) {
  /*--- m_levelSet->getLevelCount()が10個以上のとき表示させる　---*/
  QProgressDialog *progressDialog = 0;
  if (withProgressDialog && m_levelSet->getLevelCount() >= 10) {
    progressDialog = new QProgressDialog("Loading Scene Resources", "", 0,
                                         m_levelSet->getLevelCount());
    progressDialog->setModal(true);
    progressDialog->setAutoReset(
        true); /*--maximumに到達したら自動でresetを呼ぶ--*/
    progressDialog->setAutoClose(true); /*--resetが呼ばれたら自動で閉じる--*/
    progressDialog->setAttribute(Qt::WA_DeleteOnClose,
                                 true); /*--閉じたら自動でDeleteされる--*/
    progressDialog->setCancelButton(0);
    progressDialog->setValue(0);
    progressDialog->show();
  }

  int i;
  for (i = 0; i < m_levelSet->getLevelCount(); i++) {
    if (progressDialog) progressDialog->setValue(i + 1);

    TXshLevel *level = m_levelSet->getLevel(i);
    try {
      level->load();
    } catch (...) {
    }
  }
  getXsheet()->updateFrameCount();
}

//-----------------------------------------------------------------------------

void ToonzScene::loadTnzFile(const TFilePath &fp) {
  bool reading22 = false;
  TIStream is(fp);
  if (!is) throw TException(fp.getWideString() + L": Can't open file");
  try {
    std::string tagName = "";
    if (!is.matchTag(tagName)) throw TException("Bad file format");

    if (tagName == "tab" || tagName == "tnz") {
      std::string rootTagName = tagName;
      std::string v           = is.getTagAttribute("version");
      VersionNumber versionNumber(0, 0);
      int k = v.find(".");
      if (k != (int)std::string::npos && 0 < k && k < (int)v.length()) {
        versionNumber.first  = std::stoi(v.substr(0, k));
        versionNumber.second = std::stoi(v.substr(k + 1));
      }
      if (versionNumber == VersionNumber(0, 0))
        throw TException("Bad version number :" + v);
      setVersionNumber(versionNumber);
      is.setVersion(versionNumber);
      while (is.matchTag(tagName)) {
        if (tagName == "generator") {
          std::string program = is.getString();
          reading22           = program.find("2.2") != std::string::npos;
        } else if (tagName == "properties")
          m_properties->loadData(is, false);
        else if (tagName == "palette")  // per compatibilita' beta1
        {
          TPalette *palette = new TPalette;
          is >> *palette;
          delete palette;
        } else if (tagName == "levelSet")
          m_levelSet->loadData(is);
        else if (tagName == "levels") {
          // obsoleto
          if (!reading22) assert(0);
          while (!is.eos()) {
            TPersist *p = 0;
            is >> p;
            TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
            if (xshLevel) {
              xshLevel->setScene(this);
              TXshSimpleLevel *sl = xshLevel->getSimpleLevel();
              if (reading22 && sl && sl->getPath() == TFilePath()) {
                sl->setType(PLI_XSHLEVEL);
                sl->setPath(TFilePath("+drawings/") +
                            (sl->getName() + L".pli"));
              }
              m_levelSet->insertLevel(xshLevel);
            }
          }
        } else if (tagName == "xsheet")
          is >> *getXsheet();
        else if (tagName == "history") {
          std::string historyData, s;
          while (!is.eos()) {
            is >> s;
            historyData += s;
          }
          TContentHistory *history = getContentHistory(true);
          history->deserialize(QString::fromStdString(historyData));
        } else
          throw TException(tagName + " : unexpected tag");

        if (!is.matchEndTag()) throw TException(tagName + " : missing end tag");
      }
      if (!is.matchEndTag())
        throw TException(rootTagName + " : missing end tag");
    } else
      throw TException("Bad file format");

    setScenePath(fp);

    for (int i = 0; i < m_levelSet->getLevelCount(); i++)
      m_levelSet->getLevel(i)->setScene(this);

  } catch (TException &e) {
    throw TIStreamException(is, e);
  } catch (...) {
    throw TIStreamException(is);
  }

  m_properties->cloneCamerasTo(getXsheet()->getStageObjectTree());
  fixBiancoProblem(this, getXsheet());
}

//-----------------------------------------------------------------------------

// saveUntitled() viene chiamata subito dopo newScene
// serve principalmente come lock per evitare che vengano create due scene
// con lo stesso nome.

void ToonzScene::setUntitled() {
  m_isUntitled               = true;
  const std::string baseName = "untitled";
  TFilePath tempDir          = getUntitledScenesDir();
  if (TFileStatus(tempDir).doesExist() == false) {
    try {
      TSystem::mkDir(tempDir);
    } catch (...) {
    }
  }

  std::string name = baseName;
  if (TFileStatus(tempDir + name).doesExist()) {
    int count = 2;
    do {
      name = baseName + std::to_string(count++);
    } while (TFileStatus(tempDir + name).doesExist());
  }
  TFilePath fp = tempDir + name + (name + ".tnz");
  try {
    TSystem::touchParentDir(fp);
  } catch (...) {
    assert(0);
  }
  m_scenePath = fp;
}

//-----------------------------------------------------------------------------

// When saving as sub-xsheet, the sub becomes top. So, its cameras must be
// associated to the scene properties.
class CameraRedirection {
  ToonzScene *m_scene;
  TXsheet *m_xsh;

public:
  CameraRedirection(ToonzScene *scene, TXsheet *xsh)
      : m_scene(scene), m_xsh(xsh) {
    if (!xsh) xsh = m_scene->getTopXsheet();

    m_scene->getProperties()->cloneCamerasFrom(xsh->getStageObjectTree());
  }

  ~CameraRedirection() {
    if (m_xsh)
      m_scene->getProperties()->cloneCamerasFrom(
          m_scene->getTopXsheet()->getStageObjectTree());
  }
};

//-----------------------------------------------------------------------------

void ToonzScene::save(const TFilePath &fp, TXsheet *subxsh) {
  TFilePath oldScenePath = getScenePath();
  TFilePath newScenePath = fp;

  CameraRedirection redir(this, subxsh);

  bool wasUntitled = isUntitled();

  setScenePath(fp);

  TFileStatus fs(newScenePath);
  if (fs.doesExist() && !fs.isWritable())
    throw TSystemException(newScenePath,
                           "The scene cannot be saved: it is a read only "
                           "scene.\n All resources have been saved.");

  TFilePath scenePath = decodeFilePath(fp);

  // if(TFileStatus(scenePath).doesExist()) saveBackup(scenePath);

  TSystem::touchFile(scenePath);
  makeSceneIcon(this);

  // TOStream os(scenePath, compressionEnabled);
  TOStream os(scenePath, false);
  if (!os.checkStatus()) throw TException("Could not open file");

  TXsheet *xsh      = subxsh;
  if (xsh == 0) xsh = m_childStack->getTopXsheet();

  std::map<std::string, std::string> attr;
  attr["version"] =
      (QString::number(l_currentVersion.first) +
       "."  // From now on, version numbers in saved files will have
       + QString::number(
             l_currentVersion.second))  // the signature "MAJOR.MINOR", where:
          .toStdString();               //
  attr["framecount"] =
      QString::number(  //    MAJOR = Toonz version number * 10 (eg 7.0 => 70)
          xsh->getFrameCount())
          .toStdString();  //    MINOR = Reset to 0 after each major increment,
                           //    and
                           //            advancing on its own when fixing bugs.
  os.openChild("tnz", attr);

  os.child("generator") << TEnv::getApplicationFullName();
  os.openChild("properties");
  m_properties->saveData(os);
  os.closeChild();

  if (subxsh) {
    std::set<TXshLevel *> saveSet;
    subxsh->getUsedLevels(saveSet);
    m_levelSet->setSaveSet(saveSet);
  }
  os.openChild("levelSet");
  m_levelSet->saveData(os);
  os.closeChild();
  std::set<TXshLevel *> emptySaveSet;
  m_levelSet->setSaveSet(emptySaveSet);

  os.openChild("xsheet");
  os << *xsh;
  os.closeChild();

  if (getContentHistory()) {
    os.openChild("history");
    QString data = getContentHistory()->serialize();
    int i        = 0, j;
    // non scrivo tutta la std::string di seguito per evitare problemi se
    // diventa
    // troppo lunga. Cerco di spezzarla in modo che sia "bella da leggere" nel
    // tnz
    while ((j = data.indexOf("||", i)) >= i) {
      os << data.mid(i, j - i + 1).toStdWString();
      os.cr();
      i = j + 1;
    }
    os << data.mid(i).toStdWString();
    os.closeChild();
  }

  os.closeChild();
  bool status = os.checkStatus();
  if (!status) throw TException("Could not complete the save");

  if (subxsh) {
    setScenePath(oldScenePath);
    if (wasUntitled) setUntitled();
  } else {
    if (wasUntitled) deleteUntitledScene(oldScenePath.getParentDir());
  }
}

//-----------------------------------------------------------------------------

int ToonzScene::getFrameCount() const {
  TXsheet *xsh = getXsheet();
  return xsh ? xsh->getFrameCount() : 0;
}

//-----------------------------------------------------------------------------

void ToonzScene::renderFrame(const TRaster32P &ras, int row, const TXsheet *xsh,
                             bool checkFlags) const {
  if (xsh == 0) xsh = getXsheet();

  TCamera *camera        = xsh->getStageObjectTree()->getCurrentCamera();
  TDimension cameraRes   = camera->getRes();
  TDimensionD cameraSize = camera->getSize();

  // voglio che la camera sia completamente contenuta dentro raster
  double sx = (double)ras->getLx() / (double)cameraSize.lx;
  double sy = (double)ras->getLy() / (double)cameraSize.ly;
  double sc = (sx < sy) ? sx : sy;

  const TAffine &cameraAff =
      xsh->getPlacement(xsh->getStageObjectTree()->getCurrentCameraId(), row);
  const TAffine &viewAff = TScale(sc / Stage::inch) * cameraAff.inv();

  TRect clipRect(ras->getBounds());
  TOfflineGL ogl(ras->getSize());
  currentOfflineGL = &ogl;

  ogl.makeCurrent();
  {
    glTranslated(0.5 * ras->getLx(), 0.5 * ras->getLy(), 0.0);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImagePainter::VisualSettings vs;
    vs.m_plasticVisualSettings.m_drawMeshesWireframe = false;

    Stage::RasterPainter painter(ras->getSize(), viewAff, clipRect, vs,
                                 checkFlags);
    Stage::visit(painter, const_cast<ToonzScene *>(this),
                 const_cast<TXsheet *>(xsh), row);

    painter.flushRasterImages();
    glFlush();

    TRop::over(ras, ogl.getRaster());
  }
  ogl.doneCurrent();

  currentOfflineGL = 0;
}

//-----------------------------------------------------------------------------

//! Performs a camera-stand render of the specified xsheet in the specified
//! placedRect,
//! with known world/placed reference change - and returns the result in a
//! 32-bit raster.
void ToonzScene::renderFrame(const TRaster32P &ras, int row, const TXsheet *xsh,
                             const TRectD &placedRect,
                             const TAffine &worldToPlacedAff) const {
  // Build reference change affines
  const TAffine &placedToOglRefAff =
      TScale(ras->getLx() / placedRect.getLx(),
             ras->getLy() / placedRect.getLy()) *
      TTranslation(-0.5 * (placedRect.x0 + placedRect.x1),
                   -0.5 * (placedRect.y0 + placedRect.y1));

  const TAffine &cameraAff =
      xsh->getPlacement(xsh->getStageObjectTree()->getCurrentCameraId(), row);

  const TAffine &worldToOglRefAff =
      placedToOglRefAff * worldToPlacedAff * cameraAff.inv();

  TRect clipRect(ras->getBounds());

  TOfflineGL ogl(ras->getSize());
  currentOfflineGL = &ogl;

  ogl.makeCurrent();
  {
    glTranslated(0.5 * ras->getLx(), 0.5 * ras->getLy(), 0.0);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImagePainter::VisualSettings vs;
    vs.m_plasticVisualSettings.m_drawMeshesWireframe = false;

    Stage::RasterPainter painter(ras->getSize(), worldToOglRefAff, clipRect, vs,
                                 false);
    Stage::visit(painter, const_cast<ToonzScene *>(this),
                 const_cast<TXsheet *>(xsh), row);

    painter.flushRasterImages();
    glFlush();

    TRop::over(ras, ogl.getRaster());
  }
  ogl.doneCurrent();

  currentOfflineGL = 0;
}

//-----------------------------------------------------------------------------

TXshLevel *ToonzScene::createNewLevel(int type, std::wstring levelName,
                                      const TDimension &dim, double dpi,
                                      TFilePath fp) {
  TLevelSet *levelSet = getLevelSet();

  if (type == TZI_XSHLEVEL)  // TZI type corresponds to the 'Scan Level'
    type = OVL_XSHLEVEL;     // default option. See Toonz Preferences class.

  if (type == CHILD_XSHLEVEL && levelName == L"") levelName = L"sub";

  // Select a different unique level name in case it already exists (either in
  // scene or on disk)
  {
    const std::unique_ptr<NameBuilder> nameBuilder(
        NameBuilder::getBuilder(levelName));

    for (;;) {
      levelName = nameBuilder->getNext();
      /*-- levelが既にロード済みなら、次の名前を取得 --*/
      if (m_levelSet->getLevel(levelName) != 0) continue;

      /*-- LevelSetの中に同じファイルパスのLevelがあるかをチェック --*/
      if (type != CHILD_XSHLEVEL && type != PLT_XSHLEVEL) {
        if (fp.isEmpty()) fp = getDefaultLevelPath(type, levelName);
        TFilePath actualFp   = decodeFilePath(fp);

        if (TSystem::doesExistFileOrLevel(
                actualFp))  // if(TFileStatus(actualFp).doesExist()) continue;
        {
          fp = TFilePath();
          continue;
        }

        int l, lCount = levelSet->getLevelCount();
        for (l = 0; l != lCount; ++l) {
          TXshLevel *xl = levelSet->getLevel(l);
          if (!xl) continue;

          TXshSimpleLevel *sl = xl->getSimpleLevel();
          if (!sl) continue;

          TFilePath lfp = decodeFilePath(sl->getPath());
          if (actualFp == lfp) break;
        }

        if (l < lCount) {
          /*-- fpが既存のLevelと重複したため、再設定する --*/
          fp = TFilePath();
          continue;
        }
      }
      break;
    }
  }

  TXshLevel *xl = 0;
  if (type == CHILD_XSHLEVEL) {
    TXshChildLevel *cl = new TXshChildLevel(levelName);
    cl->setScene(this);
    cl->getXsheet()->setScene(this);
    xl = cl;

    // Include the project's default cameras
    const TSceneProperties &props =
        TProjectManager::instance()->getCurrentProject()->getSceneProperties();

    props.cloneCamerasTo(cl->getXsheet()->getStageObjectTree());
  } else if (type == PLT_XSHLEVEL) {
    TXshPaletteLevel *pl = new TXshPaletteLevel(levelName);
    pl->setScene(this);
    xl = pl;
  } else {
    TXshSimpleLevel *sl = new TXshSimpleLevel(levelName);

    sl->setScene(this);
    sl->setType(type);
    sl->setPath(fp);
    sl->setDirtyFlag(true);

    if (type == TZP_XSHLEVEL || type == PLI_XSHLEVEL)
      sl->setPalette(new TPalette());

    if (type == OVL_XSHLEVEL)
      sl->setPalette(FullColorPalette::instance()->getPalette(this));

    TPalette *palette = sl->getPalette();
    if (palette && type != OVL_XSHLEVEL) {
      palette->setPaletteName(sl->getName());
      palette->setDirtyFlag(true);
    }

    if (type == TZP_XSHLEVEL || type == OVL_XSHLEVEL) {
      double dpiY = dpi;
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
      if (dim == TDimension()) {
        double w, h;
        Preferences *pref = Preferences::instance();
        if (pref->isNewLevelSizeToCameraSizeEnabled()) {
          TDimensionD camSize = getCurrentCamera()->getSize();
          w                   = camSize.lx;
          h                   = camSize.ly;
          sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
          dpi  = getCurrentCamera()->getDpi().x;
          dpiY = getCurrentCamera()->getDpi().y;
        } else {
          w    = pref->getDefLevelWidth();
          h    = pref->getDefLevelHeight();
          dpi  = pref->getDefLevelDpi();
          dpiY = dpi;
        }

        sl->getProperties()->setImageRes(
            TDimension(tround(w * dpi), tround(h * dpiY)));
      } else
        sl->getProperties()->setImageRes(dim);

      sl->getProperties()->setImageDpi(TPointD(dpi, dpiY));
      sl->getProperties()->setDpi(dpi);
    }

    xl = sl;
  }

  m_levelSet->insertLevel(xl);
  TNotifier::instance()->notify(TCastChange());

  return xl;
}

//-----------------------------------------------------------------------------

TXsheet *ToonzScene::getXsheet() const { return m_childStack->getXsheet(); }

//-----------------------------------------------------------------------------

TXsheet *ToonzScene::getTopXsheet() const {
  return m_childStack->getTopXsheet();
}

//-----------------------------------------------------------------------------

struct LevelType {
  int m_ltype;
  bool m_oldLevelFlag;
  bool m_vectorNotPli;
  std::string m_ext;
};

//-----------------------------------------------------------------------------

static LevelType getLevelType(const TFilePath &fp) {
  LevelType ret;
  ret.m_ltype        = UNKNOWN_XSHLEVEL;
  ret.m_oldLevelFlag = false;
  ret.m_vectorNotPli = false;
  ret.m_ext          = fp.getType();
  std::string format = ret.m_ext;

  TFileType::Type type = TFileType::getInfo(fp);

  switch (type) {
  case TFileType::RASTER_IMAGE:
  case TFileType::RASTER_LEVEL:
  case TFileType::CMAPPED_LEVEL:
    if (format == "tzp" || format == "tzu") {
      ret.m_ltype        = TZP_XSHLEVEL;
      ret.m_oldLevelFlag = true;
      ret.m_ext          = "tlv";
    } else if (format == "tzl" || format == "tlv")
      ret.m_ltype = TZP_XSHLEVEL;
    else if (format == "tzi")
      ret.m_ltype = TZI_XSHLEVEL;
    else
      ret.m_ltype = OVL_XSHLEVEL;
    break;

  case TFileType::VECTOR_LEVEL:
    if (format == "svg") {
      ret.m_vectorNotPli = true;
      ret.m_ext          = "pli";
    }
    /*  if (format == "svg")
            ret.m_ext = "pli";*/
    ret.m_ltype = PLI_XSHLEVEL;
    break;

  case TFileType::AUDIO_LEVEL:
    ret.m_ltype = SND_XSHLEVEL;
    break;

  case TFileType::MESH_IMAGE:
  case TFileType::MESH_LEVEL:
    ret.m_ltype = MESH_XSHLEVEL;
    break;
  }

  return ret;
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::getImportedLevelPath(const TFilePath path) const {
  if (TFileType::getInfo(path) == TFileType::AUDIO_LEVEL)
    return path.withParentDir(TFilePath("+extras"));
  else if (TFileType::getInfo(path) == TFileType::PALETTE_LEVEL)
    return path.withParentDir(TFilePath("+palettes"));

  const LevelType &ltype = getLevelType(path);
  if (ltype.m_ltype == UNKNOWN_XSHLEVEL) return path;

  const std::wstring &levelName = path.getWideName();
  const std::string &ext = path.getType(), &dots = path.getDots();

  TFilePath importedLevelPath =
      getDefaultLevelPath(ltype.m_ltype, levelName).getParentDir() +
      (levelName + ::to_wstring(dots + ext));

  if (dots == "..")
    importedLevelPath = importedLevelPath.withFrame(TFrameId::EMPTY_FRAME);

  if (importedLevelPath.getType() == "tlv")  // Type shouldn't have changed...
    importedLevelPath =
        importedLevelPath.withNoFrame();  // Plus, should be unnecessary...

  return importedLevelPath;
}

//-----------------------------------------------------------------------------

/* tzp,tzu->tlv */
bool ToonzScene::convertLevelIfNeeded(TFilePath &levelPath) {
  LevelType ltype = getLevelType(levelPath);
  TFilePath fp    = levelPath;
  if (ltype.m_vectorNotPli) {
    // livello flash o svg
    levelPath = levelPath.withType("pli");
    TLevelWriterP lw(levelPath);
    TLevelReaderP lr(fp);
    if (!lr) return false;
    TLevelP inLevel = lr->loadInfo();
    if (!inLevel || inLevel->getFrameCount() == 0) return false;
    TLevelP outLevel;
    for (TLevel::Iterator it = inLevel->begin(); it != inLevel->end(); ++it) {
      TVectorImageP img = lr->getFrameReader(it->first)->load();
      if (!img) continue;
      lw->getFrameWriter(it->first)->save(img);
    }
  } else if (ltype.m_oldLevelFlag) {
    TLevelP outLevel;
    // livello Toonz 4.6
    levelPath = TFilePath(levelPath.getParentDir().getWideString() + L"\\" +
                          levelPath.getWideName() + L".tlv");
    if (TSystem::doesExistFileOrLevel(levelPath))
      TSystem::removeFileOrLevel(levelPath);
    TFilePath pltPath = TFilePath(levelPath.getParentDir().getWideString() +
                                  L"\\" + levelPath.getWideName() + L".tpl");
    if (TSystem::doesExistFileOrLevel(pltPath))
      TSystem::removeFileOrLevel(pltPath);

    TLevelWriterP lw(levelPath);
    lw->setIconSize(Preferences::instance()->getIconSize());
    TPaletteP palette =
        ToonzImageUtils::loadTzPalette(fp.withType("plt").withNoFrame());
    TLevelReaderP lr(fp);
    if (!lr) return false;
    TLevelP inLevel = lr->loadInfo();
    if (!inLevel || inLevel->getFrameCount() == 0) return false;
    outLevel->setPalette(palette.getPointer());
    try {
      for (TLevel::Iterator it = inLevel->begin(); it != inLevel->end(); ++it) {
        TToonzImageP img = lr->getFrameReader(it->first)->load();
        if (!img) continue;
        img->setPalette(palette.getPointer());
        lw->getFrameWriter(it->first)->save(img);
      }
    } catch (TException &e) {
      // devo buttare il tlv che stavo salvando!
      QString msg = QString::fromStdWString(e.getMessage());
      if (msg == QString("Old 4.1 Palette")) {
        lw = TLevelWriterP();
        if (TSystem::doesExistFileOrLevel(levelPath))
          TSystem::removeFileOrLevel(levelPath);
        throw e;
      }
    }
    lw = TLevelWriterP();  // bisogna liberare prima lw di outLevel,
    // altrimenti la paletta che lw vuole scrivere e' gia' stata
    // loutLeveliberata.
  }
  return true;
}

//-----------------------------------------------------------------------------

TXshLevel *ToonzScene::loadLevel(const TFilePath &actualPath,
                                 const LevelOptions *levelOptions,
                                 std::wstring levelName,
                                 const std::vector<TFrameId> &fIds) {
  LevelType ltype = getLevelType(actualPath);
  if (ltype.m_ltype == UNKNOWN_XSHLEVEL) return 0;

  TFilePath levelPath = actualPath;

  // Ensure the level name is unique

  if (QString::fromStdWString(levelName).isEmpty()) {
    // if the option is set in the preferences,
    // remove the scene numbers("c####_") from the file name
    levelName = getLevelNameWithoutSceneNumber(levelPath.getWideName());
  }

  NameModifier nm(levelName);
  levelName                                         = nm.getNext();
  while (m_levelSet->hasLevel(levelName)) levelName = nm.getNext();

  // Discriminate sound levels
  if (ltype.m_ltype == SND_XSHLEVEL) {
    TXshSoundLevel *sl = new TXshSoundLevel(levelName);
    sl->setType(ltype.m_ltype);
    sl->setScene(this);
    sl->setPath(codeFilePath(levelPath));

    try {
      sl->load();
    } catch (const std::string &msg)  // Intercepting std::string exceptions
    {
      throw TException(msg);
    }  // from load, and rethrowing... curious!

    m_levelSet->insertLevel(sl);
    return sl;
  } else {
    TXshSimpleLevel *xl = new TXshSimpleLevel(levelName);
    xl->setType(ltype.m_ltype);
    xl->setScene(this);

    if (!convertLevelIfNeeded(levelPath))
      return 0;  // Conversion failed, bail out

    xl->setPath(codeFilePath(levelPath), true);

    try {
      if (fIds.size() != 0)
        xl->load(fIds);
      else
        xl->load();
    } catch (const std::string &msg) {
      throw TException(msg);
    }

    if (ltype.m_oldLevelFlag)
      xl->setDirtyFlag(true);  // Not set on old level formats?

    LevelProperties *lp = xl->getProperties();
    assert(lp);

    if (levelOptions)
      lp->options() = *levelOptions;
    else {
      const Preferences &prefs = *Preferences::instance();
      int formatIdx            = prefs.matchLevelFormat(
          levelPath);  // Should I use actualPath here? It's mostly
                       // irrelevant anyway, it's for old tzp/tzu...
      if (formatIdx >= 0)
        lp->options() = prefs.levelFormat(formatIdx).m_options;
      else {
        // Default subsampling values are assigned from scene properties
        if (xl->getType() == OVL_XSHLEVEL)
          lp->setSubsampling(getProperties()->getFullcolorSubsampling());
        else if (xl->getType() == TZP_XSHLEVEL)
          lp->setSubsampling(getProperties()->getTlvSubsampling());
      }
    }

    if (lp->getDpiPolicy() == LevelProperties::DP_ImageDpi) {
      // We must check whether the image actually has a dpi.
      const TPointD &imageDpi = xl->getImageDpi();

      if (imageDpi == TPointD() ||
          Preferences::instance()->getUnits() == "pixel" ||
          Preferences::instance()->isIgnoreImageDpiEnabled()) {
        // Change to "Custom Dpi" policy and use camera dpi
        TStageObjectId cameraId =
            getXsheet()->getStageObjectTree()->getCurrentCameraId();

        lp->setDpiPolicy(LevelProperties::DP_CustomDpi);
        lp->setDpi(getCurrentCamera()->getDpi());
      } else {
        // Has dpi alright - assign it to custom dpi, too
        lp->setDpi(imageDpi);
      }
    }

    m_levelSet->insertLevel(xl);

    return xl;
  }

  return 0;
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::decodeFilePath(const TFilePath &path) const {
  TProject *project   = getProject();
  bool projectIsEmpty = false;
  TFilePath fp        = path;

  std::wstring head;
  TFilePath tail;
  path.split(head, tail);

  std::string h;
  std::wstring s;
  if (head != L"" && head[0] == L'+') {
    if (TProjectManager::instance()->isTabModeEnabled()) {
      return m_scenePath.getParentDir() +
             (m_scenePath.getWideName() + L"_files") + tail;
    }

    if (TProjectManager::instance()->isTabKidsModeEnabled()) {
      return m_scenePath.getParentDir() + m_scenePath.getWideName() + tail;
    }

    if (projectIsEmpty) {
      TFilePath dir = m_scenePath.getParentDir();
      if (dir.getName() == "scenes")
        return dir.withName(head.substr(1)) + tail;
      else
        return dir + tail;
    }
    if (project) {
      h                       = ::to_string(head.substr(1));
      TFilePath f             = project->getFolder(h);
      if (f != TFilePath()) s = f.getWideString();
    }
  }
  if (s != L"") {
    std::map<std::wstring, std::wstring> table;

    // se la scena e' untitled e l'espansione del path
    // dipende dalla scena (o perche' l'espansione contiene
    // $scenename, $scenepath o perche' si usa il savepath)
    if (m_isUntitled &&
        (s.find(L"$scene") != std::wstring::npos ||
         project->getUseScenePath(h) ||
         fp.getParentDir().getName() == getScenePath().getName())) {
      TFilePath parentDir = getScenePath().getParentDir();
      fp                  = parentDir + head.substr(1) + tail;
    } else {
      TFilePath scenePath = getScenePath();
      TFilePath scenePathRoot;
      if (project)
        scenePathRoot = project->getFolder(TProject::Scenes);
      else
        scenePathRoot = scenePath.getParentDir();

      if (scenePathRoot != TFilePath() && !scenePathRoot.isAbsolute() &&
          project)
        scenePathRoot = project->getProjectFolder() + scenePathRoot;

      if (TSystem::isUNC(scenePath) && !TSystem::isUNC(scenePathRoot))
        scenePathRoot = TSystem::toUNC(scenePathRoot);

      if (scenePathRoot.isAncestorOf(scenePath))
        scenePath = scenePath - scenePathRoot;

      table[L"$scenepath"] = scenePath.withType("").getWideString();
      table[L"$scenename"] = scenePath.withType("").getWideString();

      std::map<std::wstring, std::wstring>::reverse_iterator it;
      for (it = table.rbegin(); it != table.rend(); ++it) {
        std::wstring keyword = it->first;
        int i                = 0;
        for (;;) {
          int j = s.find(keyword, i);
          if (j == (int)std::wstring::npos) break;
          s.replace(j, keyword.length(), it->second);
          i = j;
        }
      }
      fp = TFilePath(s) + tail;
    }
  }

  if (fp != TFilePath() && !fp.isAbsolute() && project)
    fp = project->getProjectFolder() + fp;
  return fp;
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::codeFilePath(const TFilePath &path) const {
  TFilePath fp(path);
  TProject *project = getProject();

  if (project)
    for (int i = 0; i < project->getFolderCount(); i++) {
      TFilePath folderName("+" + project->getFolderName(i));
      TFilePath folderPath = decodeFilePath(folderName);
      if (folderPath.isAncestorOf(fp)) {
        fp = folderName + (fp - folderPath);
        return fp;
      }
    }
  return fp;
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::getDefaultLevelPath(int levelType,
                                          std::wstring levelName) const {
  TProject *project = getProject();
  assert(project);
  TFilePath levelPath;
  QString scanLevelType;
  switch (levelType) {
  case TZI_XSHLEVEL:
    scanLevelType = Preferences::instance()->getScanLevelType();
    levelPath     = TFilePath(levelName + L".." + scanLevelType.toStdWString());
    break;
  case PLI_XSHLEVEL:
    levelPath = TFilePath(levelName).withType("pli");
    break;
  case TZP_XSHLEVEL:
    levelPath = TFilePath(levelName).withType("tlv");
    break;
  case OVL_XSHLEVEL:
    levelPath = TFilePath(levelName + L"..tif");
    break;
  default:
    levelPath = TFilePath(levelName + L"..png");
  }
  std::string folderName = getFolderName(levelType);
  if (project->getUseScenePath(folderName))
    return TFilePath("+" + folderName) + getSavePath() + levelPath;
  else
    return TFilePath("+" + folderName) + levelPath;
}

//-----------------------------------------------------------------------------

const std::wstring savePathString(L"$savepath");

//-----------------------------------------------------------------------------

TFilePath ToonzScene::codeSavePath(TFilePath path) const {
  if (path == TFilePath()) return path;
  TFilePath savePath = getSavePath();
  if (savePath == TFilePath()) return path;  // non dovrebbe succedere mai
  TFilePath filename;
  TFilePath originalPath = path;
  if (savePath.withoutParentDir() != path.withoutParentDir()) {
    TFilePath parentDir = path.getParentDir();
    if (parentDir != TFilePath() && !parentDir.isRoot()) {
      filename = path.withoutParentDir();
      path     = parentDir;
    } else
      return originalPath;
  }

  TFilePath head;
  if (!checkTail(path, savePath, head)) return originalPath;
  if (head.getParentDir() != TFilePath() || head == TFilePath() ||
      head.getWideString()[0] != L'+')
    return originalPath;
  std::string folderName = ::to_string(head.getWideString().substr(1));
  if (!getProject()->getUseScenePath(folderName)) return originalPath;
  return head + savePathString + filename;
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::decodeSavePath(TFilePath path) const {
  std::wstring s = path.getWideString();
  int i          = s.find(savePathString);
  if (i != (int)std::wstring::npos) {
    TFilePath savePath = getSavePath();
    s.replace(i, savePathString.length(), savePath.getWideString());
    return TFilePath(s);
  } else
    return path;
}

//-----------------------------------------------------------------------------

bool ToonzScene::isExternPath(const TFilePath &fp) const {
  TProject *project = m_project;
  assert(project);
  for (int i = 0; i < project->getFolderCount(); i++) {
    if (project->getFolderName(i) == "scenes") continue;

    TFilePath folderPath =
        decodeFilePath(TFilePath("+" + project->getFolderName(i)));
    if (folderPath.isAncestorOf(fp)) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

TCamera *ToonzScene::getCurrentCamera() {
  return getXsheet()->getStageObjectTree()->getCurrentCamera();
}

//-----------------------------------------------------------------------------

TCamera *ToonzScene::getCurrentPreviewCamera() {
  return getXsheet()->getStageObjectTree()->getCurrentPreviewCamera();
}

//-----------------------------------------------------------------------------

TContentHistory *ToonzScene::getContentHistory(bool createIfNeeded) {
  if (!m_contentHistory && createIfNeeded)
    m_contentHistory = new TContentHistory(false);
  return m_contentHistory;
}

//-----------------------------------------------------------------------------

void ToonzScene::getSoundColumns(std::vector<TXshSoundColumn *> &columns) {
  ToonzScene *scene = this;
  std::set<TXsheet *> visited, toVisit;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  visited.insert(xsh);
  toVisit.insert(xsh);
  while (!toVisit.empty()) {
    xsh = *toVisit.begin();
    toVisit.erase(xsh);
    for (int i = 0; i < xsh->getColumnCount(); i++) {
      TXshColumn *column = xsh->getColumn(i);
      if (!column) continue;
      if (TXshSoundColumn *sc = column->getSoundColumn())
        columns.push_back(sc);
      else if (TXshCellColumn *cc = column->getCellColumn()) {
        int r0 = 0, r1 = -1;
        cc->getRange(r0, r1);
        if (!cc->isEmpty() && r0 <= r1) {
          for (int r = r0; r <= r1; r++) {
            TXshCell cell = cc->getCell(r);
            if (cell.m_level && cell.m_level->getChildLevel()) {
              TXsheet *subxsh = cell.m_level->getChildLevel()->getXsheet();
              if (visited.find(subxsh) == visited.end()) {
                visited.insert(subxsh);
                toVisit.insert(subxsh);
              }
            }
          }
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

void ToonzScene::updateSoundColumnFrameRate() {
  std::vector<TXshSoundColumn *> soundColumns;
  getSoundColumns(soundColumns);

  TSceneProperties *properties = getProperties();
  if (!properties) return;

  TOutputProperties *outputProperties = properties->getOutputProperties();
  if (!outputProperties) return;

  double frameRate = outputProperties->getFrameRate();

  int i;
  for (i = 0; i < (int)soundColumns.size(); i++)
    soundColumns[i]->updateFrameRate(frameRate);
}

//-----------------------------------------------------------------------------

TFilePath ToonzScene::getIconPath(const TFilePath &scenePath) {
  return scenePath.getParentDir() + "sceneIcons" +
         (scenePath.getWideName() + L" .png");
}

//-----------------------------------------------------------------------------

// se la scena sta in +scenes/pippo.tnz => pippo
// se la scena sta in +scenes/pluto/pippo.tnz => pluto/pippo
// se la scena e' untitledxxx.tnz => untitledxxx

TFilePath ToonzScene::getSavePath() const {
  std::string sceneName = getScenePath().getName();
  if (isUntitled()) return TFilePath(sceneName);
  TFilePath sceneRoot = decodeFilePath(TFilePath("+" + TProject::Scenes));
  TFilePath scenePath = getScenePath().withType("");
  TFilePath savePath(sceneName);
  if (sceneRoot.isAncestorOf(scenePath)) savePath = scenePath - sceneRoot;
  return savePath;
}

//---------------------------------------------------------------------------

double ToonzScene::shiftCameraX(double val) {
  TStageObjectTree *tree = getXsheet()->getStageObjectTree();

  TStageObject *stageObject = tree->getStageObject(tree->getCurrentCameraId());
  TPointD ret               = stageObject->getOffset();
  stageObject->setOffset(TPointD(ret.x + val, ret.y));
  return ret.x;
}

//----------------------------------------------------------------------------
// if the option is set in the preferences,
// remove the scene numbers("c####_") from the file name

std::wstring ToonzScene::getLevelNameWithoutSceneNumber(std::wstring orgName) {
  if (!Preferences::instance()->isRemoveSceneNumberFromLoadedLevelNameEnabled())
    return orgName;

  QString orgNameQstr = QString::fromStdWString(orgName);

  // do nothing if the file name has less than 7 letters
  if (orgNameQstr.size() <= 6) return orgName;

  QString sceneName = QString::fromStdWString(getSceneName()).left(5);

  // check if the first 5 letters of the level name is the same as the scene
  // name.
  // note that we must consider following both cases; "c0001_hogehoge.tif" and
  // "c0001A_####.tif"
  if (!orgNameQstr.startsWith(sceneName)) return orgName;

  if (!orgNameQstr.contains("_")) return orgName;

  return orgNameQstr.right(orgNameQstr.size() - orgNameQstr.indexOf("_") - 1)
      .toStdWString();
}
