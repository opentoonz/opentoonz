#pragma once

#ifndef LEVELSETTINGSPOPUP_H
#define LEVELSETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"

#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshsoundlevel.h"

// forward declaration
class QLabel;
class QComboBox;
class QPushButton;
class TXshLevel;
class TSelection;

namespace DVGui {
class FileField;
class DoubleField;
class LineEdit;
class IntLineEdit;
class DoubleLineEdit;
class MeasuredDoubleLineEdit;
class CheckBox;
}

//=============================================================================
// LevelSettingsPopup
//-----------------------------------------------------------------------------

class LevelSettingsPopup final : public DVGui::Dialog {
  Q_OBJECT

  TXshSimpleLevelP m_sl;
  TXshPaletteLevelP m_pl;
  TXshChildLevelP m_cl;
  TXshSoundLevelP m_sdl;

  DVGui::LineEdit *m_nameFld;
  DVGui::FileField *m_pathFld;
  QLabel *m_scanPathLabel;
  DVGui::FileField *m_scanPathFld;
  QLabel *m_typeLabel;
  QComboBox *m_dpiTypeOm;
  QLabel *m_dpiLabel;
  DVGui::DoubleLineEdit *m_dpiFld;
  DVGui::CheckBox *m_squarePixCB;
  QLabel *m_widthLabel;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  QLabel *m_heightLabel;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  QPushButton *m_useCameraDpiBtn;
  QLabel *m_cameraDpiLabel;
  QLabel *m_imageDpiLabel;
  QLabel *m_imageResLabel;
  QLabel *m_cameraDpiTitle;
  QLabel *m_imageDpiTitle;
  QLabel *m_imageResTitle;
  DVGui::CheckBox *m_doPremultiply;
  DVGui::CheckBox *m_whiteTransp;
  DVGui::CheckBox *m_doAntialias;
  DVGui::IntLineEdit *m_antialiasSoftness;

  QLabel *m_subsamplingLabel;
  DVGui::IntLineEdit *m_subsamplingFld;

public:
  LevelSettingsPopup();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

public slots:

  void onCastSelectionChanged();
  void onSelectionSwitched(TSelection *oldSelection, TSelection *newSelection);
  void updateLevelSettings();
  void onNameChanged();
  void onPathChanged();
  void onScanPathChanged();
  void onDpiTypeChanged(int);
  void onDpiFieldChanged();
  void onWidthFieldChanged();
  void onHeightFieldChanged();
  void onSquarePixelChanged(int);
  void useCameraDpi();
  void onSubsamplingChanged();
  void onDoPremultiplyChanged(int);
  void onDoAntialiasChanged(int);
  void onAntialiasSoftnessChanged();
  void onWhiteTranspChanged(int);

protected slots:
  void onSceneChanged();
};

#endif  // LEVELSETTINGSPOPUP_H
