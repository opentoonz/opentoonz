#pragma once

#ifndef FILEFIELD_H
#define FILEFIELD_H

#include "tcommon.h"
#include <QWidget>
#include <QFileDialog>
#include "tfilepath.h"
#include "toonzqt/lineedit.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QPushButton;

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \class DVGui::FileField
                \brief The FileField class provides an object to manage file
   browser.

                Inherits \b QWidget.

                The object FileField is composed of two part, a field \b
   LineEdit and a
                button \b QPushButton. Click button to open a directory browser
   popup
                that is used to choose a directory.
                You can set in constructor default path.
                Maximum height object is fixed to \b DVGui::WidgetHeight.
                By default dialog permit user to select only folder, but using
   setFileMode()
                you can indicate what the user may select in the file dialog,
   folder or file;
                you can also set file type using setFilters().
*/
class DVAPI FileField : public QWidget {
  Q_OBJECT

  LineEdit *m_field;
  QStringList m_filters;
  QFileDialog::FileMode m_fileMode;
  QString m_windowTitle;
  QString m_descriptionText;  // if the initial text is not path, set the string
                              // here and prevent browsing
  bool m_codePath;

protected:  // used in the child class for CleanupSettings
  QPushButton *m_fileBrowseButton;
  QString m_lastSelectedPath;

public:
  /* N.B. Vedi il commento della classe BrowserPopupController in
   * filebrowserpopup.cpp*/
  class BrowserPopupController {
  public:
    BrowserPopupController() {}
    virtual ~BrowserPopupController() {}
    virtual bool isExecute() { return true; };
    virtual QString getPath(bool codePath = true) { return QString(); };
    virtual void openPopup(QStringList, bool, QString,
                           const QWidget * = NULL){};
  };

  static BrowserPopupController *m_browserPopupController;

  FileField(QWidget *parent = 0, QString path = QString(),
            bool readOnly = false, bool doNotBrowseInitialPath = false,
            bool codePath = true);
  ~FileField() {}

  /*! Set what the user may select in the file dialog:
                  \li QFileDialog::DirectoryOnly show only directory.
                  \li QFileDialog::AnyFile, QFileDialog::ExistingFile,
     QFileDialog::Directory, QFileDialog::ExistingFiles
                                  show directory and file. */
  void setFileMode(const QFileDialog::FileMode &fileMode);
  /*! Set file type in dialog popup. */
  void setFilters(const QStringList &filters);
  void setValidator(const QValidator *v) { m_field->setValidator(v); }
  QString getPath();
  void setPath(const QString &path);

  static void setBrowserPopupController(BrowserPopupController *controller);
  static BrowserPopupController *getBrowserPopupController();

protected slots:
  /*! Open a static file dialog popup to browse and choose directories. If a
                  directory is seleceted and choose, set field to this
     directory. */
  // reimplemented in the "save in" filefield in CleanupSettings
  virtual void browseDirectory();

signals:
  /*!	This signal is emitted when path in field change, or by field edit or by
                  browse popup. */
  void pathChanged();
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // FILEFIELD_H
