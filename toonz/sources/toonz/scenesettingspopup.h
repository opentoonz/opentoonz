#pragma once

#ifndef SCENESETTINGSPOPUP_H
#define SCENESETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tpixel.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"

// forward declaration
class TSceneProperties;
class QComboBox;

//=============================================================================
// SceneSettingsPopup
//-----------------------------------------------------------------------------

class SceneSettingsPopup final : public QDialog {
  Q_OBJECT

  DVGui::DoubleLineEdit *m_frameRateFld;
  DVGui::ColorField *m_bgColorFld;

  DVGui::IntLineEdit *m_fieldGuideFld;
  DVGui::DoubleLineEdit *m_aspectRatioFld;

  DVGui::IntLineEdit *m_fullcolorSubsamplingFld;
  DVGui::IntLineEdit *m_tlvSubsamplingFld;

  DVGui::IntLineEdit *m_markerIntervalFld;
  DVGui::IntLineEdit *m_startFrameFld;

  DVGui::CheckBox *m_colorFilterOnRenderCB;

  TSceneProperties *getProperties() const;

public:
  SceneSettingsPopup();
  void configureNotify();

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public slots:

  void update();

  void onFrameRateEditingFinished();
  void onFieldGuideSizeEditingFinished();
  void onFieldGuideAspectRatioEditingFinished();

  void onFullColorSubsampEditingFinished();
  void onTlvSubsampEditingFinished();
  void onMakerIntervalEditingFinished();
  void onStartFrameEditingFinished();

  void setBgColor(const TPixel32 &value, bool isDragging);

  void onColorFilterOnRenderChanged();
};

#endif  // SCENESETTINGSPOPUP_H
