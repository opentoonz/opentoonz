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
#include "convertfolderpopup.h"
#include "toonzqt/imageutils.h"

// TnzQt includes
#include "toonzqt/planeviewer.h"

// STD includes
#include <map>
#include <string>

// Qt includes
#include <QFrame>

//==================================================

//    Forward  declarations

class TPropertyGroup;
class TCamera;
class ShortcutZoomer;

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
class ExportLevelPopup : public FileBrowserPopup {
  Q_OBJECT
  friend class ExportAllLevelsPopup;

public:
  ExportLevelPopup();
  ~ExportLevelPopup();

  bool execute() override;

  virtual void GetSelectedSimpLevels();
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
  std::vector<TXshSimpleLevel *> outputLevels;
  bool allPlis;
  std::map<std::string, TPropertyGroup *> m_formatProperties;

  TFrameHandle m_levelFrameIndexHandle;  //!< Autonomous current level's frame
                                         //!\a index handle.

private slots:

  virtual void updateOnSelection();
  void onOptionsClicked();
  void onRetas(int);
  void initFolder() override;
  void onformatChanged(const QString &);
  void checkAlpha();
  void updatePreview();
};

//********************************************************************************
//    ExportOptions  definition
//********************************************************************************

class ExportLevelPopup::ExportOptions final : public QFrame {
  Q_OBJECT

public:
  ExportOptions(QWidget *parent = 0);
  bool pliOtionsVisable;  // be used when updateonselection,ExportOptions show,
  IoCmd::ExportLevelOptions getOptions() const;

signals:

  void optionsChanged();

protected:
  virtual void showEvent(QShowEvent *se) override;
  
  void updateCameraDefault();
  void updateDpi();

private:
  friend class ExportLevelPopup;
  friend class ExportAllLevelsPopup;
  
  QWidget *m_pliOptions;
  
  DVGui::ColorField *m_bgColorField;
  QCheckBox *m_noAntialias;
  QCheckBox *m_createlevelfolder;

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

//********************************************************************************
//    Swatch  definition
//********************************************************************************

class ExportLevelPopup::Swatch final : public PlaneViewer {
public:
  Swatch(QWidget *parent = 0) : PlaneViewer(parent) {}

  TImageP image() const { return m_img; }
  TImageP &image() { return m_img; }

protected:
  void showEvent(QShowEvent *se) override;
  void keyPressEvent(QKeyEvent *ke) override;
  void keyPressEvent(QShowEvent *se);
  void paintGL() override;

  void setActualPixelSize();

private:
  struct ShortcutZoomer final : public ImageUtils::ShortcutZoomer {
    ShortcutZoomer(Swatch *swatch) : ImageUtils::ShortcutZoomer(swatch) {}

  private:
    bool zoom(bool zoomin, bool resetZoom) override {
      return false;
    }  // Already covered by PlaneViewer
    bool setActualPixelSize() override {
      static_cast<Swatch *>(getWidget())->setActualPixelSize();
      return true;
    }
  };

private:
  TImageP m_img;  //!< Image shown in the swatch.
};
#endif  // EXPORTLEVELPOPUP_H
