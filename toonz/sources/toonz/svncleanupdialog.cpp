

#include "svncleanupdialog.h"

// Tnz6 includes
#include "tapp.h"

// Qt includes
#include <QPushButton>
#include <QMovie>
#include <QLabel>
#include <QMainWindow>

//=============================================================================
// SVNCleanupDialog
//-----------------------------------------------------------------------------

SVNCleanupDialog::SVNCleanupDialog(QWidget *parent, const QString &workingDir)
    : Dialog(TApp::instance()->getMainWindow(), true, false, "SvnCleanup") {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Cleanup"));

  setMinimumSize(300, 150);

  beginVLayout();

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel(tr("Cleaning up %1...").arg(workingDir));

  addWidgets(m_waitingLabel, m_textLabel);

  endVLayout();

  m_closeButton = new QPushButton(tr("Close"));
  connect(m_closeButton, SIGNAL(clicked()), this, SLOT(close()));
  m_closeButton->setEnabled(false);

  addButtonBarWidget(m_closeButton);

  // 0. Connect for svn errors (that may occurs everythings)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Cleanup
  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onCleanupDone()));
  QStringList args;
  args << "cleanup";
  m_thread.executeCommand(workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNCleanupDialog::onCleanupDone() {
  m_textLabel->setText(tr("Cleanup done."));
  m_waitingLabel->hide();
  m_closeButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SVNCleanupDialog::onError(const QString &text) {
  m_waitingLabel->hide();

  m_textLabel->setText(text);
  m_closeButton->setEnabled(true);
}

//-----------------------------------------------------------------------------
