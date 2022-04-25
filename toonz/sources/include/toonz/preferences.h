#pragma once

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <memory>

// TnzCore includes
#include "tcommon.h"
#include "tgeometry.h"
#include "tpixel.h"
#include "tfilepath.h"

// TnzLib includes
#include "toonz/levelproperties.h"
#include "toonz/preferencesitemids.h"

// Qt includes
#include <QString>
#include <QObject>
#include <QMap>
#include <QRegExp>
#include <QVariant>
#include <QDataStream>

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

class QSettings;

//==============================================================

//**********************************************************************************
//    Preferences  declaration
//**********************************************************************************

class Preferences;
typedef void (Preferences::*OnEditedFunc)();
class DVAPI PreferencesItem final {
public:
  QString idString;
  QMetaType::Type type;
  QVariant value;
  QVariant min              = 0;
  QVariant max              = -1;
  OnEditedFunc onEditedFunc = nullptr;

  PreferencesItem(QString _idString, QMetaType::Type _type, QVariant _value,
                  QVariant _min = 0, QVariant _max = -1,
                  OnEditedFunc _onEditedFunc = nullptr)
      : idString(_idString)
      , type(_type)
      , value(_value)
      , min(_min)
      , max(_max)
      , onEditedFunc(_onEditedFunc) {}
  PreferencesItem() {}
};

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

  enum LevelNameDisplayType {
    ShowLevelName_Default = 0,
    ShowLevelNameOnEachMarker,
    ShowLevelNameOnColumnHeader
  };

  //--- callbacks
  // General
  void enableAutosave();
  void setAutosavePeriod();
  void setUndoMemorySize();
  // Interface
  void setPixelsOnly();
  void setUnits();
  void setCameraUnits();
  // Saving
  void setRasterBackgroundColor();

public:
  static Preferences *instance();

  QMap<PreferencesItemId, PreferencesItem> m_items;
  void initializeOptions();
  void definePreferenceItems();
  void define(PreferencesItemId id, QString idString, QMetaType::Type type,
              QVariant defaultValue, QVariant min = 0, QVariant max = -1);

  void setCallBack(const PreferencesItemId id, OnEditedFunc func);
  void resolveCompatibility();

  PreferencesItem &getItem(const PreferencesItemId id);
  bool getBoolValue(const PreferencesItemId id) const;
  int getIntValue(const PreferencesItemId id) const;
  double getDoubleValue(const PreferencesItemId id) const;
  QString getStringValue(const PreferencesItemId id) const;
  TPixel getColorValue(const PreferencesItemId id) const;
  TDimension getSizeValue(const PreferencesItemId id) const;

  void setValue(const PreferencesItemId id, QVariant value,
                bool saveToFile = true);

  // General settings  tab
  bool isDefaultViewerEnabled() const {
    return getBoolValue(defaultViewerEnabled);
  }
  bool isRasterOptimizedMemory() const {
    return getBoolValue(rasterOptimizedMemory);
  }
  bool isAutosaveEnabled() const { return getBoolValue(autosaveEnabled); }
  int getAutosavePeriod() const {
    return getIntValue(autosavePeriod);
  }  // minutes
  bool isAutosaveSceneEnabled() const {
    return getBoolValue(autosaveSceneEnabled);
  }
  bool isAutosaveOtherFilesEnabled() const {
    return getBoolValue(autosaveOtherFilesEnabled);
  }
  bool isStartupPopupEnabled() { return getBoolValue(startupPopupEnabled); }
  int getUndoMemorySize() const { return getIntValue(undoMemorySize); }
  int getDefaultTaskChunkSize() const { return getIntValue(taskchunksize); }
  bool isReplaceAfterSaveLevelAsEnabled() const {
    return getBoolValue(replaceAfterSaveLevelAs);
  }
  bool isBackupEnabled() const { return getBoolValue(backupEnabled); }
  int getBackupKeepCount() { return getIntValue(backupKeepCount); }
  bool isSceneNumberingEnabled() const {
    return getBoolValue(sceneNumberingEnabled);
  }
  bool isWatchFileSystemEnabled() {
    return getBoolValue(watchFileSystemEnabled);
  }
  int getProjectRoot() { return getIntValue(projectRoot); }
  QString getCustomProjectRoot() { return getStringValue(customProjectRoot); }
  PathAliasPriority getPathAliasPriority() const {
    return PathAliasPriority(getIntValue(pathAliasPriority));
  }

  // Interface  tab
  QStringList getStyleSheetList() const { return m_styleSheetList; }
  bool getIconTheme() const { return getBoolValue(iconTheme); }
  void storeOldUnits();  // OK
  void resetOldUnits();  // OK
  QStringList getLanguageList() const { return m_languageList; }
  QMap<int, QString> getRoomMap() const { return m_roomMaps; }

  QString getCurrentStyleSheet() const;
  QString getAdditionalStyleSheet() const {
    return getStringValue(additionalStyleSheet);
  }
  bool getPixelsOnly() const { return getBoolValue(pixelsOnly); }
  QString getOldUnits() const { return getStringValue(oldUnits); }
  QString getOldCameraUnits() const { return getStringValue(oldCameraUnits); }
  QString getUnits() const { return getStringValue(linearUnits); }
  QString getCameraUnits() const { return getStringValue(cameraUnits); }
  QString getCurrentRoomChoice() const {
    return getStringValue(CurrentRoomChoice);
  }
  FunctionEditorToggle getFunctionEditorToggle() {
    return FunctionEditorToggle(getIntValue(functionEditorToggle));
  }
  bool isMoveCurrentEnabled() const {
    return getBoolValue(moveCurrentFrameByClickCellArea);
  }
  bool isActualPixelViewOnSceneEditingModeEnabled() const {
    return getBoolValue(actualPixelViewOnSceneEditingMode);
  }
  bool isShowRasterImagesDarkenBlendedInViewerEnabled() const {
    return getBoolValue(showRasterImagesDarkenBlendedInViewer);
  }
  TDimension getIconSize() const { return getSizeValue(iconSize); }
  void getViewValues(int &shrink, int &step) const {
    shrink = getIntValue(viewShrink), step = getIntValue(viewStep);
  }
  int getViewerZoomCenter() const { return getIntValue(viewerZoomCenter); }
  QString getCurrentLanguage() const;
  QString getInterfaceFont() { return getStringValue(interfaceFont); }
  QString getInterfaceFontStyle() { return getStringValue(interfaceFontStyle); }
  bool isColorCalibrationEnabled() const {
    return getBoolValue(colorCalibrationEnabled);
  }
  void setColorCalibrationLutPath(QString monitorName, QString path);
  QString getColorCalibrationLutPath(QString &monitorName) const;
  bool is30bitDisplayEnabled() const { return getBoolValue(displayIn30bit); }
  // Visualization  tab
  bool getShow0ThickLines() const { return getBoolValue(show0ThickLines); }
  bool getRegionAntialias() const { return getBoolValue(regionAntialias); }

  // Loading  tab
  int getDefaultImportPolicy() { return getIntValue(importPolicy); }
  bool isAutoExposeEnabled() const { return getBoolValue(autoExposeEnabled); }
  bool isSubsceneFolderEnabled() const {
    return getBoolValue(subsceneFolderEnabled);
  }
  bool isRemoveSceneNumberFromLoadedLevelNameEnabled() const {
    return getBoolValue(removeSceneNumberFromLoadedLevelName);
  }
  bool isIgnoreImageDpiEnabled() const { return getBoolValue(IgnoreImageDpi); }
  int getInitialLoadTlvCachingBehavior() const {
    return getIntValue(initialLoadTlvCachingBehavior);
  }
  ColumnIconLoadingPolicy getColumnIconLoadingPolicy() const {
    return ColumnIconLoadingPolicy(getIntValue(columnIconLoadingPolicy));
  }

  int addLevelFormat(const LevelFormat &format);  //!< Inserts a new level
                                                  //! format.  \return  The
                                                  //! associated format index.
  void removeLevelFormat(int formatIdx);          //!< Removes a level format.
  const LevelFormat &levelFormat(
      int formatIdx) const;  //!< Retrieves a level format.
  int levelFormatsCount()
      const;  //!< Returns the number of stored level formats.
  /*! \return     Either the index of a matching format, or \p -1 if none
  was found.                                                        */
  int matchLevelFormat(const TFilePath &fp)
      const;  //!< Returns the \a nonnegative index of the first level format
              //!  matching the specified file path, <I>or \p -1 if none</I>.
  bool isAutoRemoveUnusedLevelsEnabled() const {
    return isAutoExposeEnabled() && getBoolValue(autoRemoveUnusedLevels);
  }

  // Saving tab
  TPixel getRasterBackgroundColor() const {
    return getColorValue(rasterBackgroundColor);
  }

  // Import Export Tab
  QString getFfmpegPath() const { return getStringValue(ffmpegPath); }
  int getFfmpegTimeout() { return getIntValue(ffmpegTimeout); }
  QString getFastRenderPath() const { return getStringValue(fastRenderPath); }
  bool getFfmpegMultiThread() const { return getBoolValue(ffmpegMultiThread); }
  QString getRhubarbPath() const { return getStringValue(rhubarbPath); }
  int getRhubarbTimeout() { return getIntValue(rhubarbTimeout); }

  // Drawing  tab
  QString getDefRasterFormat() const { return getStringValue(DefRasterFormat); }
  // QString getScanLevelType() const { return getStringValue(scanLevelType); }
  int getDefLevelType() const { return getIntValue(DefLevelType); }
  bool isNewLevelSizeToCameraSizeEnabled() const {
    return getBoolValue(newLevelSizeToCameraSizeEnabled);
  }
  double getDefLevelWidth() const { return getDoubleValue(DefLevelWidth); }
  double getDefLevelHeight() const { return getDoubleValue(DefLevelHeight); }
  double getDefLevelDpi() const { return getDoubleValue(DefLevelDpi); }
  bool isAutoCreateEnabled() const { return getBoolValue(EnableAutocreation); }
  int getNumberingSystem() const { return getIntValue(NumberingSystem); }
  bool isAutoStretchEnabled() const { return getBoolValue(EnableAutoStretch); }
  bool isCreationInHoldCellsEnabled() const {
    return getBoolValue(EnableCreationInHoldCells);
  }
  bool isAutorenumberEnabled() const {
    return getBoolValue(EnableAutoRenumber);
  }
  int getVectorSnappingTarget() { return getIntValue(vectorSnappingTarget); }
  bool isSaveUnpaintedInCleanupEnable() const {
    return getBoolValue(saveUnpaintedInCleanup);
  }
  bool isMinimizeSaveboxAfterEditing() const {
    return getBoolValue(minimizeSaveboxAfterEditing);
  }
  bool isUseNumpadForSwitchingStylesEnabled() const {
    return getBoolValue(useNumpadForSwitchingStyles);
  }
  bool getDownArrowLevelStripNewFrame() {
    return getBoolValue(downArrowInLevelStripCreatesNewFrame);
  }
  bool getKeepFillOnVectorSimplify() {
    return getBoolValue(keepFillOnVectorSimplify);
  }
  bool getUseHigherDpiOnVectorSimplify() {
    return getBoolValue(useHigherDpiOnVectorSimplify);
  }

  // Tools Tab
  // bool getDropdownShortcutsCycleOptions() {
  //  return getIntValue(dropdownShortcutsCycleOptions) == 1;
  //}
  bool getFillOnlySavebox() const { return getBoolValue(FillOnlysavebox); }
  bool isMultiLayerStylePickerEnabled() const {
    return getBoolValue(multiLayerStylePickerEnabled);
  }
  QString getCursorBrushType() const { return getStringValue(cursorBrushType); }
  QString getCursorBrushStyle() const {
    return getStringValue(cursorBrushStyle);
  }
  bool isCursorOutlineEnabled() const {
    return getBoolValue(cursorOutlineEnabled);
  }
  int getLevelBasedToolsDisplay() const {
    return getIntValue(levelBasedToolsDisplay);
  }
  bool useCtrlAltToResizeBrushEnabled() const {
    return getBoolValue(useCtrlAltToResizeBrush);
  }
  int getTempToolSwitchTimer() const {
    return getIntValue(tempToolSwitchTimer);
  }

  // Xsheet  tab
  QString getXsheetLayoutPreference() const {
    return getStringValue(xsheetLayoutPreference);
  }
  QString getLoadedXsheetLayout() const {
    return getStringValue(xsheetLayoutPreference);
  }
  int getXsheetStep() const {
    return getIntValue(xsheetStep);
  }  //!< Returns the step used for the <I>next/prev step</I> commands.
  bool isXsheetAutopanEnabled() const {
    return getBoolValue(xsheetAutopanEnabled);
  }  //!< Returns whether xsheet pans during playback.
  int getDragCellsBehaviour() const { return getIntValue(DragCellsBehaviour); }
  bool isIgnoreAlphaonColumn1Enabled() const {
    return getBoolValue(ignoreAlphaonColumn1Enabled);
  }
  bool isShowKeyframesOnXsheetCellAreaEnabled() const {
    return getBoolValue(showKeyframesOnXsheetCellArea);
  }
  bool isXsheetCameraColumnEnabled() const {
    return getBoolValue(showXsheetCameraColumn);
  }
  bool isUseArrowKeyToShiftCellSelectionEnabled() const {
    return getBoolValue(useArrowKeyToShiftCellSelection);
  }
  bool isInputCellsWithoutDoubleClickingEnabled() const {
    return getBoolValue(inputCellsWithoutDoubleClickingEnabled);
  }
  bool isShortcutCommandsWhileRenamingCellEnabled() const {
    return getBoolValue(shortcutCommandsWhileRenamingCellEnabled);
  }
  bool isShowXSheetToolbarEnabled() const {
    return getBoolValue(showXSheetToolbar);
  }
  bool isExpandFunctionHeaderEnabled() const {
    return getBoolValue(expandFunctionHeader);
  }
  bool isShowColumnNumbersEnabled() const {
    return getBoolValue(showColumnNumbers);
  }
  bool isParentColorsInXsheetColumnEnabled() const {
    return getBoolValue(parentColorsInXsheetColumn);
  }
  bool isSyncLevelRenumberWithXsheetEnabled() const {
    return getBoolValue(syncLevelRenumberWithXsheet);
  }
  bool isCurrentTimelineIndicatorEnabled() const {
    return getBoolValue(currentTimelineEnabled);
  }
  void getCurrentColumnData(TPixel &color) const {
    color = getColorValue(currentColumnColor);
  }

  LevelNameDisplayType getLevelNameDisplayType() const {
    return LevelNameDisplayType(getIntValue(levelNameDisplayType));
  }
  bool isLevelNameOnEachMarkerEnabled() const {
    return getLevelNameDisplayType() == ShowLevelNameOnEachMarker;
  }
  bool isShowFrameNumberWithLettersEnabled() const {
    return getBoolValue(showFrameNumberWithLetters);
  }

  // Animation  tab
  int getKeyframeType() const { return getIntValue(keyframeType); }
  int getAnimationStep() const { return getIntValue(animationStep); }
  bool isModifyExpressionOnMovingReferencesEnabled() const {
    return getBoolValue(modifyExpressionOnMovingReferences);
  }

  // Preview  tab
  void getBlankValues(int &bCount, TPixel32 &bColor) const {
    bCount = getIntValue(blanksCount), bColor = getColorValue(blankColor);
  }
  bool rewindAfterPlaybackEnabled() const {
    return getBoolValue(rewindAfterPlayback);
  }
  int getShortPlayFrameCount() const {
    return getIntValue(shortPlayFrameCount);
  }
  bool previewAlwaysOpenNewFlipEnabled() const {
    return getBoolValue(previewAlwaysOpenNewFlip);
  }
  bool fitToFlipbookEnabled() const { return getBoolValue(fitToFlipbook); }
  bool isGeneratedMovieViewEnabled() const {
    return getBoolValue(generatedMovieViewEnabled);
  }

  // Onion Skin  tab
  bool isOnionSkinEnabled() const { return getBoolValue(onionSkinEnabled); }
  int getOnionPaperThickness() const {
    return getIntValue(onionPaperThickness);
  }

  void getOnionData(TPixel &frontColor, TPixel &backColor,
                    bool &inksOnly) const {
    frontColor = getColorValue(frontOnionColor),
    backColor  = getColorValue(backOnionColor),
    inksOnly   = getBoolValue(onionInksOnly);
  }
  bool getOnionSkinDuringPlayback() {
    return getBoolValue(onionSkinDuringPlayback);
  }
  bool areOnionColorsUsedForShiftAndTraceGhosts() const {
    return getBoolValue(useOnionColorsForShiftAndTraceGhosts);
  }
  bool getAnimatedGuidedDrawing() const {
    return getIntValue(animatedGuidedDrawing) == 1;
  }

  // Colors  tab
  TPixel getViewerBgColor() const { return getColorValue(viewerBGColor); }
  TPixel getPreviewBgColor() const { return getColorValue(previewBGColor); }
  TPixel getLevelEditorBoxColor() const {
    return getColorValue(levelEditorBoxColor);
  }
  void getChessboardColors(TPixel32 &col1, TPixel32 &col2) const {
    col1 = getColorValue(chessboardColor1);
    col2 = getColorValue(chessboardColor2);
  }
  void getTranspCheckData(TPixel &bg, TPixel &ink, TPixel &paint) const {
    bg    = getColorValue(transpCheckInkOnBlack);
    ink   = getColorValue(transpCheckInkOnWhite);
    paint = getColorValue(transpCheckPaint);
  }

  // Version Control  tab
  bool isSVNEnabled() const { return getBoolValue(SVNEnabled); }
  bool isAutomaticSVNFolderRefreshEnabled() const {
    return getBoolValue(automaticSVNFolderRefreshEnabled);
  }
  bool isLatestVersionCheckEnabled() const {
    return getBoolValue(latestVersionCheckEnabled);
  }

  // Tablet tab
  bool isWinInkEnabled() const { return getBoolValue(winInkEnabled); }
  bool isQtNativeWinInkEnabled() const {
    return getBoolValue(useQtNativeWinInk);
  }

  // Others (not appeared in the popup)
  // Shortcut popup settings
  QString getShortcutPreset() { return getStringValue(shortcutPreset); }
  // Viewer context menu
  bool isGuidedDrawingEnabled() { return getIntValue(guidedDrawingType) > 0; }
  int getGuidedDrawingType() { return getIntValue(guidedDrawingType); }
  bool getGuidedAutoInbetween() { return getBoolValue(guidedAutoInbetween); }
  int getGuidedInterpolation() { return getIntValue(guidedInterpolationType); }
#if defined(MACOSX) && defined(__LP64__)
  int getShmMax() const {
    return getIntValue(shmmax);
  }  //! \sa The \p sysctl unix command.
  int getShmSeg() const {
    return getIntValue(shmseg);
  }  //! \sa The \p sysctl unix command.
  int getShmAll() const {
    return getIntValue(shmall);
  }  //! \sa The \p sysctl unix command.
  int getShmMni() const {
    return getIntValue(shmmni);
  }  //! \sa The \p sysctl unix command.
#endif

  void setPrecompute(bool enabled);
  bool getPrecompute() { return m_precompute; }
  bool isAnimationSheetEnabled() const {
    return getIntValue(NumberingSystem) == 1;
  }
  bool isXsheetCameraColumnVisible() const {
    return getBoolValue(showXsheetCameraColumn) &&
           getBoolValue(showKeyframesOnXsheetCellArea);
  }
  int getTextureSize() const { return m_textureSize; }
  bool useDrawPixel() { return m_textureSize == 0; }
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

  bool m_precompute = true;
  int m_textureSize = 0;

  std::string m_layerNameEncoding = "SJIS";  // Fixed to SJIS for now. You can

private:
  Preferences();
  ~Preferences();
};

#endif  // PREFERENCES_H
