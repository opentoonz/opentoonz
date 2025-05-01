#include "colormodelbehaviorpopup.h"

#include "toonzqt/colorfield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/gutil.h"

#include "tenv.h"
#include "tpixel.h"

#include <QLabel>
#include <QColor>
#include <QButtonGroup>
#include <QRadioButton>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QGridLayout>

TEnv::IntVar ColorModelBehavior_PaletteOperation(
    "ColorModelBehavior_PaletteOperation", 0);
// moved from preferences to env
TEnv::IntVar ColorModelBehavior_RasterImagePickType(
    "ColorModelBehavior_RasterImagePickType", 0);
TEnv::StringVar ColorModelBehavior_ChipGridColor(
    "ColorModelBehavior_ChipGridColor", "#FF00FF");
TEnv::IntVar ColorModelBehavior_ChipGridLineWidth(
    "ColorModelBehavior_ChipGridLineWidth", 2);
TEnv::IntVar ColorModelBehavior_ChipOrder("ColorModelBehavior_ChipOrder", 0);
//-------------------------------------------------------------------

ColorModelBehaviorPopup::ColorModelBehaviorPopup(
    const std::set<TFilePath>& paths, QWidget* parent)
    : Dialog(parent, true, true) {
  setWindowTitle(tr("Select the Palette Operation"));

  // check if the paths contain standard raster image (i.e. without palette)
  // N.B. note that paths must contain only one path for now
  for (auto itr = paths.begin(); itr != paths.end(); ++itr) {
    std::string type((*itr).getType());
    if (type != "tlv" && type != "pli") {
      m_hasRasterImage = true;
      break;
    }
  }
  m_buttonGroup = new QButtonGroup(this);
  QRadioButton* keepColorModelPltButton =
      new QRadioButton(tr("Overwrite the destination palette."));
  QRadioButton* replaceColorModelPltButton = new QRadioButton(
      tr("Keep the destination palette and apply it to the color model."));

  m_buttonGroup->addButton(keepColorModelPltButton);
  m_buttonGroup->addButton(replaceColorModelPltButton);
  m_buttonGroup->setId(keepColorModelPltButton, PaletteCmd::KeepColorModelPlt);
  m_buttonGroup->setId(replaceColorModelPltButton,
                       PaletteCmd::ReplaceColorModelPlt);

  QLabel* label =
      new QLabel(tr("The color model palette is different from the destination "
                    "palette.\nWhat do you want to do? "));
  label->setAlignment(Qt::AlignLeft);
  m_topLayout->addWidget(label);

  QGroupBox* paletteBox   = new QGroupBox(this);
  QVBoxLayout* paletteLay = new QVBoxLayout();
  paletteLay->setMargin(15);
  paletteLay->setSpacing(15);

  paletteLay->addWidget(keepColorModelPltButton);
  paletteLay->addWidget(replaceColorModelPltButton);

  paletteBox->setLayout(paletteLay);
  m_topLayout->addWidget(paletteBox);

  bool ret = true;

  QGroupBox* pickColorBox = NULL;

  if (m_hasRasterImage) {
    QRadioButton* addColorModelPltButton = new QRadioButton(
        tr("Add color model's palette to the destination palette."));
    m_buttonGroup->addButton(addColorModelPltButton);
    m_buttonGroup->setId(addColorModelPltButton, PaletteCmd::AddColorModelPlt);
    paletteLay->addWidget(addColorModelPltButton);

    //-------
    m_topLayout->addSpacing(10);

    pickColorBox = new QGroupBox(tr("Picking Colors from Raster Image"), this);

    m_pickColorCombo         = new QComboBox(this);
    m_colorChipBehaviorFrame = new QFrame(this);
    m_colorChipGridColor =
        new DVGui::ColorField(this, false, TPixel32::Magenta, 40, false);
    m_colorChipGridLineWidth = new DVGui::IntLineEdit(this, 2, 1, 20);
    m_colorChipOrder         = new QButtonGroup(this);

    QPushButton* upperLeftOrderBtn =
        new QPushButton(createQIcon("colorchiporder_upleft"), "", this);
    QPushButton* lowerLeftOrderBtn =
        new QPushButton(createQIcon("colorchiporder_lowleft"), "", this);
    QPushButton* leftUpperOrderBtn =
        new QPushButton(createQIcon("colorchiporder_leftup"), "", this);

    QStringList paletteTypes;
    paletteTypes << tr("Pick Every Colors as Different Styles")
                 << tr("Integrate Similar Colors as One Style")
                 << tr("Pick Colors in Color Chip Grid");
    m_pickColorCombo->addItems(paletteTypes);

    upperLeftOrderBtn->setCheckable(true);
    lowerLeftOrderBtn->setCheckable(true);
    leftUpperOrderBtn->setCheckable(true);
    upperLeftOrderBtn->setIconSize(QSize(48, 42));
    lowerLeftOrderBtn->setIconSize(QSize(48, 42));
    leftUpperOrderBtn->setIconSize(QSize(48, 42));
    upperLeftOrderBtn->setFixedSize(QSize(54, 48));
    lowerLeftOrderBtn->setFixedSize(QSize(54, 48));
    leftUpperOrderBtn->setFixedSize(QSize(54, 48));
    upperLeftOrderBtn->setToolTip(tr("Horizontal - Top to bottom"));
    lowerLeftOrderBtn->setToolTip(tr("Horizontal - Bottom to top"));
    leftUpperOrderBtn->setToolTip(tr("Vertical - Left to right"));
    upperLeftOrderBtn->setObjectName("MatchLineButton");
    lowerLeftOrderBtn->setObjectName("MatchLineButton");
    leftUpperOrderBtn->setObjectName("MatchLineButton");

    m_colorChipOrder->addButton(upperLeftOrderBtn);
    m_colorChipOrder->addButton(lowerLeftOrderBtn);
    m_colorChipOrder->addButton(leftUpperOrderBtn);
    m_colorChipOrder->setId(upperLeftOrderBtn, PaletteCmd::UpperLeft);
    m_colorChipOrder->setId(lowerLeftOrderBtn, PaletteCmd::LowerLeft);
    m_colorChipOrder->setId(leftUpperOrderBtn, PaletteCmd::LeftUpper);
    m_colorChipOrder->setExclusive(true);

    QGridLayout* pickColorLay = new QGridLayout();
    pickColorLay->setMargin(10);
    pickColorLay->setHorizontalSpacing(5);
    pickColorLay->setVerticalSpacing(10);
    {
      pickColorLay->addWidget(new QLabel(tr("Pick Type:"), this), 0, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
      pickColorLay->addWidget(m_pickColorCombo, 0, 1, Qt::AlignLeft);

      QGridLayout* colorChipGridLay = new QGridLayout();
      colorChipGridLay->setMargin(0);
      colorChipGridLay->setHorizontalSpacing(5);
      colorChipGridLay->setVerticalSpacing(10);
      {
        colorChipGridLay->addWidget(new QLabel(tr("Grid Line Color:"), this), 0,
                                    0, Qt::AlignRight | Qt::AlignVCenter);
        colorChipGridLay->addWidget(m_colorChipGridColor, 0, 1, Qt::AlignLeft);

        colorChipGridLay->addWidget(new QLabel(tr("Grid Line Width:"), this), 1,
                                    0, Qt::AlignRight | Qt::AlignVCenter);
        colorChipGridLay->addWidget(m_colorChipGridLineWidth, 1, 1,
                                    Qt::AlignLeft);

        colorChipGridLay->addWidget(new QLabel(tr("Chip Order:"), this), 2, 0,
                                    Qt::AlignRight | Qt::AlignVCenter);
        QHBoxLayout* orderLay = new QHBoxLayout();
        orderLay->setMargin(0);
        orderLay->setSpacing(0);
        {
          orderLay->addWidget(upperLeftOrderBtn, 0);
          orderLay->addWidget(lowerLeftOrderBtn, 0);
          orderLay->addWidget(leftUpperOrderBtn, 0);
        }
        colorChipGridLay->addLayout(orderLay, 2, 1, Qt::AlignLeft);
      }
      colorChipGridLay->setColumnStretch(0, 0);
      colorChipGridLay->setColumnStretch(1, 1);
      m_colorChipBehaviorFrame->setLayout(colorChipGridLay);

      pickColorLay->addWidget(m_colorChipBehaviorFrame, 1, 0, 1, 2);
    }
    pickColorLay->setColumnStretch(0, 0);
    pickColorLay->setColumnStretch(1, 1);
    pickColorBox->setLayout(pickColorLay);
    m_topLayout->addWidget(pickColorBox);

    ret = ret && connect(m_pickColorCombo, SIGNAL(currentIndexChanged(int)),
                         this, SLOT(onPickColorComboActivated(int)));
    ret = ret && connect(replaceColorModelPltButton, SIGNAL(toggled(bool)),
                         pickColorBox, SLOT(setDisabled(bool)));
  }

  QPushButton* applyButton = new QPushButton(QObject::tr("Apply"));
  ret = ret && connect(applyButton, SIGNAL(clicked()), this, SLOT(accept()));
  QPushButton* cancelButton = new QPushButton(QObject::tr("Cancel"));
  ret = ret && connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);

  addButtonBarWidget(applyButton, cancelButton);

  // obtain initial values from env
  QAbstractButton* button =
      m_buttonGroup->button(ColorModelBehavior_PaletteOperation);
  if (button)
    button->setChecked(true);
  else
    keepColorModelPltButton->setChecked(true);

  if (m_hasRasterImage) {
    m_pickColorCombo->setCurrentIndex(ColorModelBehavior_RasterImagePickType);
    QColor chipGridColor(
        QString::fromStdString(ColorModelBehavior_ChipGridColor));
    m_colorChipGridColor->setColor(TPixel32(
        chipGridColor.red(), chipGridColor.green(), chipGridColor.blue()));
    m_colorChipGridLineWidth->setValue(ColorModelBehavior_ChipGridLineWidth);
    button = m_colorChipOrder->button(ColorModelBehavior_ChipOrder);
    if (button) button->setChecked(true);
    onPickColorComboActivated(ColorModelBehavior_RasterImagePickType);
    pickColorBox->setDisabled(ColorModelBehavior_PaletteOperation == 1);
  }
}

//-------------------------------------------------------------------

void ColorModelBehaviorPopup::getLoadingConfiguration(
    PaletteCmd::ColorModelLoadingConfiguration& config) {
  config.behavior = static_cast<PaletteCmd::ColorModelPltBehavior>(
      m_buttonGroup->checkedId());

  if (m_colorChipOrder)
    config.colorChipOrder =
        static_cast<PaletteCmd::ColorChipOrder>(m_colorChipOrder->checkedId());

  if (m_pickColorCombo)
    config.rasterPickType = static_cast<PaletteCmd::ColorModelRasterPickType>(
        m_pickColorCombo->currentIndex());

  if (m_colorChipGridColor) config.gridColor = m_colorChipGridColor->getColor();

  if (m_colorChipGridLineWidth)
    config.gridLineWidth = m_colorChipGridLineWidth->getValue();
}

//-------------------------------------------------------------------

void ColorModelBehaviorPopup::onPickColorComboActivated(int index) {
  if (!m_colorChipBehaviorFrame) return;
  m_colorChipBehaviorFrame->setEnabled(index == 2);
}

//-------------------------------------------------------------------
// save current state to env
ColorModelBehaviorPopup::~ColorModelBehaviorPopup() {
  int id = m_buttonGroup->checkedId();
  if (id != -1) ColorModelBehavior_PaletteOperation = id;

  if (!m_hasRasterImage) return;

  ColorModelBehavior_RasterImagePickType = m_pickColorCombo->currentIndex();

  TPixel32 gridColor = m_colorChipGridColor->getColor();
  ColorModelBehavior_ChipGridColor =
      QColor(gridColor.r, gridColor.g, gridColor.b)
          .name(QColor::HexRgb)
          .toStdString();

  ColorModelBehavior_ChipGridLineWidth = m_colorChipGridLineWidth->getValue();

  id                                         = m_colorChipOrder->checkedId();
  if (id != -1) ColorModelBehavior_ChipOrder = id;
}