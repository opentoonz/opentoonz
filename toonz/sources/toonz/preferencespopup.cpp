

#include "preferencespopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "versioncontrol.h"
#include "levelsettingspopup.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/tabbar.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tscenehandle.h"
#include "toonz/levelproperties.h"
#include "toonz/tonionskinmaskhandle.h"

// TnzCore includes
#include "tsystem.h"

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

using namespace DVGui;

//*******************************************************************************************
//    Local namespace
//*******************************************************************************************

namespace
{

static const int unitsCount = 5, inchIdx = 2;
static const QString units[unitsCount] = {"cm", "mm", "inch", "field", "pixel"};

enum DpiPolicy { DP_ImageDpi,
				 DP_CustomDpi };

} // namespace

//**********************************************************************************
//    PreferencesPopup::FormatProperties  implementation
//**********************************************************************************

PreferencesPopup::FormatProperties::FormatProperties(PreferencesPopup *parent)
	: DVGui::Dialog(parent, false, true)
{
	setWindowTitle(tr("Level Settings by File Format"));
	setModal(true); // The underlying selected format should not
	// be changed by the user

	// Main layout
	beginVLayout();

	QGridLayout *gridLayout = new QGridLayout;
	int row = 0;

	// Key values
	QLabel *nameLabel = new QLabel(tr("Name:"));
	nameLabel->setFixedHeight(20); // Due to DVGui::Dialog's quirky behavior
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
	m_dpi->setRange(1, (std::numeric_limits<double>::max)()); // Tried limits::min(), but input 0 was
	gridLayout->addWidget(m_dpi, row++, 1);					  // then replaced with something * e^-128

	m_premultiply = new DVGui::CheckBox(
		LevelSettingsPopup::tr("Premultiply"));
	gridLayout->addWidget(m_premultiply, row++, 1);

	m_whiteTransp = new DVGui::CheckBox(
		LevelSettingsPopup::tr("White As Transparent"));
	gridLayout->addWidget(m_whiteTransp, row++, 1);

	m_doAntialias = new DVGui::CheckBox(
		LevelSettingsPopup::tr("Add Antialiasing"));
	gridLayout->addWidget(m_doAntialias, row++, 1);

	QLabel *antialiasLabel = new QLabel(
		LevelSettingsPopup::tr("Antialias Softness:"));
	gridLayout->addWidget(antialiasLabel, row, 0, Qt::AlignRight);

	m_antialias = new DVGui::IntLineEdit(this, 10, 0, 100); // Tried 1, but then m_doAntialias was forcedly
	gridLayout->addWidget(m_antialias, row++, 1);			// initialized to true

	QLabel *subsamplingLabel = new QLabel(
		LevelSettingsPopup::tr("Subsampling:"));
	gridLayout->addWidget(subsamplingLabel, row, 0, Qt::AlignRight);

	m_subsampling = new DVGui::IntLineEdit(this, 1, 1);
	gridLayout->addWidget(m_subsampling, row++, 1);

	addLayout(gridLayout);

	endVLayout();

	// Establish connections
	bool ret = true;

	ret = connect(m_dpiPolicy, SIGNAL(currentIndexChanged(int)), SLOT(updateEnabledStatus())) && ret;
	ret = connect(m_doAntialias, SIGNAL(clicked()), SLOT(updateEnabledStatus())) && ret;

	assert(ret);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::updateEnabledStatus()
{
	m_dpi->setEnabled(m_dpiPolicy->currentIndex() == DP_CustomDpi);
	m_antialias->setEnabled(m_doAntialias->isChecked());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::setLevelFormat(
	const Preferences::LevelFormat &lf)
{
	const LevelOptions &lo = lf.m_options;

	m_name->setText(lf.m_name);
	m_regExp->setText(lf.m_pathFormat.pattern());
	m_priority->setValue(lf.m_priority);

	m_dpiPolicy->setCurrentIndex(lo.m_dpiPolicy == LevelOptions::DP_ImageDpi ? DP_ImageDpi : DP_CustomDpi);
	m_dpi->setValue(lo.m_dpi);
	m_premultiply->setChecked(lo.m_premultiply);
	m_whiteTransp->setChecked(lo.m_whiteTransp);
	m_doAntialias->setChecked(lo.m_antialias > 0);
	m_antialias->setValue(lo.m_antialias);
	m_subsampling->setValue(lo.m_subsampling);

	updateEnabledStatus();
}

//-----------------------------------------------------------------------------

Preferences::LevelFormat PreferencesPopup::FormatProperties::levelFormat() const
{
	Preferences::LevelFormat lf(m_name->text());

	// Assign key values
	lf.m_pathFormat.setPattern(m_regExp->text());
	lf.m_priority = m_priority->getValue();

	// Assign level format values
	lf.m_options.m_dpiPolicy = (m_dpiPolicy->currentIndex() == DP_ImageDpi) ? LevelOptions::DP_ImageDpi : LevelOptions::DP_CustomDpi;
	lf.m_options.m_dpi = m_dpi->getValue();
	lf.m_options.m_subsampling = m_subsampling->getValue();
	lf.m_options.m_antialias = m_doAntialias->isChecked() ? m_antialias->getValue() : 0;
	lf.m_options.m_premultiply = m_premultiply->isChecked();
	lf.m_options.m_whiteTransp = m_whiteTransp->isChecked();

	return lf;
}

//**********************************************************************************
//    PreferencesPopup  implementation
//**********************************************************************************

void PreferencesPopup::onUnitChanged(int index)
{
	m_pref->setUnits(::units[index].toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onCameraUnitChanged(int index)
{
	m_pref->setCameraUnits(::units[index].toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onScanLevelTypeChanged(const QString &text)
{
	m_pref->setScanLevelType(text.toStdString());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMinuteChanged()
{
	m_pref->setAutosavePeriod(m_minuteFld->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onChunkSizeChanged()
{
	m_pref->setDefaultTaskChunkSize(m_chunkSizeFld->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankCountChanged()
{
	if (m_blanksCount && m_blankColor)
		m_pref->setBlankValues(m_blanksCount->getValue(), m_blankColor->getColor());
	TApp::instance()->getCurrentScene()->notifyPreferenceChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionPaperThicknessChanged()
{
	if (m_onionPaperThickness) {
		m_pref->setOnionPaperThickness(m_onionPaperThickness->getValue());
		TApp::instance()->getCurrentScene()->notifySceneChanged();
	}
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankColorChanged(const TPixel32 &, bool isDragging)
{
	if (isDragging)
		return;

	if (m_blanksCount && m_blankColor)
		m_pref->setBlankValues(m_blanksCount->getValue(), m_blankColor->getColor());
	TApp::instance()->getCurrentScene()->notifyPreferenceChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutomaticSVNRefreshChanged(int index)
{
	m_pref->enableAutomaticSVNFolderRefresh(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSVNEnabledChanged(int index)
{
	bool enabled = index == Qt::Checked;
	if (enabled) {
		if (VersionControl::instance()->testSetup())
			m_pref->enableSVN(true);
		else {
			if (m_enableVersionControl)
				m_enableVersionControl->setChecked(false);
		}
	} else
		m_pref->enableSVN(false);
}

//-----------------------------------------------------------------------------

void invalidateIcons(); //implemented in sceneviewer.cpp; in which fucking header  I can put this declaration?!

void PreferencesPopup::onTranspCheckDataChanged(const TPixel32 &, bool isDragging)
{
	if (isDragging)
		return;

	m_pref->setTranspCheckData(m_transpCheckBgColor->getColor(), m_transpCheckInkColor->getColor(), m_transpCheckPaintColor->getColor());

	invalidateIcons();
}

//---------------------------------------------------------------------------------------

void PreferencesPopup::onOnionDataChanged(const TPixel32 &, bool isDragging)
{
	if (isDragging)
		return;
	bool inksOnly = false;
	if (m_inksOnly)
		inksOnly = m_inksOnly->isChecked();
	m_pref->setOnionData(m_frontOnionColor->getColor(), m_backOnionColor->getColor(), inksOnly);

	TApp::instance()->getCurrentScene()->notifySceneChanged();
	TApp::instance()->getCurrentLevel()->notifyLevelViewChange();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionDataChanged(int)
{
	bool inksOnly = false;
	if (m_inksOnly)
		inksOnly = m_inksOnly->isChecked();
	m_pref->setOnionData(m_frontOnionColor->getColor(), m_backOnionColor->getColor(), inksOnly);

	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewValuesChanged()
{
	m_pref->setViewValues(m_viewShrink->getValue(), m_viewStep->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onIconSizeChanged()
{
	TDimension size(m_iconSizeLx->getValue(), m_iconSizeLy->getValue());
	if (m_pref->getIconSize() == size)
		return;

	m_pref->setIconSize(size);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoExposeChanged(int index)
{
	m_pref->enableAutoExpose(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSubsceneFolderChanged(int index)
{
	m_pref->enableSubsceneFolder(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewGeneratedMovieChanged(int index)
{
	m_pref->enableGeneratedMovieView(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onXsheetStepChanged()
{
	m_pref->setXsheetStep(m_xsheetStep->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onXsheetAutopanChanged(int index)
{
	m_pref->enableXsheetAutopan(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onIgnoreAlphaonColumn1Changed(int index)
{
	m_pref->enableIgnoreAlphaonColumn1(index == Qt::Checked);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onRewindAfterPlayback(int index)
{
	m_pref->enableRewindAfterPlayback(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFitToFlipbook(int index)
{
	m_pref->enableFitToFlipbook(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPreviewAlwaysOpenNewFlip(int index)
{
	m_pref->enablePreviewAlwaysOpenNewFlip(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAskForOverrideRender(int index)
{
	m_pref->setAskForOverrideRender(index == Qt::Checked);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onRasterOptimizedMemoryChanged(int index)
{
	if (m_pref->isRasterOptimizedMemory() == (index == Qt::Checked))
		return;

	m_pref->enableRasterOptimizedMemory(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onGetFillOnlySavebox(int index)
{
	if (m_pref->getFillOnlySavebox() == (index == Qt::Checked))
		return;

	m_pref->setFillOnlySavebox(index == Qt::Checked);
	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSaveUnpaintedInCleanupChanged(int index)
{
	if (m_pref->isSaveUnpaintedInCleanupEnable() == (index == Qt::Checked))
		return;

	m_pref->enableSaveUnpaintedInCleanup(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMinimizeSaveboxAfterEditing(int index)
{
	m_pref->enableMinimizeSaveboxAfterEditing(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefaultViewerChanged(int index)
{
	m_pref->enableDefaultViewer(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveChanged(int index)
{
	m_minuteFld->setEnabled(index == Qt::Checked);
	m_pref->enableAutosave(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onKeyframeTypeChanged(int index)
{
	m_pref->setKeyframeType(index + 2);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onAutocreationTypeChanged(int index)
{
	m_pref->setAutocreationType(index);
}
//-----------------------------------------------------------------------------

void PreferencesPopup::onAnimationStepChanged()
{
	m_pref->setAnimationStep(m_animationStepField->getValue());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLanguageTypeChanged(int index)
{
	m_pref->setCurrentLanguage(index);
	QString currentLanguage = m_pref->getCurrentLanguage();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMoveCurrentFrameChanged(int index)
{
	m_pref->enableMoveCurrent(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onReplaceAfterSaveLevelAsChanged(int index)
{
	m_pref->enableReplaceAfterSaveLevelAs(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setViewerBgColor(const TPixel32 &color, bool isDragging)
{
	m_pref->setViewerBGColor(color, isDragging);
	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setPreviewBgColor(const TPixel32 &color, bool isDragging)
{
	m_pref->setPreviewBGColor(color, isDragging);
	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setChessboardColor1(const TPixel32 &color, bool isDragging)
{
	m_pref->setChessboardColor1(color, isDragging);
	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::setChessboardColor2(const TPixel32 &color, bool isDragging)
{
	m_pref->setChessboardColor2(color, isDragging);
	TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onColumnIconChange(const QString &value)
{
	m_pref->setColumnIconLoadingPolicy(value == QString("At Once")
										   ? Preferences::LoadAtOnce
										   : Preferences::LoadOnDemand);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionSkinVisibilityChanged(int index)
{
	m_pref->enableOnionSkin(index == Qt::Checked);

	m_frontOnionColor->setEnabled(index == Qt::Checked);
	m_backOnionColor->setEnabled(index == Qt::Checked);
	m_inksOnly->setEnabled(index == Qt::Checked);
	m_onionPaperThickness->setEnabled(index == Qt::Checked);

	OnionSkinMask osm = TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
	osm.enable(index == Qt::Checked);
	TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
	TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onActualPixelOnSceneModeChanged(int index)
{
	m_pref->enableActualPixelViewOnSceneEditingMode(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onMultiLayerStylePickerChanged(int index)
{
	m_pref->enableMultiLayerStylePicker(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelNameOnEachMarkerChanged(int index)
{
	m_pref->enableLevelNameOnEachMarker(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInitialLoadTlvCachingBehaviorChanged(int index)
{
	m_pref->setInitialLoadTlvCachingBehavior(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onViewerZoomCenterChanged(int index)
{
	m_pref->setViewerZoomCenter(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRemoveSceneNumberFromLoadedLevelNameChanged(int index)
{
	m_pref->enableRemoveSceneNumberFromLoadedLevelName(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowRasterImageDarkenBlendedInViewerChanged(int index)
{
	m_pref->enableShowRasterImagesDarkenBlendedInViewer(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowFrameNumberWithLettersChanged(int index)
{
	m_pref->enableShowFrameNumberWithLetters(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPaletteTypeForRasterColorModelChanged(int index)
{
	m_pref->setPaletteTypeOnLoadRasterImageAsColorModel(index);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowKeyframesOnCellAreaChanged(int index)
{
	m_pref->enableShowKeyframesOnXsheetCellArea(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onStyleSheetTypeChanged(int index)
{
	m_pref->setCurrentStyleSheet(index);
	QApplication::setOverrideCursor(Qt::WaitCursor);
	QString currentStyle = m_pref->getCurrentStyleSheet();
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

void PreferencesPopup::onUndoMemorySizeChanged()
{
	int value = m_undoMemorySize->getValue();
	m_pref->setUndoMemorySize(value);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDragCellsBehaviourChanged(int index)
{
	m_pref->setDragCellsBehaviour(index);
}

//-----------------------------------------------------------------------------

#ifdef LINETEST
void PreferencesPopup::onLineTestFpsCapture(int index)
{
	if (index == 0)
		m_pref->setLineTestFpsCapture(0);
	if (index == 1)
		m_pref->setLineTestFpsCapture(25);
	else if (index == 2)
		m_pref->setLineTestFpsCapture(30);
}
#endif

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelsBackupChanged(int index)
{
	m_pref->enableLevelsBackup(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSceneNumberingChanged(int index)
{
	m_pref->enableSceneNumbering(index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefLevelTypeChanged(int index)
{
	if (0 <= index && index < m_defLevelType->count()) {
		int levelType = m_defLevelType->itemData(index).toInt();
		m_pref->setDefLevelType(levelType);
		bool isRaster = levelType != PLI_XSHLEVEL;
		m_defLevelWidth->setEnabled(isRaster);
		m_defLevelHeight->setEnabled(isRaster);
		m_defLevelDpi->setEnabled(isRaster);
	}
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefLevelParameterChanged()
{
	double w = m_defLevelWidth->getValue();
	double h = m_defLevelHeight->getValue();
	double dpi = m_defLevelDpi->getValue();
	m_pref->setDefLevelWidth(w);
	m_pref->setDefLevelHeight(h);
	m_pref->setDefLevelDpi(dpi);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::rebuildFormatsList()
{
	const Preferences &prefs = *Preferences::instance();

	m_levelFormatNames->clear();

	int lf, lfCount = prefs.levelFormatsCount();
	for (lf = 0; lf != lfCount; ++lf)
		m_levelFormatNames->addItem(prefs.levelFormat(lf).m_name);

	m_editLevelFormat->setEnabled(m_levelFormatNames->count() > 0);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAddLevelFormat()
{
	bool ok = true;
	QString formatName = DVGui::getText(
		tr("New Level Format"), tr("Assign the new level format name:"),
		tr("New Format"), &ok);

	if (ok) {
		int formatIdx = Preferences::instance()->addLevelFormat(formatName);
		rebuildFormatsList();

		m_levelFormatNames->setCurrentIndex(formatIdx);
	}
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRemoveLevelFormat()
{
	Preferences::instance()->removeLevelFormat(m_levelFormatNames->currentIndex());
	rebuildFormatsList();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onEditLevelFormat()
{
	if (!m_formatProperties) {
		m_formatProperties = new FormatProperties(this);

		bool ret = connect(m_formatProperties, SIGNAL(dialogClosed()), SLOT(onLevelFormatEdited()));
		assert(ret);
	}

	const Preferences::LevelFormat &lf = Preferences::instance()->levelFormat(
		m_levelFormatNames->currentIndex());

	m_formatProperties->setLevelFormat(lf);
	m_formatProperties->show();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelFormatEdited()
{
	assert(m_formatProperties);

	Preferences &prefs = *Preferences::instance();
	int formatIdx = m_levelFormatNames->currentIndex();

	prefs.removeLevelFormat(formatIdx);
	formatIdx = prefs.addLevelFormat(m_formatProperties->levelFormat());

	rebuildFormatsList();

	m_levelFormatNames->setCurrentIndex(formatIdx);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShow0ThickLinesChanged(int on)
{
	m_pref->setShow0ThickLines(on);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRegionAntialiasChanged(int on)
{
	m_pref->setRegionAntialias(on);
}

//**********************************************************************************
//    PrefencesPopup's  constructor
//**********************************************************************************

PreferencesPopup::PreferencesPopup()
	: QDialog(TApp::instance()->getMainWindow()), m_formatProperties(), m_inksOnly(0), m_blanksCount(0), m_blankColor(0)
{
	setWindowTitle(tr("Preferences"));
	setObjectName("PreferencesPopup");

	m_pref = Preferences::instance();

	//Category List
	QListWidget *categoryList = new QListWidget(this);

	QStackedWidget *stackedWidget = new QStackedWidget(this);

	//---General ------------------------------
	categoryList->addItem(tr("General"));

	CheckBox *useDefaultViewerCB = new CheckBox(tr("Use Default Viewer for Movie Formats"), this);
	CheckBox *minimizeRasterMemoryCB = new CheckBox(tr("Minimize Raster Memory Fragmentation *"), this);
	CheckBox *autoSaveCB = new CheckBox(tr("Save Automatically Every Minutes"));
	m_minuteFld = new DVGui::IntLineEdit(this, 15, 1, 60);
	CheckBox *replaceAfterSaveLevelAsCB = new CheckBox(tr("Replace Toonz Level after SaveLevelAs command"), this);

	m_cellsDragBehaviour = new QComboBox();
	m_undoMemorySize = new DVGui::IntLineEdit(this, m_pref->getUndoMemorySize(), 0, 2000);
	m_levelsBackup = new CheckBox(tr("Backup Animation Levels when Saving"));
	m_chunkSizeFld = new DVGui::IntLineEdit(this, m_pref->getDefaultTaskChunkSize(), 1, 2000);
	CheckBox *sceneNumberingCB = new CheckBox(tr("Show Info in Rendered Frames"));

	QLabel *note_general = new QLabel(tr("* Changes will take effect the next time you run Toonz"));
	note_general->setStyleSheet("font-size: 10px; font: italic;");

	//--- Interface ------------------------------
	categoryList->addItem(tr("Interface"));

	QComboBox *languageType = 0;
	QStringList languageList;
	int currentIndex = 0;
	for (int i = 0; i < m_pref->getLanguageCount(); i++) {
		QString string = m_pref->getLanguage(i);
		if (string == m_pref->getCurrentLanguage())
			currentIndex = i;
		languageList.push_back(string);
	}

	if (languageList.size() > 1) {
		languageType = new QComboBox(this);
		languageType->addItems(languageList);
		languageType->setCurrentIndex(currentIndex);
	}
	QComboBox *styleSheetType = new QComboBox(this);
	QComboBox *unitOm = new QComboBox(this);
	QComboBox *cameraUnitOm = new QComboBox(this);
	m_iconSizeLx = new DVGui::IntLineEdit(this, 80, 10, 400);
	m_iconSizeLy = new DVGui::IntLineEdit(this, 60, 10, 400);
	m_viewShrink = new DVGui::IntLineEdit(this, 1, 1, 20);
	m_viewStep = new DVGui::IntLineEdit(this, 1, 1, 20);

	CheckBox *moveCurrentFrameCB = new CheckBox(tr("Move Current Frame by Clicking on Xsheet / Numerical Columns Cell Area"), this);
	//Viewer BG color
	m_viewerBgColorFld = new ColorField(this, false, m_pref->getViewerBgColor());
	//Preview BG color
	m_previewBgColorFld = new ColorField(this, false, m_pref->getPreviewBgColor());
	//bg chessboard colors
	TPixel32 col1, col2;
	m_pref->getChessboardColors(col1, col2);
	m_chessboardColor1Fld = new ColorField(this, false, col1);
	m_chessboardColor2Fld = new ColorField(this, false, col2);
	CheckBox *openFlipbookAfterCB = new CheckBox(tr("Open Flipbook after Rendering"), this);
	CheckBox *actualPixelOnSceneModeCB = new CheckBox(tr("Enable Actual Pixel View on Scene Editing Mode"), this);
	CheckBox *levelNameOnEachMarkerCB = new CheckBox(tr("Display Level Name on Each Marker"), this);
	CheckBox *showRasterImageDarkenBlendedInViewerCB = new CheckBox(tr("Show Raster Images Darken Blended in Camstand View"), this);
	CheckBox *showShowFrameNumberWithLettersCB = new CheckBox(tr("Show \"ABC\" Appendix to the Frame Number in Xsheet Cell"), this);
	QComboBox *viewerZoomCenterComboBox = new QComboBox(this);

	QLabel *note_interface = new QLabel(tr("* Changes will take effect the next time you run Toonz"));
	note_interface->setStyleSheet("font-size: 10px; font: italic;");

	//--- Visualization ------------------------------
	categoryList->addItem(tr("Visualization"));
	CheckBox *show0ThickLinesCB = new CheckBox(tr("Show Lines with Thickness 0"), this);
	CheckBox *regionAntialiasCB = new CheckBox(tr("Antialiased region boundaries"), this);

	//--- Loading ------------------------------
	categoryList->addItem(tr("Loading"));

	CheckBox *exposeLoadedLevelsCB = new CheckBox(tr("Expose Loaded Levels in Xsheet"), this);
	CheckBox *createSubfolderCB = new CheckBox(tr("Create Sub-folder when Importing Sub-xsheet"), this);
	//Column Icon
	m_columnIconOm = new QComboBox(this);
	QComboBox *initialLoadTlvCachingBehaviorComboBox = new QComboBox(this);
	CheckBox *removeSceneNumberFromLoadedLevelNameCB = new CheckBox(tr("Automatically Remove Scene Number from Loaded Level Name"), this);

	m_levelFormatNames = new QComboBox;
	m_addLevelFormat = new QPushButton("+");
	m_removeLevelFormat = new QPushButton("-");
	m_editLevelFormat = new QPushButton(tr("Edit"));

	QComboBox *paletteTypeForRasterColorModelComboBox = new QComboBox(this);

	//--- Drawing ------------------------------
	categoryList->addItem(tr("Drawing"));

#ifndef BRAVO
	m_defScanLevelType = new QComboBox(this);
#endif
	m_defLevelType = new QComboBox(this);
	m_defLevelWidth = new MeasuredDoubleLineEdit(0);
	m_defLevelHeight = new MeasuredDoubleLineEdit(0);
	m_defLevelDpi = new DoubleLineEdit(0, 66.76);
	m_autocreationType = new QComboBox(this);

	CheckBox *keepOriginalCleanedUpCB = new CheckBox(tr("Keep Original Cleaned Up Drawings As Backup"), this);
	CheckBox *multiLayerStylePickerCB = new CheckBox(tr("Multi Layer Style Picker : Switch Levels by Picking"), this);
	CheckBox *useSaveboxToLimitFillingOpCB = new CheckBox(tr("Use the TLV Savebox to Limit Filling Operations"), this);
	CheckBox *minimizeSaveboxAfterEditingCB = new CheckBox(tr("Minimize Savebox after Editing"), this);

	//--- Xsheet ------------------------------
	categoryList->addItem(tr("Xsheet"));

	CheckBox *xsheetAutopanDuringPlaybackCB = new CheckBox(tr("Xsheet Autopan during Playback"), this);
	m_xsheetStep = new DVGui::IntLineEdit(this, Preferences::instance()->getXsheetStep(), 0);
	m_cellsDragBehaviour = new QComboBox();
	CheckBox *ignoreAlphaonColumn1CB = new CheckBox(tr("Ignore Alpha Channel on Levels in Column 1"), this);
	CheckBox *showKeyframesOnCellAreaCB = new CheckBox(tr("Show Keyframes on Cell Area"), this);

	//--- Animation ------------------------------
	categoryList->addItem(tr("Animation"));

	m_keyframeType = new QComboBox(this);
	m_animationStepField = new DVGui::IntLineEdit(this, 1, 1, 500);

	//--- Preview ------------------------------
	categoryList->addItem(tr("Preview"));

	m_blanksCount = new DVGui::IntLineEdit(this, 0, 0, 1000);
	m_blankColor = new ColorField(this, false, TPixel::Black);
	CheckBox *rewindAfterPlaybackCB = new CheckBox(tr("Rewind after Playback"), this);
	CheckBox *displayInNewFlipBookCB = new CheckBox(tr("Display in a New Flipbook Window"), this);
	CheckBox *fitToFlipbookCB = new CheckBox(tr("Fit to Flipbook"), this);

	//--- Onion Skin ------------------------------
	categoryList->addItem(tr("Onion Skin"));

	TPixel32 frontColor, backColor;
	bool onlyInks;
	m_pref->getOnionData(frontColor, backColor, onlyInks);
	m_onionSkinVisibility = new CheckBox(tr("Onion Skin ON"));
	m_frontOnionColor = new ColorField(this, false, frontColor);
	m_backOnionColor = new ColorField(this, false, backColor);
	m_inksOnly = new DVGui::CheckBox(tr("Display Lines Only "));
	m_inksOnly->setChecked(onlyInks);

	int thickness = m_pref->getOnionPaperThickness();
	m_onionPaperThickness = new DVGui::IntLineEdit(this, thickness, 0, 100);

	//--- Transparency Check ------------------------------
	categoryList->addItem(tr("Transparency Check"));

	TPixel32 bgColor, inkColor, paintColor;
	m_pref->getTranspCheckData(bgColor, inkColor, paintColor);
	m_transpCheckInkColor = new ColorField(this, false, inkColor);
	m_transpCheckBgColor = new ColorField(this, false, bgColor);
	m_transpCheckPaintColor = new ColorField(this, false, paintColor);

	//--- Version Control ------------------------------
	categoryList->addItem(tr("Version Control"));
	m_enableVersionControl = new DVGui::CheckBox(tr("Enable Version Control*"));
	CheckBox *autoRefreshFolderContentsCB = new CheckBox(tr("Automatically Refresh Folder Contents"), this);

	QLabel *note_version = new QLabel(tr("* Changes will take effect the next time you run Toonz"));
	note_version->setStyleSheet("font-size: 10px; font: italic;");

	/*--- set default parameters ---*/
	categoryList->setFixedWidth(160);
	categoryList->setCurrentRow(0);
	categoryList->setAlternatingRowColors(true);
	//--- General ------------------------------
	useDefaultViewerCB->setChecked(m_pref->isDefaultViewerEnabled());
	minimizeRasterMemoryCB->setChecked(m_pref->isRasterOptimizedMemory());
	autoSaveCB->setChecked(m_pref->isAutosaveEnabled());
	m_minuteFld->setValue(m_pref->getAutosavePeriod());
	m_minuteFld->setEnabled(m_pref->isAutosaveEnabled());
	replaceAfterSaveLevelAsCB->setChecked(m_pref->isReplaceAfterSaveLevelAsEnabled());

	QStringList dragCellsBehaviourList;
	dragCellsBehaviourList << tr("Cells Only") << tr("Cells and Column Data");
	m_cellsDragBehaviour->addItems(dragCellsBehaviourList);
	m_cellsDragBehaviour->setCurrentIndex(m_pref->getDragCellsBehaviour());
	m_levelsBackup->setChecked(m_pref->isLevelsBackupEnabled());
	sceneNumberingCB->setChecked(m_pref->isSceneNumberingEnabled());

	//--- Interface ------------------------------
	QStringList styleSheetList;
	for (int i = 0; i < m_pref->getStyleSheetCount(); i++) {
		QString string = m_pref->getStyleSheet(i);
		if (string == m_pref->getCurrentStyleSheet())
			currentIndex = i;
		TFilePath path(string.toStdWString());
		styleSheetList.push_back(QString::fromStdWString(path.getWideName()));
	}
	styleSheetType->addItems(styleSheetList);
	styleSheetType->setCurrentIndex(currentIndex);

	QStringList type;
	type << tr("cm") << tr("mm") << tr("inch") << tr("field");
	unitOm->addItems(type);
	int idx = std::find(::units, ::units + ::unitsCount, m_pref->getUnits()) - ::units;
	unitOm->setCurrentIndex((idx < ::unitsCount) ? idx : ::inchIdx);
	cameraUnitOm->addItems(type);

	idx = std::find(::units, ::units + ::unitsCount, m_pref->getCameraUnits()) - ::units;
	cameraUnitOm->setCurrentIndex((idx < ::unitsCount) ? idx : ::inchIdx);

	m_iconSizeLx->setValue(m_pref->getIconSize().lx);
	m_iconSizeLy->setValue(m_pref->getIconSize().ly);
	int shrink, step;
	m_pref->getViewValues(shrink, step);
	m_viewShrink->setValue(shrink);
	m_viewStep->setValue(step);
	moveCurrentFrameCB->setChecked(m_pref->isMoveCurrentEnabled());
	openFlipbookAfterCB->setChecked(m_pref->isGeneratedMovieViewEnabled());
	actualPixelOnSceneModeCB->setChecked(m_pref->isActualPixelViewOnSceneEditingModeEnabled());
	levelNameOnEachMarkerCB->setChecked(m_pref->isLevelNameOnEachMarkerEnabled());
	showRasterImageDarkenBlendedInViewerCB->setChecked(m_pref->isShowRasterImagesDarkenBlendedInViewerEnabled());
	showShowFrameNumberWithLettersCB->setChecked(m_pref->isShowFrameNumberWithLettersEnabled());
	QStringList zoomCenters;
	zoomCenters << tr("Mouse Cursor")
				<< tr("Viewer Center");
	viewerZoomCenterComboBox->addItems(zoomCenters);
	viewerZoomCenterComboBox->setCurrentIndex(m_pref->getViewerZoomCenter());

	//--- Visualization ------------------------------
	show0ThickLinesCB->setChecked(m_pref->getShow0ThickLines());
	regionAntialiasCB->setChecked(m_pref->getRegionAntialias());

	//--- Loading ------------------------------
	exposeLoadedLevelsCB->setChecked(m_pref->isAutoExposeEnabled());
	QStringList behaviors;
	behaviors << tr("On Demand") << tr("All Icons") << tr("All Icons & Images");
	initialLoadTlvCachingBehaviorComboBox->addItems(behaviors);
	initialLoadTlvCachingBehaviorComboBox->setCurrentIndex(m_pref->getInitialLoadTlvCachingBehavior());
	QStringList formats;
	formats << tr("At Once") << tr("On Demand");
	m_columnIconOm->addItems(formats);
	if (m_pref->getColumnIconLoadingPolicy() == Preferences::LoadAtOnce)
		m_columnIconOm->setCurrentIndex(m_columnIconOm->findText(tr("At Once")));
	else
		m_columnIconOm->setCurrentIndex(m_columnIconOm->findText(tr("On Demand")));
	removeSceneNumberFromLoadedLevelNameCB->setChecked(m_pref->isRemoveSceneNumberFromLoadedLevelNameEnabled());
	createSubfolderCB->setChecked(m_pref->isSubsceneFolderEnabled());

	m_addLevelFormat->setFixedSize(20, 20);
	m_removeLevelFormat->setFixedSize(20, 20);

	rebuildFormatsList();

	QStringList paletteTypes;
	paletteTypes << tr("Pick Every Colors as Different Styles") << tr("Integrate Similar Colors as One Style");
	paletteTypeForRasterColorModelComboBox->addItems(paletteTypes);
	paletteTypeForRasterColorModelComboBox->setCurrentIndex(m_pref->getPaletteTypeOnLoadRasterImageAsColorModel());

	//--- Drawing ------------------------------
	keepOriginalCleanedUpCB->setChecked(m_pref->isSaveUnpaintedInCleanupEnable());
	multiLayerStylePickerCB->setChecked(m_pref->isMultiLayerStylePickerEnabled());
	minimizeSaveboxAfterEditingCB->setChecked(m_pref->isMinimizeSaveboxAfterEditing());
	useSaveboxToLimitFillingOpCB->setChecked(m_pref->getFillOnlySavebox());

#ifndef BRAVO
	QStringList scanLevelTypes;
	scanLevelTypes << "tif"
				   << "png";
	m_defScanLevelType->addItems(scanLevelTypes);
	m_defScanLevelType->setCurrentIndex(m_defScanLevelType->findText(m_pref->getScanLevelType()));
#endif
	QStringList levelTypes;
	m_defLevelType->addItem(("Toonz Vector Level"), PLI_XSHLEVEL);
	m_defLevelType->addItem(("Toonz Raster Level"), TZP_XSHLEVEL);
	m_defLevelType->addItem(("Raster Level"), OVL_XSHLEVEL);
	m_defLevelType->setCurrentIndex(0);

	for (int i = 0; i < m_defLevelType->count(); i++) {
		if (m_defLevelType->itemData(i).toInt() == m_pref->getDefLevelType()) {
			m_defLevelType->setCurrentIndex(i);
			onDefLevelTypeChanged(i);
			break;
		}
	}

	m_defLevelWidth->setRange(0.1, (std::numeric_limits<double>::max)());
	m_defLevelWidth->setMeasure("level.lx");
	m_defLevelWidth->setValue(m_pref->getDefLevelWidth());
	m_defLevelHeight->setRange(0.1, (std::numeric_limits<double>::max)());
	m_defLevelHeight->setMeasure("level.ly");
	m_defLevelHeight->setValue(m_pref->getDefLevelHeight());
	m_defLevelDpi->setRange(0.1, (std::numeric_limits<double>::max)());
	m_defLevelDpi->setValue(m_pref->getDefLevelDpi());
	QStringList autocreationTypes;
	autocreationTypes << tr("Disabled")
					  << tr("Enabled")
					  << tr("Use Xsheet as Animation Sheet");
	m_autocreationType->addItems(autocreationTypes);
	int autocreationType = m_pref->getAutocreationType();
	m_autocreationType->setCurrentIndex(autocreationType);

	//--- Xsheet ------------------------------
	xsheetAutopanDuringPlaybackCB->setChecked(m_pref->isXsheetAutopanEnabled());
	m_cellsDragBehaviour->addItem(tr("Cells Only"));
	m_cellsDragBehaviour->addItem(tr("Cells and Column Data"));
	m_cellsDragBehaviour->setCurrentIndex(m_pref->getDragCellsBehaviour());
	ignoreAlphaonColumn1CB->setChecked(m_pref->isIgnoreAlphaonColumn1Enabled());
	showKeyframesOnCellAreaCB->setChecked(m_pref->isShowKeyframesOnXsheetCellAreaEnabled());

	//--- Animation ------------------------------
	QStringList list;
	list << tr("Linear") << tr("Speed In / Speed Out") << tr("Ease In / Ease Out") << tr("Ease In / Ease Out %");
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
	m_frontOnionColor->setEnabled(m_pref->isOnionSkinEnabled());
	m_backOnionColor->setEnabled(m_pref->isOnionSkinEnabled());
	m_inksOnly->setEnabled(m_pref->isOnionSkinEnabled());

	//--- Version Control ------------------------------
	m_enableVersionControl->setChecked(m_pref->isSVNEnabled());
	autoRefreshFolderContentsCB->setChecked(m_pref->isAutomaticSVNFolderRefreshEnabled());

	/*--- layout ---*/

	QHBoxLayout *mainLayout = new QHBoxLayout();
	mainLayout->setMargin(0);
	mainLayout->setSpacing(0);
	{
		//Category
		QVBoxLayout *categoryLayout = new QVBoxLayout();
		categoryLayout->setMargin(5);
		categoryLayout->setSpacing(10);
		{
			categoryLayout->addWidget(new QLabel(tr("Category"), this), 0, Qt::AlignLeft | Qt::AlignVCenter);
			categoryLayout->addWidget(categoryList, 1);
		}
		mainLayout->addLayout(categoryLayout, 0);

		//--- General --------------------------
		QWidget *generalBox = new QWidget(this);
		QVBoxLayout *generalFrameLay = new QVBoxLayout();
		generalFrameLay->setMargin(15);
		generalFrameLay->setSpacing(10);
		{
			generalFrameLay->addWidget(useDefaultViewerCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			generalFrameLay->addWidget(minimizeRasterMemoryCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			QHBoxLayout *saveAutoLay = new QHBoxLayout();
			saveAutoLay->setMargin(0);
			saveAutoLay->setSpacing(15);
			{
				saveAutoLay->addWidget(autoSaveCB, 0);
				saveAutoLay->addWidget(m_minuteFld, 0);
				saveAutoLay->addStretch(1);
			}
			generalFrameLay->addLayout(saveAutoLay, 0);

			//Unit, CameraUnit
			QGridLayout *unitLay = new QGridLayout();
			unitLay->setMargin(0);
			unitLay->setHorizontalSpacing(5);
			unitLay->setVerticalSpacing(10);
			{
				unitLay->addWidget(new QLabel(tr("Undo Memory Size (MB)"), this), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				unitLay->addWidget(m_undoMemorySize, 0, 1);

				unitLay->addWidget(new QLabel(tr("Render Task Chunk Size:"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				unitLay->addWidget(m_chunkSizeFld, 1, 1);
			}
			unitLay->setColumnStretch(0, 0);
			unitLay->setColumnStretch(1, 0);
			unitLay->setColumnStretch(2, 1);

			generalFrameLay->addLayout(unitLay, 0);

			generalFrameLay->addWidget(replaceAfterSaveLevelAsCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			generalFrameLay->addWidget(m_levelsBackup, 0, Qt::AlignLeft | Qt::AlignVCenter);
			generalFrameLay->addWidget(sceneNumberingCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			generalFrameLay->addStretch(1);

			generalFrameLay->addWidget(note_general, 0);
		}
		generalBox->setLayout(generalFrameLay);
		stackedWidget->addWidget(generalBox);

		//--- Interface --------------------------
		QWidget *userInterfaceBox = new QWidget(this);
		QVBoxLayout *userInterfaceFrameLay = new QVBoxLayout();
		userInterfaceFrameLay->setMargin(15);
		userInterfaceFrameLay->setSpacing(10);
		{
			QGridLayout *styleLay = new QGridLayout();
			styleLay->setMargin(0);
			styleLay->setHorizontalSpacing(5);
			styleLay->setVerticalSpacing(10);
			{
				styleLay->addWidget(new QLabel(tr("Style:")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				styleLay->addWidget(styleSheetType, 0, 1);

				styleLay->addWidget(new QLabel(tr("Unit:"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				styleLay->addWidget(unitOm, 1, 1);

				styleLay->addWidget(new QLabel(tr("Camera Unit:"), this), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
				styleLay->addWidget(cameraUnitOm, 2, 1);
			}
			styleLay->setColumnStretch(0, 0);
			styleLay->setColumnStretch(1, 0);
			styleLay->setColumnStretch(2, 1);
			userInterfaceFrameLay->addLayout(styleLay, 0);

			userInterfaceFrameLay->addWidget(openFlipbookAfterCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			userInterfaceFrameLay->addWidget(moveCurrentFrameCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			userInterfaceFrameLay->addWidget(actualPixelOnSceneModeCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			userInterfaceFrameLay->addWidget(levelNameOnEachMarkerCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			userInterfaceFrameLay->addWidget(showRasterImageDarkenBlendedInViewerCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			userInterfaceFrameLay->addWidget(showShowFrameNumberWithLettersCB, 0, Qt::AlignLeft | Qt::AlignVCenter);

			QGridLayout *bgColorsLay = new QGridLayout();
			bgColorsLay->setMargin(0);
			bgColorsLay->setHorizontalSpacing(5);
			bgColorsLay->setVerticalSpacing(10);
			{
				bgColorsLay->addWidget(new QLabel(tr("Icon Size *"), this), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_iconSizeLx, 0, 1);
				bgColorsLay->addWidget(new QLabel(tr("X"), this), 0, 2, Qt::AlignCenter);
				bgColorsLay->addWidget(m_iconSizeLy, 0, 3);

				bgColorsLay->addWidget(new QLabel(tr("Viewer  Shrink"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_viewShrink, 1, 1);
				bgColorsLay->addWidget(new QLabel(tr("Step"), this), 1, 3, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_viewStep, 1, 4);

				bgColorsLay->addWidget(new QLabel(tr("Viewer BG Color"), this), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_viewerBgColorFld, 2, 1, 1, 5);

				bgColorsLay->addWidget(new QLabel(tr("Preview BG Color"), this), 3, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_previewBgColorFld, 3, 1, 1, 5);

				bgColorsLay->addWidget(new QLabel(tr("ChessBoard Color 1"), this), 4, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_chessboardColor1Fld, 4, 1, 1, 5);

				bgColorsLay->addWidget(new QLabel(tr("Chessboard Color 2"), this), 5, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(m_chessboardColor2Fld, 5, 1, 1, 5);

				bgColorsLay->addWidget(new QLabel(tr("Viewer Zoom Center"), this), 6, 0, Qt::AlignRight | Qt::AlignVCenter);
				bgColorsLay->addWidget(viewerZoomCenterComboBox, 6, 1, 1, 4);

				if (languageType) {
					bgColorsLay->addWidget(new QLabel(tr("Language *:")), 7, 0, Qt::AlignRight | Qt::AlignVCenter);
					bgColorsLay->addWidget(languageType, 7, 1, 1, 4);
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
		QWidget *visualizatonBox = new QWidget(this);
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
		QWidget *loadingBox = new QWidget(this);
		QVBoxLayout *loadingFrameLay = new QVBoxLayout();
		loadingFrameLay->setMargin(15);
		loadingFrameLay->setSpacing(10);
		{
			loadingFrameLay->addWidget(exposeLoadedLevelsCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			loadingFrameLay->addWidget(createSubfolderCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			loadingFrameLay->addWidget(removeSceneNumberFromLoadedLevelNameCB, 0, Qt::AlignLeft | Qt::AlignVCenter);

			QGridLayout *cacheLay = new QGridLayout();
			cacheLay->setMargin(0);
			cacheLay->setHorizontalSpacing(5);
			cacheLay->setVerticalSpacing(10);
			{
				cacheLay->addWidget(new QLabel(tr("Default TLV Caching Behavior")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				cacheLay->addWidget(initialLoadTlvCachingBehaviorComboBox, 0, 1);

				cacheLay->addWidget(new QLabel(tr("Column Icon"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				cacheLay->addWidget(m_columnIconOm, 1, 1);

				cacheLay->addWidget(new QLabel(tr("Level Settings by File Format:")), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
				cacheLay->addWidget(m_levelFormatNames, 2, 1);
				cacheLay->addWidget(m_addLevelFormat, 2, 2);
				cacheLay->addWidget(m_removeLevelFormat, 2, 3);
				cacheLay->addWidget(m_editLevelFormat, 2, 4);

				cacheLay->addWidget(new QLabel(tr("Palette Type on Loading Raster Image as Color Model")), 3, 0, Qt::AlignRight | Qt::AlignVCenter);
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

		//--- Drawing --------------------------
		QWidget *drawingBox = new QWidget(this);
		QVBoxLayout *drawingFrameLay = new QVBoxLayout();
		drawingFrameLay->setMargin(15);
		drawingFrameLay->setSpacing(10);
		{
			QGridLayout *drawingTopLay = new QGridLayout();
			drawingTopLay->setVerticalSpacing(10);
			drawingTopLay->setHorizontalSpacing(15);
			drawingTopLay->setMargin(0);
			{
#ifndef BRAVO
				drawingTopLay->addWidget(new QLabel(tr("Scan File Format:")), 0, 0, Qt::AlignRight);
				drawingTopLay->addWidget(m_defScanLevelType, 0, 1, 1, 3);
#endif

				drawingTopLay->addWidget(new QLabel(tr("Default Level Type:")), 1, 0, Qt::AlignRight);
				drawingTopLay->addWidget(m_defLevelType, 1, 1, 1, 3);

				drawingTopLay->addWidget(new QLabel(tr("Width:")), 2, 0, Qt::AlignRight);
				drawingTopLay->addWidget(m_defLevelWidth, 2, 1);
				drawingTopLay->addWidget(new QLabel(tr("  Height:")), 2, 2, Qt::AlignRight);
				drawingTopLay->addWidget(m_defLevelHeight, 2, 3);

				drawingTopLay->addWidget(new QLabel(tr("DPI:")), 3, 0, Qt::AlignRight);
				drawingTopLay->addWidget(m_defLevelDpi, 3, 1);

				drawingTopLay->addWidget(new QLabel(tr("Autocreation:")), 4, 0, Qt::AlignRight);
				drawingTopLay->addWidget(m_autocreationType, 4, 1, 1, 3);
			}
			drawingFrameLay->addLayout(drawingTopLay, 0);

			drawingFrameLay->addWidget(keepOriginalCleanedUpCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			drawingFrameLay->addWidget(minimizeSaveboxAfterEditingCB, 0, Qt::AlignLeft | Qt::AlignVCenter); //6.4
			drawingFrameLay->addWidget(useSaveboxToLimitFillingOpCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			drawingFrameLay->addWidget(multiLayerStylePickerCB, 0, Qt::AlignLeft | Qt::AlignVCenter);
			drawingFrameLay->addStretch(1);
		}
		drawingBox->setLayout(drawingFrameLay);
		stackedWidget->addWidget(drawingBox);

		//--- Xsheet --------------------------
		QWidget *xsheetBox = new QWidget(this);
		QGridLayout *xsheetFrameLay = new QGridLayout();
		xsheetFrameLay->setMargin(15);
		xsheetFrameLay->setHorizontalSpacing(15);
		xsheetFrameLay->setVerticalSpacing(10);
		{
			xsheetFrameLay->addWidget(new QLabel(tr("Next/Previous Step Frames:")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
			xsheetFrameLay->addWidget(m_xsheetStep, 0, 1);

			xsheetFrameLay->addWidget(xsheetAutopanDuringPlaybackCB, 1, 0, 1, 2);

			xsheetFrameLay->addWidget(new QLabel(tr("Cell-dragging Behaviour:")), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
			xsheetFrameLay->addWidget(m_cellsDragBehaviour, 2, 1);

			xsheetFrameLay->addWidget(ignoreAlphaonColumn1CB, 3, 0, 1, 2);
			xsheetFrameLay->addWidget(showKeyframesOnCellAreaCB, 4, 0, 1, 2);
		}
		xsheetFrameLay->setColumnStretch(0, 0);
		xsheetFrameLay->setColumnStretch(1, 0);
		xsheetFrameLay->setColumnStretch(2, 1);
		xsheetFrameLay->setRowStretch(5, 1);
		xsheetBox->setLayout(xsheetFrameLay);
		stackedWidget->addWidget(xsheetBox);

		//--- Animation --------------------------
		QWidget *animationBox = new QWidget(this);
		QGridLayout *animationFrameLay = new QGridLayout();
		animationFrameLay->setMargin(15);
		animationFrameLay->setHorizontalSpacing(15);
		animationFrameLay->setVerticalSpacing(10);
		{
			animationFrameLay->addWidget(new QLabel(tr("Default Interpolation:")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
			animationFrameLay->addWidget(m_keyframeType, 0, 1);

			animationFrameLay->addWidget(new QLabel(tr("Animation Step:")), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
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
		QWidget *previewBox = new QWidget(this);
		QGridLayout *previewLayout = new QGridLayout();
		previewLayout->setMargin(15);
		previewLayout->setHorizontalSpacing(15);
		previewLayout->setVerticalSpacing(10);
		{
			previewLayout->addWidget(new QLabel(tr("Blank Frames:")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
			previewLayout->addWidget(m_blanksCount, 0, 1);

			previewLayout->addWidget(new QLabel(tr("Blank Frames Color:")), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
			previewLayout->addWidget(m_blankColor, 1, 1, 1, 2);

			previewLayout->addWidget(rewindAfterPlaybackCB, 2, 0, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
			previewLayout->addWidget(displayInNewFlipBookCB, 3, 0, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
			previewLayout->addWidget(fitToFlipbookCB, 4, 0, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
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
		QWidget *onionBox = new QWidget(this);
		QVBoxLayout *onionLay = new QVBoxLayout();
		onionLay->setMargin(15);
		onionLay->setSpacing(10);
		{
			onionLay->addWidget(m_onionSkinVisibility, 0, Qt::AlignLeft | Qt::AlignVCenter);
			QGridLayout *onionColorLay = new QGridLayout();
			{
				onionColorLay->addWidget(new QLabel(tr("Paper Thickness:")), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				onionColorLay->addWidget(m_onionPaperThickness, 0, 1);

				onionColorLay->addWidget(new QLabel(tr("Previous  Frames Correction:")), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				onionColorLay->addWidget(m_backOnionColor, 1, 1);

				onionColorLay->addWidget(new QLabel(tr("Following Frames Correction:")), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
				onionColorLay->addWidget(m_frontOnionColor, 2, 1);
			}
			onionColorLay->setColumnStretch(0, 0);
			onionColorLay->setColumnStretch(1, 0);
			onionColorLay->setColumnStretch(2, 1);
			onionLay->addLayout(onionColorLay, 0);

			onionLay->addWidget(m_inksOnly, 0, Qt::AlignLeft | Qt::AlignVCenter);

			onionLay->addStretch(1);
		}
		onionBox->setLayout(onionLay);
		stackedWidget->addWidget(onionBox);

		//--- Transparency Check --------------------------
		QWidget *tcBox = new QWidget(this);
		QGridLayout *tcLay = new QGridLayout();
		tcLay->setMargin(15);
		tcLay->setHorizontalSpacing(10);
		tcLay->setVerticalSpacing(10);
		{
			tcLay->addWidget(new QLabel(tr("Ink Color on White Bg:")), 0, 0, Qt::AlignRight);
			tcLay->addWidget(m_transpCheckInkColor, 0, 1);

			tcLay->addWidget(new QLabel(tr("Ink Color on Black Bg:")), 1, 0, Qt::AlignRight);
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
		QVBoxLayout *vcLay = new QVBoxLayout();
		vcLay->setMargin(15);
		vcLay->setSpacing(10);
		{
			vcLay->addWidget(m_enableVersionControl, 0, Qt::AlignLeft | Qt::AlignVCenter);
			vcLay->addWidget(autoRefreshFolderContentsCB, 0, Qt::AlignLeft | Qt::AlignVCenter);

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
	ret = ret && connect(categoryList, SIGNAL(currentRowChanged(int)), stackedWidget, SLOT(setCurrentIndex(int)));

	//--- General ----------------------
	ret = ret && connect(useDefaultViewerCB, SIGNAL(stateChanged(int)), this, SLOT(onDefaultViewerChanged(int)));
	ret = ret && connect(minimizeRasterMemoryCB, SIGNAL(stateChanged(int)), this, SLOT(onRasterOptimizedMemoryChanged(int)));
	ret = ret && connect(autoSaveCB, SIGNAL(stateChanged(int)), SLOT(onAutoSaveChanged(int)));
	ret = ret && connect(m_minuteFld, SIGNAL(editingFinished()), SLOT(onMinuteChanged()));
	ret = ret && connect(m_cellsDragBehaviour, SIGNAL(currentIndexChanged(int)), SLOT(onDragCellsBehaviourChanged(int)));
	ret = ret && connect(m_undoMemorySize, SIGNAL(editingFinished()), SLOT(onUndoMemorySizeChanged()));
	ret = ret && connect(m_levelsBackup, SIGNAL(stateChanged(int)), SLOT(onLevelsBackupChanged(int)));
	ret = ret && connect(sceneNumberingCB, SIGNAL(stateChanged(int)), SLOT(onSceneNumberingChanged(int)));
	ret = ret && connect(m_chunkSizeFld, SIGNAL(editingFinished()), this, SLOT(onChunkSizeChanged()));

	//--- Interface ----------------------
	ret = ret && connect(styleSheetType, SIGNAL(currentIndexChanged(int)), SLOT(onStyleSheetTypeChanged(int)));
	ret = ret && connect(unitOm, SIGNAL(currentIndexChanged(int)), SLOT(onUnitChanged(int)));
	ret = ret && connect(cameraUnitOm, SIGNAL(currentIndexChanged(int)), SLOT(onCameraUnitChanged(int)));
	ret = ret && connect(openFlipbookAfterCB, SIGNAL(stateChanged(int)), this, SLOT(onViewGeneratedMovieChanged(int)));
	ret = ret && connect(m_iconSizeLx, SIGNAL(editingFinished()), SLOT(onIconSizeChanged()));
	ret = ret && connect(m_iconSizeLy, SIGNAL(editingFinished()), SLOT(onIconSizeChanged()));
	ret = ret && connect(m_viewShrink, SIGNAL(editingFinished()), SLOT(onViewValuesChanged()));
	ret = ret && connect(m_viewStep, SIGNAL(editingFinished()), SLOT(onViewValuesChanged()));
	if (languageList.size() > 1)
		ret = ret && connect(languageType, SIGNAL(currentIndexChanged(int)), SLOT(onLanguageTypeChanged(int)));
	ret = ret && connect(moveCurrentFrameCB, SIGNAL(stateChanged(int)), this, SLOT(onMoveCurrentFrameChanged(int)));
	ret = ret && connect(viewerZoomCenterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onViewerZoomCenterChanged(int)));
	ret = ret && connect(replaceAfterSaveLevelAsCB, SIGNAL(stateChanged(int)), this, SLOT(onReplaceAfterSaveLevelAsChanged(int)));
	ret = ret && connect(showRasterImageDarkenBlendedInViewerCB, SIGNAL(stateChanged(int)), this, SLOT(onShowRasterImageDarkenBlendedInViewerChanged(int)));
	ret = ret && connect(showShowFrameNumberWithLettersCB, SIGNAL(stateChanged(int)), this, SLOT(onShowFrameNumberWithLettersChanged(int)));
	//Viewer BG color
	ret = ret && connect(m_viewerBgColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)), this, SLOT(setViewerBgColor(const TPixel32 &, bool)));
	//Preview BG color
	ret = ret && connect(m_previewBgColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)), this, SLOT(setPreviewBgColor(const TPixel32 &, bool)));
	//bg chessboard colors
	ret = ret && connect(m_chessboardColor1Fld, SIGNAL(colorChanged(const TPixel32 &, bool)), this, SLOT(setChessboardColor1(const TPixel32 &, bool)));
	ret = ret && connect(m_chessboardColor2Fld, SIGNAL(colorChanged(const TPixel32 &, bool)), this, SLOT(setChessboardColor2(const TPixel32 &, bool)));
	//Column Icon
	ret = ret && connect(m_columnIconOm, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(onColumnIconChange(const QString &)));
	ret = ret && connect(actualPixelOnSceneModeCB, SIGNAL(stateChanged(int)), SLOT(onActualPixelOnSceneModeChanged(int)));
	ret = ret && connect(levelNameOnEachMarkerCB, SIGNAL(stateChanged(int)), SLOT(onLevelNameOnEachMarkerChanged(int)));

	//--- Visualization ---------------------
	ret = ret && connect(show0ThickLinesCB, SIGNAL(stateChanged(int)), this, SLOT(onShow0ThickLinesChanged(int)));
	ret = ret && connect(regionAntialiasCB, SIGNAL(stateChanged(int)), this, SLOT(onRegionAntialiasChanged(int)));

	//--- Loading ----------------------
	ret = ret && connect(exposeLoadedLevelsCB, SIGNAL(stateChanged(int)), this, SLOT(onAutoExposeChanged(int)));
	ret = ret && connect(initialLoadTlvCachingBehaviorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onInitialLoadTlvCachingBehaviorChanged(int)));
	ret = ret && connect(createSubfolderCB, SIGNAL(stateChanged(int)), this, SLOT(onSubsceneFolderChanged(int)));
	ret = ret && connect(removeSceneNumberFromLoadedLevelNameCB, SIGNAL(stateChanged(int)), this, SLOT(onRemoveSceneNumberFromLoadedLevelNameChanged(int)));
	ret = ret && connect(m_addLevelFormat, SIGNAL(clicked()), SLOT(onAddLevelFormat()));
	ret = ret && connect(m_removeLevelFormat, SIGNAL(clicked()), SLOT(onRemoveLevelFormat()));
	ret = ret && connect(m_editLevelFormat, SIGNAL(clicked()), SLOT(onEditLevelFormat()));
	ret = ret && connect(paletteTypeForRasterColorModelComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onPaletteTypeForRasterColorModelChanged(int)));

	//--- Drawing ----------------------
	ret = ret && connect(keepOriginalCleanedUpCB, SIGNAL(stateChanged(int)), this, SLOT(onSaveUnpaintedInCleanupChanged(int)));
	ret = ret && connect(multiLayerStylePickerCB, SIGNAL(stateChanged(int)), this, SLOT(onMultiLayerStylePickerChanged(int)));
	ret = ret && connect(useSaveboxToLimitFillingOpCB, SIGNAL(stateChanged(int)), this, SLOT(onGetFillOnlySavebox(int)));
#ifndef BRAVO
	ret = ret && connect(m_defScanLevelType, SIGNAL(currentIndexChanged(const QString &)), SLOT(onScanLevelTypeChanged(const QString &)));
#endif
	ret = ret && connect(minimizeSaveboxAfterEditingCB, SIGNAL(stateChanged(int)), this, SLOT(onMinimizeSaveboxAfterEditing(int)));
	ret = ret && connect(m_defLevelType, SIGNAL(currentIndexChanged(int)), SLOT(onDefLevelTypeChanged(int)));
	ret = ret && connect(m_autocreationType, SIGNAL(currentIndexChanged(int)), SLOT(onAutocreationTypeChanged(int)));
	ret = ret && connect(m_defLevelWidth, SIGNAL(valueChanged()), SLOT(onDefLevelParameterChanged()));
	ret = ret && connect(m_defLevelHeight, SIGNAL(valueChanged()), SLOT(onDefLevelParameterChanged()));
	ret = ret && connect(m_defLevelDpi, SIGNAL(valueChanged()), SLOT(onDefLevelParameterChanged()));

	//--- Xsheet ----------------------
	ret = ret && connect(xsheetAutopanDuringPlaybackCB, SIGNAL(stateChanged(int)), this, SLOT(onXsheetAutopanChanged(int)));
	ret = ret && connect(ignoreAlphaonColumn1CB, SIGNAL(stateChanged(int)), this, SLOT(onIgnoreAlphaonColumn1Changed(int)));
	ret = ret && connect(m_xsheetStep, SIGNAL(editingFinished()), SLOT(onXsheetStepChanged()));
	ret = ret && connect(m_cellsDragBehaviour, SIGNAL(currentIndexChanged(int)), SLOT(onDragCellsBehaviourChanged(int)));
	ret = ret && connect(showKeyframesOnCellAreaCB, SIGNAL(stateChanged(int)), this, SLOT(onShowKeyframesOnCellAreaChanged(int)));

	//--- Animation ----------------------
	ret = ret && connect(m_keyframeType, SIGNAL(currentIndexChanged(int)), SLOT(onKeyframeTypeChanged(int)));
	ret = ret && connect(m_animationStepField, SIGNAL(editingFinished()), SLOT(onAnimationStepChanged()));

	//--- Preview ----------------------
	ret = ret && connect(m_blanksCount, SIGNAL(editingFinished()), SLOT(onBlankCountChanged()));
	ret = ret && connect(m_blankColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onBlankColorChanged(const TPixel32 &, bool)));
	ret = ret && connect(rewindAfterPlaybackCB, SIGNAL(stateChanged(int)), this, SLOT(onRewindAfterPlayback(int)));
	ret = ret && connect(displayInNewFlipBookCB, SIGNAL(stateChanged(int)), this, SLOT(onPreviewAlwaysOpenNewFlip(int)));
	ret = ret && connect(fitToFlipbookCB, SIGNAL(stateChanged(int)), this, SLOT(onFitToFlipbook(int)));

	//--- Onion Skin ----------------------
	ret = ret && connect(m_frontOnionColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onOnionDataChanged(const TPixel32 &, bool)));
	ret = ret && connect(m_backOnionColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onOnionDataChanged(const TPixel32 &, bool)));
	ret = ret && connect(m_inksOnly, SIGNAL(stateChanged(int)), SLOT(onOnionDataChanged(int)));
	ret = ret && connect(m_onionSkinVisibility, SIGNAL(stateChanged(int)), SLOT(onOnionSkinVisibilityChanged(int)));
	ret = ret && connect(m_onionPaperThickness, SIGNAL(editingFinished()), SLOT(onOnionPaperThicknessChanged()));

	//--- Transparency Check ----------------------
	ret = ret && connect(m_transpCheckBgColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));
	ret = ret && connect(m_transpCheckInkColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));
	ret = ret && connect(m_transpCheckPaintColor, SIGNAL(colorChanged(const TPixel32 &, bool)), SLOT(onTranspCheckDataChanged(const TPixel32 &, bool)));

	//--- Version Control ----------------------
	ret = ret && connect(m_enableVersionControl, SIGNAL(stateChanged(int)), SLOT(onSVNEnabledChanged(int)));
	ret = ret && connect(autoRefreshFolderContentsCB, SIGNAL(stateChanged(int)), SLOT(onAutomaticSVNRefreshChanged(int)));

	assert(ret);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PreferencesPopup> openPreferencesPopup(MI_Preferences);
