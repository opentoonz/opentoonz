#pragma once

#ifndef LEVELSETTINGSPOPUP_H
#define LEVELSETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"

#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshsoundlevel.h"

#include <QSet>

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
}  // namespace DVGui

enum SelectedLevelType {
  None        = 0x0,
  ToonzRaster = 0x1,
  Raster      = 0x2,
  Mesh        = 0x4,
  ToonzVector = 0x8,
  Palette     = 0x10,
  SubXsheet   = 0x20,
  Sound       = 0x40,
  Others      = 0x80,

  MultiSelection  = 0x100,
  HideOnPixelMode = 0x200,
  NoSelection     = 0x400,

  SimpleLevel = ToonzRaster | Raster | Mesh | ToonzVector,
  HasDPILevel = ToonzRaster | Raster | Mesh,
  AllTypes    = SimpleLevel | Palette | SubXsheet | Sound
};

struct LevelSettingsValues {
  QString name, path, scanPath, typeStr, imageDpi, imageRes;
  int dpiType = -1, softness = -1, subsampling = -1;
  TPointD dpi               = TPointD(0, 0);
  Qt::CheckState doPremulti = Qt::Unchecked, whiteTransp = Qt::Unchecked,
                 doAntialias = Qt::Unchecked, isDirty = Qt::Unchecked;
  double width = 0.0, height = 0.0;
};

//=============================================================================
// LevelSettingsPopup
//-----------------------------------------------------------------------------

class LevelSettingsPopup final : public DVGui::Dialog {
  Q_OBJECT

  QSet<TXshLevelP> m_selectedLevels;
  QMap<QWidget *, unsigned int> m_activateFlags;

  DVGui::LineEdit *m_nameFld;
  DVGui::FileField *m_pathFld;
  DVGui::FileField *m_scanPathFld;
  QLabel *m_typeLabel;
  QComboBox *m_dpiTypeOm;
  DVGui::DoubleLineEdit *m_dpiFld;
  DVGui::CheckBox *m_squarePixCB;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  QPushButton *m_useCameraDpiBtn;
  QLabel *m_cameraDpiLabel;
  QLabel *m_imageDpiLabel;
  QLabel *m_imageResLabel;
  DVGui::CheckBox *m_doPremultiply;
  DVGui::CheckBox *m_whiteTransp;
  DVGui::CheckBox *m_doAntialias;
  QLabel *m_softnessLabel;
  DVGui::IntLineEdit *m_antialiasSoftness;

  QLabel *m_subsamplingLabel;
  DVGui::IntLineEdit *m_subsamplingFld;

  SelectedLevelType getType(TXshLevelP);
  LevelSettingsValues getValues(TXshLevelP);

public:
  LevelSettingsPopup();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:

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
  void onDoPremultiplyClicked();
  void onDoAntialiasClicked();
  void onAntialiasSoftnessChanged();
  void onWhiteTranspClicked();
  void onSceneChanged();
  void onPreferenceChanged(const QString &);
};

#endif  // LEVELSETTINGSPOPUP_H
