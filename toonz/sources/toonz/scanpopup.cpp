

#include "scanpopup.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "texception.h"
#include "mainwindow.h"
#include "tsystem.h"
#include "timagecache.h"
#include "tgl.h"
#include "tcurveutil.h"
#include "tpixelutils.h"
#include "trop.h"
#include "tiio.h"

#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/observer.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage2.h"
#include "toonz/glrasterpainter.h"
#include "tools/toolutils.h"
#include "toonz/preferences.h"
#include "toonz/cleanupparameters.h"
#include "toonz/tcleanupper.h"

#include "tools/toolhandle.h"
#include "tools/cursors.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"

#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QApplication>

using namespace DVGui;
using namespace CleanupTypes;

//-----------------------------------------------------------------------------
namespace {

const double previewDPI = 64;
//-----------------------------------------------------------------------------

const QString BlackAndWhite = "Black & White", Graytones = "Graytones",
              Rgbcolors = "RGB Color";

bool ScannerHasBeenDefined = false;

void checkPaperFormat(TScannerParameters *parameters) {
  if (parameters->getPaperOverflow()) {
    TScanner *scanner   = TScanner::instance();
    QString scannerName = scanner ? scanner->getName() : "no scanner";
    DVGui::warning(
        QObject::tr("The selected paper format is not available for %1.")
            .arg(scannerName));
  }
}

//-----------------------------------------------------------------------------

bool defineScanner(const QString &scannerType) {
  bool ret = false;
  QApplication::setOverrideCursor(Qt::WaitCursor);

  TScanner::m_isTwain = (scannerType == "TWAIN");

  try {
    if (!TScanner::instance()->isDeviceAvailable()) {
      DVGui::warning(TScanner::m_isTwain
                         ? QObject::tr("No TWAIN scanner is available")
                         : QObject::tr("No scanner is available"));
      /* FIXME: try/catch からの goto
って合法じゃないだろ……。とりあえず応急処置したところ"例外ってナニ?"って感じになったのが
indent も腐っておりつらいので後で直す */
      // goto end;
      QApplication::restoreOverrideCursor();
      return false;
    }
    TScanner::instance()->selectDevice();
  } catch (TException &e) {
    DVGui::warning(QString::fromStdWString(e.getMessage()));
    QApplication::restoreOverrideCursor();
    return false;
    // goto end;
  }

  TScannerParameters *scanParameters = TApp::instance()
                                           ->getCurrentScene()
                                           ->getScene()
                                           ->getProperties()
                                           ->getScanParameters();
  try {
    scanParameters->adaptToCurrentScanner();
  } catch (TException &e) {
    DVGui::warning(QString::fromStdWString(e.getMessage()));
    // goto end;
    QApplication::restoreOverrideCursor();
    return false;
  }
  scanParameters->updatePaperFormat();
  checkPaperFormat(scanParameters);
  // ScanSettingsPopup::instance()->updateUI();

  ret = true;

  // end:
  QApplication::restoreOverrideCursor();
  return ret;
}

//-----------------------------------------------------------------------------

bool checkScannerDefinition() {
  if (!ScannerHasBeenDefined) {
    QString scannerType = QSettings().value("CurrentScannerType").toString();
    if (scannerType == "") {
      QSettings().setValue("CurrentScannerType", "Internal");
      scannerType = "Internal";
    }
    ScannerHasBeenDefined = defineScanner(scannerType);
  }
  return ScannerHasBeenDefined;
}

//-----------------------------------------------------------------------------

void fillOutputType(QComboBox *t, TScannerParameters *params) {
  while (t->count()) t->removeItem(0);

  if (params->isSupported(TScannerParameters::BW)) t->addItem(BlackAndWhite);
  if (params->isSupported(TScannerParameters::GR8)) t->addItem(Graytones);
  if (params->isSupported(TScannerParameters::RGB24)) t->addItem(Rgbcolors);
}

//-----------------------------------------------------------------------------

TPixelGR8 fromRGB(int r, int g, int b) {
  return TPixelGR8(
      (((UINT)(r)*19594 + (UINT)(g)*38472 + (UINT)(b)*7470 + (UINT)(1 << 15)) >>
       16));
}

//-----------------------------------------------------------------------------

void makeTransparent(const TRaster32P &ras) {
  if (!ras) return;
  TRop::addBackground(ras, TPixel32::White);
  int x, y;
  for (x = 0; x < ras->getLx(); x++) {
    for (y = 0; y < ras->getLy(); y++) {
      TPixel32 *pix    = &ras->pixels(y)[x];
      TPixelGR8 pixGR8 = fromRGB(pix->r, pix->g, pix->b);
      int value        = pixGR8.value;
      pix->m           = (UCHAR)tcrop<SHORT>(255 - value, 0, 255);
      premult(*pix);
    }
  }
}

//=============================================================================

}  // namepsapce

//=============================================================================
/*! \class DefineScannerPopup
                \brief The DefineScannerPopup class provides a modal dialog to
   choose and define a scan for application.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

DefineScannerPopup::DefineScannerPopup()
    : Dialog(TApp::instance()->getMainWindow(), true,
             Preferences::instance()->getCurrentLanguage() == "English",
             "DefineScanner") {
#ifdef MACOSX
  setModal(false);
#endif
  setWindowTitle(tr("Define Scanner"));

  m_scanDriverOm = new QComboBox();
  m_scanDriverOm->setFixedSize(150, WidgetHeight);
  QStringList scan;
  scan << "TWAIN"
       << "Internal";
  m_scanDriverOm->addItems(scan);
  addWidget(tr("Scanner Driver:"), m_scanDriverOm);

  if (QSettings().value("CurrentScannerType").toString() == "Internal")
    m_scanDriverOm->setCurrentIndex(1);

  QPushButton *okBtn = new QPushButton(tr("OK"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------

void DefineScannerPopup::accept() {
  QString scannerType = m_scanDriverOm->currentText();

  if (QSettings().value("CurrentScannerType").toString() != scannerType ||
      !ScannerHasBeenDefined) {
    QSettings().setValue("CurrentScannerType", scannerType);
    ScannerHasBeenDefined = defineScanner(scannerType);
    QAction *setCropAction =
        CommandManager::instance()->getAction("MI_SetScanCropbox");
    QAction *resetCropAction =
        CommandManager::instance()->getAction("MI_ResetScanCropbox");
    if (scannerType == "TWAIN")
      setCropAction->setDisabled(true);
    else
      setCropAction->setDisabled(false);
    resetCropAction->setDisabled(true);
  }
  QDialog::accept();
}

//-----------------------------------------------------------------------------

//=============================================================================
/*! \class ScanSettingsPopup
                \brief The ScanSettingsPopup class provides a dialog to change
   scan settings.

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

ScanSettingsPopup::ScanSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, true, "ScanSettings") {
  setWindowTitle(tr("Scan Settings"));
  m_scannerNameLbl = new QLabel(tr("[no scanner]"));

  addWidget(m_scannerNameLbl);

  beginVLayout();

  m_paperFormatOm = new QComboBox();
  m_paperFormatOm->setFixedSize(150, WidgetHeight);
  std::vector<std::string> formats;
  TPaperFormatManager::instance()->getFormats(formats);
  int i;
  for (i = 0; i < (int)formats.size(); i++)
    m_paperFormatOm->addItem(QString(formats[i].c_str()));
  addWidgets(m_formatLbl = new QLabel(tr("Paper Format:")), m_paperFormatOm);

  m_reverseOrderCB = new CheckBox(tr("Reverse Order"));
  m_reverseOrderCB->setFixedSize(150, WidgetHeight);
  addWidget(m_reverseOrderCB);

  m_paperFeederCB = new CheckBox(tr("Paper Feeder"));
  m_paperFeederCB->setFixedSize(150, WidgetHeight);
  addWidget(m_paperFeederCB);

  m_dpi = new DVGui::DoubleField();
  addWidgets(m_dpiLbl = new QLabel(tr("Dpi: ")), m_dpi);
  m_dpi->hide();
  m_dpiLbl->hide();

  m_modeOm = new QComboBox();
  m_modeOm->setMaximumHeight(WidgetHeight);
  addWidgets(m_modeLbl = new QLabel(tr("Mode:")), m_modeOm);

  m_threshold = new DVGui::IntField();
  addWidgets(m_thresholdLbl = new QLabel(tr("Threshold: ")), m_threshold);
  m_threshold->hide(), m_thresholdLbl->hide();

  m_brightness = new DVGui::IntField();
  addWidgets(m_brightnessLbl = new QLabel(tr("Brightness: ")), m_brightness);
  m_brightness->hide(), m_brightnessLbl->hide();

  endVLayout();

  connectAll();
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::showEvent(QShowEvent *event) {
  connectAll();
  updateUI();
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::hideEvent(QHideEvent *event) {
  disconnectAll();
  Dialog::hideEvent(event);
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::connectAll() {
  bool ret = true;
  ret      = ret &&
        connect(m_paperFormatOm, SIGNAL(currentIndexChanged(const QString &)),
                SLOT(onPaperChanged(const QString &)));
  ret = ret && connect(m_reverseOrderCB, SIGNAL(stateChanged(int)),
                       SLOT(onToggle(int)));
  ret = ret && connect(m_paperFeederCB, SIGNAL(stateChanged(int)),
                       SLOT(onToggle(int)));
  ret = ret &&
        connect(m_dpi, SIGNAL(valueChanged(bool)), SLOT(onValueChanged(bool)));
  ret = ret && connect(m_brightness, SIGNAL(valueChanged(bool)),
                       SLOT(onValueChanged(bool)));
  ret = ret && connect(m_threshold, SIGNAL(valueChanged(bool)),
                       SLOT(onValueChanged(bool)));
  ret = ret && connect(m_modeOm, SIGNAL(currentIndexChanged(const QString &)),
                       SLOT(onModeChanged(const QString &)));
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(sceneSwitched()), SLOT(updateUI()));
  assert(ret);
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::disconnectAll() {
  bool ret = true;
  ret      = ret && m_paperFormatOm->disconnect();
  ret      = ret && m_reverseOrderCB->disconnect();
  ret      = ret && m_paperFeederCB->disconnect();
  ret      = ret && m_dpi->disconnect();
  ret      = ret && m_threshold->disconnect();
  ret      = ret && m_brightness->disconnect();
  ret      = ret && m_modeOm->disconnect();
  ret      = ret && disconnect(TApp::instance()->getCurrentScene(),
                          SIGNAL(sceneSwitched()), this, SLOT(updateUI()));
  assert(ret);
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::onValueChanged(bool) {
  TScannerParameters *params = TApp::instance()
                                   ->getCurrentScene()
                                   ->getScene()
                                   ->getProperties()
                                   ->getScanParameters();
  params->m_dpi.m_value        = m_dpi->getValue();
  params->m_threshold.m_value  = m_threshold->getValue();
  params->m_brightness.m_value = m_brightness->getValue();
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::onToggle(int) {
  TScannerParameters *sp = TApp::instance()
                               ->getCurrentScene()
                               ->getScene()
                               ->getProperties()
                               ->getScanParameters();

  sp->setReverseOrder(m_reverseOrderCB->isChecked());
  sp->enablePaperFeeder(m_paperFeederCB->isChecked());
}

//-----------------------------------------------------------------------------

void ScanSettingsPopup::onPaperChanged(const QString &format) {
  if (!ScannerHasBeenDefined) return;

  TScannerParameters *sp = TApp::instance()
                               ->getCurrentScene()
                               ->getScene()
                               ->getProperties()
                               ->getScanParameters();
  sp->setPaperFormat(format.toStdString());
  sp->setCropBox(sp->getScanArea());
  checkPaperFormat(sp);
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::onModeChanged(const QString &mode) {
  if (!ScannerHasBeenDefined) return;

  TScannerParameters *sp = TApp::instance()
                               ->getCurrentScene()
                               ->getScene()
                               ->getProperties()
                               ->getScanParameters();
  m_threshold->setVisible(false);
  m_thresholdLbl->setVisible(false);

  if (mode == BlackAndWhite) {
    sp->setScanType(TScannerParameters::BW);
    m_threshold->setVisible(true);
    m_thresholdLbl->setVisible(true);
  } else if (mode == Graytones)
    sp->setScanType(TScannerParameters::GR8);
  else if (mode == Rgbcolors)
    sp->setScanType(TScannerParameters::RGB24);
  else {
    // assert(0);
    sp->setScanType(TScannerParameters::None);
  }
}

//------------------------------------------------------------------------------

void ScanSettingsPopup::updateUI() {
  disconnectAll();
  checkScannerDefinition();
  TScannerParameters *params = TApp::instance()
                                   ->getCurrentScene()
                                   ->getScene()
                                   ->getProperties()
                                   ->getScanParameters();
  QApplication::setOverrideCursor(Qt::WaitCursor);
  params->adaptToCurrentScanner();
  QApplication::restoreOverrideCursor();

  TScanner *scanner = TScanner::instance();
  m_scannerNameLbl->setText(scanner && scanner->getName() != ""
                                ? scanner->getName()
                                : "[no scanner]");

  m_reverseOrderCB->setChecked(params->isReverseOrder());
  m_paperFeederCB->setChecked(params->m_paperFeeder.m_value == 1.0);

  if (params->m_dpi.m_supported) {
    if (params->m_dpi.m_max > params->m_dpi.m_min) {
      m_dpi->setRange(params->m_dpi.m_min, params->m_dpi.m_max);
      m_dpi->setValue(params->m_dpi.m_value);
    }
    m_dpi->show(), m_dpiLbl->show();
  } else
    m_dpi->hide(), m_dpiLbl->hide();

  if (TScanner::m_isTwain) {
    m_paperFeederCB->hide();
    m_modeOm->hide(), m_modeLbl->hide();
    m_paperFormatOm->hide(), m_formatLbl->hide();
    m_threshold->hide(), m_thresholdLbl->hide();
    m_brightness->hide(), m_brightnessLbl->hide();
    connectAll();
    return;
  }
  m_paperFeederCB->show();
  m_modeOm->show(), m_modeLbl->show();
  m_paperFormatOm->show(), m_formatLbl->show();

  disconnect(m_modeOm);
  fillOutputType(m_modeOm, params);
  connect(m_modeOm, SIGNAL(currentIndexChanged(const QString &)),
          SLOT(onModeChanged(const QString &)));

  m_modeOm->setEnabled(true);
  switch (params->getScanType()) {
  case TScannerParameters::BW:
    m_modeOm->setCurrentIndex(m_modeOm->findText(BlackAndWhite));
    break;
  case TScannerParameters::GR8:
    m_modeOm->setCurrentIndex(m_modeOm->findText(Graytones));
    break;
  case TScannerParameters::RGB24:
    m_modeOm->setCurrentIndex(m_modeOm->findText(Rgbcolors));
    break;
  default:
    m_modeOm->setEnabled(false);
    break;
  }

  m_paperFormatOm->setCurrentIndex(m_paperFormatOm->findText(
      QString::fromStdString(params->getPaperFormat())));

  if (params->m_threshold.m_supported) {
    m_threshold->setRange(params->m_threshold.m_min, params->m_threshold.m_max);
    m_threshold->setValue(params->m_threshold.m_value);
    if (params->getScanType() == TScannerParameters::BW) {
      m_threshold->show();
      m_thresholdLbl->show();
    } else {
      m_threshold->hide();
      m_thresholdLbl->hide();
    }
  }
  if (params->m_brightness.m_supported) {
    if (params->m_brightness.m_max > params->m_brightness.m_min) {
      m_brightness->setRange(params->m_brightness.m_min,
                             params->m_brightness.m_max);
      m_brightness->setValue(params->m_brightness.m_value);
    }
    m_brightness->show(), m_brightnessLbl->show();
  } else
    m_brightness->hide(), m_brightnessLbl->hide();

  connectAll();
}
#ifdef LINETEST
//-----------------------------------------------------------------------------

AutocenterPopup::AutocenterPopup() : DVGui::Dialog(0, false, true) {
  setWindowTitle(tr("Autocenter"));

  QGridLayout *settingsLayout = new QGridLayout(this);
  settingsLayout->setSizeConstraint(QLayout::SetFixedSize);
  settingsLayout->setSpacing(5);
  settingsLayout->setMargin(12);
  int row = 0;

  // AutoCenter
  QWidget *w = new QWidget();
  w->setFixedWidth(70);
  settingsLayout->addWidget(w, row, 0);
  m_autocenter = new CheckBox(tr("Autocenter"), this);
  m_autocenter->setFixedSize(150, WidgetHeight);
  settingsLayout->addWidget(m_autocenter, row, 1, Qt::AlignLeft);
  ++row;

  // Pegbar Holse
  settingsLayout->addWidget(new QLabel(tr("Pegbar Holes:")), row, 0,
                            Qt::AlignRight);
  m_pegbarHoles = new QComboBox(this);
  m_pegbarHoles->setFixedHeight(WidgetHeight);
  m_pegbarHoles->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
  QStringList pegbarHoles;
  pegbarHoles << "Bottom"
              << "Top"
              << "Left"
              << "Right";
  m_pegbarHoles->addItems(pegbarHoles);
  settingsLayout->addWidget(m_pegbarHoles, row, 1, Qt::AlignLeft);
  ++row;

  // Feld Guide
  settingsLayout->addWidget(new QLabel(tr("Field Guide:")), row, 0,
                            Qt::AlignRight);
  m_fieldGuide = new QComboBox(this);
  m_fieldGuide->setFixedHeight(WidgetHeight);
  m_fieldGuide->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
  std::vector<std::string> fdgNames;
  CleanupParameters::getFdgNames(fdgNames);
  int i;
  for (i = 0; i < (int)fdgNames.size(); i++)
    m_fieldGuide->addItem(QString(fdgNames[i].c_str()));
  settingsLayout->addWidget(m_fieldGuide, row, 1, Qt::AlignLeft);
  ++row;

  beginHLayout();
  addLayout(settingsLayout, false);
  endHLayout();

  bool ret = true;
  ret      = ret && connect(m_autocenter, SIGNAL(toggled(bool)), this,
                       SLOT(onAutocenterToggled(bool)));
  ret = ret &&
        connect(m_pegbarHoles, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(onPegbarHolesChanged(const QString &)));
  ret =
      ret && connect(m_fieldGuide, SIGNAL(currentIndexChanged(const QString &)),
                     this, SLOT(onFieldGuideChanged(const QString &)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void AutocenterPopup::showEvent(QShowEvent *e) {
  CleanupParameters *cp = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getProperties()
                              ->getCleanupParameters();
  m_autocenter->setChecked(cp->m_autocenterType == AUTOCENTER_FDG);

  CleanupTypes::PEGS_SIDE type = cp->m_pegSide;
  switch (cp->m_pegSide) {
  case PEGS_BOTTOM:
    m_pegbarHoles->setCurrentIndex(0);
    break;
  case PEGS_TOP:
    m_pegbarHoles->setCurrentIndex(1);
    break;
  case PEGS_LEFT:
    m_pegbarHoles->setCurrentIndex(2);
    break;
  case PEGS_RIGHT:
    m_pegbarHoles->setCurrentIndex(3);
    break;
  default:
    assert(false);
  }

  QString fieldName = QString::fromStdString(cp->getFdgName());
  int index         = (fieldName == "") ? 0 : m_fieldGuide->findText(fieldName);
  assert(index != -1);
  m_fieldGuide->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void AutocenterPopup::onAutocenterToggled(bool toggled) {
  CleanupParameters *cp = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getProperties()
                              ->getCleanupParameters();
  cp->m_autocenterType = toggled ? AUTOCENTER_FDG : AUTOCENTER_NONE;
}

//-----------------------------------------------------------------------------

void AutocenterPopup::onPegbarHolesChanged(const QString &pg) {
  CleanupParameters *cp = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getProperties()
                              ->getCleanupParameters();
  CleanupTypes::PEGS_SIDE type;
  if (pg == "Bottom") type = PEGS_BOTTOM;
  if (pg == "Top") type    = PEGS_TOP;
  if (pg == "Left") type   = PEGS_LEFT;
  if (pg == "Right") type  = PEGS_RIGHT;
  if (cp->m_pegSide == type) return;
  cp->m_pegSide = type;
}

//-----------------------------------------------------------------------------

void AutocenterPopup::onFieldGuideChanged(const QString &fg) {
  CleanupParameters *cp = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getProperties()
                              ->getCleanupParameters();
  if (cp->getFdgName() == fg.toStdString()) return;
  cp->setFdgByName(fg.toStdString());
}

//-----------------------------------------------------------------------------
#endif

MyScannerListener::MyScannerListener(const ScanList &scanList)
    : m_scanList(scanList)
    , m_current(0)
    , m_inc(+1)
    , m_isCanceled(false)
    , m_progressDialog(0) {
  TScannerParameters *parameters = TApp::instance()
                                       ->getCurrentScene()
                                       ->getScene()
                                       ->getProperties()
                                       ->getScanParameters();

  m_isPreview = parameters->isPreview();
  if (m_isPreview) return;
  if (parameters->isPaperFeederEnabled()) {
    int frameCount         = m_scanList.getFrameCount();
    ScanListFrame frame    = m_scanList.getFrame(m_current);
    TXshSimpleLevel *newXl = frame.getLevel();
    TFilePath levelName(newXl->getName());
    QString imageName =
        toQString(levelName.withFrame(frame.getFrameId())) + QString(".tif");
    QString text = tr("Scanning in progress: ") + imageName + " 1/" +
                   QString::number(frameCount);
    m_progressDialog =
        new DVGui::ProgressDialog(text, QObject::tr("Cancel"), 0, frameCount);
    connect(m_progressDialog, SIGNAL(canceled()), this,
            SLOT(cancelButtonPressed()));
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->show();
  }
  if (parameters->isReverseOrder()) {
    m_current = m_scanList.getFrameCount() - 1;
    m_inc     = -1;
  }
}

//-----------------------------------------------------------------------------

void MyScannerListener::onImage(const TRasterImageP &rasImg) {
  if (!rasImg || !rasImg->getRaster()) {
    DVGui::warning(tr("The pixel type is not supported."));
    m_current += m_inc;
    return;
  }
  if (!m_isPreview &&
      (m_current < 0 || m_current >= m_scanList.getFrameCount())) {
    DVGui::warning(tr("The scanning process is completed."));
    return;
  }
  if (m_isPreview) {
    TImageCache::instance()->add((std::string) "setScanCropboxId",
                                 rasImg.getPointer());
  } else {
#ifdef LINETEST
    // Autocenter
    CleanupParameters *cp = TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getCleanupParameters();
    if (cp->m_autocenterType != AUTOCENTER_NONE) {
      bool autocentered;
      TCleanupper *cl = TCleanupper::instance();
      cl->setParameters(cp);
      TRasterImageP outImg = cl->autocenterOnly(rasImg, false, autocentered);
      if (!autocentered)
        DVGui::warning(
            QObject::tr("The autocentering failed on the current drawing."));
      else
        rasImg->setRaster(outImg->getRaster());
    }
    makeTransparent(rasImg->getRaster());
#endif
    TScannerParameters *params = TApp::instance()
                                     ->getCurrentScene()
                                     ->getScene()
                                     ->getProperties()
                                     ->getScanParameters();

    ScanListFrame frame = m_scanList.getFrame(m_current);
    bool isBW           = false;
    if (params->getScanType() == TScannerParameters::BW) {
      isBW = true;
      rasImg->setScanBWFlag(true);
      Tiio::Writer::setBlackAndWhiteThreshold(params->m_threshold.m_value);
    }
    frame.setRasterImage(rasImg, isBW);

    TApp::instance()->getCurrentFrame()->setFid(frame.getFrameId());
    TApp::instance()->getCurrentScene()->notifyCastChange();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();

    m_current += m_inc;
  }
}

//-----------------------------------------------------------------------------

void MyScannerListener::onError() {
  if (m_progressDialog) m_progressDialog->hide();
  DVGui::warning(tr("There was an error during the scanning process."));
}

//-----------------------------------------------------------------------------

void MyScannerListener::onNextPaper() {
  assert(!m_isPreview);
  if (TScanner::instance()->m_isTwain)
    DVGui::info(
        tr("Please, place the next paper drawing on the scanner flatbed, then "
           "select the relevant command in the TWAIN interface."));
  else {
    QString question(
        tr("Please, place the next paper drawing on the scanner flatbed, then "
           "click the Scan button."));
    int ret =
        DVGui::MsgBox(question, QObject::tr("Scan"), QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) m_isCanceled = true;
  }
}

//-----------------------------------------------------------------------------

void MyScannerListener::onAutomaticallyNextPaper() {
  assert(!m_isPreview);
  assert(!!m_progressDialog);
  int frameCount = m_scanList.getFrameCount();
  int step       = m_inc == -1 ? frameCount - m_current - 1 : m_current;
  if (step < frameCount) {
    ScanListFrame frame    = m_scanList.getFrame(m_current);
    TXshSimpleLevel *newXl = frame.getLevel();
    TFilePath levelName(newXl->getName());
    QString imageName =
        toQString(levelName.withFrame(frame.getFrameId())) + QString(".tif");
    m_progressDialog->setLabelText(tr("Scanning in progress: ") + imageName +
                                   QString::number(m_current + 1) + " /" +
                                   QString::number(frameCount));
  }
  m_progressDialog->setValue(step);
  if (m_progressDialog && (step == frameCount)) m_progressDialog->hide();
}

//-----------------------------------------------------------------------------

bool MyScannerListener::isCanceled() { return m_isCanceled; }

//-----------------------------------------------------------------------------

void MyScannerListener::cancelButtonPressed() {
  assert(!!m_progressDialog);
  m_isCanceled = true;
}

//=============================================================================
//
static void doScan() {
  if (!checkScannerDefinition()) return;
  ScanList scanList;
  if (scanList.areScannedFramesSelected()) {
    int ret = DVGui::MsgBox(
        QObject::tr("Some of the selected drawings were already scanned. Do "
                    "you want to scan them again?"),
        QObject::tr("Scan"), QObject::tr("Don't Scan"), QObject::tr("Cancel"));
    if (ret == 3) return;
    scanList.update(ret == 1);
  } else
    scanList.update(true);

  if (scanList.getFrameCount() == 0) {
    DVGui::warning(QObject::tr("There are no frames to scan."));
    return;
  }

  try {
    TScanner *scanner = TScanner::instance();

    int rc = scanner->isDeviceAvailable();
    if (!rc) {
      DVGui::warning(QObject::tr("TWAIN is not available."));
      return;
    }

    MyScannerListener lst(scanList);
    scanner->addListener(&lst);

    TScannerParameters *sp = TApp::instance()
                                 ->getCurrentScene()
                                 ->getScene()
                                 ->getProperties()
                                 ->getScanParameters();
    sp->adaptToCurrentScannerIfNeeded();
    scanner->acquire(*sp, scanList.getFrameCount());
    scanner->removeListener(&lst);

    SetScanCropboxCheck *cropboxCheck = SetScanCropboxCheck::instance();
    if (cropboxCheck->isEnabled()) cropboxCheck->uncheck();
  } catch (TException &e) {
    DVGui::warning(QString::fromStdWString(e.getMessage()));
  }

  // If some levels were scanned successfully, their renumber table must be
  // updated.

  // A level's renumber table is usually updated when it is either loaded or
  // before saving -
  // this is a similar case, where a level is filled with frames.
  // An empty renumber table means that a renumbering operation was carried out
  // with all frames
  // being eradicated - which may lead to the level being saved with missing
  // frames.
  int i, frameCount = scanList.getFrameCount();
  TXshSimpleLevel *oldLevel = 0, *level;
  for (i = 0; i < frameCount; ++i) {
    level = scanList.getFrame(i).getLevel();
    if (level != oldLevel) level->setRenumberTable();
    oldLevel = level;
  }
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<DefineScannerPopup> openDefineScannerPopup(
    MI_DefineScanner);

OpenPopupCommandHandler<ScanSettingsPopup> openScanSettingsPopup(
    MI_ScanSettings);

#ifdef LINETEST
OpenPopupCommandHandler<AutocenterPopup> openAutocenterPopup(MI_Autocenter);
#endif

class ScanCommand final : public MenuItemHandler {
public:
  ScanCommand() : MenuItemHandler("MI_Scan") {}
  void execute() override { doScan(); }
} ScanCommand;

//=========================================================================================
//
// SetCropboxCommand
//
//=========================================================================================

class SetCropboxCommand final : public MenuItemHandler {
  TTool *m_currentTool;

public:
  SetCropboxCommand()
      : MenuItemHandler("MI_SetScanCropbox"), m_currentTool(0) {}

  void execute() override {
    TApp *app                         = TApp::instance();
    SetScanCropboxCheck *cropboxCheck = SetScanCropboxCheck::instance();
    cropboxCheck->setIsEnabled(!cropboxCheck->isEnabled());

    TScannerParameters *sp = TApp::instance()
                                 ->getCurrentScene()
                                 ->getScene()
                                 ->getProperties()
                                 ->getScanParameters();
    QAction *resetCropAction =
        CommandManager::instance()->getAction("MI_ResetScanCropbox");
    if (!cropboxCheck->isEnabled()) {
      if (m_currentTool)
        app->getCurrentTool()->setTool(
            QString::fromStdString(m_currentTool->getName()));
      return;
    }

    if (!checkScannerDefinition()) return;
    ScanList scanList;
    scanList.update(true);
    try {
      TScanner *scanner = TScanner::instance();

      int rc = scanner->isDeviceAvailable();
      if (!rc) {
        DVGui::warning(QObject::tr("TWAIN is not available."));
        return;
      }

      double currentDPI = sp->m_dpi.m_value;
      sp->m_dpi.m_value = previewDPI;
      double reverse    = sp->isReverseOrder();
      sp->setReverseOrder(false);
      sp->setIsPreview(true);

      MyScannerListener lst(scanList);
      scanner->addListener(&lst);

      sp->adaptToCurrentScannerIfNeeded();
      scanner->acquire(*sp, 1);
      scanner->removeListener(&lst);
      sp->m_dpi.m_value = currentDPI;
      sp->setReverseOrder(reverse);

      m_currentTool = app->getCurrentTool()->getTool();
      app->getCurrentTool()->setTool("T_SetScanCropbox");
      sp->setIsPreview(false);
      if (resetCropAction) resetCropAction->setDisabled(false);
    } catch (TException &e) {
      DVGui::warning(QString::fromStdWString(e.getMessage()));
    }
  }
} setCropboxCommand;

//=========================================================================================
//
// ResetCropboxCommand
//
//=========================================================================================

class ResetCropboxCommand final : public MenuItemHandler {
public:
  ResetCropboxCommand() : MenuItemHandler("MI_ResetScanCropbox") {}

  void execute() override {
    TScannerParameters *sp = TApp::instance()
                                 ->getCurrentScene()
                                 ->getScene()
                                 ->getProperties()
                                 ->getScanParameters();
    sp->setCropBox(sp->getScanArea());
  }
} resetCropboxCommand;

//=========================================================================================
//
// SetScanCropboxTool
//
//=========================================================================================

class SetScanCropboxTool final : public TTool {
  const std::string m_imgId;
  TScannerParameters *m_parameters;
  int m_scaling;
  TPointD m_lastPos;
  enum { eNone, eMove, e00, e01, e10, e11, eM0, e1M, eM1, e0M };

public:
  SetScanCropboxTool()
      : TTool("T_SetScanCropbox")
      , m_imgId("setScanCropboxId")
      , m_scaling(eNone)
      , m_lastPos() {
    bind(TTool::AllTargets);  // Deals with tool deactivation internally
  }

  //-----------------------------------------------------------------------------

  ToolType getToolType() const override { return TTool::GenericTool; }

  //-----------------------------------------------------------------------------

  void draw() override {
    TRasterImageP ri = TImageCache::instance()->get(m_imgId, false);
    if (ri) {
      TPointD center = ri->getRaster()->getCenterD();
      GLRasterPainter::drawRaster(
          TScale(Stage::inch / previewDPI, Stage::inch / previewDPI), ri, true);
    }

    TRectD cropBox   = rect2pix(m_parameters->getCropBox());
    double pixelSize = getPixelSize();

    tglColor(TPixel::Red);
    glLineStipple(1, 0xFFFF);
    glEnable(GL_LINE_STIPPLE);

    tglDrawRect(cropBox);

    TPointD size(10, 10);
    tglColor(TPixel::Red);
    ToolUtils::drawSquare(cropBox.getP00(), pixelSize * 4, TPixel::Red);
    ToolUtils::drawSquare(cropBox.getP01(), pixelSize * 4, TPixel::Red);
    ToolUtils::drawSquare(cropBox.getP10(), pixelSize * 4, TPixel::Red);
    ToolUtils::drawSquare(cropBox.getP11(), pixelSize * 4, TPixel::Red);

    TPointD center = (cropBox.getP00() + cropBox.getP11()) * 0.5;

    ToolUtils::drawSquare(TPointD(center.x, cropBox.y0), pixelSize * 4,
                          TPixel::Red);  // draw M0 handle
    ToolUtils::drawSquare(TPointD(cropBox.x1, center.y), pixelSize * 4,
                          TPixel::Red);  // draw 1M handle
    ToolUtils::drawSquare(TPointD(center.x, cropBox.y1), pixelSize * 4,
                          TPixel::Red);  // draw M1 handle
    ToolUtils::drawSquare(TPointD(cropBox.x0, center.y), pixelSize * 4,
                          TPixel::Red);  // draw 0M handle

    glDisable(GL_LINE_STIPPLE);
  }

  //-----------------------------------------------------------------------------

  void mouseMove(const TPointD &p, const TMouseEvent &e) override {
    double pixelSize = getPixelSize();
    TPointD size(10 * pixelSize, 10 * pixelSize);
    TRectD cropBox = rect2pix(m_parameters->getCropBox());

    double maxDist = 5 * pixelSize;

    if (TRectD(cropBox.getP00() - size, cropBox.getP00() + size).contains(p))
      m_scaling = e00;
    else if (TRectD(cropBox.getP01() - size, cropBox.getP01() + size)
                 .contains(p))
      m_scaling = e01;
    else if (TRectD(cropBox.getP11() - size, cropBox.getP11() + size)
                 .contains(p))
      m_scaling = e11;
    else if (TRectD(cropBox.getP10() - size, cropBox.getP10() + size)
                 .contains(p))
      m_scaling = e10;
    else if (isCloseToSegment(p, TSegment(cropBox.getP00(), cropBox.getP10()),
                              maxDist))
      m_scaling = eM0;
    else if (isCloseToSegment(p, TSegment(cropBox.getP10(), cropBox.getP11()),
                              maxDist))
      m_scaling = e1M;
    else if (isCloseToSegment(p, TSegment(cropBox.getP11(), cropBox.getP01()),
                              maxDist))
      m_scaling = eM1;
    else if (isCloseToSegment(p, TSegment(cropBox.getP01(), cropBox.getP00()),
                              maxDist))
      m_scaling = e0M;
    else if (cropBox.contains(p))
      m_scaling = eMove;
    else
      m_scaling = eNone;
  }

  //-----------------------------------------------------------------------------

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    m_lastPos = pos;
  }

  //-----------------------------------------------------------------------------

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    TPointD dp         = pos - m_lastPos;
    double scaleFactor = Stage::inch / previewDPI;
    dp.x               = ((dp.x / scaleFactor) * 25.4) / previewDPI;
    dp.y               = ((dp.y / scaleFactor) * 25.4) / previewDPI;

    TRectD scanArea = m_parameters->getScanArea();
    TRectD cropBox  = m_parameters->getCropBox();
    if (m_scaling != eNone && m_scaling != eMove) {
      if ((m_scaling == eM1 ||
           ((m_scaling == e11 || m_scaling == e01) && !e.isShiftPressed())) &&
          cropBox.x0 - dp.y < cropBox.x1)
        cropBox.x0 -= dp.y;
      if ((m_scaling == eM0 ||
           ((m_scaling == e10 || m_scaling == e00) && !e.isShiftPressed())) &&
          cropBox.x1 - dp.y > cropBox.x0)
        cropBox.x1 -= dp.y;
      if ((m_scaling == e0M ||
           ((m_scaling == e00 || m_scaling == e01) && !e.isShiftPressed())) &&
          cropBox.y1 - dp.x > cropBox.y0)
        cropBox.y1 -= dp.x;
      if ((m_scaling == e1M ||
           ((m_scaling == e11 || m_scaling == e10) && !e.isShiftPressed())) &&
          cropBox.y0 - dp.x < cropBox.y1)
        cropBox.y0 -= dp.x;

      if (e.isShiftPressed()) {
        TPointD delta;
        delta.x = (fabs(dp.x) > fabs(dp.y)) ? dp.x : dp.y;
        delta.y = delta.x * scanArea.getLx() / scanArea.getLy();

        if (m_scaling == e11) {
          if (cropBox.y0 - delta.x < cropBox.y1) cropBox.y0 -= delta.x;
          if (cropBox.x0 - delta.y < cropBox.x1) cropBox.x0 -= delta.y;
        } else if (m_scaling == e00) {
          if (cropBox.y1 - delta.x > cropBox.y0) cropBox.y1 -= delta.x;
          if (cropBox.x1 - delta.y > cropBox.x0) cropBox.x1 -= delta.y;
        } else if (m_scaling == e01) {
          if (cropBox.y1 - delta.x > cropBox.y0) cropBox.y1 -= delta.x;
          if (cropBox.x0 - delta.y < cropBox.x1) cropBox.x0 -= delta.y;
        }
        if (m_scaling == e10) {
          if (cropBox.y0 - delta.x < cropBox.y1) cropBox.y0 -= delta.x;
          if (cropBox.x1 - delta.y > cropBox.x0) cropBox.x1 -= delta.y;
        }
      }
      cropBox *= scanArea;
      if (cropBox != m_parameters->getCropBox()) {
        m_parameters->setCropBox(cropBox);
        m_lastPos = pos;
      }
    } else if (m_scaling == eMove) {
      cropBox += TPointD(-dp.y, 0);
      if (scanArea.contains(cropBox)) m_parameters->setCropBox(cropBox);
      cropBox += TPointD(0, -dp.x);
      if (scanArea.contains(cropBox)) m_parameters->setCropBox(cropBox);
      m_lastPos = pos;
    }
    invalidate();
  }

  //-----------------------------------------------------------------------------

  int getCursorId() const override {
    switch (m_scaling) {
    case eNone:
      return ToolCursor::StrokeSelectCursor;
    case eMove:
      return ToolCursor::MoveCursor;
    case e11:
    case e00:
      return ToolCursor::ScaleCursor;
    case e10:
    case e01:
      return ToolCursor::ScaleInvCursor;
    case e1M:
    case e0M:
      return ToolCursor::ScaleHCursor;
    case eM1:
    case eM0:
      return ToolCursor::ScaleVCursor;
    default:
      assert(false);
    }
    return 0;
  }

  //-----------------------------------------------------------------------------

  void onActivate() override {
    m_parameters = TTool::getApplication()
                       ->getCurrentScene()
                       ->getScene()
                       ->getProperties()
                       ->getScanParameters();
  }

  void onEnter() override {
    m_parameters = TTool::getApplication()
                       ->getCurrentScene()
                       ->getScene()
                       ->getProperties()
                       ->getScanParameters();
  }

  //-----------------------------------------------------------------------------

private:
  TRectD rect2pix(const TRectD &rect) {
    const double f         = 25.4;
    TRectD scanArea        = m_parameters->getScanArea();
    TPointD scanAreaCenter = (scanArea.getP00() + scanArea.getP11()) * 0.5;
    double scaleFactor     = Stage::inch / previewDPI;

    double cbOffsetx =
        (((rect.x0 - scanAreaCenter.x) * previewDPI) / f) * scaleFactor;
    double cbOffsety =
        (((rect.y0 - scanAreaCenter.y) * previewDPI) / f) * scaleFactor;
    double cbSizelx = (((rect.x1 - rect.x0) * previewDPI) / f) * scaleFactor;
    double cbSizely = (((rect.y1 - rect.y0) * previewDPI) / f) * scaleFactor;

    TRectD rectPix(TPointD(cbOffsetx, cbOffsety),
                   TDimensionD(cbSizelx, cbSizely));
    return TRectD(-rectPix.y1, -rectPix.x1, -rectPix.y0, -rectPix.x0);
  }
} setScanCropboxTool;
