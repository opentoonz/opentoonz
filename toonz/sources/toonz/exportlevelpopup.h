#pragma once

#ifndef EXPORTLEVELPOPUP_H
#define EXPORTLEVELPOPUP_H

// TnzCore includes
#include "tpixel.h"

// TnzLib includes
#include "toonz/tframehandle.h"

// Tnz6 includes
#include "filebrowserpopup.h"
#include "exportlevelcommand.h"

// STD includes
#include <map>
#include <string>

// Qt includes
#include <QFrame>

//==================================================

//    Forward  declarations

class TPropertyGroup;
class TCamera;

namespace DVGui {
class CheckBox;
class IntLineEdit;
class DoubleLineEdit;
class MeasuredDoubleLineEdit;
class ColorField;
}

class FileBrowser;

class QString;
class QLabel;
class QCheckBox;
class QPushButton;
class QComboBox;
class QGroupBox;
class QShowEvent;
class QHideEvent;

//==================================================

//*********************************************************************************
//    Export Level Popup  declaration
//*********************************************************************************

/*!
  \brief    The popup dealing with level exports in Toonz.
*/
class ExportLevelPopup final : public FileBrowserPopup {
  Q_OBJECT

public:
  ExportLevelPopup();
  ~ExportLevelPopup();

  bool execute() override;

  TPropertyGroup *getFormatProperties(const std::string &ext);
  IoCmd::ExportLevelOptions getOptions(const std::string &ext);

protected:
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;

private:
  class ExportOptions;
  class Swatch;

private:
  // Widgets

  DVGui::CheckBox *m_retas;
  QComboBox *m_format;
  QPushButton *m_formatOptions;

  ExportOptions *m_exportOptions;
  Swatch *m_swatch;

  // Others

  std::map<std::string, TPropertyGroup *> m_formatProperties;

  TFrameHandle m_levelFrameIndexHandle;  //!< Autonomous current level's frame
                                         //!\a index handle.

private slots:

  void onOptionsClicked();
  void onRetas(int);
  void initFolder() override;
  void updateOnSelection();
  void onformatChanged(const QString &);
  void checkAlpha();
  void updatePreview();
};

//-----------------------------------------------------------------------------

class ExportLevelPopup::ExportOptions final : public QFrame {
  Q_OBJECT

public:
  ExportOptions(QWidget *parent = 0);

  IoCmd::ExportLevelOptions getOptions() const;

  void updateOnSelection();

signals:

  void optionsChanged();

protected:
  void showEvent(QShowEvent *se) override;

  void updateCameraDefault();
  void updateDpi();

private:
  friend class ExportLevelPopup;

  QWidget *m_pliOptions;

  DVGui::ColorField *m_bgColorField;
  QCheckBox *m_noAntialias;

  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;

  DVGui::IntLineEdit *m_hResFld;
  DVGui::IntLineEdit *m_vResFld;

  DVGui::MeasuredDoubleLineEdit *m_resScale;

  QLabel *m_dpiLabel, *m_widthLabel, *m_heightLabel;

  QComboBox *m_thicknessTransformMode;

  DVGui::MeasuredDoubleLineEdit *m_fromThicknessScale,
      *m_fromThicknessDisplacement, *m_toThicknessScale,
      *m_toThicknessDisplacement;

private slots:

  void updateXRes();
  void updateYRes();
  void scaleRes();

  void onThicknessTransformModeChanged();
};

#endif  // EXPORTLEVELPOPUP_H
