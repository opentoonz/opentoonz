

#include "outputsettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "tapp.h"
#include "camerasettingspopup.h"
#include "pane.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/filefield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/preferences.h"
#include "toutputproperties.h"
#include "toonz/tcamera.h"

// TnzBase includes
#include "trasterfx.h"

// TnzCore includes
#include "tiio.h"
#include "tlevel_io.h"
#include "tenv.h"
#include "tsystem.h"
#include "tstream.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMainWindow>
#include <QFrame>
#include <QMessageBox>

//-----------------------------------------------------------------------------
namespace
{

enum ResampleOption {
	c_standard,
	c_improved,
	c_high,

	c_triangle,
	c_mitchell,
	c_cubic5,
	c_cubic75,
	c_cubic1,
	c_hann2,
	c_hann3,
	c_hamming2,
	c_hamming3,
	c_lanczos2,
	c_lanczos3,
	c_gauss,
	c_closestPixel,
	c_bilinear
};

enum ChannelWidth {
	c_8bit,
	c_16bit
};

enum DominantField {
	c_odd,
	c_even,
	c_none
};

enum ThreadsOption {
	c_single,
	c_half,
	c_all,
};

enum GranularityOption {
	c_off,
	c_large,
	c_medium,
	c_small
};

} //anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class OutputSettingsPopup
		\brief The OutputSettingsPopup class provides a output settings dialog.

		Inherits \b Dialog.
		This class allows to set output properties of current scene.
		It's composed of a list of field to set this preperties:
		a \b DVGui::FileField to set output directory, a LineEdit for output name,
		a \b QComboBox to choose format, a button near QComboBox that open a popup
		to change format chosen settings, other QComboBox to choose camera,
		resample balance, channel width and dominant field and other number field
		to set values for frame and time stretch.
*/
OutputSettingsPopup::OutputSettingsPopup(bool isPreview)
	: Dialog(TApp::instance()->getMainWindow(),
			 false,
			 Preferences::instance()->getCurrentLanguage() == "english",
			 isPreview ? "PreviewSettings" : "OutputSettings"),
	  m_subcameraChk(0), m_applyShrinkChk(0), m_outputCameraOm(0), m_isPreviewSettings(isPreview), m_presetCombo(0)
{
	setWindowTitle(isPreview ? tr("Preview Settings") : tr("Output Settings"));

	QLabel *cameraSettingsLabel = new QLabel(tr("Camera Settings"), this);
	QLabel *fileSettingsLabel = new QLabel(tr("File Settings"), this);
	QFrame *cameraSettingsBox = new QFrame(this);
	QFrame *fileSettingsBox = new QFrame(this);
	QFrame *cameraParametersBox;
	if (!isPreview) {
		//Save In
		m_saveInFileFld = new DVGui::FileField(0, QString(""));
		//File Name
		m_fileNameFld = new DVGui::LineEdit(QString(""));
		//File Format
		m_fileFormat = new QComboBox();
		m_fileFormatButton = new QPushButton(tr("Options"));

		m_cameraSettings = new CameraSettingsPopup();
		cameraParametersBox = new QFrame(this);
	}
	//Output Camera
	m_outputCameraOm = new QComboBox();
//m_outputCameraOm->hide();
	if (isPreview) {
		//Subcamera checkbox
		m_subcameraChk = new DVGui::CheckBox(tr("Use Sub-Camera"));
	}

	//Frame Start-End
	m_startFld = new DVGui::IntLineEdit(this);
	m_endFld = new DVGui::IntLineEdit(this);
	//Step-Shrink
	m_stepFld = new DVGui::IntLineEdit(this);
	m_shrinkFld = new DVGui::IntLineEdit(this);
	if (isPreview)
		m_applyShrinkChk = new DVGui::CheckBox(tr("Apply Shrink to Main Viewer"), this);
	else
		m_applyShrinkChk = 0;
	//Resample Balance
	m_resampleBalanceOm = new QComboBox();
	//Channel Width
	m_channelWidthOm = new QComboBox();

	QPushButton *showOtherSettingsButton = NULL;
	QLabel *otherSettingsLabel = NULL;
	QFrame *otherSettingsFrame = NULL;
	QPushButton *showCameraSettingsButton = NULL;
	QPushButton *addPresetButton = NULL;
	QPushButton *removePresetButton = NULL;

	m_dominantFieldOm = 0;
	if (!isPreview) {
		showOtherSettingsButton = new QPushButton("", this);
		otherSettingsLabel = new QLabel(tr("Other Settings"), this);
		otherSettingsFrame = new QFrame(this);

		//Gamma
		m_gammaFld = new DVGui::DoubleLineEdit();
		//Dominant Field
		m_dominantFieldOm = new QComboBox();

		//Stretch
		TRenderSettings rs = getProperties()->getRenderSettings();

		//Scene Settings FPS
		double frameRate = getProperties()->getFrameRate();
		m_frameRateFld = new DVGui::DoubleLineEdit(this, frameRate);

		m_stretchFromFld = new DVGui::DoubleLineEdit(this, rs.m_timeStretchFrom);
		m_stretchToFld = new DVGui::DoubleLineEdit(this, rs.m_timeStretchTo);
		m_multimediaOm = new QComboBox(this);

		showCameraSettingsButton = new QPushButton("", this);
		addPresetButton = new QPushButton(tr("Add"), this);
		removePresetButton = new QPushButton(tr("Remove"), this);
		m_presetCombo = new QComboBox(this);

		m_doStereoscopy = new DVGui::CheckBox("Do stereoscopy", this);
		m_stereoShift = new DVGui::DoubleLineEdit(this, 0.05);
	}
	//Threads
	m_threadsComboOm = new QComboBox();
	//Granularity
	m_rasterGranularityOm = new QComboBox();

	//----プロパティの設定

	cameraSettingsLabel->setObjectName("OutputSettingsLabel");
	fileSettingsLabel->setObjectName("OutputSettingsLabel");
	cameraSettingsBox->setObjectName("OutputSettingsBox");
	fileSettingsBox->setObjectName("OutputSettingsBox");

	if (!isPreview) {
		//File Format
		QStringList formats;
		TImageWriter::getSupportedFormats(formats, true);
		TLevelWriter::getSupportedFormats(formats, true);
		Tiio::Writer::getSupportedFormats(formats, true);
		formats.sort();
		m_fileFormat->addItems(formats);
		cameraParametersBox->setObjectName("OutputSettingsCameraBox");
	}

	//Resample Balance
	QStringList resampleBalance;
	resampleBalance << tr("Standard")
					<< tr("Improved")
					<< tr("High")
					<< tr("Triangle filter")
					<< tr("Mitchell-Netravali filter")
					<< tr("Cubic convolution, a = .5")
					<< tr("Cubic convolution, a = .75")
					<< tr("Cubic convolution, a = 1")
					<< tr("Hann window, rad = 2")
					<< tr("Hann window, rad = 3")
					<< tr("Hamming window, rad = 2")
					<< tr("Hamming window, rad = 3")
					<< tr("Lanczos window, rad = 2")
					<< tr("Lanczos window, rad = 3")
					<< tr("Gaussian convolution")
					<< tr("Closest Pixel (Nearest Neighbor)")
					<< tr("Bilinear");

	m_resampleBalanceOm->addItems(resampleBalance);

	//Channel Width
	QStringList chWidth;
	chWidth << tr("8 bit") << tr("16 bit");
	m_channelWidthOm->addItems(chWidth);

	if (!isPreview) {
		showOtherSettingsButton->setObjectName("OutputSettingsShowButton");
		showOtherSettingsButton->setFixedSize(15, 15);
		showOtherSettingsButton->setCheckable(true);
		showOtherSettingsButton->setChecked(false);
		showOtherSettingsButton->setFocusPolicy(Qt::NoFocus);
		otherSettingsLabel->setObjectName("OutputSettingsLabel");
		otherSettingsFrame->setObjectName("OutputSettingsBox");
		otherSettingsFrame->setVisible(false);

		//Dominant Field
		QStringList dominantField;
		dominantField << tr("Odd (NTSC)") << tr("Even (PAL)") << tr("None");
		m_dominantFieldOm->addItems(dominantField);
		m_stretchFromFld->setRange(1, 1000);
		m_stretchToFld->setRange(1, 1000);
		m_stretchFromFld->setDecimals(2);
		m_stretchToFld->setDecimals(2);

		//Multimedia rendering enum
		QStringList multimediaTypes;
		multimediaTypes << tr("None") << tr("Fx Schematic Flows") << tr("Fx Schematic Terminal Nodes");
		m_multimediaOm->addItems(multimediaTypes);

		showCameraSettingsButton->setObjectName("OutputSettingsShowButton");
		showCameraSettingsButton->setFixedSize(15, 15);
		showCameraSettingsButton->setCheckable(true);
		showCameraSettingsButton->setChecked(true);
		showCameraSettingsButton->setFocusPolicy(Qt::NoFocus);

		addPresetButton->setObjectName("PushButton_NoPadding");
		removePresetButton->setObjectName("PushButton_NoPadding");
		/*-- OutputSettingsのプリセット登録の説 明--*/
		QString tooltip("Save current output settings.\nThe parameters to be saved are:\n\
- Camera settings\n- Project folder to be saved in\n- File format\n- File options\n- Resample Balance\n- Channel width");
		addPresetButton->setToolTip(tooltip);
		/*-- プリセットフォルダを調べ、コンボボックスにアイテムを格納する --*/
		updatePresetComboItems();
		m_stereoShift->setEnabled(false);
	}

	QStringList threadsChoices;
	threadsChoices << tr("Single") << tr("Half") << tr("All");
	m_threadsComboOm->addItems(threadsChoices);
	QStringList granularityChoices;
	granularityChoices << tr("None") << tr("Large") << tr("Medium") << tr("Small");
	m_rasterGranularityOm->addItems(granularityChoices);

	//----layout

	m_topLayout->setMargin(5);
	m_topLayout->setSpacing(3);
	{
		if (!isPreview) {
			QHBoxLayout *presetLay = new QHBoxLayout();
			presetLay->setMargin(0);
			presetLay->setSpacing(5);
			{
				presetLay->addStretch(1);
				presetLay->addWidget(new QLabel(tr("Presets:"), this), 1, Qt::AlignRight | Qt::AlignVCenter);
				presetLay->addWidget(m_presetCombo, 1);
				presetLay->addWidget(addPresetButton, 0);
				presetLay->addWidget(removePresetButton, 0);
			}
			m_topLayout->addLayout(presetLay, 0);
		}

		QHBoxLayout *cameraSettingsLabelLay = new QHBoxLayout();
		cameraSettingsLabelLay->setMargin(0);
		cameraSettingsLabelLay->setSpacing(3);
		{
			if (!isPreview)
				cameraSettingsLabelLay->addWidget(showCameraSettingsButton, 0);
			cameraSettingsLabelLay->addWidget(cameraSettingsLabel, 0);
			cameraSettingsLabelLay->addStretch(1);
		}
		m_topLayout->addLayout(cameraSettingsLabelLay, 0);

		QVBoxLayout *camSetBoxLay = new QVBoxLayout();
		camSetBoxLay->setMargin(10);
		camSetBoxLay->setSpacing(10);
		{
			//Output Camera
			QHBoxLayout *outCamLay = new QHBoxLayout();
			outCamLay->setMargin(0);
			outCamLay->setSpacing(5);
			{
				outCamLay->addWidget(new QLabel(tr("Output Camera:"), this), 0);
				outCamLay->addWidget(m_outputCameraOm, 1);
			}
			camSetBoxLay->addLayout(outCamLay);

			if (!isPreview) {
				QVBoxLayout *camParamLay = new QVBoxLayout();
				camParamLay->setMargin(5);
				camParamLay->setSpacing(0);
				{
					camParamLay->addWidget(m_cameraSettings);
				}
				cameraParametersBox->setLayout(camParamLay);

				camSetBoxLay->addWidget(cameraParametersBox);
			}

			//Frame start/end
			QHBoxLayout *frameLay = new QHBoxLayout();
			frameLay->setMargin(0);
			frameLay->setSpacing(5);
			{
				frameLay->addWidget(new QLabel(tr("Frame Start:"), this), 0);
				frameLay->addWidget(m_startFld, 0);
				frameLay->addSpacing(3);
				frameLay->addWidget(new QLabel(tr("End:"), this), 0);
				frameLay->addWidget(m_endFld, 0);
				frameLay->addSpacing(10);
				frameLay->addWidget(new QLabel(tr("Step:"), this), 0);
				frameLay->addWidget(m_stepFld, 0);
				frameLay->addStretch(1);
			}
			camSetBoxLay->addLayout(frameLay);

			//Frame Shrink
			QHBoxLayout *shrinkLay = new QHBoxLayout();
			shrinkLay->setMargin(0);
			shrinkLay->setSpacing(5);
			{
				shrinkLay->addWidget(new QLabel(tr("Shrink:"), this), 0);
				shrinkLay->addWidget(m_shrinkFld, 0);
				shrinkLay->addStretch();
				//Apply Shrink to MainViewer
				if (isPreview)
					shrinkLay->addWidget(m_applyShrinkChk, 0);
			}
			camSetBoxLay->addLayout(shrinkLay);
		}
		cameraSettingsBox->setLayout(camSetBoxLay);
		m_topLayout->addWidget(cameraSettingsBox, 0);

		m_topLayout->addSpacing(10);

		//File Settings
		m_topLayout->addWidget(fileSettingsLabel, 0);

		QVBoxLayout *fileSetBoxLay = new QVBoxLayout();
		fileSetBoxLay->setMargin(10);
		fileSetBoxLay->setSpacing(10);
		{
			if (!isPreview) {
				QGridLayout *upperGridLay = new QGridLayout();
				upperGridLay->setMargin(0);
				upperGridLay->setHorizontalSpacing(5);
				upperGridLay->setVerticalSpacing(10);
				{
					//Save In
					upperGridLay->addWidget(new QLabel(tr("Save in:"), this), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
					upperGridLay->addWidget(m_saveInFileFld, 0, 1, 1, 3);
					//File Name
					upperGridLay->addWidget(new QLabel(tr("Name:"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
					upperGridLay->addWidget(m_fileNameFld, 1, 1);
					//File Format
					upperGridLay->addWidget(m_fileFormat, 1, 2);
					upperGridLay->addWidget(m_fileFormatButton, 1, 3);
				}
				upperGridLay->setColumnStretch(0, 0);
				upperGridLay->setColumnStretch(1, 1);
				upperGridLay->setColumnStretch(2, 0);
				upperGridLay->setColumnStretch(3, 0);

				fileSetBoxLay->addLayout(upperGridLay);
			}

			QGridLayout *bottomGridLay = new QGridLayout();
			bottomGridLay->setMargin(0);
			bottomGridLay->setHorizontalSpacing(5);
			bottomGridLay->setVerticalSpacing(10);
			{
				//Resample Balance
				bottomGridLay->addWidget(new QLabel(tr("Resample Balance:"), this), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
				bottomGridLay->addWidget(m_resampleBalanceOm, 0, 1, 1, 2);
				//Channel Width
				bottomGridLay->addWidget(new QLabel(tr("Channel Width:"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
				bottomGridLay->addWidget(m_channelWidthOm, 1, 1, 1, 2);
				//Threads
				bottomGridLay->addWidget(new QLabel(tr("Dedicated CPUs:"), this), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
				bottomGridLay->addWidget(m_threadsComboOm, 2, 1, 1, 2);
				//Granularity
				bottomGridLay->addWidget(new QLabel(tr("Render Tile:"), this), 3, 0, Qt::AlignRight | Qt::AlignVCenter);
				bottomGridLay->addWidget(m_rasterGranularityOm, 3, 1, 1, 2);
				if (m_subcameraChk) {
					bottomGridLay->addWidget(m_subcameraChk, 4, 1, 1, 2);
				}
			}
			bottomGridLay->setColumnStretch(0, 0);
			bottomGridLay->setColumnStretch(1, 1);

			fileSetBoxLay->addLayout(bottomGridLay);

			//shrinkable settings
			if (!isPreview) {
				fileSetBoxLay->addSpacing(5);

				QHBoxLayout *otherSettingsLabelLay = new QHBoxLayout();
				otherSettingsLabelLay->setMargin(0);
				otherSettingsLabelLay->setSpacing(3);
				{
					otherSettingsLabelLay->addWidget(showOtherSettingsButton, 0);
					otherSettingsLabelLay->addWidget(otherSettingsLabel, 0);
					otherSettingsLabelLay->addStretch(1);
				}
				fileSetBoxLay->addLayout(otherSettingsLabelLay, 0);

				QGridLayout *otherSettingsLay = new QGridLayout();
				otherSettingsLay->setMargin(5);
				otherSettingsLay->setHorizontalSpacing(5);
				otherSettingsLay->setVerticalSpacing(10);
				{
					//Gamma
					otherSettingsLay->addWidget(new QLabel(tr("Gamma:"), this), 0, 0, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_gammaFld, 0, 1, 1, 3);
					//Dominant Field
					otherSettingsLay->addWidget(new QLabel(tr("Dominant Field:"), this), 1, 0, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_dominantFieldOm, 1, 1, 1, 4);
					//Scene Settings' FPS
					otherSettingsLay->addWidget(new QLabel(tr("Frame Rate (linked to Scene Settings):"), this), 2, 0, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_frameRateFld, 2, 1, 1, 3);
					//Strech
					otherSettingsLay->addWidget(new QLabel(tr("Stretch from FPS:"), this), 3, 0, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_stretchFromFld, 3, 1);
					otherSettingsLay->addWidget(new QLabel(tr("  To:"), this), 3, 2, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_stretchToFld, 3, 3);
					//new in V6.1
					//Multimedia rendering enum
					otherSettingsLay->addWidget(new QLabel(tr("Multiple Rendering:"), this), 4, 0, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_multimediaOm, 4, 1, 1, 4);

					otherSettingsLay->addWidget(m_doStereoscopy, 5, 0);
					otherSettingsLay->addWidget(new QLabel(tr("Camera Shift:")), 5, 1, Qt::AlignRight | Qt::AlignVCenter);
					otherSettingsLay->addWidget(m_stereoShift, 5, 2);

					otherSettingsLay->addLayout(bottomGridLay, 6, 0, 4, 5);
				}
				otherSettingsLay->setColumnStretch(0, 0);
				otherSettingsLay->setColumnStretch(1, 0);
				otherSettingsLay->setColumnStretch(2, 0);
				otherSettingsLay->setColumnStretch(3, 0);
				otherSettingsLay->setColumnStretch(4, 1);

				otherSettingsFrame->setLayout(otherSettingsLay);
				fileSetBoxLay->addWidget(otherSettingsFrame, 0);
			}
		}
		fileSettingsBox->setLayout(fileSetBoxLay);
		m_topLayout->addWidget(fileSettingsBox, 0);

		m_topLayout->addStretch(1);
	}

	//----signal-slot connections
	bool ret = true;

	if (!isPreview) {
		ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this, SLOT(onPathChanged()));
		ret = ret && connect(m_fileNameFld, SIGNAL(editingFinished()), SLOT(onNameChanged()));
		ret = ret && connect(m_fileFormat, SIGNAL(currentIndexChanged(const QString &)),
							 SLOT(onFormatChanged(const QString &)));
		ret = ret && connect(m_fileFormatButton, SIGNAL(pressed()), this, SLOT(openSettingsPopup()));
	}
	ret = ret && connect(m_outputCameraOm, SIGNAL(currentIndexChanged(const QString &)),
						 SLOT(onCameraChanged(const QString &)));
	ret = ret && connect(m_startFld, SIGNAL(editingFinished()), SLOT(onFrameFldEditFinished()));
	ret = ret && connect(m_endFld, SIGNAL(editingFinished()), SLOT(onFrameFldEditFinished()));
	ret = ret && connect(m_stepFld, SIGNAL(editingFinished()), SLOT(onFrameFldEditFinished()));
	ret = ret && connect(m_shrinkFld, SIGNAL(editingFinished()), SLOT(onFrameFldEditFinished()));

	if (isPreview) {
		ret = ret && connect(m_applyShrinkChk, SIGNAL(stateChanged(int)), SLOT(onApplyShrinkChecked(int)));
	}

	ret = ret && connect(m_resampleBalanceOm, SIGNAL(currentIndexChanged(int)), SLOT(onResampleChanged(int)));
	ret = ret && connect(m_channelWidthOm, SIGNAL(currentIndexChanged(int)), SLOT(onChannelWidthChanged(int)));

	if (!isPreview) {
		ret = ret && connect(m_gammaFld, SIGNAL(editingFinished()), SLOT(onGammaFldEditFinished()));
		ret = ret && connect(m_dominantFieldOm, SIGNAL(currentIndexChanged(int)), SLOT(onDominantFieldChanged(int)));
		ret = ret && connect(m_frameRateFld, SIGNAL(editingFinished()), this, SLOT(onFrameRateEditingFinished()));
		ret = ret && connect(m_stretchFromFld, SIGNAL(editingFinished()), SLOT(onStretchFldEditFinished()));
		ret = ret && connect(m_stretchToFld, SIGNAL(editingFinished()), SLOT(onStretchFldEditFinished()));
		ret = ret && connect(m_multimediaOm, SIGNAL(currentIndexChanged(int)), this, SLOT(onMultimediaChanged(int)));
		ret = ret && connect(showOtherSettingsButton, SIGNAL(toggled(bool)), otherSettingsFrame, SLOT(setVisible(bool)));
		ret = ret && connect(showCameraSettingsButton, SIGNAL(toggled(bool)), cameraSettingsBox, SLOT(setVisible(bool)));
		ret = ret && connect(addPresetButton, SIGNAL(pressed()), this, SLOT(onAddPresetButtonPressed()));
		ret = ret && connect(removePresetButton, SIGNAL(pressed()), this, SLOT(onRemovePresetButtonPressed()));
		ret = ret && connect(m_presetCombo, SIGNAL(activated(const QString &)), this, SLOT(onPresetSelected(const QString &)));
		ret = ret && connect(m_cameraSettings, SIGNAL(changed()), this, SLOT(onCameraSettingsChanged()));
		ret = ret && connect(m_doStereoscopy, SIGNAL(stateChanged(int)), SLOT(onStereoChecked(int)));
		ret = ret && connect(m_stereoShift, SIGNAL(editingFinished()), SLOT(onStereoChanged()));
	}

	ret = ret && connect(m_threadsComboOm, SIGNAL(currentIndexChanged(int)),
						 SLOT(onThreadsComboChanged(int)));
	ret = ret && connect(m_rasterGranularityOm, SIGNAL(currentIndexChanged(int)),
						 SLOT(onRasterGranularityChanged(int)));

	if (m_subcameraChk)
		ret = ret && connect(m_subcameraChk, SIGNAL(stateChanged(int)), SLOT(onSubcameraChecked(int)));

	assert(ret);
}

//-----------------------------------------------------------------------------
/*! Return current \b ToonzScene scene.
*/
ToonzScene *OutputSettingsPopup::getCurrentScene() const
{
	ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
	return scene;
}

//-----------------------------------------------------------------------------
/*!	Return current scene output properties \b TOutputProperties, if
		current scene is not empty.
*/
TOutputProperties *OutputSettingsPopup::getProperties() const
{
	ToonzScene *scene = getCurrentScene();
	if (!scene)
		return 0;

	return m_isPreviewSettings ? scene->getProperties()->getPreviewProperties() : scene->getProperties()->getOutputProperties();
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::showEvent(QShowEvent *)
{
	TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
	TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
	bool ret = connect(sceneHandle, SIGNAL(sceneChanged()),
					   this, SLOT(updateField()));
	ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()),
						 this, SLOT(updateField()));

	ret = ret && connect(xsheetHandle, SIGNAL(xsheetChanged()),
						 this, SLOT(updateField()));
	updateField();
	assert(ret);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::hideEvent(QHideEvent *e)
{
	TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
	TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
	bool ret = disconnect(sceneHandle, SIGNAL(sceneChanged()),
						  this, SLOT(updateField()));
	ret = ret && disconnect(sceneHandle, SIGNAL(sceneSwitched()),
							this, SLOT(updateField()));

	ret = ret && disconnect(xsheetHandle, SIGNAL(xsheetChanged()),
							this, SLOT(updateField()));
	assert(ret);
	Dialog::hideEvent(e);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onApplyShrinkChecked(int state)
{
	TRenderSettings rs = getProperties()->getRenderSettings();
	rs.m_applyShrinkToViewer = (state == Qt::Checked);
	getProperties()->setRenderSettings(rs);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onSubcameraChecked(int state)
{
	getProperties()->setSubcameraPreview(state == Qt::Checked);
}

//-----------------------------------------------------------------------------

/*!	Update all field value take care current scene output properties.
*/
void OutputSettingsPopup::updateField()
{
	ToonzScene *scene = getCurrentScene();
	TOutputProperties *prop = getProperties();
	if (!prop) {
		if (!m_isPreviewSettings) {
			m_saveInFileFld->setPath(QString());
			m_fileNameFld->setText(QString());
			m_fileFormat->setCurrentIndex(0);
			m_gammaFld->setText(QString());
			m_dominantFieldOm->setCurrentIndex(c_none);
			m_frameRateFld->setText(QString());
			m_stretchFromFld->setText(QString());
			m_stretchToFld->setText(QString());
			m_multimediaOm->setCurrentIndex(0);
		}
		if (m_outputCameraOm)
			m_outputCameraOm->setCurrentIndex(0);
		m_startFld->setText(QString());
		m_endFld->setText(QString());
		m_stepFld->setText(QString());
		m_shrinkFld->setText(QString());
		if (m_applyShrinkChk)
			m_applyShrinkChk->setCheckState(Qt::Unchecked);
		m_resampleBalanceOm->setCurrentIndex(c_standard);
		m_channelWidthOm->setCurrentIndex(c_8bit);
		m_threadsComboOm->setCurrentIndex(0);
		m_rasterGranularityOm->setCurrentIndex(0);

		if (m_subcameraChk)
			m_subcameraChk->setCheckState(Qt::Unchecked);
		return;
	}

	TRenderSettings renderSettings = prop->getRenderSettings();

	if (!m_isPreviewSettings) {
		TFilePath path = prop->getPath();
		QString name = QString::fromStdWString(path.getWideName());
		if (name.isEmpty())
			name = QString::fromStdString(scene->getScenePath().getName());
		m_saveInFileFld->setPath(toQString(path.getParentDir()));
		m_fileNameFld->setText(name);
		//RenderController::instance()->setMovieExt(path.getType());
		m_fileFormat->setCurrentIndex(m_fileFormat->findText(QString::fromStdString(path.getType())));
		m_multimediaOm->setCurrentIndex(prop->getMultimediaRendering());
	}

	// camera
	if (m_outputCameraOm) {
		m_outputCameraOm->blockSignals(true);
		m_outputCameraOm->clear();
	}
	TStageObjectTree *tree = scene->getXsheet()->getStageObjectTree();
	int cameraCount = tree->getCameraCount();
	TStageObjectId cameraId = m_isPreviewSettings ? tree->getCurrentPreviewCameraId() : tree->getCurrentCameraId();
	QStringList cameras;
	int tmpCameraId = 0;
	int index = 0;
	int i;
	for (i = 0; i < cameraCount;) {
		/*-- Deleteされたカメラの場合tmpCameraIdだけIncrementする --*/
		if (!tree->getStageObject(TStageObjectId::CameraId(tmpCameraId), false)) {
			tmpCameraId++;
			continue;
		}

		std::string name = tree->getStageObject(TStageObjectId::CameraId(tmpCameraId))->getName();
		cameras.append(QString::fromStdString(name));
		if (name == tree->getStageObject(cameraId)->getName())
			index = i;
		i++;
		tmpCameraId++;
	}
	if (m_outputCameraOm) {
		m_outputCameraOm->addItems(cameras);
		m_outputCameraOm->blockSignals(false);
		m_outputCameraOm->setCurrentIndex(index);
	}
	if (m_subcameraChk)
		m_subcameraChk->setCheckState(prop->isSubcameraPreview() ? Qt::Checked : Qt::Unchecked);

	// start,end,step
	int r0 = 0, r1 = -1, step;
	prop->getRange(r0, r1, step);
	if (r0 > r1) {
		r0 = 0;
		r1 = scene->getFrameCount() - 1;
		if (r1 < 0)
			r1 = 0;
	}
	m_startFld->setValue(r0 + 1);
	m_endFld->setValue(r1 + 1);
	m_stepFld->setValue(step);
	m_shrinkFld->setValue(renderSettings.m_shrinkX);
	if (m_applyShrinkChk)
		m_applyShrinkChk->setCheckState(renderSettings.m_applyShrinkToViewer ? Qt::Checked : Qt::Unchecked);
	// resample
	switch (renderSettings.m_quality) {
	case TRenderSettings::ImprovedResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_improved);
		break;
	case TRenderSettings::HighResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_high);
		break;
	case TRenderSettings::StandardResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_standard);
		break;

	case TRenderSettings::Mitchell_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_mitchell);
		break;
	case TRenderSettings::Cubic5_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_cubic5);
		break;
	case TRenderSettings::Cubic75_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_cubic75);
		break;
	case TRenderSettings::Cubic1_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_cubic1);
		break;
	case TRenderSettings::Hann2_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_hann2);
		break;
	case TRenderSettings::Hann3_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_hann3);
		break;
	case TRenderSettings::Hamming2_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_hamming2);
		break;
	case TRenderSettings::Hamming3_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_hamming3);
		break;
	case TRenderSettings::Lanczos2_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_lanczos2);
		break;
	case TRenderSettings::Lanczos3_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_lanczos3);
		break;
	case TRenderSettings::Gauss_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_gauss);
		break;
	case TRenderSettings::ClosestPixel_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_closestPixel);
		break;
	case TRenderSettings::Bilinear_FilterResampleQuality:
		m_resampleBalanceOm->setCurrentIndex(c_bilinear);
		break;
	default:
		m_resampleBalanceOm->setCurrentIndex(c_standard);
		break;
	}

	// channel width
	switch (renderSettings.m_bpp) {
	case 64:
		m_channelWidthOm->setCurrentIndex(c_16bit);
		break;
	default:
		m_channelWidthOm->setCurrentIndex(c_8bit);
		break;
	}

	//Threads
	m_threadsComboOm->setCurrentIndex(prop->getThreadIndex());

	//Raster granularity
	m_rasterGranularityOm->setCurrentIndex(prop->getMaxTileSizeIndex());

	if (m_isPreviewSettings)
		return;

	m_doStereoscopy->setChecked(renderSettings.m_stereoscopic);
	m_stereoShift->setValue(renderSettings.m_stereoscopicShift);
	m_stereoShift->setEnabled(renderSettings.m_stereoscopic);

	// gamma
	m_gammaFld->setValue(renderSettings.m_gamma);

	// dominant field
	switch (renderSettings.m_fieldPrevalence) {
	case TRenderSettings::OddField:
		m_dominantFieldOm->setCurrentIndex(c_odd);
		break;
	case TRenderSettings::EvenField:
		m_dominantFieldOm->setCurrentIndex(c_even);
		break;
	case TRenderSettings::NoField:
	default:
		m_dominantFieldOm->setCurrentIndex(c_none);
	}
	m_dominantFieldOm->setEnabled(!renderSettings.m_stereoscopic);
	// time stretch
	m_stretchFromFld->setValue(renderSettings.m_timeStretchFrom);
	m_stretchToFld->setValue(renderSettings.m_timeStretchTo);

	m_frameRateFld->setValue(prop->getFrameRate());
}

//-----------------------------------------------------------------------------
/*! Set current scene output path to new path modified in popup field.
*/
void OutputSettingsPopup::onPathChanged()
{
	if (!getCurrentScene())
		return;

	QString text = m_saveInFileFld->getPath();
	TFilePath newFp(text.toStdWString());
	newFp = TApp::instance()->getCurrentScene()->getScene()->codeFilePath(newFp);
	m_saveInFileFld->setPath(QString::fromStdWString(newFp.getWideString()));

	TOutputProperties *prop = getProperties();
	TFilePath fp = prop->getPath();

	if (fp == newFp)
		return;
	fp = fp.withParentDir(newFp);
	prop->setPath(fp);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);

	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output name to new name modified in popup field.
*/
void OutputSettingsPopup::onNameChanged()
{
	ToonzScene *scene = getCurrentScene();
	if (!scene)
		return;

	QString name = m_fileNameFld->text();
	if (!isValidFileName(name)) {
		DVGui::error("A filename cannot be empty or contain any of the following characters:\n \\ / : * ? \" < > |");
		TOutputProperties *prop = getProperties();
		TFilePath fp = prop->getPath();
		QString name = QString::fromStdString(fp.getName());
		if (name.isEmpty())
			name = QString::fromStdString(scene->getScenePath().getName());
		m_fileNameFld->setText(name);

		return;
	}

	std::wstring wname = name.toStdWString();
	{
		// It could be possible that users add an extension to the level name.
		// This is not supported, and the extension must be removed.
		// Observe that leaving wname as is WOULD be a bug (see Bugzilla #6578).

		const std::wstring &actualWName = TFilePath(wname).getWideName();
		if (wname != actualWName) {
			wname = actualWName;
			m_fileNameFld->setText(QString::fromStdWString(wname));
		}
	}

	// In case the output level name is reverted back to the scene name, we assume that
	// a "Save Scene As..." command should move the value to the *new* scene name
	if (wname == scene->getScenePath().getWideName())
		wname = std::wstring(); // An empty name is redirected to the scene name
								// directly by the render procedure, let's use this!
	TOutputProperties *prop = getProperties();
	TFilePath fp = prop->getPath();

	if (fp.getWideName() == wname)
		return; // Already had the right name

	fp = fp.withName(wname);
	prop->setPath(fp);

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);

	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output format to new format set in popup field.
*/
void OutputSettingsPopup::onFormatChanged(const QString &str)
{
	//string ext = RenderController::instance()->format2ext(str.toStdString());
	//RenderController::instance()->setMovieExt(ext);

	TOutputProperties *prop = getProperties();
	TFilePath fp = prop->getPath().withType(str.toStdString());
	prop->setPath(fp);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);

	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Open a popup output format settings popup to set current output format
		settings.
*/
void OutputSettingsPopup::openSettingsPopup()
{
	TOutputProperties *prop = getProperties();
	std::string ext = prop->getPath().getType();
	openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext));

	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output camera to camera set in popup field.
*/
void OutputSettingsPopup::onCameraChanged(const QString &str)
{
	ToonzScene *scene = getCurrentScene();
	if (!scene)
		return;
	TStageObjectTree *tree = scene->getXsheet()->getStageObjectTree();
	int cameraCount = tree->getCameraCount();
	/*-- 注：CameraCountはDeleteされたCameraは入らないがIndexは欠番になる。
		ので、CameraCountとIndexの最大値は必ずしも一致しない
	--*/
	int tmpCameraId = 0;
	int i;
	for (i = 0; i < cameraCount;) {
		if (!tree->getStageObject(TStageObjectId::CameraId(tmpCameraId), false)) {
			tmpCameraId++;
			continue;
		}
		std::string name = tree->getStageObject(TStageObjectId::CameraId(tmpCameraId))->getName();
		if (name == str.toStdString())
			break;
		tmpCameraId++;
		i++;
	}
	if (i >= cameraCount)
		return;
	TApp *app = TApp::instance();
	TStageObjectId cameraId = TStageObjectId::CameraId(i);
	TStageObjectId currentCameraId = m_isPreviewSettings ? tree->getCurrentPreviewCameraId() : tree->getCurrentCameraId();
	if (currentCameraId != cameraId) {
		TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
		disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(updateField()));

		if (m_isPreviewSettings)
			tree->setCurrentPreviewCameraId(cameraId);
		else
			tree->setCurrentCameraId(cameraId);
		app->getCurrentScene()->setDirtyFlag(true);
		xsheetHandle->notifyXsheetChanged();

		connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(updateField()));
	}
	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output frame values: start frame, end frame, step and
		shrink, to current field values.
*/

void OutputSettingsPopup::onStereoChecked(int)
{
	m_stereoShift->setEnabled(m_doStereoscopy->isChecked());
	if (m_dominantFieldOm)
		m_dominantFieldOm->setEnabled(!m_doStereoscopy->isChecked());
	onStereoChanged();
}

void OutputSettingsPopup::onStereoChanged()
{
	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	rs.m_stereoscopic = m_doStereoscopy->isChecked();
	rs.m_stereoscopicShift = m_stereoShift->getValue();
	prop->setRenderSettings(rs);
}

//----------------------------------------------

void OutputSettingsPopup::onFrameFldEditFinished()
{
	ToonzScene *scene = getCurrentScene();
	if (!scene)
		return;
	TOutputProperties *prop = getProperties();

	int maxR0 = tmax(0, scene->getFrameCount() - 1);
	int r0 = (int)m_startFld->getValue() - 1;
	int r1 = (int)m_endFld->getValue() - 1;
	int step = (int)m_stepFld->getValue();
	int shrink = (int)m_shrinkFld->getValue();

	bool error = false;
	if (r0 < 0) {
		error = true;
		r0 = 0;
	}
	if (r0 > maxR0) {
		error = true;
		r0 = maxR0;
	}
	if (r1 < r0) {
		error = true;
		r1 = r0;
	}
	if (r1 > maxR0) {
		error = true;
		r1 = maxR0;
	}
	if (error) {
		m_startFld->setValue(r0 + 1);
		m_endFld->setValue(r1 + 1);
	}
	if (step < 1) {
		step = 1;
		m_stepFld->setValue(step);
	}
	if (shrink < 1) {
		shrink = 1;
		m_shrinkFld->setValue(shrink);
	}
	if (r0 == 0 && r1 == maxR0) {
		r0 = 0;
		r1 = -1;
	}
	prop->setRange(r0, r1, step);
	TRenderSettings rs = prop->getRenderSettings();
	rs.m_shrinkX = rs.m_shrinkY = shrink;
	rs.m_applyShrinkToViewer = m_applyShrinkChk ? m_applyShrinkChk->checkState() == Qt::Checked : false;

	prop->setRenderSettings(rs);

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! Set current scene output resample balance to balance identified by \b type
		index in popup QComboBox.
*/
void OutputSettingsPopup::onResampleChanged(int type)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	if (type == c_improved)
		rs.m_quality = TRenderSettings::ImprovedResampleQuality;
	else if (type == c_high)
		rs.m_quality = TRenderSettings::HighResampleQuality;
	else if (type == c_standard)
		rs.m_quality = TRenderSettings::StandardResampleQuality;
	else if (type == c_triangle)
		rs.m_quality = TRenderSettings::Triangle_FilterResampleQuality;
	else if (type == c_mitchell)
		rs.m_quality = TRenderSettings::Mitchell_FilterResampleQuality;
	else if (type == c_cubic5)
		rs.m_quality = TRenderSettings::Cubic5_FilterResampleQuality;
	else if (type == c_cubic75)
		rs.m_quality = TRenderSettings::Cubic75_FilterResampleQuality;
	else if (type == c_cubic1)
		rs.m_quality = TRenderSettings::Cubic1_FilterResampleQuality;
	else if (type == c_hann2)
		rs.m_quality = TRenderSettings::Hann2_FilterResampleQuality;
	else if (type == c_hann3)
		rs.m_quality = TRenderSettings::Hann3_FilterResampleQuality;
	else if (type == c_hamming2)
		rs.m_quality = TRenderSettings::Hamming2_FilterResampleQuality;
	else if (type == c_hamming3)
		rs.m_quality = TRenderSettings::Hamming3_FilterResampleQuality;
	else if (type == c_lanczos2)
		rs.m_quality = TRenderSettings::Lanczos2_FilterResampleQuality;
	else if (type == c_lanczos3)
		rs.m_quality = TRenderSettings::Lanczos3_FilterResampleQuality;
	else if (type == c_gauss)
		rs.m_quality = TRenderSettings::Gauss_FilterResampleQuality;
	else if (type == c_closestPixel)
		rs.m_quality = TRenderSettings::ClosestPixel_FilterResampleQuality;
	else if (type == c_bilinear)
		rs.m_quality = TRenderSettings::Bilinear_FilterResampleQuality;
	else
		rs.m_quality = TRenderSettings::StandardResampleQuality;
	prop->setRenderSettings(rs);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output channel width to value identified by \b type
		index in popup QComboBox.
*/
void OutputSettingsPopup::onChannelWidthChanged(int type)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	if (type == c_8bit)
		rs.m_bpp = 32;
	else
		rs.m_bpp = 64;
	prop->setRenderSettings(rs);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output gamma value to current value in popup gamma field.
*/
void OutputSettingsPopup::onGammaFldEditFinished()
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();

	double gamma = m_gammaFld->getValue();
	m_gammaFld->setCursorPosition(0);
	if (gamma <= 0) {
		gamma = 0.001;
		m_gammaFld->setValue(gamma);
	}
	TRenderSettings rs = prop->getRenderSettings();
	rs.m_gamma = gamma;
	prop->setRenderSettings(rs);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! Set current scene output dominant field to value identified by \b type
		index in popup QComboBox. If time stretch velue from-to are different
		and dominant field is different from none show a message error.
*/
void OutputSettingsPopup::onDominantFieldChanged(int type)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	if (type != c_none && m_stretchFromFld->getValue() != m_stretchToFld->getValue()) {
		DVGui::error("Can't apply field rendering in a time stretched scene");
		rs.m_fieldPrevalence = TRenderSettings::NoField;
		m_dominantFieldOm->setCurrentIndex(c_none);
	} else if (type == c_odd)
		rs.m_fieldPrevalence = TRenderSettings::OddField;
	else if (type == c_even)
		rs.m_fieldPrevalence = TRenderSettings::EvenField;
	else
		rs.m_fieldPrevalence = TRenderSettings::NoField;
	m_doStereoscopy->setEnabled(rs.m_fieldPrevalence == TRenderSettings::NoField);
	m_stereoShift->setEnabled(rs.m_fieldPrevalence == TRenderSettings::NoField);
	prop->setRenderSettings(rs);
	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! If dominant field is different from none set time stretch field values to
		current scene output time stretch values; otherwise set current scene
		output time stretch values to current field values.
*/
void OutputSettingsPopup::onStretchFldEditFinished()
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	if (m_dominantFieldOm->currentIndex() != c_none) {
		DVGui::error("Can't stretch time in a field rendered scene\n");
		m_stretchFromFld->setValue(rs.m_timeStretchFrom);
		m_stretchToFld->setValue(rs.m_timeStretchTo);
	} else {
		rs.m_timeStretchFrom = m_stretchFromFld->getValue();
		rs.m_timeStretchTo = m_stretchToFld->getValue();
		prop->setRenderSettings(rs);
	}

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onMultimediaChanged(int state)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	prop->setMultimediaRendering(state);

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onThreadsComboChanged(int type)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	prop->setThreadIndex(type);

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onRasterGranularityChanged(int type)
{
	if (!getCurrentScene())
		return;
	TOutputProperties *prop = getProperties();
	prop->setMaxTileSizeIndex(type);

	TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! OutputSettingsのPreset登録
*/
void OutputSettingsPopup::onAddPresetButtonPressed()
{
	//*-- プリセット名を取得 --*/
	bool ok;
	QString qs = DVGui::getText(
		tr("Add preset"),
		tr("Enter the name for the output settings preset."),
		"", &ok);
	if (!ok || qs.isEmpty())
		return;

	if (!qs.endsWith(".txt"))
		qs.append(".txt");
	TFilePath fp = TEnv::getConfigDir() + "outputpresets";
	if (!TFileStatus(fp).doesExist())
		TSystem::mkDir(fp);
	fp = fp + qs.toStdString();

	/*-- すでに存在する場合、上書きを確認 --*/
	if (TFileStatus(fp).doesExist()) {
		int ret = QMessageBox::question(this, tr("Add output settings preset"),
										QString("The file %1 does already exist.\nDo you want to overwrite it?").arg(qs),
										QMessageBox::Save | QMessageBox::Cancel,
										QMessageBox::Save);
		if (ret == QMessageBox::Cancel)
			return;
	}
	TSystem::touchFile(fp);

	TOStream os(fp, false);
	if (!os.checkStatus())
		return;

	os.openChild("outputsettingspreset");

	/*-- カメラ設定 --*/
	os.openChild("camera");
	TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
	TStageObjectId cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
	TCamera *camera = xsheet->getStageObject(cameraId)->getCamera();
	camera->saveData(os);
	os.closeChild();

	/*--  Saveinのプロジェクトフォルダ部分 --*/
	QString saveInPath = m_saveInFileFld->getPath();
	if (saveInPath.startsWith("+")) {
		QString projectFolderName = (saveInPath.contains("\\")) ? saveInPath.left(saveInPath.indexOf("\\")) : saveInPath;
		os.child("savein_projectfolder") << projectFolderName.toStdString();
	}

	/*-- ファイル形式 --*/
	QString format = m_fileFormat->currentText();
	os.child("format") << format.toStdString();

	TOutputProperties *prop = getProperties();

	/*-- ファイルオプション --*/
	os.openChild("formatsProperties");
	std::vector<std::string> fileExtensions;
	prop->getFileFormatPropertiesExtensions(fileExtensions);
	for (int i = 0; i < (int)fileExtensions.size(); i++) {
		std::string ext = fileExtensions[i];
		TPropertyGroup *pg = prop->getFileFormatProperties(ext);
		assert(pg);
		std::map<std::string, std::string> attr;
		attr["ext"] = ext;
		os.openChild("formatProperties", attr);
		pg->saveData(os);
		os.closeChild();
	}
	os.closeChild();

	// Resample Balance
	QString resq = m_resampleBalanceOm->currentText();
	os.child("resquality") << resq.toStdString();

	// Channel Width
	QString chanw = m_channelWidthOm->currentText();
	os.child("bpp") << chanw.toStdString();

	//140503 iwasawa Frame Rate (Scene Settings)
	os.child("frameRate") << m_frameRateFld->text().toStdString();

	//140325 iwasawa stretch FPS
	os.child("stretchFrom") << m_stretchFromFld->text().toStdString();
	os.child("stretchTo") << m_stretchToFld->text().toStdString();
	//140325 iwasawa gamma
	os.child("gamma") << m_gammaFld->text().toStdString();

	os.closeChild(); //outputsettingspreset

	m_presetCombo->blockSignals(true);
	updatePresetComboItems();
	m_presetCombo->setCurrentIndex(m_presetCombo->findText(QString::fromStdString(fp.getName())));
	m_presetCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------
/*! プリセットフォルダを調べ、コンボボックスにアイテムを格納する
*/
void OutputSettingsPopup::updatePresetComboItems()
{
	m_presetCombo->clear();
	m_presetCombo->addItem(tr("<custom>"));
	TFilePath folder = TEnv::getConfigDir() + "outputpresets";

	TFileStatus fs(folder);
	if (!fs.doesExist())
		TSystem::mkDir(folder);

	TFilePathSet paths = TSystem::readDirectory(folder);
	for (TFilePathSet::iterator it = paths.begin(); it != paths.end(); it++) {
		std::cout << it->getName() << std::endl;
		m_presetCombo->addItem(QString::fromStdString(it->getName()));
	}
	m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! OutputSettingsのPreset削除
*/
void OutputSettingsPopup::onRemovePresetButtonPressed()
{
	int index = m_presetCombo->currentIndex();
	if (index <= 0)
		return;
	int ret = QMessageBox::question(this, tr("Remove preset"),
									QString("Deleting \"%1\".\nAre you sure?").arg(m_presetCombo->currentText()),
									QMessageBox::Ok | QMessageBox::Cancel,
									QMessageBox::Ok);
	if (ret == QMessageBox::Cancel)
		return;

	TFilePath fp = TEnv::getConfigDir() + "outputpresets" + QString("%1.txt").arg(m_presetCombo->currentText()).toStdString();
	if (TFileStatus(fp).doesExist())
		TSystem::deleteFile(fp);

	m_presetCombo->blockSignals(true);
	m_presetCombo->removeItem(index);
	m_presetCombo->setCurrentIndex(0);
	m_presetCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onPresetSelected(const QString &str)
{
	/*-- "<custom>"を選択したら何もせずreturn --*/
	if (m_presetCombo->currentIndex() == 0)
		return;
	TFilePath fp = TEnv::getConfigDir() + "outputpresets" + QString("%1.txt").arg(str).toStdString();
	if (!TFileStatus(fp).doesExist()) {
		QMessageBox::warning(this, tr("Warning"),
							 QString("The preset file %1.txt not found").arg(str),
							 QMessageBox::Ok,
							 QMessageBox::Ok);
		return;
	}
	TIStream is(fp);
	if (!is) {
		QMessageBox::warning(this, tr("Warning"),
							 QString("Failed to open the preset file %1.txt").arg(str),
							 QMessageBox::Ok,
							 QMessageBox::Ok);
		return;
	}

	std::string tagName = "";
	if (!is.matchTag(tagName) ||
		tagName != "outputsettingspreset") {
		QMessageBox::warning(this, tr("Warning"),
							 QString("Bad file format: %1.txt").arg(str),
							 QMessageBox::Ok,
							 QMessageBox::Ok);
		return;
	}

	TOutputProperties *prop = getProperties();
	TRenderSettings rs = prop->getRenderSettings();
	while (is.matchTag(tagName)) {
		//Camera
		if (tagName == "camera") {
			TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
			TStageObjectId cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
			TCamera *camera = xsheet->getStageObject(cameraId)->getCamera();
			camera->loadData(is);
		} // if "camera"
		/*-- Saveinのプロジェクトフォルダ部分 --*/
		else if (tagName == "savein_projectfolder") {
			std::string projectFolderName;
			is >> projectFolderName;

			QString newProjectFolder = QString::fromStdString(projectFolderName);
			QString fp = QString::fromStdWString(prop->getPath().getWideString());
			/*-- 出力パスがプロジェクトフォルダで始まっている場合は、差し替える --*/
			if (!newProjectFolder.isEmpty() &&
				fp.startsWith("+")) {
				if (fp.indexOf("\\") >= 0)
					fp = QString("%1\\%2").arg(newProjectFolder).arg(fp.right(fp.size() - fp.indexOf("\\") - 1));
				else
					fp = newProjectFolder;
				prop->setPath(TFilePath(fp.toStdWString()));
			}
		}
		//File format
		else if (tagName == "format") {
			std::string formatName;
			is >> formatName;
			TFilePath fp = prop->getPath().withType(formatName);
			prop->setPath(fp);
		}
		//File options
		else if (tagName == "formatsProperties") {
			while (is.matchTag(tagName)) {
				if (tagName == "formatProperties") {
					std::string ext = is.getTagAttribute("ext");
					TPropertyGroup *pg =
						prop->getFileFormatProperties(ext);
					if (ext == "avi") {
						TPropertyGroup appProperties;
						appProperties.loadData(is);
						if (pg->getPropertyCount() != 1 || appProperties.getPropertyCount() != 1) {
							is.closeChild();
							continue;
						}
						TEnumProperty *enumProp = dynamic_cast<TEnumProperty *>(pg->getProperty(0));
						TEnumProperty *enumAppProp = dynamic_cast<TEnumProperty *>(appProperties.getProperty(0));
						assert(enumAppProp && enumProp);
						if (enumAppProp && enumProp) {
							try {
								enumProp->setValue(enumAppProp->getValue());
							} catch (TProperty::RangeError &) {
							}
						} else
							throw TException();
					} else
						pg->loadData(is);
					is.closeChild();
				} else
					throw TException("unexpected tag: " + tagName);
			} // end while
		}
		// Resample Balance
		else if (tagName == "resquality") {
			std::string resq;
			is >> resq;
			int index = m_resampleBalanceOm->findText(QString::fromStdString(resq));
			if (index >= 0)
				m_resampleBalanceOm->setCurrentIndex(index);
		}
		// Channel Width
		else if (tagName == "bpp") {
			std::string chanw;
			is >> chanw;
			int index = m_channelWidthOm->findText(QString::fromStdString(chanw));
			if (index >= 0)
				m_channelWidthOm->setCurrentIndex(index);
		}

		//Frame Rate (Scene Settings)
		else if (tagName == "frameRate") {
			std::string strFps;
			is >> strFps;
			prop->setFrameRate(QString::fromStdString(strFps).toDouble());
		}

		// stretch FPS
		else if (tagName == "stretchFrom") {
			std::string strFrm;
			is >> strFrm;
			rs.m_timeStretchFrom = QString::fromStdString(strFrm).toDouble();
		} else if (tagName == "stretchTo") {
			std::string strTo;
			is >> strTo;
			rs.m_timeStretchTo = QString::fromStdString(strTo).toDouble();
		}
		// gamma
		else if (tagName == "gamma") {
			std::string gamma;
			is >> gamma;
			rs.m_gamma = QString::fromStdString(gamma).toDouble();
		}

		is.closeChild();
	}

	/*-- renderSettingsを戻す --*/
	prop->setRenderSettings(rs);

	TApp::instance()->getCurrentScene()->notifySceneChanged();

	m_presetCombo->blockSignals(true);
	m_presetCombo->setCurrentIndex(m_presetCombo->findText(str));
	m_presetCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onCameraSettingsChanged()
{
	if (m_presetCombo)
		m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onFrameRateEditingFinished()
{
	double frameRate = getProperties()->getFrameRate();
	double frameRateFldValue = m_frameRateFld->getValue();
	if (frameRate == frameRateFldValue)
		return;
	getProperties()->setFrameRate(frameRateFldValue);
	TApp::instance()->getCurrentScene()->getScene()->updateSoundColumnFrameRate();
	TApp::instance()->getCurrentScene()->notifySceneChanged();
	TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
	TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<OutputSettingsPopup> openOutputSettingsPopup(MI_OutputSettings);
OpenPopupCommandHandler<PreviewSettingsPopup> openPreviewSettingsPopup(MI_PreviewSettings);
