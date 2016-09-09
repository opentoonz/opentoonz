#pragma once

#ifndef STARTUPPOPUP_H
#define STARTUPPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/camerasettingswidget.h"
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

// forward declaration
class QLabel;
class QComboBox;

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class StartupPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_nameFld;
  DVGui::FileField *m_pathFld;
  QLabel *m_widthLabel;
  QLabel *m_heightLabel;
  QLabel *m_fpsLabel;
  QLabel *m_resLabel;
  QLabel *m_resTextLabel;
  QLabel *m_dpiLabel;
  DVGui::DoubleLineEdit *m_dpiFld;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_fpsFld;
  QList<QString> names;
  QList<TFilePath> m_projectPaths;
  QCheckBox *m_showAtStartCB;
  QCheckBox *m_usePixelsCB;
  QComboBox *m_projectsCB;
  QPushButton *m_loadOtherSceneButton;
  QPushButton *m_newProjectButton;
  QComboBox *m_presetCombo;
  QPushButton *m_addPresetBtn, *m_removePresetBtn;
  CameraSettingsWidget *m_cameraSettingsWidget;
  int m_dpi, m_xRes, m_yRes;
  bool m_updating = false;
  QString m_presetListFile;

public:
  StartupPopup();

protected:
  void showEvent(QShowEvent *) override;
  void loadPresetList();
  void savePresetList();
  QString aspectRatioValueToString(double value, int width = 0, int height = 0);
  double aspectRatioStringToValue(const QString &s);
  bool parsePresetString(const QString &str, QString &name, int &xres,
                         int &yres, double &fx, double &fy, QString &xoffset,
                         QString &yoffset, double &ar, bool forCleanup = false);

public slots:
  void onRecentSceneClicked(int index);
  void onCreateButton();
  void onUsePixelsChanged(int index);
  void onShowAtStartChanged(int index);
  void updateProjectCB();
  void onProjectChanged(int index);
  void onNewProjectButtonPressed();
  void onLoadSceneButtonPressed();
  void onSceneChanged();
  void updateResolution();
  void onDpiChanged();
  void addPreset();
  void removePreset();
  void onPresetSelected(const QString &str);
};

class StartupLabel : public QLabel {
  Q_OBJECT
public:
  explicit StartupLabel(const QString &text = "", QWidget *parent = 0,
                        int index = -1);
  ~StartupLabel();
  QString m_text;
  int m_index;
signals:
  void wasClicked(int index);

protected:
  void mousePressEvent(QMouseEvent *event);
};

#endif  // STARTUPPOPUP_H
