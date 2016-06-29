#pragma once

#ifndef SVN_COMMIT_DIALOG_H
#define SVN_COMMIT_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QList>
#include <QMap>
#include <QDir>

class QLabel;
class QPushButton;
class QTreeWidget;
class QCheckBox;
class QPlainTextEdit;
class QTreeWidgetItem;
class QFile;
class ToonzScene;
class TXshLevel;

//-----------------------------------------------------------------------------

class SVNCommitDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QPushButton *m_commitButton;
  QPushButton *m_cancelButton;

  VersionControlThread m_thread;

  QList<SVNStatus> m_status;

  QStringList m_filesToAdd;
  QStringList m_filesToCommit;

  QList<QString> m_filesAdded;

  // Use for "fileMode"
  QStringList m_filesToPut;  // filesToAdd + filesToCommit
  QStringList m_sceneResourcesToCommit;

  QMap<QString, QTreeWidgetItem *> m_items;

  QCheckBox *m_commitSceneContentsCheckBox;

  // QList<SVNStatus> m_filesOutOfDate;

  QString m_workingDir;
  QStringList m_files;

  QTreeWidget *m_treeWidget;
  QCheckBox *m_selectionCheckBox;
  QLabel *m_selectionLabel;

  // Perform a commit on one or more folder
  bool m_folderOnly;

  QFile *m_targetTempFile;

  int m_sceneIconAdded;
  int m_folderAdded;  // Used only to display the right number on the label

public:
  SVNCommitDialog(QWidget *parent, const QString &workingDir,
                  const QStringList &files, bool folderOnly = false,
                  int m_sceneIconAdded = 0);

  void checkFiles(bool isExternalFiles = false);

private:
  void addFiles();
  void commitFiles();
  void setPropertyFiles();

  void switchToCloseButton();

  void addUnversionedItem(const QString &relativePath);
  void addModifiedItem(const QString &relativePath);

  void addUnversionedFolders(const QDir &dir, const QString &relativePath);

  void updateTreeSelectionLabel();

  void initTreeWidget();

protected slots:

  void onCommitButtonClicked();

  void onAddDone();
  void onSetPropertyDone();
  void onCommitDone();

  void onItemChanged(QTreeWidgetItem *);
  void onSelectionCheckBoxClicked(bool checked);

  void onError(const QString &);

  void onStatusRetrieved(const QString &);
  void onStatusRetrievedAfterAdd(const QString &);
  void onResourcesStatusRetrieved(const QString &);

  void onCommiSceneContentsToggled(bool);

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNCommitFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QPushButton *m_commitButton;
  QPushButton *m_cancelButton;

  QString m_workingDir;
  QStringList m_filesToCommit;
  QString m_file;

  QString m_propertyValue;

  VersionControlThread m_thread;

  bool m_hasError;

  QList<SVNPartialLockInfo> m_lockInfos;
  SVNPartialLockInfo m_myInfo;

  ToonzScene *m_scene;
  TXshLevel *m_level;

  QString m_hookFileName;
  QString m_newHookFileName;

  bool m_isOVLLevel;

public:
  SVNCommitFrameRangeDialog(QWidget *parent, const QString &workingDir,
                            const QString &file);

private:
  void switchToCloseButton();

protected slots:
  void onError(const QString &);
  void onPropGetDone(const QString &);
  void onPropSetDone();

  void onHookFileAdded();

  void onPutButtonClicked();

  void commit();

  void onCommitDone();
  void onUpdateDone();
  void onLockDone();

signals:
  void done(const QStringList &);
};

#endif  // SVN_COMMIT_DIALOG_H
