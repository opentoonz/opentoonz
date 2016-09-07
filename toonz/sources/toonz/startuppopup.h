#pragma once

#ifndef STARTUPPOPUP_H
#define STARTUPPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

// forward declaration
class QLabel;
class QComboBox;
class CameraSettingsPopup;
// class DVGui::MeasuredDoubleLineEdit;

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class StartupPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_nameFld;
  //DVGui::IntLineEdit *m_fromFld;
  //DVGui::IntLineEdit *m_toFld;
  //QComboBox *m_levelTypeOm;
  //DVGui::IntLineEdit *m_stepFld;
  //DVGui::IntLineEdit *m_incFld;
  DVGui::FileField *m_pathFld;
  QLabel *m_widthLabel;
  QLabel *m_heightLabel;
  QLabel *m_fpsLabel;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_fpsFld;
  QList<QString> names;
  QCheckBox *m_dontShowAgainCB;
  QCheckBox *m_usePixelsCB;
  //CameraSettingsPopup *m_cameraSettings;
  //QPushButton m_createSceneButton;
  //QPushButton m_dontShowAgainButton;
  

public:
	StartupPopup();

  //void setSizeWidgetEnable(bool isEnable);
  //int getLevelType() const;

protected:
  // set m_pathFld to the default path
  //void updatePath();
  //void nextName();
  void showEvent(QShowEvent *) override;

public slots:
  //void onLevelTypeChanged(const QString &text);
  //void onCreateSceneButtonClicked();
  void onRecentSceneClicked(int index);
  void onCreateButton();
  void onUsePixelsChanged(int index);
  void onDontShowAgainChanged(int index);
};


class StartupLabel : public QLabel {
	Q_OBJECT
public:
	explicit StartupLabel(const QString& text = "", QWidget* parent = 0, int index = -1);
	~StartupLabel();
	QString m_text;
	int m_index;
signals:
	void wasClicked(int index);
protected:
	void mousePressEvent(QMouseEvent* event);
};

#endif  // LEVELCREATEPOPUP_H
