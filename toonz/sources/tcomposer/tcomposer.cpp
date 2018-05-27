

#define _CRT_SECURE_NO_WARNINGS (1)   // Warning treated as errors
#define _CRT_SECURE_NO_DEPRECATE (1)  // use of fopen deprecated

// Tnz includes (-.-')
#include "../toonz/tapp.h"

// TFarmController includes
#include "tfarmcontroller.h"

// TnzStdfx includes
#include "stdfx/shaderfx.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/tlog.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/stage.h"
#include "toonz/preferences.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/tcamera.h"
#include "toonz/scenefx.h"
#include "toonz/movierenderer.h"
#include "toonz/multimediarenderer.h"
#include "toutputproperties.h"
#include "toonz/imagestyles.h"

// TnzSound includes
#include "tnzsound.h"

// TnzImage includes
#include "timage_io.h"
#include "tnzimage.h"
#include "tflash.h"

#ifdef _WIN32
#include "avicodecrestrictions.h"
#endif

// TnzBase includes
#include "tcli.h"
#include "tunit.h"
#include "tenv.h"
#include "tpassivecachemanager.h"
//#include "tcacheresourcepool.h"

// TnzCore includes
#include "tsystem.h"
#include "tsmartpointer.h"
#include "tthread.h"
#include "tthreadmessage.h"
#include "tmsgcore.h"
#include "tstopwatch.h"
#include "timagecache.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tpluginmanager.h"
#include "tiio_std.h"
#include "tsimplecolorstyles.h"

#include "tvectorbrushstyle.h"
#include "tpalette.h"

// TnzQt includes
#include "toonzqt/pluginloader.h"

// Qt includes
#include <QApplication>
#include <QWaitCondition>
#include <QMessageBox>

//==================================================================================

using namespace std;

using namespace TCli;
typedef ArgumentT<TFilePath> FilePathArgument;
typedef QualifierT<TFilePath> FilePathQualifier;

namespace {
double currentCameraSize = 12;
double getCurrentCameraSize() { return currentCameraSize; }
}  // namespace

//========================================================================
//
// Application names and versions
//
//------------------------------------------------------------------------

namespace {

//
// applicationName & Version compongono la registry root
// applicationFullName e' la title bar (e compare dentro i .tnz)
// rootVarName e' quello che dice di essere
// systemVarPrefix viene prefissa a tutte le variabili nei registry
//   (es <systemVarPrefix>PROJECTS etc.)
//

const char *applicationName     = "OpenToonz";
const char *applicationVersion  = "1.2";
const char *applicationFullName = "OpenToonz 1.2";
const char *rootVarName         = "TOONZROOT";
const char *systemVarPrefix     = "TOONZ";

// TODO: forse anche questo andrebbe in tnzbase
// ci possono essere altri programmi offline oltre al tcomposer

//========================================================================
//
// fatalError
//
//------------------------------------------------------------------------

void fatalError(string msg) {
#ifdef _WIN32
  std::cout << msg << std::endl;
  // MessageBox(0,msg.c_str(),"Fatal error",MB_ICONERROR);
  exit(1);
#else
  // TODO: Come si fa ad aggiungere un messaggio di errore qui?
  std::cout << msg << std::endl;
  abort();
#endif
}

//
#ifdef MACOSX

inline bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\n'; }

//
// TODO: se questo sistema e' il modo Mac di emulare i registry di windows
// allora **DEVE** essere messo in libreria. Parliamone.
//
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

}  // namespace

//==============================================================================================

namespace {
TStopWatch Sw1;
TStopWatch Sw2;

bool UseRenderFarm = false;
QString FarmControllerName;
int FarmControllerPort;

TFarmController *FarmController = 0;
TUserLogAppend *m_userLog;
QString TaskId;

//-------------------------------------------------------------------------------

void tcomposerRunOutOfContMemHandler(unsigned long size) {
  string msg("Run out of contiguous memory: tried to allocate " +
             std::to_string(size >> 10) + " KB");
  cout << msg << endl;
  m_userLog->error(msg);

  // The task will necessarily do something wrong. So we quit with an error
  // code.
  TImageCache::instance()->clear(true);
  exit(2);
}
}  // namespace

//==============================================================================================

class MyMovieRenderListener final : public MovieRenderer::Listener {
public:
  MyMovieRenderListener(const TFilePath &fp, int frameCount,
                        QWaitCondition &renderCompleted, bool stereo)
      : m_fp(fp)
      , m_frameCount(frameCount)
      , m_frameCompletedCount(0)
      , m_frameFailedCount(0)
      , m_renderCompleted(renderCompleted)
      , m_stereo(stereo) {}

  bool onFrameCompleted(int frame) override;
  bool onFrameFailed(int frame, TException &e) override;
  void onSequenceCompleted(const TFilePath &fp) override;

  TFilePath m_fp;
  int m_frameCount;
  int m_frameCompletedCount;
  int m_frameFailedCount;
  QWaitCondition &m_renderCompleted;
  bool m_stereo;
};

//==================================================================================

bool MyMovieRenderListener::onFrameCompleted(int frame) {
  TFilePath fp = m_fp.withFrame(frame + 1);
  string msg;
  if (m_stereo)
    msg = ::to_string(fp.withName(fp.getName() + "_l")) + " and " +
          ::to_string(fp.withName(fp.getName() + "_r")) + " computed";
  else
    msg = ::to_string(fp) + " computed";
  cout << msg << endl;
  m_userLog->info(msg);
  DVGui::info(QString::fromStdString(msg));
  if (FarmController) {
    try {
      FarmController->taskProgress(TaskId,
                                   m_frameCompletedCount + m_frameFailedCount,
                                   m_frameCount, frame + 1, FrameDone);
    } catch (...) {
      msg = "Unable to connect to " + std::to_string(FarmControllerPort) + "@" +
            FarmControllerName.toStdString();
      cout << msg;
      m_userLog->error(msg);
    }
  }

  m_frameCompletedCount++;

  return true;
}

//------------------------------------------------------------------------------

bool MyMovieRenderListener::onFrameFailed(int frame, TException &e) {
  TFilePath fp = m_fp.withFrame(frame + 1);
  string msg;
  msg = ::to_string(fp) + " failed";

  if (!e.getMessage().empty()) msg += ": " + ::to_string(e.getMessage());

  cout << msg << endl;
  m_userLog->error(msg);
  if (FarmController) {
    try {
      FarmController->taskProgress(TaskId,
                                   m_frameCompletedCount + m_frameFailedCount,
                                   m_frameCount, frame + 1, FrameFailed);
    } catch (...) {
      msg = "Unable to connect to " + std::to_string(FarmControllerPort) + "@" +
            FarmControllerName.toStdString();
      cout << msg;
      m_userLog->error(msg);
    }
  }

  m_frameFailedCount++;

  return true;
}

//------------------------------------------------------------------------------

void MyMovieRenderListener::onSequenceCompleted(const TFilePath &fp) {
  cout << endl;
  QCoreApplication::instance()->quit();
}

//==============================================================================================

class MyMultimediaRenderListener final : public MultimediaRenderer::Listener {
public:
  TFilePath m_fp;
  int m_frameCount;
  int m_frameCompletedCount;
  int m_frameFailedCount;

  MyMultimediaRenderListener(const TFilePath &fp, int frameCount)
      : m_fp(fp)
      , m_frameCount(frameCount)
      , m_frameCompletedCount(0)
      , m_frameFailedCount(0) {}

  bool onFrameCompleted(int frame, int column) override;
  bool onFrameFailed(int frame, int column, TException &e) override;
  void onSequenceCompleted(int column) override {}
  void onRenderCompleted() override {}
};

//==================================================================================

bool MyMultimediaRenderListener::onFrameCompleted(int frame, int column) {
  int actualFrame = frame + 1;
  string msg      = ::to_string(m_fp) + ", column " + std::to_string(column) +
               ", frame " + std::to_string(actualFrame) + " computed";
  cout << msg << endl;
  m_userLog->info(msg);
  if (FarmController) {
    try {
      FarmController->taskProgress(TaskId,
                                   m_frameCompletedCount + m_frameFailedCount,
                                   m_frameCount, actualFrame, FrameDone);
    } catch (...) {
      msg = "Unable to connect to " + std::to_string(FarmControllerPort) + "@" +
            FarmControllerName.toStdString();
      cout << msg;
      m_userLog->error(msg);
    }
  }

  m_frameCompletedCount++;
  return true;
}

//------------------------------------------------------------------------------

bool MyMultimediaRenderListener::onFrameFailed(int frame, int column,
                                               TException &e) {
  int actualFrame = frame + 1;
  string msg      = ::to_string(m_fp) + ", column " + std::to_string(column) +
               ", frame " + std::to_string(actualFrame) + " failed";

  if (!e.getMessage().empty()) msg += ": " + ::to_string(e.getMessage());

  cout << msg << endl;
  m_userLog->error(msg);
  if (FarmController) {
    try {
      FarmController->taskProgress(TaskId,
                                   m_frameCompletedCount + m_frameFailedCount,
                                   m_frameCount, actualFrame, FrameFailed);
    } catch (...) {
      msg = "Unable to connect to " + std::to_string(FarmControllerPort) + "@" +
            FarmControllerName.toStdString();
      cout << msg;
      m_userLog->error(msg);
    }
  }

  m_frameFailedCount++;
  return true;
}

//==================================================================================

static std::pair<int, int> generateMovie(ToonzScene *scene, const TFilePath &fp,
                                         int r0, int r1, int step, int shrink,
                                         int threadCount, int maxTileSize) {
  QWaitCondition renderCompleted;

  // riporto gli indici a base zero
  r0 = r0 - 1;
  r1 = r1 - 1;

  if (r0 < 0) r0                                 = 0;
  if (r1 < 0 || r1 >= scene->getFrameCount()) r1 = scene->getFrameCount() - 1;
  string msg;
  assert(r1 >= r0);
  TSceneProperties *sprop          = scene->getProperties();
  TOutputProperties outputSettings = *sprop->getOutputProperties();

  currentCameraSize = scene->getCurrentCamera()->getSize().lx;

  TDimension cameraSize = scene->getCurrentCamera()->getRes();
  TSystem::touchParentDir(fp);

  string ext = fp.getType();

#ifdef _WIN32
  if (ext == "avi") {
    TPropertyGroup *props =
        scene->getProperties()->getOutputProperties()->getFileFormatProperties(
            ext);
    string codecName = props->getProperty(0)->getValueAsString();
    TDimension res   = scene->getCurrentCamera()->getRes();
    if (!AviCodecRestrictions::canWriteMovie(::to_wstring(codecName), res)) {
      string msg(
          "The resolution of the output camera does not fit with the options "
          "chosen for the output file format.");
      m_userLog->error(msg);
      exit(1);
    }
  }
#endif

  double cameraXDpi, cameraYDpi;
  TPointD camDpi = scene->getCurrentCamera()->getDpi();
  cameraXDpi     = camDpi.x;
  cameraYDpi     = camDpi.y;

  int which        = outputSettings.getWhichLevels();
  double stretchTo = (double)outputSettings.getRenderSettings().m_timeStretchTo;
  double stretchFrom =
      (double)outputSettings.getRenderSettings().m_timeStretchFrom;

  double timeStretchFactor = stretchFrom / stretchTo;
  int numFrames            = (int)((r1 - r0 + 1) / timeStretchFactor);
  double r                 = (r0)*timeStretchFactor;
  double stepd             = step * timeStretchFactor;

  int multimediaRender = outputSettings.getMultimediaRendering();

  //---------------------------------------------------------
  //    Multimedia render
  //---------------------------------------------------------

  if (multimediaRender) {
    MultimediaRenderer multimediaRenderer(scene, fp, multimediaRender,
                                          threadCount);
    TRenderSettings rs = outputSettings.getRenderSettings();
    rs.m_maxTileSize   = maxTileSize;
    rs.m_shrinkX = rs.m_shrinkY = shrink;
    multimediaRenderer.setRenderSettings(rs);
    multimediaRenderer.setDpi(cameraXDpi, cameraYDpi);
    multimediaRenderer.enablePrecomputing(true);
    for (int i = 0; i < numFrames; i += step, r += stepd)
      multimediaRenderer.addFrame(r);

    MyMultimediaRenderListener *listener = new MyMultimediaRenderListener(
        fp, multimediaRenderer.getFrameCount() *
                multimediaRenderer.getColumnsCount());
    multimediaRenderer.addListener(listener);

    multimediaRenderer.start();

    //----------------- main thread remains above until render is done
    //----------------

    // NOTE: the passed total frame count "columns * frames" is an overestimate
    // in this case:
    // columns which do not appear on certain frames are not rendered. So, just
    // realize the
    // number of failed frames if any.
    std::pair<int, int> framePair =
        std::make_pair(listener->m_frameCount, listener->m_frameCount);

    delete listener;
    return framePair;
  }

  //---------------------------------------------------------
  //    Movie render
  //---------------------------------------------------------

  else {
    MovieRenderer movieRenderer(scene, fp, threadCount, false);
    TRenderSettings rs = outputSettings.getRenderSettings();
    rs.m_maxTileSize   = maxTileSize;
    rs.m_shrinkX = rs.m_shrinkY = shrink;

    movieRenderer.setRenderSettings(rs);
    movieRenderer.setDpi(cameraXDpi, cameraYDpi);

    movieRenderer.enablePrecomputing(true);

    MyMovieRenderListener *listener =
        new MyMovieRenderListener(fp, tceil((numFrames) / (float)step),
                                  renderCompleted, rs.m_stereoscopic);

    movieRenderer.addListener(listener);

    for (int i = 0; i < numFrames; i += step, r += stepd) {
      TFxPair fx;
      if (rs.m_stereoscopic) scene->shiftCameraX(-rs.m_stereoscopicShift / 2);

      fx.m_frameA = (TRasterFxP)buildSceneFx(scene, scene->getXsheet(), r,
                                             which, shrink, false);

      if (rs.m_fieldPrevalence != TRenderSettings::NoField)
        fx.m_frameB = (TRasterFxP)buildSceneFx(scene, scene->getXsheet(),
                                               r + 0.5 * timeStretchFactor,
                                               which, shrink, false);
      else if (rs.m_stereoscopic) {
        scene->shiftCameraX(rs.m_stereoscopicShift);
        fx.m_frameB = (TRasterFxP)buildSceneFx(scene, scene->getXsheet(), r,
                                               which, shrink, false);
        scene->shiftCameraX(-rs.m_stereoscopicShift / 2);
      } else
        fx.m_frameB = TRasterFxP();

      movieRenderer.addFrame(r, fx);
    }

    movieRenderer.start();

    // Start main loop
    QCoreApplication::instance()->exec();

    //----------------- tcomposer's main thread loops here ----------------

    // int frameCompleted = listener->m_frameCompletedCount;
    std::pair<int, int> framePair =
        std::make_pair(listener->m_frameCompletedCount, listener->m_frameCount);

    delete listener;
    return framePair;
  }
}

//==================================================================================
//
// main()
//
//----------------------------------------------------------------------------------

// TODO: il main comincia a diventare troppo lungo. Forse val la pena
// separarlo in varie funzioni
// (tipo initToonzEnvironment(), parseCommandLine(), ecc)
// TODO: the main starts getting too long. Perhaps it is worth
// separated into various functions
// (type initToonzEnvironment (), ParseCommandLine (), etc.)

DV_IMPORT_API void initStdFx();
DV_IMPORT_API void initColorFx();
int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // Create a QObject destroyed just before app - see Tnz6's main.cpp for
  // rationale
  std::unique_ptr<QObject> mainScope(new QObject(&app));
  mainScope->setObjectName("mainScope");

#ifdef _WIN32
#ifndef x64
  // Store the floating point control word. It will be re-set before Toonz
  // initialization
  // has ended.
  unsigned int fpWord = 0;
  _controlfp_s(&fpWord, 0, 0);
#endif
#endif

  // Set the app's locale for numeric stuff to standard C. This is important for
  // atof() and similar
  // calls that are locale-dependant.
  setlocale(LC_NUMERIC, "C");

  // Install run out of contiguous memory callback
  TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
      &tcomposerRunOutOfContMemHandler);

#ifdef _WIN32
// Define 64-bit precision for floating-point arithmetic. Please observe that
// the
// initImageIo() call below would already impose this precision. This just wants
// to be
// explicit.
//_controlfp_s(0, 0, 0x10000);
#endif

  // Initialize thread components
  TThread::init();

  // questo definisce la registry root e inizializza TEnv
  TEnv::setApplication(applicationName, applicationVersion);
  TEnv::setApplicationFullName(applicationFullName);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);
  TSystem::hasMainLoop(true);

  // QMessageBox::information(0, QString("eccolo"), QString("composer!"));

  int i;
  for (i = 0; i < argc; i++)  // tmsg must be set as soon as it's possible
  {
    QString str = argv[i];
    if (str == "-tmsg") {
      TMsgCore::instance()->connectTo(argv[i + 1]);
      break;
    }
  }
  if (i == argc) TMsgCore::instance()->connectTo("");

  // controllo se la xxxroot e' definita e corrisponde ad un file esistente
  TFilePath fp = TEnv::getStuffDir();
  if (fp == TFilePath())
    fatalError(string("Undefined: \"") + ::to_string(TEnv::getRootVarPath()) +
               "\"");
  if (!TFileStatus(fp).isDirectory())
    fatalError(string("Directory \"") + ::to_string(fp) +
               "\" not found or not readable");

  TFilePath lRootDir    = fp + "toonzfarm";
  TFilePath logFilePath = lRootDir + "tcomposer.log";
  m_userLog             = new TUserLogAppend(logFilePath);
  string msg;

  // Initialize measure units
  Preferences::instance();                      // Loads standard (linear) units
  TMeasureManager::instance()->                 // Loads camera-related units
      addCameraMeasures(getCurrentCameraSize);  //

  TFilePathSet fps = ToonzFolder::getProjectsFolders();
  TFilePathSet::iterator fpIt;
  for (fpIt = fps.begin(); fpIt != fps.end(); ++fpIt)
    TProjectManager::instance()->addProjectsRoot(*fpIt);

  TFilePath libraryFolder = ToonzFolder::getLibraryFolder();
  TRasterImagePatternStrokeStyle::setRootDir(libraryFolder);
  TVectorImagePatternStrokeStyle::setRootDir(libraryFolder);
  TVectorBrushStyle::setRootDir(libraryFolder);
  TPalette::setRootDir(libraryFolder);
  TImageStyle::setLibraryDir(libraryFolder);
  TFilePath cacheRoot                = ToonzFolder::getCacheRootFolder();
  if (cacheRoot.isEmpty()) cacheRoot = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheRoot);
  // #endif

  //  setCurrentModule("tcomposer");
  TCli::FilePathArgument srcName("srcName", "Source file");
  FilePathQualifier dstName("-o dstName", "Target file");
  RangeQualifier range;
  IntQualifier stepOpt("-step n", "Step");
  IntQualifier shrinkOpt("-shrink n", "Shrink");
  IntQualifier multimedia("-multimedia n", "Multimedia rendering mode");
  StringQualifier farmData("-farm data", "TFarm Controller");
  StringQualifier idq("-id n", "id");
  StringQualifier nthreads("-nthreads n", "Number of rendering threads");
  StringQualifier tileSize("-maxtilesize n",
                           "Enable tile rendering of max n MB per tile");
  StringQualifier tmsg("-tmsg val", "only internal use");

  Usage usage(argv[0]);
  usage.add(srcName + dstName + range + stepOpt + shrinkOpt + multimedia +
            farmData + idq + nthreads + tileSize + tmsg);
  if (!usage.parse(argc, argv)) exit(1);

  TaskId       = QString::fromStdString(idq.getValue());
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
      FarmControllerName = QString::fromStdString(fdata.substr(pos + 1));
    }
  }

  if (UseRenderFarm) {
    TFarmControllerFactory factory;
    factory.create(FarmControllerName, FarmControllerPort, &FarmController);
  }

  while (!PluginLoader::load_entries("")) app.processEvents();

  std::pair<int, int> framePair(1, 0);

  try {
    Tiio::defineStd();

    initImageIo();
    Tiio::defineStd();
    initSoundIo();
    initStdFx();
    initColorFx();

    loadShaderInterfaces(ToonzFolder::getLibraryFolder() +
                         TFilePath("shaders"));

    //#endif

    //---------------------------------------------------------

    TFilePath srcFilePath = srcName.getValue();

    try {
      srcFilePath = TSystem::toLocalPath(srcFilePath);
    } catch (...) {
    }

    //---------------------------------------------------------
    msg = "Loading " + srcFilePath.getName();
    cout << endl << msg << endl;
    m_userLog->info(msg);
    TProjectManager *pm = TProjectManager::instance();
    // pm->enableTabMode(true);

    TProjectP project = pm->loadSceneProject(srcFilePath);
    if (!project) {
      msg = "Couldn't find the project";  //+ project->getName().getName();
      cerr << msg << endl;
      m_userLog->error(msg);
      return -2;
    }
    msg = "project: " + project->getName().getName();
    cout << msg << endl;
    m_userLog->info(msg);
    // pm->setCurrentProject(project, false); // false => temporaneamente

    Sw1.start();

    if (!TSystem::doesExistFileOrLevel(srcFilePath)) return false;
    ToonzScene *scene = new ToonzScene();

    TImageStyle::setCurrentScene(scene);

    try {
      Sw2.start();
      scene->load(srcFilePath);
      Sw2.stop();
    } catch (TException &e) {
      cout << ::to_string(e.getMessage()) << endl;
      m_userLog->error(::to_string(e.getMessage()));
      return -2;
    } catch (...) {
      string msg;
      msg = "There were problems loading the scene " +
            ::to_string(srcFilePath) + ".\n Some files may be missing.";
      cout << msg << endl;
      m_userLog->error(msg);
      // return false;
    }

    msg = "scene loaded";
    cout << "scene loaded" << endl;
    m_userLog->info(msg);

    //---------------------------------------------------------

    TFilePath dstFilePath;
    if (dstName.isSelected())
      dstFilePath = dstName.getValue();
    else {
      dstFilePath = scene->getProperties()->getOutputProperties()->getPath();
      if (dstFilePath == TFilePath())
        dstFilePath = TFilePath("+outputs") + "$scenename.tif";
      else if (dstFilePath.getName() == "")
        dstFilePath = (dstFilePath.getParentDir() + scene->getSceneName())
                          .withType(dstFilePath.getType());
    }

    dstFilePath = scene->decodeFilePath(dstFilePath);

    //---------------------------------------------------------
    msg = "Generating " + dstFilePath.getName();
    cout << endl << "Generating " << dstFilePath << endl << endl;
    m_userLog->info(msg);

    TFilePath theDstFilePath = dstFilePath;
    try {
      theDstFilePath = TSystem::toLocalPath(dstFilePath);
    } catch (...) {
    }

    int r0 = -1, r1 = -1, step = 1, shrink = 1;
    TOutputProperties *outProp = scene->getProperties()->getOutputProperties();
    int scene_from, scene_to, scene_step;

    outProp->getRange(scene_from, scene_to, scene_step);
    int scene_shrink = outProp->getRenderSettings().m_shrinkX;

    if (scene_from == 0 && scene_to == -1) {
      scene_from = 1;
      scene_to   = scene->getFrameCount();
    } else {
      scene_from++;
      scene_to++;
    }
    if (range.isSelected()) {
      r0 = range.getFrom();
      r1 = range.getTo();
    } else {
      r0 = scene_from;
      r1 = scene_to;
    }

    if (stepOpt.isSelected())
      step = stepOpt.getValue();
    else
      step = scene_step;
    if (shrinkOpt.isSelected())
      shrink = shrinkOpt.getValue();
    else
      shrink = scene_shrink;
    if (multimedia.isSelected())
      scene->getProperties()->getOutputProperties()->setMultimediaRendering(
          multimedia.getValue());

    // Retrieve Thread count
    const int procCount = TSystem::getProcessorCount();
    int threadCount;
    const int threadCounts[3] = {1, procCount / 2, procCount};
    if (nthreads.isSelected()) {
      QString threadCountStr = QString::fromStdString(nthreads.getValue());
      threadCount            = (threadCountStr == "single")
                        ? threadCounts[0]
                        : (threadCountStr == "half")
                              ? threadCounts[1]
                              : (threadCountStr == "all")
                                    ? threadCounts[2]
                                    : threadCountStr.toInt();

      if (threadCount <= 0) {
        cout << "Qualifier 'nthreads': bad input" << endl;
        exit(1);
      }
    } else {
      int threadIndex = outProp->getThreadIndex();
      threadCount     = threadCounts[threadIndex];
    }

    threadCount = tcrop(1, procCount, threadCount);

    // Retrieve max tile size (raster granularity)
    int maxTileSize;
    const int maxTileSizes[4] = {
        (std::numeric_limits<int>::max)(), TOutputProperties::LargeVal,
        TOutputProperties::MediumVal, TOutputProperties::SmallVal};
    if (tileSize.isSelected()) {
      QString tileSizeStr = QString::fromStdString(tileSize.getValue());
      maxTileSize         = (tileSizeStr == "none")
                        ? maxTileSizes[0]
                        : (tileSizeStr == "large")
                              ? maxTileSizes[1]
                              : (tileSizeStr == "medium")
                                    ? maxTileSizes[2]
                                    : (tileSizeStr == "small")
                                          ? maxTileSizes[3]
                                          : tileSizeStr.toInt();

      if (maxTileSize <= 0) {
        cout << "Qualifier 'maxtilesize': bad input" << endl;
        exit(1);
      }
    } else {
      int maxTileSizeIndex = outProp->getMaxTileSizeIndex();
      maxTileSize          = maxTileSizes[maxTileSizeIndex];
    }

    m_userLog->info("Threads count: " + std::to_string(threadCount));
    if (maxTileSize != (std::numeric_limits<int>::max)())
      m_userLog->info("Render tile: " + std::to_string(maxTileSize));

    // Disable the Passive cache manager. It has no sense if it cannot write on
    // disk...
    // TCacheResourcePool::instance();   //Needs to be instanced before
    // TPassiveCacheManager...
    TPassiveCacheManager::instance()->setEnabled(false);

#ifdef _WIN32
#ifndef x64
    // On 32-bit architecture, there could be cases in which initialization
    // could alter the
    // FPU floating point control word. I've seen this happen when loading some
    // AVI coded (VFAPI),
    // where 80-bit internal precision was used instead of the standard 64-bit
    // (much faster and
    // sufficient - especially considering that x86 truncates to 64-bit
    // representation anyway).
    // IN ANY CASE, revert to the original control word.
    // In the x64 case these precision changes simply should not take place up
    // to _controlfp_s
    // documentation.
    _controlfp_s(0, fpWord, -1);
#endif
#endif

    framePair = generateMovie(scene, theDstFilePath, r0, r1, step, shrink,
                              threadCount, maxTileSize);

    Sw1.stop();

    m_userLog->info(
        "Raster Allocation Peak: " +
        std::to_string(TBigMemoryManager::instance()->getAllocationPeak()) +
        " KB");
    m_userLog->info(
        "Raster Allocation Mean: " +
        std::to_string(TBigMemoryManager::instance()->getAllocationMean()) +
        " KB");

    msg = "Compositing completed in " +
          ::to_string(Sw1.getTotalTime() / 1000.0, 2) + " seconds";
    string msg2 =
        "\n" + ::to_string(Sw2.getTotalTime() / 1000.0, 2) +
        " seconds spent on loading" + "\n" +
        ::to_string(TStopWatch::global(0).getTotalTime() / 1000.0, 2) +
        " seconds spent on saving" + "\n" +
        ::to_string(TStopWatch::global(8).getTotalTime() / 1000.0, 2) +
        " seconds spent on rendering" + "\n";
    cout << msg + msg2;
    m_userLog->info(msg + msg2);
    DVGui::info(QString::fromStdString(msg));
    TImageCache::instance()->clear(true);
  } catch (TException &e) {
    msg = "Untrapped exception: " + ::to_string(e.getMessage()), cout << msg
                                                                      << endl;
    m_userLog->error(msg);
    TImageCache::instance()->clear(true);
  } catch (...) {
    cout << "Untrapped exception" << endl;
    m_userLog->error("Untrapped exception");
    TImageCache::instance()->clear(true);
  }

  if (framePair.first != framePair.second) return -1;
  return 0;
}
