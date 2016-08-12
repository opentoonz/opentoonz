#pragma once

#ifndef MENUBAR_COMMANDS_IDS_DEFINED
#define MENUBAR_COMMANDS_IDS_DEFINED

// TnzQt includes
#include "toonzqt/selectioncommandids.h"

/*!
  \file     menubarcommandids.h
  \brief    Contains string identifiers for Tnz6 commands.
*/

#define MI_NewScene "MI_NewScene"
#define MI_LoadScene "MI_LoadScene"
#define MI_SaveScene "MI_SaveScene"
#define MI_SaveSceneAs "MI_SaveSceneAs"
#define MI_SaveAll "MI_SaveAll"
#define MI_RevertScene "MI_RevertScene"
#define MI_LoadSubSceneFile "MI_LoadSubSceneFile"

#define MI_OpenRecentScene "MI_OpenRecentScene"
#define MI_OpenRecentLevel "MI_OpenRecentLevel"
#define MI_ClearRecentScene "MI_ClearRecentScene"
#define MI_ClearRecentLevel "MI_ClearRecentLevel"
#define MI_PrintXsheet "MI_PrintXsheet"
#define MI_NewLevel "MI_NewLevel"
#define MI_LoadLevel "MI_LoadLevel"
#define MI_LoadFolder "MI_LoadFolder"
#define MI_SaveLevel "MI_SaveLevel"
#define MI_SaveLevelAs "MI_SaveLevelAs"
#define MI_ExportLevel "MI_ExportLevel"
#define MI_SavePaletteAs "MI_SavePaletteAs"
#define MI_OverwritePalette "MI_OverwritePalette"
#define MI_LoadColorModel "MI_LoadColorModel"
#define MI_ImportMagpieFile "MI_ImportMagpieFile"
#define MI_NewProject "MI_NewProject"
#define MI_ProjectSettings "MI_ProjectSettings"
#define MI_SaveDefaultSettings "MI_SaveDefaultSettings"
#define MI_OutputSettings "MI_OutputSettings"
#define MI_PreviewSettings "MI_PreviewSettings"
#define MI_Render "MI_Render"
#define MI_FastRender "MI_FastRender"
#define MI_Preview "MI_Preview"
#define MI_RegeneratePreview "MI_RegeneratePreview"
#define MI_RegenerateFramePr "MI_RegenerateFramePr"
#define MI_ClonePreview "MI_ClonePreview"
#define MI_FreezePreview "MI_FrezzePreview"
#define MI_SavePreviewedFrames "MI_SavePreviewedFrames"
//#define MI_SavePreview         "MI_SavePreview"
#define MI_Print "MI_Print"
#define MI_Preferences "MI_Preferences"
#define MI_SavePreset "MI_SavePreset"
#define MI_ShortcutPopup "MI_ShortcutPopup"
#define MI_Quit "MI_Quit"

#define MI_Undo "MI_Undo"
#define MI_Redo "MI_Redo"

#define MI_DefineScanner "MI_DefineScanner"
#define MI_ScanSettings "MI_ScanSettings"
#define MI_Scan "MI_Scan"
#define MI_Autocenter "MI_Autocenter"
#define MI_SetScanCropbox "MI_SetScanCropbox"
#define MI_ResetScanCropbox "MI_ResetScanCropbox"
#define MI_CleanupSettings "MI_CleanupSettings"
#define MI_CleanupPreview "MI_CleanupPreview"
#define MI_CameraTest "MI_CameraTest"
#define MI_OpacityCheck "MI_OpacityCheck"
#define MI_Cleanup "MI_Cleanup"

#define MI_AddFrames "MI_AddFrames"
#define MI_Renumber "MI_Renumber"
#define MI_FileInfo "MI_FileInfo"
#define MI_LevelSettings "MI_LevelSettings"
#define MI_CanvasSize "MI_CanvasSize"
#define MI_RemoveUnused "MI_RemoveUnused"

//#define MI_OpenCurrentScene  "MI_OpenCurrentScene"
#define MI_OpenFileBrowser "MI_OpenFileBrowser"
#define MI_OpenFileViewer "MI_OpenFileViewer"
#define MI_OpenFilmStrip "MI_OpenFilmStrip"
#define MI_OpenPalette "MI_OpenPalette"
#define MI_OpenPltGizmo "MI_OpenPltGizmo"
#define MI_OpenFileBrowser2 "MI_OpenFileBrowser2"
#define MI_OpenStyleControl "MI_OpenStyleControl"
#define MI_OpenToolbar "MI_OpenToolbar"
#define MI_OpenToolOptionBar "MI_OpenToolOptionBar"
#define MI_OpenLevelView "MI_OpenLevelView"
#ifdef LINETEST
#define MI_OpenExport "MI_OpenExport"
#define MI_OpenLineTestView "MI_OpenLineTestView"
#define MI_OpenLineTestCapture "MI_OpenLineTestCapture"
#define MI_Capture "MI_Capture"
#endif
#define MI_BrightnessAndContrast "MI_BrightnessAndContrast"
#define MI_Antialias "MI_Antialias"
#define MI_AdjustLevels "MI_AdjustLevels"
#define MI_AdjustThickness "MI_AdjustThickness"
#define MI_Binarize "MI_Binarize"
#define MI_LinesFade "MI_LinesFade"
#define MI_OpenXshView "MI_OpenXshView"
#define MI_OpenMessage "MI_OpenMessage"
#define MI_OpenTest "MI_OpenTest"
#define MI_OpenTasks "MI_OpenTasks"
#define MI_OpenBatchServers "MI_OpenBatchServers"
#define MI_OpenTMessage "MI_OpenTMessage"
#define MI_OpenColorModel "MI_OpenColorModel"
#define MI_OpenStudioPalette "MI_OpenStudioPalette"
#define MI_OpenSchematic "MI_OpenSchematic"

#define MI_Export "MI_Export"
#define MI_TestAnimation "MI_TestAnimation"

#define MI_SceneSettings "MI_SceneSettings"
#define MI_CameraSettings "MI_CameraSettings"
#define MI_CameraStage "MI_CameraStage"
#define MI_SaveSubxsheetAs "MI_SaveSubxsheetAs"
#define MI_Resequence "MI_Resequence"
#define MI_CloneChild "MI_CloneChild"
#define MI_ApplyMatchLines "MI_ApplyMatchLines"
#define MI_MergeCmapped "MI_MergeCmapped"
#define MI_MergeColumns "MI_MergeColumns"
#define MI_DeleteMatchLines "MI_DeleteMatchLines"
#define MI_DeleteInk "MI_DeleteInk"
#define MI_InsertSceneFrame "MI_InsertSceneFrame"
#define MI_RemoveSceneFrame "MI_RemoveSceneFrame"

#define MI_InsertGlobalKeyframe "MI_InsertGlobalKeyframe"
#define MI_RemoveGlobalKeyframe "MI_RemoveGlobalKeyframe"
#define MI_DrawingSubForward "MI_DrawingSubForward"
#define MI_DrawingSubBackward "MI_DrawingSubBackward"
#define MI_DrawingSubGroupForward "MI_DrawingSubGroupForward"
#define MI_DrawingSubGroupBackward "MI_DrawingSubGroupBackward"

#define MI_InsertFx "MI_InsertFx"
#define MI_NewOutputFx "MI_NewOutputFx"

#define MI_PasteNew "MI_PasteNew"
#define MI_Autorenumber "MI_Autorenumber"

#define MI_MergeFrames "MI_MergeFrames"
#define MI_Reverse "MI_Reverse"
#define MI_Swing "MI_Swing"
#define MI_Random "MI_Random"
#define MI_Increment "MI_Increment"
#define MI_Dup "MI_Dup"
#define MI_ResetStep "MI_ResetStep"
#define MI_IncreaseStep "MI_IncreaseStep"
#define MI_DecreaseStep "MI_DecreaseStep"
#define MI_Step2 "MI_Step2"
#define MI_Step3 "MI_Step3"
#define MI_Step4 "MI_Step4"
#define MI_Each2 "MI_Each2"
#define MI_Each3 "MI_Each3"
#define MI_Each4 "MI_Each4"
#define MI_Rollup "MI_Rollup"
#define MI_Rolldown "MI_Rolldown"
#define MI_TimeStretch "MI_TimeStretch"
#define MI_Duplicate "MI_Duplicate"
#define MI_CloneLevel "MI_CloneLevel"
#define MI_SetKeyframes "MI_SetKeyframes"

#define MI_ViewCamera "MI_ViewCamera"
#define MI_ViewBBox "MI_ViewBBox"
#define MI_ViewTable "MI_ViewTable"
#define MI_FieldGuide "MI_FieldGuide"
#ifdef LINETEST
#define MI_CapturePanelFieldGuide "MI_CapturePanelFieldGuide"
#endif
#define MI_RasterizePli "MI_RasterizePli"
#define MI_SafeArea "MI_SafeArea"
#define MI_ViewColorcard "MI_ViewColorcard"
#define MI_ViewGuide "MI_ViewGuide"
#define MI_ViewRuler "MI_ViewRuler"
#define MI_TCheck "MI_TCheck"
#define MI_ICheck "MI_ICheck"
#define MI_Ink1Check "MI_Ink1Check"
#define MI_PCheck "MI_PCheck"
#define MI_IOnly "MI_IOnly"
#define MI_BCheck "MI_BCheck"
#define MI_GCheck "MI_GCheck"
#define MI_ACheck "MI_ACheck"
#define MI_ShiftTrace "MI_ShiftTrace"
#define MI_EditShift "MI_EditShift"
#define MI_NoShift "MI_NoShift"
#define MI_ResetShift "MI_ResetShift"
#define MI_Histogram "MI_Histogram"
#define MI_FxParamEditor "MI_FxParamEditor"

#define MI_Link "MI_Link"
#define MI_Play "MI_Play"
#define MI_Loop "MI_Loop"
#define MI_Pause "MI_Pause"
#define MI_FirstFrame "MI_FirstFrame"
#define MI_LastFrame "MI_LastFrame"
#define MI_NextFrame "MI_NextFrame"
#define MI_PrevFrame "MI_PrevFrame"
#define MI_NextDrawing "MI_NextDrawing"
#define MI_PrevDrawing "MI_PrevDrawing"
#define MI_NextStep "MI_NextStep"
#define MI_PrevStep "MI_PrevStep"

#define MI_RedChannel "MI_RedChannel"
#define MI_GreenChannel "MI_GreenChannel"
#define MI_BlueChannel "MI_BlueChannel"
#define MI_MatteChannel "MI_MatteChannel"

#define MI_AutoFillToggle "MI_AutoFillToggle"
#define MI_RedChannelGreyscale "MI_RedChannelGreyscale"
#define MI_GreenChannelGreyscale "MI_GreenChannelGreyscale"
#define MI_BlueChannelGreyscale "MI_BlueChannelGreyscale"

#define MI_DockingCheck "MI_DockingCheck"

#define MI_OpenFileViewer "MI_OpenFileViewer"
#define MI_OpenFileBrowser2 "MI_OpenFileBrowser2"
#define MI_OpenStyleControl "MI_OpenStyleControl"
#define MI_OpenFunctionEditor "MI_OpenFunctionEditor"
#define MI_OpenLevelView "MI_OpenLevelView"
#define MI_OpenXshView "MI_OpenXshView"
#define MI_OpenCleanupSettings "MI_OpenCleanupSettings"
#define MI_ResetRoomLayout "MI_ResetRoomLayout"
#define MI_MaximizePanel "MI_MaximizePanel"
#define MI_FullScreenWindow "MI_FullScreenWindow"
#define MI_OnionSkin "MI_OnionSkin"
#define MI_ZeroThick "MI_ZeroThick"

//#define MI_LoadResourceFile       "MI_LoadResourceFile"
#define MI_DuplicateFile "MI_DuplicateFile"
#define MI_ViewFile "MI_ViewFile"
#define MI_ConvertFiles "MI_ConvertFiles"
#define MI_ConvertFileWithInput "MI_ConvertFileWithInput"
#define MI_ShowFolderContents "MI_ShowFolderContents"
#define MI_PremultiplyFile "MI_PremultiplyFile"
#define MI_AddToBatchRenderList "MI_AddToBatchRenderList"
#define MI_AddToBatchCleanupList "MI_AddToBatchCleanupList"
#define MI_ExposeResource "MI_ExposeResource"
#define MI_EditLevel "MI_EditLevel"
#define MI_ReplaceLevel "MI_ReplaceLevel"
#define MI_RevertToCleanedUp "MI_RevertToCleanedUp"
#define MI_RevertToLastSaved "MI_RevertToLastSaved"
#define MI_ConvertToVectors "MI_ConvertToVectors"
#define MI_Tracking "MI_Tracking"
#define MI_RemoveLevel "MI_RemoveLevel"
#define MI_CollectAssets "MI_CollectAssets"
#define MI_ImportScenes "MI_ImportScenes"
#define MI_ExportScenes "MI_ExportScenes"

#define MI_SelectRowKeyframes "MI_SelectRowKeyframes"
#define MI_SelectColumnKeyframes "MI_SelectColumnKeyframes"
#define MI_SelectAllKeyframes "MI_SelectAllKeyframes"
#define MI_SelectAllKeyframesNotBefore "MI_SelectAllKeyframesNotBefore"
#define MI_SelectAllKeyframesNotAfter "MI_SelectAllKeyframesNotAfter"
#define MI_SelectPreviousKeysInColumn "MI_SelectPreviousKeysInColumn"
#define MI_SelectFollowingKeysInColumn "MI_SelectFollowingKeysInColumn"
#define MI_SelectPreviousKeysInRow "MI_SelectPreviousKeysInRow"
#define MI_SelectFollowingKeysInRow "MI_SelectFollowingKeysInRow"
#define MI_InvertKeyframeSelection "MI_InvertKeyframeSelection"

#define MI_SetAcceleration "MI_SetAcceleration"
#define MI_SetDeceleration "MI_SetDeceleration"
#define MI_SetConstantSpeed "MI_SetConstantSpeed"
#define MI_ResetInterpolation "MI_ResetInterpolation"

#define MI_ActivateThisColumnOnly "MI_ActivateThisColumnOnly"
#define MI_ActivateSelectedColumns "MI_ActivateSelectedColumns"
#define MI_ActivateAllColumns "MI_ActivateAllColumns"
#define MI_DeactivateSelectedColumns "MI_DeactivateSelectedColumns"
#define MI_DeactivateAllColumns "MI_DeactivateAllColumns"
#define MI_ToggleColumnsActivation "MI_ToggleColumnsActivation"
#define MI_EnableThisColumnOnly "MI_EnableThisColumnOnly"
#define MI_EnableSelectedColumns "MI_EnableSelectedColumns"
#define MI_EnableAllColumns "MI_EnableAllColumns"
#define MI_DisableAllColumns "MI_DisableAllColumns"
#define MI_DisableSelectedColumns "MI_DisableSelectedColumns"
#define MI_SwapEnabledColumns "MI_SwapEnabledColumns"
#define MI_LockThisColumnOnly "MI_LockThisColumnOnly"
#define MI_LockSelectedColumns "MI_LockSelectedColumns"
#define MI_LockAllColumns "MI_LockAllColumns"
#define MI_UnlockSelectedColumns "MI_UnlockSelectedColumns"
#define MI_UnlockAllColumns "MI_UnlockAllColumns"
#define MI_ToggleColumnLocks "MI_ToggleColumnLocks"

#define MI_FoldColumns "MI_FoldColumns"

#define MI_LoadIntoCurrentPalette "MI_LoadIntoCurrentPalette"
#define MI_AdjustCurrentLevelToPalette "MI_AdjustCurrentLevelToPalette"
#define MI_MergeToCurrentPalette "MI_MergeToCurrentPalette"
#define MI_ReplaceWithCurrentPalette "MI_ReplaceWithCurrentPalette"
#define MI_DeletePalette "MI_DeletePalette"
#define MI_RefreshTree "MI_RefreshTree"
#define MI_LoadRecentImage "MI_LoadRecentImage"
#define MI_ClearRecentImage "MI_ClearRecentImage"

#define MI_OpenComboViewer "MI_OpenComboViewer"
#define MI_OpenHistoryPanel "MI_OpenHistoryPanel"
#define MI_ReplaceParentDirectory "MI_ReplaceParentDirectory"
#define MI_Reframe1 "MI_Reframe1"
#define MI_Reframe2 "MI_Reframe2"
#define MI_Reframe3 "MI_Reframe3"
#define MI_Reframe4 "MI_Reframe4"

#define MI_FillAreas "MI_FillAreas"
#define MI_FillLines "MI_FillLines"
#define MI_PickStyleAreas "MI_PickStyleAreas"
#define MI_PickStyleLines "MI_PickStyleLines"

#define MI_DeactivateUpperColumns "MI_DeactivateUpperColumns"
#define MI_CompareToSnapshot "MI_CompareToSnapshot"
#define MI_PreviewFx "MI_PreviewFx"

#define MI_About "MI_About"
#define MI_PencilTest "MI_PencilTest"
#endif
