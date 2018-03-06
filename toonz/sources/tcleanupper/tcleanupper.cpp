

// TnzFarm includes
#include "tfarmcontroller.h"

// TnzLib includes
#include "toonz/txshleveltypes.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/levelproperties.h"
#include "toonz/levelupdater.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/toonzscene.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tproject.h"
#include "toonz/tcleanupper.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/tlog.h"
#include "toonz/imagestyles.h"

// TnzBase includes
#include "tcli.h"
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tstopwatch.h"
#include "tthread.h"
#include "tthreadmessage.h"
#include "timagecache.h"
#include "tiio_std.h"
#include "tnzimage.h"
#include "tmsgcore.h"
#include "tpluginmanager.h"
#include "tpalette.h"
#include "tsimplecolorstyles.h"

// Qt includes
#include <QApplication>

using namespace TCli;
using namespace std;

//------------------------------------------------------------------------

inline ostream &operator<<(ostream &out, const wstring &w) {
  return out << ::to_string(w);
}

//------------------------------------------------------------------------

inline ostream &operator<<(ostream &out, const TFilePath &fp) {
  return out << fp.getWideString();
}

//------------------------------------------------------------------------
namespace {

const char *applicationName     = "OpenToonz";
const char *applicationVersion  = "1.2";
const char *applicationFullName = "OpenToonz 1.2";
const char *rootVarName         = "TOONZROOT";
const char *systemVarPrefix     = "TOONZ";

namespace {

CleanupParameters GlobalParameters;
TFilePath CurrentSettingsFile = TFilePath();

//-----------------------------------------------------------------------------

void loadSettings(const TFilePath &settingsFile, CleanupParameters *cp) {
  if (CurrentSettingsFile ==
      TFilePath())  // the parameters were global ...I store them
    GlobalParameters.assign(cp);

  CurrentSettingsFile = settingsFile;

  TIStream *is = new TIStream(settingsFile);
  int minor, major;
  string tagName;

  // Extract file version if any
  is->matchTag(tagName);
  if (tagName == "version") {
    *is >> major >> minor;
    is->matchEndTag();
    is->setVersion(VersionNumber(major, minor));
  } else {
    delete is;
    is = new TIStream(settingsFile);
  }

  cp->loadData(*is, true);

  delete is;
}

//-----------------------------------------------------------------------------

void restoreGlobalSettings(CleanupParameters *cp) {
  if (CurrentSettingsFile == TFilePath()) return;  // already global!

  cp->assign(&GlobalParameters);
  CurrentSettingsFile = TFilePath();
}

}  // namespace

//========================================================================
//
// fatalError
//
//------------------------------------------------------------------------

void fatalError(string msg) {
#ifdef _WIN32
  msg = "Application can't start:\n" + msg;
  DVGui::error(QString::fromStdString(msg));
  // MessageBox(0,msg.c_str(),"Fatal error",MB_ICONERROR);
  exit(1);
#else
  // TODO: Come si fa ad aggiungere un messaggio di errore qui?
  std::cout << msg << std::endl;
  abort();
#endif
}

}  // namespace

#ifdef MACOSX

inline bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\n'; }

//========================================================================
// setToonzFolder
//------------------------------------------------------------------------

// Ritorna il path della variabile passata come secondo argomento
// entrambe vengono lette da un file di testo (filename).

TFilePath setToonzFolder(const TFilePath &filename, std::string toonzVar) {
  Tifstream is(filename);
  if (!is) return TFilePath();

  char buffer[1024];

  while (is.getline(buffer, sizeof(buffer))) {
    // le righe dentro toonzenv.txt sono del tipo
    // export set TOONZPROJECT="....."

    // devo trovare la linea che contiene toonzVar
    char *s = buffer;
    while (isBlank(*s)) s++;
    // Se la riga  vuota, o inizia per # o ! salto alla prossima
    if (*s == '\0' || *s == '#' || *s == '!') continue;
    if (*s == '=') continue;  // errore: nome variabile non c'
    char *t = s;
    // Mi prendo la sottoStringa fino all'=
    while (*t && *t != '=') t++;
    if (*t != '=') continue;  // errore: manca '='
    char *q = t;
    // Torno indietro fino al primo blank
    while (q > s && !isBlank(*(q - 1))) q--;
    if (q == s)
      continue;  // non dovrebbe mai succedere: prima di '=' tutti blanks

    string toonzVarString(q, t);

    // Confronto la stringa trovata con toonzVar, se   lei vado avanti.
    if (toonzVar != toonzVarString) continue;  // errore: stringhe diverse
    s = t + 1;
    // Salto gli spazi
    while (isBlank(*s)) s++;
    if (*s == '\0') continue;  // errore: dst vuoto
    t = s;
    while (*t) t++;
    while (t > s && isBlank(*(t - 1))) t--;
    if (t == s) continue;
    // ATTENZIONE : tolgo le virgolette !!
    string pathName(s + 1, t - s - 2);
    return TFilePath(pathName);
  }

  return TFilePath();
}

#endif

//==============================================================================================

static void prepareToCleanup(TXshSimpleLevel *xl, TPalette *cleanupPalette) {
  assert(xl->getScene());
  if (xl->getProperties()->getSubsampling() != 1) {
    xl->getProperties()->setSubsampling(1);
    xl->invalidateFrames();
  }

  /* int ltype = xl->getType();
if(ltype != TZP_XSHLEVEL)
xl->makeTlv(xl->getScene()->getDefaultParentDir(TZP_XSHLEVEL));
if(xl->getPalette()==0)
{
//PaletteController* pc = TApp::instance()->getPaletteController();
          xl->setPalette(cleanupPalette);
          //if(xl == TApp::instance()->getCurrentLevel()->getLevel())
          //	pc->getCurrentLevelPalette()->setPalette(xl->getPalette());
}*/
}

//==============================================================================================

namespace {
TStopWatch Sw1;
TStopWatch Sw2;

bool UseRenderFarm = false;
string FarmControllerName;
int FarmControllerPort;

TFarmController *FarmController = 0;

string TaskId;
}
//========================================================================
//
// searchLevelsToCleanup
//
// Restituisce l'elenco (ordinato per ordine alfabetico di nome)
// dei livelli TLV(con scannedPath)/TZI/OVL presenti nell'xsheet (o nei
// sottoxsheet)
//
// se selectedOnly = true salta i livelli presenti nelle colonne senza
// "occhietto"
//
//------------------------------------------------------------------------

static void searchLevelsToCleanup(
    std::vector<std::pair<TXshSimpleLevel *, std::set<TFrameId>>> &levels,
    TXsheet *xsh, bool selectedOnly) {
  std::map<wstring, TXshSimpleLevel *> levelTable;
  std::map<wstring, std::set<TFrameId>> framesTable;

  std::set<TXsheet *> visited;
  std::vector<TXsheet *> xsheets;
  xsheets.push_back(xsh);
  visited.insert(xsh);
  while (!xsheets.empty()) {
    TXsheet *xsh = xsheets.back();
    xsheets.pop_back();
    for (int c = 0; c < xsh->getColumnCount(); c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column || column->isEmpty()) continue;
      // if(selectedOnly && !column->isPreviewVisible()) continue;
      int r0 = 0, r1 = -1;
      xsh->getCellRange(c, r0, r1);
      for (int r = r0; r <= r1; r++) {
        TXshCell cell = xsh->getCell(r, c);
        if (cell.isEmpty()) continue;
        if (TXshSimpleLevel *sl = cell.m_level->getSimpleLevel()) {
          if (selectedOnly && !column->isPreviewVisible())  // pezza: se questo
                                                            // "if" veniva fatto
                                                            // sopra(riga
                                                            // commentata)
          // quando si fa save della Scene alla fine della cleanuppata,
          // venivano backuppate le tif non processate e cancellati gli
          // originali!
          // Deliri del levelUPdater; evito di mettere le mani in quel pattume.
          // vinz
          {
            sl->setDirtyFlag(false);
            continue;
          }
          int ltype = sl->getType();
          if (ltype == TZP_XSHLEVEL && sl->getScannedPath() != TFilePath() ||
              ltype == OVL_XSHLEVEL || ltype == TZI_XSHLEVEL) {
            wstring levelName     = sl->getName();
            levelTable[levelName] = sl;
            framesTable[levelName].insert(cell.m_frameId);
          }
        } else if (TXshChildLevel *cl = cell.m_level->getChildLevel()) {
          TXsheet *subXsh = cl->getXsheet();
          if (visited.count(subXsh) == 0) {
            visited.insert(subXsh);
            xsheets.push_back(subXsh);
          }
        }
      }
    }
  }

  assert(levelTable.size() == framesTable.size());

  for (auto const &level : levelTable) {
    auto const it = framesTable.find(level.first);
    if (it == framesTable.end()) {
      continue;
    }
    levels.push_back(std::make_pair(level.second, (*it).second));
  }
}

//------------------------------------------------------------------------------
/*- CleanupDefaultパレットを追加する -*/
static void addCleanupDefaultPalette(TXshSimpleLevel *sl) {
  /*- 元となるパレットはStudioPaletteフォルダに置く -*/
  TFilePath palettePath =
      ToonzFolder::getStudioPaletteFolder() + "cleanup_default.tpl";
  TFileStatus pfs(palettePath);

  if (!pfs.doesExist() || !pfs.isReadable()) {
    wcout << L"CleanupDefaultPalette file: " << palettePath.getWideString()
          << L" is not found!" << endl;
    return;
  }

  TIStream is(palettePath);
  if (!is) {
    cout << "CleanupDefaultPalette file: failed to get TIStream" << endl;
    return;
  }

  string tagName;
  if (!is.matchTag(tagName) || tagName != "palette") {
    cout << "CleanupDefaultPalette file: This is not palette file" << endl;
    return;
  }

  string gname;
  is.getTagParam("name", gname);
  TPalette *defaultPalette = new TPalette();
  defaultPalette->loadData(is);

  sl->getPalette()->setIsCleanupPalette(false);

  TPalette::Page *dstPage = sl->getPalette()->getPage(0);
  TPalette::Page *srcPage = defaultPalette->getPage(0);

  for (int srcIndexInPage = 0; srcIndexInPage < srcPage->getStyleCount();
       srcIndexInPage++) {
    int id = srcPage->getStyleId(srcIndexInPage);

    bool isUsedInCleanupPalette;
    isUsedInCleanupPalette = false;

    for (int dstIndexInPage = 0; dstIndexInPage < dstPage->getStyleCount();
         dstIndexInPage++) {
      if (dstPage->getStyleId(dstIndexInPage) == id) {
        isUsedInCleanupPalette = true;
        break;
      }
    }

    if (isUsedInCleanupPalette)
      continue;

    else {
      int addedId = sl->getPalette()->addStyle(
          srcPage->getStyle(srcIndexInPage)->clone());
      dstPage->addStyle(addedId);
      /*- StudioPalette由来のDefaultPaletteの場合、GrobalNameを消去する -*/
      sl->getPalette()->getStyle(addedId)->setGlobalName(L"");
      sl->getPalette()->getStyle(addedId)->setOriginalName(L"");
    }
  }

  delete defaultPalette;
}

//========================================================================
//
// cleanupLevel
//
// effettua il cleanup del livello. Se il livello e' un fullcolor
// modifica tipo e parametri e crea la palette
//
// se overwrite == false non fa il cleanup dei frames gia' cleanuppati
//
//------------------------------------------------------------------------

static void cleanupLevel(TXshSimpleLevel *xl, std::set<TFrameId> fidsInXsheet,
                         ToonzScene *scene, bool overwrite,
                         TUserLogAppend &m_userLog) {
  prepareToCleanup(xl, scene->getProperties()
                           ->getCleanupParameters()
                           ->m_cleanupPalette.getPointer());

  TCleanupper *cl = TCleanupper::instance();

  TFilePath fp = scene->decodeFilePath(xl->getPath());
  TSystem::touchParentDir(fp);
  cout << "cleanupping " << xl->getName() << " path=" << fp << endl;
  string info = "cleanupping " + ::to_string(xl->getPath());
  LevelUpdater updater(xl);
  m_userLog.info(info);
  DVGui::info(QString::fromStdString(info));
  bool firstImage = true;
  for (auto const &fid : fidsInXsheet) {
    cout << "  " << fid << endl;
    info = "  " + fid.expand();
    m_userLog.info(info);
    int status = xl->getFrameStatus(fid);

    if (0 != (status & TXshSimpleLevel::Cleanupped) && !overwrite) {
      cout << "  skipped" << endl;
      m_userLog.info("  skipped");
      DVGui::info(QString("--skipped frame ") +
                  QString::fromStdString(fid.expand()));
      continue;
    }
    TRasterImageP original = xl->getFrameToCleanup(fid);
    if (!original) {
      string err = "    *error* missed frame";
      m_userLog.error(err);
      cout << err << endl;
      continue;
    }

    CleanupParameters *params = scene->getProperties()->getCleanupParameters();

    if (params->m_lineProcessingMode == lpNone) {
      TRasterImageP ri;
      if (params->m_autocenterType == CleanupTypes::AUTOCENTER_NONE)
        ri = original;
      else {
        bool autocentered;
        ri = cl->autocenterOnly(original, false, autocentered);
        if (!autocentered) {
          m_userLog.error("The autocentering failed on the current drawing.");
          cout << "The autocentering failed on the current drawing." << endl;
        }
      }
      updater.update(fid, ri);
      continue;
    }
    // Obtain the source dpi. Changed it to be done once at the first frame of
    // each level in order to avoid the following problem:
    // If the original raster level has no dpi (such as TGA images), obtaining
    // dpi in every frame causes dpi mismatch between the first frame and the
    // following frames, since the value
    // TXshSimpleLevel::m_properties->getDpi() will be changed to the
    // dpi of cleanup camera (= TLV's dpi) after finishing the first frame.
    if (firstImage) {
      TPointD dpi;
      original->getDpi(dpi.x, dpi.y);
      if (dpi.x == 0 && dpi.y == 0) dpi = xl->getProperties()->getDpi();
      cl->setSourceDpi(dpi);
    }

    CleanupPreprocessedImage *cpi;
    {
      TRasterImageP resampledImage;
      cpi = cl->process(original, firstImage, resampledImage);
    }

    TToonzImageP timage = cl->finalize(cpi, true);
    TPointD dpi(0, 0);
    timage->getDpi(dpi.x, dpi.y);
    if (dpi.x != 0 && dpi.y != 0) xl->getProperties()->setDpi(dpi);

    if (firstImage) addCleanupDefaultPalette(xl);
    firstImage = false;

    timage->setPalette(xl->getPalette());
    xl->setFrameStatus(fid, status | TXshSimpleLevel::Cleanupped);
    xl->setFrame(fid, timage);

    updater.update(fid, timage);

    /*- 1フレーム終わったら、そのフレームのキャッシュは消す -*/
    xl->invalidateFrame(fid);

    delete cpi;
  }
}

//========================================================================
//
// main
//
// usage: tcleanup filename.tnz [-selected][-overwrite]
//
//------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // questo definisce la registry root e inizializza TEnv
  TEnv::setApplication(applicationName, applicationVersion);
  TEnv::setApplicationFullName(applicationFullName);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);
  TSystem::hasMainLoop(false);
  int i;
  for (i = 0; i < argc; i++)  // tmsg must be set as soon as it's possible
  {
    QString str = argv[i];
    if (str == "-tmsg")

    {
      TMsgCore::instance()->connectTo(argv[i + 1]);
      break;
    }
  }
  if (i == argc) TMsgCore::instance()->connectTo("");

// TThread::init();  //For the ImageManager construction
// controllo se la xxxroot e' definita e corrisponde ad un file esistente
#ifdef MACOSX

  // StuffDir

  QFileInfo infoStuff(QString("Toonz 7.1 stuff"));
  TFilePath stuffDirPath(infoStuff.absoluteFilePath().toStdString());
  TEnv::setStuffDir(stuffDirPath);

/*
    TFilePath  stuffDir("/Applications/Toonz 7.1/Toonz 7.1 stuff");

  TEnv::setStuffDir(stuffDir);
*/
#endif

  TFilePath fproot = TEnv::getStuffDir();
  if (fproot == TFilePath())
    fatalError(string("Undefined: \"") + ::to_string(TEnv::getRootVarPath()) +
               "\"");
  if (!TFileStatus(fproot).isDirectory())
    fatalError(string("Directory \"") + ::to_string(fproot) +
               "\" not found or not readable");

  TFilePath lRootDir    = TEnv::getStuffDir() + "toonzfarm";
  TFilePath logFilePath = lRootDir + "tcleanup.log";
  TUserLogAppend m_userLog(logFilePath);

  TFilePathSet fps = ToonzFolder::getProjectsFolders();
  TFilePathSet::iterator fpIt;
  for (fpIt = fps.begin(); fpIt != fps.end(); ++fpIt)
    TProjectManager::instance()->addProjectsRoot(*fpIt);

  TFilePath libraryFolder = ToonzFolder::getLibraryFolder();
  TRasterImagePatternStrokeStyle::setRootDir(libraryFolder);
  TVectorImagePatternStrokeStyle::setRootDir(libraryFolder);
  TPalette::setRootDir(libraryFolder);
  TImageStyle::setLibraryDir(libraryFolder);
  TFilePath cacheRoot                = ToonzFolder::getCacheRootFolder();
  if (cacheRoot.isEmpty()) cacheRoot = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheRoot);

  FilePathArgument srcName("tnzFile", "Scene file");
  SimpleQualifier selectedOnlyOption("-onlyvisible", "Selected column only");

  SimpleQualifier overwriteAllOption("-overwriteAll",
                                     "Overwrite all already cleanupped frames");
  SimpleQualifier overwriteNoPaintOption(
      "-overwriteNoPaint",
      "Overwrite only no-paint levels of already cleanupped frames");

  StringQualifier farmData("-farm data", "TFarm Controller");
  StringQualifier idq("-id n", "id");
  StringQualifier tmsg("-tmsg n", "Internal use only");
  Usage usage(argv[0]);
  usage.add(srcName + selectedOnlyOption + overwriteAllOption +
            overwriteNoPaintOption + farmData + idq + tmsg);
  if (!usage.parse(argc, argv)) exit(1);

  TaskId       = idq.getValue();
  string fdata = farmData.getValue();
  if (fdata.empty())
    UseRenderFarm = false;
  else {
    UseRenderFarm         = true;
    string::size_type pos = fdata.find('@');
    if (pos == string::npos)
      UseRenderFarm = false;
    else {
      FarmControllerPort = std::stoi(fdata.substr(0, pos));
      FarmControllerName = fdata.substr(pos + 1);
    }
  }

  if (UseRenderFarm) {
    TFarmControllerFactory factory;
    factory.create(QString::fromStdString(FarmControllerName),
                   FarmControllerPort, &FarmController);
  }

  /*- 画像Read/Writeの関数を登録 -*/
  initImageIo();
  Tiio::defineStd();

  /*- プロジェクトのロード -*/
  TProjectManager *pm = TProjectManager::instance();
  TProjectP project   = pm->loadSceneProject(srcName);

  if (!project) {
    string err = "Couldn't find the project" + project->getName().getName();
    m_userLog.error(err);
    cerr << err << endl;
    return -2;
  }

  cout << "project:" << project->getName() << endl;

  TFilePath fp = srcName;

  /*- CLNファイルを直接指定した場合 -*/
  bool sourceFileIsCleanupSetting = (fp.getType() == "cln");

  bool selectedOnly = selectedOnlyOption;

  ToonzScene *scene = new ToonzScene();

  TImageStyle::setCurrentScene(scene);

  /*- シーンファイルパスを入力した場合 -*/
  if (!sourceFileIsCleanupSetting) {
    try {
      scene->loadNoResources(fp);
    } catch (...) {
      string err = "can't read " + fp.getName();
      m_userLog.error(err);
      cerr << "can't read " << fp << endl;
      TImageCache::instance()->clear(true);
      return -3;
    }
  }
  /*-- CleanupSettingsファイルパスを直接入力した場合 --*/
  else {
    try {
      TProjectManager *pm    = TProjectManager::instance();
      TProjectP sceneProject = pm->loadSceneProject(fp);
      if (!sceneProject) {
        cerr << "can't open project." << endl;
        return -3;
      }
      scene->setProject(sceneProject.getPointer());

      /*- CleanupSettingsファイルに対応するTIFファイルが有るかチェック -*/
      TFilePath tifImagePath = fp.withType("tif");
      std::wcout << L"tifImagePath : " << tifImagePath.getWideString()
                 << std::endl;

      /*-
       * 無ければ、連番画像がソースである可能性がある。tifImagePathを連番に差し替える
       * -*/
      if (!TFileStatus(tifImagePath).doesExist()) {
        tifImagePath =
            tifImagePath.getParentDir() + (tifImagePath.getName() + "..tif");
        std::wcout << "change the source path to : "
                   << tifImagePath.getWideString() << std::endl;
      } else
        std::cout << "tif single image found" << std::endl;

      /*- シーンファイルに対象のTIFファイルをロード -*/
      TXshLevel *xl = scene->loadLevel(tifImagePath);
      if (!xl) {
        std::cout << "failed to load level" << std::endl;
        throw TException("Failed to load level.");
      }
      std::vector<TFrameId> dummy;
      int frameLength = scene->getXsheet()->exposeLevel(0, 0, xl, dummy);
      std::cout << "expose done. frameLength : " << frameLength << std::endl;
    } catch (...) {
      cerr << "can't read Cleanup Settings file " << fp << endl;
      TImageCache::instance()->clear(true);
      return -3;
    }
  }

  // Levels contiene i livelli e i rispettivi frames prsenti nell'xsheet.
  std::vector<std::pair<TXshSimpleLevel *, std::set<TFrameId>>> levels;

  /*- XsheetからCleanupするLevelのリストを得る -*/
  searchLevelsToCleanup(levels, scene->getXsheet(), selectedOnly);
  TSceneProperties *sprop   = scene->getProperties();
  CleanupParameters *params = scene->getProperties()->getCleanupParameters();
  for (int i = 0; i < (int)levels.size(); i++) {
    bool overwrite = overwriteAllOption;

    TXshSimpleLevel *xl = levels[i].first->getSimpleLevel();
    if (!xl) continue;

    /*- CLNファイルパスを取得 -*/
    TFilePath settingsFile =
        xl->getScannedPath().isEmpty() ? xl->getPath() : xl->getScannedPath();
    settingsFile =
        scene->decodeFilePath(settingsFile).withNoFrame().withType("cln");

    /*- CLNファイルがあればそれを用いる。無ければGlobal設定を読み込む -*/
    // TFilePath settingsFile =
    // scene->decodeFilePath(xl->getPath()).withNoFrame().withType("cln");
    if (TFileStatus(settingsFile).doesExist())
      loadSettings(settingsFile, params);
    else
      restoreGlobalSettings(params);

    TCleanupper::instance()->setParameters(params);

    bool lineProcessing = (params->m_lineProcessingMode != lpNone);
    TFilePath tmp       = params->getPath(scene);
    int ltype           = xl->getType();
    assert(ltype == TZP_XSHLEVEL || ltype == TZI_XSHLEVEL ||
           ltype == OVL_XSHLEVEL);

    // target path
    TFilePath targetPath = xl->getPath();
    if (ltype != TZP_XSHLEVEL) {
      if (lineProcessing)
        targetPath =
            targetPath
                .withParentDir(params->getPath(
                    scene))  // scene->getDefaultParentDir(TZP_XSHLEVEL))
                .withNoFrame()
                .withType("tlv");
      else
        targetPath = targetPath.withParentDir(params->getPath(
            scene));  // scene->getDefaultParentDir(TZP_XSHLEVEL));
    } else {
      targetPath = targetPath.withParentDir(params->getPath(
          scene));  // scene->getDefaultParentDir(TZP_XSHLEVEL));
    }

    /*- NoPaintを作る設定のとき、NoPaintの方だけにOverwriteするようにする -*/
    bool isReCleanup;
    isReCleanup = false;

    /*- 再Cleanupのときにパスを元に戻すために用いる -*/
    TFilePath originalLevelPath = TFilePath();

    TFilePath actualTargetPath = scene->decodeFilePath(targetPath);
    /*- すでにLevelがある場合 -*/
    if (TSystem::doesExistFileOrLevel(actualTargetPath) &&
        Preferences::instance()->isSaveUnpaintedInCleanupEnable() &&
        overwriteNoPaintOption) {
      overwrite         = true;
      originalLevelPath = scene->codeFilePath(targetPath);
      /*- パスを書き換え、再Cleanupのフラグを立てる -*/
      isReCleanup = true;
      /*- nopaintフォルダの作成 -*/
      TFilePath nopaintDir = targetPath.getParentDir() + "nopaint";
      if (!TFileStatus(nopaintDir).doesExist()) {
        try {
          TSystem::mkDir(nopaintDir);
        } catch (...) {
          return 0;
        }
      }
      /*- 保存先のパスをnopaintの方にする -*/
      targetPath =
          targetPath.getParentDir() + "nopaint\\" +
          TFilePath(targetPath.getName() + "_np." + targetPath.getType());
    }
    std::wcout << L"targetPath = " << targetPath.getWideString() << std::endl;

    if (lineProcessing) {
      if (ltype != TZP_XSHLEVEL) {
        xl->makeTlv(targetPath);
      } else if (targetPath != xl->getPath()) {
        xl->setPath(targetPath, false);
      }

      xl->setPalette(
          TCleanupper::instance()->createToonzPaletteFromCleanupPalette());
    } else if (!params->getPath(scene).isEmpty()) {
      xl->setScannedPath(xl->getPath());
      xl->setPath(scene->codeFilePath(targetPath), false);
    }

    std::set<TFrameId> fidsInXsheet = levels[i].second;
    assert(fidsInXsheet.size() > 0);

    xl->load();
    cleanupLevel(xl, fidsInXsheet, scene, overwrite, m_userLog);

    /*- Cleanup完了後、Nopaintをnopaintフォルダに保存する -*/
    if (Preferences::instance()->isSaveUnpaintedInCleanupEnable() &&
        !isReCleanup) /*-- 再Cleanupのときはnopaintを作らない --*/
    {
      /*- nopaintフォルダの作成 -*/
      TFilePath nopaintDir = actualTargetPath.getParentDir() + "nopaint";
      if (!TFileStatus(nopaintDir).doesExist()) {
        try {
          TSystem::mkDir(nopaintDir);
        } catch (...) {
          string err = "Can't make directory  " + nopaintDir.getName();
          m_userLog.error(err);
          cerr << "Can't make directory" << endl;
        }
      }

      // Se sono nell'ultimo frame del livello salvo il livello cleanuppato
      // in un file chiamato nome-unpainted (file di backup del cleanup).

      /*- 保存先 -*/
      TFilePath unpaintedLevelPath =
          actualTargetPath.getParentDir() + "nopaint\\" +
          TFilePath(targetPath.getName() + "_np." + targetPath.getType());

      if (TFileStatus(actualTargetPath).doesExist() &&
          (!TFileStatus(unpaintedLevelPath).doesExist() ||
           overwriteAllOption) /*- 全て上書きなら既存のNoPaintに上書きする -*/
          && xl) {
        /*- GUI上でCleanupするときと同様に、Cleanup結果をコピーして作る -*/
        TSystem::copyFile(unpaintedLevelPath, actualTargetPath);
        /*- パレットのコピー -*/
        TFilePath levelPalettePath =
            actualTargetPath.getParentDir() +
            TFilePath(actualTargetPath.getName() + ".tpl");
        TFilePath unpaintedLevelPalettePath =
            levelPalettePath.getParentDir() + "nopaint\\" +
            TFilePath(levelPalettePath.getName() + "_np." +
                      levelPalettePath.getType());
        TSystem::copyFile(unpaintedLevelPalettePath, levelPalettePath);
      }
    }
    /*--
       再Cleanupの場合はXsheet上のパスをオリジナルのもの(NoPaintでないもの)に戻す
       --*/
    else if (isReCleanup) {
      xl->setPath(originalLevelPath);
    }
  }

  /*- CleanupParamをGrobalに戻す -*/
  restoreGlobalSettings(params);

  /*- CleanupSettingsからCleanupを行った場合はシーンの保存は行わない -*/
  if (sourceFileIsCleanupSetting) {
    std::cout << "Cleanup from Setting file completed" << std::endl;
    return 0;
  }

  wstring wInfo  = L"updating scene " + scene->getSceneName();
  QString qInfo_ = QString::fromStdWString(wInfo);
  string info    = qInfo_.toStdString();
  m_userLog.info(info);
  cout << "updating scene " << scene->getSceneName() << endl;
  /*- シーンを上書き保存 -*/
  try {
    scene->save(fp);
    TImageCache::instance()->clear(true);
  } catch (...) {
    wstring wErr = L"Can't save scene  " + fp.getWideName();
    QString qErr = QString::fromStdWString(wErr);
    string err   = qErr.toStdString();
    m_userLog.error(err);
    cerr << "Can't save scene" << endl;
    TImageCache::instance()->clear(true);
  }

  DVGui::info("Cleanup Done.");
  return 0;
}
//------------------------------------------------------------------------

namespace {
const char *toonzVersion = "Toonz 7.1";
}  // namespace

static string getToonzVersion() { return toonzVersion; }
