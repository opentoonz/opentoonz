

#include "outputsettingspopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "tapp.h"
#include "camerasettingspopup.h"
#include "pane.h"
#include "boardsettingspopup.h"

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
#include "toonz/boardsettings.h"

// TnzBase includes
#include "trasterfx.h"

// TnzCore includes
#include "tiio.h"
#include "tlevel_io.h"
#include "tenv.h"
#include "tsystem.h"
#include "tstream.h"
#include "tfiletype.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMainWindow>
#include <QFrame>
#include <QMessageBox>
#include <QMap>
#include <QListWidget>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QSpacerItem>
#include <QEvent>
//-----------------------------------------------------------------------------
namespace {

enum ResampleOption {
  c_standard = 0,
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
  c_bilinear,
  ResampleOptionCount
};

struct resampleInfo {
  QString idString;
  TRenderSettings::ResampleQuality quality;
  QString uiString = "";
  resampleInfo(QString _idStr = "",
               TRenderSettings::ResampleQuality _quality =
                   TRenderSettings::StandardResampleQuality)
      : idString(_idStr), quality(_quality), uiString("") {}
};

QMap<ResampleOption, resampleInfo> resampleInfoMap = {
    {c_standard,
     resampleInfo("Standard", TRenderSettings::StandardResampleQuality)},
    {c_improved,
     resampleInfo("Improved", TRenderSettings::ImprovedResampleQuality)},
    {c_high, resampleInfo("High", TRenderSettings::HighResampleQuality)},
    {c_triangle, resampleInfo("Triangle filter",
                              TRenderSettings::Triangle_FilterResampleQuality)},
    {c_mitchell, resampleInfo("Mitchell-Netravali filter",
                              TRenderSettings::Mitchell_FilterResampleQuality)},
    {c_cubic5, resampleInfo("Cubic convolution, a = .5",
                            TRenderSettings::Cubic5_FilterResampleQuality)},
    {c_cubic75, resampleInfo("Cubic convolution, a = .75",
                             TRenderSettings::Cubic75_FilterResampleQuality)},
    {c_cubic1, resampleInfo("Cubic convolution, a = 1",
                            TRenderSettings::Cubic1_FilterResampleQuality)},
    {c_hann2, resampleInfo("Hann window, rad = 2",
                           TRenderSettings::Hann2_FilterResampleQuality)},
    {c_hann3, resampleInfo("Hann window, rad = 3",
                           TRenderSettings::Hann3_FilterResampleQuality)},
    {c_hamming2, resampleInfo("Hamming window, rad = 2",
                              TRenderSettings::Hamming2_FilterResampleQuality)},
    {c_hamming3, resampleInfo("Hamming window, rad = 3",
                              TRenderSettings::Hamming3_FilterResampleQuality)},
    {c_lanczos2, resampleInfo("Lanczos window, rad = 2",
                              TRenderSettings::Lanczos2_FilterResampleQuality)},
    {c_lanczos3, resampleInfo("Lanczos window, rad = 3",
                              TRenderSettings::Lanczos3_FilterResampleQuality)},
    {c_gauss, resampleInfo("Gaussian convolution",
                           TRenderSettings::Gauss_FilterResampleQuality)},
    {c_closestPixel,
     resampleInfo("Closest Pixel (Nearest Neighbor)",
                  TRenderSettings::ClosestPixel_FilterResampleQuality)},
    {c_bilinear,
     resampleInfo("Bilinear",
                  TRenderSettings::Bilinear_FilterResampleQuality)}};

ResampleOption quality2index(TRenderSettings::ResampleQuality quality) {
  QMapIterator<ResampleOption, resampleInfo> i(resampleInfoMap);
  while (i.hasNext()) {
    i.next();
    if (i.value().quality == quality) return i.key();
  }
  return c_standard;
}

enum ChannelWidth { c_8bit, c_16bit, c_32bit };  // 32bit: compute in float

enum DominantField { c_odd, c_even, c_none };

enum ThreadsOption {
  c_single,
  c_half,
  c_all,
};

enum GranularityOption { c_off, c_large, c_medium, c_small };

bool checkForSeqNum(QString type) {
  TFileType::Type typeInfo = TFileType::getInfoFromExtension(type);
  if ((typeInfo & TFileType::IMAGE) && !(typeInfo & TFileType::LEVEL))
    return true;
  else
    return false;
}
}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class OutputSettingsPopup
                \brief The OutputSettingsPopup class provides a output settings
   dialog.

                Inherits \b Dialog.
                This class allows to set output properties of current scene.
                It's composed of a list of field to set this preperties:
                a \b DVGui::FileField to set output directory, a LineEdit for
   output name,
                a \b QComboBox to choose format, a button near QComboBox that
   open a popup
                to change format chosen settings, other QComboBox to choose
   camera,
                resample balance, channel width and dominant field and other
   number field
                to set values for frame and time stretch.
*/
OutputSettingsPopup::OutputSettingsPopup(bool isPreview)
    : Dialog(TApp::instance()->getMainWindow(), false, isPreview,
             isPreview ? "PreviewSettings" : "OutputSettings")
    , m_subcameraChk(nullptr)
    , m_applyShrinkChk(nullptr)
    , m_outputCameraOm(nullptr)
    , m_isPreviewSettings(isPreview)
    , m_allowMT(Preferences::instance()->getFfmpegMultiThread())
    , m_presetCombo(nullptr)
    , m_syncColorSettingsButton(nullptr) {
  setWindowTitle(isPreview ? tr("Preview Settings") : tr("Output Settings"));
  if (!isPreview) setObjectName("OutputSettingsPopup");
  // create panel
  QFrame *panel = createPanel(isPreview);

  // preview settings
  if (isPreview) {
    m_topLayout->setMargin(5);
    m_topLayout->addWidget(panel, 0);
    return;
  }

  // outputsettings with category list & scrollable panel
  QPushButton *addPresetButton    = new QPushButton(tr("Add"), this);
  QPushButton *removePresetButton = new QPushButton(tr("Remove"), this);
  m_presetCombo                   = new QComboBox(this);

  addPresetButton->setObjectName("PushButton_NoPadding");
  removePresetButton->setObjectName("PushButton_NoPadding");
  QString tooltip =
      tr("Save current output settings.\nThe parameters to be saved are:\n- "
         "Camera settings\n- Project folder to be saved in\n- File format\n- "
         "File options\n- Resample Balance\n- Channel width\n- Linear Color "
         "Space\n- Color Space Gamma");
  addPresetButton->setToolTip(tooltip);
  /*-- プリセットフォルダを調べ、コンボボックスにアイテムを格納する --*/
  updatePresetComboItems();

  QListWidget *categoryList = new QListWidget(this);
  QStringList categories;
  categories << tr("Camera") << tr("Color") << tr("File") << tr("More");
  categoryList->addItems(categories);
  categoryList->setFixedWidth(100);
  categoryList->setCurrentRow(0);

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setWidget(panel);

  QPushButton *renderButton = new QPushButton(tr("Render"), this);
  renderButton->setIcon(createQIcon("render"));
  renderButton->setIconSize(QSize(20, 20));
  renderButton->setFixedWidth(200);

  m_topLayout->setMargin(5);
  {
    QHBoxLayout *presetLay = new QHBoxLayout();
    presetLay->setMargin(0);
    presetLay->setSpacing(5);
    {
      presetLay->addStretch(1);
      presetLay->addWidget(new QLabel(tr("Presets:"), this), 1,
                           Qt::AlignRight | Qt::AlignVCenter);
      presetLay->addWidget(m_presetCombo, 1);
      presetLay->addWidget(addPresetButton, 0);
      presetLay->addWidget(removePresetButton, 0);
    }
    m_topLayout->addLayout(presetLay, 0);

    QHBoxLayout *middleLay = new QHBoxLayout();
    middleLay->setMargin(0);
    middleLay->setSpacing(5);
    {
      middleLay->addWidget(categoryList, 0);
      middleLay->addWidget(m_scrollArea, 1);
    }
    m_topLayout->addLayout(middleLay, 1);

    m_topLayout->addSpacing(5);
    m_topLayout->addWidget(renderButton, 0, Qt::AlignCenter);
  }

  bool ret = true;
  ret      = ret &&
        connect(renderButton, SIGNAL(pressed()), this, SLOT(onRenderClicked()));
  ret = ret && connect(addPresetButton, SIGNAL(pressed()), this,
                       SLOT(onAddPresetButtonPressed()));
  ret = ret && connect(removePresetButton, SIGNAL(pressed()), this,
                       SLOT(onRemovePresetButtonPressed()));
  ret = ret && connect(m_presetCombo, SIGNAL(activated(const QString &)), this,
                       SLOT(onPresetSelected(const QString &)));
  ret = ret && connect(categoryList, SIGNAL(itemClicked(QListWidgetItem *)),
                       this, SLOT(onCategoryActivated(QListWidgetItem *)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onCategoryActivated(QListWidgetItem *item) {
  AnimatedLabel *label;
  QFrame *frame;
  if (item->text() == tr("Camera")) {
    label = m_cameraLabel;
    frame = m_cameraBox;
  } else if (item->text() == tr("Color")) {
    label = m_colorLabel;
    frame = m_colorBox;
  } else if (item->text() == tr("File")) {
    label = m_fileLabel;
    frame = m_fileBox;
  } else if (item->text() == tr("More")) {
    label = m_moreLabel;
    frame = m_moreBox;
  } else
    return;

  m_scrollArea->ensureWidgetVisible(frame);
  QPropertyAnimation *a = new QPropertyAnimation(label, "color");
  a->setDuration(1000);
  a->setStartValue(QColor(255, 255, 0, 128));
  a->setEndValue(QColor(255, 255, 0, 0));
  a->setEasingCurve(QEasingCurve::OutBack);
  a->start(QPropertyAnimation::DeleteWhenStopped);
}

//-----------------------------------------------------------------------------

QFrame *OutputSettingsPopup::createPanel(bool isPreview) {
  QFrame *panel = new QFrame(this);

  m_cameraLabel = new AnimatedLabel(tr("Camera Settings"), this);
  m_colorLabel  = new AnimatedLabel(tr("Color Settings"), this);
  if (isPreview)
    m_syncColorSettingsButton =
        new DVGui::CheckBox(tr("Sync with Output Settings"));
  m_fileLabel = new AnimatedLabel(tr("File Settings"), this);
  m_cameraBox = createCameraSettingsBox(isPreview);
  m_colorBox  = createColorSettingsBox(isPreview);
  m_fileBox   = createFileSettingsBox(isPreview);
  if (!isPreview) {
    m_moreLabel = new AnimatedLabel(tr("More Settings"), this);
    m_moreBox   = createMoreSettingsBox();
  }
  QVBoxLayout *lay = new QVBoxLayout();
  lay->setMargin(5);
  lay->setSpacing(3);
  {
    lay->addWidget(m_cameraLabel, 0);
    lay->addWidget(m_cameraBox, 0);
    lay->addSpacing(10);

    QHBoxLayout *colorLabelLay = new QHBoxLayout();
    colorLabelLay->setMargin(0);
    colorLabelLay->setSpacing(0);
    {
      colorLabelLay->addWidget(m_colorLabel, 0);
      colorLabelLay->addStretch(1);
      if (isPreview && m_syncColorSettingsButton)
        colorLabelLay->addWidget(m_syncColorSettingsButton, 0);
    }
    lay->addLayout(colorLabelLay, 0);
    lay->addWidget(m_colorBox, 0);
    lay->addSpacing(10);

    lay->addWidget(m_fileLabel, 0);
    lay->addWidget(m_fileBox, 0);

    if (!isPreview) {
      lay->addSpacing(10);
      lay->addWidget(m_moreLabel, 0);
      lay->addWidget(m_moreBox, 0);
    }
    lay->addStretch(1);
  }
  panel->setLayout(lay);

  if (isPreview && m_syncColorSettingsButton) {
    bool ret = connect(m_syncColorSettingsButton, SIGNAL(stateChanged(int)),
                       SLOT(onSyncColorSettingsChecked(int)));
    assert(ret);
  }

  return panel;
}

//-----------------------------------------------------------------------------

QFrame *OutputSettingsPopup::createCameraSettingsBox(bool isPreview) {
  QFrame *cameraSettingsBox = new QFrame(this);
  cameraSettingsBox->setObjectName("OutputSettingsBox");

  // Output Camera
  m_outputCameraOm = new QComboBox();
  // Frame Start-End
  m_startFld = new DVGui::IntLineEdit(this);
  m_endFld   = new DVGui::IntLineEdit(this);
  // Step-Shrink
  m_stepFld   = new DVGui::IntLineEdit(this);
  m_shrinkFld = new DVGui::IntLineEdit(this);

  QFrame *cameraParametersBox = nullptr;
  if (!isPreview) {
    m_cameraSettings    = new CameraSettingsPopup();
    cameraParametersBox = new QFrame(this);
    cameraParametersBox->setObjectName("OutputSettingsCameraBox");
    m_outputCameraOm->setFocusPolicy(Qt::StrongFocus);
    m_outputCameraOm->installEventFilter(this);
  } else {
    // Subcamera checkbox
    m_subcameraChk = new DVGui::CheckBox(tr("Use Sub-Camera"));
    m_applyShrinkChk =
        new DVGui::CheckBox(tr("Apply Shrink to Main Viewer"), this);
  }
  m_outputCameraOm->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  //-----

  QVBoxLayout *lay = new QVBoxLayout();
  lay->setMargin(10);
  lay->setSpacing(10);
  {
    // Output Camera
    QHBoxLayout *outCamLay = new QHBoxLayout();
    outCamLay->setMargin(0);
    outCamLay->setSpacing(5);
    {
      outCamLay->addWidget(new QLabel(tr("Output Camera:"), this), 0);
      outCamLay->addWidget(m_outputCameraOm, 0);
      outCamLay->addStretch(1);
    }
    lay->addLayout(outCamLay);

    if (!isPreview) {
      QVBoxLayout *camParamLay = new QVBoxLayout();
      camParamLay->setMargin(5);
      camParamLay->setSpacing(0);
      { camParamLay->addWidget(m_cameraSettings); }
      cameraParametersBox->setLayout(camParamLay);

      lay->addWidget(cameraParametersBox);
    }

    // Frame start/end
    QGridLayout *frameShrinkLay = new QGridLayout();
    frameShrinkLay->setMargin(0);
    frameShrinkLay->setHorizontalSpacing(5);
    frameShrinkLay->setVerticalSpacing(10);
    {
      frameShrinkLay->addWidget(new QLabel(tr("Frame Start:"), this), 0, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameShrinkLay->addWidget(m_startFld, 0, 1);
      frameShrinkLay->addItem(
          new QSpacerItem(3, 1, QSizePolicy::Fixed, QSizePolicy::Fixed), 0, 2);
      frameShrinkLay->addWidget(new QLabel(tr("End:"), this), 0, 3,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameShrinkLay->addWidget(m_endFld, 0, 4);
      frameShrinkLay->addItem(
          new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Fixed), 0, 5);
      frameShrinkLay->addWidget(new QLabel(tr("Step:"), this), 0, 6,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameShrinkLay->addWidget(m_stepFld, 0, 7);

      frameShrinkLay->addWidget(new QLabel(tr("Shrink:"), this), 1, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      frameShrinkLay->addWidget(m_shrinkFld, 1, 1);

      if (isPreview)
        frameShrinkLay->addWidget(m_applyShrinkChk, 1, 4, 1, 5,
                                  Qt::AlignRight | Qt::AlignVCenter);
    }
    frameShrinkLay->setColumnStretch(8, 1);

    lay->addLayout(frameShrinkLay);
  }
  cameraSettingsBox->setLayout(lay);

  //-----

  bool ret = true;
  ret      = ret &&
        connect(m_outputCameraOm, SIGNAL(currentIndexChanged(const QString &)),
                SLOT(onCameraChanged(const QString &)));
  ret = ret && connect(m_startFld, SIGNAL(editingFinished()),
                       SLOT(onFrameFldEditFinished()));
  ret = ret && connect(m_endFld, SIGNAL(editingFinished()),
                       SLOT(onFrameFldEditFinished()));
  ret = ret && connect(m_stepFld, SIGNAL(editingFinished()),
                       SLOT(onFrameFldEditFinished()));
  ret = ret && connect(m_shrinkFld, SIGNAL(editingFinished()),
                       SLOT(onFrameFldEditFinished()));
  if (!isPreview) {
    ret = ret && connect(m_cameraSettings, SIGNAL(changed()), this,
                         SLOT(onCameraSettingsChanged()));
  } else {
    ret = ret && connect(m_applyShrinkChk, SIGNAL(stateChanged(int)),
                         SLOT(onApplyShrinkChecked(int)));
  }

  assert(ret);
  return cameraSettingsBox;
}

//-----------------------------------------------------------------------------

QFrame *OutputSettingsPopup::createColorSettingsBox(bool isPreview) {
  QFrame *colorSettingsBox = new QFrame(this);
  colorSettingsBox->setObjectName("OutputSettingsBox");

  // Channel Width
  m_channelWidthOm = new QComboBox();
  // Linear Color Space
  m_linearColorSpaceChk = new DVGui::CheckBox();
  // color space gamma
  m_colorSpaceGammaFld = new DVGui::DoubleLineEdit(this, 2.2);

  // Channel Width
  m_channelWidthOm->addItem(tr("8 bit"), "8 bit");
  m_channelWidthOm->addItem(tr("16 bit"), "16 bit");
  m_channelWidthOm->addItem(tr("32 bit Floating point"), "32 bit");

  m_linearColorSpaceChk->setToolTip(
      tr("On rendering, color values will be temporarily converted to linear "
         "light from nonlinear RGB values by using color space gamma."));
  if (m_isPreviewSettings)
    m_colorSpaceGammaFld->setRange(-1.0, 5.0);
  else
    m_colorSpaceGammaFld->setRange(1.0, 5.0);
  m_colorSpaceGammaFld->setDecimals(3);
  QString colorSpaceGammaTooltip =
      tr("Color Space Gamma value is used for conversion between the linear "
         "and nonlinear color spaces,\n"
         "when the \"Linear Color Space\" option is enabled.");
  if (m_isPreviewSettings)
    colorSpaceGammaTooltip +=
        tr("\nInput less than 1.0 to sync the value with the output settings.");
  m_colorSpaceGammaFld->setToolTip(colorSpaceGammaTooltip);

  if (!isPreview) {
    m_channelWidthOm->setFocusPolicy(Qt::StrongFocus);
  }

  QGridLayout *gridLay = new QGridLayout();
  gridLay->setMargin(5);
  gridLay->setHorizontalSpacing(5);
  gridLay->setVerticalSpacing(10);
  {
    // Channel Width
    gridLay->addWidget(new QLabel(tr("Channel Width:"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_channelWidthOm, 0, 1, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);

    // Linear Color Space and Color SpaceGamma
    gridLay->addWidget(new QLabel(tr("Linear Color Space:"), this), 1, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_linearColorSpaceChk, 1, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
    gridLay->addWidget(new QLabel(tr("Color Space Gamma:"), this), 1, 2,
                       Qt::AlignRight | Qt::AlignVCenter);
    gridLay->addWidget(m_colorSpaceGammaFld, 1, 3);
  }
  gridLay->setColumnStretch(2, 1);
  colorSettingsBox->setLayout(gridLay);

  bool ret = true;
  ret      = ret && connect(m_channelWidthOm, SIGNAL(currentIndexChanged(int)),
                            SLOT(onChannelWidthChanged(int)));
  ret      = ret && connect(m_linearColorSpaceChk, SIGNAL(stateChanged(int)),
                            SLOT(onLinearColorSpaceChecked(int)));

  ret = ret && connect(m_colorSpaceGammaFld, SIGNAL(editingFinished()),
                       SLOT(onColorSpaceGammaEdited()));
  assert(ret);
  return colorSettingsBox;
}

//-----------------------------------------------------------------------------

QFrame *OutputSettingsPopup::createFileSettingsBox(bool isPreview) {
  QFrame *fileSettingsBox = new QFrame(this);
  fileSettingsBox->setObjectName("OutputSettingsBox");

  // Resample Balance
  m_resampleBalanceOm = new QComboBox();
  // Threads
  m_threadsComboOm = new QComboBox();
  // Granularity
  m_rasterGranularityOm = new QComboBox();

  // Resample Balance
  translateResampleOptions();
  for (int i = 0; i < ResampleOptionCount; i++) {
    m_resampleBalanceOm->addItem(resampleInfoMap[(ResampleOption)i].uiString,
                                 resampleInfoMap[(ResampleOption)i].idString);
  }

  QStringList threadsChoices;
  threadsChoices << tr("Single") << tr("Half") << tr("All");
  m_threadsComboOm->addItems(threadsChoices);
  QStringList granularityChoices;
  granularityChoices << tr("None") << tr("Large") << tr("Medium")
                     << tr("Small");
  m_rasterGranularityOm->addItems(granularityChoices);

  if (!isPreview) {
    // Save In
    m_saveInFileFld = new DVGui::FileField(0, QString(""));
    // File Name
    m_fileNameFld = new DVGui::LineEdit(QString(""));
    // File Format
    m_fileFormat       = new QComboBox();
    m_fileFormatButton = new QPushButton(tr("Options"));

    // File Format
    QStringList formats;
    TImageWriter::getSupportedFormats(formats, true);
    TLevelWriter::getSupportedFormats(formats, true);
    Tiio::Writer::getSupportedFormats(formats, true);
    formats.sort();
    m_fileFormat->addItems(formats);
    m_fileFormat->setFocusPolicy(Qt::StrongFocus);
    m_resampleBalanceOm->setFocusPolicy(Qt::StrongFocus);
    m_threadsComboOm->setFocusPolicy(Qt::StrongFocus);
    m_rasterGranularityOm->setFocusPolicy(Qt::StrongFocus);
    m_fileFormat->installEventFilter(this);
    m_resampleBalanceOm->installEventFilter(this);
    m_channelWidthOm->installEventFilter(this);
    m_threadsComboOm->installEventFilter(this);
    m_rasterGranularityOm->installEventFilter(this);
  }

  //-----

  QVBoxLayout *lay = new QVBoxLayout();
  lay->setMargin(10);
  lay->setSpacing(10);
  {
    if (!isPreview) {
      QGridLayout *upperGridLay = new QGridLayout();
      upperGridLay->setMargin(0);
      upperGridLay->setHorizontalSpacing(5);
      upperGridLay->setVerticalSpacing(10);
      {
        // Save In
        upperGridLay->addWidget(new QLabel(tr("Save in:"), this), 0, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
        upperGridLay->addWidget(m_saveInFileFld, 0, 1, 1, 3);
        // File Name
        upperGridLay->addWidget(new QLabel(tr("Name:"), this), 1, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
        upperGridLay->addWidget(m_fileNameFld, 1, 1);
        // File Format
        upperGridLay->addWidget(m_fileFormat, 1, 2);
        upperGridLay->addWidget(m_fileFormatButton, 1, 3);
      }
      upperGridLay->setColumnStretch(1, 1);

      lay->addLayout(upperGridLay);
    }

    QGridLayout *bottomGridLay = new QGridLayout();
    bottomGridLay->setMargin(0);
    bottomGridLay->setHorizontalSpacing(5);
    bottomGridLay->setVerticalSpacing(10);
    {
      // Resample Balance
      bottomGridLay->addWidget(new QLabel(tr("Resample Balance:"), this), 0, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      bottomGridLay->addWidget(m_resampleBalanceOm, 0, 1, 1, 2,
                               Qt::AlignLeft | Qt::AlignVCenter);

      // Threads
      bottomGridLay->addWidget(new QLabel(tr("Dedicated CPUs:"), this), 1, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      bottomGridLay->addWidget(m_threadsComboOm, 1, 1);
      // Granularity
      bottomGridLay->addWidget(new QLabel(tr("Render Tile:"), this), 2, 0,
                               Qt::AlignRight | Qt::AlignVCenter);
      bottomGridLay->addWidget(m_rasterGranularityOm, 2, 1);
      if (m_subcameraChk) {
        bottomGridLay->addWidget(m_subcameraChk, 3, 1, 1, 2);
      }
    }
    bottomGridLay->setColumnStretch(2, 1);

    lay->addLayout(bottomGridLay);
  }
  fileSettingsBox->setLayout(lay);

  //-----

  bool ret = true;
  if (!isPreview) {
    ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                         SLOT(onPathChanged()));
    ret = ret && connect(m_fileNameFld, SIGNAL(editingFinished()),
                         SLOT(onNameChanged()));
    ret = ret &&
          connect(m_fileFormat, SIGNAL(currentIndexChanged(const QString &)),
                  SLOT(onFormatChanged(const QString &)));
    ret = ret && connect(m_fileFormatButton, SIGNAL(pressed()), this,
                         SLOT(openSettingsPopup()));
  }
  ret = ret && connect(m_resampleBalanceOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onResampleChanged(int)));

  ret = ret && connect(m_threadsComboOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onThreadsComboChanged(int)));
  ret = ret && connect(m_rasterGranularityOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onRasterGranularityChanged(int)));

  if (m_subcameraChk)
    ret = ret && connect(m_subcameraChk, SIGNAL(stateChanged(int)),
                         SLOT(onSubcameraChecked(int)));
  assert(ret);
  return fileSettingsBox;
}

//-----------------------------------------------------------------------------

QFrame *OutputSettingsPopup::createMoreSettingsBox() {
  QFrame *moreSettingsBox = new QFrame(this);
  moreSettingsBox->setObjectName("OutputSettingsBox");

  // Board
  m_addBoard         = new DVGui::CheckBox(tr("Add Clapperboard"), this);
  m_boardSettingsBtn = new QPushButton(tr("Edit Clapperboard..."), this);
  // Gamma
  m_gammaFld = new DVGui::DoubleLineEdit();
  // Dominant Field
  m_dominantFieldOm = new QComboBox();

  // Stretch
  TRenderSettings rs = getProperties()->getRenderSettings();

  // Scene Settings FPS
  double frameRate = getProperties()->getFrameRate();
  m_frameRateFld   = new DVGui::DoubleLineEdit(this, frameRate);
  m_stretchFromFld = new DVGui::DoubleLineEdit(this, rs.m_timeStretchFrom);
  m_stretchToFld   = new DVGui::DoubleLineEdit(this, rs.m_timeStretchTo);
  m_multimediaOm   = new QComboBox(this);
  m_doStereoscopy  = new DVGui::CheckBox(tr("Do stereoscopy"), this);
  m_stereoShift    = new DVGui::DoubleLineEdit(this, 0.05);

  // Dominant Field
  QStringList dominantField;
  dominantField << tr("Odd (NTSC)") << tr("Even (PAL)") << tr("None");
  m_dominantFieldOm->addItems(dominantField);
  m_dominantFieldOm->setFocusPolicy(Qt::StrongFocus);
  m_dominantFieldOm->installEventFilter(this);
  m_stretchFromFld->setRange(1, 1000);
  m_stretchToFld->setRange(1, 1000);
  m_stretchFromFld->setDecimals(2);
  m_stretchToFld->setDecimals(2);
  // Multimedia rendering enum
  QStringList multimediaTypes;
  multimediaTypes << tr("None") << tr("Fx Schematic Flows")
                  << tr("Fx Schematic Terminal Nodes");
  m_multimediaOm->addItems(multimediaTypes);
  m_multimediaOm->setFocusPolicy(Qt::StrongFocus);
  m_multimediaOm->installEventFilter(this);
  m_stereoShift->setEnabled(false);

  //-----

  QGridLayout *lay = new QGridLayout();
  lay->setMargin(5);
  lay->setHorizontalSpacing(5);
  lay->setVerticalSpacing(10);
  {
    // clapperboard
    lay->addWidget(m_addBoard, 0, 0);
    lay->addWidget(m_boardSettingsBtn, 0, 1, 1, 3,
                   Qt::AlignLeft | Qt::AlignVCenter);

    // Gamma
    lay->addWidget(new QLabel(tr("Gamma:"), this), 1, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_gammaFld, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    // Dominant Field
    lay->addWidget(new QLabel(tr("Dominant Field:"), this), 2, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_dominantFieldOm, 2, 1, 1, 2,
                   Qt::AlignLeft | Qt::AlignVCenter);
    // Scene Settings' FPS
    lay->addWidget(new QLabel(tr("Frame Rate:"), this), 3, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_frameRateFld, 3, 1, Qt::AlignLeft | Qt::AlignVCenter);
    lay->addWidget(new QLabel(tr("(linked to Scene Settings)"), this), 3, 2, 1,
                   2, Qt::AlignLeft | Qt::AlignVCenter);
    // Stretch
    lay->addWidget(new QLabel(tr("Stretch from FPS:"), this), 4, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_stretchFromFld, 4, 1, Qt::AlignLeft | Qt::AlignVCenter);
    lay->addWidget(new QLabel(tr("  To:"), this), 4, 2,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_stretchToFld, 4, 3, Qt::AlignLeft | Qt::AlignVCenter);
    // new in V6.1
    // Multimedia rendering enum
    lay->addWidget(new QLabel(tr("Multiple Rendering:"), this), 5, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_multimediaOm, 5, 1, 1, 3,
                   Qt::AlignLeft | Qt::AlignVCenter);

    lay->addWidget(m_doStereoscopy, 6, 0);
    lay->addWidget(new QLabel(tr("Camera Shift:")), 6, 1, 1, 2,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_stereoShift, 6, 3, Qt::AlignLeft | Qt::AlignVCenter);
  }
  lay->setColumnStretch(4, 1);

  moreSettingsBox->setLayout(lay);

  //-----

  bool ret = true;
  // clapperboard
  ret = ret && connect(m_addBoard, SIGNAL(stateChanged(int)), this,
                       SLOT(onAddBoardChecked(int)));
  ret = ret && connect(m_boardSettingsBtn, SIGNAL(clicked()), this,
                       SLOT(onBoardSettingsBtnClicked()));

  ret = ret && connect(m_gammaFld, SIGNAL(editingFinished()),
                       SLOT(onGammaFldEditFinished()));
  ret = ret && connect(m_dominantFieldOm, SIGNAL(currentIndexChanged(int)),
                       SLOT(onDominantFieldChanged(int)));
  ret = ret && connect(m_frameRateFld, SIGNAL(editingFinished()), this,
                       SLOT(onFrameRateEditingFinished()));
  ret = ret && connect(m_stretchFromFld, SIGNAL(editingFinished()),
                       SLOT(onStretchFldEditFinished()));
  ret = ret && connect(m_stretchToFld, SIGNAL(editingFinished()),
                       SLOT(onStretchFldEditFinished()));
  ret = ret && connect(m_multimediaOm, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onMultimediaChanged(int)));
  ret = ret && connect(m_doStereoscopy, SIGNAL(stateChanged(int)),
                       SLOT(onStereoChecked(int)));
  ret = ret && connect(m_stereoShift, SIGNAL(editingFinished()),
                       SLOT(onStereoChanged()));
  assert(ret);
  return moreSettingsBox;
}

//-----------------------------------------------------------------------------
/*! Return current \b ToonzScene scene.
 */
ToonzScene *OutputSettingsPopup::getCurrentScene() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return scene;
}

//-----------------------------------------------------------------------------
/*!	Return current scene output properties \b TOutputProperties, if
                current scene is not empty.
*/
TOutputProperties *OutputSettingsPopup::getProperties() const {
  ToonzScene *scene = getCurrentScene();
  if (!scene) return 0;

  return m_isPreviewSettings ? scene->getProperties()->getPreviewProperties()
                             : scene->getProperties()->getOutputProperties();
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::showEvent(QShowEvent *) {
  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  bool ret =
      connect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(updateField()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(updateField()));

  ret = ret && connect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                       SLOT(updateField()));
  updateField();
  assert(ret);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::hideEvent(QHideEvent *e) {
  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  bool ret = disconnect(sceneHandle, SIGNAL(sceneChanged()), this,
                        SLOT(updateField()));
  ret      = ret && disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
                               SLOT(updateField()));

  ret = ret && disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this,
                          SLOT(updateField()));
  assert(ret);
  Dialog::hideEvent(e);
}

//-----------------------------------------------------------------------------
// ignore wheelevent on comboboxes
bool OutputSettingsPopup::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::Wheel) {
    QComboBox *combo = qobject_cast<QComboBox *>(obj);
    if (combo && !combo->hasFocus()) return true;
  }
  return QObject::eventFilter(obj, e);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onApplyShrinkChecked(int state) {
  TRenderSettings rs       = getProperties()->getRenderSettings();
  rs.m_applyShrinkToViewer = (state == Qt::Checked);
  getProperties()->setRenderSettings(rs);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onSubcameraChecked(int state) {
  getProperties()->setSubcameraPreview(state == Qt::Checked);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onRenderClicked() {
  CommandManager::instance()->execute("MI_Render");
}

//-----------------------------------------------------------------------------

/*!	Update all field value take care current scene output properties.
 */
void OutputSettingsPopup::updateField() {
  ToonzScene *scene       = getCurrentScene();
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
    if (m_outputCameraOm) m_outputCameraOm->setCurrentIndex(0);
    m_startFld->setText(QString());
    m_endFld->setText(QString());
    m_stepFld->setText(QString());
    m_shrinkFld->setText(QString());
    if (m_applyShrinkChk) m_applyShrinkChk->setCheckState(Qt::Unchecked);
    m_resampleBalanceOm->setCurrentIndex(c_standard);
    m_channelWidthOm->setCurrentIndex(c_8bit);
    m_threadsComboOm->setCurrentIndex(0);
    m_rasterGranularityOm->setCurrentIndex(0);

    if (m_subcameraChk) m_subcameraChk->setCheckState(Qt::Unchecked);
    m_linearColorSpaceChk->setCheckState(Qt::Unchecked);
    m_colorSpaceGammaFld->setText(QString());
    if (m_syncColorSettingsButton)
      m_syncColorSettingsButton->setCheckState(Qt::Unchecked);
    return;
  }

  TRenderSettings renderSettings = prop->getRenderSettings();

  if (!m_isPreviewSettings) {
    TFilePath path = prop->getPath();
    QString name   = QString::fromStdWString(path.getWideName());
    if (name.isEmpty())
      name = QString::fromStdString(scene->getScenePath().getName());
    m_saveInFileFld->setPath(toQString(path.getParentDir()));
    m_fileNameFld->setText(name);
    // RenderController::instance()->setMovieExt(path.getType());
    m_fileFormat->setCurrentIndex(
        m_fileFormat->findText(QString::fromStdString(path.getType())));
    m_multimediaOm->setCurrentIndex(prop->getMultimediaRendering());
  }

  // Refresh format if allow-multithread was toggled
  if (m_allowMT != Preferences::instance()->getFfmpegMultiThread()) {
    onFormatChanged(m_fileFormat->currentText());
  }

  // camera
  if (m_outputCameraOm) {
    m_outputCameraOm->blockSignals(true);
    m_outputCameraOm->clear();
  }
  TStageObjectTree *tree  = scene->getXsheet()->getStageObjectTree();
  int cameraCount         = tree->getCameraCount();
  TStageObjectId cameraId = m_isPreviewSettings
                                ? tree->getCurrentPreviewCameraId()
                                : tree->getCurrentCameraId();
  QStringList cameras;
  int tmpCameraId = 0;
  int index       = 0;
  int i;
  for (i = 0; i < cameraCount;) {
    /*-- Deleteされたカメラの場合tmpCameraIdだけIncrementする --*/
    if (!tree->getStageObject(TStageObjectId::CameraId(tmpCameraId), false)) {
      tmpCameraId++;
      continue;
    }

    std::string name =
        tree->getStageObject(TStageObjectId::CameraId(tmpCameraId))->getName();
    cameras.append(QString::fromStdString(name));
    if (name == tree->getStageObject(cameraId)->getName()) index = i;
    i++;
    tmpCameraId++;
  }
  if (m_outputCameraOm) {
    m_outputCameraOm->addItems(cameras);
    m_outputCameraOm->blockSignals(false);
    m_outputCameraOm->setCurrentIndex(index);
  }
  if (m_subcameraChk)
    m_subcameraChk->setCheckState(prop->isSubcameraPreview() ? Qt::Checked
                                                             : Qt::Unchecked);

  // start,end,step
  int r0 = 0, r1 = -1, step;
  prop->getRange(r0, r1, step);
  if (r0 > r1) {
    r0 = 0;
    r1 = scene->getFrameCount() - 1;
    if (r1 < 0) r1 = 0;
  }
  m_startFld->setValue(r0 + 1);
  m_endFld->setValue(r1 + 1);
  m_stepFld->setValue(step);
  m_shrinkFld->setValue(renderSettings.m_shrinkX);
  if (m_applyShrinkChk)
    m_applyShrinkChk->setCheckState(
        renderSettings.m_applyShrinkToViewer ? Qt::Checked : Qt::Unchecked);
  // resample
  m_resampleBalanceOm->setCurrentIndex(
      (int)quality2index(renderSettings.m_quality));

  // channel width
  switch (renderSettings.m_bpp) {
  case 64:
    m_channelWidthOm->setCurrentIndex(c_16bit);
    break;
  case 128:
    m_channelWidthOm->setCurrentIndex(c_32bit);
    break;
  default:
    m_channelWidthOm->setCurrentIndex(c_8bit);
    break;
  }

  m_linearColorSpaceChk->setCheckState(
      renderSettings.m_linearColorSpace ? Qt::Checked : Qt::Unchecked);
  // currently bpp should be 128 when the linearColorSpace is ON
  m_channelWidthOm->setDisabled(renderSettings.m_linearColorSpace &&
                                renderSettings.m_bpp == 128);

  m_colorSpaceGammaFld->setValue(renderSettings.m_colorSpaceGamma);

  if (m_isPreviewSettings && m_syncColorSettingsButton) {
    m_syncColorSettingsButton->setChecked(
        getProperties()->isColorSettingsSynced());
  }

  // Threads
  m_threadsComboOm->setCurrentIndex(prop->getThreadIndex());

  // Raster granularity
  m_rasterGranularityOm->setCurrentIndex(prop->getMaxTileSizeIndex());

  if (m_isPreviewSettings) return;

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

  // clapperboard
  BoardSettings *boardSettings = prop->getBoardSettings();
  m_addBoard->setChecked(boardSettings->isActive());
  // clapperboard is only available with movie formats
  if (isMovieType(m_fileFormat->currentText().toStdString())) {
    m_addBoard->setEnabled(true);
    m_boardSettingsBtn->setEnabled(m_addBoard->isChecked());
  } else {
    m_addBoard->setEnabled(false);
    m_boardSettingsBtn->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------
/*! Set current scene output path to new path modified in popup field.
 */
void OutputSettingsPopup::onPathChanged() {
  if (!getCurrentScene()) return;

  QString text = m_saveInFileFld->getPath();
  TFilePath newFp(text.toStdWString());
  newFp = TApp::instance()->getCurrentScene()->getScene()->codeFilePath(newFp);
  m_saveInFileFld->setPath(QString::fromStdWString(newFp.getWideString()));

  TOutputProperties *prop = getProperties();
  TFilePath fp            = prop->getPath();

  if (fp == newFp) return;
  fp = fp.withParentDir(newFp);
  prop->setPath(fp);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output name to new name modified in popup field.
 */
void OutputSettingsPopup::onNameChanged() {
  ToonzScene *scene = getCurrentScene();
  if (!scene) return;

  QString name = m_fileNameFld->text();
  if (!isValidFileName(name)) {
    DVGui::error(
        "A filename cannot be empty or contain any of the following "
        "characters:\n \\ / : * ? \" < > |");
    TOutputProperties *prop = getProperties();
    TFilePath fp            = prop->getPath();
    QString name            = QString::fromStdString(fp.getName());
    if (name.isEmpty())
      name = QString::fromStdString(scene->getScenePath().getName());
    m_fileNameFld->setText(name);

    return;
  }
  if (isReservedFileName_message(name)) {
    TOutputProperties *prop = getProperties();
    TFilePath fp            = prop->getPath();
    QString name            = QString::fromStdString(fp.getName());
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

  // In case the output level name is reverted back to the scene name, we assume
  // that
  // a "Save Scene As..." command should move the value to the *new* scene name
  if (wname == scene->getScenePath().getWideName())
    wname = std::wstring();  // An empty name is redirected to the scene name
  // directly by the render procedure, let's use this!
  TOutputProperties *prop = getProperties();
  TFilePath fp            = prop->getPath();

  TFilePath newFp = fp.withName(wname);
  if (newFp == fp) return;  // Already had the right name

  prop->setPath(newFp);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output format to new format set in popup field.
 */
void OutputSettingsPopup::onFormatChanged(const QString &str) {
  auto isMultiRenderInvalid = [](std::string ext, bool allowMT) -> bool {
    return (!allowMT && (ext == "mp4" || ext == "gif" || ext == "webm")) ||
           ext == "spritesheet";
  };

  TOutputProperties *prop = getProperties();
  bool wasMultiRenderInvalid =
      isMultiRenderInvalid(prop->getPath().getType(), m_allowMT);
  // remove sepchar, ..
  TFilePath fp = prop->getPath().withNoFrame().withType(str.toStdString());
  // .. then add sepchar for sequencial image formats
  if (checkForSeqNum(str)) fp = fp.withFrame(prop->formatTemplateFId());

  if (prop->getPath() != fp) {
    prop->setPath(fp);
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }
  m_allowMT = Preferences::instance()->getFfmpegMultiThread();

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
  if (isMultiRenderInvalid(str.toStdString(), m_allowMT)) {
    m_threadsComboOm->setDisabled(true);
    m_threadsComboOm->setCurrentIndex(0);
  } else {
    m_threadsComboOm->setDisabled(false);
    if (wasMultiRenderInvalid) m_threadsComboOm->setCurrentIndex(2);
  }

  // clapperboard is only available with movie formats
  if (isMovieType(str.toStdString())) {
    m_addBoard->setEnabled(true);
    m_boardSettingsBtn->setEnabled(m_addBoard->isChecked());
  } else {
    m_addBoard->setEnabled(false);
    m_boardSettingsBtn->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------
/*! Open a popup output format settings popup to set current output format
                settings.
*/
void OutputSettingsPopup::openSettingsPopup() {
  TOutputProperties *prop = getProperties();
  std::string ext         = prop->getPath().getType();

  TFrameId oldTmplFId = prop->formatTemplateFId();

  bool ret =
      openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext),
                              &prop->formatTemplateFId(), false);

  if (!ret) return;

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);

  if (oldTmplFId.getZeroPadding() !=
          prop->formatTemplateFId().getZeroPadding() ||
      oldTmplFId.getStartSeqInd() !=
          prop->formatTemplateFId().getStartSeqInd()) {
    TFilePath fp =
        prop->getPath().withNoFrame().withFrame(prop->formatTemplateFId());
    prop->setPath(fp);
  }
}

//-----------------------------------------------------------------------------
/*! Set current scene output camera to camera set in popup field.
 */
void OutputSettingsPopup::onCameraChanged(const QString &str) {
  ToonzScene *scene = getCurrentScene();
  if (!scene) return;
  TStageObjectTree *tree = scene->getXsheet()->getStageObjectTree();
  int cameraCount        = tree->getCameraCount();
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
    std::string name =
        tree->getStageObject(TStageObjectId::CameraId(tmpCameraId))->getName();
    if (name == str.toStdString()) break;
    tmpCameraId++;
    i++;
  }
  if (i >= cameraCount) return;
  TApp *app                      = TApp::instance();
  TStageObjectId cameraId        = TStageObjectId::CameraId(i);
  TStageObjectId currentCameraId = m_isPreviewSettings
                                       ? tree->getCurrentPreviewCameraId()
                                       : tree->getCurrentCameraId();
  if (currentCameraId != cameraId) {
    TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
    disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this,
               SLOT(updateField()));

    if (m_isPreviewSettings)
      tree->setCurrentPreviewCameraId(cameraId);
    else
      tree->setCurrentCameraId(cameraId);
    app->getCurrentScene()->setDirtyFlag(true);
    xsheetHandle->notifyXsheetChanged();

    connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(updateField()));
  }
  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output frame values: start frame, end frame, step and
                shrink, to current field values.
*/

void OutputSettingsPopup::onStereoChecked(int) {
  m_stereoShift->setEnabled(m_doStereoscopy->isChecked());
  if (m_dominantFieldOm)
    m_dominantFieldOm->setEnabled(!m_doStereoscopy->isChecked());
  onStereoChanged();
}

void OutputSettingsPopup::onStereoChanged() {
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();
  rs.m_stereoscopic       = m_doStereoscopy->isChecked();
  rs.m_stereoscopicShift  = m_stereoShift->getValue();
  prop->setRenderSettings(rs);
}

//----------------------------------------------

void OutputSettingsPopup::onSyncColorSettingsChecked(int state) {
  assert(m_isPreviewSettings);
  bool doSync = (state == Qt::Checked);
  m_colorBox->setDisabled(doSync);
  getProperties()->syncColorSettings(doSync);
  // take values from the output settings
  if (doSync) {
    TRenderSettings out_rs = getCurrentScene()
                                 ->getProperties()
                                 ->getOutputProperties()
                                 ->getRenderSettings();
    TRenderSettings rs    = getProperties()->getRenderSettings();
    rs.m_bpp              = out_rs.m_bpp;
    rs.m_linearColorSpace = out_rs.m_linearColorSpace;
    rs.m_colorSpaceGamma  = out_rs.m_colorSpaceGamma;
    getProperties()->setRenderSettings(rs);
  }
  // dirty flag will be set here
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//----------------------------------------------

void OutputSettingsPopup::onFrameFldEditFinished() {
  ToonzScene *scene = getCurrentScene();
  if (!scene) return;
  TOutputProperties *prop = getProperties();

  int maxR0  = std::max(0, scene->getFrameCount() - 1);
  int r0     = (int)m_startFld->getValue() - 1;
  int r1     = (int)m_endFld->getValue() - 1;
  int step   = (int)m_stepFld->getValue();
  int shrink = (int)m_shrinkFld->getValue();

  bool error            = false;
  bool somethingChanged = false;

  if (r0 < 0) {
    error = true;
    r0    = 0;
  }
  if (r0 > maxR0) {
    error = true;
    r0    = maxR0;
  }
  if (r1 < r0) {
    error = true;
    r1    = r0;
  }
  if (r1 > maxR0) {
    error = true;
    r1    = maxR0;
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
  int oldR0, oldR1, oldStep;
  prop->getRange(oldR0, oldR1, oldStep);
  if (r0 != oldR0 || r1 != oldR1 || step != oldStep) {
    prop->setRange(r0, r1, step);
    somethingChanged = true;
  }
  TRenderSettings rs   = prop->getRenderSettings();
  int oldShrink        = rs.m_shrinkX;
  bool old_applyShrink = rs.m_applyShrinkToViewer;

  rs.m_shrinkX = rs.m_shrinkY = shrink;
  rs.m_applyShrinkToViewer =
      m_applyShrinkChk ? m_applyShrinkChk->checkState() == Qt::Checked : false;

  if (rs.m_shrinkX != oldShrink ||
      rs.m_applyShrinkToViewer != old_applyShrink) {
    prop->setRenderSettings(rs);
    somethingChanged = true;
  }

  if (somethingChanged) TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! Set current scene output resample balance to balance identified by \b type
                index in popup QComboBox.
*/
void OutputSettingsPopup::onResampleChanged(int type) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop                      = getProperties();
  TRenderSettings rs                           = prop->getRenderSettings();
  TRenderSettings::ResampleQuality old_quality = rs.m_quality;
  rs.m_quality = resampleInfoMap[(ResampleOption)type].quality;
  if (rs.m_quality == old_quality) return;
  prop->setRenderSettings(rs);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/*! Set current scene output channel width to value identified by \b type
                index in popup QComboBox.
*/
void OutputSettingsPopup::onChannelWidthChanged(int type) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();
  int old_bpp             = rs.m_bpp;
  if (type == c_8bit)
    rs.m_bpp = 32;
  else if (type == c_16bit)
    rs.m_bpp = 64;
  else
    rs.m_bpp = 128;

  if (rs.m_bpp == old_bpp) return;

  prop->setRenderSettings(rs);

  // sync output settings value to the preview settings
  if (!m_isPreviewSettings && getCurrentScene()
                                  ->getProperties()
                                  ->getPreviewProperties()
                                  ->isColorSettingsSynced()) {
    TRenderSettings prev_rs = getCurrentScene()
                                  ->getProperties()
                                  ->getPreviewProperties()
                                  ->getRenderSettings();
    prev_rs.m_bpp = rs.m_bpp;
    getCurrentScene()
        ->getProperties()
        ->getPreviewProperties()
        ->setRenderSettings(prev_rs);
  }

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
  // dirty flag will be set here
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}
//-----------------------------------------------------------------------------

void OutputSettingsPopup::onLinearColorSpaceChecked(int state) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();
  rs.m_linearColorSpace   = (state == Qt::Checked);
  // force floating point when compute in linear color space
  if (rs.m_linearColorSpace) {
    prop->setNonlinearBpp(rs.m_bpp);
    rs.m_bpp = 128;
  } else
    rs.m_bpp = prop->getNonlinearBpp();
  prop->setRenderSettings(rs);

  // sync output settings value to the preview settings
  TOutputProperties *prev_prop =
      getCurrentScene()->getProperties()->getPreviewProperties();
  if (!m_isPreviewSettings && prev_prop->isColorSettingsSynced()) {
    TRenderSettings prev_rs    = prev_prop->getRenderSettings();
    prev_rs.m_linearColorSpace = (state == Qt::Checked);
    if (prev_rs.m_linearColorSpace) {
      prev_prop->setNonlinearBpp(prev_rs.m_bpp);
      prev_rs.m_bpp = 128;
    } else
      prev_rs.m_bpp = prev_prop->getNonlinearBpp();
    getCurrentScene()
        ->getProperties()
        ->getPreviewProperties()
        ->setRenderSettings(prev_rs);
  }

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
  // dirty flag will be set here
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onColorSpaceGammaEdited() {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();

  double colorSpaceGamma = m_colorSpaceGammaFld->getValue();

  rs.m_colorSpaceGamma = colorSpaceGamma;
  prop->setRenderSettings(rs);

  // sync output settings value to the preview settings
  if (!m_isPreviewSettings && getCurrentScene()
                                  ->getProperties()
                                  ->getPreviewProperties()
                                  ->isColorSettingsSynced()) {
    TRenderSettings prev_rs = getCurrentScene()
                                  ->getProperties()
                                  ->getPreviewProperties()
                                  ->getRenderSettings();
    prev_rs.m_colorSpaceGamma = colorSpaceGamma;
    getCurrentScene()
        ->getProperties()
        ->getPreviewProperties()
        ->setRenderSettings(prev_rs);
  }

  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
  // dirty flag will be set here
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------
/*! Set current scene output gamma value to current value in popup gamma field.
 */
void OutputSettingsPopup::onGammaFldEditFinished() {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();

  double gamma = m_gammaFld->getValue();
  m_gammaFld->setCursorPosition(0);
  if (gamma <= 0) {
    gamma = 0.001;
    m_gammaFld->setValue(gamma);
  }
  TRenderSettings rs = prop->getRenderSettings();
  if (rs.m_gamma == gamma) return;
  rs.m_gamma = gamma;
  prop->setRenderSettings(rs);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! Set current scene output dominant field to value identified by \b type
                index in popup QComboBox. If time stretch velue from-to are
   different
                and dominant field is different from none show a message error.
*/
void OutputSettingsPopup::onDominantFieldChanged(int type) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();
  TRenderSettings::FieldPrevalence oldFieldPrevalence = rs.m_fieldPrevalence;
  if (type != c_none &&
      m_stretchFromFld->getValue() != m_stretchToFld->getValue()) {
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
  if (rs.m_fieldPrevalence == oldFieldPrevalence) return;
  prop->setRenderSettings(rs);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! If dominant field is different from none set time stretch field values to
                current scene output time stretch values; otherwise set current
   scene
                output time stretch values to current field values.
*/
void OutputSettingsPopup::onStretchFldEditFinished() {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();
  if (m_dominantFieldOm->currentIndex() != c_none) {
    DVGui::error("Can't stretch time in a field rendered scene\n");
    m_stretchFromFld->setValue(rs.m_timeStretchFrom);
    m_stretchToFld->setValue(rs.m_timeStretchTo);
    return;
  } else if (rs.m_timeStretchFrom == m_stretchFromFld->getValue() &&
             rs.m_timeStretchTo == m_stretchToFld->getValue()) {
    return;
  }

  rs.m_timeStretchFrom = m_stretchFromFld->getValue();
  rs.m_timeStretchTo   = m_stretchToFld->getValue();
  prop->setRenderSettings(rs);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onMultimediaChanged(int state) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  if (prop->getMultimediaRendering() == state) return;
  prop->setMultimediaRendering(state);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onThreadsComboChanged(int type) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  if (prop->getThreadIndex() == type) return;
  prop->setThreadIndex(type);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onRasterGranularityChanged(int type) {
  if (!getCurrentScene()) return;
  TOutputProperties *prop = getProperties();
  if (prop->getMaxTileSizeIndex() == type) return;
  prop->setMaxTileSizeIndex(type);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------
/*! OutputSettingsのPreset登録
 */
void OutputSettingsPopup::onAddPresetButtonPressed() {
  //*-- プリセット名を取得 --*/
  bool ok;
  QString qs = DVGui::getText(
      tr("Add preset"), tr("Enter the name for the output settings preset."),
      "", &ok);
  if (!ok || qs.isEmpty()) return;

  if (!qs.endsWith(".txt")) qs.append(".txt");
  TFilePath fp = TEnv::getConfigDir() + "outputpresets";
  if (!TFileStatus(fp).doesExist()) TSystem::mkDir(fp);
  fp = fp + qs.toStdString();

  /*-- すでに存在する場合、上書きを確認 --*/
  if (TFileStatus(fp).doesExist()) {
    int ret = QMessageBox::question(
        this, tr("Add output settings preset"),
        QString("The file %1 does already exist.\nDo you want to overwrite it?")
            .arg(qs),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Save);
    if (ret == QMessageBox::Cancel) return;
  }
  TSystem::touchFile(fp);

  TOStream os(fp, false);
  if (!os.checkStatus()) return;

  os.openChild("outputsettingspreset");

  /*-- カメラ設定 --*/
  os.openChild("camera");
  TXsheet *xsheet         = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
  TCamera *camera         = xsheet->getStageObject(cameraId)->getCamera();
  camera->saveData(os);
  os.closeChild();

  /*--  Saveinのプロジェクトフォルダ部分 --*/
  QString saveInPath = m_saveInFileFld->getPath();
  if (saveInPath.startsWith("+")) {
    QString projectFolderName = (saveInPath.contains("\\"))
                                    ? saveInPath.left(saveInPath.indexOf("\\"))
                                    : saveInPath;
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
    std::string ext    = fileExtensions[i];
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
  QString resq =
      resampleInfoMap[(ResampleOption)m_resampleBalanceOm->currentIndex()]
          .idString;
  os.child("resquality") << resq.toStdString();

  // Channel Width
  QString chanw = m_channelWidthOm->currentData().toString();
  os.child("bpp") << chanw.toStdString();
  // Linear Color Space & Color Space Gamma
  std::string linearStr = (m_linearColorSpaceChk->isChecked()) ? "1" : "0";
  os.child("linearColorSpace") << linearStr;
  os.child("colorSpaceGamma") << m_colorSpaceGammaFld->text().toStdString();

  // 140503 iwasawa Frame Rate (Scene Settings)
  os.child("frameRate") << m_frameRateFld->text().toStdString();

  // 140325 iwasawa stretch FPS
  os.child("stretchFrom") << m_stretchFromFld->text().toStdString();
  os.child("stretchTo") << m_stretchToFld->text().toStdString();
  // 140325 iwasawa gamma
  os.child("gamma") << m_gammaFld->text().toStdString();

  os.closeChild();  // outputsettingspreset

  m_presetCombo->blockSignals(true);
  updatePresetComboItems();
  m_presetCombo->setCurrentIndex(
      m_presetCombo->findText(QString::fromStdString(fp.getName())));
  m_presetCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------
/*! プリセットフォルダを調べ、コンボボックスにアイテムを格納する
 */
void OutputSettingsPopup::updatePresetComboItems() {
  m_presetCombo->clear();
  m_presetCombo->addItem(tr("<custom>"));
  TFilePath folder = TEnv::getConfigDir() + "outputpresets";

  TFileStatus fs(folder);
  if (!fs.doesExist()) TSystem::mkDir(folder);

  TFilePathSet paths = TSystem::readDirectory(folder);
  for (TFilePathSet::iterator it = paths.begin(); it != paths.end(); it++) {
    std::cout << it->getName() << std::endl;
    m_presetCombo->addItem(QString::fromStdString(it->getName()));
  }
  m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::translateResampleOptions() {
  resampleInfoMap[c_standard].uiString = tr("Standard");
  resampleInfoMap[c_improved].uiString = tr("Improved");
  resampleInfoMap[c_high].uiString     = tr("High");
  resampleInfoMap[c_triangle].uiString = tr("Triangle filter");
  resampleInfoMap[c_mitchell].uiString = tr("Mitchell-Netravali filter");
  resampleInfoMap[c_cubic5].uiString   = tr("Cubic convolution, a = .5");
  resampleInfoMap[c_cubic75].uiString  = tr("Cubic convolution, a = .75");
  resampleInfoMap[c_cubic1].uiString   = tr("Cubic convolution, a = 1");
  resampleInfoMap[c_hann2].uiString    = tr("Hann window, rad = 2");
  resampleInfoMap[c_hann3].uiString    = tr("Hann window, rad = 3");
  resampleInfoMap[c_hamming2].uiString = tr("Hamming window, rad = 2");
  resampleInfoMap[c_hamming3].uiString = tr("Hamming window, rad = 3");
  resampleInfoMap[c_lanczos2].uiString = tr("Lanczos window, rad = 2");
  resampleInfoMap[c_lanczos3].uiString = tr("Lanczos window, rad = 3");
  resampleInfoMap[c_gauss].uiString    = tr("Gaussian convolution");
  resampleInfoMap[c_closestPixel].uiString =
      tr("Closest Pixel (Nearest Neighbor)");
  resampleInfoMap[c_bilinear].uiString = tr("Bilinear");
}

//-----------------------------------------------------------------------------
/*! OutputSettingsのPreset削除
 */
void OutputSettingsPopup::onRemovePresetButtonPressed() {
  int index = m_presetCombo->currentIndex();
  if (index <= 0) return;
  int ret = QMessageBox::question(this, tr("Remove preset"),
                                  QString("Deleting \"%1\".\nAre you sure?")
                                      .arg(m_presetCombo->currentText()),
                                  QMessageBox::Ok | QMessageBox::Cancel,
                                  QMessageBox::Ok);
  if (ret == QMessageBox::Cancel) return;

  TFilePath fp =
      TEnv::getConfigDir() + "outputpresets" +
      QString("%1.txt").arg(m_presetCombo->currentText()).toStdString();
  if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);

  m_presetCombo->blockSignals(true);
  m_presetCombo->removeItem(index);
  m_presetCombo->setCurrentIndex(0);
  m_presetCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onPresetSelected(const QString &str) {
  /*-- "<custom>"を選択したら何もせずreturn --*/
  if (m_presetCombo->currentIndex() == 0) return;
  TFilePath fp = TEnv::getConfigDir() + "outputpresets" +
                 QString("%1.txt").arg(str).toStdString();
  if (!TFileStatus(fp).doesExist()) {
    QMessageBox::warning(this, tr("Warning"),
                         QString("The preset file %1.txt not found").arg(str),
                         QMessageBox::Ok, QMessageBox::Ok);
    return;
  }
  TIStream is(fp);
  if (!is) {
    QMessageBox::warning(
        this, tr("Warning"),
        QString("Failed to open the preset file %1.txt").arg(str),
        QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  std::string tagName = "";
  if (!is.matchTag(tagName) || tagName != "outputsettingspreset") {
    QMessageBox::warning(this, tr("Warning"),
                         QString("Bad file format: %1.txt").arg(str),
                         QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  TOutputProperties *prop = getProperties();
  TRenderSettings rs      = prop->getRenderSettings();

  // set back the linear settings to default
  // in order to make old presets properly reproduce the settings before the
  // implementation of linear rendering
  rs.m_linearColorSpace = false;
  rs.m_colorSpaceGamma  = 2.2;

  while (is.matchTag(tagName)) {
    // Camera
    if (tagName == "camera") {
      TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
      TStageObjectId cameraId =
          xsheet->getStageObjectTree()->getCurrentCameraId();
      TCamera *camera = xsheet->getStageObject(cameraId)->getCamera();
      camera->loadData(is);
    }  // if "camera"
    /*-- Saveinのプロジェクトフォルダ部分 --*/
    else if (tagName == "savein_projectfolder") {
      std::string projectFolderName;
      is >> projectFolderName;

      QString newProjectFolder = QString::fromStdString(projectFolderName);
      QString fp = QString::fromStdWString(prop->getPath().getWideString());
      /*-- 出力パスがプロジェクトフォルダで始まっている場合は、差し替える --*/
      if (!newProjectFolder.isEmpty() && fp.startsWith("+")) {
        if (fp.indexOf("\\") >= 0)
          fp = QString("%1\\%2")
                   .arg(newProjectFolder)
                   .arg(fp.right(fp.size() - fp.indexOf("\\") - 1));
        else
          fp = newProjectFolder;
        prop->setPath(TFilePath(fp.toStdWString()));
      }
    }
    // File format
    else if (tagName == "format") {
      std::string formatName;
      is >> formatName;
      TFilePath fp = prop->getPath().withType(formatName);
      prop->setPath(fp);
    }
    // File options
    else if (tagName == "formatsProperties") {
      while (is.matchTag(tagName)) {
        if (tagName == "formatProperties") {
          std::string ext    = is.getTagAttribute("ext");
          TPropertyGroup *pg = prop->getFileFormatProperties(ext);
          if (ext == "avi") {
            TPropertyGroup appProperties;
            appProperties.loadData(is);
            if (pg->getPropertyCount() != 1 ||
                appProperties.getPropertyCount() != 1) {
              is.closeChild();
              continue;
            }
            TEnumProperty *enumProp =
                dynamic_cast<TEnumProperty *>(pg->getProperty(0));
            TEnumProperty *enumAppProp =
                dynamic_cast<TEnumProperty *>(appProperties.getProperty(0));
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
      }  // end while
    }
    // Resample Balance
    else if (tagName == "resquality") {
      std::string resq;
      is >> resq;
      // first, search in the combobox item data.
      int index = m_resampleBalanceOm->findData(QString::fromStdString(resq));
      // second, search in the combobox ui text (which may be translated).
      // the second search is kept in order to keep backward compatibility.
      if (index < 0)
        index = m_resampleBalanceOm->findText(QString::fromStdString(resq));
      if (index >= 0) {
        m_resampleBalanceOm->setCurrentIndex(index);
        rs.m_quality = resampleInfoMap[(ResampleOption)index].quality;
      }
    }
    // Channel Width
    else if (tagName == "bpp") {
      std::string chanw;
      is >> chanw;
      // first, search in the combobox item data.
      int index = m_channelWidthOm->findData(QString::fromStdString(chanw));
      // second, search in the combobox ui text (which may be translated).
      // the second search is kept in order to keep backward compatibility.
      if (index < 0)
        index = m_channelWidthOm->findText(QString::fromStdString(chanw));
      if (index >= 0) {
        m_channelWidthOm->setCurrentIndex(index);
        if (index == c_8bit)
          rs.m_bpp = 32;
        else if (index == c_16bit)
          rs.m_bpp = 64;
        else
          rs.m_bpp = 128;
      }
    }
    // Linear Color Space & Color Space Gamma
    else if (tagName == "linearColorSpace") {
      std::string linearStr;
      is >> linearStr;
      rs.m_linearColorSpace = (linearStr != "0");
    } else if (tagName == "colorSpaceGamma") {
      std::string gamma;
      is >> gamma;
      rs.m_colorSpaceGamma = QString::fromStdString(gamma).toDouble();
    }

    // Frame Rate (Scene Settings)
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

void OutputSettingsPopup::onCameraSettingsChanged() {
  if (m_presetCombo) m_presetCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onFrameRateEditingFinished() {
  double frameRate         = getProperties()->getFrameRate();
  double frameRateFldValue = m_frameRateFld->getValue();
  if (frameRate == frameRateFldValue) return;
  getProperties()->setFrameRate(frameRateFldValue);
  TApp::instance()->getCurrentScene()->getScene()->updateSoundColumnFrameRate();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void OutputSettingsPopup::onAddBoardChecked(int state) {
  BoardSettings *boardSettings = getProperties()->getBoardSettings();
  boardSettings->setActive(state == Qt::Checked);

  m_boardSettingsBtn->setEnabled(state == Qt::Checked);
}

void OutputSettingsPopup::onBoardSettingsBtnClicked() {
  std::cout << "board settings button clicked" << std::endl;
  BoardSettingsPopup popup(this);
  popup.exec();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<OutputSettingsPopup> openOutputSettingsPopup(
    MI_OutputSettings);
OpenPopupCommandHandler<PreviewSettingsPopup> openPreviewSettingsPopup(
    MI_PreviewSettings);
