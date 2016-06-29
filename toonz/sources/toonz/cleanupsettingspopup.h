#pragma once

#ifndef CLEANUPSETTINGSPOPUP__H
#define CLEANUPSETTINGSPOPUP__H

// ToonzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/cleanupcamerasettingswidget.h"

// ToonzLib includes
#include "toonz/cleanupparameters.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QFrame>

//================================================

//  Forward declarations

class QComboBox;

class CleanupTab;
class ProcessingTab;
class CameraTab;
class CleanupSwatch;
class CleanupCameraSettingsWidget;
class CleanupPaletteViewer;

namespace DVGui {

class CheckBox;
class DoubleLineEdit;
class FileField;
class MeasuredDoubleLineEdit;
class DoubleField;
class IntField;
}

//================================================

//*****************************************************************************
//    CleanupSettingsPopup declaration
//*****************************************************************************

class CleanupSettings final : public QWidget {
  Q_OBJECT

  CleanupTab *m_cleanupTab;
  ProcessingTab *m_processingTab;
  CameraTab *m_cameraTab;

  CleanupSwatch *m_swatch;
  QAction *m_swatchAct, *m_opacityAct;

  CleanupParameters m_backupParams;

  bool m_attached;  //!< Whether the settomgs are attached to the
                    //!< cleanup model
public:
  CleanupSettings(QWidget *parent = 0);

signals:

  /*! \details   The window title may change to reflect updates of the
     underlying
           cleanup settings model.                                            */
  void windowTitleChanged(
      const QString &title);  //!< Signals a change of the window title.

public slots:

  void updateGui(bool postProcessPreviews);

  void enableSwatch(bool);
  void enableOpacityCheck(bool);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

private slots:

  void onImageSwitched();
  void onPreviewDataChanged();
  void postProcess();
  void onClnLoaded();
  void onRestoreSceneSettings();
};

//**********************************************************************
//    Cleanup Tab declaration
//**********************************************************************

class CleanupTab final : public QFrame {
  Q_OBJECT

  DVGui::CheckBox *m_autoCenter, *m_flipX, *m_flipY;
  QComboBox *m_pegHolesOm, *m_fieldGuideOm, *m_rotateOm;

  DVGui::FileField *m_pathField;
  TFilePath m_path;  //!< Actual params' path, may be different
                     //!< from the field

public:
  CleanupTab();

  void updateGui(CleanupParameters *params, CleanupParameters *oldParams);

private slots:

  void onGenericSettingsChange();
  void onPathChange();

private:
  QString pathString(const TFilePath &path, bool lpNone);
};

//**********************************************************************
//    ProcessingTab declaration
//**********************************************************************

class ProcessingTab final : public QFrame {
  Q_OBJECT

  CleanupPaletteViewer *m_paletteViewer;

  QComboBox *m_lineProcessing;
  QLabel *m_antialiasLabel;
  QComboBox *m_antialias;
  QLabel *m_sharpLabel;
  DVGui::DoubleField *m_sharpness;
  QLabel *m_despeckLabel;
  DVGui::IntField *m_despeckling;
  QLabel *m_aaValueLabel;
  DVGui::IntField *m_aaValue;
  QLabel *m_autoadjustLabel;
  QComboBox *m_autoadjustOm;
  DVGui::Separator *m_paletteSep;
  QWidget *m_settingsFrame;

public:
  ProcessingTab();

  void updateGui(CleanupParameters *params, CleanupParameters *oldParams);

private:
  void updateVisibility();

private slots:

  void onGenericSettingsChange();
  void onSharpnessChange(bool dragging);
};

//**********************************************************************
//    CameraTab declaration
//**********************************************************************

class CameraTab final : public CleanupCameraSettingsWidget {
  Q_OBJECT

public:
  CameraTab();

  void updateGui(CleanupParameters *params, CleanupParameters *oldParams);
  void updateImageInfo();

private slots:

  void onLevelSwitched();
  void onGenericSettingsChange();
};

#endif  // CLEANUPSETTINGSPOPUP__H
