#pragma once

#ifndef SVN_UPDATE_DIALOG_H
#define SVN_UPDATE_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include <QList>

class QLabel;
class QPushButton;
class DateChooserWidget;
class ConflictWidget;
class QTextEdit;
class QCheckBox;

//-----------------------------------------------------------------------------

class SVNUpdateDialog final : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_closeButton;
  QPushButton *m_cancelButton;
  QPushButton *m_updateButton;  // only in update to revision

  VersionControlThread m_thread;

  QCheckBox *m_updateSceneContentsCheckBox;

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QString m_workingDir;

  QList<SVNStatus> m_status;
  QStringList m_files;

  QList<QString> m_filesToUpdate;
  QList<QString> m_filesWithConflict;

  QList<QString> m_sceneResources;

  bool m_updateToRevision;
  bool m_nonRecursive;

  QTextEdit *m_output;

  DateChooserWidget *m_dateChooserWidget;
  ConflictWidget *m_conflictWidget;

  int m_sceneIconsCount;

  // Used to choose to display "Get scene contents"
  // If there is some missing .tnz files don't display "Get Scene Contents"
  // checkbox.
  bool m_someSceneIsMissing;

private:
  void updateFiles();

  void checkFiles();

  void switchToCloseButton();

public:
  SVNUpdateDialog(QWidget *parent, const QString &workingDir,
                  const QStringList &filesToUpdate, int sceneIconsCount,
                  bool isFolderOnly, bool updateToRevision, bool nonRecursive);

protected slots:

  void onError(const QString &);

  void onUpdateDone(const QString &);

  void onConflictSetted();

  void onUpdateToMineDone();
  void onConflictResolved();

  // Only for update to revision
  void onUpdateToRevisionButtonClicked();

  void onUpdateButtonClicked();

  void addOutputText(const QString &text);

  void onStatusRetrieved(const QString &);

  void onUpdateSceneContentsToggled(bool);

signals:
  void done(const QStringList &);
};

#endif  // SVN_REVERT_DIALOG_H
