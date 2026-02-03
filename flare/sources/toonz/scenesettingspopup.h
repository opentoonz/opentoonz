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
class QLineEdit;

class CellMarksPopup final : public QDialog {
  Q_OBJECT
  struct MarkerField {
    int id;
    DVGui::ColorField *colorField;
    QLineEdit *nameField;
  };

  QList<MarkerField> m_fields;

public:
  CellMarksPopup(QWidget *parent);
  void update();
protected slots:
  void onColorChanged(const TPixel32 &, bool);
  void onNameChanged();
};

class ColorFiltersPopup final : public QDialog {
  Q_OBJECT
  struct FilterField {
    DVGui::ColorField *colorField;
    QLineEdit *nameField;
    QPushButton *clearBtn;
  };

  QMap<int, FilterField> m_fields;

public:
  ColorFiltersPopup(QWidget *parent);
  void updateContents();
protected slots:
  void onColorChanged(const TPixel32 &, bool);
  void onNameChanged();
  void onClearButtonClicked();
};
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

  CellMarksPopup *m_cellMarksPopup;
  ColorFiltersPopup *m_colorFiltersPopup;

  DVGui::DoubleLineEdit *m_colorSpaceGammaFld;

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
  void onMakerInformationChanged();

  void setBgColor(const TPixel32 &value, bool isDragging);

  void onColorFilterOnRenderChanged();

  void onEditCellMarksButtonClicked();
  void onEditColorFiltersButtonClicked();
};

#endif  // SCENESETTINGSPOPUP_H
