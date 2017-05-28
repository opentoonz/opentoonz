#pragma once

#ifndef CONVERTPOPUP_H
#define CONVERTPOPUP_H

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

class Convert2Tlv;

class QCheckBox;
class QLabel;
class QComboBox;

namespace DVGui {
class IntLineEdit;
class FileField;
class LineEdit;
class ColorField;
class ProgressDialog;
class CheckBox;
class DoubleLineEdit;
}

namespace ImageUtils {
class FrameTaskNotifier;
}

//==============================================================

//*****************************************************************************
//    ConvertPopup  declaration
//*****************************************************************************

/*!
  \brief    Window class used in the convertion of levels between different
            file formats.
*/

class ConvertPopup : public DVGui::Dialog {
  Q_OBJECT

public:
  ConvertPopup(bool specifyInput = false);
  ~ConvertPopup();

  void setFiles(const std::vector<TFilePath> &fps);
  bool isConverting() const { return m_isConverting; }

  void convertToTlv(bool toPainted);

  QString getDestinationType() const;
  QString getTlvMode() const;
  QString TlvMode_Unpainted;
  /*- 塗られていないカラー二値画像からTLVへの変換 -*/
  QString TlvMode_UnpaintedFromNonAA;
  QString TlvMode_PaintedFromTwoImages;
  QString TlvMode_PaintedFromNonAA;
  QString SameAsPainted;
  QString CreateNewPalette;

public slots:

  void apply();  //!< Starts the convertion.
  void onOptionsClicked();
  void onConvertFinished();
  void onTlvModeSelected(const QString &tlvMode);
  void onFormatSelected(const QString &format);
  void onAntialiasSelected(int index);
  void onFileInChanged();
  void onRangeChanged();
  void onLevelConverted(const TFilePath &fullPath);

  void onFormatChanged(const QString &);
  void onPalettePathChanged();

  void onDpiModeSelected(int index);

protected:
  Convert2Tlv *makeTlvConverter(const TFilePath &sourceFilePath);
  bool checkParameters() const;
  TPalette *readUserProvidedPalette() const;
  TFilePath getDestinationFilePath(const TFilePath &sourceFilePath);
  void getFrameRange(const TFilePath &sourceFilePath, TFrameId &from,
                     TFrameId &to);

private:
  DVGui::FileField *m_saveInFileFld, *m_convertFileFld, *m_unpaintedFolder,
      *m_palettePath;
  DVGui::IntLineEdit *m_fromFld, *m_toFld, *m_antialiasIntensity, *m_tolerance;
  DVGui::LineEdit *m_fileNameFld, *m_unpaintedSuffix;
  DVGui::ColorField *m_bgColorField;
  QFrame *m_tlvFrame;
  QCheckBox *m_applyAutoclose, *m_removeDotBeforeFrameNumber,
      *m_saveBackupToNopaint, *m_appendDefaultPalette, *m_removeUnusedStyles;
  DVGui::CheckBox *m_skip;
  QComboBox *m_antialias, *m_tlvMode, *m_fileFormat;
  QLabel *m_bgColorLabel, *m_suffixLabel, *m_unpaintedFolderLabel,
      *m_antialiasLabel;

  QPushButton *m_okBtn, *m_cancelBtn, *m_formatOptions;

  QComboBox *m_dpiMode;
  DVGui::DoubleLineEdit *m_dpiFld;
  double m_imageDpi;

  class Converter;
  Converter *m_converter;

  ImageUtils::FrameTaskNotifier *m_notifier;
  DVGui::ProgressDialog *m_progressDialog;

  std::vector<TFilePath> m_srcFilePaths;
  static QMap<std::string, TPropertyGroup *> m_formatProperties;

  bool m_isConverting;

private:
  TPropertyGroup *getFormatProperties(const std::string &ext);
  QFrame *createTlvSettings();
  QFrame *createSvgSettings();

  bool isSaveTlvBackupToNopaintActive();
};

class ConvertPopupWithInput final : public ConvertPopup {
public:
  ConvertPopupWithInput() : ConvertPopup(true) {}
};

#endif  // CONVERTPOPUP_H
