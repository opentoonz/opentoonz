#pragma once

#ifndef FIRSTLAUNCHSETUP_H
#define FIRSTLAUNCHSETUP_H

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QListWidget>
#include <QTranslator>

namespace OTZSetup {

#define INITWIZARD_REVISION 1

//-----------------------------------------------------------------------------

// List of launch wizard pages
enum {
  LANGUAGE_PAGE,
  THIRDPARTY_PAGE,
  FFMPEG_PAGE,
  RHUBARB_PAGE,
  UI_PAGE,
  PROJECTS_PAGE
};

//-----------------------------------------------------------------------------

bool requireInitWizard();

// TODO: add button on perferences to reset initialize wizard

void flagInitWizard(bool completed);

//-----------------------------------------------------------------------------

class LanguagePage : public QWizardPage {
  Q_OBJECT

public:
  LanguagePage(QWidget *parent = nullptr);
  ~LanguagePage();

private:
  QLabel *m_headLabel;
  QLabel *m_footLabel;
  QComboBox *m_languageCombo;
  QTranslator *m_translator;

  void initializePage();

private slots:
  void languageChanged(int index);
};

//-----------------------------------------------------------------------------

class ThirdPartyPage : public QWizardPage {
  Q_OBJECT

public:
  ThirdPartyPage(QWidget *parent = nullptr);

private:
  QLabel *m_footLabel;
  QPushButton *m_ffmpegButton;
  QLabel *m_ffmpegStatus;
  QLabel *m_ffmpegLabel;
  QPushButton *m_rhubarbButton;
  QLabel *m_rhubarbStatus;
  QLabel *m_rhubarbLabel;

  int m_nextPage;

  void initializePage();
  int nextId() const { return m_nextPage; };

private slots:
  void ffmpegSetup();
  void rhubarbSetup();
};

//-----------------------------------------------------------------------------

class FFMPEGPage : public QWizardPage {
  Q_OBJECT

public:
  FFMPEGPage(QWidget *parent = nullptr);

private:
  QLabel *m_richLabel;
  QLabel *m_pathLabel;
  QLineEdit *m_pathEdit;
  QPushButton *m_pathBrowse;
  QLabel *m_statusIcon;
  QLabel *m_statusLabel;

  void initializePage();

private slots:
  void browsePath();
  void testPath(const QString &path);
};

//-----------------------------------------------------------------------------

class RhubarbPage : public QWizardPage {
  Q_OBJECT

public:
  RhubarbPage(QWidget *parent = nullptr);

private:
  QLabel *m_richLabel;
  QLabel *m_pathLabel;
  QLineEdit *m_pathEdit;
  QPushButton *m_pathBrowse;
  QLabel *m_statusIcon;
  QLabel *m_statusLabel;

  void initializePage();

private slots:
  void browsePath();
  void testPath(const QString &path);
};

//-----------------------------------------------------------------------------

class UIPage : public QWizardPage {
  Q_OBJECT

public:
  UIPage(QWidget *parent = nullptr);

private:
  QLabel *m_roomsLabel;
  QComboBox *m_roomsCombo;
  QPushButton *m_resetRoom;

  QLabel *m_themeLabel;
  QComboBox *m_themeCombo;
  QLabel *m_dIconLabel;
  QComboBox *m_dIconCombo;
  QLabel *m_themeIcon;

  void initializePage();
  bool validatePage();

private slots:
  void themeChanged(int index);
  void resetRoom();
};

//-----------------------------------------------------------------------------

class ProjectsPage : public QWizardPage {
  Q_OBJECT

public:
  ProjectsPage(QWidget *parent = nullptr);

private:
  QCheckBox *m_stuffCB;
  QCheckBox *m_myDocsCB;
  QCheckBox *m_desktopCB;
  QCheckBox *m_customCB;
  QListWidget *m_customPaths;
  QPushButton *m_customAdd;
  QPushButton *m_customDel;
  QPushButton *m_customRefsh;

  void loadPaths();
  void initializePage();
  bool validatePage();

private slots:
  void customCBChanged(int state);
  void addPath();
  void delPath();
  void refreshPaths();
};

//-----------------------------------------------------------------------------

class InitWizard : public QWizard {
  Q_OBJECT

public:
  InitWizard(QWidget *parent = nullptr);

  void keyPressEvent(QKeyEvent *e);

public slots:
  void customBack();
  void customNext();
  void customCancel();
};

//-----------------------------------------------------------------------------

}  // namespace OTZSetup

#endif
