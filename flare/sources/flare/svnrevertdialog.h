#pragma once

#ifndef SVN_REVERT_DIALOG_H
#define SVN_REVERT_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QList>

class QPushButton;
class QTreeWidget;
class QCheckBox;

//-----------------------------------------------------------------------------

class SVNRevertDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QString m_workingDir;
  QStringList m_files;

  QPushButton *m_revertButton;
  QPushButton *m_cancelButton;

  QTreeWidget *m_treeWidget;

  VersionControlThread m_thread;

  QList<SVNStatus> m_status;

  QList<QString> m_filesToRevert;
  QStringList m_sceneResources;

  QCheckBox *m_revertSceneContentsCheckBox;

  // Perform a revert on one or more folder
  bool m_folderOnly;

  int m_sceneIconAdded;

public:
  SVNRevertDialog(QWidget *parent, const QString &workingDir,
                  const QStringList &files, bool folderOnly = false,
                  int sceneIconAdded = 0);

  void checkFiles();

private:
  void switchToCloseButton();

  void revertFiles();

  void initTreeWidget();

protected slots:

  void onRevertButtonClicked();
  void onRevertDone();

  void onError(const QString &);
  void onStatusRetrieved(const QString &);

  void onRevertSceneContentsToggled(bool checked);

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNRevertFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QString m_workingDir;
  QString m_file;
  QString m_tempFileName;

  QPushButton *m_revertButton;
  QPushButton *m_cancelButton;

  VersionControlThread m_thread;
  QList<SVNStatus> m_status;
  QList<QString> m_filesToRevert;
  QStringList m_files;

public:
  SVNRevertFrameRangeDialog(QWidget *parent, const QString &workingDir,
                            const QString &file, const QString &tempFileName);

private:
  void switchToCloseButton();
  void revertFiles();

  void checkFiles();

protected slots:
  void onRevertButtonClicked();
  void onError(const QString &);
  void onStatusRetrieved(const QString &);
  void onRevertDone();

signals:
  void done(const QStringList &);
};

#endif  // SVN_REVERT_DIALOG_H
