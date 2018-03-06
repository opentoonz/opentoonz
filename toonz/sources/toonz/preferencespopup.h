#pragma once

#ifndef PREFERENCESPOPUP_H
#define PREFERENCESPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/filefield.h"

// TnzLib includes
#include "toonz/preferences.h"

// Qt includes
#include <QComboBox>

//==============================================================

//    Forward declarations

class QLineEdit;
class QPushButton;
class QLabel;
class QGroupBox;

//==============================================================

//**********************************************************************************
//    PreferencesPopup  definition
//**********************************************************************************

class PreferencesPopup final : public QDialog {
  Q_OBJECT

public:
  PreferencesPopup();

private:
  class FormatProperties;

private:
  Preferences *m_pref;

  FormatProperties *m_formatProperties;

  DVGui::ColorField *m_blankColor, *m_frontOnionColor, *m_backOnionColor,
      *m_transpCheckBgColor, *m_transpCheckInkColor, *m_transpCheckPaintColor,
      *m_viewerBgColorFld, *m_previewBgColorFld, *m_chessboardColor1Fld,
      *m_chessboardColor2Fld;

  QComboBox *m_keyframeType, *m_cellsDragBehaviour, *m_defScanLevelType,
      *m_defLevelType, *m_autocreationType, *m_levelFormatNames,
      *m_columnIconOm, *m_unitOm, *m_cameraUnitOm, *m_importPolicy,
      *m_vectorSnappingTargetCB, *m_dropdownShortcutsCycleOptionsCB,
      *m_interfaceFont, *m_interfaceFontWeight, *m_guidedDrawingStyle;

  DVGui::MeasuredDoubleLineEdit *m_defLevelWidth, *m_defLevelHeight;

  DVGui::DoubleLineEdit *m_defLevelDpi;

  QLabel *m_dpiLabel, *m_customProjectRootLabel, *m_projectRootDirections;

  DVGui::IntLineEdit *m_minuteFld, *m_chunkSizeFld, *m_iconSizeLx,
      *m_iconSizeLy, *m_viewShrink, *m_viewStep, *m_blanksCount,
      *m_onionPaperThickness, *m_animationStepField, *m_undoMemorySize,
      *m_xsheetStep, *m_ffmpegTimeout;

  QPushButton *m_addLevelFormat, *m_removeLevelFormat, *m_editLevelFormat;

  DVGui::CheckBox *m_inksOnly, *m_enableVersionControl, *m_levelsBackup,
      *m_onionSkinVisibility, *m_pixelsOnlyCB, *m_projectRootDocuments,
      *m_projectRootDesktop, *m_projectRootCustom, *m_projectRootStuff,
      *m_onionSkinDuringPlayback, *m_autoSaveSceneCB, *m_autoSaveOtherFilesCB,
      *m_useNumpadForSwitchingStyles, *m_expandFunctionHeader,
      *m_useHigherDpiOnVectorSimplifyCB, *m_keepFillOnVectorSimplifyCB,
      *m_newLevelToCameraSizeCB, *m_ignoreImageDpiCB,
      *m_syncLevelRenumberWithXsheet, *m_downArrowInLevelStripCreatesNewFrame;

  DVGui::FileField *m_customProjectRootFileField;

  DVGui::FileField *m_ffmpegPathFileFld, *m_fastRenderPathFileField,
      *m_lutPathFileField;

  QGroupBox *m_autoSaveGroup, *m_showXSheetToolbar, *m_colorCalibration;

private:
  // QWidget* create(const QString& lbl, bool def, const char* slot);
  void rebuildFormatsList();

private slots:

  void onPixelsOnlyChanged(int index);
  void onProjectRootChanged();
  void onCustomProjectRootChanged();
  void onPixelUnitExternallySelected(bool on);
  void onAutoSaveExternallyChanged();
  void onAutoSavePeriodExternallyChanged();
  void onUnitChanged(int index);
  void onCameraUnitChanged(int index);
  void onRoomChoiceChanged(int index);
  void onScanLevelTypeChanged(const QString &text);
  void onMinuteChanged();
  void onIconSizeChanged();
  void onViewValuesChanged();
  void onAutoExposeChanged(int index);
  void onSubsceneFolderChanged(int index);
  void onViewGeneratedMovieChanged(int index);
  void onXsheetStepChanged();
  void onXsheetAutopanChanged(int index);
  void onIgnoreAlphaonColumn1Changed(int index);
  void onRewindAfterPlayback(int index);
  void onPreviewAlwaysOpenNewFlip(int index);
  void onRasterOptimizedMemoryChanged(int index);
  void onSaveUnpaintedInCleanupChanged(int index);
  void onMinimizeSaveboxAfterEditing(int index);
  void onAutoSaveChanged(bool on);
  void onAutoSaveSceneChanged(int index);
  void onAutoSaveOtherFilesChanged(int index);
  void onStartupPopupChanged(int index);
  void onDefaultViewerChanged(int index);
  void onBlankCountChanged();
  void onBlankColorChanged(const TPixel32 &, bool isDragging);
  void onAskForOverrideRender(int index);
  void onKeyframeTypeChanged(int index);
  void onAutocreationTypeChanged(int index);
  void onAnimationStepChanged();
  void onOnionPaperThicknessChanged();
  void onTranspCheckDataChanged(const TPixel32 &, bool isDragging);
  void onOnionDataChanged(const TPixel32 &, bool isDragging);
  void onOnionDataChanged(int);
  void onLanguageTypeChanged(const QString &);
  void onStyleSheetTypeChanged(const QString &);
  void onUndoMemorySizeChanged();
  void onSVNEnabledChanged(int);
  void onAutomaticSVNRefreshChanged(int);
  void onDragCellsBehaviourChanged(int);
  void onLevelsBackupChanged(int);
  void onSceneNumberingChanged(int);
  void onChunkSizeChanged();
  void onDefLevelTypeChanged(int);
  void onDefLevelParameterChanged();
  void onGetFillOnlySavebox(int index);
  void onFitToFlipbook(int);
  void onDropdownShortcutsCycleOptionsChanged(int);
  void onDownArrowInLevelStripCreatesNewFrame(int);
  void onAddLevelFormat();
  void onRemoveLevelFormat();
  void onEditLevelFormat();
  void onLevelFormatEdited();
  void onIgnoreImageDpiChanged(int index);
  void onShow0ThickLinesChanged(int);
  void onKeepFillOnVectorSimplifyChanged(int);
  void onUseHigherDpiOnVectorSimplifyChanged(int);
  void onRegionAntialiasChanged(int);
  void onImportPolicyChanged(int);
  void onImportPolicyExternallyChanged(int policy);
  void onNewLevelToCameraSizeChanged(bool checked);
  void onVectorSnappingTargetChanged(int index);

#ifdef LINETEST
  void onLineTestFpsCapture(int);
#endif

  void onMoveCurrentFrameChanged(int index);
  void setViewerBgColor(const TPixel32 &, bool);
  void setPreviewBgColor(const TPixel32 &, bool);
  void setChessboardColor1(const TPixel32 &, bool);
  void setChessboardColor2(const TPixel32 &, bool);
  void onColumnIconChange(const QString &);
  void onReplaceAfterSaveLevelAsChanged(int index);
  void onOnionSkinVisibilityChanged(int);
  void onOnionSkinDuringPlaybackChanged(int);
  void onGuidedDrawingStyleChanged(int);
  void onActualPixelOnSceneModeChanged(int);
  void onMultiLayerStylePickerChanged(int);
  void onLevelNameOnEachMarkerChanged(int);
  void onInitialLoadTlvCachingBehaviorChanged(int index);
  void onViewerZoomCenterChanged(int index);
  void onRemoveSceneNumberFromLoadedLevelNameChanged(int index);
  void onShowRasterImageDarkenBlendedInViewerChanged(int index);
  void onShowFrameNumberWithLettersChanged(int index);
  void onPaletteTypeForRasterColorModelChanged(int index);
  void onShowKeyframesOnCellAreaChanged(int);
  void onFfmpegPathChanged();
  void onFfmpegTimeoutChanged();
  void onFastRenderPathChanged();
  void onUseNumpadForSwitchingStylesClicked(bool);
  void onShowXSheetToolbarClicked(bool);
  void onSyncLevelRenumberWithXsheetChanged(int);
  void onExpandFunctionHeaderClicked(bool);
  void onShowColumnNumbersChanged(int);
  void onUseArrowKeyToShiftCellSelectionClicked(int);
  void onInputCellsWithoutDoubleClickingClicked(int);
  void onShortcutCommandsWhileRenamingCellClicked(int);
  void onWatchFileSystemClicked(int);
  void onInterfaceFontChanged(int index);
  void onInterfaceFontWeightChanged(int index);
  void onXsheetLayoutChanged(const QString &text);
  void onPathAliasPriorityChanged(int index);
  void onShowCurrentTimelineChanged(int);
  void onColorCalibrationChanged(bool);
  void onLutPathChanged();
};

//**********************************************************************************
//    PreferencesPopup::FormatProperties  definition
//**********************************************************************************

class PreferencesPopup::FormatProperties final : public DVGui::Dialog {
  Q_OBJECT

public:
  FormatProperties(PreferencesPopup *parent);

  void setLevelFormat(const Preferences::LevelFormat &lf);
  Preferences::LevelFormat levelFormat() const;

private:
  QComboBox *m_dpiPolicy;

  DVGui::LineEdit *m_name, *m_regExp;

  DVGui::DoubleLineEdit *m_dpi;

  DVGui::IntLineEdit *m_priority, *m_subsampling, *m_antialias;

  DVGui::CheckBox *m_premultiply, *m_whiteTransp, *m_doAntialias;

private slots:

  void updateEnabledStatus();
};

#endif  // PREFERENCESPOPUP_H
