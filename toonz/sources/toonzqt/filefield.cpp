

#include "toonzqt/filefield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/lineedit.h"
#include "tfilepath.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>

using namespace DVGui;

FileField::BrowserPopupController *FileField::m_browserPopupController = 0;

//=============================================================================
// FileField
//-----------------------------------------------------------------------------

FileField::FileField(QWidget *parent, QString path, bool readOnly,
                     bool doNotBrowseInitialPath, bool codePath)
    : QWidget(parent)
    , m_filters(QStringList())
    , m_fileMode(QFileDialog::DirectoryOnly)
    , m_lastSelectedPath(path)
    , m_codePath(codePath) {
  setMaximumHeight(WidgetHeight);

  m_field            = new LineEdit(path);
  m_fileBrowseButton = new QPushButton(tr("..."));

  m_field->setReadOnly(readOnly);

  m_fileBrowseButton->setMinimumSize(20, WidgetHeight);
  m_fileBrowseButton->setObjectName("PushButton_NoPadding");

  // if the initial text is not path, set the string here and prevent browsing
  if (doNotBrowseInitialPath) m_descriptionText = path;

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(1);
  {
    mainLayout->addWidget(m_field, 5);
    mainLayout->addWidget(m_fileBrowseButton, 1);
  }
  setLayout(mainLayout);

  //--- signal-slot connections
  if (!readOnly)
    connect(m_field, SIGNAL(editingFinished()), SIGNAL(pathChanged()));
  connect(m_fileBrowseButton, SIGNAL(pressed()), this, SLOT(browseDirectory()));
}

//-----------------------------------------------------------------------------

void FileField::setFileMode(const QFileDialog::FileMode &fileMode) {
  m_fileMode = fileMode;
}

//-----------------------------------------------------------------------------

void FileField::setFilters(const QStringList &filters) { m_filters = filters; }

//-----------------------------------------------------------------------------

QString FileField::getPath() { return m_field->text(); }

//-----------------------------------------------------------------------------

void FileField::setPath(const QString &path) {
  m_field->setText(path);
  m_lastSelectedPath = path;
}

//-----------------------------------------------------------------------------

void FileField::setBrowserPopupController(BrowserPopupController *controller) {
  m_browserPopupController = controller;
}

//-----------------------------------------------------------------------------

FileField::BrowserPopupController *FileField::getBrowserPopupController() {
  return m_browserPopupController;
}

//-----------------------------------------------------------------------------

void FileField::browseDirectory() {
  if (!m_fileBrowseButton->hasFocus()) return;
  QString directory = QString();

  if (!m_browserPopupController) return;
  m_browserPopupController->openPopup(
      m_filters, (m_fileMode == QFileDialog::DirectoryOnly),
      (m_lastSelectedPath == m_descriptionText) ? "" : m_lastSelectedPath,
      this);
  if (m_browserPopupController->isExecute())
    directory = m_browserPopupController->getPath(m_codePath);

  if (!directory.isEmpty()) {
    setPath(directory);
    m_lastSelectedPath = directory;
    emit pathChanged();
    return;
  }
}
