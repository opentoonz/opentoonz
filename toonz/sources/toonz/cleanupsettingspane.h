#pragma once

#ifndef CLEANUPSETTINGSPANE_H
#define CLEANUPSETTINGSPANE_H

// ToonzLib includes
#include "toonz/cleanupparameters.h"

// ToonzQt includes
#include "toonzqt/cleanupcamerasettingswidget.h"
#include "toonzqt/filefield.h"

// Qt includes
#include <QFrame>

//  Forward declarations
class QComboBox;
class QLabel;
class QCheckBox;
class CleanupPaletteViewer;

namespace DVGui {

class DoubleField;
class IntField;
}

/*
"Save In"
フィールドのためのFileField。browseDirectoryを再実装して、フィールドが空欄のときは、
カレントレベル（Scan画像。TIF等）の入っているフォルダの１つ上をデフォルトフォルダにして開くようにしたい。
*/
class CleanupSaveInField : public DVGui::FileField {
  Q_OBJECT
public:
  CleanupSaveInField(QWidget *parent = 0, QString path = 0)
      : DVGui::FileField(parent, path) {}

protected slots:
  void browseDirectory();
};

class CleanupSettingsPane : public QFrame {
  Q_OBJECT
public:
  //----Cleanup Camera Settings
  CleanupCameraSettingsWidget *m_cameraWidget;

private:
  //----Rotate & Flip
  QComboBox *m_rotateOm;
  QCheckBox *m_flipX;
  QCheckBox *m_flipY;
  //----Line Processing
  QComboBox *m_antialias;
  DVGui::DoubleField *m_sharpness;
  DVGui::IntField *m_despeckling;
  QLabel *m_aaValueLabel;
  DVGui::IntField *m_aaValue;
  QComboBox *m_lineProcessing;
  //----Cleanup Palette
  CleanupPaletteViewer *m_paletteViewer;
  //----Bottom Parts
  CleanupSaveInField *m_pathField;
  TFilePath m_path;

  CleanupParameters m_backupParams;
  bool m_attached;

public:
  CleanupSettingsPane(QWidget *parent = 0);

protected:
  void showEvent(QShowEvent *);
  void hideEvent(QHideEvent *);

public slots:

  void updateGui(bool postProcessPreviews);
  void updateGui(CleanupParameters *params, CleanupParameters *oldParams);
  void updateImageInfo();

  // called from CleanupSaveInField
  TFilePath getLastSelectedPath() { return m_path; }

private:
  void updateVisibility();

private slots:

  void onImageSwitched();
  void onPreviewDataChanged();
  void postProcess();
  void onClnLoaded();
  void onRestoreSceneSettings();
  void onGenericSettingsChange();
  void onPathChange();
  void onSharpnessChange(bool dragging);
  void onLevelSwitched();
  void onSaveSettings();
};

#endif