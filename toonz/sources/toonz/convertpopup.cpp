

#include "convertpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "formatsettingspopups.h"
#include "filebrowser.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/doublefield.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tproject.h"
#include "toutputproperties.h"
#include "convert2tlv.h"
#include "toonz/preferences.h"
#include "toonz/tcamera.h"

// TnzCore includes
#include "tsystem.h"
#include "tfiletype.h"
#include "tlevel_io.h"
#include "tiio.h"
#include "tenv.h"

// Qt includes
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QMainWindow>
#include <QCheckBox>

/*-- Format --*/
TEnv::StringVar ConvertPopupFileFormat("ConvertPopupFileFormat", "tga");
/*-- 背景色 --*/
TEnv::IntVar ConvertPopupBgColorR("ConvertPopupBgColorR", 255);
TEnv::IntVar ConvertPopupBgColorG("ConvertPopupBgColorG", 255);
TEnv::IntVar ConvertPopupBgColorB("ConvertPopupBgColorB", 255);
TEnv::IntVar ConvertPopupBgColorA("ConvertPopupBgColorA", 255);
TEnv::IntVar ConvertPopupSkipExisting("ConvertPopupSkipExisting", 0);
/*-- フレーム番号の前のドットを取る --*/
TEnv::IntVar ConvertPopupRemoveDot("ConvertPopupRemoveDot", 1);
TEnv::IntVar ConvertPopupSaveToNopaint("ConvertPopupSaveToNopaint", 1);
TEnv::IntVar ConvertPopupAppendDefaultPalette(
    "ConvertPopupAppendDefaultPalette", 0);
TEnv::IntVar ConvertPopupRemoveUnusedStyles("ConvertPopupRemoveUnusedStyles",
                                            0);
// dpi settings for conversion to tlv
TEnv::IntVar ConvertPopupDpiMode("ConvertPopupDpiMode", 2);  // Custom DPI
TEnv::DoubleVar ConvertPopupDpiValue("ConvertPopupDpiValue", 72.0);

//=============================================================================
// convertPopup
//-----------------------------------------------------------------------------

QMap<std::string, TPropertyGroup *> ConvertPopup::m_formatProperties;

/*
 TRANSLATOR namespace::ConvertPopup
 */

// const QString CreateNewPalette(QObject::tr("Create new palette"));
const QString TlvExtension("tlv");
const QString OldLevelToTlvExtension("Old Level to TLV");
/*
  const QString TlvMode_Unpainted(QObject::tr("Unpainted tlv"));
  const QString TlvMode_PaintedFromTwoImages(QObject::tr("Painted tlv from two
  images"));
  const QString TlvMode_PaintedFromNonAA(QObject::tr("Painted tlv from non AA
  source"));


  const QString SameAsPainted(QObject::tr("Same as Painted"));
  */

//=============================================================================
//
// Converter
//
//-----------------------------------------------------------------------------

class ConvertPopup::Converter final : public QThread {
  int m_skippedCount;
  ConvertPopup *m_parent;

  bool m_saveToNopaintOnlyFlag;

public:
  Converter(ConvertPopup *parent) : m_parent(parent), m_skippedCount(0) {}
  void run() override;
  void convertLevel(const TFilePath &fp);
  void convertLevelWithConvert2Tlv(const TFilePath &fp);
  int getSkippedCount() const { return m_skippedCount; }
};

void ConvertPopup::Converter::run() {
  ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
  DVGui::ProgressDialog *progressDialog = m_parent->m_progressDialog;
  int levelCount                        = m_parent->m_srcFilePaths.size();
  TFilePath dstFolder(m_parent->m_saveInFileFld->getPath());

  for (int i = 0; !m_parent->m_notifier->abortTask() && i < levelCount; i++) {
    TFilePath sourceLevelPath = sc->decodeFilePath(m_parent->m_srcFilePaths[i]);
    QString levelName = QString::fromStdString(sourceLevelPath.getLevelName());

    // check already exsistent levels
    TFilePath dstFilePath =
        m_parent->getDestinationFilePath(m_parent->m_srcFilePaths[i]);

    // perche' ?? 		   TFilePath
    // path=dstFilePath.getParentDir()+(dstFilePath.getName()+"."+dstFilePath.getDottedType());
    //                   if(TSystem::doesExistFileOrLevel(dstFilePath)||TSystem::doesExistFileOrLevel(path))

    m_saveToNopaintOnlyFlag = false;

    if (TSystem::doesExistFileOrLevel(dstFilePath)) {
      if (m_parent->m_skip->isChecked()) {
        DVGui::info(tr("Level %1 already exists; skipped.").arg(levelName));
        m_skippedCount++;
        continue;
      } else {
        /*--- ここで、tlv形式に変換、かつ、UnpaintedTlvが選択され、かつ、
　　　nopaintにバックアップを保存するオプションがONのとき、
        出力先をnopaintに変更する。既にあるnopaint内のtlvを消す
---*/
        if (m_parent->isSaveTlvBackupToNopaintActive()) {
          TFilePath unpaintedLevelPath =
              dstFilePath.getParentDir() + "nopaint\\" +
              TFilePath(dstFilePath.getName() + "_np." + dstFilePath.getType());
          if (TSystem::doesExistFileOrLevel(unpaintedLevelPath)) {
            TSystem::removeFileOrLevel(unpaintedLevelPath);
            TSystem::deleteFile((unpaintedLevelPath.getParentDir() +
                                 unpaintedLevelPath.getName())
                                    .withType("tpl"));
          }
          m_saveToNopaintOnlyFlag = true;
        } else
          // todo: handle errors!!
          TSystem::removeFileOrLevel(dstFilePath);
      }
    }

    if (m_parent->m_srcFilePaths.size() == 1) {
      progressDialog->setLabelText(QString(tr("Converting %1").arg(levelName)));
    } else {
      progressDialog->setLabelText(QString(tr("Converting level %1 of %2: %3")
                                               .arg(i + 1)
                                               .arg(levelCount)
                                               .arg(levelName)));
    }
    m_parent->m_notifier->notifyFrameCompleted(0);
    convertLevel(sourceLevelPath);
  }
}

void ConvertPopup::Converter::convertLevel(
    const TFilePath &sourceFileFullPath) {
  ToonzScene *sc      = TApp::instance()->getCurrentScene()->getScene();
  ConvertPopup *popup = m_parent;

  QString levelName = QString::fromStdString(sourceFileFullPath.getLevelName());
  std::string ext   = popup->m_fileFormat->currentText().toStdString();
  TPropertyGroup *prop = popup->getFormatProperties(ext);

  TFilePath dstFileFullPath = popup->getDestinationFilePath(sourceFileFullPath);

  // bool isTlvNonAA = isTlv && popup->getTlvMode() == TlvMode_PaintedFromNonAA;
  bool isTlvNonAA =
      !(m_parent->getTlvMode().compare(m_parent->TlvMode_PaintedFromNonAA));

  TFrameId from, to;

  if (TFileType::isLevelFilePath(sourceFileFullPath)) {
    popup->getFrameRange(sourceFileFullPath, from, to);

    if (from == TFrameId() || to == TFrameId()) {
      DVGui::warning(tr("Level %1 has no frame; skipped.").arg(levelName));
      popup->m_notifier->notifyError();

      return;
    }
  }

  TOutputProperties *oprop = TApp::instance()
                                 ->getCurrentScene()
                                 ->getScene()
                                 ->getProperties()
                                 ->getOutputProperties();
  double framerate = oprop->getFrameRate();

  if (popup->m_fileFormat->currentText() == TlvExtension) {
    // convert to TLV
    if (!(m_parent->getTlvMode().compare(m_parent->TlvMode_PaintedFromNonAA))) {
      // no AA source (retas)
      TPaletteP palette = popup->readUserProvidedPalette();
      ImageUtils::convertNaa2Tlv(sourceFileFullPath, dstFileFullPath, from, to,
                                 m_parent->m_notifier, palette.getPointer(),
                                 m_parent->m_removeUnusedStyles->isChecked(),
                                 m_parent->m_dpiFld->getValue());
    } else {
      convertLevelWithConvert2Tlv(sourceFileFullPath);
    }
  } else if (popup->m_fileFormat->currentText() == OldLevelToTlvExtension) {
    // convert old levels (tzp/tzu) to tlv
    ImageUtils::convertOldLevel2Tlv(sourceFileFullPath, dstFileFullPath, from,
                                    to, m_parent->m_notifier);
  } else {
    // convert to full-color
    TPixel32 bgColor = m_parent->m_bgColorField->getColor();
    ImageUtils::convert(sourceFileFullPath, dstFileFullPath, from, to,
                        framerate, prop, m_parent->m_notifier, bgColor,
                        m_parent->m_removeDotBeforeFrameNumber->isChecked());
  }

  popup->m_notifier->notifyLevelCompleted(dstFileFullPath);
}

// to use legacy code
void ConvertPopup::Converter::convertLevelWithConvert2Tlv(
    const TFilePath &sourceFileFullPath) {
  ConvertPopup *popup       = m_parent;
  Convert2Tlv *tlvConverter = popup->makeTlvConverter(sourceFileFullPath);

  TFilePath levelOut = tlvConverter->m_levelOut;

  if (m_saveToNopaintOnlyFlag) {
    TFilePath unpaintedLevelPath =
        tlvConverter->m_levelOut.getParentDir() + "nopaint\\" +
        TFilePath(tlvConverter->m_levelOut.getName() + "_np." +
                  tlvConverter->m_levelOut.getType());
    tlvConverter->m_levelOut = unpaintedLevelPath;
  }

  std::string errorMessage;
  if (!tlvConverter->init(errorMessage)) {
    DVGui::warning(QString::fromStdString(errorMessage));
    tlvConverter->abort();
  } else {
    int count = tlvConverter->getFramesToConvertCount();
    bool stop = false;
    for (int j = 0; j < count && !stop; j++) {
      if (!tlvConverter->convertNext(errorMessage)) {
        stop = true;
        DVGui::warning(QString::fromStdString(errorMessage));
      }
      if (popup->m_progressDialog->wasCanceled()) stop = true;
    }
    if (stop) tlvConverter->abort();

    delete tlvConverter;
  }

  /*--- nopaintファイルを複製する ---*/
  if (m_parent->isSaveTlvBackupToNopaintActive() && !m_saveToNopaintOnlyFlag) {
    /*--- nopaintフォルダの作成 ---*/
    TFilePath nopaintDir = levelOut.getParentDir() + "nopaint";
    if (!TFileStatus(nopaintDir).doesExist()) {
      try {
        TSystem::mkDir(nopaintDir);
      } catch (...) {
        return;
      }
    }

    TFilePath unpaintedLevelPath =
        levelOut.getParentDir() + "nopaint\\" +
        TFilePath(levelOut.getName() + "_np." + levelOut.getType());

    /*--- もしnopaintのファイルがあった場合は消去する ---*/
    if (TSystem::doesExistFileOrLevel(unpaintedLevelPath)) {
      TSystem::removeFileOrLevel(unpaintedLevelPath);
      TSystem::deleteFile(
          (unpaintedLevelPath.getParentDir() + unpaintedLevelPath.getName())
              .withType("tpl"));
    }

    TSystem::copyFile(unpaintedLevelPath, levelOut);

    /*--- Paletteのコピー ---*/
    TFilePath levelPalettePath =
        levelOut.getParentDir() + TFilePath(levelOut.getName() + ".tpl");

    TFilePath unpaintedLevelPalettePath =
        levelPalettePath.getParentDir() + "nopaint\\" +
        TFilePath(levelPalettePath.getName() + "_np." +
                  levelPalettePath.getType());

    TSystem::copyFile(unpaintedLevelPalettePath, levelPalettePath);
  }

  /*
    QApplication::restoreOverrideCursor();
    FileBrowser::refreshFolder(m_srcFilePaths[0].getParentDir());
    TFilePath::setUnderscoreFormatAllowed(false);

        TFilePath levelOut(converters[i]->m_levelOut);
delete converters[i];
IconGenerator::instance()->invalidate(levelOut);
*/
}

//=============================================================================

ConvertPopup::ConvertPopup(bool specifyInput)
    : Dialog(TApp::instance()->getMainWindow(), true, false, "Convert")
    , m_converter(0)
    , m_isConverting(false) {
  // Su MAC i dialog modali non hanno bottoni di chiusura nella titleBar
  setModal(false);
  TlvMode_Unpainted            = tr("Unpainted tlv");
  TlvMode_UnpaintedFromNonAA   = tr("Unpainted tlv from non AA source");
  TlvMode_PaintedFromTwoImages = tr("Painted tlv from two images");
  TlvMode_PaintedFromNonAA     = tr("Painted tlv from non AA source");

  SameAsPainted    = tr("Same as Painted");
  CreateNewPalette = tr("Create new palette");

  m_fromFld       = new DVGui::IntLineEdit(this);
  m_toFld         = new DVGui::IntLineEdit(this);
  m_saveInFileFld = new DVGui::FileField(0, QString(""));
  m_fileNameFld   = new DVGui::LineEdit(QString(""));
  m_fileFormat    = new QComboBox();
  m_formatOptions = new QPushButton(tr("Options"), this);
  m_bgColorField =
      new DVGui::ColorField(this, true, TPixel32::Transparent, 35, false);

  m_okBtn     = new QPushButton(tr("Convert"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);

  m_bgColorLabel = new QLabel(tr("Bg Color:"));
  m_tlvFrame     = createTlvSettings();
  m_notifier     = new ImageUtils::FrameTaskNotifier();

  m_progressDialog = new DVGui::ProgressDialog("", tr("Cancel"), 0, 0);
  m_skip           = new DVGui::CheckBox(tr("Skip Existing Files"), this);

  m_removeDotBeforeFrameNumber =
      new QCheckBox(tr("Remove dot before frame number"), this);

  if (specifyInput)
    m_convertFileFld = new DVGui::FileField(0, QString(""), true);
  else
    m_convertFileFld = 0;

  //-----------------------
  // File Format
  QStringList formats;
  TImageWriter::getSupportedFormats(formats, true);
  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);
  m_fileFormat->addItems(formats);
  m_fileFormat->setMaxVisibleItems(15);
  m_skip->setChecked(true);
  m_okBtn->setDefault(true);
  if (specifyInput) {
    m_convertFileFld->setFileMode(QFileDialog::ExistingFile);
    QStringList filter;
    filter << "tif"
           << "tiff"
           << "png"
           << "tga"
           << "tlv"
           << "gif"
           << "bmp"
           << "avi"
           << "mov"
           << "jpg"
           << "pic"
           << "pict"
           << "rgb"
           << "sgi"
           << "png";
    m_convertFileFld->setFilters(filter);
    m_okBtn->setEnabled(false);
  }

  m_removeDotBeforeFrameNumber->setChecked(false);

  m_progressDialog->setWindowTitle(tr("Convert... "));
  m_progressDialog->setWindowFlags(
      Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
  m_progressDialog->setWindowModality(Qt::WindowModal);

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(5);
  {
    QGridLayout *upperLay = new QGridLayout();
    upperLay->setMargin(0);
    upperLay->setSpacing(5);
    {
      int row = 0;
      if (specifyInput) {
        upperLay->addWidget(new QLabel(tr("File to convert:"), this), row, 0);
        upperLay->addWidget(m_convertFileFld, row, 1, 1, 4);
        row++;
      }

      upperLay->addWidget(new QLabel(tr("Start:"), this), row, 0);
      upperLay->addWidget(m_fromFld, row, 1);
      upperLay->addWidget(new QLabel(tr("  End:"), this), row, 2);
      upperLay->addWidget(m_toFld, row, 3);
      row++;

      upperLay->addWidget(new QLabel(tr("Save in:"), this), row, 0);
      upperLay->addWidget(m_saveInFileFld, row, 1, 1, 4);
      row++;

      upperLay->addWidget(new QLabel(tr("File Name:"), this), row, 0);
      upperLay->addWidget(m_fileNameFld, row, 1, 1, 4);
      ;
      row++;

      upperLay->addWidget(new QLabel(tr("File Format:"), this), row, 0);
      upperLay->addWidget(m_fileFormat, row, 1, 1, 2);
      upperLay->addWidget(m_formatOptions, row, 3);
      ;
      row++;

      upperLay->addWidget(m_bgColorLabel, row, 0);
      upperLay->addWidget(m_bgColorField, row, 1, 1, 4);
      ;
      row++;

      upperLay->addWidget(m_skip, row, 1, 1, 4);
      ;
      row++;

      upperLay->addWidget(m_removeDotBeforeFrameNumber, row, 1, 1, 4);
    }
    upperLay->setColumnStretch(0, 0);
    upperLay->setColumnStretch(1, 1);
    upperLay->setColumnStretch(2, 0);
    upperLay->setColumnStretch(3, 1);
    upperLay->setColumnStretch(4, 1);
    m_topLayout->addLayout(upperLay);

    m_topLayout->addWidget(m_tlvFrame);
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  {
    m_buttonLayout->addWidget(m_okBtn);
    m_buttonLayout->addWidget(m_cancelBtn);
  }

  int formatIndex = m_fileFormat->findText(
      QString::fromStdString(ConvertPopupFileFormat.getValue()));
  if (formatIndex)
    m_fileFormat->setCurrentIndex(formatIndex);
  else
    m_fileFormat->setCurrentIndex(m_fileFormat->findText("tif"));
  m_bgColorField->setColor(TPixel32(ConvertPopupBgColorR, ConvertPopupBgColorG,
                                    ConvertPopupBgColorB,
                                    ConvertPopupBgColorA));
  m_skip->setChecked(ConvertPopupSkipExisting != 0);
  m_removeDotBeforeFrameNumber->setChecked(ConvertPopupRemoveDot != 0);
  m_saveBackupToNopaint->setChecked(ConvertPopupSaveToNopaint != 0);
  m_appendDefaultPalette->setChecked(ConvertPopupAppendDefaultPalette != 0);
  m_removeUnusedStyles->setChecked(ConvertPopupRemoveUnusedStyles != 0);

  //--- signal-slot connections
  qRegisterMetaType<TFilePath>("TFilePath");

  bool ret = true;
  ret = ret && connect(m_tlvMode, SIGNAL(currentIndexChanged(const QString &)),
                       this, SLOT(onTlvModeSelected(const QString &)));
  ret = ret && connect(m_fromFld, SIGNAL(editingFinished()), this,
                       SLOT(onRangeChanged()));
  ret = ret && connect(m_toFld, SIGNAL(editingFinished()), this,
                       SLOT(onRangeChanged()));
  ret =
      ret && connect(m_fileFormat, SIGNAL(currentIndexChanged(const QString &)),
                     this, SLOT(onFormatSelected(const QString &)));
  ret = ret && connect(m_formatOptions, SIGNAL(clicked()), this,
                       SLOT(onOptionsClicked()));
  ret = ret && connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(m_notifier, SIGNAL(frameCompleted(int)),
                       m_progressDialog, SLOT(setValue(int)));
  ret = ret && connect(m_notifier, SIGNAL(levelCompleted(const TFilePath &)),
                       this, SLOT(onLevelConverted(const TFilePath &)));
  ret = ret && connect(m_progressDialog, SIGNAL(canceled()), m_notifier,
                       SLOT(onCancelTask()));

  if (specifyInput)
    ret = ret && connect(m_convertFileFld, SIGNAL(pathChanged()), this,
                         SLOT(onFileInChanged()));

  // update unable/enable of checkboxes
  onTlvModeSelected(m_tlvMode->currentText());

  assert(ret);
}

//------------------------------------------------------------------

ConvertPopup::~ConvertPopup() {
  delete m_notifier;
  delete m_progressDialog;
  delete m_converter;
}

//-----------------------------------------------------------------------------

QString ConvertPopup::getDestinationType() const {
  return m_fileFormat->currentText();
}

//-----------------------------------------------------------------------------

QString ConvertPopup::getTlvMode() const { return m_tlvMode->currentText(); }

//-----------------------------------------------------------------------------

QFrame *ConvertPopup::createSvgSettings() {
  bool ret = true;
  QHBoxLayout *hLayout;
  QFrame *frame        = new QFrame();
  QVBoxLayout *vLayout = new QVBoxLayout();
  frame->setLayout(vLayout);

  hLayout = new QHBoxLayout();
  hLayout->addWidget(new QLabel(tr("Stroke Mode:")));

  hLayout->addWidget(m_tlvMode = new QComboBox());

  QStringList items;
  items << tr("Centerline") << tr("Outline");
  m_tlvMode->addItems(items);

  hLayout->addStretch();
  vLayout->addLayout(hLayout);
  frame->setVisible(false);
  return frame;
}

QFrame *ConvertPopup::createTlvSettings() {
  QFrame *frame = new QFrame();
  frame->setObjectName("SolidLineFrame");

  m_tlvMode              = new QComboBox();
  m_unpaintedFolderLabel = new QLabel(tr("Unpainted File Folder:"));
  m_unpaintedFolder =
      new DVGui::FileField(0, QString(tr("Same as Painted")), true, true);
  m_suffixLabel     = new QLabel(tr(" Unpainted File Suffix:"));
  m_unpaintedSuffix = new DVGui::LineEdit("_np");
  m_applyAutoclose  = new QCheckBox(tr("Apply Autoclose"));
  m_saveBackupToNopaint =
      new QCheckBox(tr("Save Backup to \"nopaint\" Folder"));
  m_appendDefaultPalette = new QCheckBox(tr("Append Default Palette"));
  m_antialias            = new QComboBox();
  m_antialiasIntensity   = new DVGui::IntLineEdit(0, 50, 0, 100);
  m_palettePath =
      new DVGui::FileField(0, QString(CreateNewPalette), true, true);
  m_tolerance = new DVGui::IntLineEdit(0, 0, 0, 255);

  m_removeUnusedStyles =
      new QCheckBox(tr("Remove Unused Styles from Input Palette"));

  m_dpiMode = new QComboBox();
  m_dpiFld  = new DVGui::DoubleLineEdit();

  m_unpaintedFolder->setFileMode(QFileDialog::DirectoryOnly);
  m_unpaintedSuffix->setMaximumWidth(40);
  QStringList items1;
  items1 << tr("Keep Original Antialiasing")
         << tr("Add Antialiasing with Intensity:")
         << tr("Remove Antialiasing using Threshold:");
  m_antialias->addItems(items1);

  QStringList items;
  items << TlvMode_Unpainted << TlvMode_UnpaintedFromNonAA
        << TlvMode_PaintedFromTwoImages << TlvMode_PaintedFromNonAA;
  m_tlvMode->addItems(items);
  m_antialiasIntensity->setEnabled(false);

  m_appendDefaultPalette->setToolTip(
      tr("When activated, styles of the default "
         "palette\n($TOONZSTUDIOPALETTE\\cleanup_default.tpl) will \nbe "
         "appended to the palette after conversion in \norder to save the "
         "effort of creating styles \nbefore color designing."));

  m_palettePath->setMinimumWidth(300);
  m_palettePath->setFileMode(QFileDialog::ExistingFile);
  m_palettePath->setFilters(QStringList("tpl"));
  // m_tolerance->setMaximumWidth(10);

  QStringList dpiModes;
  dpiModes << tr("Image DPI") << tr("Current Camera DPI") << tr("Custom DPI");
  m_dpiMode->addItems(dpiModes);
  m_dpiMode->setToolTip(tr(
      "Specify the policy for setting DPI of converted tlv. \n"
      "If you select the \"Image DPI\" option and the source image does not \n"
      "contain the dpi information, then the current camera dpi will be "
      "used.\n"));
  m_dpiMode->setCurrentIndex(ConvertPopupDpiMode);
  m_dpiFld->setValue(ConvertPopupDpiValue);

  QGridLayout *gridLay = new QGridLayout();
  {
    gridLay->addWidget(new QLabel(tr("Mode:")), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_tlvMode, 0, 1);

    gridLay->addWidget(m_unpaintedFolderLabel, 1, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_unpaintedFolder, 1, 1);
    gridLay->addWidget(m_suffixLabel, 1, 2, Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_unpaintedSuffix, 1, 3);

    gridLay->addWidget(m_applyAutoclose, 2, 1, 1, 3);

    gridLay->addWidget(new QLabel(tr("Antialias:")), 3, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_antialias, 3, 1);
    gridLay->addWidget(m_antialiasIntensity, 3, 2);

    gridLay->addWidget(new QLabel(tr("Palette:")), 4, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_palettePath, 4, 1);
    gridLay->addWidget(new QLabel(tr("Tolerance:")), 4, 2,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_tolerance, 4, 3);

    gridLay->addWidget(m_removeUnusedStyles, 5, 1, 1, 3);
    gridLay->addWidget(m_appendDefaultPalette, 6, 1, 1, 3);
    gridLay->addWidget(m_saveBackupToNopaint, 7, 1, 1, 3);

    gridLay->addWidget(new QLabel(tr("Dpi:")), 8, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_dpiMode, 8, 1);
    gridLay->addWidget(m_dpiFld, 8, 2);
  }
  gridLay->setColumnStretch(0, 0);
  gridLay->setColumnStretch(1, 1);
  gridLay->setColumnStretch(2, 0);
  gridLay->setColumnStretch(3, 0);
  frame->setLayout(gridLay);

  bool ret = true;
  ret      = ret && connect(m_antialias, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onAntialiasSelected(int)));
  ret = ret && connect(m_palettePath, SIGNAL(pathChanged()), this,
                       SLOT(onPalettePathChanged()));
  ret = ret && connect(m_dpiMode, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onDpiModeSelected(int)));

  assert(ret);

  frame->setVisible(false);

  return frame;
}

//--------------------------------------------------------------------------

void ConvertPopup::onRangeChanged() {
  if (m_srcFilePaths.empty()) return;

  TLevelReaderP lr = TLevelReaderP(m_srcFilePaths[0]);
  if (lr) {
    TLevelP l        = lr->loadInfo();
    TLevel::Table *t = l->getTable();
    if (t->empty()) return;
    TFrameId start = t->begin()->first;
    TFrameId end   = t->rbegin()->first;
    if (m_toFld->getValue() > end.getNumber())
      m_toFld->setValue(end.getNumber() != -2 ? end.getNumber() : 1);
    if (m_fromFld->getValue() < start.getNumber())
      m_fromFld->setValue(start.getNumber());
  }
}

//-------------------------------------------------------------------

void ConvertPopup::onAntialiasSelected(int index) {
  m_antialiasIntensity->setEnabled(index != 0);
}

//-----------------------------------------------------------------

void ConvertPopup::onFileInChanged() {
  assert(m_convertFileFld);
  std::vector<TFilePath> fps;
  TProject *project =
      TProjectManager::instance()->getCurrentProject().getPointer();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  fps.push_back(scene->decodeFilePath(
      TFilePath(m_convertFileFld->getPath().toStdString())));

  setFiles(fps);
}

//-------------------------------------------------

void ConvertPopup::onTlvModeSelected(const QString &tlvMode) {
  bool usesTwoImages = TlvMode_PaintedFromTwoImages == tlvMode;
  m_unpaintedFolderLabel->setEnabled(usesTwoImages);
  m_unpaintedFolder->setEnabled(usesTwoImages);
  m_suffixLabel->setEnabled(usesTwoImages);
  m_unpaintedSuffix->setEnabled(usesTwoImages);
  m_antialias->setEnabled(TlvMode_PaintedFromNonAA != tlvMode);
  // m_palettePath->setEnabled(TlvMode_PaintedFromNonAA != tlvMode);
  m_tolerance->setEnabled(TlvMode_PaintedFromNonAA != tlvMode);
  m_appendDefaultPalette->setEnabled(TlvMode_PaintedFromNonAA != tlvMode);

  m_removeUnusedStyles->setEnabled(TlvMode_PaintedFromNonAA == tlvMode &&
                                   m_palettePath->getPath() !=
                                       CreateNewPalette);

  m_saveBackupToNopaint->setEnabled(TlvMode_Unpainted == tlvMode);
}

//-------------------------------------------------

void ConvertPopup::onFormatSelected(const QString &format) {
  onFormatChanged(format);

  bool isTlv          = format == TlvExtension;
  bool isOldLevel2Tlv = format == OldLevelToTlvExtension;
  bool isPli          = format == "svg";

  m_formatOptions->setVisible(!isTlv && !isOldLevel2Tlv);
  m_bgColorField->setVisible(!isTlv && !isPli && !isOldLevel2Tlv);
  m_bgColorLabel->setVisible(!isTlv && !isPli && !isOldLevel2Tlv);

  m_tlvFrame->setVisible(isTlv);
  // m_svgFrame->setVisible(isPli);

  if (isTlv) {
    bool isPainted = m_tlvMode->currentText() == TlvMode_PaintedFromTwoImages;
    m_unpaintedFolderLabel->setEnabled(isPainted);
    m_unpaintedFolder->setEnabled(isPainted);
    m_suffixLabel->setEnabled(isPainted);
    m_unpaintedSuffix->setEnabled(isPainted);

    m_saveBackupToNopaint->setEnabled(m_tlvMode->currentText() ==
                                      TlvMode_Unpainted);
  }
}

//------------------------------------------------------------------

void ConvertPopup::setFiles(const std::vector<TFilePath> &fps) {
  m_okBtn->setEnabled(true);
  m_fileFormat->setEnabled(true);
  m_fileFormat->removeItem(m_fileFormat->findText("svg"));
  m_fileFormat->removeItem(m_fileFormat->findText(OldLevelToTlvExtension));

  // true if the ALL files are full color raster
  bool areFullcolor = true;

  // true if AT LEAST ONE file is vector. Since the popup will never open if the
  // file selection contains both raster and vector file types, we can assume
  // ALL files are vector if areVector == true.
  bool areVector = false;
  // For the same logic as the above, true if all files are old levels
  // (tzp/tzu).
  bool areOldLevel = false;

  for (int i = 0; i < (int)fps.size(); i++) {
    TFileType::Type type = TFileType::getInfo(fps[i]);
    if (!TFileType::isFullColor(type)) {
      areFullcolor = false;
      if (TFileType::isVector(type))
        areVector = true;
      else if (fps[i].getType() == "tzp" || fps[i].getType() == "tzu")
        areOldLevel = true;
      break;
    }
  }

  int currIndex = m_fileFormat->currentIndex();
  int tlvIndex  = m_fileFormat->findText(TlvExtension);

  if (areFullcolor) {
    if (tlvIndex < 0) m_fileFormat->addItem(TlvExtension);

  } else if (areVector) {
    int svgIndex = m_fileFormat->findText("svg");
    if (svgIndex < 0) m_fileFormat->addItem("svg");
    m_fileFormat->setCurrentIndex(m_fileFormat->findText("svg"));
    m_fileFormat->setEnabled(false);
    onFormatSelected("svg");
  } else if (areOldLevel) {
    if (tlvIndex < 0) m_fileFormat->addItem(OldLevelToTlvExtension);
    m_fileFormat->setCurrentIndex(
        m_fileFormat->findText(OldLevelToTlvExtension));
    m_fileFormat->setEnabled(false);
    onFormatSelected(OldLevelToTlvExtension);
  } else {
    if (tlvIndex >= 0) {
      int index = m_fileFormat->currentIndex();
      m_fileFormat->removeItem(tlvIndex);
    }
  }

  m_srcFilePaths = fps;

  m_imageDpi = 0.0;

  if (m_srcFilePaths.size() == 1) {
    setWindowTitle(tr("Convert 1 Level"));
    m_fromFld->setEnabled(true);
    m_toFld->setEnabled(true);
    m_fileNameFld->setEnabled(true);

    m_fromFld->setText("");
    m_toFld->setText("");
    TLevelP levelTmp;
    TLevelReaderP lrTmp = TLevelReaderP(fps[0]);
    if (lrTmp) {
      levelTmp         = lrTmp->loadInfo();
      TLevel::Table *t = levelTmp->getTable();
      if (!t->empty()) {
        TFrameId start = t->begin()->first;
        TFrameId end   = t->rbegin()->first;
        if (start.getNumber() > 0)
          m_fromFld->setText(QString::number(start.getNumber()));
        if (end.getNumber() > 0)
          m_toFld->setText(QString::number(end.getNumber()));
      }

      // use the image dpi for the converted tlv
      const TImageInfo *ii = lrTmp->getImageInfo();
      if (ii)
        m_imageDpi = ii->m_dpix;  // for now, consider only the horizontal dpi
    }
    // m_fromFld->setText("1");
    // m_toFld->setText(QString::number(levelTmp->getFrameCount()==-2?1:levelTmp->getFrameCount()));

    m_fileNameFld->setText(QString::fromStdString(fps[0].getName()));
  } else {
    setWindowTitle(tr("Convert %1 Levels").arg(m_srcFilePaths.size()));
    m_fromFld->setText("");
    m_toFld->setText("");
    m_fileNameFld->setText("");
    m_fromFld->setEnabled(false);
    m_toFld->setEnabled(false);
    m_fileNameFld->setEnabled(false);
  }
  m_saveInFileFld->setPath(toQString(fps[0].getParentDir()));
  // if (m_unpaintedFolder->getPath()==SameAsPainted)
  //   m_unpaintedFolder->setPath(toQString(fps[0].getParentDir()));
  m_palettePath->setPath(CreateNewPalette);
  m_removeUnusedStyles->setEnabled(false);

  // update the image dpi field
  onDpiModeSelected(m_dpiMode->currentIndex());
  // m_fileFormat->setCurrentIndex(areFullcolor?0:m_fileFormat->findText("tif"));
}

//-------------------------------------------------------------------

Convert2Tlv *ConvertPopup::makeTlvConverter(const TFilePath &sourceFilePath) {
  ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
  TFilePath unpaintedfilePath;
  if (m_tlvMode->currentText() == TlvMode_PaintedFromTwoImages) {
    QString suffixString = m_unpaintedSuffix->text();

    TFilePath unpaintedFolder;
    if (m_unpaintedFolder->getPath() == SameAsPainted)
      unpaintedFolder = sourceFilePath.getParentDir();
    else
      unpaintedFolder = TFilePath(m_unpaintedFolder->getPath().toStdString());

    std::string basename =
        sourceFilePath.getName() + suffixString.toStdString();
    unpaintedfilePath = sc->decodeFilePath(
        sourceFilePath.withParentDir(unpaintedFolder).withName(basename));
  }
  int from = -1, to = -1;
  if (m_srcFilePaths.size() > 1) {
    from = m_fromFld->getValue();
    to   = m_toFld->getValue();
  }

  TFilePath palettePath;
  if (m_palettePath->getPath() != CreateNewPalette)
    palettePath =
        sc->decodeFilePath(TFilePath(m_palettePath->getPath().toStdString()));

  TFilePath fn = sc->decodeFilePath(sourceFilePath);
  TFilePath out =
      sc->decodeFilePath(TFilePath(m_saveInFileFld->getPath().toStdWString()));

  Convert2Tlv *converter = new Convert2Tlv(
      fn, unpaintedfilePath, out, m_fileNameFld->text(), from, to,
      m_applyAutoclose->isChecked(), palettePath, m_tolerance->getValue(),
      m_antialias->currentIndex(), m_antialiasIntensity->getValue(),
      getTlvMode() == TlvMode_UnpaintedFromNonAA,
      m_appendDefaultPalette->isChecked(), m_dpiFld->getValue());
  return converter;
}

//-------------------------------------------------------------------

void ConvertPopup::convertToTlv(bool toPainted) {
#ifdef CICCIO
  ToonzScene *sc   = TApp::instance()->getCurrentScene()->getScene();
  bool doAutoclose = false;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  int i, totFrames = 0, skipped = 0;

  TFilePath::setUnderscoreFormatAllowed(
      !m_unpaintedSuffix->text().contains('_'));

  std::vector<Convert2Tlv *> converters;
  for (i = 0; i < m_srcFilePaths.size(); i++) {
    Convert2Tlv *converter = makeTlvConverter(m_srcFilePaths[i]);
    if (TSystem::doesExistFileOrLevel(converter->m_levelOut)) {
      if (m_skip->isChecked()) {
        DVGui::info(
            QString(tr("Level ")) +
            QString::fromStdWString(
                converter->m_levelOut.withoutParentDir().getWideString()) +
            QString(tr(" already exists; skipped")));
        delete converter;
        skipped++;
        continue;
      } else
        TSystem::removeFileOrLevel(converter->m_levelOut);
    }
    totFrames += converter->getFramesToConvertCount();
    converters.push_back(converter);
  }

  if (converters.empty()) {
    QApplication::restoreOverrideCursor();
    TFilePath::setUnderscoreFormatAllowed(false);
    return;
  }

  ProgressDialog pb("", tr("Cancel"), 0, totFrames);
  int j, l, k = 0;
  for (i = 0; i < converters.size(); i++) {
    std::string errorMessage;
    if (!converters[i]->init(errorMessage)) {
      converters[i]->abort();
      DVGui::error(QString::fromStdString(errorMessage));
      delete converters[i];
      converters[i] = 0;
      skipped++;
      continue;
    }

    int count = converters[i]->getFramesToConvertCount();

    pb.setLabelText(tr("Generating level ") +
                    toQString(converters[i]->m_levelOut));
    pb.show();

    for (j = 0; j < count; j++) {
      std::string errorMessage = "";
      if (!converters[i]->convertNext(errorMessage) || pb.wasCanceled()) {
        for (l = i; l < converters.size(); l++) {
          converters[l]->abort();
          delete converters[i];
          converters[i] = 0;
        }
        if (errorMessage != "")
          DVGui::error(QString::fromStdString(errorMessage));
        QApplication::restoreOverrideCursor();
        FileBrowser::refreshFolder(m_srcFilePaths[0].getParentDir());
        TFilePath::setUnderscoreFormatAllowed(false);
        return;
      }
      pb.setValue(++k);
      DVGui::info(QString(tr("Level ")) +
                  QString::fromStdWString(
                      m_srcFilePaths[i].withoutParentDir().getWideString()) +
                  QString(tr(" converted to tlv.")));
    }
    TFilePath levelOut(converters[i]->m_levelOut);
    delete converters[i];
    IconGenerator::instance()->invalidate(levelOut);

    converters[i] = 0;
  }

  QApplication::restoreOverrideCursor();
  pb.hide();

  if (m_srcFilePaths.size() == 1) {
    if (skipped == 0)
      DVGui::info(
          tr("Level %1 converted to TLV Format")
              .arg(QString::fromStdWString(
                  m_srcFilePaths[0].withoutParentDir().getWideString())));
    else
      DVGui::warning(
          tr("Warning: Level %1 NOT converted to TLV Format")
              .arg(QString::fromStdWString(
                  m_srcFilePaths[0].withoutParentDir().getWideString())));
  } else
    DVGui::MsgBox(skipped == 0 ? DVGui::INFORMATION : DVGui::WARNING,
                  tr("Converted %1 out of %2 Levels to TLV Format")
                      .arg(QString::number(m_srcFilePaths.size() - skipped))
                      .arg(QString::number(m_srcFilePaths.size())));

  FileBrowser::refreshFolder(m_srcFilePaths[0].getParentDir());
  TFilePath::setUnderscoreFormatAllowed(false);
#endif
}

//-----------------------------------------------------------------------------

TFilePath ConvertPopup::getDestinationFilePath(
    const TFilePath &sourceFilePath) {
  // Build the DECODED output folder path
  TFilePath destFolder = sourceFilePath.getParentDir();

  if (!m_saveInFileFld->getPath().isEmpty()) {
    TFilePath dir(m_saveInFileFld->getPath().toStdWString());

    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    destFolder        = scene->decodeFilePath(dir);
  }

  // Build the output level name
  const QString &fldName = m_fileNameFld->text();
  const std::string &ext =
      (m_fileFormat->currentText() == OldLevelToTlvExtension)
          ? "tlv"
          : m_fileFormat->currentText().toStdString();
  const std::wstring &name =
      fldName.isEmpty() ? sourceFilePath.getWideName() : fldName.toStdWString();

  TFilePath destName = TFilePath(name).withType(ext);

  if (TFileType::isLevelFilePath(sourceFilePath) &&
      !TFileType::isLevelExtension(ext))
    destName = destName.withFrame(TFrameId::EMPTY_FRAME);  // use the '..'
                                                           // format to denote
                                                           // an output level

  // Merge the two
  return destFolder + destName;
}

//-----------------------------------------------------------------------------

TPalette *ConvertPopup::readUserProvidedPalette() const {
  if (m_palettePath->getPath() == CreateNewPalette ||
      m_fileFormat->currentText() != TlvExtension)
    return 0;

  ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
  TFilePath palettePath =
      sc->decodeFilePath(TFilePath(m_palettePath->getPath().toStdString()));
  TPalette *palette = 0;
  try {
    TIStream is(palettePath);
    is >> palette;
    // note! refCount==0
  } catch (...) {
    DVGui::warning(
        tr("Warning: Can't read palette '%1' ").arg(m_palettePath->getPath()));
    if (palette) {
      delete palette;
      palette = 0;
    }
  }
  return palette;
}

//-----------------------------------------------------------------------------

void ConvertPopup::getFrameRange(const TFilePath &sourceFilePath,
                                 TFrameId &from, TFrameId &to) {
  from = to = TFrameId();

  if (!TFileType::isLevelFilePath(sourceFilePath)) return;

  TLevelReaderP lr(sourceFilePath);
  if (!lr) return;

  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return;

  TFrameId firstFrame = from = level->begin()->first,
           lastFrame = to = (--level->end())->first;

  if (m_srcFilePaths.size() == 1) {
    // Just one level - consider from/to input fields, too
    TFrameId fid;
    bool ok;

    fid                        = TFrameId(m_fromFld->text().toInt(&ok));
    if (ok && fid > from) from = tcrop(fid, firstFrame, lastFrame);

    fid                    = TFrameId(m_toFld->text().toInt(&ok));
    if (ok && fid < to) to = tcrop(fid, firstFrame, lastFrame);
  }
}

//-----------------------------------------------------------------------------

bool ConvertPopup::checkParameters() const {
  if (m_srcFilePaths.size() == 1 && m_fileNameFld->text().isEmpty()) {
    DVGui::warning(
        tr("No output filename specified: please choose a valid level name."));
    return false;
  }

  if (m_fileFormat->currentText() == TlvExtension) {
    if (m_tlvMode->currentText() == TlvMode_PaintedFromTwoImages) {
      if (m_unpaintedSuffix->text() == "") {
        DVGui::warning(tr("No unpainted suffix specified: cannot convert."));
        return false;
      }
    }
  }

  if (m_palettePath->getPath() == CreateNewPalette ||
      m_fileFormat->currentText() != TlvExtension) {
    TPalette *palette = readUserProvidedPalette();
    delete palette;
    // note: don't keep a reference to the palette because that can not be used
    // by the Convert2Tlv class (that requires a TFilePath instead)
  }

  return true;
}

//--------------------------------------------------
void ConvertPopup::apply() {
  // if parameters are not ok do nothing and don't close the dialog
  if (!checkParameters()) return;

  // store the parameters in user env file
  ConvertPopupFileFormat    = m_fileFormat->currentText().toStdString();
  TPixel32 bgCol            = m_bgColorField->getColor();
  ConvertPopupBgColorR      = (int)bgCol.r;
  ConvertPopupBgColorG      = (int)bgCol.g;
  ConvertPopupBgColorB      = (int)bgCol.b;
  ConvertPopupBgColorA      = (int)bgCol.m;
  ConvertPopupSkipExisting  = m_skip->isChecked() ? 1 : 0;
  ConvertPopupRemoveDot     = m_removeDotBeforeFrameNumber->isChecked() ? 1 : 0;
  ConvertPopupSaveToNopaint = m_saveBackupToNopaint->isChecked() ? 1 : 0;
  ConvertPopupAppendDefaultPalette =
      m_appendDefaultPalette->isChecked() ? 1 : 0;
  ConvertPopupRemoveUnusedStyles = m_removeUnusedStyles->isChecked() ? 1 : 0;

  ConvertPopupDpiMode = m_dpiMode->currentIndex();
  if (m_dpiMode->currentIndex() == 2)  // In the case of Custom DPI
    ConvertPopupDpiValue = m_dpiFld->getValue();

  // parameters are ok: close the dialog first
  close();

  m_isConverting = true;
  m_progressDialog->show();
  m_notifier->reset();
  QApplication::setOverrideCursor(Qt::WaitCursor);

  m_converter = new Converter(this);
#if QT_VERSION >= 0x050000
  bool ret =
      connect(m_converter, SIGNAL(finished()), this, SLOT(onConvertFinished()));
#else
  int ret =
      connect(m_converter, SIGNAL(finished()), this, SLOT(onConvertFinished()));
#endif
  Q_ASSERT(ret);

  // TODO: salvare il vecchio stato
  if (m_fileFormat->currentText() == TlvExtension &&
      m_tlvMode->currentText() == TlvMode_PaintedFromTwoImages) {
    if (!m_unpaintedSuffix->text().contains('_'))
      TFilePath::setUnderscoreFormatAllowed(true);
  }

  // start converting. Conversion end is handled by onConvertFinished() slot
  m_converter->start();
}

//------------------------------------------------------------------

void ConvertPopup::onConvertFinished() {
  m_isConverting = false;
  TFilePath dstFilePath(m_saveInFileFld->getPath().toStdWString());
  if (dstFilePath == m_srcFilePaths[0].getParentDir())
    FileBrowser::refreshFolder(dstFilePath);

  m_progressDialog->close();
  QApplication::restoreOverrideCursor();

  int errorCount   = m_notifier->getErrorCount();
  int skippedCount = m_converter->getSkippedCount();
  if (errorCount > 0) {
    if (skippedCount > 0)
      DVGui::error(
          tr("Convert completed with %1 error(s) and %2 level(s) skipped")
              .arg(errorCount)
              .arg(skippedCount));
    else
      DVGui::error(tr("Convert completed with %1 error(s) ").arg(errorCount));
  } else if (skippedCount > 0) {
    DVGui::warning(tr("%1 level(s) skipped").arg(skippedCount));
  }
  /*
if (m_srcFilePaths.size()==1)
{
if (skipped==0)
  DVGui::info(tr("Level %1 converted to TLV
Format").arg(QString::fromStdWString(m_srcFilePaths[0].withoutParentDir().getWideString())));
else
  DVGui::warning(tr("Warning: Level %1 NOT converted to TLV
Format").arg(QString::fromStdWString(m_srcFilePaths[0].withoutParentDir().getWideString())));
}
else
DVGui::MsgBox(skipped==0?DVGui::INFORMATION:DVGui::WARNING, tr("Converted %1 out
of %2 Levels to TLV Format").arg(
QString::number(m_srcFilePaths.size()-skipped)).arg(QString::number(m_srcFilePaths.size())));
*/

  // TFilePath parentDir = m_srcFilePaths[0].getParentDir();
  // FileBrowser::refreshFolder(parentDir);

  delete m_converter;
  m_converter = 0;
}

//-------------------------------------------------------------------

void ConvertPopup::onLevelConverted(const TFilePath &fullPath) {
  IconGenerator::instance()->invalidate(fullPath);
}

//-------------------------------------------------------------------

TPropertyGroup *ConvertPopup::getFormatProperties(const std::string &ext) {
  if (m_formatProperties.contains(ext)) return m_formatProperties[ext];
  TPropertyGroup *props   = Tiio::makeWriterProperties(ext);
  m_formatProperties[ext] = props;
  return props;
}

//-------------------------------------------------------------------

void ConvertPopup::onOptionsClicked() {
  std::string ext       = m_fileFormat->currentText().toStdString();
  TPropertyGroup *props = getFormatProperties(ext);

  openFormatSettingsPopup(this, ext, props, m_srcFilePaths.size() == 1
                                                ? m_srcFilePaths[0]
                                                : TFilePath());
}

//-------------------------------------------------------------------

void ConvertPopup::onFormatChanged(const QString &ext) {
  if (ext == QString("avi") || ext == QString("tzp") || ext == TlvExtension ||
      ext == OldLevelToTlvExtension) {
    m_removeDotBeforeFrameNumber->setChecked(false);
    m_removeDotBeforeFrameNumber->setEnabled(false);
  } else {
    m_removeDotBeforeFrameNumber->setEnabled(true);
    if (ext == QString("tga")) m_removeDotBeforeFrameNumber->setChecked(true);
  }
}

//-------------------------------------------------------------------

void ConvertPopup::onPalettePathChanged() {
  m_removeUnusedStyles->setEnabled(
      m_tlvMode->currentText() == TlvMode_PaintedFromNonAA &&
      m_palettePath->getPath() != CreateNewPalette);
}

//-------------------------------------------------------------------

bool ConvertPopup::isSaveTlvBackupToNopaintActive() {
  return m_fileFormat->currentText() ==
             TlvExtension /*-- tlvが選択されている --*/
         &&
         m_tlvMode->currentText() ==
             TlvMode_Unpainted /*-- Unpainted Tlvが選択されている --*/
         && m_saveBackupToNopaint->isChecked(); /*-- Save Backup to "nopaint"
                                                   Folder オプションが有効 --*/
}

//-------------------------------------------------------------------

void ConvertPopup::onDpiModeSelected(int index) {
  double dpiVal;
  if (index == 2) {  // Custom
    m_dpiFld->setEnabled(true);
    dpiVal = ConvertPopupDpiValue;
  } else {  // Image or Camera DPI
    m_dpiFld->setEnabled(false);
    if (index == 1 || m_imageDpi == 0.0) {  // If dpi information is not
                                            // contained in the source, use the
                                            // camera dpi
      dpiVal = TApp::instance()
                   ->getCurrentScene()
                   ->getScene()
                   ->getCurrentCamera()
                   ->getDpi()
                   .x;
    } else
      dpiVal = m_imageDpi;
  }
  m_dpiFld->setValue(dpiVal);
}

//=============================================================================
// ConvertPopupCommand
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ConvertPopup> openConvertPopup(MI_ConvertFiles);
