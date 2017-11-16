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
#include <QGroupBox>

// forward declaration
class QLabel;
class QComboBox;
class StartupLabel;

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
  QLabel *m_resXLabel;
  QLabel *m_resTextLabel;
  QLabel *m_dpiLabel;
  QLabel *m_sceneNameLabel;
  DVGui::DoubleLineEdit *m_dpiFld;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_fpsFld;
  DVGui::DoubleLineEdit *m_resXFld;
  DVGui::DoubleLineEdit *m_resYFld;
  DVGui::IntLineEdit *m_autoSaveTimeFld;
  QList<QString> m_sceneNames;
  QList<TFilePath> m_projectPaths;
  QCheckBox *m_showAtStartCB;
  QCheckBox *m_autoSaveOnCB;
  QComboBox *m_projectsCB;
  QComboBox *m_unitsCB;
  QPushButton *m_loadOtherSceneButton;
  QPushButton *m_newProjectButton;
  QComboBox *m_presetCombo;
  QPushButton *m_addPresetBtn, *m_removePresetBtn;
  CameraSettingsWidget *m_cameraSettingsWidget;
  double m_dpi;
  int m_xRes, m_yRes;
  const int RECENT_SCENES_MAX_COUNT = 10;
  bool m_updating                   = false;
  QString m_presetListFile;
  QGroupBox *m_projectBox;
  QGroupBox *m_sceneBox;
  QGroupBox *m_recentBox;
  QVBoxLayout *m_recentSceneLay;
  QVector<StartupLabel *> m_recentNamesLabels;

public:
  StartupPopup();

protected:
  void showEvent(QShowEvent *) override;
  void loadPresetList();
  void savePresetList();
  void refreshRecentScenes();
  QString aspectRatioValueToString(double value, int width = 0, int height = 0);
  double aspectRatioStringToValue(const QString &s);
  bool parsePresetString(const QString &str, QString &name, int &xres,
                         int &yres, double &fx, double &fy, QString &xoffset,
                         QString &yoffset, double &ar, bool forCleanup = false);

public slots:
  void onRecentSceneClicked(int index);
  void onCreateButton();
  void onShowAtStartChanged(int index);
  void updateProjectCB();
  void onProjectChanged(int index);
  void onNewProjectButtonPressed();
  void onLoadSceneButtonPressed();
  void onSceneChanged();
  void updateResolution();
  void updateSize();
  void onDpiChanged();
  void addPreset();
  void removePreset();
  void onPresetSelected(const QString &str);
  void onCameraUnitChanged(int index);
  void onAutoSaveOnChanged(int index);
  void onAutoSaveTimeChanged();
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
