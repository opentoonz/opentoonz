

#include "preferencespopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "versioncontrol.h"
#include "levelsettingspopup.h"
#include "tapp.h"
#include "cleanupsettingsmodel.h"

// TnzQt includes
#include "toonzqt/tabbar.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/levelproperties.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/stage.h"

// TnzCore includes
#include "tsystem.h"
#include "tfont.h"

// Qt includes
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QLineEdit>
#include <QFileDialog>
#include <QFile>
#include <QPushButton>
#include <QApplication>
#include <QMainWindow>
#include <QStringList>
#include <QListWidget>
#include <QGroupBox>

using namespace DVGui;

//*******************************************************************************************
//    Local namespace
//*******************************************************************************************

namespace {

static const int unitsCount = 5, inchIdx = 2;
static const QString units[unitsCount] = {"cm", "mm", "inch", "field", "pixel"};
static const QString rooms[2]          = {"standard", "studioGhibli"};
enum DpiPolicy { DP_ImageDpi, DP_CustomDpi };

}  // namespace

//**********************************************************************************
//    PreferencesPopup::FormatProperties  implementation
//**********************************************************************************

PreferencesPopup::FormatProperties::FormatProperties(PreferencesPopup *parent)
    : DVGui::Dialog(parent, false, true) {
  setWindowTitle(tr("Level Settings by File Format"));
  setModal(true);  // The underlying selected format should not
  // be changed by the user

  // Main layout
  beginVLayout();

  QGridLayout *gridLayout = new QGridLayout;
  int row                 = 0;

  // Key values
  QLabel *nameLabel = new QLabel(tr("Name:"));
  nameLabel->setFixedHeight(20);  // Due to DVGui::Dialog's quirky behavior
  gridLayout->addWidget(nameLabel, row, 0, Qt::AlignRight);

  m_name = new DVGui::LineEdit;
  gridLayout->addWidget(m_name, row++, 1);

  QLabel *regExpLabel = new QLabel(tr("Regular Expression:"));
  gridLayout->addWidget(regExpLabel, row, 0, Qt::AlignRight);

  m_regExp = new DVGui::LineEdit;
  gridLayout->addWidget(m_regExp, row++, 1);

  QLabel *priorityLabel = new QLabel(tr("Priority"));
  gridLayout->addWidget(priorityLabel, row, 0, Qt::AlignRight);

  m_priority = new DVGui::IntLineEdit;
  gridLayout->addWidget(m_priority, row++, 1);

  gridLayout->setRowMinimumHeight(row++, 20);

  // LevelProperties
  m_dpiPolicy = new QComboBox;
  gridLayout->addWidget(m_dpiPolicy, row++, 1);

  m_dpiPolicy->addItem(QObject::tr("Image DPI"));
  m_dpiPolicy->addItem(QObject::tr("Custom DPI"));

  QLabel *dpiLabel = new QLabel(LevelSettingsPopup::tr("DPI:"));
  gridLayout->addWidget(dpiLabel, row, 0, Qt::AlignRight);

  m_dpi = new DVGui::DoubleLineEdit;
  m_dpi->setRange(1, (std::numeric_limits<double>::max)());  // Tried
                                                             // limits::min(),
                                                             // but input 0 was
  gridLayout->addWidget(m_dpi, row++,
                        1);  // then replaced with something * e^-128

  m_premultiply = new DVGui::CheckBox(LevelSettingsPopup::tr("Premultiply"));
  gridLayout->addWidget(m_premultiply, row++, 1);

  m_whiteTransp =
      new DVGui::CheckBox(LevelSettingsPopup::tr("White As Transparent"));
  gridLayout->addWidget(m_whiteTransp, row++, 1);

  m_doAntialias =
      new DVGui::CheckBox(LevelSettingsPopup::tr("Add Antialiasing"));
  gridLayout->addWidget(m_doAntialias, row++, 1);

  QLabel *antialiasLabel =
      new QLabel(LevelSettingsPopup::tr("Antialias Softness:"));
  gridLayout->addWidget(antialiasLabel, row, 0, Qt::AlignRight);

  m_antialias = new DVGui::IntLineEdit(
      this, 10, 0, 100);  // Tried 1, but then m_doAntialias was forcedly
  gridLayout->addWidget(m_antialias, row++, 1);  // initialized to true

  QLabel *subsamplingLabel = new QLabel(LevelSettingsPopup::tr("Subsampling:"));
  gridLayout->addWidget(subsamplingLabel, row, 0, Qt::AlignRight);

  m_subsampling = new DVGui::IntLineEdit(this, 1, 1);
  gridLayout->addWidget(m_subsampling, row++, 1);

  addLayout(gridLayout);

  endVLayout();

  // Establish connections
  bool ret = true;

  ret = connect(m_dpiPolicy, SIGNAL(currentIndexChanged(int)),
                SLOT(updateEnabledStatus())) &&
        ret;
  ret =
      connect(m_doAntialias, SIGNAL(clicked()), SLOT(updateEnabledStatus())) &&
      ret;

  assert(ret);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::updateEnabledStatus() {
  m_dpi->setEnabled(m_dpiPolicy->currentIndex() == DP_CustomDpi);
  m_antialias->setEnabled(m_doAntialias->isChecked());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::setLevelFormat(
    const Preferences::LevelFormat &lf) {
  const LevelOptions &lo = lf.m_options;

  m_name->setText(lf.m_name);
  m_regExp->setText(lf.m_pathFormat.pattern());
  m_priority->setValue(lf.m_priority);

  m_dpiPolicy->setCurrentIndex(
      lo.m_dpiPolicy == LevelOptions::DP_ImageDpi ? DP_ImageDpi : DP_CustomDpi);
  m_dpi->setValue(lo.m_dpi);
  m_premultiply->setChecked(lo.m_premultiply);
  m_whiteTransp->setChecked(lo.m_whiteTransp);
  m_doAntialias->setChecked(lo.m_antialias > 0);
  m_antialias->setValue(lo.m_antialias);
  m_subsampling->setValue(lo.m_subsampling);

  updateEnabledStatus();
}

//-----------------------------------------------------------------------------

Preferences::LevelFormat PreferencesPopup::FormatProperties::levelFormat()
    const {
  Preferences::LevelFormat lf(m_name->text());

  // Assign key values
  lf.m_pathFormat.setPattern(m_regExp->text());
  lf.m_priority = m_priority->getValue();

  // Assign level format values
  lf.m_options.m_dpiPolicy = (m_dpiPolicy->currentIndex() == DP_ImageDpi)
                                 ? LevelOptions::DP_ImageDpi
                                 : LevelOptions::DP_CustomDpi;
  lf.m_options.m_dpi         = m_dpi->getValue();
  lf.m_options.m_subsampling = m_subsampling->getValue();
  lf.m_options.m_antialias =
      m_doAntialias->isChecked() ? m_antialias->getValue() : 0;
  lf.m_options.m_premultiply = m_premultiply->isChecked();
  lf.m_options.m_whiteTransp = m_whiteTransp->isChecked();

  return lf;
}

//**********************************************************************************
//    PreferencesPopup  implementation
//**********************************************************************************

void PreferencesPopup::onPixelsOnlyChanged(int index) {
  bool enabled = index == Qt::Checked;
  if (enabled) {
    m_pref->setDefLevelDpi(Stage::standardDpi);
    m_pref->setPixelsOnly(true);
    TCamera *camera;
    camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimension camRes = camera->getRes();
    TDimensionD camSize;
    camSize.lx = camRes.lx / Stage::standardDpi;
    camSize.ly = camRes.ly / Stage::standardDpi;
    camera->setSize(camSize);
    TDimension cleanupRes = CleanupSettingsModel::instance()
                                ->getCurrentParameters()
                                ->m_camera.getRes();
    TDimensionD cleanupSize;
    cleanupSize.lx = cleanupRes.lx / Stage::standardDpi;
    cleanupSize.ly = cleanupRes.ly / Stage::standardDpi;
    CleanupSettingsModel::instance()->getCurrentParameters()->m_camera.setSize(
        cleanupSize);
    m_pref->storeOldUnits();
    if (m_unitOm->currentIndex() != 4) m_unitOm->setCurrentIndex(4);
    if (m_cameraUnitOm->currentIndex() != 4) m_cameraUnitOm->setCurrentIndex(4);
    m_unitOm->setDisabled(true);
    m_cameraUnitOm->setDisabled(true);
    m_defLevelDpi->setDisabled(true);
    m_defLevelDpi->setValue(Stage::standardDpi);
    m_defLevelWidth->setMeasure("camera.lx");
    m_defLevelHeight->setMeasure("camera.ly");
    m_defLevelWidth->setValue(m_pref->getDefLevelWidth());
    m_defLevelHeight->setValue(m_pref->getDefLevelHeight());
    m_defLevelHeight->setDecimals(0);
    m_defLevelWidth->setDecimals(0);

  } else {
    QString tempUnit;
    int unitIndex;
    tempUnit  = m_pref->getOldUnits();
    unitIndex = m_unitOm->findText(tempUnit);
    m_unitOm->setCurrentIndex(unitIndex);
    tempUnit  = m_pref->getOldCameraUnits();
    unitIndex = m_cameraUnitOm->findText(tempUnit);
    m_cameraUnitOm->setCurrentIndex(unitIndex);
    m_unitOm->setDisabled(false);
    m_cameraUnitOm->setDisabled(false);
    m_pref->setPixelsOnly(false);
    int levelType = m_pref->getDefLevelType();
    bool isRaster = levelType != PLI_XSHLEVEL;
    if (isRaster) {
      m_defLevelDpi->setDisabled(false);
    }
    m_defLevelHeight->setMeasure("level.ly");
    m_defLevelWidth->setMeasure("level.lx");
    m_defLevelWidth->setValue(m_pref->getDefLevelWidth());
    m_defLevelHeight->setValue(m_pref->getDefLevelHeight());
    m_defLevelHeight->setDecimals(4);
    m_defLevelWidth->setDecimals(4);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onProjectRootChanged() {
  int index = 0;
  if (m_projectRootStuff->isChecked()) index |= 0x08;
  if (m_projectRootDocuments->isChecked()) index |= 0x04;
  if (m_projectRootDesktop->isChecked()) index |= 0x02;
  if (m_projectRootCustom->isChecked()) index |= 0x01;
  m_pref->setProjectRoot(index);
  if (index & 0x01) {
    m_customProjectRootFileField->show();
    m_customProjectRootLabel->show();
    m_projectRootDirections->show();
  } else {
    m_customProjectRootFileField->hide();
    m_customProjectRootLabel->hide();
    m_projectRootDirections->hide();
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onCustomProjectRootChanged() {
  QString text = m_customProjectRootFileField->getPath();
  m_pref->setCustomProjectRoot(text.toStdWString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPixelUnitExternallySelected(bool on) {
  // call slot function onPixelsOnlyChanged() accordingly
  m_pixelsOnlyCB->setCheckState((on) ? Qt::Checked : Qt::Unchecked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onUnitChanged(int index) {
  if (index == 4 && m_pixelsOnlyCB->isChecked() == false) {
    m_pixelsOnlyCB->setCheckState(Qt::Checked);
  }
  m_pref->setUnits(::units[index].toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onCameraUnitChanged(int index) {
  if (index == 4 && m_pixelsOnlyCB->isChecked() == false) {
    m_pixelsOnlyCB->setChecked(true);
  }
  m_pref->setCameraUnits(::units[index].toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRoomChoiceChanged(int index) {
  TApp::instance()->writeSettings();
  m_pref->setCurrentRoomChoice(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInterfaceFontChanged(int index) {
  QString font = m_interfaceFont->currentText();
  m_pref->setInterfaceFont(font.toStdString());
  if (font.contains("Comic Sans"))
    DVGui::warning(tr("Life is too short for Comic Sans"));
  if (font.contains("Wingdings"))
    DVGui::warning(tr("Good luck.  You're on your own from here."));
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInterfaceFontWeightChanged(int index) {
  m_pref->setInterfaceFontWeight(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onImportPolicyChanged(int index) {
  m_pref->setDefaultImportPolicy(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onImportPolicyExternallyChanged(int policy) {
  // call slot function onImportPolicyChanged() accordingly
  m_importPolicy->setCurrentIndex(policy);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onScanLevelTypeChanged(const QString &text) {
  m_pref->setScanLevelType(text.toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMinuteChanged() {
  m_pref->setAutosavePeriod(m_minuteFld->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onChunkSizeChanged() {
  m_pref->setDefaultTaskChunkSize(m_chunkSizeFld->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankCountChanged() {
  if (m_blanksCount && m_blankColor)
    m_pref->setBlankValues(m_blanksCount->getValue(), m_blankColor->getColor());
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("BlankCount");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionPaperThicknessChanged() {
  if (m_onionPaperThickness) {
    m_pref->setOnionPaperThickness(m_onionPaperThickness->getValue());
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankColorChanged(const TPixel32 &, bool isDragging) {
  if (isDragging) return;

  if (m_blanksCount && m_blankColor)
    m_pref->setBlankValues(m_blanksCount->getValue(), m_blankColor->getColor());
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("BlankColor");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutomaticSVNRefreshChanged(int index) {
  m_pref->enableAutomaticSVNFolderRefresh(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSVNEnabledChanged(int index) {
  bool enabled = index == Qt::Checked;
  if (enabled) {
    if (VersionControl::instance()->testSetup())
      m_pref->enableSVN(true);
    else {
      if (m_enableVersionControl) m_enableVersionControl->setChecked(false);
    }
  } else
    m_pref->enableSVN(false);
}

//-----------------------------------------------------------------------------

void invalidateIcons();  // implemented in sceneviewer.cpp; in which fucking
                         // header  I can put this declaration?!

void PreferencesPopup::onTranspCheckDataChanged(const TPixel32 &,
                                                bool isDragging) {
  if (isDragging) return;

  m_pref->setTranspCheckData(m_transpCheckBgColor->getColor(),
                             m_transpCheckInkColor->getColor(),
                             m_transpCheckPaintColor->getColor());

  invalidateIcons();
}

//---------------------------------------------------------------------------------------

void PreferencesPopup::onOnionDataChanged(const TPixel32 &, bool isDragging) {
  if (isDragging) return;
  bool inksOnly            = false;
  if (m_inksOnly) inksOnly = m_inksOnly->isChecked();
  m_pref->setOnionData(m_frontOnionColor->getColor(),
                       m_backOnionColor->getColor(), inksOnly);

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelViewChange();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionDataChanged(int) {
  bool inksOnly            = false;
  if (m_inksOnly) inksOnly = m_inksOnly->isChecked();
  m_pref->setOnionData(m_frontOnionColor->getColor(),
                       m_backOnionColor->getColor(), inksOnly);

  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewValuesChanged() {
  m_pref->setViewValues(m_viewShrink->getValue(), m_viewStep->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onIconSizeChanged() {
  TDimension size(m_iconSizeLx->getValue(), m_iconSizeLy->getValue());
  if (m_pref->getIconSize() == size) return;

  m_pref->setIconSize(size);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoExposeChanged(int index) {
  m_pref->enableAutoExpose(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onIgnoreImageDpiChanged(int index) {
  m_pref->setIgnoreImageDpi(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSubsceneFolderChanged(int index) {
  m_pref->enableSubsceneFolder(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewGeneratedMovieChanged(int index) {
  m_pref->enableGeneratedMovieView(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onXsheetStepChanged() {
  m_pref->setXsheetStep(m_xsheetStep->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onXsheetAutopanChanged(int index) {
  m_pref->enableXsheetAutopan(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onIgnoreAlphaonColumn1Changed(int index) {
  m_pref->enableIgnoreAlphaonColumn1(index == Qt::Checked);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onRewindAfterPlayback(int index) {
  m_pref->enableRewindAfterPlayback(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFitToFlipbook(int index) {
  m_pref->enableFitToFlipbook(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPreviewAlwaysOpenNewFlip(int index) {
  m_pref->enablePreviewAlwaysOpenNewFlip(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAskForOverrideRender(int index) {
  m_pref->setAskForOverrideRender(index == Qt::Checked);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onRasterOptimizedMemoryChanged(int index) {
  if (m_pref->isRasterOptimizedMemory() == (index == Qt::Checked)) return;

  m_pref->enableRasterOptimizedMemory(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onGetFillOnlySavebox(int index) {
  if (m_pref->getFillOnlySavebox() == (index == Qt::Checked)) return;

  m_pref->setFillOnlySavebox(index == Qt::Checked);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSaveUnpaintedInCleanupChanged(int index) {
  if (m_pref->isSaveUnpaintedInCleanupEnable() == (index == Qt::Checked))
    return;

  m_pref->enableSaveUnpaintedInCleanup(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMinimizeSaveboxAfterEditing(int index) {
  m_pref->enableMinimizeSaveboxAfterEditing(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefaultViewerChanged(int index) {
  m_pref->enableDefaultViewer(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveChanged(bool on) {
  m_pref->enableAutosave(on);
  if (on && !m_autoSaveSceneCB->isChecked() &&
      !m_autoSaveOtherFilesCB->isChecked()) {
    m_autoSaveSceneCB->setChecked(true);
    m_autoSaveOtherFilesCB->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveSceneChanged(int index) {
  m_pref->enableAutosaveScene(index == Qt::Checked);
  if (!m_autoSaveOtherFilesCB->isChecked() && index == Qt::Unchecked) {
    m_autoSaveGroup->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveOtherFilesChanged(int index) {
  m_pref->enableAutosaveOtherFiles(index == Qt::Checked);
  if (!m_autoSaveSceneCB->isChecked() && index == Qt::Unchecked) {
    m_autoSaveGroup->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onStartupPopupChanged(int index) {
  m_pref->enableStartupPopup(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onKeyframeTypeChanged(int index) {
  m_pref->setKeyframeType(index + 2);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onAutocreationTypeChanged(int index) {
  m_pref->setAutocreationType(index);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onAnimationStepChanged() {
  m_pref->setAnimationStep(m_animationStepField->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLanguageTypeChanged(const QString &langName) {
  m_pref->setCurrentLanguage(langName);
  QString currentLanguage = m_pref->getCurrentLanguage();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMoveCurrentFrameChanged(int index) {
  m_pref->enableMoveCurrent(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onReplaceAfterSaveLevelAsChanged(int index) {
  m_pref->enableReplaceAfterSaveLevelAs(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setViewerBgColor(const TPixel32 &color,
                                        bool isDragging) {
  m_pref->setViewerBGColor(color, isDragging);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setPreviewBgColor(const TPixel32 &color,
                                         bool isDragging) {
  m_pref->setPreviewBGColor(color, isDragging);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setChessboardColor1(const TPixel32 &color,
                                           bool isDragging) {
  m_pref->setChessboardColor1(color, isDragging);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setChessboardColor2(const TPixel32 &color,
                                           bool isDragging) {
  m_pref->setChessboardColor2(color, isDragging);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onColumnIconChange(const QString &value) {
  m_pref->setColumnIconLoadingPolicy(value == tr("At Once")
                                         ? Preferences::LoadAtOnce
                                         : Preferences::LoadOnDemand);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionSkinVisibilityChanged(int index) {
  m_pref->enableOnionSkin(index == Qt::Checked);

  m_frontOnionColor->setEnabled(index == Qt::Checked);
  m_backOnionColor->setEnabled(index == Qt::Checked);
  m_inksOnly->setEnabled(index == Qt::Checked);
  m_onionPaperThickness->setEnabled(index == Qt::Checked);

  OnionSkinMask osm =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  osm.enable(index == Qt::Checked);
  TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
  TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionSkinDuringPlaybackChanged(int index) {
  m_pref->setOnionSkinDuringPlayback(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onGuidedDrawingStyleChanged(int index) {
  m_pref->setAnimatedGuidedDrawing(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onActualPixelOnSceneModeChanged(int index) {
  m_pref->enableActualPixelViewOnSceneEditingMode(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMultiLayerStylePickerChanged(int index) {
  m_pref->enableMultiLayerStylePicker(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelNameOnEachMarkerChanged(int index) {
  m_pref->enableLevelNameOnEachMarker(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInitialLoadTlvCachingBehaviorChanged(int index) {
  m_pref->setInitialLoadTlvCachingBehavior(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewerZoomCenterChanged(int index) {
  m_pref->setViewerZoomCenter(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRemoveSceneNumberFromLoadedLevelNameChanged(
    int index) {
  m_pref->enableRemoveSceneNumberFromLoadedLevelName(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowRasterImageDarkenBlendedInViewerChanged(
    int index) {
  m_pref->enableShowRasterImagesDarkenBlendedInViewer(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowFrameNumberWithLettersChanged(int index) {
  m_pref->enableShowFrameNumberWithLetters(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPaletteTypeForRasterColorModelChanged(int index) {
  m_pref->setPaletteTypeOnLoadRasterImageAsColorModel(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowKeyframesOnCellAreaChanged(int index) {
  m_pref->enableShowKeyframesOnXsheetCellArea(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onStyleSheetTypeChanged(const QString &styleSheetName) {
  m_pref->setCurrentStyleSheet(styleSheetName);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QString currentStyle = m_pref->getCurrentStyleSheetPath();
  qApp->setStyleSheet(currentStyle);
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------
/*
QWidget* PreferencesPopup::create(const QString &lbl, bool def, const char*slot)
{
  DVGui::CheckBox* cb = new DVGui::CheckBox(lbl);
  cb->setMaximumHeight(WidgetHeight);
  cb->setChecked(def);
  bool ret = connect(cb, SIGNAL(stateChanged (int)), slot);
  assert(ret);
  return cb;
}
*/
//-----------------------------------------------------------------------------

void PreferencesPopup::onUndoMemorySizeChanged() {
  int value = m_undoMemorySize->getValue();
  m_pref->setUndoMemorySize(value);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDragCellsBehaviourChanged(int index) {
  m_pref->setDragCellsBehaviour(index);
}

//-----------------------------------------------------------------------------

#ifdef LINETEST
void PreferencesPopup::onLineTestFpsCapture(int index) {
  if (index == 0) m_pref->setLineTestFpsCapture(0);
  if (index == 1)
    m_pref->setLineTestFpsCapture(25);
  else if (index == 2)
    m_pref->setLineTestFpsCapture(30);
}
#endif

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelsBackupChanged(int index) {
  m_pref->enableLevelsBackup(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSceneNumberingChanged(int index) {
  m_pref->enableSceneNumbering(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefLevelTypeChanged(int index) {
  if (0 <= index && index < m_defLevelType->count()) {
    int levelType = m_defLevelType->itemData(index).toInt();
    m_pref->setDefLevelType(levelType);
    bool isRaster =
        levelType != PLI_XSHLEVEL && !m_newLevelToCameraSizeCB->isChecked();
    m_defLevelWidth->setEnabled(isRaster);
    m_defLevelHeight->setEnabled(isRaster);
    if (!m_pixelsOnlyCB->isChecked()) m_defLevelDpi->setEnabled(isRaster);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefLevelParameterChanged() {
  double w   = m_defLevelWidth->getValue();
  double h   = m_defLevelHeight->getValue();
  double dpi = m_defLevelDpi->getValue();
  m_pref->setDefLevelWidth(w);
  m_pref->setDefLevelHeight(h);
  m_pref->setDefLevelDpi(dpi);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onVectorSnappingTargetChanged(int index) {
  m_vectorSnappingTargetCB->setCurrentIndex(index);
  m_pref->setVectorSnappingTarget(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::rebuildFormatsList() {
  const Preferences &prefs = *Preferences::instance();

  m_levelFormatNames->clear();

  int lf, lfCount = prefs.levelFormatsCount();
  for (lf = 0; lf != lfCount; ++lf)
    m_levelFormatNames->addItem(prefs.levelFormat(lf).m_name);

  m_editLevelFormat->setEnabled(m_levelFormatNames->count() > 0);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAddLevelFormat() {
  bool ok            = true;
  QString formatName = DVGui::getText(tr("New Level Format"),
                                      tr("Assign the new level format name:"),
                                      tr("New Format"), &ok);

  if (ok) {
    int formatIdx = Preferences::instance()->addLevelFormat(formatName);
    rebuildFormatsList();

    m_levelFormatNames->setCurrentIndex(formatIdx);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRemoveLevelFormat() {
  Preferences::instance()->removeLevelFormat(
      m_levelFormatNames->currentIndex());
  rebuildFormatsList();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onEditLevelFormat() {
  if (!m_formatProperties) {
    m_formatProperties = new FormatProperties(this);

    bool ret = connect(m_formatProperties, SIGNAL(dialogClosed()),
                       SLOT(onLevelFormatEdited()));
    assert(ret);
  }

  const Preferences::LevelFormat &lf =
      Preferences::instance()->levelFormat(m_levelFormatNames->currentIndex());

  m_formatProperties->setLevelFormat(lf);
  m_formatProperties->show();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelFormatEdited() {
  assert(m_formatProperties);

  Preferences &prefs = *Preferences::instance();
  int formatIdx      = m_levelFormatNames->currentIndex();

  prefs.removeLevelFormat(formatIdx);
  formatIdx = prefs.addLevelFormat(m_formatProperties->levelFormat());

  rebuildFormatsList();

  m_levelFormatNames->setCurrentIndex(formatIdx);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShow0ThickLinesChanged(int on) {
  m_pref->setShow0ThickLines(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRegionAntialiasChanged(int on) {
  m_pref->setRegionAntialias(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFfmpegPathChanged() {
  QString text = m_ffmpegPathFileFld->getPath();
  m_pref->setFfmpegPath(text.toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFastRenderPathChanged() {
  QString text = m_fastRenderPathFileField->getPath();
  m_pref->setFastRenderPath(text.toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFfmpegTimeoutChanged() {
  m_pref->setFfmpegTimeout(m_ffmpegTimeout->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onUseNumpadForSwitchingStylesClicked(bool checked) {
  if (checked) {
    // check if there are any commands with numpadkey shortcuts
    CommandManager *cm = CommandManager::instance();
    QList<QAction *> actionsList;
    for (int key = Qt::Key_0; key <= Qt::Key_9; key++) {
      std::string str = QKeySequence(key).toString().toStdString();
      QAction *action = cm->getActionFromShortcut(str);
      if (action) actionsList.append(action);
    }
    QAction *tabAction = cm->getActionFromShortcut("Tab");
    if (tabAction) actionsList.append(tabAction);
    tabAction = cm->getActionFromShortcut("Shift+Tab");
    if (tabAction) actionsList.append(tabAction);
    // if there are actions using numpad shortcuts, notify to release them.
    if (!actionsList.isEmpty()) {
      QString msgStr =
          tr("Numpad keys are assigned to the following commands.\nIs it OK to "
             "release these shortcuts?");
      for (int a = 0; a < actionsList.size(); a++) {
        msgStr += "\n" + actionsList.at(a)->iconText() + "  (" +
                  actionsList.at(a)->shortcut().toString() + ")";
      }
      int ret = DVGui::MsgBox(msgStr, tr("OK"), tr("Cancel"), 1);
      if (ret == 2 || ret == 0) {  // canceled
        m_useNumpadForSwitchingStyles->setChecked(false);
        return;
      } else {  // accepted, then release shortcuts
        for (int a = 0; a < actionsList.size(); a++)
          cm->setShortcut(actionsList[a], "");
      }
    }
  }
  m_pref->enableUseNumpadForSwitchingStyles(checked);
  // emit signal to update Palette and Viewer
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "NumpadForSwitchingStyles");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onNewLevelToCameraSizeChanged(bool checked) {
  m_pref->enableNewLevelSizeToCameraSize(checked);
  onDefLevelTypeChanged(m_defLevelType->currentIndex());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowXSheetToolbarClicked(bool checked) {
  m_pref->enableShowXSheetToolbar(checked);
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("XSheetToolbar");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onExpandFunctionHeaderClicked(bool checked) {
  m_pref->enableExpandFunctionHeader(checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowColumnNumbersChanged(int index) {
  m_pref->enableShowColumnNumbers(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onXsheetLayoutChanged(const QString &text) {
  m_pref->setXsheetLayoutPreference(text.toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onUseArrowKeyToShiftCellSelectionClicked(int on) {
  m_pref->enableUseArrowKeyToShiftCellSelection(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInputCellsWithoutDoubleClickingClicked(int on) {
  m_pref->enableInputCellsWithoutDoubleClicking(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShortcutCommandsWhileRenamingCellClicked(int on) {
  m_pref->enableShortcutCommandsWhileRenamingCell(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onWatchFileSystemClicked(int on) {
  m_pref->enableWatchFileSystem(on);
  // emit signal to update behavior of the File browser
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "WatchFileSystem");
}

//**********************************************************************************
//    PrefencesPopup's  constructor
//**********************************************************************************

PreferencesPopup::PreferencesPopup()
    : QDialog(TApp::instance()->getMainWindow())
    , m_formatProperties()
    , m_inksOnly(0)
    , m_blanksCount(0)
    , m_blankColor(0) {
  setWindowTitle(tr("Preferences"));
  setObjectName("PreferencesPopup");

  m_pref = Preferences::instance();

  // Category List
  QListWidget *categoryList = new QListWidget(this);

  QStackedWidget *stackedWidget = new QStackedWidget(this);

  //---General ------------------------------
  categoryList->addItem(tr("General"));

  CheckBox *useDefaultViewerCB =
      new CheckBox(tr("Use Default Viewer for Movie Formats"), this);
  CheckBox *minimizeRasterMemoryCB =
      new CheckBox(tr("Minimize Raster Memory Fragmentation *"), this);
  m_autoSaveGroup = new QGroupBox(tr("Save Automatically"), this);
  m_autoSaveGroup->setCheckable(true);
  m_autoSaveSceneCB = new CheckBox(tr("Automatically Save the Scene File"));
  m_autoSaveOtherFilesCB =
      new CheckBox(tr("Automatically Save Non-Scene Files"));
  CheckBox *startupPopupCB =
      new CheckBox(tr("Show Startup Window when OpenToonz Starts"));
  m_minuteFld = new DVGui::IntLineEdit(this, 15, 1, 60);
  CheckBox *replaceAfterSaveLevelAsCB =
      new CheckBox(tr("Replace Toonz Level after SaveLevelAs command"), this);

  m_cellsDragBehaviour = new QComboBox();
  m_undoMemorySize =
      new DVGui::IntLineEdit(this, m_pref->getUndoMemorySize(), 0, 2000);
  m_levelsBackup = new CheckBox(tr("Backup Animation Levels when Saving"));
  m_chunkSizeFld =
      new DVGui::IntLineEdit(this, m_pref->getDefaultTaskChunkSize(), 1, 2000);
  CheckBox *sceneNumberingCB = new CheckBox(tr("Show Info in Rendered Frames"));
  CheckBox *watchFileSystemCB = new CheckBox(
      tr("Watch File System and Update File Browser Automatically"), this);

  m_projectRootDocuments = new CheckBox(tr("My Documents/OpenToonz*"), this);
  m_projectRootDesktop   = new CheckBox(tr("Desktop/OpenToonz*"), this);
  m_projectRootStuff     = new CheckBox(tr("Stuff Folder*"), this);
  m_projectRootCustom    = new CheckBox(tr("Custom*"), this);
  m_customProjectRootFileField = new DVGui::FileField(this, QString(""));
  m_customProjectRootLabel     = new QLabel(tr("Custom Project Path(s): "));
  m_projectRootDirections      = new QLabel(
      tr("Advanced: Multiple paths can be separated by ** (No Spaces)"));

  QLabel *note_general =
      new QLabel(tr("* Changes will take effect the next time you run Toonz"));
  note_general->setStyleSheet("font-size: 10px; font: italic;");

  //--- Interface ------------------------------
  categoryList->addItem(tr("Interface"));

  QComboBox *languageType = 0;
  QStringList languageList;
  int currentIndex = 0;
  for (int i = 0; i < m_pref->getLanguageCount(); i++) {
    QString string = m_pref->getLanguage(i);
    if (string == m_pref->getCurrentLanguage()) currentIndex = i;
    languageList.push_back(string);
  }

  if (languageList.size() > 1) {
    languageType = new QComboBox(this);
    languageType->addItems(languageList);
    languageType->setCurrentIndex(currentIndex);
  }
  QComboBox *styleSheetType = new QComboBox(this);
  m_pixelsOnlyCB =
      new CheckBox(tr("All imported images will use the same DPI"), this);
  m_unitOm       = new QComboBox(this);
  m_cameraUnitOm = new QComboBox(this);

  // Choose between standard and Studio Ghibli rooms
  QComboBox *roomChoice = new QComboBox(this);

  m_iconSizeLx = new DVGui::IntLineEdit(this, 80, 10, 400);
  m_iconSizeLy = new DVGui::IntLineEdit(this, 60, 10, 400);
  m_viewShrink = new DVGui::IntLineEdit(this, 1, 1, 20);
  m_viewStep   = new DVGui::IntLineEdit(this, 1, 1, 20);

  CheckBox *moveCurrentFrameCB =
      new CheckBox(tr("Move Current Frame by Clicking on Xsheet / Numerical "
                      "Columns Cell Area"),
                   this);
  // Viewer BG color
  m_viewerBgColorFld = new ColorField(this, false, m_pref->getViewerBgColor());
  // Preview BG color
  m_previewBgColorFld =
      new ColorField(this, false, m_pref->getPreviewBgColor());
  // bg chessboard colors
  TPixel32 col1, col2;
  m_pref->getChessboardColors(col1, col2);
  m_chessboardColor1Fld = new ColorField(this, false, col1);
  m_chessboardColor2Fld = new ColorField(this, false, col2);
  CheckBox *openFlipbookAfterCB =
      new CheckBox(tr("Open Flipbook after Rendering"), this);
  CheckBox *actualPixelOnSceneModeCB =
      new CheckBox(tr("Enable Actual Pixel View on Scene Editing Mode"), this);
  CheckBox *levelNameOnEachMarkerCB =
      new CheckBox(tr("Display Level Name on Each Marker"), this);
  CheckBox *showRasterImageDarkenBlendedInViewerCB = new CheckBox(
      tr("Show Raster Images Darken Blended in Camstand View"), this);
  CheckBox *showShowFrameNumberWithLettersCB = new CheckBox(
      tr("Show \"ABC\" Appendix to the Frame Number in Xsheet Cell"), this);
  QComboBox *viewerZoomCenterComboBox = new QComboBox(this);

  QLabel *note_interface =
      new QLabel(tr("* Changes will take effect the next time you run Toonz"));
  note_interface->setStyleSheet("font-size: 10px; font: italic;");

  m_interfaceFont       = new QComboBox(this);
  m_interfaceFontWeight = new QComboBox(this);

  //--- Visualization ------------------------------
  categoryList->addItem(tr("Visualization"));
  CheckBox *show0ThickLinesCB =
      new CheckBox(tr("Show Lines with Thickness 0"), this);
  CheckBox *regionAntialiasCB =
      new CheckBox(tr("Antialiased region boundaries"), this);

  //--- Loading ------------------------------
  categoryList->addItem(tr("Loading"));

  CheckBox *exposeLoadedLevelsCB =
      new CheckBox(tr("Expose Loaded Levels in Xsheet"), this);
  CheckBox *createSubfolderCB =
      new CheckBox(tr("Create Sub-folder when Importing Sub-xsheet"), this);
  CheckBox *m_ignoreImageDpiCB =
      new CheckBox(tr("Use Camera DPI for All Imported Images"), this);
  // Column Icon
  m_columnIconOm                                   = new QComboBox(this);
  QComboBox *initialLoadTlvCachingBehaviorComboBox = new QComboBox(this);
  CheckBox *removeSceneNumberFromLoadedLevelNameCB = new CheckBox(
      tr("Automatically Remove Scene Number from Loaded Level Name"), this);

  m_levelFormatNames  = new QComboBox;
  m_addLevelFormat    = new QPushButton("+");
  m_removeLevelFormat = new QPushButton("-");
  m_editLevelFormat   = new QPushButton(tr("Edit"));

  QComboBox *paletteTypeForRasterColorModelComboBox = new QComboBox(this);

  m_importPolicy = new QComboBox;

  //--- Import/Export ------------------------------
  categoryList->addItem(tr("Import/Export"));
  m_ffmpegPathFileFld = new DVGui::FileField(this, QString(""));
  m_fastRenderPathFileField =
      new DVGui::FileField(this, QString("desktop"), false, true);
  m_ffmpegTimeout = new DVGui::IntLineEdit(this, 30, 1);

  QLabel *note_io =
      new QLabel(tr("* Changes will take effect the next time you run Toonz"));
  note_io->setStyleSheet("font-size: 10px; font: italic;");

  //--- Drawing ------------------------------
  categoryList->addItem(tr("Drawing"));

  m_defScanLevelType       = new QComboBox(this);
  m_defLevelType           = new QComboBox(this);
  m_defLevelWidth          = new MeasuredDoubleLineEdit(0);
  m_defLevelHeight         = new MeasuredDoubleLineEdit(0);
  m_defLevelDpi            = new DoubleLineEdit(0, 66.76);
  m_autocreationType       = new QComboBox(this);
  m_dpiLabel               = new QLabel(tr("DPI:"), this);
  m_vectorSnappingTargetCB = new QComboBox(this);
  m_newLevelToCameraSizeCB =
      new CheckBox(tr("New Levels Default to the Current Camera Size"), this);

  CheckBox *keepOriginalCleanedUpCB =
      new CheckBox(tr("Keep Original Cleaned Up Drawings As Backup"), this);
  CheckBox *multiLayerStylePickerCB = new CheckBox(
      tr("Multi Layer Style Picker : Switch Levels by Picking"), this);
  CheckBox *useSaveboxToLimitFillingOpCB =
      new CheckBox(tr("Use the TLV Savebox to Limit Filling Operations"), this);
  CheckBox *minimizeSaveboxAfterEditingCB =
      new CheckBox(tr("Minimize Savebox after Editing"), this);
  m_useNumpadForSwitchingStyles =
      new CheckBox(tr("Use Numpad and Tab keys for Switching Styles"), this);

  //--- Xsheet ------------------------------
  categoryList->addItem(tr("Xsheet"));

  CheckBox *xsheetAutopanDuringPlaybackCB =
      new CheckBox(tr("Xsheet Autopan during Playback"), this);
  m_xsheetStep =
      new DVGui::IntLineEdit(this, Preferences::instance()->getXsheetStep(), 0);
  m_cellsDragBehaviour = new QComboBox();
  CheckBox *ignoreAlphaonColumn1CB =
      new CheckBox(tr("Ignore Alpha Channel on Levels in Column 1"), this);
  CheckBox *showKeyframesOnCellAreaCB =
      new CheckBox(tr("Show Keyframes on Cell Area"), this);
  CheckBox *useArrowKeyToShiftCellSelectionCB =
      new CheckBox(tr("Use Arrow Key to Shift Cell Selection"), this);
  CheckBox *inputCellsWithoutDoubleClickingCB =
      new CheckBox(tr("Enable to Input Cells without Double Clicking"), this);
  CheckBox *shortcutCommandsWhileRenamingCellCB = new CheckBox(
      tr("Enable OpenToonz Commands' Shortcut Keys While Renaming Cell"), this);
  m_showXSheetToolbar = new QGroupBox(tr("Show Toolbar in the XSheet "), this);
  m_showXSheetToolbar->setCheckable(true);
  m_expandFunctionHeader = new CheckBox(
      tr("Expand Function Editor Header to Match XSheet Toolbar Height "
         "(Requires Restart)"),
      this);
  CheckBox *showColumnNumbersCB =
      new CheckBox(tr("Show Column Numbers in Column Headers"), this);
  QStringList xsheetLayouts;
  xsheetLayouts << tr("Classic") << tr("Classic-revised") << tr("Compact");
  QComboBox *xsheetLayoutOptions = new QComboBox(this);
  xsheetLayoutOptions->addItems(xsheetLayouts);
  xsheetLayoutOptions->setCurrentIndex(
      xsheetLayoutOptions->findText(m_pref->getXsheetLayoutPreference()));

  QLabel *note_xsheet =
      new QLabel(tr("* Changes will take effect the next time you run Toonz"));
  note_xsheet->setStyleSheet("font-size: 10px; font: italic;");

  //--- Animation ------------------------------
  categoryList->addItem(tr("Animation"));

  m_keyframeType       = new QComboBox(this);
  m_animationStepField = new DVGui::IntLineEdit(this, 1, 1, 500);

  //--- Preview ------------------------------
  categoryList->addItem(tr("Preview"));

  m_blanksCount = new DVGui::IntLineEdit(this, 0, 0, 1000);
  m_blankColor  = new ColorField(this, false, TPixel::Black);
  CheckBox *rewindAfterPlaybackCB =
      new CheckBox(tr("Rewind after Playback"), this);
  CheckBox *displayInNewFlipBookCB =
      new CheckBox(tr("Display in a New Flipbook Window"), this);
  CheckBox *fitToFlipbookCB = new CheckBox(tr("Fit to Flipbook"), this);

  //--- Onion Skin ------------------------------
  categoryList->addItem(tr("Onion Skin"));

  TPixel32 frontColor, backColor;
  bool onlyInks;
  m_pref->getOnionData(frontColor, backColor, onlyInks);
  m_onionSkinVisibility = new CheckBox(tr("Onion Skin ON"));
  m_onionSkinDuringPlayback =
      new CheckBox(tr("Show Onion Skin During Playback"));
  m_frontOnionColor = new ColorField(this, false, frontColor);
  m_backOnionColor  = new ColorField(this, false, backColor);
  m_inksOnly        = new DVGui::CheckBox(tr("Display Lines Only "));
  m_inksOnly->setChecked(onlyInks);

  int thickness         = m_pref->getOnionPaperThickness();
  m_onionPaperThickness = new DVGui::IntLineEdit(this, thickness, 0, 100);
  m_guidedDrawingStyle  = new QComboBox(this);

  //--- Transparency Check ------------------------------
  categoryList->addItem(tr("Transparency Check"));

  TPixel32 bgColor, inkColor, paintColor;
  m_pref->getTranspCheckData(bgColor, inkColor, paintColor);
  m_transpCheckInkColor   = new ColorField(this, false, inkColor);
  m_transpCheckBgColor    = new ColorField(this, false, bgColor);
  m_transpCheckPaintColor = new ColorField(this, false, paintColor);

  //--- Version Control ------------------------------
  categoryList->addItem(tr("Version Control"));
  m_enableVersionControl = new DVGui::CheckBox(tr("Enable Version Control*"));
  CheckBox *autoRefreshFolderContentsCB =
      new CheckBox(tr("Automatically Refresh Folder Contents"), this);

  QLabel *note_version =
      new QLabel(tr("* Changes will take effect the next time you run Toonz"));
  note_version->setStyleSheet("font-size: 10px; font: italic;");

  /*--- set default parameters ---*/
  categoryList->setFixedWidth(160);
  categoryList->setCurrentRow(0);
  categoryList->setAlternatingRowColors(true);
  //--- General ------------------------------
  useDefaultViewerCB->setChecked(m_pref->isDefaultViewerEnabled());
  minimizeRasterMemoryCB->setChecked(m_pref->isRasterOptimizedMemory());
  m_autoSaveGroup->setChecked(m_pref->isAutosaveEnabled());
  m_autoSaveSceneCB->setChecked(m_pref->isAutosaveSceneEnabled());
  m_autoSaveOtherFilesCB->setChecked(m_pref->isAutosaveOtherFilesEnabled());
  m_minuteFld->setValue(m_pref->getAutosavePeriod());
  startupPopupCB->setChecked(m_pref->isStartupPopupEnabled());
  replaceAfterSaveLevelAsCB->setChecked(
      m_pref->isReplaceAfterSaveLevelAsEnabled());

  QStringList dragCellsBehaviourList;
  dragCellsBehaviourList << tr("Cells Only") << tr("Cells and Column Data");
  m_cellsDragBehaviour->addItems(dragCellsBehaviourList);
  m_cellsDragBehaviour->setCurrentIndex(m_pref->getDragCellsBehaviour());
  m_levelsBackup->setChecked(m_pref->isLevelsBackupEnabled());
  sceneNumberingCB->setChecked(m_pref->isSceneNumberingEnabled());
  watchFileSystemCB->setChecked(m_pref->isWatchFileSystemEnabled());

  m_customProjectRootFileField->setPath(m_pref->getCustomProjectRoot());

  int projectPaths = m_pref->getProjectRoot();
  m_projectRootStuff->setChecked(projectPaths & 0x08);
  m_projectRootDocuments->setChecked(projectPaths & 0x04);
  m_projectRootDesktop->setChecked(projectPaths & 0x02);
  m_projectRootCustom->setChecked(projectPaths & 0x01);

  m_projectRootStuff->hide();
  if (!(projectPaths & 0x01)) {
    m_customProjectRootFileField->hide();
    m_customProjectRootLabel->hide();
    m_projectRootDirections->hide();
  }
  //--- Interface ------------------------------
  QStringList styleSheetList;
  currentIndex = 0;
  for (int i = 0; i < m_pref->getStyleSheetCount(); i++) {
    QString string = m_pref->getStyleSheet(i);
    if (string == m_pref->getCurrentStyleSheetName()) currentIndex = i;
    TFilePath path(string.toStdWString());
    styleSheetList.push_back(QString::fromStdWString(path.getWideName()));
  }
  styleSheetType->addItems(styleSheetList);
  styleSheetType->setCurrentIndex(currentIndex);
  bool po = m_pref->getPixelsOnly();
  m_pixelsOnlyCB->setChecked(po);
  // m_pixelsOnlyCB->setChecked(true);
  if (po) {
    m_unitOm->setDisabled(true);
    m_cameraUnitOm->setDisabled(true);
  }
  QStringList type;
  type << tr("cm") << tr("mm") << tr("inch") << tr("field") << tr("pixel");
  m_unitOm->addItems(type);
  int idx =
      std::find(::units, ::units + ::unitsCount, m_pref->getUnits()) - ::units;
  m_unitOm->setCurrentIndex((idx < ::unitsCount) ? idx : ::inchIdx);
  m_cameraUnitOm->addItems(type);

  idx = std::find(::units, ::units + ::unitsCount, m_pref->getCameraUnits()) -
        ::units;
  m_cameraUnitOm->setCurrentIndex((idx < ::unitsCount) ? idx : ::inchIdx);

  QStringList roomList;
  int currentRoomIndex = 0;
  for (int i = 0; i < m_pref->getRoomChoiceCount(); i++) {
    QString string = m_pref->getRoomChoice(i);
    if (string == m_pref->getCurrentRoomChoice()) currentRoomIndex = i;
    roomList.push_back(string);
  }

  if (roomList.size() > 1) {
    roomChoice->addItems(roomList);
    roomChoice->setCurrentIndex(currentRoomIndex);
  }

  m_iconSizeLx->setValue(m_pref->getIconSize().lx);
  m_iconSizeLy->setValue(m_pref->getIconSize().ly);
  int shrink, step;
  m_pref->getViewValues(shrink, step);
  m_viewShrink->setValue(shrink);
  m_viewStep->setValue(step);
  moveCurrentFrameCB->setChecked(m_pref->isMoveCurrentEnabled());
  openFlipbookAfterCB->setChecked(m_pref->isGeneratedMovieViewEnabled());
  actualPixelOnSceneModeCB->setChecked(
      m_pref->isActualPixelViewOnSceneEditingModeEnabled());
  levelNameOnEachMarkerCB->setChecked(m_pref->isLevelNameOnEachMarkerEnabled());
  showRasterImageDarkenBlendedInViewerCB->setChecked(
      m_pref->isShowRasterImagesDarkenBlendedInViewerEnabled());
  showShowFrameNumberWithLettersCB->setChecked(
      m_pref->isShowFrameNumberWithLettersEnabled());
  QStringList zoomCenters;
  zoomCenters << tr("Mouse Cursor") << tr("Viewer Center");
  viewerZoomCenterComboBox->addItems(zoomCenters);
  viewerZoomCenterComboBox->setCurrentIndex(m_pref->getViewerZoomCenter());

  TFontManager *instance = TFontManager::instance();
  bool validFonts;
  try {
    instance->loadFontNames();
    validFonts = true;
  } catch (TFontLibraryLoadingError &) {
    validFonts = false;
    //    TMessage::error(toString(e.getMessage()));
  }

  if (validFonts) {
    std::vector<std::wstring> names;
    instance->getAllFamilies(names);

    for (std::vector<std::wstring>::iterator it = names.begin();
         it != names.end(); ++it)
      m_interfaceFont->addItem(QString::fromStdWString(*it));

    m_interfaceFont->setCurrentText(m_pref->getInterfaceFont());
  }

  QStringList fontStyles;
  fontStyles << "Regular"
             << "Bold";
  m_interfaceFontWeight->addItems(fontStyles);
  m_interfaceFontWeight->setCurrentIndex(m_pref->getInterfaceFontWeight());

  //--- Visualization ------------------------------
  show0ThickLinesCB->setChecked(m_pref->getShow0ThickLines());
  regionAntialiasCB->setChecked(m_pref->getRegionAntialias());

  //--- Loading ------------------------------
  exposeLoadedLevelsCB->setChecked(m_pref->isAutoExposeEnabled());
  m_ignoreImageDpiCB->setChecked(m_pref->isIgnoreImageDpiEnabled());
  QStringList behaviors;
  behaviors << tr("On Demand") << tr("All Icons") << tr("All Icons & Images");
  initialLoadTlvCachingBehaviorComboBox->addItems(behaviors);
  initialLoadTlvCachingBehaviorComboBox->setCurrentIndex(
      m_pref->getInitialLoadTlvCachingBehavior());
  QStringList formats;
  formats << tr("At Once") << tr("On Demand");
  m_columnIconOm->addItems(formats);
  if (m_pref->getColumnIconLoadingPolicy() == Preferences::LoadAtOnce)
    m_columnIconOm->setCurrentIndex(m_columnIconOm->findText(tr("At Once")));
  else
    m_columnIconOm->setCurrentIndex(m_columnIconOm->findText(tr("On Demand")));
  removeSceneNumberFromLoadedLevelNameCB->setChecked(
      m_pref->isRemoveSceneNumberFromLoadedLevelNameEnabled());
  createSubfolderCB->setChecked(m_pref->isSubsceneFolderEnabled());

  m_addLevelFormat->setFixedSize(20, 20);
  m_removeLevelFormat->setFixedSize(20, 20);

  rebuildFormatsList();

  QStringList paletteTypes;
  paletteTypes << tr("Pick Every Colors as Different Styles")
               << tr("Integrate Similar Colors as One Style");
  paletteTypeForRasterColorModelComboBox->addItems(paletteTypes);
  paletteTypeForRasterColorModelComboBox->setCurrentIndex(
      m_pref->getPaletteTypeOnLoadRasterImageAsColorModel());

  QStringList policies;
  policies << tr("Always ask before loading or importing")
           << tr("Always import the file to the current project")
           << tr("Always load the file from the current location");
  m_importPolicy->addItems(policies);
  m_importPolicy->setCurrentIndex(m_pref->getDefaultImportPolicy());

  //--- Import/Export ------------------------------
  QString path = m_pref->getFfmpegPath();
  m_ffmpegPathFileFld->setPath(path);
  path = m_pref->getFastRenderPath();
  m_fastRenderPathFileField->setPath(path);
  m_ffmpegTimeout->setValue(m_pref->getFfmpegTimeout());

  //--- Drawing ------------------------------
  keepOriginalCleanedUpCB->setChecked(m_pref->isSaveUnpaintedInCleanupEnable());
  multiLayerStylePickerCB->setChecked(m_pref->isMultiLayerStylePickerEnabled());
  minimizeSaveboxAfterEditingCB->setChecked(
      m_pref->isMinimizeSaveboxAfterEditing());
  useSaveboxToLimitFillingOpCB->setChecked(m_pref->getFillOnlySavebox());
  m_useNumpadForSwitchingStyles->setChecked(
      m_pref->isUseNumpadForSwitchingStylesEnabled());
  m_newLevelToCameraSizeCB->setChecked(
      m_pref->isNewLevelSizeToCameraSizeEnabled());
  QStringList scanLevelTypes;
  scanLevelTypes << "tif"
                 << "png";
  m_defScanLevelType->addItems(scanLevelTypes);
  m_defScanLevelType->setCurrentIndex(
      m_defScanLevelType->findText(m_pref->getScanLevelType()));
  QStringList levelTypes;
  m_defLevelType->addItem((tr("Toonz Vector Level")), PLI_XSHLEVEL);
  m_defLevelType->addItem((tr("Toonz Raster Level")), TZP_XSHLEVEL);
  m_defLevelType->addItem((tr("Raster Level")), OVL_XSHLEVEL);
  m_defLevelType->setCurrentIndex(0);

  for (int i = 0; i < m_defLevelType->count(); i++) {
    if (m_defLevelType->itemData(i).toInt() == m_pref->getDefLevelType()) {
      m_defLevelType->setCurrentIndex(i);
      onDefLevelTypeChanged(i);
      break;
    }
  }

  if (Preferences::instance()->getUnits() == "pixel") {
    m_defLevelWidth->setRange(0.1, (std::numeric_limits<double>::max)());
    m_defLevelWidth->setMeasure("camera.lx");
    m_defLevelWidth->setValue(m_pref->getDefLevelWidth());
    m_defLevelHeight->setRange(0.1, (std::numeric_limits<double>::max)());
    m_defLevelHeight->setMeasure("camera.ly");
    m_defLevelHeight->setValue(m_pref->getDefLevelHeight());
  } else {
    m_defLevelWidth->setRange(0.1, (std::numeric_limits<double>::max)());
    m_defLevelWidth->setMeasure("level.lx");
    m_defLevelWidth->setValue(m_pref->getDefLevelWidth());
    m_defLevelHeight->setRange(0.1, (std::numeric_limits<double>::max)());
    m_defLevelHeight->setMeasure("level.ly");
    m_defLevelHeight->setValue(m_pref->getDefLevelHeight());
  }
  m_defLevelDpi->setRange(0.1, (std::numeric_limits<double>::max)());
  m_defLevelDpi->setValue(m_pref->getDefLevelDpi());
  QStringList autocreationTypes;
  autocreationTypes << tr("Disabled") << tr("Enabled")
                    << tr("Use Xsheet as Animation Sheet");
  m_autocreationType->addItems(autocreationTypes);
  int autocreationType = m_pref->getAutocreationType();
  m_autocreationType->setCurrentIndex(autocreationType);

  QStringList vectorSnappingTargets;
  vectorSnappingTargets << tr("Strokes") << tr("Guides") << tr("All");
  m_vectorSnappingTargetCB->addItems(vectorSnappingTargets);
  m_vectorSnappingTargetCB->setCurrentIndex(m_pref->getVectorSnappingTarget());

  //--- Xsheet ------------------------------
  xsheetAutopanDuringPlaybackCB->setChecked(m_pref->isXsheetAutopanEnabled());
  m_cellsDragBehaviour->addItem(tr("Cells Only"));
  m_cellsDragBehaviour->addItem(tr("Cells and Column Data"));
  m_cellsDragBehaviour->setCurrentIndex(m_pref->getDragCellsBehaviour());
  ignoreAlphaonColumn1CB->setChecked(m_pref->isIgnoreAlphaonColumn1Enabled());
  showKeyframesOnCellAreaCB->setChecked(
      m_pref->isShowKeyframesOnXsheetCellAreaEnabled());
  useArrowKeyToShiftCellSelectionCB->setChecked(
      m_pref->isUseArrowKeyToShiftCellSelectionEnabled());
  inputCellsWithoutDoubleClickingCB->setChecked(
      m_pref->isInputCellsWithoutDoubleClickingEnabled());
  shortcutCommandsWhileRenamingCellCB->setChecked(
      m_pref->isShortcutCommandsWhileRenamingCellEnabled());
  m_showXSheetToolbar->setChecked(m_pref->isShowXSheetToolbarEnabled());
  m_expandFunctionHeader->setChecked(m_pref->isExpandFunctionHeaderEnabled());
  showColumnNumbersCB->setChecked(m_pref->isShowColumnNumbersEnabled());

  //--- Animation ------------------------------
  QStringList list;
  list << tr("Linear") << tr("Speed In / Speed Out") << tr("Ease In / Ease Out")
       << tr("Ease In / Ease Out %");
  m_keyframeType->addItems(list);
  int keyframeType = m_pref->getKeyframeType();
  m_keyframeType->setCurrentIndex(keyframeType - 2);
  m_animationStepField->setValue(m_pref->getAnimationStep());

  //--- Preview ------------------------------
  int blanksCount;
  TPixel32 blankColor;
  m_pref->getBlankValues(blanksCount, blankColor);
  m_blanksCount->setValue(blanksCount);
  m_blankColor->setColor(blankColor);

  rewindAfterPlaybackCB->setChecked(m_pref->rewindAfterPlaybackEnabled());
  displayInNewFlipBookCB->setChecked(m_pref->previewAlwaysOpenNewFlipEnabled());
  fitToFlipbookCB->setChecked(m_pref->fitToFlipbookEnabled());

  //--- Onion Skin ------------------------------
  m_onionSkinVisibility->setChecked(m_pref->isOnionSkinEnabled());
  m_onionSkinDuringPlayback->setChecked(m_pref->getOnionSkinDuringPlayback());
  m_frontOnionColor->setEnabled(m_pref->isOnionSkinEnabled());
  m_backOnionColor->setEnabled(m_pref->isOnionSkinEnabled());
  m_inksOnly->setEnabled(m_pref->isOnionSkinEnabled());
  QStringList guidedDrawingStyles;
  guidedDrawingStyles << tr("Arrow Markers") << tr("Animated Guide");
  m_guidedDrawingStyle->addItems(guidedDrawingStyles);
  m_guidedDrawingStyle->setCurrentIndex(m_pref->getAnimatedGuidedDrawing());

  //--- Version Control ------------------------------
  m_enableVersionControl->setChecked(m_pref->isSVNEnabled());
  autoRefreshFolderContentsCB->setChecked(
      m_pref->isAutomaticSVNFolderRefreshEnabled());

  /*--- layout ---*/

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  {
    // Category
    QVBoxLayout *categoryLayout = new QVBoxLayout();
    categoryLayout->setMargin(5);
    categoryLayout->setSpacing(10);
    {
      categoryLayout->addWidget(new QLabel(tr("Category"), this), 0,
                                Qt::AlignLeft | Qt::AlignVCenter);
      categoryLayout->addWidget(categoryList, 1);
    }
    mainLayout->addLayout(categoryLayout, 0);

    //--- General --------------------------
    QWidget *generalBox          = new QWidget(this);
    QVBoxLayout *generalFrameLay = new QVBoxLayout();
    generalFrameLay->setMargin(15);
    generalFrameLay->setSpacing(10);
    {
      generalFrameLay->addWidget(useDefaultViewerCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      generalFrameLay->addWidget(minimizeRasterMemoryCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);

      QVBoxLayout *autoSaveOptionsLay = new QVBoxLayout();
      autoSaveOptionsLay->setMargin(10);
      {
        QHBoxLayout *saveAutoLay = new QHBoxLayout();
        saveAutoLay->setMargin(0);
        saveAutoLay->setSpacing(5);
        {
          saveAutoLay->addWidget(new QLabel(tr("Interval(Minutes): "), this));
          saveAutoLay->addWidget(m_minuteFld, 0);
          saveAutoLay->addStretch(1);
        }
        autoSaveOptionsLay->addLayout(saveAutoLay, 0);

        autoSaveOptionsLay->addWidget(m_autoSaveSceneCB, 0,
                                      Qt::AlignLeft | Qt::AlignVCenter);
        autoSaveOptionsLay->addWidget(m_autoSaveOtherFilesCB, 0,
                                      Qt::AlignLeft | Qt::AlignVCenter);
      }
      m_autoSaveGroup->setLayout(autoSaveOptionsLay);
      generalFrameLay->addWidget(m_autoSaveGroup);
      generalFrameLay->addWidget(startupPopupCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      // Unit, CameraUnit
      QGridLayout *unitLay = new QGridLayout();
      unitLay->setMargin(0);
      unitLay->setHorizontalSpacing(5);
      unitLay->setVerticalSpacing(10);
      {
        unitLay->addWidget(new QLabel(tr("Undo Memory Size (MB)"), this), 0, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
        unitLay->addWidget(m_undoMemorySize, 0, 1);

        unitLay->addWidget(new QLabel(tr("Render Task Chunk Size:"), this), 1,
                           0, Qt::AlignRight | Qt::AlignVCenter);
        unitLay->addWidget(m_chunkSizeFld, 1, 1);
      }
      unitLay->setColumnStretch(0, 0);
      unitLay->setColumnStretch(1, 0);
      unitLay->setColumnStretch(2, 1);

      generalFrameLay->addLayout(unitLay, 0);

      generalFrameLay->addWidget(replaceAfterSaveLevelAsCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      generalFrameLay->addWidget(m_levelsBackup, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      generalFrameLay->addWidget(sceneNumberingCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      generalFrameLay->addWidget(watchFileSystemCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);

      QGroupBox *projectGroupBox =
          new QGroupBox(tr("Additional Project Locations"), this);
      QGridLayout *projectRootLay = new QGridLayout();
      projectRootLay->setMargin(10);
      projectRootLay->setHorizontalSpacing(5);
      projectRootLay->setVerticalSpacing(10);
      {
        projectRootLay->addWidget(m_projectRootStuff, 0, 0);
        projectRootLay->addWidget(m_projectRootDocuments, 1, 0);
        projectRootLay->addWidget(m_projectRootDesktop, 2, 0);
        projectRootLay->addWidget(m_projectRootCustom, 3, 0);
        projectRootLay->addWidget(m_customProjectRootLabel, 4, 0,
                                  Qt::AlignRight | Qt::AlignVCenter);
        projectRootLay->addWidget(m_customProjectRootFileField, 4, 1, 1, 3);
        projectRootLay->addWidget(m_projectRootDirections, 5, 0, 1, 4);
      }
      projectGroupBox->setLayout(projectRootLay);
      generalFrameLay->addWidget(projectGroupBox, 0);
      generalFrameLay->addStretch(1);

      generalFrameLay->addWidget(note_general, 0);
    }
    generalBox->setLayout(generalFrameLay);
    stackedWidget->addWidget(generalBox);

    //--- Interface --------------------------
    QWidget *userInterfaceBox          = new QWidget(this);
    QVBoxLayout *userInterfaceFrameLay = new QVBoxLayout();
    userInterfaceFrameLay->setMargin(15);
    userInterfaceFrameLay->setSpacing(10);
    {
      QGridLayout *styleLay = new QGridLayout();
      styleLay->setMargin(0);
      styleLay->setHorizontalSpacing(5);
      styleLay->setVerticalSpacing(10);
      {
        styleLay->addWidget(new QLabel(tr("Style:")), 0, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        styleLay->addWidget(styleSheetType, 0, 1);

        styleLay->addWidget(new QLabel(tr("Pixels Only:"), this), 1, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        styleLay->addWidget(m_pixelsOnlyCB, 1, 1);

        styleLay->addWidget(new QLabel(tr("Unit:"), this), 2, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        styleLay->addWidget(m_unitOm, 2, 1);

        styleLay->addWidget(new QLabel(tr("Camera Unit:"), this), 3, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        styleLay->addWidget(m_cameraUnitOm, 3, 1);

        styleLay->addWidget(new QLabel(tr("Rooms*:"), this), 4, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        styleLay->addWidget(roomChoice, 4, 1);
      }
      styleLay->setColumnStretch(0, 0);
      styleLay->setColumnStretch(1, 0);
      styleLay->setColumnStretch(2, 1);
      userInterfaceFrameLay->addLayout(styleLay, 0);

      userInterfaceFrameLay->addWidget(openFlipbookAfterCB, 0,
                                       Qt::AlignLeft | Qt::AlignVCenter);
      userInterfaceFrameLay->addWidget(moveCurrentFrameCB, 0,
                                       Qt::AlignLeft | Qt::AlignVCenter);
      userInterfaceFrameLay->addWidget(actualPixelOnSceneModeCB, 0,
                                       Qt::AlignLeft | Qt::AlignVCenter);
      userInterfaceFrameLay->addWidget(levelNameOnEachMarkerCB, 0,
                                       Qt::AlignLeft | Qt::AlignVCenter);
      userInterfaceFrameLay->addWidget(showRasterImageDarkenBlendedInViewerCB,
                                       0, Qt::AlignLeft | Qt::AlignVCenter);
      userInterfaceFrameLay->addWidget(showShowFrameNumberWithLettersCB, 0,
                                       Qt::AlignLeft | Qt::AlignVCenter);

      QGridLayout *bgColorsLay = new QGridLayout();
      bgColorsLay->setMargin(0);
      bgColorsLay->setHorizontalSpacing(5);
      bgColorsLay->setVerticalSpacing(10);
      {
        bgColorsLay->addWidget(new QLabel(tr("Icon Size *"), this), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_iconSizeLx, 0, 1);
        bgColorsLay->addWidget(new QLabel(tr("X"), this), 0, 2,
                               Qt::AlignCenter);
        bgColorsLay->addWidget(m_iconSizeLy, 0, 3);

        bgColorsLay->addWidget(new QLabel(tr("Viewer  Shrink"), this), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_viewShrink, 1, 1);
        bgColorsLay->addWidget(new QLabel(tr("Step"), this), 1, 3,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_viewStep, 1, 4);

        bgColorsLay->addWidget(new QLabel(tr("Viewer BG Color"), this), 2, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_viewerBgColorFld, 2, 1, 1, 5);

        bgColorsLay->addWidget(new QLabel(tr("Preview BG Color"), this), 3, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_previewBgColorFld, 3, 1, 1, 5);

        bgColorsLay->addWidget(new QLabel(tr("ChessBoard Color 1"), this), 4, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_chessboardColor1Fld, 4, 1, 1, 5);

        bgColorsLay->addWidget(new QLabel(tr("Chessboard Color 2"), this), 5, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(m_chessboardColor2Fld, 5, 1, 1, 5);

        bgColorsLay->addWidget(new QLabel(tr("Viewer Zoom Center"), this), 6, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
        bgColorsLay->addWidget(viewerZoomCenterComboBox, 6, 1, 1, 4);

        if (languageType) {
          bgColorsLay->addWidget(new QLabel(tr("Language *:")), 7, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
          bgColorsLay->addWidget(languageType, 7, 1, 1, 4);
        }
        if (m_interfaceFont->count() > 0) {
          bgColorsLay->addWidget(new QLabel(tr("Font *:")), 8, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
          bgColorsLay->addWidget(m_interfaceFont, 8, 1, 1, 4);
          bgColorsLay->addWidget(new QLabel(tr("Font Weight *:")), 9, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
          bgColorsLay->addWidget(m_interfaceFontWeight, 9, 1, 1, 4);
        }
      }
      bgColorsLay->setColumnStretch(0, 0);
      bgColorsLay->setColumnStretch(1, 0);
      bgColorsLay->setColumnStretch(2, 0);
      bgColorsLay->setColumnStretch(3, 0);
      bgColorsLay->setColumnStretch(4, 0);
      bgColorsLay->setColumnStretch(5, 1);
      userInterfaceFrameLay->addLayout(bgColorsLay, 0);

      userInterfaceFrameLay->addStretch(1);

      userInterfaceFrameLay->addWidget(note_interface, 0);
    }
    userInterfaceBox->setLayout(userInterfaceFrameLay);
    stackedWidget->addWidget(userInterfaceBox);

    //--- Visualization ---------------------
    QWidget *visualizatonBox          = new QWidget(this);
    QVBoxLayout *visualizatonFrameLay = new QVBoxLayout();
    visualizatonFrameLay->setMargin(15);
    visualizatonFrameLay->setSpacing(10);
    {
      visualizatonFrameLay->addWidget(show0ThickLinesCB, 0);
      visualizatonFrameLay->addWidget(regionAntialiasCB, 0);
      visualizatonFrameLay->addStretch(1);
    }
    visualizatonBox->setLayout(visualizatonFrameLay);
    stackedWidget->addWidget(visualizatonBox);

    //--- Loading --------------------------
    QWidget *loadingBox          = new QWidget(this);
    QVBoxLayout *loadingFrameLay = new QVBoxLayout();
    loadingFrameLay->setMargin(15);
    loadingFrameLay->setSpacing(10);
    {
      QHBoxLayout *importLay = new QHBoxLayout();
      importLay->setMargin(0);
      importLay->setSpacing(5);
      {
        importLay->addWidget(new QLabel(tr("Default File Import Behavior:")));
        importLay->addWidget(m_importPolicy);
      }
      importLay->addStretch(0);
      loadingFrameLay->addLayout(importLay, 0);
      loadingFrameLay->addWidget(exposeLoadedLevelsCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      loadingFrameLay->addWidget(createSubfolderCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      loadingFrameLay->addWidget(removeSceneNumberFromLoadedLevelNameCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      loadingFrameLay->addWidget(m_ignoreImageDpiCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);

      QGridLayout *cacheLay = new QGridLayout();
      cacheLay->setMargin(0);
      cacheLay->setHorizontalSpacing(5);
      cacheLay->setVerticalSpacing(10);
      {
        cacheLay->addWidget(new QLabel(tr("Default TLV Caching Behavior:")), 0,
                            0, Qt::AlignRight | Qt::AlignVCenter);
        cacheLay->addWidget(initialLoadTlvCachingBehaviorComboBox, 0, 1);

        cacheLay->addWidget(new QLabel(tr("Column Icon:"), this), 1, 0,
                            Qt::AlignRight | Qt::AlignVCenter);
        cacheLay->addWidget(m_columnIconOm, 1, 1);

        cacheLay->addWidget(new QLabel(tr("Level Settings by File Format:")), 2,
                            0, Qt::AlignRight | Qt::AlignVCenter);
        cacheLay->addWidget(m_levelFormatNames, 2, 1);
        cacheLay->addWidget(m_addLevelFormat, 2, 2);
        cacheLay->addWidget(m_removeLevelFormat, 2, 3);
        cacheLay->addWidget(m_editLevelFormat, 2, 4);

        cacheLay->addWidget(
            new QLabel(
                tr("Palette Type on Loading Raster Image as Color Model:")),
            3, 0, Qt::AlignRight | Qt::AlignVCenter);
        cacheLay->addWidget(paletteTypeForRasterColorModelComboBox, 3, 1, 1, 5);
      }
      cacheLay->setColumnStretch(0, 0);
      cacheLay->setColumnStretch(1, 0);
      cacheLay->setColumnStretch(2, 0);
      cacheLay->setColumnStretch(3, 0);
      cacheLay->setColumnStretch(4, 0);
      cacheLay->setColumnStretch(5, 1);
      loadingFrameLay->addLayout(cacheLay, 0);

      loadingFrameLay->addStretch(1);
    }
    loadingBox->setLayout(loadingFrameLay);
    stackedWidget->addWidget(loadingBox);

    //--- Import/Export --------------------------
    QWidget *ioBox     = new QWidget(this);
    QVBoxLayout *ioLay = new QVBoxLayout();
    ioLay->setMargin(15);
    ioLay->setSpacing(10);
    {
      ioLay->addWidget(
          new QLabel(
              tr("OpenToonz can use FFmpeg for additional file formats.")),
          0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(tr("FFmpeg is not bundled with OpenToonz")),
                       0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(" "), 0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(tr("NOTE: This is an experimental feature.")),
                       0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(tr("Please SAVE YOUR WORK before exporting "
                                     "in MP4, WEBM, or GIF format.")),
                       0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(" "), 0, Qt::AlignCenter | Qt::AlignVCenter);
      ioLay->addWidget(new QLabel(tr("Please provide the path where FFmpeg is "
                                     "located on your computer.")),
                       0, Qt::AlignLeft | Qt::AlignVCenter);
      QGridLayout *ioGridLay = new QGridLayout();
      ioGridLay->setVerticalSpacing(10);
      ioGridLay->setHorizontalSpacing(15);
      ioGridLay->setMargin(0);
      {
        ioGridLay->addWidget(new QLabel(tr("FFmpeg Path: ")), 0, 0,
                             Qt::AlignRight);
        ioGridLay->addWidget(m_ffmpegPathFileFld, 0, 1, 1, 3);
        ioGridLay->addWidget(new QLabel(" "), 1, 0);
        ioGridLay->addWidget(
            new QLabel(tr("Number of seconds to wait for FFmpeg to complete "
                          "processing the output:")),
            2, 0, 1, 4);
        ioGridLay->addWidget(
            new QLabel(tr("Note: FFmpeg begins working once all images "
                          "have been processed.")),
            3, 0, 1, 4);
        ioGridLay->addWidget(new QLabel(tr("FFmpeg Timeout:")), 4, 0,
                             Qt::AlignRight);
        ioGridLay->addWidget(m_ffmpegTimeout, 4, 1, 1, 3);
        ioGridLay->addWidget(new QLabel(" "), 5, 0);
        ioGridLay->addWidget(
            new QLabel(tr("Please indicate where you would like "
                          "exports from Fast Render(MP4) to go.")),
            6, 0, 1, 4);
        ioGridLay->addWidget(new QLabel(tr("Fast Render Path: ")), 7, 0,
                             Qt::AlignRight);
        ioGridLay->addWidget(m_fastRenderPathFileField, 7, 1, 1, 3);
      }
      ioLay->addLayout(ioGridLay);
      ioLay->addStretch(1);

      ioLay->addWidget(note_io, 0);
    }
    ioBox->setLayout(ioLay);
    stackedWidget->addWidget(ioBox);

    //--- Drawing --------------------------
    QWidget *drawingBox          = new QWidget(this);
    QVBoxLayout *drawingFrameLay = new QVBoxLayout();
    drawingFrameLay->setMargin(15);
    drawingFrameLay->setSpacing(10);
    {
      QGridLayout *drawingTopLay = new QGridLayout();
      drawingTopLay->setVerticalSpacing(10);
      drawingTopLay->setHorizontalSpacing(15);
      drawingTopLay->setMargin(0);
      {
        drawingTopLay->addWidget(new QLabel(tr("Scan File Format:")), 0, 0,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_defScanLevelType, 0, 1, 1, 3);

        drawingTopLay->addWidget(new QLabel(tr("Default Level Type:")), 1, 0,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_defLevelType, 1, 1, 1, 3);
        drawingTopLay->addWidget(m_newLevelToCameraSizeCB, 2, 0, 1, 3);
        drawingTopLay->addWidget(new QLabel(tr("Width:")), 3, 0,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_defLevelWidth, 3, 1);
        drawingTopLay->addWidget(new QLabel(tr("  Height:")), 3, 2,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_defLevelHeight, 3, 3);
        drawingTopLay->addWidget(m_dpiLabel, 4, 0, Qt::AlignRight);
        drawingTopLay->addWidget(m_defLevelDpi, 4, 1);
        drawingTopLay->addWidget(new QLabel(tr("Autocreation:")), 5, 0,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_autocreationType, 5, 1, 1, 3);
        drawingTopLay->addWidget(new QLabel(tr("Vector Snapping:")), 6, 0,
                                 Qt::AlignRight);
        drawingTopLay->addWidget(m_vectorSnappingTargetCB, 6, 1, 1, 3);
      }
      drawingFrameLay->addLayout(drawingTopLay, 0);

      drawingFrameLay->addWidget(keepOriginalCleanedUpCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      drawingFrameLay->addWidget(minimizeSaveboxAfterEditingCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);  // 6.4
      drawingFrameLay->addWidget(useSaveboxToLimitFillingOpCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      drawingFrameLay->addWidget(multiLayerStylePickerCB, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      drawingFrameLay->addWidget(m_useNumpadForSwitchingStyles, 0,
                                 Qt::AlignLeft | Qt::AlignVCenter);
      drawingFrameLay->addStretch(1);
    }
    drawingBox->setLayout(drawingFrameLay);
    stackedWidget->addWidget(drawingBox);
    if (m_pixelsOnlyCB->isChecked()) {
      m_defLevelDpi->setDisabled(true);
      m_defLevelWidth->setDecimals(0);
      m_defLevelHeight->setDecimals(0);
    }

    //--- Xsheet --------------------------
    QWidget *xsheetBox             = new QWidget(this);
    QVBoxLayout *xsheetBoxFrameLay = new QVBoxLayout();
    xsheetBoxFrameLay->setMargin(15);
    xsheetBoxFrameLay->setSpacing(10);
    {
      QGridLayout *xsheetFrameLay = new QGridLayout();
      xsheetFrameLay->setMargin(0);
      xsheetFrameLay->setHorizontalSpacing(15);
      xsheetFrameLay->setVerticalSpacing(10);
      {
        xsheetFrameLay->addWidget(new QLabel(tr("Layout Preference*:")), 0, 0,
                                  Qt::AlignRight | Qt::AlignVCenter);
        xsheetFrameLay->addWidget(xsheetLayoutOptions, 0, 1);

        xsheetFrameLay->addWidget(new QLabel(tr("Next/Previous Step Frames:")),
                                  1, 0, Qt::AlignRight | Qt::AlignVCenter);
        xsheetFrameLay->addWidget(m_xsheetStep, 1, 1);

        xsheetFrameLay->addWidget(xsheetAutopanDuringPlaybackCB, 2, 0, 1, 2);

        xsheetFrameLay->addWidget(new QLabel(tr("Cell-dragging Behaviour:")), 3,
                                  0, Qt::AlignRight | Qt::AlignVCenter);
        xsheetFrameLay->addWidget(m_cellsDragBehaviour, 3, 1);

        xsheetFrameLay->addWidget(ignoreAlphaonColumn1CB, 4, 0, 1, 2);
        xsheetFrameLay->addWidget(showKeyframesOnCellAreaCB, 5, 0, 1, 2);
        xsheetFrameLay->addWidget(useArrowKeyToShiftCellSelectionCB, 6, 0, 1,
                                  2);
        xsheetFrameLay->addWidget(inputCellsWithoutDoubleClickingCB, 7, 0, 1,
                                  2);
        xsheetFrameLay->addWidget(shortcutCommandsWhileRenamingCellCB, 8, 0, 1,
                                  2);

        QVBoxLayout *xSheetToolbarLay = new QVBoxLayout();
        xSheetToolbarLay->setMargin(11);
        {
          xSheetToolbarLay->addWidget(m_expandFunctionHeader, 0,
                                      Qt::AlignLeft | Qt::AlignVCenter);
        }
        m_showXSheetToolbar->setLayout(xSheetToolbarLay);

        xsheetFrameLay->addWidget(m_showXSheetToolbar, 9, 0, 3, 3);
        xsheetFrameLay->addWidget(showColumnNumbersCB, 12, 0, 1, 2);
      }
      xsheetFrameLay->setColumnStretch(0, 0);
      xsheetFrameLay->setColumnStretch(1, 0);
      xsheetFrameLay->setColumnStretch(2, 1);
      xsheetFrameLay->setRowStretch(13, 1);

      xsheetBoxFrameLay->addLayout(xsheetFrameLay);

      xsheetBoxFrameLay->addStretch(1);

      xsheetBoxFrameLay->addWidget(note_xsheet, 0);
    }
    xsheetBox->setLayout(xsheetBoxFrameLay);
    stackedWidget->addWidget(xsheetBox);

    //--- Animation --------------------------
    QWidget *animationBox          = new QWidget(this);
    QGridLayout *animationFrameLay = new QGridLayout();
    animationFrameLay->setMargin(15);
    animationFrameLay->setHorizontalSpacing(15);
    animationFrameLay->setVerticalSpacing(10);
    {
      animationFrameLay->addWidget(new QLabel(tr("Default Interpolation:")), 0,
                                   0, Qt::AlignRight | Qt::AlignVCenter);
      animationFrameLay->addWidget(m_keyframeType, 0, 1);

      animationFrameLay->addWidget(new QLabel(tr("Animation Step:")), 1, 0,
                                   Qt::AlignRight | Qt::AlignVCenter);
      animationFrameLay->addWidget(m_animationStepField, 1, 1);
    }
    animationFrameLay->setColumnStretch(0, 0);
    animationFrameLay->setColumnStretch(1, 0);
    animationFrameLay->setColumnStretch(2, 1);
    animationFrameLay->setRowStretch(0, 0);
    animationFrameLay->setRowStretch(1, 0);
    animationFrameLay->setRowStretch(2, 1);
    animationBox->setLayout(animationFrameLay);
    stackedWidget->addWidget(animationBox);

    //--- Preview --------------------------
    QWidget *previewBox        = new QWidget(this);
    QGridLayout *previewLayout = new QGridLayout();
    previewLayout->setMargin(15);
    previewLayout->setHorizontalSpacing(15);
    previewLayout->setVerticalSpacing(10);
    {
      previewLayout->addWidget(new QLabel(tr("Blank Frames:")), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      previewLayout->addWidget(m_blanksCount, 0, 1);

      previewLayout->addWidget(new QLabel(tr("Blank Frames Color:")), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      previewLayout->addWidget(m_blankColor, 1, 1, 1, 2);

      previewLayout->addWidget(rewindAfterPlaybackCB, 2, 0, 1, 3,
                               Qt::AlignLeft | Qt::AlignVCenter);
      previewLayout->addWidget(displayInNewFlipBookCB, 3, 0, 1, 3,
                               Qt::AlignLeft | Qt::AlignVCenter);
      previewLayout->addWidget(fitToFlipbookCB, 4, 0, 1, 3,
                               Qt::AlignLeft | Qt::AlignVCenter);
    }
    previewLayout->setColumnStretch(0, 0);
    previewLayout->setColumnStretch(1, 0);
    previewLayout->setColumnStretch(2, 1);
    previewLayout->setRowStretch(0, 0);
    previewLayout->setRowStretch(1, 0);
    previewLayout->setRowStretch(2, 0);
    previewLayout->setRowStretch(3, 0);
    previewLayout->setRowStretch(4, 0);
    previewLayout->setRowStretch(5, 1);
    previewBox->setLayout(previewLayout);
    stackedWidget->addWidget(previewBox);

    //--- Onion Skin --------------------------
    QWidget *onionBox     = new QWidget(this);
    QVBoxLayout *onionLay = new QVBoxLayout();
    onionLay->setMargin(15);
    onionLay->setSpacing(10);
    {
      onionLay->addWidget(m_onionSkinVisibility, 0,
                          Qt::AlignLeft | Qt::AlignVCenter);
      QGridLayout *onionColorLay = new QGridLayout();
      {
        onionColorLay->addWidget(new QLabel(tr("Paper Thickness:")), 0, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
        onionColorLay->addWidget(m_onionPaperThickness, 0, 1);

        onionColorLay->addWidget(new QLabel(tr("Previous  Frames Correction:")),
                                 1, 0, Qt::AlignRight | Qt::AlignVCenter);
        onionColorLay->addWidget(m_backOnionColor, 1, 1);

        onionColorLay->addWidget(new QLabel(tr("Following Frames Correction:")),
                                 2, 0, Qt::AlignRight | Qt::AlignVCenter);
        onionColorLay->addWidget(m_frontOnionColor, 2, 1);
      }
      onionColorLay->setColumnStretch(0, 0);
      onionColorLay->setColumnStretch(1, 0);
      onionColorLay->setColumnStretch(2, 1);
      onionLay->addLayout(onionColorLay, 0);

      onionLay->addWidget(m_inksOnly, 0, Qt::AlignLeft | Qt::AlignVCenter);
      onionLay->addWidget(m_onionSkinDuringPlayback, 0,
                          Qt::AlignLeft | Qt::AlignVCenter);
      QGridLayout *guidedDrawingLay = new QGridLayout();
      {
        guidedDrawingLay->addWidget(new QLabel(tr("Vector Guided Style:")), 0,
                                    0, Qt::AlignLeft | Qt::AlignVCenter);
        guidedDrawingLay->addWidget(m_guidedDrawingStyle, 0, 1,
                                    Qt::AlignLeft | Qt::AlignVCenter);
        guidedDrawingLay->setColumnStretch(0, 0);
        guidedDrawingLay->setColumnStretch(1, 0);
        guidedDrawingLay->setColumnStretch(2, 1);
      }
      onionLay->addLayout(guidedDrawingLay, 0);
      onionLay->addStretch(1);
    }
    onionBox->setLayout(onionLay);
    stackedWidget->addWidget(onionBox);

    //--- Transparency Check --------------------------
    QWidget *tcBox     = new QWidget(this);
    QGridLayout *tcLay = new QGridLayout();
    tcLay->setMargin(15);
    tcLay->setHorizontalSpacing(10);
    tcLay->setVerticalSpacing(10);
    {
      tcLay->addWidget(new QLabel(tr("Ink Color on White Bg:")), 0, 0,
                       Qt::AlignRight);
      tcLay->addWidget(m_transpCheckInkColor, 0, 1);

      tcLay->addWidget(new QLabel(tr("Ink Color on Black Bg:")), 1, 0,
                       Qt::AlignRight);
      tcLay->addWidget(m_transpCheckBgColor, 1, 1);

      tcLay->addWidget(new QLabel(tr("Paint Color:")), 2, 0, Qt::AlignRight);
      tcLay->addWidget(m_transpCheckPaintColor, 2, 1);
    }
    tcLay->setColumnStretch(0, 0);
    tcLay->setColumnStretch(1, 0);
    tcLay->setColumnStretch(2, 1);
    tcLay->setRowStretch(0, 0);
    tcLay->setRowStretch(1, 0);
    tcLay->setRowStretch(2, 0);
    tcLay->setRowStretch(3, 1);
    tcBox->setLayout(tcLay);
    stackedWidget->addWidget(tcBox);

    //--- Version Control --------------------------
    QWidget *versionControlBox = new QWidget(this);
    QVBoxLayout *vcLay         = new QVBoxLayout();
    vcLay->setMargin(15);
    vcLay->setSpacing(10);
    {
      vcLay->addWidget(m_enableVersionControl, 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
      vcLay->addWidget(autoRefreshFolderContentsCB, 0,
                       Qt::AlignLeft | Qt::AlignVCenter);

      vcLay->addStretch(1);

      vcLay->addWidget(note_version, 0);
    }
    versionControlBox->setLayout(vcLay);
    stackedWidget->addWidget(versionControlBox);

    mainLayout->addWidget(stackedWidget, 1);
  }
  setLayout(mainLayout);

  /*---- signal-slot connections -----*/

  bool ret = true;
  ret      = ret && connect(categoryList, SIGNAL(currentRowChanged(int)),
                       stackedWidget, SLOT(setCurrentIndex(int)));

  //--- General ----------------------
  ret = ret && connect(useDefaultViewerCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onDefaultViewerChanged(int)));
  ret = ret && connect(minimizeRasterMemoryCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onRasterOptimizedMemoryChanged(int)));
  ret = ret && connect(m_autoSaveGroup, SIGNAL(toggled(bool)),
                       SLOT(onAutoSaveChanged(bool)));
  ret = ret && connect(m_autoSaveSceneCB, SIGNAL(stateChanged(int)),
                       SLOT(onAutoSaveSceneChanged(int)));
  ret = ret && connect(m_autoSaveOtherFilesCB, SIGNAL(stateChanged(int)),
                       SLOT(onAutoSaveOtherFilesChanged(int)));
  ret = ret && connect(m_minuteFld, SIGNAL(editingFinished()),
                       SLOT(onMinuteChanged()));
  ret = ret && connect(startupPopupCB, SIGNAL(stateChanged(int)),
                       SLOT(onStartupPopupChanged(int)));
  ret = ret && connect(m_cellsDragBehaviour, SIGNAL(currentIndexChanged(int)),
                       SLOT(onDragCellsBehaviourChanged(int)));
  ret = ret && connect(m_undoMemorySize, SIGNAL(editingFinished()),
                       SLOT(onUndoMemorySizeChanged()));
  ret = ret && connect(m_levelsBackup, SIGNAL(stateChanged(int)),
                       SLOT(onLevelsBackupChanged(int)));
  ret = ret && connect(sceneNumberingCB, SIGNAL(stateChanged(int)),
                       SLOT(onSceneNumberingChanged(int)));
  ret = ret && connect(watchFileSystemCB, SIGNAL(stateChanged(int)),
                       SLOT(onWatchFileSystemClicked(int)));
  ret = ret && connect(m_chunkSizeFld, SIGNAL(editingFinished()), this,
                       SLOT(onChunkSizeChanged()));
  ret = ret && connect(m_customProjectRootFileField, SIGNAL(pathChanged()),
                       this, SLOT(onCustomProjectRootChanged()));
  ret = ret && connect(m_projectRootDocuments, SIGNAL(stateChanged(int)),
                       SLOT(onProjectRootChanged()));
  ret = ret && connect(m_projectRootDesktop, SIGNAL(stateChanged(int)),
                       SLOT(onProjectRootChanged()));
  ret = ret && connect(m_projectRootStuff, SIGNAL(stateChanged(int)),
                       SLOT(onProjectRootChanged()));
  ret = ret && connect(m_projectRootCustom, SIGNAL(stateChanged(int)),
                       SLOT(onProjectRootChanged()));
  //--- Interface ----------------------
  ret = ret &&
        connect(styleSheetType, SIGNAL(currentIndexChanged(const QString &)),
                SLOT(onStyleSheetTypeChanged(const QString &)));
  ret = ret && connect(m_pixelsOnlyCB, SIGNAL(stateChanged(int)),
                       SLOT(onPixelsOnlyChanged(int)));
  // pixels unit may deactivated externally on loading scene (see
  // IoCmd::loadScene())
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(pixelUnitSelected(bool)), this,
                       SLOT(onPixelUnitExternallySelected(bool)));
  ret = ret && connect(m_unitOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onUnitChanged(int)));
  ret = ret && connect(m_cameraUnitOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onCameraUnitChanged(int)));
  ret = ret && connect(roomChoice, SIGNAL(currentIndexChanged(int)),
                       SLOT(onRoomChoiceChanged(int)));
  ret = ret && connect(openFlipbookAfterCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onViewGeneratedMovieChanged(int)));
  ret = ret && connect(m_iconSizeLx, SIGNAL(editingFinished()),
                       SLOT(onIconSizeChanged()));
  ret = ret && connect(m_iconSizeLy, SIGNAL(editingFinished()),
                       SLOT(onIconSizeChanged()));
  ret = ret && connect(m_viewShrink, SIGNAL(editingFinished()),
                       SLOT(onViewValuesChanged()));
  ret = ret && connect(m_viewStep, SIGNAL(editingFinished()),
                       SLOT(onViewValuesChanged()));
  if (languageList.size() > 1)
    ret = ret &&
          connect(languageType, SIGNAL(currentIndexChanged(const QString &)),
                  SLOT(onLanguageTypeChanged(const QString &)));
  ret = ret && connect(moveCurrentFrameCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onMoveCurrentFrameChanged(int)));
  ret =
      ret && connect(viewerZoomCenterComboBox, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(onViewerZoomCenterChanged(int)));
  ret = ret && connect(m_interfaceFont, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onInterfaceFontChanged(int)));
  ret = ret && connect(m_interfaceFontWeight, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onInterfaceFontWeightChanged(int)));
  ret = ret && connect(replaceAfterSaveLevelAsCB, SIGNAL(stateChanged(int)),
                       this, SLOT(onReplaceAfterSaveLevelAsChanged(int)));
  ret =
      ret &&
      connect(showRasterImageDarkenBlendedInViewerCB, SIGNAL(stateChanged(int)),
              this, SLOT(onShowRasterImageDarkenBlendedInViewerChanged(int)));
  ret = ret &&
        connect(showShowFrameNumberWithLettersCB, SIGNAL(stateChanged(int)),
                this, SLOT(onShowFrameNumberWithLettersChanged(int)));
  // Viewer BG color
  ret = ret && connect(m_viewerBgColorFld,
                       SIGNAL(colorChanged(const TPixel32 &, bool)), this,
                       SLOT(setViewerBgColor(const TPixel32 &, bool)));
  // Preview BG color
  ret = ret && connect(m_previewBgColorFld,
                       SIGNAL(colorChanged(const TPixel32 &, bool)), this,
                       SLOT(setPreviewBgColor(const TPixel32 &, bool)));
  // bg chessboard colors
  ret = ret && connect(m_chessboardColor1Fld,
                       SIGNAL(colorChanged(const TPixel32 &, bool)), this,
                       SLOT(setChessboardColor1(const TPixel32 &, bool)));
  ret = ret && connect(m_chessboardColor2Fld,
                       SIGNAL(colorChanged(const TPixel32 &, bool)), this,
                       SLOT(setChessboardColor2(const TPixel32 &, bool)));
  // Column Icon
  ret = ret &&
        connect(m_columnIconOm, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(onColumnIconChange(const QString &)));
  ret = ret && connect(actualPixelOnSceneModeCB, SIGNAL(stateChanged(int)),
                       SLOT(onActualPixelOnSceneModeChanged(int)));
  ret = ret && connect(levelNameOnEachMarkerCB, SIGNAL(stateChanged(int)),
                       SLOT(onLevelNameOnEachMarkerChanged(int)));

  //--- Visualization ---------------------
  ret = ret && connect(show0ThickLinesCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onShow0ThickLinesChanged(int)));
  ret = ret && connect(regionAntialiasCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onRegionAntialiasChanged(int)));

  //--- Loading ----------------------
  ret = ret && connect(exposeLoadedLevelsCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onAutoExposeChanged(int)));
  ret = ret && connect(m_ignoreImageDpiCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onIgnoreImageDpiChanged(int)));
  ret = ret && connect(initialLoadTlvCachingBehaviorComboBox,
                       SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onInitialLoadTlvCachingBehaviorChanged(int)));
  ret = ret && connect(createSubfolderCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onSubsceneFolderChanged(int)));
  ret =
      ret &&
      connect(removeSceneNumberFromLoadedLevelNameCB, SIGNAL(stateChanged(int)),
              this, SLOT(onRemoveSceneNumberFromLoadedLevelNameChanged(int)));
  ret = ret &&
        connect(m_addLevelFormat, SIGNAL(clicked()), SLOT(onAddLevelFormat()));
  ret = ret && connect(m_removeLevelFormat, SIGNAL(clicked()),
                       SLOT(onRemoveLevelFormat()));
  ret = ret && connect(m_editLevelFormat, SIGNAL(clicked()),
                       SLOT(onEditLevelFormat()));
  ret = ret && connect(paletteTypeForRasterColorModelComboBox,
                       SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onPaletteTypeForRasterColorModelChanged(int)));
  ret = ret && connect(m_importPolicy, SIGNAL(currentIndexChanged(int)),
                       SLOT(onImportPolicyChanged(int)));
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(importPolicyChanged(int)), this,
                       SLOT(onImportPolicyExternallyChanged(int)));

  //--- Import/Export ----------------------
  ret = ret && connect(m_ffmpegPathFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onFfmpegPathChanged()));
  ret = ret && connect(m_fastRenderPathFileField, SIGNAL(pathChanged()), this,
                       SLOT(onFastRenderPathChanged()));
  ret = ret && connect(m_ffmpegTimeout, SIGNAL(editingFinished()), this,
                       SLOT(onFfmpegTimeoutChanged()));

  //--- Drawing ----------------------
  ret = ret && connect(keepOriginalCleanedUpCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onSaveUnpaintedInCleanupChanged(int)));
  ret = ret && connect(multiLayerStylePickerCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onMultiLayerStylePickerChanged(int)));
  ret = ret && connect(useSaveboxToLimitFillingOpCB, SIGNAL(stateChanged(int)),
                       this, SLOT(onGetFillOnlySavebox(int)));
  ret = ret && connect(m_defScanLevelType,
                       SIGNAL(currentIndexChanged(const QString &)),
                       SLOT(onScanLevelTypeChanged(const QString &)));
  ret = ret && connect(minimizeSaveboxAfterEditingCB, SIGNAL(stateChanged(int)),
                       this, SLOT(onMinimizeSaveboxAfterEditing(int)));
  ret = ret && connect(m_defLevelType, SIGNAL(currentIndexChanged(int)),
                       SLOT(onDefLevelTypeChanged(int)));
  ret = ret && connect(m_autocreationType, SIGNAL(currentIndexChanged(int)),
                       SLOT(onAutocreationTypeChanged(int)));
  ret =
      ret && connect(m_vectorSnappingTargetCB, SIGNAL(currentIndexChanged(int)),
                     SLOT(onVectorSnappingTargetChanged(int)));
  ret = ret && connect(m_defLevelWidth, SIGNAL(valueChanged()),
                       SLOT(onDefLevelParameterChanged()));
  ret = ret && connect(m_defLevelHeight, SIGNAL(valueChanged()),
                       SLOT(onDefLevelParameterChanged()));
  ret = ret && connect(m_defLevelDpi, SIGNAL(valueChanged()),
                       SLOT(onDefLevelParameterChanged()));
  ret = ret && connect(m_useNumpadForSwitchingStyles, SIGNAL(clicked(bool)),
                       SLOT(onUseNumpadForSwitchingStylesClicked(bool)));
  ret = ret && connect(m_newLevelToCameraSizeCB, SIGNAL(clicked(bool)),
                       SLOT(onNewLevelToCameraSizeChanged(bool)));

  //--- Xsheet ----------------------
  ret = ret && connect(xsheetAutopanDuringPlaybackCB, SIGNAL(stateChanged(int)),
                       this, SLOT(onXsheetAutopanChanged(int)));
  ret = ret && connect(ignoreAlphaonColumn1CB, SIGNAL(stateChanged(int)), this,
                       SLOT(onIgnoreAlphaonColumn1Changed(int)));
  ret = ret && connect(m_xsheetStep, SIGNAL(editingFinished()),
                       SLOT(onXsheetStepChanged()));
  ret = ret && connect(m_cellsDragBehaviour, SIGNAL(currentIndexChanged(int)),
                       SLOT(onDragCellsBehaviourChanged(int)));
  ret = ret && connect(showKeyframesOnCellAreaCB, SIGNAL(stateChanged(int)),
                       this, SLOT(onShowKeyframesOnCellAreaChanged(int)));
  ret = ret &&
        connect(useArrowKeyToShiftCellSelectionCB, SIGNAL(stateChanged(int)),
                SLOT(onUseArrowKeyToShiftCellSelectionClicked(int)));
  ret = ret &&
        connect(inputCellsWithoutDoubleClickingCB, SIGNAL(stateChanged(int)),
                SLOT(onInputCellsWithoutDoubleClickingClicked(int)));
  ret = ret &&
        connect(shortcutCommandsWhileRenamingCellCB, SIGNAL(stateChanged(int)),
                SLOT(onShortcutCommandsWhileRenamingCellClicked(int)));
  ret = ret && connect(m_showXSheetToolbar, SIGNAL(clicked(bool)),
                       SLOT(onShowXSheetToolbarClicked(bool)));
  ret = ret && connect(m_expandFunctionHeader, SIGNAL(clicked(bool)),
                       SLOT(onExpandFunctionHeaderClicked(bool)));

  ret = ret && connect(showColumnNumbersCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onShowColumnNumbersChanged(int)));
  ret = ret && connect(xsheetLayoutOptions,
                       SIGNAL(currentIndexChanged(const QString &)), this,
                       SLOT(onXsheetLayoutChanged(const QString &)));

  //--- Animation ----------------------
  ret = ret && connect(m_keyframeType, SIGNAL(currentIndexChanged(int)),
                       SLOT(onKeyframeTypeChanged(int)));
  ret = ret && connect(m_animationStepField, SIGNAL(editingFinished()),
                       SLOT(onAnimationStepChanged()));

  //--- Preview ----------------------
  ret = ret && connect(m_blanksCount, SIGNAL(editingFinished()),
                       SLOT(onBlankCountChanged()));
  ret =
      ret && connect(m_blankColor, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     SLOT(onBlankColorChanged(const TPixel32 &, bool)));
  ret = ret && connect(rewindAfterPlaybackCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onRewindAfterPlayback(int)));
  ret = ret && connect(displayInNewFlipBookCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onPreviewAlwaysOpenNewFlip(int)));
  ret = ret && connect(fitToFlipbookCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onFitToFlipbook(int)));

  //--- Onion Skin ----------------------
  ret = ret &&
        connect(m_frontOnionColor, SIGNAL(colorChanged(const TPixel32 &, bool)),
                SLOT(onOnionDataChanged(const TPixel32 &, bool)));
  ret = ret &&
        connect(m_backOnionColor, SIGNAL(colorChanged(const TPixel32 &, bool)),
                SLOT(onOnionDataChanged(const TPixel32 &, bool)));
  ret = ret && connect(m_inksOnly, SIGNAL(stateChanged(int)),
                       SLOT(onOnionDataChanged(int)));
  ret = ret && connect(m_onionSkinVisibility, SIGNAL(stateChanged(int)),
                       SLOT(onOnionSkinVisibilityChanged(int)));
  ret = ret && connect(m_onionSkinDuringPlayback, SIGNAL(stateChanged(int)),
                       SLOT(onOnionSkinDuringPlaybackChanged(int)));
  ret = ret && connect(m_onionPaperThickness, SIGNAL(editingFinished()),
                       SLOT(onOnionPaperThicknessChanged()));
  ret = ret && connect(m_guidedDrawingStyle, SIGNAL(currentIndexChanged(int)),
                       SLOT(onGuidedDrawingStyleChanged(int)));
  //--- Transparency Check ----------------------
  ret = ret && connect(m_transpCheckBgColor,
                       SIGNAL(colorChanged(const TPixel32 &, bool)),
                       SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));
  ret = ret && connect(m_transpCheckInkColor,
                       SIGNAL(colorChanged(const TPixel32 &, bool)),
                       SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));
  ret = ret && connect(m_transpCheckPaintColor,
                       SIGNAL(colorChanged(const TPixel32 &, bool)),
                       SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));

  //--- Version Control ----------------------
  ret = ret && connect(m_enableVersionControl, SIGNAL(stateChanged(int)),
                       SLOT(onSVNEnabledChanged(int)));
  ret = ret && connect(autoRefreshFolderContentsCB, SIGNAL(stateChanged(int)),
                       SLOT(onAutomaticSVNRefreshChanged(int)));

  assert(ret);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PreferencesPopup> openPreferencesPopup(MI_Preferences);
