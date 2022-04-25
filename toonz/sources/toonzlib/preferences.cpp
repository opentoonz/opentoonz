

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
#include "timage_io.h"

// Qt includes
#include <QSettings>
#include <QStringList>
#include <QAction>
#include <QColor>
#include <QTextStream>

// boost includes
#include <boost/bind.hpp>

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

typedef Preferences::LevelFormat LevelFormat;
typedef std::vector<LevelFormat> LevelFormatVector;

//-----------------------------------------------------------------

const char *s_levelFormats = "levelFormats";

const char *s_name = "name", *s_regexp = "regexp", *s_priority = "priority";

const char *s_dpiPolicy = "dpiPolicy", *s_dpi = "dpi",
           *s_subsampling = "subsampling", *s_antialias = "antialias",
           *s_premultiply = "premultiply", *s_whiteTransp = "whiteTransp";

//=================================================================

inline QString colorToString(const QColor &color) {
  return QString("%1 %2 %3 %4")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(color.alpha());
}

inline QColor stringToColor(const QString &str) {
  QStringList values = str.split(' ');
  return QColor(values[0].toInt(), values[1].toInt(), values[2].toInt(),
                values[3].toInt());
}

inline TPixel colorToTPixel(const QColor &color) {
  return TPixel(color.red(), color.green(), color.blue(), color.alpha());
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
  lfv.resize(2);
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
    // UPDATE : from V1.5, PNG images are premultiplied on loading
    // lfv[2].m_name                  = Preferences::tr("PNG");
    // lfv[2].m_pathFormat            = QRegExp("..*\\.png",
    // Qt::CaseInsensitive); lfv[2].m_options.m_premultiply = true;
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

void _setValue(QSettings &settings, const LevelFormatVector &lfv) {
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

  // from OT V1.5, PNG images are premultiplied on loading.
  // Leaving the premultiply option will cause unwanted double operation.
  // So, check the loaded options and modify it "silently".
  bool changed                   = false;
  LevelFormatVector::iterator it = lfv.begin();
  while (it != lfv.end()) {
    if ((*it).m_name == Preferences::tr("PNG") &&
        (*it).m_pathFormat == QRegExp("..*\\.png", Qt::CaseInsensitive) &&
        (*it).m_options.m_premultiply == true) {
      LevelOptions defaultValue;
      defaultValue.m_premultiply = true;
      // if other parameters are the same as default, just erase the item
      if ((*it).m_options == defaultValue) it = lfv.erase(it);
      // if there are some adjustments by user, then disable only premultiply
      // option
      else {
        (*it).m_options.m_premultiply = false;
        ++it;
      }
      changed = true;
    }
    // remove the "empty" condition which may inserted due to the previous bug
    else if ((*it).m_name.isEmpty() &&
             (*it).m_pathFormat == QRegExp(".*", Qt::CaseInsensitive) &&
             (*it).m_priority == 1 && (*it).m_options == LevelOptions()) {
      it      = lfv.erase(it);
      changed = true;
    } else
      ++it;
  }
  // overwrite the setting
  if (changed) _setValue(settings, lfv);
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

Preferences::Preferences() {
  // load preference file
  TFilePath layoutDir = ToonzFolder::getMyModuleDir();
  TFilePath prefPath  = layoutDir + TFilePath("preferences.ini");
  // In case the personal settings is not exist (for new users)
  if (!TFileStatus(prefPath).doesExist()) {
    TFilePath templatePath =
        ToonzFolder::getTemplateModuleDir() + TFilePath("preferences.ini");
    // If there is the template, copy it to the personal one
    if (TFileStatus(templatePath).doesExist())
      TSystem::copyFile(prefPath, templatePath);
  }
  m_settings.reset(new QSettings(
      QString::fromStdWString(prefPath.getWideString()), QSettings::IniFormat));

  initializeOptions();

  definePreferenceItems();
  // resolve compatibility for deprecated items
  resolveCompatibility();

  // initialize environment based on loaded preferences
  setUnits();
  setCameraUnits();
  setUndoMemorySize();

  // Load level formats
  getDefaultLevelFormats(m_levelFormats);
  getValue(*m_settings, m_levelFormats);
  std::sort(m_levelFormats.begin(),
            m_levelFormats.end(),  // Format sorting must be
            formatLess);           // enforced

  if (m_roomMaps.key(getStringValue(CurrentRoomChoice), -1) == -1) {
    assert(!m_roomMaps.isEmpty());
    setValue(CurrentRoomChoice, m_roomMaps[0]);
  }

  if (!m_styleSheetList.contains(getStringValue(CurrentStyleSheetName)))
    setValue(CurrentStyleSheetName, "Default");

  if (!m_languageList.contains(getStringValue(CurrentLanguageName)))
    setValue(CurrentLanguageName, "English");

  TImageWriter::setBackgroundColor(getColorValue(rasterBackgroundColor));
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
// load and initialize options for languages, styles and rooms

void Preferences::initializeOptions() {
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
  TFilePath room_path(ToonzFolder::getRoomsDir());
  TFilePathSet room_fpset;
  try {
    TSystem::readDirectory(room_fpset, room_path, true, false);
    TFilePathSet::iterator it = room_fpset.begin();
    for (int i = 0; it != room_fpset.end(); it++, i++) {
      TFilePath newPath = *it;
      if (newPath == room_path) continue;
      if (TFileStatus(newPath).isDirectory()) {
        QString string = QString::fromStdWString(newPath.getWideName());
        m_roomMaps[i]  = string;
      }
    }
  } catch (...) {
  }
}

//-----------------------------------------------------------------

void Preferences::definePreferenceItems() {
  // General
  define(defaultViewerEnabled, "defaultViewerEnabled", QMetaType::Bool, false);
  define(rasterOptimizedMemory, "rasterOptimizedMemory", QMetaType::Bool,
         false);
  define(startupPopupEnabled, "startupPopupEnabled", QMetaType::Bool, true);
  define(undoMemorySize, "undoMemorySize", QMetaType::Int, 100, 0, 2000);
  define(taskchunksize, "taskchunksize", QMetaType::Int, 10, 1, 2000);
  define(sceneNumberingEnabled, "sceneNumberingEnabled", QMetaType::Bool,
         false);
  define(watchFileSystemEnabled, "watchFileSystemEnabled", QMetaType::Bool,
         true);
  define(projectRoot, "projectRoot", QMetaType::Int, 0x08);
  define(customProjectRoot, "customProjectRoot", QMetaType::QString, "");
  define(pathAliasPriority, "pathAliasPriority", QMetaType::Int,
         (int)ProjectFolderOnly);

  setCallBack(undoMemorySize, &Preferences::setUndoMemorySize);

  // Interface
  define(CurrentStyleSheetName, "CurrentStyleSheetName", QMetaType::QString,
         "Default");

  // Qt has a bug in recent versions that Menu item Does not show correctly
  // (QTBUG-90242) Since the current OT is made to handle such issue, so we need
  // to apply an extra adjustment when it is run on the older versions (5.9.x)
  // of Qt
  // Update: confirmed that the bug does not appear at least in Qt 5.12.8
  QString defaultAditionalSheet = "";
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 9)
  defaultAditionalSheet = "QMenu::Item{ padding: 3 28 3 28; }";
#endif

  define(additionalStyleSheet, "additionalStyleSheet", QMetaType::QString,
         defaultAditionalSheet);
  define(iconTheme, "iconTheme", QMetaType::Bool, false);
  define(pixelsOnly, "pixelsOnly", QMetaType::Bool, false);
  define(oldUnits, "oldUnits", QMetaType::QString, "mm");
  define(oldCameraUnits, "oldCameraUnits", QMetaType::QString, "inch");
  define(linearUnits, "linearUnits", QMetaType::QString, "mm");
  define(cameraUnits, "cameraUnits", QMetaType::QString, "inch");
  define(CurrentRoomChoice, "CurrentRoomChoice", QMetaType::QString, "Default");
  define(functionEditorToggle, "functionEditorToggle", QMetaType::Int,
         (int)ShowGraphEditorInPopup);
  define(moveCurrentFrameByClickCellArea, "moveCurrentFrameByClickCellArea",
         QMetaType::Bool, true);
  define(actualPixelViewOnSceneEditingMode, "actualPixelViewOnSceneEditingMode",
         QMetaType::Bool, false);
  define(showRasterImagesDarkenBlendedInViewer,
         "showRasterImagesDarkenBlendedInViewer", QMetaType::Bool, false);
  define(iconSize, "iconSize", QMetaType::QSize, QSize(80, 45), QSize(10, 10),
         QSize(400, 400));
  define(viewShrink, "viewShrink", QMetaType::Int, 1, 1, 20);
  define(viewStep, "viewStep", QMetaType::Int, 1, 1, 20);
  define(viewerZoomCenter, "viewerZoomCenter", QMetaType::Int,
         0);  // Mouse Cursor
  define(CurrentLanguageName, "CurrentLanguageName", QMetaType::QString,
         "English");
#ifdef _WIN32
  QString defaultFont("Segoe UI");
#elif defined Q_OS_MACOS
  QString defaultFont("Helvetica Neue");
#else
  QString defaultFont("Helvetica");
#endif
  define(interfaceFont, "interfaceFont", QMetaType::QString, defaultFont);
  define(interfaceFontStyle, "interfaceFontStyle", QMetaType::QString,
         "Regular");
  define(colorCalibrationEnabled, "colorCalibrationEnabled", QMetaType::Bool,
         false);
  define(colorCalibrationLutPaths, "colorCalibrationLutPaths",
         QMetaType::QVariantMap, QVariantMap());
  define(displayIn30bit, "displayIn30bit", QMetaType::Bool, false);

  // hide menu icons by default in macOS since the icon color may not match with
  // the system color theme
#ifdef Q_OS_MACOS
  bool defIconsVisible = false;
#else
  bool defIconsVisible = true;
#endif
  define(showIconsInMenu, "showIconsInMenu", QMetaType::Bool, defIconsVisible);

  setCallBack(pixelsOnly, &Preferences::setPixelsOnly);
  setCallBack(linearUnits, &Preferences::setUnits);
  setCallBack(cameraUnits, &Preferences::setCameraUnits);

  // Visualization
  define(show0ThickLines, "show0ThickLines", QMetaType::Bool, true);
  define(regionAntialias, "regionAntialias", QMetaType::Bool, false);

  // Loading
  define(importPolicy, "importPolicy", QMetaType::Int, 0);  // Always ask
  define(autoExposeEnabled, "autoExposeEnabled", QMetaType::Bool, true);
  define(subsceneFolderEnabled, "subsceneFolderEnabled", QMetaType::Bool, true);
  define(removeSceneNumberFromLoadedLevelName,
         "removeSceneNumberFromLoadedLevelName", QMetaType::Bool, false);
  define(IgnoreImageDpi, "IgnoreImageDpi", QMetaType::Bool, false);
  define(initialLoadTlvCachingBehavior, "initialLoadTlvCachingBehavior",
         QMetaType::Int, 0);  // On Demand
  define(columnIconLoadingPolicy, "columnIconLoadingPolicy", QMetaType::Int,
         (int)LoadAtOnce);
  define(autoRemoveUnusedLevels, "autoRemoveUnusedLevels", QMetaType::Bool,
         false);

  //"levelFormats" need to be handle separately

  // Saving
  define(autosaveEnabled, "autosaveEnabled", QMetaType::Bool, false);
  define(autosavePeriod, "autosavePeriod", QMetaType::Int, 15, 1, 60);
  define(autosaveSceneEnabled, "autosaveSceneEnabled", QMetaType::Bool, true);
  define(autosaveOtherFilesEnabled, "autosaveOtherFilesEnabled",
         QMetaType::Bool, true);
  define(replaceAfterSaveLevelAs, "replaceAfterSaveLevelAs", QMetaType::Bool,
         true);
  define(backupEnabled, "backupEnabled", QMetaType::Bool, true);
  define(backupKeepCount, "backupKeepCount", QMetaType::Int, 1, 1,
         std::numeric_limits<int>::max());
  define(rasterBackgroundColor, "rasterBackgroundColor", QMetaType::QColor,
         QColor(Qt::white));
  define(resetUndoOnSavingLevel, "resetUndoOnSavingLevel", QMetaType::Bool,
         true);

  setCallBack(rasterBackgroundColor, &Preferences::setRasterBackgroundColor);
  setCallBack(autosaveEnabled, &Preferences::enableAutosave);
  setCallBack(autosavePeriod, &Preferences::setAutosavePeriod);

  // Import / Export
  define(ffmpegPath, "ffmpegPath", QMetaType::QString, "");
  define(ffmpegTimeout, "ffmpegTimeout", QMetaType::Int, 600, 1,
         std::numeric_limits<int>::max());
  define(fastRenderPath, "fastRenderPath", QMetaType::QString, "desktop");
  define(ffmpegMultiThread, "ffmpegMultiThread", QMetaType::Bool, false);
  define(rhubarbPath, "rhubarbPath", QMetaType::QString, "");
  define(rhubarbTimeout, "rhubarbTimeout", QMetaType::Int, 600, 0,
         std::numeric_limits<int>::max());

  // Drawing
  define(DefRasterFormat, "DefRasterFormat", QMetaType::QString, "tif");
  // define(scanLevelType, "scanLevelType", QMetaType::QString, "tif");
  define(DefLevelType, "DefLevelType", QMetaType::Int, TZP_XSHLEVEL);
  define(newLevelSizeToCameraSizeEnabled, "newLevelSizeToCameraSizeEnabled",
         QMetaType::Bool, false);
  define(DefLevelWidth, "DefLevelWidth", QMetaType::Double,
         TCamera().getSize().lx, 0.1, std::numeric_limits<double>::max());
  define(DefLevelHeight, "DefLevelHeight", QMetaType::Double,
         TCamera().getSize().ly, 0.1, std::numeric_limits<double>::max());
  define(DefLevelDpi, "DefLevelDpi", QMetaType::Double, TCamera().getDpi().x,
         0.1, std::numeric_limits<double>::max());

  define(EnableAutocreation, "EnableAutocreation", QMetaType::Bool, true);
  define(NumberingSystem, "NumberingSystem", QMetaType::Int, 0);  // Incremental
  define(EnableAutoStretch, "EnableAutoStretch", QMetaType::Bool, true);
  define(EnableCreationInHoldCells, "EnableCreationInHoldCells",
         QMetaType::Bool, true);
  define(EnableAutoRenumber, "EnableAutoRenumber", QMetaType::Bool, true);

  define(vectorSnappingTarget, "vectorSnappingTarget", QMetaType::Int,
         (int)SnapAll);
  define(saveUnpaintedInCleanup, "saveUnpaintedInCleanup", QMetaType::Bool,
         true);
  define(minimizeSaveboxAfterEditing, "minimizeSaveboxAfterEditing",
         QMetaType::Bool, true);
  define(useNumpadForSwitchingStyles, "useNumpadForSwitchingStyles",
         QMetaType::Bool, true);
  define(downArrowInLevelStripCreatesNewFrame,
         "downArrowInLevelStripCreatesNewFrame", QMetaType::Bool, true);
  define(keepFillOnVectorSimplify, "keepFillOnVectorSimplify", QMetaType::Bool,
         true);
  define(useHigherDpiOnVectorSimplify, "useHigherDpiOnVectorSimplify",
         QMetaType::Bool, false);

  // Tools
  // define(dropdownShortcutsCycleOptions, "dropdownShortcutsCycleOptions",
  //       QMetaType::Int,
  //       1);  // Cycle through the available options (changed from bool to
  //       int)
  define(FillOnlysavebox, "FillOnlysavebox", QMetaType::Bool, false);
  define(multiLayerStylePickerEnabled, "multiLayerStylePickerEnabled",
         QMetaType::Bool, false);
  define(cursorBrushType, "cursorBrushType", QMetaType::QString, "Small");
  define(cursorBrushStyle, "cursorBrushStyle", QMetaType::QString, "Default");
  define(cursorOutlineEnabled, "cursorOutlineEnabled", QMetaType::Bool, true);
  define(levelBasedToolsDisplay, "levelBasedToolsDisplay", QMetaType::Int,
         0);  // Default
  define(useCtrlAltToResizeBrush, "useCtrlAltToResizeBrush", QMetaType::Bool,
         true);
  define(tempToolSwitchTimer, "tempToolSwitchTimer", QMetaType::Int, 500, 1,
         std::numeric_limits<int>::max());

  // Xsheet
  define(xsheetLayoutPreference, "xsheetLayoutPreference", QMetaType::QString,
         "Classic-revised");
  define(xsheetStep, "xsheetStep", QMetaType::Int, 10, 0,
         std::numeric_limits<int>::max());
  define(xsheetAutopanEnabled, "xsheetAutopanEnabled", QMetaType::Bool, true);
  define(DragCellsBehaviour, "DragCellsBehaviour", QMetaType::Int,
         1);  // Cells and Column Data
  define(ignoreAlphaonColumn1Enabled, "ignoreAlphaonColumn1Enabled",
         QMetaType::Bool, false);
  define(showKeyframesOnXsheetCellArea, "showKeyframesOnXsheetCellArea",
         QMetaType::Bool, true);
  define(showXsheetCameraColumn, "showXsheetCameraColumn", QMetaType::Bool,
         true);
  define(useArrowKeyToShiftCellSelection, "useArrowKeyToShiftCellSelection",
         QMetaType::Bool, true);
  define(inputCellsWithoutDoubleClickingEnabled,
         "inputCellsWithoutDoubleClickingEnabled", QMetaType::Bool, false);
  define(shortcutCommandsWhileRenamingCellEnabled,
         "shortcutCommandsWhileRenamingCellEnabled", QMetaType::Bool, false);
  define(showXSheetToolbar, "showXSheetToolbar", QMetaType::Bool, true);
  define(expandFunctionHeader, "expandFunctionHeader", QMetaType::Bool, false);
  define(showColumnNumbers, "showColumnNumbers", QMetaType::Bool, false);
  define(parentColorsInXsheetColumn, "parentColorsInXsheetColumn",
         QMetaType::Bool, false);
  define(highlightLineEverySecond, "highlightLineEverySecond", QMetaType::Bool,
         false);
  define(syncLevelRenumberWithXsheet, "syncLevelRenumberWithXsheet",
         QMetaType::Bool, true);
  define(currentTimelineEnabled, "currentTimelineEnabled", QMetaType::Bool,
         true);
  define(currentColumnColor, "currentColumnColor", QMetaType::QColor,
         QColor(Qt::yellow));
  // define(levelNameOnEachMarkerEnabled, "levelNameOnEachMarkerEnabled",
  //  QMetaType::Bool, false);
  define(levelNameDisplayType, "levelNameDisplayType", QMetaType::Int,
         0);  // default
  define(showFrameNumberWithLetters, "showFrameNumberWithLetters",
         QMetaType::Bool, false);

  // Animation
  define(keyframeType, "keyframeType", QMetaType::Int, 2);  // Linear
  define(animationStep, "animationStep", QMetaType::Int, 1, 1, 500);
  define(modifyExpressionOnMovingReferences,
         "modifyExpressionOnMovingReferences", QMetaType::Bool, false);

  // Preview
  define(blanksCount, "blanksCount", QMetaType::Int, 0, 0, 1000);
  define(blankColor, "blankColor", QMetaType::QColor, QColor(Qt::white));
  define(rewindAfterPlayback, "rewindAfterPlayback", QMetaType::Bool, true);
  define(shortPlayFrameCount, "shortPlayFrameCount", QMetaType::Int, 8, 1, 100);
  define(previewAlwaysOpenNewFlip, "previewAlwaysOpenNewFlip", QMetaType::Bool,
         false);
  define(fitToFlipbook, "fitToFlipbook", QMetaType::Bool, false);
  define(generatedMovieViewEnabled, "generatedMovieViewEnabled",
         QMetaType::Bool, true);

  // Onion Skin
  define(onionSkinEnabled, "onionSkinEnabled", QMetaType::Bool, true);
  define(onionPaperThickness, "onionPaperThickness", QMetaType::Int, 50, 0,
         100);
  define(backOnionColor, "backOnionColor", QMetaType::QColor, QColor(Qt::red));
  define(frontOnionColor, "frontOnionColor", QMetaType::QColor,
         QColor(Qt::green));
  define(onionInksOnly, "onionInksOnly", QMetaType::Bool, false);
  define(onionSkinDuringPlayback, "onionSkinDuringPlayback", QMetaType::Bool,
         false);
  define(useOnionColorsForShiftAndTraceGhosts,
         "useOnionColorsForShiftAndTraceGhosts", QMetaType::Bool, true);
  define(animatedGuidedDrawing, "animatedGuidedDrawing", QMetaType::Int,
         0);  // Arrow Markers (changed from bool to int)

  // Colors
  define(viewerBGColor, "viewerBGColor", QMetaType::QColor,
         QColor(128, 128, 128));
  define(previewBGColor, "previewBGColor", QMetaType::QColor,
         QColor(64, 64, 64));
  define(levelEditorBoxColor, "levelEditorBoxColor", QMetaType::QColor,
         QColor(128, 128, 128));
  define(chessboardColor1, "chessboardColor1", QMetaType::QColor,
         QColor(180, 180, 180));
  define(chessboardColor2, "chessboardColor2", QMetaType::QColor,
         QColor(230, 230, 230));
  define(transpCheckInkOnWhite, "transpCheckInkOnWhite", QMetaType::QColor,
         QColor(Qt::black));
  define(transpCheckInkOnBlack, "transpCheckInkOnBlack", QMetaType::QColor,
         QColor(Qt::white));
  define(transpCheckPaint, "transpCheckPaint", QMetaType::QColor,
         QColor(127, 127, 127));

  // Version Control
  define(SVNEnabled, "SVNEnabled", QMetaType::Bool, false);
  define(automaticSVNFolderRefreshEnabled, "automaticSVNFolderRefreshEnabled",
         QMetaType::Bool, true);
  define(latestVersionCheckEnabled, "latestVersionCheckEnabled",
         QMetaType::Bool, true);

  // Touch / Tablet Settings
  // TounchGestureControl // Touch Gesture is a checkable command and not in
  // preferences.ini
  define(winInkEnabled, "winInkEnabled", QMetaType::Bool, false);
  // This option will be shown & available only when WITH_WINTAB is defined
  define(useQtNativeWinInk, "useQtNativeWinInk", QMetaType::Bool, false);

  // Others (not appeared in the popup)
  // Shortcut popup settings
  define(shortcutPreset, "shortcutPreset", QMetaType::QString, "defopentoonz");
  // Viewer context menu
  define(guidedDrawingType, "guidedDrawingType", QMetaType::Int, 0);  // Off
  define(guidedAutoInbetween, "guidedAutoInbetween", QMetaType::Bool,
         false);  // Off
  define(guidedInterpolationType, "guidedInterpolationType", QMetaType::Int,
         1);  // Linear
#if defined(MACOSX) && defined(__LP64__)
  // OSX shared memory settings
  define(shmmax, "shmmax", QMetaType::Int, -1);
  define(shmseg, "shmseg", QMetaType::Int, -1);
  define(shmall, "shmall", QMetaType::Int, -1);
  define(shmmni, "shmmni", QMetaType::Int, -1);
#endif
}

//-----------------------------------------------------------------

void Preferences::define(PreferencesItemId id, QString idString,
                         QMetaType::Type type, QVariant defaultValue,
                         QVariant min, QVariant max) {
  // load value
  QVariant value(defaultValue);
  switch (type) {
  case QMetaType::Bool:
  case QMetaType::Int:
  case QMetaType::Double:
  case QMetaType::QString:
    if (m_settings->contains(idString) &&
        m_settings->value(idString).canConvert(type))
      value = m_settings->value(idString);
    break;
  case QMetaType::QSize:  // used in iconSize
    if (m_settings->contains(idString) &&
        m_settings->value(idString).canConvert(QMetaType::QSize))
      value = m_settings->value(idString);
    // to keep compatibility with older versions
    else if (m_settings->contains(idString + "X")) {
      QSize size = value.toSize();
      size.setWidth(m_settings->value(idString + "X", size.width()).toInt());
      size.setHeight(m_settings->value(idString + "Y", size.height()).toInt());
      value.setValue(size);
    }
    break;
  case QMetaType::QMetaType::QColor:
    if (m_settings->contains(idString)) {
      QString str = m_settings->value(idString).toString();
      value.setValue(stringToColor(str));
    }
    // following two conditions are to keep compatibility with older versions
    else if (m_settings->contains(idString + "_R")) {
      QColor color = value.value<QColor>();
      color.setRed(m_settings->value(idString + "_R", color.red()).toInt());
      color.setGreen(m_settings->value(idString + "_G", color.green()).toInt());
      color.setBlue(m_settings->value(idString + "_B", color.blue()).toInt());
      color.setAlpha(m_settings->value(idString + "_M", color.alpha()).toInt());
      value.setValue(color);
    } else if (m_settings->contains(idString + ".r")) {
      QColor color = value.value<QColor>();
      color.setRed(m_settings->value(idString + ".r", color.red()).toInt());
      color.setGreen(m_settings->value(idString + ".g", color.green()).toInt());
      color.setBlue(m_settings->value(idString + ".b", color.blue()).toInt());
      color.setAlpha(255);
      value.setValue(color);
    }
    break;
  case QMetaType::QVariantMap:  // used in colorCalibrationLutPaths
    if (m_settings->contains(idString) &&
        m_settings->value(idString).canConvert(type)) {
      QMap<QString, QString> pathMap;
      QAssociativeIterable iterable =
          m_settings->value(idString).value<QAssociativeIterable>();
      QAssociativeIterable::const_iterator it        = iterable.begin();
      const QAssociativeIterable::const_iterator end = iterable.end();
      for (; it != end; ++it)
        pathMap.insert(it.key().toString(), it.value().toString());
      value.setValue(pathMap);
    }
    break;
  default:
    std::cout << "Unsupported type detected" << std::endl;
    // load anyway
    value = m_settings->value(idString, value);
    break;
  }

  m_items.insert(id, PreferencesItem(idString, type, value, min, max));
}

//-----------------------------------------------------------------

void Preferences::setCallBack(const PreferencesItemId id, OnEditedFunc func) {
  getItem(id).onEditedFunc = func;
}

//-----------------------------------------------------------------

void Preferences::resolveCompatibility() {
  // autocreation type is divided into "EnableAutocreation" and
  // "NumberingSystem"
  if (m_settings->contains("AutocreationType") &&
      !m_settings->contains("EnableAutocreation")) {
    int type = m_settings->value("AutocreationType").toInt();
    switch (type) {
    case 0:  // former "Disabled"
      setValue(EnableAutocreation, false);
      break;
    case 1:  // former "Enabled"
      setValue(EnableAutocreation, true);
      setValue(NumberingSystem, 0);  // set numbering system to "Incremental"
      break;
    case 2:  // former "Use Xsheet as Animation Sheet"
      setValue(EnableAutocreation, true);
      setValue(NumberingSystem, 1);
      break;
    }
  }
  // "levelNameOnEachMarkerEnabled" is changed to "levelNameDisplayType", adding
  // a new option
  if (m_settings->contains("levelNameOnEachMarkerEnabled") &&
      !m_settings->contains("levelNameDisplayType")) {
    if (m_settings->value("levelNameOnEachMarkerEnabled")
            .toBool())  // show level name on each marker
      setValue(levelNameDisplayType, ShowLevelNameOnEachMarker);
    else  // Default (level name on top of each cell block)
      setValue(levelNameDisplayType, ShowLevelName_Default);
  }
  // "scanLevelType" is changed to "DefRasterFormat", enabling to specify
  // default format for both the Scan and the Raster levels.
  if (m_settings->contains("scanLevelType") &&
      !m_settings->contains("DefRasterFormat")) {
    setValue(DefRasterFormat, m_settings->value("scanLevelType").toString());
  }
}

//-----------------------------------------------------------------

PreferencesItem &Preferences::getItem(const PreferencesItemId id) {
  assert(m_items.contains(id));
  return m_items[id];
}

//-----------------------------------------------------------------

bool Preferences::getBoolValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return false;
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::Bool);
  if (item.type != QMetaType::Bool) return false;

  return item.value.toBool();
}

//-----------------------------------------------------------------

int Preferences::getIntValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return -1;
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::Int);
  if (item.type != QMetaType::Int) return -1;

  return item.value.toInt();
}

//-----------------------------------------------------------------

double Preferences::getDoubleValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return -1.0;
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::Double);
  if (item.type != QMetaType::Double) return -1.0;

  return item.value.toDouble();
}

//-----------------------------------------------------------------

QString Preferences::getStringValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return QString();
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::QString);
  if (item.type != QMetaType::QString) return QString();

  return item.value.toString();
}

//-----------------------------------------------------------------

TPixel Preferences::getColorValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return TPixel();
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::QColor);
  if (item.type != QMetaType::QColor) return TPixel();

  return colorToTPixel(item.value.value<QColor>());
}

//-----------------------------------------------------------------

TDimension Preferences::getSizeValue(const PreferencesItemId id) const {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return TDimension();
  PreferencesItem item = m_items.value(id);
  assert(item.type == QMetaType::QSize);
  if (item.type != QMetaType::QSize) return TDimension();
  QSize size = item.value.toSize();
  return TDimension(size.width(), size.height());
}

//-----------------------------------------------------------------
// saveToFile is true by default, becomes false when dragging color field
void Preferences::setValue(const PreferencesItemId id, QVariant value,
                           bool saveToFile) {
  assert(m_items.contains(id));
  if (!m_items.contains(id)) return;
  m_items[id].value = value;
  if (saveToFile) {
    if (m_items[id].type ==
        QMetaType::QColor)  // write in human-readable format
      m_settings->setValue(m_items[id].idString,
                           colorToString(value.value<QColor>()));
    else if (m_items[id].type ==
             QMetaType::Bool)  // write 1/0 instead of true/false to keep
                               // compatibility
      m_settings->setValue(m_items[id].idString, value.toBool() ? "1" : "0");
    else
      m_settings->setValue(m_items[id].idString, value);
  }

  // execute callback
  if (m_items[id].onEditedFunc) (this->*(m_items[id].onEditedFunc))();
}

//-----------------------------------------------------------------

void Preferences::enableAutosave() {
  bool autoSaveOn = getBoolValue(autosaveEnabled);
  if (autoSaveOn)
    emit startAutoSave();
  else
    emit stopAutoSave();
}

//-----------------------------------------------------------------

void Preferences::setAutosavePeriod() {
  emit stopAutoSave();
  emit startAutoSave();
  emit autoSavePeriodChanged();
}

//-----------------------------------------------------------------

void Preferences::setUndoMemorySize() {
  int memorySize = getIntValue(undoMemorySize);
  TUndoManager::manager()->setUndoMemorySize(memorySize);
}

//-----------------------------------------------------------------

void Preferences::setPixelsOnly() {
  bool pixelSelected = getBoolValue(pixelsOnly);
  if (pixelSelected)
    storeOldUnits();
  else
    resetOldUnits();
}

//-----------------------------------------------------------------

void Preferences::setUnits() {
  std::string units = getStringValue(linearUnits).toStdString();
  setCurrentUnits("length", units);
  setCurrentUnits("length.x", units);
  setCurrentUnits("length.y", units);
  setCurrentUnits("length.lx", units);
  setCurrentUnits("length.ly", units);
  setCurrentUnits("fxLength", units);
  setCurrentUnits("pippo", units);
}

//-----------------------------------------------------------------

void Preferences::setCameraUnits() {
  std::string units = getStringValue(cameraUnits).toStdString();
  setCurrentUnits("camera.lx", units);
  setCurrentUnits("camera.ly", units);
}

//-----------------------------------------------------------------

void Preferences::setRasterBackgroundColor() {
  TPixel color = getColorValue(rasterBackgroundColor);
  TImageWriter::setBackgroundColor(color);
}

//-----------------------------------------------------------------

void Preferences::storeOldUnits() {
  setValue(oldUnits, getStringValue(linearUnits));
  setValue(oldCameraUnits, getStringValue(cameraUnits));
}

//-----------------------------------------------------------------

void Preferences::resetOldUnits() {
  QString oldLinearU = getStringValue(oldUnits);
  QString oldCameraU = getStringValue(oldCameraUnits);
  if (oldLinearU != "" && oldCameraU != "") {
    setValue(linearUnits, oldLinearU);
    setValue(cameraUnits, oldCameraU);
  }
}

//-----------------------------------------------------------------

QString Preferences::getCurrentLanguage() const {
  QString lang = getStringValue(CurrentLanguageName);
  if (m_languageList.contains(lang)) return lang;
  // If no valid option selected, then return English
  return m_languageList[0];
}

//-----------------------------------------------------------------

QString Preferences::getCurrentStyleSheet() const {
  QString currentStyleSheetName = getStringValue(CurrentStyleSheetName);
  if (currentStyleSheetName.isEmpty()) return QString();
  TFilePath path(TEnv::getConfigDir() + "qss");
  QString string = currentStyleSheetName + QString("/") +
                   currentStyleSheetName + QString(".qss");
  QString styleSheetPath = path.getQString() + "/" + string;

  QString additionalSheetStr = getStringValue(additionalStyleSheet);
  // if there is no additional style sheet, return the path and let
  // Qt to load and parse it
  if (additionalSheetStr.isEmpty()) return QString("file:///" + styleSheetPath);

  // if there is any additional style sheet, load the style sheet
  // from the file and combine with it
  QString styleSheetStr;
  QFile f(styleSheetPath);
  if (f.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream ts(&f);
    styleSheetStr = ts.readAll();
  }
  styleSheetStr += additionalSheetStr;

  // here we will convert all relative paths to absolute paths
  // or Qt will look for images relative to the current working directory
  // since it has no idea where the style sheet comes from.

  QString currentStyleFolderPath =
      path.getQString().replace("\\", "/") + "/" + currentStyleSheetName;

  styleSheetStr.replace(QRegExp("url\\(['\"]([^'\"]+)['\"]\\)"),
                        "url(\"" + currentStyleFolderPath + QString("/\\1\")"));

  return styleSheetStr;
}

//-----------------------------------------------------------------

void Preferences::setPrecompute(bool enabled) { m_precompute = enabled; }

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
  _setValue(*m_settings, m_levelFormats);

  return formatIdx;
}

//-----------------------------------------------------------------

void Preferences::removeLevelFormat(int formatIdx) {
  assert(0 <= formatIdx && formatIdx < int(m_levelFormats.size()));
  m_levelFormats.erase(m_levelFormats.begin() + formatIdx);

  _setValue(*m_settings, m_levelFormats);
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

void Preferences::setColorCalibrationLutPath(QString monitorName,
                                             QString path) {
  PreferencesItem item = m_items.value(colorCalibrationLutPaths);
  QMap<QString, QVariant> lutPathMap =
      item.value.value<QMap<QString, QVariant>>();
  lutPathMap.insert(monitorName, path);
  setValue(colorCalibrationLutPaths, lutPathMap);
}

//-----------------------------------------------------------------

QString Preferences::getColorCalibrationLutPath(QString &monitorName) const {
  PreferencesItem item = m_items.value(colorCalibrationLutPaths);
  QMap<QString, QVariant> lutPathMap =
      item.value.value<QMap<QString, QVariant>>();

  return lutPathMap.value(monitorName).toString();
}
