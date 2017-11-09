

#include "toonz/preferences.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/toonzfolders.h"
#include "toonz/tcamera.h"
#include "toonz/txshleveltypes.h"

// TnzBase includes
#include "tenv.h"
#include "tunit.h"

// TnzCore includes
#include "tsystem.h"
#include "tconvert.h"
#include "tundo.h"
#include "tbigmemorymanager.h"
#include "tfilepath.h"

// Qt includes
#include <QSettings>
#include <QStringList>
#include <QAction>

// boost includes
#include <boost/bind.hpp>

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

typedef Preferences::LevelFormat LevelFormat;
typedef std::vector<LevelFormat> LevelFormatVector;

//-----------------------------------------------------------------

const char *s_bool[2] = {"0", "1"};

const char *s_show0ThickLines = "show0ThickLines",
           *s_regionAntialias = "regionAntialias",
           *s_levelFormats    = "levelFormats";

const char *s_name = "name", *s_regexp = "regexp", *s_priority = "priority";

const char *s_dpiPolicy = "dpiPolicy", *s_dpi = "dpi",
           *s_subsampling = "subsampling", *s_antialias = "antialias",
           *s_premultiply = "premultiply", *s_whiteTransp = "whiteTransp";

//=================================================================

inline void getValue(const QSettings &s, const QString &key, bool &ret) {
  QString val;
  val                = s.value(key).toString();
  if (val != "") ret = (val.toInt() != 0);
}

//-----------------------------------------------------------------

inline void getValue(const QSettings &s, const QString &key, int &ret) {
  QString val;
  val                = s.value(key).toString();
  if (val != "") ret = val.toInt();
}

//-----------------------------------------------------------------

inline void getValue(const QSettings &s, const QString &key, double &ret) {
  QString val;
  val                = s.value(key).toString();
  if (val != "") ret = val.toDouble();
}

//-----------------------------------------------------------------

inline void getValue(const QSettings &s, QString key, TPixel32 &ret) {
  QString val;
  val                  = s.value(QString(key).append("_R")).toString();
  if (val != "") ret.r = val.toInt();
  val                  = s.value(QString(key).append("_G")).toString();
  if (val != "") ret.g = val.toInt();
  val                  = s.value(QString(key).append("_B")).toString();
  if (val != "") ret.b = val.toInt();
  val                  = s.value(QString(key).append("_M")).toString();
  if (val != "") ret.m = val.toInt();
}

//-----------------------------------------------------------------

inline bool formatLess(const Preferences::LevelFormat &a,
                       const Preferences::LevelFormat &b) {
  return (
      a.m_priority > b.m_priority  // Observe '>' used here - we want inverse
      || (!(b.m_priority >
            a.m_priority)  // sorting on priority, higher priorities come first
          && a.m_name < b.m_name));
}

//=================================================================

void getDefaultLevelFormats(LevelFormatVector &lfv) {
  lfv.resize(3);
  {
    LevelFormat &lf = lfv[0];

    lf.m_name       = Preferences::tr("Retas Level Format");
    lf.m_pathFormat = QRegExp(".+[0-9]{4,4}\\.tga", Qt::CaseInsensitive);
    lf.m_options.m_whiteTransp = true;
    lf.m_options.m_antialias   = 70;

    // for all PSD files, set the premultiply options to layers
    lfv[1].m_name                  = Preferences::tr("Adobe Photoshop");
    lfv[1].m_pathFormat            = QRegExp("..*\\.psd", Qt::CaseInsensitive);
    lfv[1].m_options.m_premultiply = true;

    // for all PNG files, set premultiply by default
    lfv[2].m_name                  = Preferences::tr("PNG");
    lfv[2].m_pathFormat            = QRegExp("..*\\.png", Qt::CaseInsensitive);
    lfv[2].m_options.m_premultiply = true;
  }
}

//=================================================================

void setValue(QSettings &settings, const LevelOptions &lo) {
  settings.setValue(s_dpiPolicy, int(lo.m_dpiPolicy));
  settings.setValue(s_dpi, lo.m_dpi);
  settings.setValue(s_subsampling, lo.m_subsampling);
  settings.setValue(s_antialias, lo.m_antialias);
  settings.setValue(s_premultiply, int(lo.m_premultiply));
  settings.setValue(s_whiteTransp, int(lo.m_whiteTransp));
}

//-----------------------------------------------------------------

void getValue(const QSettings &settings, LevelOptions &lo) {
  int dpiPolicy    = settings.value(s_dpiPolicy, int(lo.m_dpiPolicy)).toInt();
  lo.m_dpiPolicy   = LevelOptions::DpiPolicy(dpiPolicy);
  lo.m_dpi         = settings.value(s_dpi, lo.m_dpi).toDouble();
  lo.m_subsampling = settings.value(s_subsampling, lo.m_subsampling).toInt();
  lo.m_antialias   = settings.value(s_antialias, lo.m_antialias).toInt();
  lo.m_premultiply =
      (settings.value(s_premultiply, lo.m_premultiply).toInt() != 0);
  lo.m_whiteTransp =
      (settings.value(s_whiteTransp, lo.m_whiteTransp).toInt() != 0);
}

//-----------------------------------------------------------------

void setValue(QSettings &settings, const LevelFormat &lf) {
  settings.setValue(s_name, lf.m_name);
  settings.setValue(s_regexp, lf.m_pathFormat.pattern());
  settings.setValue(s_priority, lf.m_priority);
  setValue(settings, lf.m_options);
}

//-----------------------------------------------------------------

void getValue(const QSettings &settings, LevelFormat &lf) {
  lf.m_name = settings.value(s_name, lf.m_name).toString();
  lf.m_pathFormat =
      QRegExp(settings.value(s_regexp, lf.m_pathFormat).toString(),
              Qt::CaseInsensitive);
  lf.m_priority = settings.value(s_priority, lf.m_priority).toInt();
  getValue(settings, lf.m_options);
}

//-----------------------------------------------------------------

void setValue(QSettings &settings, const LevelFormatVector &lfv) {
  int lf, lfCount = int(lfv.size());

  settings.remove(s_levelFormats);

  settings.beginWriteArray(s_levelFormats, lfCount);
  {
    for (lf = 0; lf != lfCount; ++lf) {
      settings.setArrayIndex(lf);
      setValue(settings, lfv[lf]);
    }
  }
  settings.endArray();
}

//-----------------------------------------------------------------

void getValue(QSettings &settings,
              LevelFormatVector &lfv)  // Why does QSettings' interface require
{  // non-const access on reading arrays/groups?
  if (!settings.childGroups().contains(s_levelFormats))
    return;  // Default is no level formats - use builtins

  int lfCount = settings.beginReadArray(s_levelFormats);  // lfCount could be 0
  lfv.resize(lfCount);

  for (int lf = 0; lf != lfCount; ++lf) {
    settings.setArrayIndex(lf);
    getValue(settings, lfv[lf]);
  }
  settings.endArray();
}

}  // namespace

//**********************************************************************************
//    Preferences::LevelFormat  implementation
//**********************************************************************************

bool Preferences::LevelFormat::matches(const TFilePath &fp) const {
  return m_pathFormat.exactMatch(fp.getQString());
}

//**********************************************************************************
//    Preferences  implementation
//**********************************************************************************

Preferences::Preferences()
    : m_pixelsOnly(false)
    , m_units("mm")
    , m_cameraUnits("inch")
    , m_currentRoomChoice("Default")
    , m_scanLevelType("tif")
#ifdef _WIN32
    , m_interfaceFont("Segoe UI")
#else
    , m_interfaceFont("Helvetica")
#endif
    , m_interfaceFontWeight(0)
    , m_defLevelWidth(0.0)
    , m_defLevelHeight(0.0)
    , m_defLevelDpi(0.0)
    , m_iconSize(160, 90)
    , m_blankColor(TPixel32::White)
    , m_frontOnionColor(TPixel::Black)
    , m_backOnionColor(TPixel::Black)
    , m_transpCheckBg(TPixel::White)
    , m_transpCheckInk(TPixel::Black)
    , m_transpCheckPaint(TPixel(127, 127, 127))
    , m_autosavePeriod(15)
    , m_chunkSize(10)
    , m_rasterOptimizedMemory(0)
    , m_shrink(1)
    , m_step(1)
    , m_blanksCount(0)
    , m_keyframeType(3)
    , m_animationStep(1)
    , m_textureSize(0)
    , m_xsheetStep(10)
    , m_shmmax(-1)
    , m_shmseg(-1)
    , m_shmall(-1)
    , m_shmmni(-1)
    , m_onionPaperThickness(50)
    , m_currentLanguage("English")
    , m_currentStyleSheet("Default")
    , m_undoMemorySize(100)
    , m_dragCellsBehaviour(0)
    , m_lineTestFpsCapture(25)
    , m_defLevelType(0)
    , m_vectorSnappingTarget(SnapAll)
    , m_autocreationType(1)
    , m_autoExposeEnabled(true)
    , m_autoCreateEnabled(true)
    , m_subsceneFolderEnabled(true)
    , m_generatedMovieViewEnabled(true)
    , m_xsheetAutopanEnabled(true)
    , m_ignoreAlphaonColumn1Enabled(false)
    , m_rewindAfterPlaybackEnabled(true)
    , m_fitToFlipbookEnabled(false)
    , m_previewAlwaysOpenNewFlipEnabled(false)
    , m_autosaveEnabled(false)
    , m_autosaveSceneEnabled(true)
    , m_autosaveOtherFilesEnabled(true)
    , m_startupPopupEnabled(true)
    , m_defaultViewerEnabled(false)
    , m_saveUnpaintedInCleanup(true)
    , m_askForOverrideRender(true)
    , m_automaticSVNFolderRefreshEnabled(true)
    , m_SVNEnabled(false)
    , m_minimizeSaveboxAfterEditing(true)
    , m_levelsBackupEnabled(false)
    , m_sceneNumberingEnabled(false)
    , m_animationSheetEnabled(false)
    , m_inksOnly(false)
    , m_fillOnlySavebox(false)
    , m_show0ThickLines(true)
    , m_regionAntialias(false)
    , m_viewerBGColor(128, 128, 128, 255)
    , m_previewBGColor(64, 64, 64, 255)
    , m_chessboardColor1(180, 180, 180)
    , m_chessboardColor2(230, 230, 230)
    , m_showRasterImagesDarkenBlendedInViewer(false)
    , m_actualPixelViewOnSceneEditingMode(false)
    , m_viewerZoomCenter(0)
    , m_initialLoadTlvCachingBehavior(0)
    , m_removeSceneNumberFromLoadedLevelName(false)
    , m_replaceAfterSaveLevelAs(true)
    , m_showFrameNumberWithLetters(false)
    , m_levelNameOnEachMarker(false)
    , m_columnIconLoadingPolicy((int)LoadAtOnce)
    , m_moveCurrentFrameByClickCellArea(true)
    , m_onionSkinEnabled(true)
    , m_onionSkinDuringPlayback(false)
    , m_dropdownShortcutsCycleOptions(false)
    , m_multiLayerStylePickerEnabled(false)
    , m_paletteTypeOnLoadRasterImageAsColorModel(0)
    , m_showKeyframesOnXsheetCellArea(true)
    , m_projectRoot(0x08)
    , m_customProjectRoot("")
    , m_precompute(true)
    , m_fastRenderPath("desktop")
    , m_ffmpegTimeout(60)
    , m_shortcutPreset("defopentoonz")
    , m_useNumpadForSwitchingStyles(true)
    , m_newLevelSizeToCameraSizeEnabled(false)
    , m_showXSheetToolbar(false)
    , m_expandFunctionHeader(false)
    , m_showColumnNumbers(false)
    , m_useArrowKeyToShiftCellSelection(false)
    , m_inputCellsWithoutDoubleClickingEnabled(false)
    , m_importPolicy(0)
    , m_guidedDrawingType(0)
    , m_animatedGuidedDrawing(false)
    , m_ignoreImageDpi(false)
    , m_watchFileSystem(true)
    , m_shortcutCommandsWhileRenamingCellEnabled(false)
    , m_xsheetLayoutPreference("Classic-revised")
    , m_loadedXsheetLayout("Classic-revised") {
  TCamera camera;
  m_defLevelType   = PLI_XSHLEVEL;
  m_defLevelWidth  = camera.getSize().lx;
  m_defLevelHeight = camera.getSize().ly;
  m_defLevelDpi    = camera.getDpi().x;

  TFilePath layoutDir = ToonzFolder::getMyModuleDir();
  TFilePath prefPath  = layoutDir + TFilePath("preferences.ini");

  bool existingUser = true;

  // In case the personal settings is not exist (for new users)
  if (!TFileStatus(prefPath).doesExist()) {
    TFilePath templatePath =
        ToonzFolder::getTemplateModuleDir() + TFilePath("preferences.ini");
    // If there is the template, copy it to the personal one
    if (TFileStatus(templatePath).doesExist())
      TSystem::copyFile(prefPath, templatePath);

    existingUser = false;
  }

  m_settings.reset(new QSettings(
      QString::fromStdWString(prefPath.getWideString()), QSettings::IniFormat));

  getValue(*m_settings, "autoExposeEnabled", m_autoExposeEnabled);
  getValue(*m_settings, "autoCreateEnabled", m_autoCreateEnabled);
  getValue(*m_settings, "subsceneFolderEnabled", m_subsceneFolderEnabled);
  getValue(*m_settings, "generatedMovieViewEnabled",
           m_generatedMovieViewEnabled);
  getValue(*m_settings, "xsheetAutopanEnabled", m_xsheetAutopanEnabled);
  getValue(*m_settings, "ignoreAlphaonColumn1Enabled",
           m_ignoreAlphaonColumn1Enabled);
  getValue(*m_settings, "rewindAfterPlayback", m_rewindAfterPlaybackEnabled);
  getValue(*m_settings, "previewAlwaysOpenNewFlip",
           m_previewAlwaysOpenNewFlipEnabled);
  getValue(*m_settings, "fitToFlipbook", m_fitToFlipbookEnabled);
  getValue(*m_settings, "automaticSVNFolderRefreshEnabled",
           m_automaticSVNFolderRefreshEnabled);
  getValue(*m_settings, "SVNEnabled", m_SVNEnabled);
  getValue(*m_settings, "minimizeSaveboxAfterEditing",
           m_minimizeSaveboxAfterEditing);
  getValue(*m_settings, "levelsBackupEnabled", m_levelsBackupEnabled);
  getValue(*m_settings, "sceneNumberingEnabled", m_sceneNumberingEnabled);
  getValue(*m_settings, "animationSheetEnabled", m_animationSheetEnabled);
  getValue(*m_settings, "autosaveEnabled", m_autosaveEnabled);
  getValue(*m_settings, "autosaveSceneEnabled", m_autosaveSceneEnabled);
  getValue(*m_settings, "autosaveOtherFilesEnabled",
           m_autosaveOtherFilesEnabled);
  getValue(*m_settings, "startupPopupEnabled", m_startupPopupEnabled);
  getValue(*m_settings, "dropdownShortcutsCycleOptions",
           m_dropdownShortcutsCycleOptions);
  getValue(*m_settings, "defaultViewerEnabled", m_defaultViewerEnabled);
  getValue(*m_settings, "rasterOptimizedMemory", m_rasterOptimizedMemory);
  getValue(*m_settings, "saveUnpaintedInCleanup", m_saveUnpaintedInCleanup);
  getValue(*m_settings, "autosavePeriod", m_autosavePeriod);
  getValue(*m_settings, "taskchunksize", m_chunkSize);
  getValue(*m_settings, "xsheetStep", m_xsheetStep);
  getValue(*m_settings, "vectorSnappingTarget", m_vectorSnappingTarget);
  int r = 0, g = 255, b = 0;
  getValue(*m_settings, "frontOnionColor.r", r);
  getValue(*m_settings, "frontOnionColor.g", g);
  getValue(*m_settings, "frontOnionColor.b", b);
  m_frontOnionColor = TPixel32(r, g, b);

  getValue(*m_settings, "onionPaperThickness", m_onionPaperThickness);

  r = 255, g = 0, b = 0;
  getValue(*m_settings, "backOnionColor.r", r);
  getValue(*m_settings, "backOnionColor.g", g);
  getValue(*m_settings, "backOnionColor.b", b);
  m_backOnionColor = TPixel32(r, g, b);

  r = m_transpCheckBg.r, g = m_transpCheckBg.g, b = m_transpCheckBg.b;
  getValue(*m_settings, "transpCheckInkOnBlack.r", r);
  getValue(*m_settings, "transpCheckInkOnBlack.g", g);
  getValue(*m_settings, "transpCheckInkOnBlack.b", b);
  m_transpCheckBg = TPixel32(r, g, b);

  r = m_transpCheckInk.r, g = m_transpCheckInk.g, b = m_transpCheckInk.b;
  getValue(*m_settings, "transpCheckInkOnWhite.r", r);
  getValue(*m_settings, "transpCheckInkOnWhite.g", g);
  getValue(*m_settings, "transpCheckInkOnWhite.b", b);
  m_transpCheckInk = TPixel32(r, g, b);
  r = m_transpCheckPaint.r, g = m_transpCheckPaint.g, b = m_transpCheckPaint.b;
  getValue(*m_settings, "transpCheckPaint.r", r);
  getValue(*m_settings, "transpCheckPaint.g", g);
  getValue(*m_settings, "transpCheckPaint.b", b);
  m_transpCheckPaint = TPixel32(r, g, b);

  getValue(*m_settings, "onionInksOnly", m_inksOnly);
  getValue(*m_settings, "iconSizeX", m_iconSize.lx);
  getValue(*m_settings, "iconSizeY", m_iconSize.ly);
  getValue(*m_settings, s_show0ThickLines, m_show0ThickLines);
  getValue(*m_settings, s_regionAntialias, m_regionAntialias);
  getValue(*m_settings, "viewShrink", m_shrink);
  getValue(*m_settings, "viewStep", m_step);
  getValue(*m_settings, "blanksCount", m_blanksCount);
  getValue(*m_settings, "askForOverrideRender", m_askForOverrideRender);
  r = 255, g = 255, b = 255;
  getValue(*m_settings, "blankColor.r", r);
  getValue(*m_settings, "blankColor.g", g);
  getValue(*m_settings, "blankColor.b", b);
  getValue(*m_settings, "undoMemorySize", m_undoMemorySize);
  setUndoMemorySize(m_undoMemorySize);
  m_blankColor = TPixel32(r, g, b);

  // for Pixels only

  getValue(*m_settings, "pixelsOnly",
           m_pixelsOnly);  // doesn't work for some reason.
  QString pos                     = m_settings->value("pixelsOnly").toString();
  if (pos == "true") m_pixelsOnly = true;

  QString units;
  units      = m_settings->value("oldUnits", m_units).toString();
  m_oldUnits = units;

  units = m_settings->value("oldCameraUnits", m_cameraUnits).toString();
  m_oldCameraUnits = units;
  // end for pixels only

  getValue(*m_settings, "projectRoot", m_projectRoot);
  m_customProjectRoot = m_settings->value("customProjectRoot").toString();

  units                    = m_settings->value("linearUnits").toString();
  if (units != "") m_units = units;
  setUnits(m_units.toStdString());

  units                          = m_settings->value("cameraUnits").toString();
  if (units != "") m_cameraUnits = units;
  setCameraUnits(m_cameraUnits.toStdString());

  getValue(*m_settings, "keyframeType", m_keyframeType);

  getValue(*m_settings, "animationStep", m_animationStep);

  getValue(*m_settings, "textureSize", m_textureSize);
  QString scanLevelType;
  scanLevelType = m_settings->value("scanLevelType").toString();
  if (scanLevelType != "") m_scanLevelType = scanLevelType;
  setScanLevelType(m_scanLevelType.toStdString());

  getValue(*m_settings, "shmmax", m_shmmax);
  getValue(*m_settings, "shmseg", m_shmseg);
  getValue(*m_settings, "shmall", m_shmall);
  getValue(*m_settings, "shmmni", m_shmmni);

  // Load level formats
  getDefaultLevelFormats(m_levelFormats);
  getValue(*m_settings, m_levelFormats);
  std::sort(m_levelFormats.begin(),
            m_levelFormats.end(),  // Format sorting must be
            formatLess);           // enforced

  // load languages
  TFilePath lang_path = TEnv::getConfigDir() + "loc";
  TFilePathSet lang_fpset;
  m_languageList.append("English");
  // m_currentLanguage=0;
  try {
    TFileStatus langPathFs(lang_path);

    if (langPathFs.doesExist() && langPathFs.isDirectory()) {
      TSystem::readDirectory(lang_fpset, lang_path, true, false);
    }

    int i = 0;
    for (auto const &newPath : lang_fpset) {
      ++i;
      if (newPath == lang_path) continue;
      if (TFileStatus(newPath).isDirectory()) {
        QString string = QString::fromStdWString(newPath.getWideName());
        m_languageList.append(string);
      }
    }
  } catch (...) {
  }

  // load styles
  TFilePath path(TEnv::getConfigDir() + "qss");
  TFilePathSet fpset;
  try {
    TSystem::readDirectory(fpset, path, true, false);
    int i = -1;
    for (auto const &newPath : fpset) {
      ++i;
      if (newPath == path) continue;
      QString fpName = QString::fromStdWString(newPath.getWideName());
      m_styleSheetList.append(fpName);
    }
  } catch (...) {
  }

  // load rooms or layouts
  QString rooms;
  bool roomsExist = false;
  rooms           = m_settings->value("CurrentRoomChoice").toString();

  TFilePath room_path(TEnv::getStuffDir() + "profiles/layouts/rooms");
  TFilePathSet room_fpset;
  try {
    TSystem::readDirectory(room_fpset, room_path, true, false);
    TFilePathSet::iterator it = room_fpset.begin();
    int i                     = 0;
    for (it; it != room_fpset.end(); it++) {
      TFilePath newPath = *it;
      if (newPath == room_path) continue;
      if (TFileStatus(newPath).isDirectory()) {
        QString string = QString::fromStdWString(newPath.getWideName());
        if (string == rooms) roomsExist = true;
        m_roomMaps[i]                   = string;
        i++;
      }
    }
  } catch (...) {
  }
  // make sure the selected rooms exists
  if (rooms != "" && roomsExist) {
    m_currentRoomChoice = rooms;
    setCurrentRoomChoice(rooms);
  }
  // or set the selected rooms to the first ones in the folder
  else {
    m_currentRoomChoice = m_roomMaps[0];
    setCurrentRoomChoice(0);
  }

  QString currentLanguage;
  currentLanguage = m_settings->value("CurrentLanguageName").toString();
  if (!currentLanguage.isEmpty() && m_languageList.contains(currentLanguage))
    m_currentLanguage = currentLanguage;
  QString currentStyleSheet;
  currentStyleSheet = m_settings->value("CurrentStyleSheetName").toString();
  if (!currentStyleSheet.isEmpty() &&
      m_styleSheetList.contains(currentStyleSheet))
    m_currentStyleSheet = currentStyleSheet;

  getValue(*m_settings, "DragCellsBehaviour", m_dragCellsBehaviour);

  getValue(*m_settings, "LineTestFpsCapture", m_lineTestFpsCapture);
  getValue(*m_settings, "FillOnlysavebox", m_fillOnlySavebox);
  getValue(*m_settings, "AutocreationType", m_autocreationType);
  getValue(*m_settings, "DefLevelType", m_defLevelType);
  getValue(*m_settings, "DefLevelWidth", m_defLevelWidth);
  getValue(*m_settings, "DefLevelHeight", m_defLevelHeight);
  getValue(*m_settings, "DefLevelDpi", m_defLevelDpi);
  getValue(*m_settings, "IgnoreImageDpi", m_ignoreImageDpi);
  getValue(*m_settings, "viewerBGColor", m_viewerBGColor);
  getValue(*m_settings, "previewBGColor", m_previewBGColor);
  getValue(*m_settings, "chessboardColor1", m_chessboardColor1);
  getValue(*m_settings, "chessboardColor2", m_chessboardColor2);
  getValue(*m_settings, "showRasterImagesDarkenBlendedInViewer",
           m_showRasterImagesDarkenBlendedInViewer);
  getValue(*m_settings, "actualPixelViewOnSceneEditingMode",
           m_actualPixelViewOnSceneEditingMode);
  getValue(*m_settings, "viewerZoomCenter", m_viewerZoomCenter);
  getValue(*m_settings, "initialLoadTlvCachingBehavior",
           m_initialLoadTlvCachingBehavior);
  getValue(*m_settings, "removeSceneNumberFromLoadedLevelName",
           m_removeSceneNumberFromLoadedLevelName);
  getValue(*m_settings, "replaceAfterSaveLevelAs", m_replaceAfterSaveLevelAs);
  getValue(*m_settings, "showFrameNumberWithLetters",
           m_showFrameNumberWithLetters);
  getValue(*m_settings, "levelNameOnEachMarkerEnabled",
           m_levelNameOnEachMarker);
  getValue(*m_settings, "columnIconLoadingPolicy", m_columnIconLoadingPolicy);
  getValue(*m_settings, "moveCurrentFrameByClickCellArea",
           m_moveCurrentFrameByClickCellArea);
  getValue(*m_settings, "onionSkinEnabled", m_onionSkinEnabled);
  getValue(*m_settings, "onionSkinDuringPlayback", m_onionSkinDuringPlayback);
  getValue(*m_settings, "multiLayerStylePickerEnabled",
           m_multiLayerStylePickerEnabled);
  getValue(*m_settings, "paletteTypeOnLoadRasterImageAsColorModel",
           m_paletteTypeOnLoadRasterImageAsColorModel);
  getValue(*m_settings, "showKeyframesOnXsheetCellArea",
           m_showKeyframesOnXsheetCellArea);
  QString ffmpegPath = m_settings->value("ffmpegPath").toString();
  if (ffmpegPath != "") m_ffmpegPath = ffmpegPath;
  setFfmpegPath(m_ffmpegPath.toStdString());
  QString fastRenderPath = m_settings->value("fastRenderPath").toString();
  if (fastRenderPath != "") m_fastRenderPath = fastRenderPath;
  setFastRenderPath(m_fastRenderPath.toStdString());
  getValue(*m_settings, "ffmpegTimeout", m_ffmpegTimeout);
  QString shortcutPreset = m_settings->value("shortcutPreset").toString();
  if (shortcutPreset != "") m_shortcutPreset = shortcutPreset;
  setShortcutPreset(m_shortcutPreset.toStdString());
  QString interfaceFont = m_settings->value("interfaceFont").toString();
  if (interfaceFont != "") m_interfaceFont = interfaceFont;
  setInterfaceFont(m_interfaceFont.toStdString());
  getValue(*m_settings, "interfaceFontWeight", m_interfaceFontWeight);
  getValue(*m_settings, "useNumpadForSwitchingStyles",
           m_useNumpadForSwitchingStyles);
  getValue(*m_settings, "guidedDrawingType", m_guidedDrawingType);
  getValue(*m_settings, "animatedGuidedDrawing", m_animatedGuidedDrawing);
  getValue(*m_settings, "newLevelSizeToCameraSizeEnabled",
           m_newLevelSizeToCameraSizeEnabled);
  getValue(*m_settings, "showXSheetToolbar", m_showXSheetToolbar);
  getValue(*m_settings, "expandFunctionHeader", m_expandFunctionHeader);
  getValue(*m_settings, "showColumnNumbers", m_showColumnNumbers);
  getValue(*m_settings, "useArrowKeyToShiftCellSelection",
           m_useArrowKeyToShiftCellSelection);
  getValue(*m_settings, "inputCellsWithoutDoubleClickingEnabled",
           m_inputCellsWithoutDoubleClickingEnabled);
  getValue(*m_settings, "importPolicy", m_importPolicy);
  getValue(*m_settings, "watchFileSystemEnabled", m_watchFileSystem);
  getValue(*m_settings, "shortcutCommandsWhileRenamingCellEnabled",
           m_shortcutCommandsWhileRenamingCellEnabled);

  QString xsheetLayoutPreference;
  xsheetLayoutPreference =
      m_settings->value("xsheetLayoutPreference").toString();
  if (xsheetLayoutPreference != "")
    m_xsheetLayoutPreference = xsheetLayoutPreference;
  else if (existingUser)  // Existing users with missing preference defaults to
                          // Classic. New users will be Classic-revised
    m_xsheetLayoutPreference = QString("Classic");
  setXsheetLayoutPreference(m_xsheetLayoutPreference.toStdString());
  m_loadedXsheetLayout = m_xsheetLayoutPreference;
}

//-----------------------------------------------------------------

Preferences::~Preferences() {
  // DO NOT REMOVE
}

//-----------------------------------------------------------------

Preferences *Preferences::instance() {
  static Preferences _instance;
  return &_instance;
}

//-----------------------------------------------------------------

void Preferences::enableAutoExpose(bool on) {
  m_autoExposeEnabled = on;
  m_settings->setValue("autoExposeEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableSubsceneFolder(bool on) {
  m_subsceneFolderEnabled = on;
  m_settings->setValue("subsceneFolderEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableGeneratedMovieView(bool on) {
  m_generatedMovieViewEnabled = on;
  m_settings->setValue("generatedMovieViewEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableXsheetAutopan(bool on) {
  m_xsheetAutopanEnabled = on;
  m_settings->setValue("xsheetAutopanEnabled", on ? "1" : "0");
}

//------------------------------------------------------------------

void Preferences::enableIgnoreAlphaonColumn1(bool on) {
  m_ignoreAlphaonColumn1Enabled = on;
  m_settings->setValue("ignoreAlphaonColumn1Enabled", on ? "1" : "0");
}

//------------------------------------------------------------------

void Preferences::enableShowKeyframesOnXsheetCellArea(bool on) {
  m_showKeyframesOnXsheetCellArea = on;
  m_settings->setValue("showKeyframesOnXsheetCellArea", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableRewindAfterPlayback(bool on) {
  m_rewindAfterPlaybackEnabled = on;
  m_settings->setValue("rewindAfterPlayback", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableFitToFlipbook(bool on) {
  m_fitToFlipbookEnabled = on;
  m_settings->setValue("fitToFlipbook", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enablePreviewAlwaysOpenNewFlip(bool on) {
  m_previewAlwaysOpenNewFlipEnabled = on;
  m_settings->setValue("previewAlwaysOpenNewFlip", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableAutosave(bool on) {
  m_autosaveEnabled = on;
  m_settings->setValue("autosaveEnabled", on ? "1" : "0");

  if (!on)
    emit stopAutoSave();
  else
    emit startAutoSave();
}

//-----------------------------------------------------------------

void Preferences::enableAutosaveScene(bool on) {
  m_autosaveSceneEnabled = on;
  m_settings->setValue("autosaveSceneEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableAutosaveOtherFiles(bool on) {
  m_autosaveOtherFilesEnabled = on;
  m_settings->setValue("autosaveOtherFilesEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableStartupPopup(bool on) {
  m_startupPopupEnabled = on;
  m_settings->setValue("startupPopupEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setAskForOverrideRender(bool on) {
  m_autosaveEnabled = on;
  m_settings->setValue("askForOverrideRender", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableDefaultViewer(bool on) {
  m_defaultViewerEnabled = on;
  m_settings->setValue("defaultViewerEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableRasterOptimizedMemory(bool on) {
  m_rasterOptimizedMemory = on;
  m_settings->setValue("rasterOptimizedMemory", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableSaveUnpaintedInCleanup(bool on) {
  m_saveUnpaintedInCleanup = on;
  m_settings->setValue("saveUnpaintedInCleanup", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableAutomaticSVNFolderRefresh(bool on) {
  m_automaticSVNFolderRefreshEnabled = on;
  m_settings->setValue("automaticSVNFolderRefreshEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableSVN(bool on) {
  m_SVNEnabled = on;
  m_settings->setValue("SVNEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableMinimizeSaveboxAfterEditing(bool on) {
  m_minimizeSaveboxAfterEditing = on;
  m_settings->setValue("minimizeSaveboxAfterEditing", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setAutosavePeriod(int minutes) {
  m_autosavePeriod = minutes;
  m_settings->setValue("autosavePeriod", QString::number(minutes));
  emit stopAutoSave();
  emit startAutoSave();
}

//-----------------------------------------------------------------

void Preferences::setDefaultTaskChunkSize(int chunkSize) {
  m_chunkSize = chunkSize;
  m_settings->setValue("taskchunksize", QString::number(chunkSize));
}

//-----------------------------------------------------------------

void Preferences::setXsheetStep(int step) {
  m_xsheetStep = step;
  m_settings->setValue("xsheetStep", QString::number(m_xsheetStep));
}

//-----------------------------------------------------------------

void Preferences::setTranspCheckData(const TPixel &bg, const TPixel &ink,
                                     const TPixel &paint) {
  m_transpCheckBg    = bg;
  m_transpCheckInk   = ink;
  m_transpCheckPaint = paint;

  m_settings->setValue("transpCheckInkOnBlack.r",
                       QString::number(m_transpCheckBg.r));
  m_settings->setValue("transpCheckInkOnBlack.g",
                       QString::number(m_transpCheckBg.g));
  m_settings->setValue("transpCheckInkOnBlack.b",
                       QString::number(m_transpCheckBg.b));

  m_settings->setValue("transpCheckInkOnWhite.r",
                       QString::number(m_transpCheckInk.r));
  m_settings->setValue("transpCheckInkOnWhite.g",
                       QString::number(m_transpCheckInk.g));
  m_settings->setValue("transpCheckInkOnWhite.b",
                       QString::number(m_transpCheckInk.b));

  m_settings->setValue("transpCheckPaint.r",
                       QString::number(m_transpCheckPaint.r));
  m_settings->setValue("transpCheckPaint.g",
                       QString::number(m_transpCheckPaint.g));
  m_settings->setValue("transpCheckPaint.b",
                       QString::number(m_transpCheckPaint.b));
}

//-----------------------------------------------------------

void Preferences::setOnionData(const TPixel &frontOnionColor,
                               const TPixel &backOnionColor, bool inksOnly) {
  m_frontOnionColor = frontOnionColor;
  m_backOnionColor  = backOnionColor;
  m_inksOnly        = inksOnly;

  m_settings->setValue("frontOnionColor.r", QString::number(frontOnionColor.r));
  m_settings->setValue("frontOnionColor.g", QString::number(frontOnionColor.g));
  m_settings->setValue("frontOnionColor.b", QString::number(frontOnionColor.b));
  m_settings->setValue("backOnionColor.r", QString::number(backOnionColor.r));
  m_settings->setValue("backOnionColor.g", QString::number(backOnionColor.g));
  m_settings->setValue("backOnionColor.b", QString::number(backOnionColor.b));
  m_settings->setValue("onionInksOnly", QString::number(m_inksOnly));
}

//-----------------------------------------------------------------

void Preferences::setIconSize(const TDimension &dim) {
  m_iconSize = dim;

  m_settings->setValue("iconSizeX", QString::number(dim.lx));
  m_settings->setValue("iconSizeY", QString::number(dim.ly));
}

//-----------------------------------------------------------------

void Preferences::setViewerBGColor(const TPixel32 &color, bool isDragging) {
  m_viewerBGColor = color;
  if (!isDragging) {
    m_settings->setValue("viewerBGColor_R", QString::number((int)color.r));
    m_settings->setValue("viewerBGColor_G", QString::number((int)color.g));
    m_settings->setValue("viewerBGColor_B", QString::number((int)color.b));
    m_settings->setValue("viewerBGColor_M", QString::number((int)color.m));
  }
}

//-----------------------------------------------------------------

void Preferences::setPreviewBGColor(const TPixel32 &color, bool isDragging) {
  m_previewBGColor = color;
  if (!isDragging) {
    m_settings->setValue("previewBGColor_R", QString::number((int)color.r));
    m_settings->setValue("previewBGColor_G", QString::number((int)color.g));
    m_settings->setValue("previewBGColor_B", QString::number((int)color.b));
    m_settings->setValue("previewBGColor_M", QString::number((int)color.m));
  }
}

//-----------------------------------------------------------------

void Preferences::setChessboardColor1(const TPixel32 &color, bool isDragging) {
  m_chessboardColor1 = color;
  if (!isDragging) {
    m_settings->setValue("chessboardColor1_R", QString::number((int)color.r));
    m_settings->setValue("chessboardColor1_G", QString::number((int)color.g));
    m_settings->setValue("chessboardColor1_B", QString::number((int)color.b));
    m_settings->setValue("chessboardColor1_M", QString::number((int)color.m));
  }
}

//-----------------------------------------------------------------

void Preferences::setChessboardColor2(const TPixel32 &color, bool isDragging) {
  m_chessboardColor2 = color;
  if (!isDragging) {
    m_settings->setValue("chessboardColor2_R", QString::number((int)color.r));
    m_settings->setValue("chessboardColor2_G", QString::number((int)color.g));
    m_settings->setValue("chessboardColor2_B", QString::number((int)color.b));
    m_settings->setValue("chessboardColor2_M", QString::number((int)color.m));
  }
}

//-----------------------------------------------------------------

void Preferences::enableShowRasterImagesDarkenBlendedInViewer(bool on) {
  m_showRasterImagesDarkenBlendedInViewer = on;
  m_settings->setValue("showRasterImagesDarkenBlendedInViewer", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableActualPixelViewOnSceneEditingMode(bool on) {
  m_actualPixelViewOnSceneEditingMode = on;
  m_settings->setValue("actualPixelViewOnSceneEditingMode", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableMultiLayerStylePicker(bool on) {
  m_multiLayerStylePickerEnabled = on;
  m_settings->setValue("multiLayerStylePickerEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setViewerZoomCenter(int type) {
  m_viewerZoomCenter = type;
  m_settings->setValue("viewerZoomCenter", type);
}

//-----------------------------------------------------------------

void Preferences::setInitialLoadTlvCachingBehavior(int type) {
  m_initialLoadTlvCachingBehavior = type;
  m_settings->setValue("initialLoadTlvCachingBehavior", type);
}

//-----------------------------------------------------------------

void Preferences::enableShowFrameNumberWithLetters(bool on) {
  m_showFrameNumberWithLetters = on;
  m_settings->setValue("showFrameNumberWithLetters", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableLevelNameOnEachMarker(bool on) {
  m_levelNameOnEachMarker = on;
  m_settings->setValue("levelNameOnEachMarkerEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setColumnIconLoadingPolicy(ColumnIconLoadingPolicy cilp) {
  assert(cilp == LoadAtOnce || cilp == LoadOnDemand);
  m_columnIconLoadingPolicy = cilp;
  m_settings->setValue("columnIconLoadingPolicy", QString::number((int)cilp));
}

//-----------------------------------------------------------------

void Preferences::enableMoveCurrent(bool on) {
  m_moveCurrentFrameByClickCellArea = on;
  m_settings->setValue("moveCurrentFrameByClickCellArea", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableRemoveSceneNumberFromLoadedLevelName(bool on) {
  m_removeSceneNumberFromLoadedLevelName = on;
  m_settings->setValue("removeSceneNumberFromLoadedLevelName", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableReplaceAfterSaveLevelAs(bool on) {
  m_replaceAfterSaveLevelAs = on;
  m_settings->setValue("replaceAfterSaveLevelAs", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableOnionSkin(bool on) {
  m_onionSkinEnabled = on;
  m_settings->setValue("onionSkinEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setOnionSkinDuringPlayback(bool on) {
  m_onionSkinDuringPlayback = on;
  m_settings->setValue("onionSkinDuringPlayback", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setShow0ThickLines(bool on) {
  m_show0ThickLines = on;
  m_settings->setValue(s_show0ThickLines, s_bool[on]);
}

//-----------------------------------------------------------------

void Preferences::setRegionAntialias(bool on) {
  m_regionAntialias = on;
  m_settings->setValue(s_regionAntialias, s_bool[on]);
}

//-----------------------------------------------------------------

void Preferences::setBlankValues(int blanksCount, TPixel32 blankColor) {
  m_blanksCount = blanksCount;
  m_blankColor  = blankColor;
  m_settings->setValue("blanksCount", QString::number(blanksCount));
  m_settings->setValue("blankColor.r", QString::number(blankColor.r));
  m_settings->setValue("blankColor.g", QString::number(blankColor.g));
  m_settings->setValue("blankColor.b", QString::number(blankColor.b));
}

//-----------------------------------------------------------------

void Preferences::setOnionPaperThickness(int thickness) {
  m_onionPaperThickness = thickness;
  m_settings->setValue("onionPaperThickness", QString::number(thickness));
}

//-----------------------------------------------------------------

void Preferences::setViewValues(int shrink, int step) {
  m_shrink = shrink;
  m_step   = step;

  m_settings->setValue("viewShrink", QString::number(shrink));
  m_settings->setValue("viewStep", QString::number(step));
}

//-----------------------------------------------------------------

static void setCurrentUnits(std::string measureName, std::string units) {
  TMeasure *m = TMeasureManager::instance()->get(measureName);
  if (!m) return;
  TUnit *u = m->getUnit(::to_wstring(units));
  if (!u) return;
  m->setCurrentUnit(u);
}

//-----------------------------------------------------------------

void Preferences::setPixelsOnly(bool state) {
  m_pixelsOnly = state;
  m_settings->setValue("pixelsOnly", m_pixelsOnly);
  if (state) {
    storeOldUnits();
  } else {
    resetOldUnits();
  }
}

//-----------------------------------------------------------------

void Preferences::setProjectRoot(int index) {
  // storing the index of the selection instead of the text
  // to make translation work
  m_projectRoot = index;
  m_settings->setValue("projectRoot", m_projectRoot);
}

//-----------------------------------------------------------------

void Preferences::setCustomProjectRoot(std::wstring customProjectRoot) {
  m_customProjectRoot = QString::fromStdWString(customProjectRoot);
  m_settings->setValue("customProjectRoot", m_customProjectRoot);
}

//-----------------------------------------------------------------

void Preferences::setUnits(std::string units) {
  m_units = QString::fromStdString(units);
  m_settings->setValue("linearUnits", m_units);
  setCurrentUnits("length", units);
  setCurrentUnits("length.x", units);
  setCurrentUnits("length.y", units);
  setCurrentUnits("length.lx", units);
  setCurrentUnits("length.ly", units);
  setCurrentUnits("fxLength", units);
  setCurrentUnits("pippo", units);
}

//-----------------------------------------------------------------

void Preferences::setCameraUnits(std::string units) {
  m_cameraUnits = QString::fromStdString(units);
  m_settings->setValue("cameraUnits", m_cameraUnits);
  setCurrentUnits("camera.lx", units);
  setCurrentUnits("camera.ly", units);
}

//-----------------------------------------------------------------

void Preferences::storeOldUnits() {
  m_oldUnits       = getUnits();
  m_oldCameraUnits = getCameraUnits();
  m_settings->setValue("oldUnits", m_oldUnits);
  m_settings->setValue("oldCameraUnits", m_oldCameraUnits);
}

//-----------------------------------------------------------------

void Preferences::resetOldUnits() {
  if (m_oldUnits != "" && m_oldCameraUnits != "") {
    setUnits(m_oldUnits.toStdString());
    setCameraUnits(m_oldCameraUnits.toStdString());
  }
}

//-----------------------------------------------------------------

void Preferences::setCurrentRoomChoice(int currentRoomChoice) {
  m_currentRoomChoice = getRoomChoice(currentRoomChoice);
  m_settings->setValue("CurrentRoomChoice", m_currentRoomChoice);
}

//-----------------------------------------------------------------

void Preferences::setCurrentRoomChoice(QString currentRoomChoice) {
  m_currentRoomChoice = currentRoomChoice;
  m_settings->setValue("CurrentRoomChoice", m_currentRoomChoice);
}

//-----------------------------------------------------------------

QString Preferences::getCurrentRoomChoice() const {
  return m_currentRoomChoice;
}

//-----------------------------------------------------------------

int Preferences::getRoomChoiceCount() const { return m_roomMaps.size(); }

//-----------------------------------------------------------------

QString Preferences::getRoomChoice(int index) const {
  return m_roomMaps[index];
}

//-----------------------------------------------------------------

void Preferences::setScanLevelType(std::string type) {
  m_scanLevelType = QString::fromStdString(type);
  m_settings->setValue("scanLevelType", m_scanLevelType);
}

//-----------------------------------------------------------------

void Preferences::setKeyframeType(int s) {
  m_keyframeType = s;
  m_settings->setValue("keyframeType", s);
}
//-----------------------------------------------------------------

void Preferences::setAnimationStep(int s) {
  m_animationStep = s;
  m_settings->setValue("animationStep", s);
}

//-----------------------------------------------------------------

void Preferences::setUndoMemorySize(int memorySize) {
  m_undoMemorySize = memorySize;
  TUndoManager::manager()->setUndoMemorySize(memorySize);
  m_settings->setValue("undoMemorySize", memorySize);
}

//-----------------------------------------------------------------

QString Preferences::getCurrentLanguage() const {
  if (m_languageList.contains(m_currentLanguage)) return m_currentLanguage;
  // If no valid option selected, then return English
  return m_languageList[0];
}

//-----------------------------------------------------------------

QString Preferences::getLanguage(int index) const {
  return m_languageList[index];
}

//-----------------------------------------------------------------

int Preferences::getLanguageCount() const { return (int)m_languageList.size(); }

//-----------------------------------------------------------------

void Preferences::setCurrentLanguage(const QString &currentLanguage) {
  m_currentLanguage = currentLanguage;
  m_settings->setValue("CurrentLanguageName", m_currentLanguage);
}

//-----------------------------------------------------------------

QString Preferences::getCurrentStyleSheetName() const {
  if (m_styleSheetList.contains(m_currentStyleSheet))
    return m_currentStyleSheet;
  // If no valid option selected, then return the first oprion
  return m_styleSheetList.isEmpty() ? QString() : m_styleSheetList[0];
}

//-----------------------------------------------------------------

void Preferences::setInterfaceFont(std::string font) {
  m_interfaceFont = QString::fromStdString(font);
  m_settings->setValue("interfaceFont", m_interfaceFont);
}

//-----------------------------------------------------------------

void Preferences::setInterfaceFontWeight(int weight) {
  m_interfaceFontWeight = weight;
  m_settings->setValue("interfaceFontWeight", m_interfaceFontWeight);
}

//-----------------------------------------------------------------

QString Preferences::getCurrentStyleSheetPath() const {
  if (m_currentStyleSheet.isEmpty()) return QString();
  TFilePath path(TEnv::getConfigDir() + "qss");
  QString string = m_currentStyleSheet + QString("/") + m_currentStyleSheet +
                   QString(".qss");
  return QString("file:///" + path.getQString() + "/" + string);
}

//-----------------------------------------------------------------

QString Preferences::getStyleSheet(int index) const {
  return m_styleSheetList[index];
}

//-----------------------------------------------------------------

int Preferences::getStyleSheetCount() const {
  return (int)m_styleSheetList.size();
}

//-----------------------------------------------------------------

void Preferences::setCurrentStyleSheet(const QString &currentStyleSheet) {
  m_currentStyleSheet = currentStyleSheet;
  m_settings->setValue("CurrentStyleSheetName", m_currentStyleSheet);
}
//-----------------------------------------------------------------

void Preferences::setAutocreationType(int autocreationType) {
  m_autocreationType = autocreationType;
  m_settings->setValue("AutocreationType", m_autocreationType);
}

//-----------------------------------------------------------------

void Preferences::setDragCellsBehaviour(int dragCellsBehaviour) {
  m_dragCellsBehaviour = dragCellsBehaviour;
  m_settings->setValue("DragCellsBehaviour", m_dragCellsBehaviour);
}

//-----------------------------------------------------------------

void Preferences::setLineTestFpsCapture(int lineTestFpsCapture) {
  m_lineTestFpsCapture = lineTestFpsCapture;
  m_settings->setValue("LineTestFpsCapture", m_lineTestFpsCapture);
}

//-----------------------------------------------------------------

void Preferences::setFillOnlySavebox(bool on) {
  m_fillOnlySavebox = on;
  m_settings->setValue("FillOnlysavebox", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableLevelsBackup(bool enabled) {
  m_levelsBackupEnabled = enabled;
  m_settings->setValue("levelsBackupEnabled", enabled ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableSceneNumbering(bool enabled) {
  m_sceneNumberingEnabled = enabled;
  m_settings->setValue("sceneNumberingEnabled", enabled ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setDropdownShortcutsCycleOptions(bool on) {
  m_dropdownShortcutsCycleOptions = on;
  m_settings->setValue("dropdownShortcutsCycleOptions", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setDefLevelType(int levelType) {
  m_defLevelType = levelType;
  m_settings->setValue("DefLevelType", levelType);
}

//-----------------------------------------------------------------

void Preferences::setDefLevelWidth(double width) {
  m_defLevelWidth = width;
  m_settings->setValue("DefLevelWidth", width);
}

//-----------------------------------------------------------------

void Preferences::setDefLevelHeight(double height) {
  m_defLevelHeight = height;
  m_settings->setValue("DefLevelHeight", height);
}

//-----------------------------------------------------------------

void Preferences::setDefLevelDpi(double dpi) {
  m_defLevelDpi = dpi;
  m_settings->setValue("DefLevelDpi", dpi);
}

//-----------------------------------------------------------------

void Preferences::setIgnoreImageDpi(bool on) {
  m_ignoreImageDpi = on;
  m_settings->setValue("IgnoreImageDpi", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setVectorSnappingTarget(int target) {
  m_vectorSnappingTarget = target;
  m_settings->setValue("vectorSnappingTarget", target);
}

//-----------------------------------------------------------------

void Preferences::setPaletteTypeOnLoadRasterImageAsColorModel(int type) {
  m_paletteTypeOnLoadRasterImageAsColorModel = type;
  m_settings->setValue("paletteTypeOnLoadRasterImageAsColorModel", type);
}

//-----------------------------------------------------------------

void Preferences::setFfmpegPath(std::string path) {
  m_ffmpegPath        = QString::fromStdString(path);
  std::string strPath = m_ffmpegPath.toStdString();
  m_settings->setValue("ffmpegPath", m_ffmpegPath);
}

//-----------------------------------------------------------------

void Preferences::setFastRenderPath(std::string path) {
  m_fastRenderPath    = QString::fromStdString(path);
  std::string strPath = m_ffmpegPath.toStdString();
  m_settings->setValue("fastRenderPath", m_fastRenderPath);
}

//-----------------------------------------------------------------

void Preferences::setShortcutPreset(std::string preset) {
  m_shortcutPreset = QString::fromStdString(preset);
  m_settings->setValue("shortcutPreset", m_shortcutPreset);
}

//-----------------------------------------------------------------

void Preferences::setPrecompute(bool enabled) { m_precompute = enabled; }

//-----------------------------------------------------------------

void Preferences::setFfmpegTimeout(int seconds) {
  m_ffmpegTimeout = seconds;
  m_settings->setValue("ffmpegTimeout", seconds);
}

//-----------------------------------------------------------------

void Preferences::setDefaultImportPolicy(int policy) {
  m_importPolicy = policy;
  m_settings->setValue("importPolicy", policy);
}

//-----------------------------------------------------------------

int Preferences::addLevelFormat(const LevelFormat &format) {
  LevelFormatVector::iterator lft = m_levelFormats.insert(
      std::upper_bound(m_levelFormats.begin(), m_levelFormats.end(), format,
                       formatLess),
      format);

  int formatIdx = int(
      lft -
      m_levelFormats.begin());  // NOTE: Must be disjoint from the instruction
  //       above, since operator-'s param evaluation
  //       order is unspecified
  setValue(*m_settings, m_levelFormats);

  return formatIdx;
}

//-----------------------------------------------------------------

void Preferences::removeLevelFormat(int formatIdx) {
  assert(0 <= formatIdx && formatIdx < int(m_levelFormats.size()));
  m_levelFormats.erase(m_levelFormats.begin() + formatIdx);

  setValue(*m_settings, m_levelFormats);
}

//-----------------------------------------------------------------

const Preferences::LevelFormat &Preferences::levelFormat(int formatIdx) const {
  assert(0 <= formatIdx && formatIdx < int(m_levelFormats.size()));
  return m_levelFormats[formatIdx];
}

//-----------------------------------------------------------------

int Preferences::levelFormatsCount() const {
  return int(m_levelFormats.size());
}

//-----------------------------------------------------------------

int Preferences::matchLevelFormat(const TFilePath &fp) const {
  LevelFormatVector::const_iterator lft =
      std::find_if(m_levelFormats.begin(), m_levelFormats.end(),
                   boost::bind(&LevelFormat::matches, _1, boost::cref(fp)));

  return (lft != m_levelFormats.end()) ? lft - m_levelFormats.begin() : -1;
}

//-----------------------------------------------------------------

void Preferences::enableUseNumpadForSwitchingStyles(bool on) {
  m_useNumpadForSwitchingStyles = on;
  m_settings->setValue("useNumpadForSwitchingStyles", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::setGuidedDrawing(int status) {
  m_guidedDrawingType = status;
  m_settings->setValue("guidedDrawingType", status);
}

//-----------------------------------------------------------------

void Preferences::setAnimatedGuidedDrawing(bool status) {
  m_animatedGuidedDrawing = status;
  m_settings->setValue("animatedGuidedDrawing", status);
}

//-----------------------------------------------------------------

void Preferences::enableNewLevelSizeToCameraSize(bool on) {
  m_newLevelSizeToCameraSizeEnabled = on;
  m_settings->setValue("newLevelSizeToCameraSizeEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableShowXSheetToolbar(bool on) {
  m_showXSheetToolbar = on;
  m_settings->setValue("showXSheetToolbar", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableExpandFunctionHeader(bool on) {
  m_expandFunctionHeader = on;
  m_settings->setValue("expandFunctionHeader", on ? "1" : "0");
}

void Preferences::enableShowColumnNumbers(bool on) {
  m_showColumnNumbers = on;
  m_settings->setValue("showColumnNumbers", on ? "1" : "0");
}

void Preferences::setXsheetLayoutPreference(std::string layout) {
  m_xsheetLayoutPreference = QString::fromStdString(layout);
  m_settings->setValue("xsheetLayoutPreference", m_xsheetLayoutPreference);
}

void Preferences::setLoadedXsheetLayout(std::string layout) {
  m_loadedXsheetLayout = QString::fromStdString(layout);
}

//-----------------------------------------------------------------

void Preferences::enableUseArrowKeyToShiftCellSelection(bool on) {
  m_useArrowKeyToShiftCellSelection = on;
  m_settings->setValue("useArrowKeyToShiftCellSelection", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableInputCellsWithoutDoubleClicking(bool on) {
  m_inputCellsWithoutDoubleClickingEnabled = on;
  m_settings->setValue("inputCellsWithoutDoubleClickingEnabled",
                       on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableWatchFileSystem(bool on) {
  m_watchFileSystem = on;
  m_settings->setValue("watchFileSystemEnabled", on ? "1" : "0");
}

//-----------------------------------------------------------------

void Preferences::enableShortcutCommandsWhileRenamingCell(bool on) {
  m_shortcutCommandsWhileRenamingCellEnabled = on;
  m_settings->setValue("shortcutCommandsWhileRenamingCellEnabled",
                       on ? "1" : "0");
}