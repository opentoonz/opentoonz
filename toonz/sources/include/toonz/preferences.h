#pragma once

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <memory>

// TnzCore includes
#include "tcommon.h"
#include "tgeometry.h"
#include "tpixel.h"

// TnzLib includes
#include "toonz/levelproperties.h"

// Qt includes
#include <QString>
#include <QObject>
#include <QMap>
#include <QRegExp>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================================================

//    Forward declarations

class TFilePath;
class QSettings;

//==============================================================

//**********************************************************************************
//    Preferences  declaration
//**********************************************************************************

/*!
  \brief    Stores application-wide preferences used throughout Toonz.
*/

class DVAPI Preferences final : public QObject  // singleton
{
  Q_OBJECT

public:
  struct LevelFormat {
    QString m_name;  //!< Name displayed for the format.
    QRegExp
        m_pathFormat;  //!< <TT>[default: ".*"]</TT>Used to recognize levels in
                       //!  the format. It's case <I>in</I>sensitive.
    LevelOptions m_options;  //!< Options associated to levels in the format.
    int m_priority;  //!< <TT>[default: 1]</TT> Priority value for the format.
    //!  Higher priority means that the format is matched first.
  public:
    LevelFormat(const QString &name = QString())
        : m_name(name)
        , m_pathFormat(".*", Qt::CaseInsensitive)
        , m_priority(1) {}

    bool matches(const TFilePath &fp) const;
  };

  enum ColumnIconLoadingPolicy {
    LoadAtOnce,  /**< it tells to load and display column icon
                            \b at \b once when the scene is opened;      */
    LoadOnDemand /**< it tells to load display icon \b on
                            \b demand (generally by clicking on the column
                    header)     */
  };

  enum SnappingTarge { SnapStrokes, SnapGuides, SnapAll };

  enum PathAliasPriority {
    ProjectFolderAliases = 0,
    SceneFolderAlias,
    ProjectFolderOnly
  };

  enum FunctionEditorToggle {
    ShowGraphEditorInPopup = 0,
    ShowFunctionSpreadsheetInPopup,
    ToggleBetweenGraphAndSpreadsheet
  };

public:
  static Preferences *instance();

  // General settings  tab

  void setUndoMemorySize(int memorySize);
  int getUndoMemorySize() const { return m_undoMemorySize; }

  void setDefaultTaskChunkSize(int chunkSize);
  int getDefaultTaskChunkSize() const { return m_chunkSize; }

  void enableDefaultViewer(bool on);
  bool isDefaultViewerEnabled() const { return m_defaultViewerEnabled; }

  void enableRasterOptimizedMemory(bool on);
  bool isRasterOptimizedMemory() const { return m_rasterOptimizedMemory; }

  void enableAutosave(bool on);
  bool isAutosaveEnabled() const { return m_autosaveEnabled; }

  void setAutosavePeriod(int minutes);
  int getAutosavePeriod() const { return m_autosavePeriod; }  // minutes

  void enableAutosaveScene(bool on);
  bool isAutosaveSceneEnabled() const { return m_autosaveSceneEnabled; }

  void enableAutosaveOtherFiles(bool on);
  bool isAutosaveOtherFilesEnabled() const {
    return m_autosaveOtherFilesEnabled;
  }

  void enableLevelsBackup(bool enabled);
  bool isLevelsBackupEnabled() const { return m_levelsBackupEnabled; }

  void enableSceneNumbering(bool enabled);
  bool isSceneNumberingEnabled() const { return m_sceneNumberingEnabled; }

  void enableReplaceAfterSaveLevelAs(bool on);
  bool isReplaceAfterSaveLevelAsEnabled() const {
    return m_replaceAfterSaveLevelAs;
  }

  void enableStartupPopup(bool on);
  bool isStartupPopupEnabled() { return m_startupPopupEnabled; }

  void setProjectRoot(int index);
  int getProjectRoot() { return m_projectRoot; }

  void setCustomProjectRoot(std::wstring path);
  QString getCustomProjectRoot() { return m_customProjectRoot; }

  void enableWatchFileSystem(bool on);
  bool isWatchFileSystemEnabled() { return m_watchFileSystem; }

  void setPathAliasPriority(PathAliasPriority priority);
  PathAliasPriority getPathAliasPriority() const { return m_pathAliasPriority; }

  // Interface  tab

  void setCurrentLanguage(const QString &currentLanguage);
  QString getCurrentLanguage() const;
  QString getLanguage(int index) const;
  int getLanguageCount() const;

  void setCurrentStyleSheet(const QString &currentStyleSheet);
  QString getCurrentStyleSheetName() const;
  QString getCurrentStyleSheetPath() const;
  QString getStyleSheet(int index) const;
  int getStyleSheetCount() const;

  void setPixelsOnly(bool state);
  bool getPixelsOnly() const { return m_pixelsOnly; }

  void storeOldUnits();
  void resetOldUnits();
  QString getOldUnits() const { return m_oldUnits; }
  QString getOldCameraUnits() const { return m_oldCameraUnits; }

  void setUnits(std::string s);
  QString getUnits() const { return m_units; }

  void setCameraUnits(std::string s);
  QString getCameraUnits() const { return m_cameraUnits; }

  void setCurrentRoomChoice(int currentRoomChoice);
  void setCurrentRoomChoice(QString currentRoomChoice);
  QString getCurrentRoomChoice() const;
  int getRoomChoiceCount() const;
  QString getRoomChoice(int index) const;

  void enableGeneratedMovieView(bool on);
  bool isGeneratedMovieViewEnabled() const {
    return m_generatedMovieViewEnabled;
  }

  void setViewValues(int shrink, int step);
  void getViewValues(int &shrink, int &step) const {
    shrink = m_shrink, step = m_step;
  }

  void setIconSize(const TDimension &dim);
  TDimension getIconSize() const { return m_iconSize; }

  void setViewerBGColor(const TPixel32 &color, bool isDragging);
  TPixel getViewerBgColor() const { return m_viewerBGColor; }

  void setPreviewBGColor(const TPixel32 &color, bool isDragging);
  TPixel getPreviewBgColor() const { return m_previewBGColor; }

  void setChessboardColor1(const TPixel32 &color, bool isDragging);
  void setChessboardColor2(const TPixel32 &color, bool isDragging);
  void getChessboardColors(TPixel32 &col1, TPixel32 &col2) const {
    col1 = m_chessboardColor1;
    col2 = m_chessboardColor2;
  }

  void enableShowRasterImagesDarkenBlendedInViewer(bool on);
  bool isShowRasterImagesDarkenBlendedInViewerEnabled() const {
    return m_showRasterImagesDarkenBlendedInViewer;
  }

  void enableActualPixelViewOnSceneEditingMode(bool on);
  bool isActualPixelViewOnSceneEditingModeEnabled() const {
    return m_actualPixelViewOnSceneEditingMode;
  }

  void setViewerZoomCenter(int type);
  int getViewerZoomCenter() const { return m_viewerZoomCenter; }

  void enableShowFrameNumberWithLetters(bool on);
  bool isShowFrameNumberWithLettersEnabled() const {
    return m_showFrameNumberWithLetters;
  }
  void enableLevelNameOnEachMarker(bool on);
  bool isLevelNameOnEachMarkerEnabled() const {
    return m_levelNameOnEachMarker;
  }
  void setColumnIconLoadingPolicy(ColumnIconLoadingPolicy cilp);
  ColumnIconLoadingPolicy getColumnIconLoadingPolicy() const {
    return (ColumnIconLoadingPolicy)m_columnIconLoadingPolicy;
  }
  void enableMoveCurrent(bool on);
  bool isMoveCurrentEnabled() const {
    return m_moveCurrentFrameByClickCellArea;
  }

  void setInterfaceFont(std::string font);
  QString getInterfaceFont() { return m_interfaceFont; }
  void setInterfaceFontWeight(int weight);
  int getInterfaceFontWeight() { return m_interfaceFontWeight; }

  void setFunctionEditorToggle(FunctionEditorToggle status);
  FunctionEditorToggle getFunctionEditorToggle() {
    return m_functionEditorToggle;
  }

  // color calibration using 3DLUT
  void enableColorCalibration(bool on);
  bool isColorCalibrationEnabled() const { return m_colorCalibrationEnabled; }
  void setColorCalibrationLutPath(QString monitorName, QString path);
  QMap<QString, QString> &getColorCalibrationLutPathMap() {
    return m_colorCalibrationLutPaths;
  }
  QString getColorCalibrationLutPath(QString &monitorName) const;

  // Visualization  tab

  void setShow0ThickLines(bool on);
  bool getShow0ThickLines() const { return m_show0ThickLines; }

  void setRegionAntialias(bool on);
  bool getRegionAntialias() const { return m_regionAntialias; }

  // Loading  tab

  void enableAutoExpose(bool on);
  bool isAutoExposeEnabled() const { return m_autoExposeEnabled; }

  void enableSubsceneFolder(bool on);
  bool isSubsceneFolderEnabled() const { return m_subsceneFolderEnabled; }

  int addLevelFormat(const LevelFormat &format);  //!< Inserts a new level
                                                  //! format.  \return  The
  //! associated format index.
  void removeLevelFormat(int formatIdx);  //!< Removes a level format.

  const LevelFormat &levelFormat(
      int formatIdx) const;  //!< Retrieves a level format.
  int levelFormatsCount()
      const;  //!< Returns the number of stored level formats.

  /*! \return     Either the index of a matching format, or \p -1 if none
            was found.                                                        */

  int matchLevelFormat(const TFilePath &fp)
      const;  //!< Returns the \a nonnegative index of the first level format
              //!  matching the specified file path, <I>or \p -1 if none</I>.

  void setInitialLoadTlvCachingBehavior(int type);
  int getInitialLoadTlvCachingBehavior() const {
    return m_initialLoadTlvCachingBehavior;
  }

  void enableRemoveSceneNumberFromLoadedLevelName(bool on);
  bool isRemoveSceneNumberFromLoadedLevelNameEnabled() const {
    return m_removeSceneNumberFromLoadedLevelName;
  }

  void setDefaultImportPolicy(int policy);
  int getDefaultImportPolicy() { return m_importPolicy; }

  void setIgnoreImageDpi(bool on);
  bool isIgnoreImageDpiEnabled() const { return m_ignoreImageDpi; }

  // Drawing  tab

  void setScanLevelType(std::string s);
  QString getScanLevelType() const { return m_scanLevelType; }

  void setDefLevelType(int levelType);
  int getDefLevelType() const { return m_defLevelType; }

  void setDefLevelWidth(double width);
  double getDefLevelWidth() const { return m_defLevelWidth; }

  void setDefLevelHeight(double height);
  double getDefLevelHeight() const { return m_defLevelHeight; }

  void setDefLevelDpi(double dpi);
  double getDefLevelDpi() const { return m_defLevelDpi; }

  void setAutocreationType(int s);
  int getAutocreationType() const { return m_autocreationType; }

  bool isAutoCreateEnabled() const { return m_autocreationType > 0; }
  bool isAnimationSheetEnabled() const { return m_autocreationType == 2; }

  void enableAutoStretch(bool on);
  bool isAutoStretchEnabled() const { return m_enableAutoStretch; }

  void enableSaveUnpaintedInCleanup(bool on);
  bool isSaveUnpaintedInCleanupEnable() const {
    return m_saveUnpaintedInCleanup;
  }

  void enableMinimizeSaveboxAfterEditing(bool on);
  bool isMinimizeSaveboxAfterEditing() const {
    return m_minimizeSaveboxAfterEditing;
  }

  void setFillOnlySavebox(bool on);
  bool getFillOnlySavebox() const { return m_fillOnlySavebox; }

  void enableMultiLayerStylePicker(bool on);
  bool isMultiLayerStylePickerEnabled() const {
    return m_multiLayerStylePickerEnabled;
  }

  void enableUseNumpadForSwitchingStyles(bool on);
  bool isUseNumpadForSwitchingStylesEnabled() const {
    return m_useNumpadForSwitchingStyles;
  }

  void setGuidedDrawing(int status);
  int getGuidedDrawing() { return m_guidedDrawingType; }
  void setAnimatedGuidedDrawing(bool status);
  bool getAnimatedGuidedDrawing() const { return m_animatedGuidedDrawing; }

  void enableNewLevelSizeToCameraSize(bool on);
  bool isNewLevelSizeToCameraSizeEnabled() const {
    return m_newLevelSizeToCameraSizeEnabled;
  }

  void setVectorSnappingTarget(int target);
  int getVectorSnappingTarget() { return m_vectorSnappingTarget; }

  void setKeepFillOnVectorSimplify(bool on);
  bool getKeepFillOnVectorSimplify() { return m_keepFillOnVectorSimplify; }

  void setUseHigherDpiOnVectorSimplify(bool on);
  bool getUseHigherDpiOnVectorSimplify() {
    return m_useHigherDpiOnVectorSimplify;
  }

  void setDownArrowLevelStripNewFrame(bool on);
  bool getDownArrowLevelStripNewFrame() {
    return m_downArrowInLevelStripCreatesNewFrame;
  }

  // Tools Tab
  void setDropdownShortcutsCycleOptions(bool on);
  bool getDropdownShortcutsCycleOptions() {
    return m_dropdownShortcutsCycleOptions;
  }

  void setCursorBrushType(std::string brushType);
  QString getCursorBrushType() const { return m_cursorBrushType; }

  void setCursorBrushStyle(std::string brushStyle);
  QString getCursorBrushStyle() const { return m_cursorBrushStyle; }

  void enableCursorOutline(bool on);
  bool isCursorOutlineEnabled() const { return m_cursorOutlineEnabled; }

  // Xsheet  tab
  void setXsheetStep(int step);  //!< Sets the step used for the <I>next/prev
                                 //! step</I> commands.
  int getXsheetStep() const {
    return m_xsheetStep;
  }  //!< Returns the step used for the <I>next/prev step</I> commands.

  void enableXsheetAutopan(
      bool on);  //!< Enables/disables xsheet panning during playback.
  bool isXsheetAutopanEnabled() const {
    return m_xsheetAutopanEnabled;
  }  //!< Returns whether xsheet pans during playback.

  void enableIgnoreAlphaonColumn1(
      bool on);  //!< Enables/disables xsheet panning during playback.
  bool isIgnoreAlphaonColumn1Enabled() const {
    return m_ignoreAlphaonColumn1Enabled;
  }  //!< Returns whether xsheet pans during playback.

  void setDragCellsBehaviour(int dragCellsBehaviour);
  int getDragCellsBehaviour() const { return m_dragCellsBehaviour; }

  void enableShowKeyframesOnXsheetCellArea(bool on);
  bool isShowKeyframesOnXsheetCellAreaEnabled() const {
    return m_showKeyframesOnXsheetCellArea;
  }

  void enableUseArrowKeyToShiftCellSelection(bool on);
  bool isUseArrowKeyToShiftCellSelectionEnabled() const {
    return m_useArrowKeyToShiftCellSelection;
  }

  void enableInputCellsWithoutDoubleClicking(bool on);
  bool isInputCellsWithoutDoubleClickingEnabled() const {
    return m_inputCellsWithoutDoubleClickingEnabled;
  }

  void enableShowXSheetToolbar(bool on);
  bool isShowXSheetToolbarEnabled() const { return m_showXSheetToolbar; }

  void enableExpandFunctionHeader(bool on);
  bool isExpandFunctionHeaderEnabled() const { return m_expandFunctionHeader; }

  void enableShowColumnNumbers(bool on);
  bool isShowColumnNumbersEnabled() const { return m_showColumnNumbers; }

  void enableSyncLevelRenumberWithXsheet(bool on);
  bool isSyncLevelRenumberWithXsheetEnabled() const {
    return m_syncLevelRenumberWithXsheet;
  }

  void enableShortcutCommandsWhileRenamingCell(bool on);
  bool isShortcutCommandsWhileRenamingCellEnabled() const {
    return m_shortcutCommandsWhileRenamingCellEnabled;
  }

  void setXsheetLayoutPreference(std::string layout);
  QString getXsheetLayoutPreference() const { return m_xsheetLayoutPreference; }

  void setLoadedXsheetLayout(std::string layout);
  QString getLoadedXsheetLayout() const { return m_loadedXsheetLayout; }

  void setCurrentColumnData(const TPixel &currentColumnColor);
  void getCurrentColumnData(TPixel &currentColumnColor) const {
    currentColumnColor = m_currentColumnColor;
  }

  // Animation  tab

  void setKeyframeType(int s);
  int getKeyframeType() const { return m_keyframeType; }

  void setAnimationStep(int s);
  int getAnimationStep() const { return m_animationStep; }

  // Preview  tab

  void setBlankValues(int blanksCount, TPixel32 blankColor);
  void getBlankValues(int &blanksCount, TPixel32 &blankColor) const {
    blanksCount = m_blanksCount, blankColor = m_blankColor;
  }

  void enablePreviewAlwaysOpenNewFlip(bool on);
  bool previewAlwaysOpenNewFlipEnabled() const {
    return m_previewAlwaysOpenNewFlipEnabled;
  }

  void enableRewindAfterPlayback(bool on);
  bool rewindAfterPlaybackEnabled() const {
    return m_rewindAfterPlaybackEnabled;
  }

  void enableFitToFlipbook(bool on);
  bool fitToFlipbookEnabled() const { return m_fitToFlipbookEnabled; }

  // Onion Skin  tab

  void enableOnionSkin(bool on);
  bool isOnionSkinEnabled() const { return m_onionSkinEnabled; }
  void setOnionPaperThickness(int thickness);
  int getOnionPaperThickness() const { return m_onionPaperThickness; }

  void setOnionData(const TPixel &frontOnionColor, const TPixel &backOnionColor,
                    bool inksOnly);
  void getOnionData(TPixel &frontOnionColor, TPixel &backOnionColor,
                    bool &inksOnly) const {
    frontOnionColor = m_frontOnionColor, backOnionColor = m_backOnionColor,
    inksOnly = m_inksOnly;
  }
  bool getOnionSkinDuringPlayback() { return m_onionSkinDuringPlayback; }
  void setOnionSkinDuringPlayback(bool on);
  // Transparency Check  tab

  void setTranspCheckData(const TPixel &bg, const TPixel &ink,
                          const TPixel &paint);
  void getTranspCheckData(TPixel &bg, TPixel &ink, TPixel &paint) const {
    bg    = m_transpCheckBg;
    ink   = m_transpCheckInk;
    paint = m_transpCheckPaint;
  }

  void enableCurrentTimelineIndicator(bool on);
  bool isCurrentTimelineIndicatorEnabled() const {
    return m_currentTimelineEnabled;
  }

  // Version Control  tab

  void enableSVN(bool on);
  bool isSVNEnabled() const { return m_SVNEnabled; }

  void enableAutomaticSVNFolderRefresh(bool on);
  bool isAutomaticSVNFolderRefreshEnabled() const {
    return m_automaticSVNFolderRefreshEnabled;
  }

  void enableLatestVersionCheck(bool on);
  bool isLatestVersionCheckEnabled() const {
    return m_latestVersionCheckEnabled;
  }

  // Import Export Tab

  void setFfmpegPath(std::string path);
  QString getFfmpegPath() const { return m_ffmpegPath; }
  void setPrecompute(bool enabled);
  bool getPrecompute() { return m_precompute; }
  void setFfmpegTimeout(int seconds);
  int getFfmpegTimeout() { return m_ffmpegTimeout; }
  void setFastRenderPath(std::string path);
  QString getFastRenderPath() const { return m_fastRenderPath; }
  // Uncategorized - internals

  void setAskForOverrideRender(bool on);
  bool askForOverrideRender() const { return m_askForOverrideRender; }

  void setLineTestFpsCapture(int lineTestFpsCapture);
  int getLineTestFpsCapture() const { return m_lineTestFpsCapture; }

  int getTextureSize() const { return m_textureSize; }
  bool useDrawPixel() { return m_textureSize == 0; }

  void setShortcutPreset(std::string preset);
  QString getShortcutPreset() { return m_shortcutPreset; }

  int getShmMax() const {
    return m_shmmax;
  }  //! \sa The \p sysctl unix command.
  int getShmSeg() const {
    return m_shmseg;
  }  //! \sa The \p sysctl unix command.
  int getShmAll() const {
    return m_shmall;
  }  //! \sa The \p sysctl unix command.
  int getShmMni() const {
    return m_shmmni;
  }  //! \sa The \p sysctl unix command.
  std::string getLayerNameEncoding() const { return m_layerNameEncoding; };

Q_SIGNALS:

  void stopAutoSave();
  void startAutoSave();
  void autoSavePeriodChanged();

private:
  std::unique_ptr<QSettings> m_settings;

  QStringList m_languageList, m_styleSheetList;
  QMap<int, QString> m_roomMaps;

  std::vector<LevelFormat> m_levelFormats;

  QString m_units, m_cameraUnits, m_scanLevelType, m_currentRoomChoice,
      m_oldUnits, m_oldCameraUnits, m_ffmpegPath, m_shortcutPreset,
      m_customProjectRoot, m_interfaceFont;
  QString m_fastRenderPath;

  double m_defLevelWidth, m_defLevelHeight, m_defLevelDpi;

  TDimension m_iconSize;

  TPixel32 m_blankColor, m_frontOnionColor, m_backOnionColor, m_transpCheckBg,
      m_transpCheckInk, m_transpCheckPaint;

  int m_autosavePeriod,  // minutes
      m_chunkSize, m_blanksCount, m_onionPaperThickness, m_step, m_shrink,
      m_textureSize, m_autocreationType, m_keyframeType, m_animationStep,
      m_ffmpegTimeout;  // seconds
  int m_projectRoot, m_importPolicy, m_interfaceFontWeight, m_guidedDrawingType;
  QString m_currentLanguage, m_currentStyleSheet;
  int m_undoMemorySize,  // in megabytes
      m_dragCellsBehaviour, m_lineTestFpsCapture, m_defLevelType, m_xsheetStep,
      m_shmmax, m_shmseg, m_shmall, m_shmmni, m_vectorSnappingTarget;

  bool m_autoExposeEnabled, m_autoCreateEnabled, m_subsceneFolderEnabled,
      m_generatedMovieViewEnabled, m_xsheetAutopanEnabled,
      m_ignoreAlphaonColumn1Enabled, m_previewAlwaysOpenNewFlipEnabled,
      m_rewindAfterPlaybackEnabled, m_fitToFlipbookEnabled, m_autosaveEnabled,
      m_autosaveSceneEnabled, m_autosaveOtherFilesEnabled,
      m_defaultViewerEnabled, m_pixelsOnly, m_showXSheetToolbar,
      m_expandFunctionHeader, m_showColumnNumbers, m_animatedGuidedDrawing;
  bool m_rasterOptimizedMemory, m_saveUnpaintedInCleanup,
      m_askForOverrideRender, m_automaticSVNFolderRefreshEnabled, m_SVNEnabled,
      m_levelsBackupEnabled, m_minimizeSaveboxAfterEditing,
      m_sceneNumberingEnabled, m_animationSheetEnabled, m_inksOnly,
      m_startupPopupEnabled;
  bool m_fillOnlySavebox, m_show0ThickLines, m_regionAntialias;
  bool m_onionSkinDuringPlayback, m_ignoreImageDpi,
      m_syncLevelRenumberWithXsheet;
  bool m_keepFillOnVectorSimplify, m_useHigherDpiOnVectorSimplify;
  bool m_downArrowInLevelStripCreatesNewFrame;
  TPixel32 m_viewerBGColor, m_previewBGColor, m_chessboardColor1,
      m_chessboardColor2;
  bool m_showRasterImagesDarkenBlendedInViewer,
      m_actualPixelViewOnSceneEditingMode;
  bool m_dropdownShortcutsCycleOptions;
  int m_viewerZoomCenter;  // MOUSE_CURSOR = 0, VIEWER_CENTER = 1
  // used in the load level popup. ON_DEMAND = 0, ALL_ICONS = 1,
  // ALL_ICONS_AND_IMAGES = 2
  int m_initialLoadTlvCachingBehavior;
  // automatically remove 6 letters of scene number from the level name
  // ("c0001_A.tlv" -> "A")
  bool m_removeSceneNumberFromLoadedLevelName;
  // after save level as command, replace the level with "save-as"ed level
  bool m_replaceAfterSaveLevelAs;
  // convert the last one digit of the frame number to alphabet
  // Ex.  12 -> 1B    21 -> 2A   30 -> 3
  bool m_showFrameNumberWithLetters;
  // display level name on each marker in the xsheet cell area
  bool m_levelNameOnEachMarker;
  // whether to load the column icon(thumbnail) at once / on demand
  int m_columnIconLoadingPolicy;
  bool m_moveCurrentFrameByClickCellArea;
  bool m_onionSkinEnabled;
  bool m_multiLayerStylePickerEnabled;
  bool m_precompute;

  bool m_showKeyframesOnXsheetCellArea;
  std::string m_layerNameEncoding = "SJIS";  // Fixed to SJIS for now. You can
                                             // add interface if you wanna
                                             // change encoding.
  // whether to use numpad and tab key shortcut for selecting styles
  bool m_useNumpadForSwitchingStyles;

  // whether to set the new level size to be the same as the camera size by
  // default
  bool m_newLevelSizeToCameraSizeEnabled;

  // use arrow key to shift cel selection, ctrl + arrow key to resize the
  // selection range.
  bool m_useArrowKeyToShiftCellSelection;

  // enable to input drawing numbers into cells without double-clicking
  bool m_inputCellsWithoutDoubleClickingEnabled;

  // enable to watch file system in order to update file browser automatically
  bool m_watchFileSystem;

  // enable OT command shortcut keys while renaming xsheet cell
  bool m_shortcutCommandsWhileRenamingCellEnabled;

  QString m_xsheetLayoutPreference,
      m_loadedXsheetLayout;  // Classic, Classic-revised, compact

  // defines which alias to be used if both are possible on coding file path
  PathAliasPriority m_pathAliasPriority;

  // defines behavior of toggle switch in function editor
  FunctionEditorToggle m_functionEditorToggle;

  bool m_currentTimelineEnabled;
  bool m_enableAutoStretch;

  // color calibration using 3DLUT
  bool m_colorCalibrationEnabled = false;
  // map of [monitor name]-[path to the lut file].
  // for now non-Windows accepts only one lut path for all kinds of monitors
  QMap<QString, QString> m_colorCalibrationLutPaths;

  // release version check
  bool m_latestVersionCheckEnabled = true;

  // Cursor settings
  QString m_cursorBrushType;
  QString m_cursorBrushStyle;
  bool m_cursorOutlineEnabled = false;

  TPixel32 m_currentColumnColor;

private:
  Preferences();
  ~Preferences();
};

#endif  // PREFERENCES_H
