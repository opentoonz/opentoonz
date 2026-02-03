#pragma once

#ifndef SVN_CLEANUP_DIALOG_H
#define SVN_CLEANUP_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

class QPushButton;
class QLabel;

//-----------------------------------------------------------------------------

class SVNCleanupDialog final : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_closeButton;
  QLabel *m_waitingLabel;
  QLabel *m_textLabel;

  VersionControlThread m_thread;

public:
  SVNCleanupDialog(QWidget *parent, const QString &workingDir);

protected slots:
  void onCleanupDone();
  void onError(const QString &);
};

#endif  // SVN_CLEANUP_DIALOG_H
