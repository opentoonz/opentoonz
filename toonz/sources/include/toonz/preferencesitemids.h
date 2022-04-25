#ifndef PREFERENCESITEMIDS_H
#define PREFERENCESITEMIDS_H

enum PreferencesItemId {
  // General
  defaultViewerEnabled,
  rasterOptimizedMemory,
  startupPopupEnabled,
  undoMemorySize,
  taskchunksize,
  sceneNumberingEnabled,
  watchFileSystemEnabled,
  projectRoot,
  customProjectRoot,
  pathAliasPriority,

  //----------
  // Interface
  CurrentStyleSheetName,
  additionalStyleSheet,
  iconTheme,
  pixelsOnly,
  oldUnits,
  oldCameraUnits,
  linearUnits,
  cameraUnits,
  CurrentRoomChoice,
  functionEditorToggle,
  moveCurrentFrameByClickCellArea,
  actualPixelViewOnSceneEditingMode,
  showRasterImagesDarkenBlendedInViewer,
  iconSize,
  viewShrink,
  viewStep,
  viewerZoomCenter,
  CurrentLanguageName,
  interfaceFont,
  interfaceFontStyle,
  colorCalibrationEnabled,
  colorCalibrationLutPaths,
  showIconsInMenu,
  displayIn30bit,

  //----------
  // Visualization
  show0ThickLines,
  regionAntialias,

  //----------
  // Loading
  importPolicy,
  autoExposeEnabled,
  subsceneFolderEnabled,
  removeSceneNumberFromLoadedLevelName,
  IgnoreImageDpi,
  initialLoadTlvCachingBehavior,
  columnIconLoadingPolicy,
  levelFormats,  // need to be handle separately
  autoRemoveUnusedLevels,

  //----------
  // Saving
  autosaveEnabled,
  autosavePeriod,
  autosaveSceneEnabled,
  autosaveOtherFilesEnabled,
  replaceAfterSaveLevelAs,
  backupEnabled,
  backupKeepCount,
  rasterBackgroundColor,
  resetUndoOnSavingLevel,

  //----------
  // Import / Export
  ffmpegPath,
  ffmpegTimeout,
  fastRenderPath,
  ffmpegMultiThread,
  rhubarbPath,
  rhubarbTimeout,

  //----------
  // Drawing
  DefRasterFormat,
  // scanLevelType,// deprecated
  DefLevelType,
  newLevelSizeToCameraSizeEnabled,
  DefLevelWidth,
  DefLevelHeight,
  DefLevelDpi,
  // AutocreationType,// deprecated
  EnableAutocreation,
  NumberingSystem,
  EnableAutoStretch,
  EnableCreationInHoldCells,
  EnableAutoRenumber,
  vectorSnappingTarget,
  saveUnpaintedInCleanup,
  minimizeSaveboxAfterEditing,
  useNumpadForSwitchingStyles,
  downArrowInLevelStripCreatesNewFrame,
  keepFillOnVectorSimplify,
  useHigherDpiOnVectorSimplify,

  //----------
  // Tools
  // dropdownShortcutsCycleOptions, // removed
  FillOnlysavebox,
  multiLayerStylePickerEnabled,
  cursorBrushType,
  cursorBrushStyle,
  cursorOutlineEnabled,
  levelBasedToolsDisplay,
  useCtrlAltToResizeBrush,
  tempToolSwitchTimer,

  //----------
  // Xsheet
  xsheetLayoutPreference,
  xsheetStep,
  xsheetAutopanEnabled,
  DragCellsBehaviour,
  ignoreAlphaonColumn1Enabled,
  showKeyframesOnXsheetCellArea,
  showXsheetCameraColumn,
  useArrowKeyToShiftCellSelection,
  inputCellsWithoutDoubleClickingEnabled,
  shortcutCommandsWhileRenamingCellEnabled,
  showXSheetToolbar,
  expandFunctionHeader,
  showColumnNumbers,
  parentColorsInXsheetColumn,
  highlightLineEverySecond,
  syncLevelRenumberWithXsheet,
  currentTimelineEnabled,
  currentColumnColor,
  levelNameDisplayType,
  showFrameNumberWithLetters,

  //----------
  // Animation
  keyframeType,
  animationStep,
  modifyExpressionOnMovingReferences,

  //----------
  // Preview
  blanksCount,
  blankColor,
  rewindAfterPlayback,
  previewAlwaysOpenNewFlip,
  fitToFlipbook,
  generatedMovieViewEnabled,
  shortPlayFrameCount,

  //----------
  // Onion Skin
  onionSkinEnabled,
  onionPaperThickness,
  backOnionColor,
  frontOnionColor,
  onionInksOnly,
  onionSkinDuringPlayback,
  useOnionColorsForShiftAndTraceGhosts,
  animatedGuidedDrawing,

  //----------
  // Colors
  viewerBGColor,
  previewBGColor,
  levelEditorBoxColor,
  chessboardColor1,
  chessboardColor2,
  transpCheckInkOnWhite,
  transpCheckInkOnBlack,
  transpCheckPaint,

  //----------
  // Version Control
  SVNEnabled,
  automaticSVNFolderRefreshEnabled,
  latestVersionCheckEnabled,

  //----------
  // Touch / Tablet Settings
  // TounchGestureControl // Touch Gesture is a checkable command and not in
  // preferences.ini
  winInkEnabled,
  // This option will be shown & available only when WITH_WINTAB is defined
  useQtNativeWinInk,

  //----------
  // Others (not appeared in the popup)
  // Shortcut popup settings
  shortcutPreset,
  // Viewer context menu
  guidedDrawingType,
  guidedAutoInbetween,
  guidedInterpolationType,
  // OSX shared memory settings
  shmmax,
  shmseg,
  shmall,
  shmmni,
  //- obsoleted / unused members
  // interfaceFontWeight,
  // autoCreateEnabled,
  // animationSheetEnabled,
  // askForOverrideRender,
  // textureSize, // set to 0
  // LineTestFpsCapture,
  // guidedDrawingType,

  PreferencesItemCount
};

#endif
