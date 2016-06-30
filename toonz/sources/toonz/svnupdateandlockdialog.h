#pragma once

#ifndef SVN_UPDATE_AND_LOCK_DIALOG_H
#define SVN_UPDATE_AND_LOCK_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QList>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QCheckBox;

//-----------------------------------------------------------------------------

class SVNUpdateAndLockDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QPushButton *m_updateAndLockButton;
  QPushButton *m_lockButton;
  QPushButton *m_cancelButton;

  QCheckBox *m_editSceneContentsCheckBox;

  QString m_workingDir;
  QList<SVNStatus> m_status;
  QStringList m_files;
  QStringList m_filesToEdit;
  QStringList m_filesToMerge;
  QStringList m_sceneResources;
  int m_workingRevision;

  VersionControlThread m_thread;

  bool m_hasError;
  bool m_updateWorkingRevisionNeeded;

  int m_sceneIconAdded;

public:
  SVNUpdateAndLockDialog(QWidget *parent, const QString &workingDir,
                         const QStringList &files, int workingRevision,
                         int sceneIconAdded);

private:
  void switchToCloseButton();
  void checkFiles();

  void lockCommand();
  void updateCommand();

protected slots:
  void onError(const QString &);

  void onUpdateAndLockButtonClicked();
  void onLockButtonClicked();

  void onUpdateDone();
  void onLockDone();

  void finish();

  void onStatusRetrieved(const QString &);
  void onEditSceneContentsToggled(bool checked);
  void onSceneResourcesStatusRetrieved(const QString &);

  void reverseMerge();

signals:
  void done(const QStringList &);
};

#endif  // SVN_UPDATE_AND_LOCK_DIALOG_H
