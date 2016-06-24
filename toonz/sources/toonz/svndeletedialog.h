#pragma once

#ifndef SVN_DELETE_DIALOG_H
#define SVN_DELETE_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

class QLabel;
class QPushButton;
class QCheckBox;
class QPlainTextEdit;
class QTreeWidget;
class QFile;

//-----------------------------------------------------------------------------

class SVNDeleteDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QPushButton *m_deleteLocalButton;
  QPushButton *m_deleteServerButton;
  QPushButton *m_cancelButton;

  QTreeWidget *m_treeWidget;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QCheckBox *m_keepLocalCopyCheckBox;

  QCheckBox *m_deleteSceneContentsCheckBox;

  QStringList m_sceneResources;

  QString m_workingDir;
  QStringList m_files;

  VersionControlThread m_thread;

  QFile *m_targetTempFile;

  bool m_isFolder;

  int m_sceneIconAdded;

public:
  SVNDeleteDialog(QWidget *parent, const QString &workingDir,
                  const QStringList &filesToDelete, bool isFolder,
                  int sceneIconAdded);

protected:
  void showEvent(QShowEvent *) override;

private:
  void switchToCloseButton();

  void updateFileBrowser();

  void initTreeWidget();

protected slots:

  void onError(const QString &);
  void onDeleteLocalButtonClicked();
  void onDeleteServerButtonClicked();

  void deleteFiles();
  void commitDeletedFiles();

  void onCommitDone();

  void onDeleteSceneContentsToggled(bool);

signals:
  void done(const QStringList &);
};

#endif  // SVN_DELETE_DIALOG_H
