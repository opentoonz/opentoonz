#pragma once

#ifndef SVN_LOCK_FRAME_RANGE_DIALOG_H
#define SVN_LOCK_FRAME_RANGE_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "versioncontrol.h"

#include <QList>

class QLabel;
class QPushButton;
class QPlainTextEdit;

//-----------------------------------------------------------------------------

class SVNLockFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QLabel *m_fromLabel;
  QLabel *m_toLabel;
  DVGui::IntLineEdit *m_fromLineEdit;
  DVGui::IntLineEdit *m_toLineEdit;

  QPushButton *m_lockButton;
  QPushButton *m_cancelButton;

  QString m_workingDir;
  QString m_file;

  QString m_propertyValue;

  VersionControlThread m_thread;

  bool m_hasError;

  bool m_fromIsValid;
  bool m_toIsValid;

  QList<SVNPartialLockInfo> m_lockInfos;

public:
  SVNLockFrameRangeDialog(QWidget *parent, const QString &workingDir,
                          const QString &file, int frameCount);

private:
  void switchToCloseButton();

protected slots:
  void onError(const QString &);
  void onPropGetDone(const QString &);
  void onPropSetDone();

  void onLockButtonClicked();
  void onCancelButtonClicked();

  void onLockDone();

  void finish();

  void onFromLineEditTextChanged();
  void onToLineEditTextChanged();

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNLockMultiFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QLabel *m_commentLabel;
  QPlainTextEdit *m_commentTextEdit;

  QLabel *m_fromLabel;
  QLabel *m_toLabel;
  DVGui::IntLineEdit *m_fromLineEdit;
  DVGui::IntLineEdit *m_toLineEdit;

  QPushButton *m_lockButton;
  QPushButton *m_cancelButton;

  QString m_workingDir;
  QStringList m_files;
  QStringList m_filesToLock;

  VersionControlThread m_thread;

  bool m_hasError;

  bool m_fromIsValid;
  bool m_toIsValid;

  QList<SVNStatus> m_status;
  QList<SVNPartialLockInfo> m_lockInfos;

public:
  SVNLockMultiFrameRangeDialog(QWidget *parent, const QString &workingDir,
                               const QStringList &files);

private:
  void switchToCloseButton();

protected slots:
  void onError(const QString &);

  void onLockButtonClicked();

  void onStatusRetrieved(const QString &);

  void onFromLineEditTextChanged();
  void onToLineEditTextChanged();

  void onLockDone();

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNUnlockFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QPushButton *m_unlockButton;
  QPushButton *m_cancelButton;

  QString m_workingDir;
  QString m_file;

  QString m_propertyValue;

  VersionControlThread m_thread;

  bool m_hasError;

  QList<SVNPartialLockInfo> m_lockInfos;
  SVNPartialLockInfo m_myInfo;

public:
  SVNUnlockFrameRangeDialog(QWidget *parent, const QString &workingDir,
                            const QString &file);

private:
  void switchToCloseButton();

protected slots:
  void onError(const QString &);
  void onPropGetDone(const QString &);
  void onPropSetDone();

  void onUnlockButtonClicked();

  void onCommitDone();
  void onUpdateDone();
  void onLockDone();

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNUnlockMultiFrameRangeDialog final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  QPushButton *m_unlockButton;
  QPushButton *m_cancelButton;

  QString m_workingDir;
  QStringList m_files;

  VersionControlThread m_thread;

  bool m_hasError;

  QList<SVNStatus> m_status;
  QStringList m_filesToUnlock;

public:
  SVNUnlockMultiFrameRangeDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files);

private:
  void switchToCloseButton();

protected slots:
  void onError(const QString &);

  void onStatusRetrieved(const QString &);

  void onUnlockButtonClicked();

  void onUnlockDone();

signals:
  void done(const QStringList &);
};

//-----------------------------------------------------------------------------

class SVNFrameRangeLockInfoDialog final : public DVGui::Dialog {
  Q_OBJECT

  QString m_workingDir;
  QString m_file;

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  VersionControlThread m_thread;

public:
  SVNFrameRangeLockInfoDialog(QWidget *parent, const QString &workingDir,
                              const QString &file);

protected slots:
  void onError(const QString &);
  void onPropGetDone(const QString &);
};

//-----------------------------------------------------------------------------

class SVNMultiFrameRangeLockInfoDialog final : public DVGui::Dialog {
  Q_OBJECT

  QString m_workingDir;
  QStringList m_files;

  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  VersionControlThread m_thread;

public:
  SVNMultiFrameRangeLockInfoDialog(QWidget *parent, const QString &workingDir,
                                   const QStringList &files);

protected slots:
  void onError(const QString &);
  void onStatusRetrieved(const QString &);
};

#endif  // SVN_LOCK_FRAME_RANGE_DIALOG_H
