#pragma once

#ifndef COLORMODELBEHAVIORPOPUP_H
#define COLORMODELBEHAVIORPOPUP_H

#include "tfilepath.h"
#include "toonzqt/dvdialog.h"
#include "toonz/palettecmd.h"

class QButtonGroup;
class QComboBox;

namespace DVGui {
class ColorField;
class IntLineEdit;
};

class ColorModelBehaviorPopup : public DVGui::Dialog {
  Q_OBJECT

  QButtonGroup* m_buttonGroup;

  QComboBox* m_pickColorCombo                  = NULL;
  QFrame* m_colorChipBehaviorFrame             = NULL;
  DVGui::ColorField* m_colorChipGridColor      = NULL;
  DVGui::IntLineEdit* m_colorChipGridLineWidth = NULL;
  QButtonGroup* m_colorChipOrder               = NULL;

  bool m_hasRasterImage = false;

public:
  ColorModelBehaviorPopup(const std::set<TFilePath>& paths,
                          QWidget* parent = 0);
  ~ColorModelBehaviorPopup();

  void getLoadingConfiguration(
      PaletteCmd::ColorModelLoadingConfiguration& config);

public slots:
  void onPickColorComboActivated(int);
};

#endif
