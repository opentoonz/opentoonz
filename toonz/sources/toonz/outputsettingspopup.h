#pragma once

#ifndef OUTPUTSETTINGSPOPUP_H
#define OUTPUTSETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"

#include "toonz/sceneproperties.h"

// forward declaration
class ToonzScene;
class QComboBox;

namespace DVGui {
class FileField;
class LineEdit;
class IntLineEdit;
class CheckBox;
class DoubleLineEdit;
}

class CameraSettingsPopup;

//=============================================================================
// OutputSettingsPopup
//-----------------------------------------------------------------------------

class OutputSettingsPopup : public DVGui::Dialog {
  Q_OBJECT

  DVGui::FileField *m_saveInFileFld;
  DVGui::LineEdit *m_fileNameFld;
  QComboBox *m_fileFormat;
  QComboBox *m_outputCameraOm;
  DVGui::IntLineEdit *m_startFld, *m_endFld;
  DVGui::IntLineEdit *m_stepFld, *m_shrinkFld;
  QComboBox *m_multimediaOm;
  QComboBox *m_resampleBalanceOm;
  QComboBox *m_channelWidthOm;
  DVGui::DoubleLineEdit *m_gammaFld;
  QComboBox *m_dominantFieldOm;
  DVGui::CheckBox *m_applyShrinkChk;
  DVGui::CheckBox *m_subcameraChk;
  DVGui::DoubleLineEdit *m_stretchFromFld, *m_stretchToFld;
  DVGui::CheckBox *m_doStereoscopy;
  DVGui::DoubleLineEdit *m_stereoShift;
  QComboBox *m_rasterGranularityOm;
  QComboBox *m_threadsComboOm;

  DVGui::DoubleLineEdit *m_frameRateFld;
  QPushButton *m_fileFormatButton;
  QPushButton *m_renderButton;
  CameraSettingsPopup *m_cameraSettings;
  QComboBox *m_presetCombo;

  // clapperboard
  DVGui::CheckBox *m_addBoard;
  QPushButton *m_boardSettingsBtn;

  bool m_isPreviewSettings;

  void updatePresetComboItems();
  void translateResampleOptions();

public:
  OutputSettingsPopup(bool isPreview = false);

protected:
  ToonzScene *getCurrentScene() const;
  TOutputProperties *getProperties() const;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:

  void updateField();
  void onPathChanged();
  void onNameChanged();
  void onFormatChanged(const QString &str);
  void openSettingsPopup();
  void onCameraChanged(const QString &str);
  void onFrameFldEditFinished();
  void onResampleChanged(int type);
  void onChannelWidthChanged(int type);
  void onGammaFldEditFinished();
  void onDominantFieldChanged(int type);
  void onStretchFldEditFinished();
  void onApplyShrinkChecked(int state);
  void onSubcameraChecked(int state);
  void onMultimediaChanged(int mode);
  void onThreadsComboChanged(int type);
  void onRasterGranularityChanged(int type);
  void onStereoChecked(int);
  void onStereoChanged();
  void onRenderClicked();

  /*-- OutputSettingsのPreset登録/削除/選択 --*/
  void onAddPresetButtonPressed();
  void onRemovePresetButtonPressed();
  void onPresetSelected(const QString &);
  /*-- OutputsettingsのPresetコンボを更新するため --*/
  void onCameraSettingsChanged();
  /*-- Scene Settings のFPSを編集できるようにする --*/
  void onFrameRateEditingFinished();
  // clapperboard
  void onAddBoardChecked(int state);
  void onBoardSettingsBtnClicked();
};

class PreviewSettingsPopup final : public OutputSettingsPopup {
public:
  PreviewSettingsPopup() : OutputSettingsPopup(true) {}
};

#endif  // OUTPUTSETTINGSPOPUP_H
