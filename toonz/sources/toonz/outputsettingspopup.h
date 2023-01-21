#pragma once

#ifndef OUTPUTSETTINGSPOPUP_H
#define OUTPUTSETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"

#include "toonz/sceneproperties.h"

#include <QLabel>
// forward declaration
class ToonzScene;
class QComboBox;
class QScrollArea;
class QListWidgetItem;

namespace DVGui {
class FileField;
class LineEdit;
class IntLineEdit;
class CheckBox;
class DoubleLineEdit;
}  // namespace DVGui

class CameraSettingsPopup;

//=============================================================================
// OutputSettingsPopup
//-----------------------------------------------------------------------------

class AnimatedLabel : public QLabel {
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor)
public:
  AnimatedLabel(const QString &text, QWidget *parent = 0)
      : QLabel(text, parent) {
    setObjectName("OutputSettingsLabel");
  }
  void setColor(QColor color) {
    setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
                      .arg(color.red())
                      .arg(color.green())
                      .arg(color.blue())
                      .arg(color.alpha()));
  }
  QColor color() { return Qt::black; }
};

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
  DVGui::CheckBox *m_linearColorSpaceChk;
  DVGui::DoubleLineEdit *m_colorSpaceGammaFld;
  DVGui::DoubleLineEdit *m_gammaFld;
  QComboBox *m_dominantFieldOm;
  DVGui::CheckBox *m_applyShrinkChk;
  DVGui::CheckBox *m_subcameraChk;
  DVGui::DoubleLineEdit *m_stretchFromFld, *m_stretchToFld;
  DVGui::CheckBox *m_doStereoscopy;
  DVGui::DoubleLineEdit *m_stereoShift;
  QComboBox *m_rasterGranularityOm;
  QComboBox *m_threadsComboOm;
  bool m_allowMT;

  DVGui::DoubleLineEdit *m_frameRateFld;
  QPushButton *m_fileFormatButton;
  CameraSettingsPopup *m_cameraSettings;
  QComboBox *m_presetCombo;

  // clapperboard
  DVGui::CheckBox *m_addBoard;
  QPushButton *m_boardSettingsBtn;

  QScrollArea *m_scrollArea;
  AnimatedLabel *m_cameraLabel, *m_colorLabel, *m_fileLabel, *m_moreLabel;
  QFrame *m_cameraBox, *m_colorBox, *m_fileBox, *m_moreBox;

  DVGui::CheckBox *m_syncColorSettingsButton;

  bool m_isPreviewSettings;

  void updatePresetComboItems();
  void translateResampleOptions();

  QFrame *createPanel(bool isPreview);
  QFrame *createCameraSettingsBox(bool isPreview);
  QFrame *createColorSettingsBox(bool isPreview);
  QFrame *createFileSettingsBox(bool isPreview);
  QFrame *createMoreSettingsBox();

public:
  OutputSettingsPopup(bool isPreview = false);

protected:
  ToonzScene *getCurrentScene() const;
  TOutputProperties *getProperties() const;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  bool eventFilter(QObject *, QEvent *) override;

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
  void onLinearColorSpaceChecked(int state);
  void onColorSpaceGammaEdited();
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
  void onSyncColorSettingsChecked(int state);
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

  void onCategoryActivated(QListWidgetItem *);
};

class PreviewSettingsPopup final : public OutputSettingsPopup {
public:
  PreviewSettingsPopup() : OutputSettingsPopup(true) {}
};

#endif  // OUTPUTSETTINGSPOPUP_H
