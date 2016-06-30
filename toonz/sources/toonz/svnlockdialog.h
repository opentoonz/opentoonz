#pragma once

#ifndef SVN_LOCK_DIALOG_H
#define SVN_LOCK_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QList>

class QLabel;
class QPushButton;
class QCheckBox;
class QPlainTextEdit;
class QTreeWidget;
class QFile;

//-----------------------------------------------------------------------------

class SVNLockDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QTreeWidget *m_treeWidget;

  QPushButton *m_lockButton;
  QPushButton *m_cancelButton;

  QCheckBox *m_editSceneContentsCheckBox;

  QString m_workingDir;
  QList<SVNStatus> m_status;
  QStringList m_files;
  QStringList m_filesToEdit;
  QStringList m_sceneResources;

  VersionControlThread m_thread;

  bool m_lock;
  bool m_hasError;

  int m_sceneIconAdded;

  QFile *m_targetTempFile;

public:
  SVNLockDialog(QWidget *parent, const QString &workingDir,
                const QStringList &filesToEdit, bool lock, int sceneIconAdded);

private:
  void switchToCloseButton();
  void checkFiles();

  void initTreeWidget();
  void executeCommand();

protected slots:

  void onStatusRetrieved(const QString &);
  void onError(const QString &);
  void onLockButtonClicked();
  void onLockDone();

  void onSceneResourcesStatusRetrieved(const QString &);

  void onEditSceneContentsToggled(bool checked);

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNLockInfoDialog final : public DVGui::Dialog {
  Q_OBJECT
  SVNStatus m_status;

public:
  SVNLockInfoDialog(QWidget *parent, const SVNStatus &status);
};

#endif  // SVN_LOCK_DIALOG_H
