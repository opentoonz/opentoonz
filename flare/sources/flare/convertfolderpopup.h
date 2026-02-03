#pragma once

#ifndef CONVERTFOLDERPOPUP_H
#define CONVERTFOLDERPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QMap>
#include <QThread>
#include <QTranslator>

//==============================================================

//    Forward declarations

class TPalette;
class TPropertyGroup;
class ToonzScene;

class QCheckBox;
class QLabel;
class QListWidget;

namespace DVGui {
class FileField;
class ProgressDialog;
class CheckBox;
}  // namespace DVGui

namespace ImageUtils {
class FrameTaskNotifier;
}

//==============================================================

//*****************************************************************************
//    ConvertPopup  declaration
//*****************************************************************************

/*!
  \brief    Window class used in the conversion of levels between different
            file formats.
*/

class ConvertFolderPopup : public DVGui::Dialog {
  Q_OBJECT

public:
  ConvertFolderPopup();
  ~ConvertFolderPopup();

  void setFiles();
  bool isConverting() const { return m_isConverting; }

public slots:

  void apply();  //!< Starts the conversion.
  void onConvertFinished();
  void onLevelConverted(const TFilePath& fullPath);
  void onFileInFolderChanged();
  void onSkipChanged();
  void onSubfolderChanged();

protected:
  TFilePath getDestinationFilePath(const TFilePath& sourceFilePath);
  void getFrameRange(const TFilePath& sourceFilePath, TFrameId& from,
                     TFrameId& to);

  void showEvent(QShowEvent* e) override { setFiles(); }

private:
  DVGui::FileField* m_convertFolderFld;
  DVGui::CheckBox *m_skip, *m_subfolder;

  QPushButton *m_okBtn, *m_cancelBtn;

  QListWidget* m_srcFileList;

  class Converter;
  Converter* m_converter;

  ImageUtils::FrameTaskNotifier* m_notifier;
  DVGui::ProgressDialog* m_progressDialog;

  std::vector<TFilePath> m_srcFilePaths;

  bool m_isConverting;
};

class ConvertResultPopup : public QDialog {
  Q_OBJECT

  QString m_logTxt;
  TFilePath m_targetPath;

public:
  ConvertResultPopup(QString log, TFilePath path);
protected slots:
  void onSaveLog();
};

#endif  // CONVERTPOPUP_H
