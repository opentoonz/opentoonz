

#include "toonzqt/styleeditor.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/cleanupcolorstyles.h"
#include "tconvert.h"
#include "tcolorstyles.h"
#include "trop.h"
#include "toonzqt/lutcalibrator.h"

#include <QLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QLabel>

using namespace DVGui;

//=============================================================================
/*! \class DVGui::StyleSample
                \brief The StyleSample class provides to view a square color.

                Inherits \b QWidget.

                By default square color is set to \b TPixel32(235,235,235,255),
   you
                can set other color using setColor(); you can also define
   current color
                with a style \b TColorStyle, \b getStyle(), using setStyle().
                You can pass to constructor square size.

                StyleSample permit to manage click event, it's possile to enable
   this
                feature setting enableClick(bool on) to true.
                If it is enable when click in square class emit the signal
                clicked(const TColorStyle &style).
*/
/*!	\fn void DVGui::StyleSample::clicked(const TColorStyle &style)
                This signal is emitted when click event is enable and a style is
   set.
*/
/*!	\fn void DVGui::StyleSample::enableClick(bool on)
                Set click event enable if \b is true, disable otherwise.
                If true setStyle store current style and buttonPress emit signal
                clicked(const TColorStyle &style).
*/
StyleSample::StyleSample(QWidget *parent, int sizeX, int sizeY)
    : m_samplePixmap(sizeX, sizeY, QImage::Format_ARGB32)
    , m_bgRas(sizeX, sizeY)
    , m_style(0)
    , m_clickEnabled(false)
    , m_chessColor1(0, 0, 0)
    , m_chessColor2(255, 255, 255)
    , m_isEditing(false) {
  setMinimumSize(sizeX, sizeY);
  setColor(TPixel32::Transparent);
  TRop::checkBoard(m_bgRas, m_chessColor1, m_chessColor2,
                   TDimensionD(sizeX / 8, sizeX / 8), TPointD(0, 0));
  setEnable(true);
}

//-----------------------------------------------------------------------------

StyleSample::~StyleSample() {
  if (m_style) delete m_style;
  m_style = 0;
}

//-----------------------------------------------------------------------------
/*! Return current StyleSample \b TColorStyle style.
*/
TColorStyle *StyleSample::getStyle() const { return m_style; }

//-----------------------------------------------------------------------------
/*! Update current square colore and, if click event is enable set current
                StyleSample \b TColorStyle style to \b style.
*/
void StyleSample::setStyle(TColorStyle &style) {
  /*-- TSolidColorStyleの場合のみ、単色塗りつぶし --*/
  if (style.getTagId() == 3)
    setColor(style.getMainColor());
  else {
    TRaster32P icon =
        style.getIcon(qsize2Dimension(m_samplePixmap.rect().size()));
    m_samplePixmap = rasterToQImage(icon, false);  // modified in 6.2
    update();
  }
  if (m_clickEnabled) m_style = style.clone();
}

//-----------------------------------------------------------------------------
/*! Update current square colore to \b TPixel32 \b color.
                Useful for efficiency if click event is disable.
*/
void StyleSample::setColor(const TPixel32 &pixel) {
  QColor color(pixel.r, pixel.g, pixel.b, pixel.m);
  if (LutManager::instance()->isValid()) LutManager::instance()->convert(color);

  m_samplePixmap.fill(color.rgba());
  update();
}

//-----------------------------------------------------------------------------

void StyleSample::setChessboardColors(const TPixel32 &col1,
                                      const TPixel32 &col2) {
  m_chessColor1 = col1;
  m_chessColor2 = col2;
  TRop::checkBoard(m_bgRas, m_chessColor1, m_chessColor2,
                   TDimensionD(m_bgRas->getLx() / 8, m_bgRas->getLy() / 8),
                   TPointD(0, 0));
  update();
}

//-----------------------------------------------------------------------------
/*! Paint square color.
*/
void StyleSample::paintEvent(QPaintEvent *event) {
  if (!isEnable()) return;
  QPainter painter(this);
  QImage img(m_bgRas->getRawData(), m_bgRas->getLx(), m_bgRas->getLy(),
             QImage::Format_ARGB32);
  painter.drawImage(0, 0, img.scaled(size()));
  painter.drawImage(0, 0, m_samplePixmap.scaled(size()));
  if (m_isEditing) {
    // QRect rect(0,0,m_bgRas->getLx(),m_bgRas->getLy());
    painter.setPen(Qt::white);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    painter.drawRect(rect().adjusted(2, 2, -3, -3));
    painter.setPen(QColor(180, 210, 255));
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
  }
}

//-----------------------------------------------------------------------------
/*! If exist current style and event click is enable emit signal
                clicked(const TColorStyle &style).
*/
void StyleSample::mousePressEvent(QMouseEvent *event) {
  if (m_style && m_clickEnabled)
    emit clicked(*m_style);
  else
    event->ignore();
}

//-----------------------------------------------------------------------------

void StyleSample::mouseDoubleClickEvent(QMouseEvent *event) { event->ignore(); }

//=============================================================================
/*! \class DVGui::ChannelField
                \brief The ChannelField class is used to view an object to
   manage a color
                                         value, red, green, blue or transparency
   channel.

                Inherits \b QWidget.

                The object is composed of grid layout \b QGridLayout which
   contains a label
                in first row, first column, to identify channel, a text field \b
   IntLineEdit
                in first row, second column, and a slider in second row, second
   column.
                Texf field and slider are connected, so if you change one the
   other automatically
                change. You can set current value getChannel(), using
   setChannel().
                This two object is used to manage channel value, them range is
   fixed to [0,255].
                This object size is fixed, [50, 2*DVGui::WidgetHeight].

                To know when channel parameter value change class provides a
   signal, valueChanged(int value);
                class emit signal when slider value change or when text field is
   editing,
                see SLOT: onSliderChanged(int value) and onEditChanged(const
   QString &str)
                to know when signal is emitted.
*/
/*!	\fn void DVGui::ChannelField::valueChanged(int value)
                This signal is emitted when ChannelField, slider or text field,
   value change;
                if slider position change or text field is editing.
                \sa onEditChanged(const QString &str) and onSliderChanged(int
   value).
*/
ChannelField::ChannelField(QWidget *parent, const QString &string, int value,
                           int maxValue, bool horizontal, int labelWidth,
                           int sliderWidth)
    : QWidget(parent), m_maxValue(maxValue) {
  assert(maxValue > 0);
  assert(0 <= value && value <= m_maxValue);

  QLabel *channelName = new QLabel(string, this);
  m_channelEdit       = new DVGui::IntLineEdit(this, value, 0, maxValue);
  m_channelSlider     = new QSlider(Qt::Horizontal, this);

  channelName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  channelName->setFixedWidth(labelWidth);

  m_channelSlider->setRange(0, maxValue);
  m_channelSlider->setValue(value);

  //----layout
  QGridLayout *mainLayout = new QGridLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(3);
  {
    mainLayout->addWidget(channelName, 0, 0);
    mainLayout->addWidget(m_channelEdit, 0, 1);

    mainLayout->addWidget(m_channelSlider, horizontal ? 0 : 1,
                          horizontal ? 2 : 1);
  }
  mainLayout->setColumnStretch(0, 0);
  mainLayout->setColumnStretch(1, 1);
  mainLayout->setRowStretch(2, 1);
  setLayout(mainLayout);

  //----singnal-slot connections

  bool ret = connect(m_channelEdit, SIGNAL(textChanged(const QString &)),
                     SLOT(onEditChanged(const QString &)));
  ret = ret && connect(m_channelSlider, SIGNAL(valueChanged(int)),
                       SLOT(onSliderChanged(int)));
  ret = ret && connect(m_channelSlider, SIGNAL(sliderReleased()),
                       SLOT(onSliderReleased()));
  assert(ret);
}

//-----------------------------------------------------------------------------
/*! Set current value to \b value.
                \sa getChannel()
*/
void ChannelField::setChannel(int value) {
  if (getChannel() == value) return;
  assert(0 <= value && value <= m_maxValue);
  m_channelSlider->setValue(value);
  m_channelEdit->setValue(value);
}

//-----------------------------------------------------------------------------
/*! Return current channel value.
                \sa setChannel()
*/
int ChannelField::getChannel() {
  int value = m_channelEdit->getValue();
  assert(m_channelSlider->value() == value);
  return value;
}

//-----------------------------------------------------------------------------
/*!	Set slider value to new string \b str value.
                Verify if value is lower than 255 or greater than 0, range,
   otherwise set
                current value to 255 or 0. If slider value is different from
   value in \b str
                emit signal valueChanged(int value).
*/
void ChannelField::onEditChanged(const QString &str) {
  int value                     = str.toInt();
  if (value < 0) value          = 0;
  if (value > m_maxValue) value = m_maxValue;
  assert(0 <= value && value <= m_maxValue);
  if (str.toInt() != value) m_channelEdit->setValue(value);
  if (m_channelSlider->value() == value) return;
  m_channelSlider->setValue(value);
  emit valueChanged(value, false);
}

//-----------------------------------------------------------------------------
/*! Set text field value to \b value. If text field value is different from \b
   value
                emit signal valueChanged(int value).
*/
void ChannelField::onSliderChanged(int value) {
  assert(0 <= value && value <= m_maxValue);
  if (m_channelEdit->getValue() == value) return;
  m_channelEdit->setText(QString(std::to_string(value).c_str()));
  emit valueChanged(value, true);
}

//-----------------------------------------------------------------------------

void ChannelField::onSliderReleased() {
  emit valueChanged(m_channelSlider->value(), false);
}

//=============================================================================

ColorField::ColorFieldEditorController *ColorField::m_editorController = 0;
//																							new
// ColorField::ColorFieldEditorController();

//=============================================================================
/*! \class DVGui::ColorField
                \brief The ColorField class is used to view an object to manage
   a color.

                Inherits \b QWidget.

                The object is composed of a horizontal layout \b QHBoxLayout
   which contains
                a StyleSample, and three or four ChannelField, it depend if
   transparency is
                count, to manage color channel.
                You can pass to constructor current color value, getColor(), or
   set it
                calling setColor(). You can also pass to constructor a boolean
   to know if
                manage transparency channel or not, and an integer for
   StyleSample size.

                To know when color value change class provides a signal,
   colorChanged(const TPixel32 &);
                class emit signal when one ChannelField value change.
                See SLOT: onRedChannelChanged(int value),
   onGreenChannelChanged(int value),
                onBlueChannelChanged(int value) and onAlphaChannelChanged(int
   value) to know
                when signal is emitted.

                \b Example: initialize a ColorField with transparency channel.
                \code
                        ColorField* colorFld = new
   ColorField(0,true,TPixel32(0,0,255,255),50);
                \endcode
                \b Result:
                        \image html ColorField.jpg
*/
/*!	\fn void DVGui::ColorField::colorChanged(const TPixel32 &)
                This signal is emitted when a channel value of current color
   change.
*/
/*!	\fn TPixel32  DVGui::ColorField::getColor() const
                Return ColorField current color.
*/
ColorField::ColorField(QWidget *parent, bool isAlphaActive, TPixel32 color,
                       int squareSize, bool useStyleEditor)
    : QWidget(parent)
    , m_color(color)
    , m_notifyEditingChange(true)
    , m_useStyleEditor(useStyleEditor) {
  setMaximumHeight(squareSize);
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(5);

  layout->setSizeConstraint(QLayout::SetFixedSize);

  int h = WidgetHeight;

  m_colorSample = new StyleSample(this, squareSize, squareSize);
  m_colorSample->setColor(m_color);
  m_redChannel = new ChannelField(this, tr("R:"), m_color.r);
  connect(m_redChannel, SIGNAL(valueChanged(int, bool)),
          SLOT(onRedChannelChanged(int, bool)));
  m_greenChannel = new ChannelField(this, tr("G:"), m_color.g);
  connect(m_greenChannel, SIGNAL(valueChanged(int, bool)),
          SLOT(onGreenChannelChanged(int, bool)));
  m_blueChannel = new ChannelField(this, tr("B:"), m_color.b);
  connect(m_blueChannel, SIGNAL(valueChanged(int, bool)),
          SLOT(onBlueChannelChanged(int, bool)));
  m_alphaChannel = new ChannelField(this, tr("A:"), m_color.m);
  connect(m_alphaChannel, SIGNAL(valueChanged(int, bool)),
          SLOT(onAlphaChannelChanged(int, bool)));

  layout->addWidget(m_colorSample);
  layout->addWidget(m_redChannel);
  layout->addWidget(m_greenChannel);
  layout->addWidget(m_blueChannel);
  layout->addWidget(m_alphaChannel);

  if (!isAlphaActive) m_alphaChannel->hide();
  setLayout(layout);
}

//-----------------------------------------------------------------------------
/*! Set ColorField current color to \b color. Update channel value of
                \b ChannelField and \b StyleSample color.
*/

void ColorField::setAlphaActive(bool active) {
  if (active && !m_alphaChannel->isVisible()) {
    m_alphaChannel->show();
    connect(m_alphaChannel, SIGNAL(valueChanged(int, bool)),
            SLOT(onAlphaChannelChanged(int, bool)));
    assert(m_color.m == 255);
    m_alphaChannel->setChannel(0);
    m_color.m = 0;
    m_colorSample->setColor(m_color);
    emit colorChanged(m_color, false);
  } else if (!active && m_alphaChannel->isVisible()) {
    m_alphaChannel->hide();
    disconnect(m_alphaChannel, SIGNAL(valueChanged(int, bool)), this,
               SLOT(onAlphaChannelChanged(int, bool)));
    if (m_color.m != 255) {
      m_alphaChannel->setChannel(255);
      m_color.m = 255;
      m_colorSample->setColor(m_color);
      emit colorChanged(m_color, false);
    }
  }
}

//------------------------------

void ColorField::setColor(const TPixel32 &color) {
  if (m_color == color) return;
  m_color = color;
  updateChannels();
  m_colorSample->setColor(m_color);
}

//-----------------------------------------------------------------------------
/*! Set all \b ChannelField channel value to ColorField current color.
*/
void ColorField::hideChannelsFields(bool hide) {
  if (hide) {
    m_redChannel->hide();
    m_greenChannel->hide();
    m_blueChannel->hide();
    m_alphaChannel->hide();
    disconnect(m_redChannel, SIGNAL(valueChanged(int, bool)), this,
               SLOT(onRedChannelChanged(int, bool)));
    disconnect(m_greenChannel, SIGNAL(valueChanged(int, bool)), this,
               SLOT(onGreenChannelChanged(int, bool)));
    disconnect(m_blueChannel, SIGNAL(valueChanged(int, bool)), this,
               SLOT(onBlueChannelChanged(int, bool)));
    disconnect(m_alphaChannel, SIGNAL(valueChanged(int, bool)), this,
               SLOT(onAlphaChannelChanged(int, bool)));
  } else {
    m_redChannel->show();
    m_greenChannel->show();
    m_blueChannel->show();
    m_alphaChannel->show();
    ;
    connect(m_redChannel, SIGNAL(valueChanged(int, bool)),
            SLOT(onRedChannelChanged(int, bool)));
    connect(m_greenChannel, SIGNAL(valueChanged(int, bool)),
            SLOT(onGreenChannelChanged(int, bool)));
    connect(m_blueChannel, SIGNAL(valueChanged(int, bool)),
            SLOT(onBlueChannelChanged(int, bool)));
    connect(m_alphaChannel, SIGNAL(valueChanged(int, bool)),
            SLOT(onAlphaChannelChanged(int, bool)));
  }
}

//-----------------------------------------------------------------------------
/*! Set all \b ChannelField channel value to ColorField current color.
*/
void ColorField::updateChannels() {
  m_redChannel->setChannel(m_color.r);
  m_greenChannel->setChannel(m_color.g);
  m_blueChannel->setChannel(m_color.b);
  m_alphaChannel->setChannel(m_color.m);
}

//-----------------------------------------------------------------------------

void ColorField::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  QPoint p = event->pos();
  if (!m_colorSample->visibleRegion().contains(p)) return;

  if (!m_useStyleEditor || !getEditorController()) return;

  getEditorController()->edit(this);
}

//-----------------------------------------------------------------------------

void ColorField::mouseDoubleClickEvent(QMouseEvent *event) {
  QPoint p = event->pos();
  if (!m_colorSample->visibleRegion().contains(p)) return;

  if (!m_useStyleEditor || !getEditorController()) return;

  CommandManager::instance()->execute("MI_OpenStyleControl");
  getEditorController()->edit(this);
}

//-----------------------------------------------------------------------------

void ColorField::hideEvent(QHideEvent *) {
  if (!m_useStyleEditor || !getEditorController()) return;

  getEditorController()->hide();
}

//-----------------------------------------------------------------------------
/*! If current red channel value of color is different from \b value set it,
                change \b StyleSample color and emit signal \b
   colorChanged(const TPixel32 &).
*/
void ColorField::onRedChannelChanged(int value, bool isDragging) {
  if (m_color.r == value) {
    if (!isDragging) emit colorChanged(m_color, isDragging);
    return;
  }
  m_color = TPixel32(value, m_color.g, m_color.b, m_color.m);
  m_colorSample->setColor(m_color);
  emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------
/*! If current green channel value of color is different from \b value set it,
                change \b StyleSample color and emit signal \b
   colorChanged(const TPixel32 &).
*/
void ColorField::onGreenChannelChanged(int value, bool isDragging) {
  if (m_color.g == value) {
    if (!isDragging) emit colorChanged(m_color, isDragging);
    return;
  }
  m_color = TPixel32(m_color.r, value, m_color.b, m_color.m);
  m_colorSample->setColor(m_color);
  emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------
/*! If current blue channel value of color is different from \b value set it,
                change \b StyleSample color and emit signal \b
   colorChanged(const TPixel32 &).
*/
void ColorField::onBlueChannelChanged(int value, bool isDragging) {
  if (m_color.b == value) {
    if (!isDragging) emit colorChanged(m_color, isDragging);
    return;
  }
  m_color = TPixel32(m_color.r, m_color.g, value, m_color.m);
  m_colorSample->setColor(m_color);
  emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------
/*! If current alpha channel value of color is different from \b value set it,
                change \b StyleSample color and emit signal \b
   colorChanged(const TPixel32 &).
*/
void ColorField::onAlphaChannelChanged(int value, bool isDragging) {
  if (m_color.m == value) {
    if (!isDragging) emit colorChanged(m_color, isDragging);
    return;
  }
  m_color = TPixel32(m_color.r, m_color.g, m_color.b, value);
  m_colorSample->setColor(m_color);
  emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------

void ColorField::setChessboardColors(const TPixel32 &col1,
                                     const TPixel32 &col2) {
  m_colorSample->setChessboardColors(col1, col2);
}

//-----------------------------------------------------------------------------

void ColorField::setEditorController(
    ColorFieldEditorController *editorController) {
  m_editorController = editorController;
}

//-----------------------------------------------------------------------------

ColorField::ColorFieldEditorController *ColorField::getEditorController() {
  return m_editorController;
}

//-----------------------------------------------------------------------
#define SQUARESIZE 50

void CleanupColorField::onBrightnessChannelChanged(int value, bool dragging) {
  m_cleanupStyle->setBrightness(value);
  m_ph->notifyColorStyleChanged(dragging);
}

//-----------------------------------------------

void CleanupColorField::onContrastChannelChanged(int value, bool dragging) {
  m_cleanupStyle->setContrast(value);
  m_ph->notifyColorStyleChanged(dragging);
}

//-----------------------------------------------

void CleanupColorField::onCThresholdChannelChanged(int value, bool dragging) {
  ((TBlackCleanupStyle *)m_cleanupStyle)->setColorThreshold((double)value);
  m_ph->notifyColorStyleChanged(dragging);
}

//-----------------------------------------------

void CleanupColorField::onWThresholdChannelChanged(int value, bool dragging) {
  ((TBlackCleanupStyle *)m_cleanupStyle)->setWhiteThreshold((double)value);
  m_ph->notifyColorStyleChanged(dragging);
}

void CleanupColorField::onHRangeChannelChanged(int value, bool dragging) {
  ((TColorCleanupStyle *)m_cleanupStyle)->setHRange(value);
  m_ph->notifyColorStyleChanged(dragging);
}

//-----------------------------------------------

void CleanupColorField::onLineWidthChannelChanged(int value, bool dragging) {
  ((TColorCleanupStyle *)m_cleanupStyle)->setLineWidth(value);
  m_ph->notifyColorStyleChanged(dragging);
}

//---------------------------------------------------

void CleanupColorField::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;

  emit StyleSelected(m_cleanupStyle);

  if (getEditorController()) getEditorController()->edit(this);
}

//-----------------------------------------------
CleanupColorField::CleanupColorField(QWidget *parent,
                                     TCleanupStyle *cleanupStyle,
                                     TPaletteHandle *ph, bool greyMode)
    : QWidget(parent)
    , m_style(cleanupStyle)
    , m_cleanupStyle(cleanupStyle)
    , m_ph(ph)
    , m_greyMode(greyMode)
    , m_notifyEditingChange(true) {
  TBlackCleanupStyle *bs = dynamic_cast<TBlackCleanupStyle *>(cleanupStyle);
  TColorCleanupStyle *cs = dynamic_cast<TColorCleanupStyle *>(cleanupStyle);
  assert(bs || cs);

  m_colorSample = new StyleSample(this, SQUARESIZE / 2, SQUARESIZE);
  m_brightnessChannel =
      new ChannelField(this, DVGui::CleanupColorField::tr("Brightness:"),
                       cleanupStyle->getBrightness(), 100, true, 75, -1);
  m_contrastChannel =
      new ChannelField(this, DVGui::CleanupColorField::tr("Contrast:"),
                       cleanupStyle->getContrast(), 100, true, 75, -1);
  if (!greyMode) {
    if (bs) {
      m_cThresholdChannel =
          new ChannelField(this, DVGui::CleanupColorField::tr("Color Thres"),
                           bs->getColorThreshold(), 100, true, 75, -1);
      m_wThresholdChannel =
          new ChannelField(this, DVGui::CleanupColorField::tr("White Thres"),
                           bs->getWhiteThreshold(), 100, true, 75, -1);
    } else  // cs
    {
      m_hRangeChannel =
          new ChannelField(this, DVGui::CleanupColorField::tr("H Range"),
                           cs->getHRange(), 120, true, 75, -1);
      m_lineWidthChannel =
          new ChannelField(this, DVGui::CleanupColorField::tr("Line Width"),
                           cs->getLineWidth(), 100, true, 75, -1);
    }
  }

  m_colorSample->setStyle(*cleanupStyle);

  //---- layout

  QHBoxLayout *mainLay = new QHBoxLayout();
  mainLay->setMargin(8);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(m_colorSample, 0);

    QVBoxLayout *paramLay = new QVBoxLayout();
    paramLay->setMargin(0);
    paramLay->setSpacing(3);
    {
      paramLay->addWidget(m_brightnessChannel);
      paramLay->addWidget(m_contrastChannel);
      if (!greyMode) {
        if (bs) {
          paramLay->addWidget(m_cThresholdChannel);
          paramLay->addWidget(m_wThresholdChannel);
        } else {
          paramLay->addWidget(m_hRangeChannel);
          paramLay->addWidget(m_lineWidthChannel);
        }
      }
    }
    mainLay->addLayout(paramLay, 1);
  }
  setLayout(mainLay);

  //---- signal-slot connections

  bool ret = true;
  ret = ret && connect(m_brightnessChannel, SIGNAL(valueChanged(int, bool)),
                       SLOT(onBrightnessChannelChanged(int, bool)));
  ret = ret && connect(m_contrastChannel, SIGNAL(valueChanged(int, bool)),
                       SLOT(onContrastChannelChanged(int, bool)));
  if (!greyMode) {
    if (bs) {
      ret = ret && connect(m_cThresholdChannel, SIGNAL(valueChanged(int, bool)),
                           SLOT(onCThresholdChannelChanged(int, bool)));
      ret = ret && connect(m_wThresholdChannel, SIGNAL(valueChanged(int, bool)),
                           SLOT(onWThresholdChannelChanged(int, bool)));
    } else {
      ret = ret && connect(m_hRangeChannel, SIGNAL(valueChanged(int, bool)),
                           SLOT(onHRangeChannelChanged(int, bool)));
      ret = ret && connect(m_lineWidthChannel, SIGNAL(valueChanged(int, bool)),
                           SLOT(onLineWidthChannelChanged(int, bool)));
    }
  }
}

//--------------------------------------------------------------

void CleanupColorField::updateColor() {
  if (m_cleanupStyle->canUpdate()) {
    m_cleanupStyle->invalidateIcon();
    m_colorSample->setStyle(*m_cleanupStyle);

    m_brightnessChannel->setChannel(m_cleanupStyle->getBrightness());
    if (m_cleanupStyle->isContrastEnabled())
      m_contrastChannel->setChannel(m_cleanupStyle->getContrast());

    TBlackCleanupStyle *bs;
    TColorCleanupStyle *cs;
    if ((bs = dynamic_cast<TBlackCleanupStyle *>(m_cleanupStyle)) &&
        !m_greyMode) {
      m_cThresholdChannel->setChannel(bs->getColorThreshold());
      m_wThresholdChannel->setChannel(bs->getWhiteThreshold());
    } else if ((cs = dynamic_cast<TColorCleanupStyle *>(m_cleanupStyle))) {
      m_hRangeChannel->setChannel(cs->getHRange());
      m_lineWidthChannel->setChannel(cs->getLineWidth());
    }
  }
}

//--------------------------------------------------------------

TPixel32 CleanupColorField::getColor() const {
  return m_cleanupStyle->getMainColor();
}

//--------------------------------------------------------------

void CleanupColorField::setColor(const TPixel32 &color) {
  if (m_cleanupStyle->getMainColor() == color) return;

  m_cleanupStyle->setMainColor(color);
  m_cleanupStyle->invalidateIcon();
  m_colorSample->setStyle(*m_cleanupStyle);
  m_ph->notifyColorStyleChanged(false);
}

//------------------------------------------------------------

TPixel32 CleanupColorField::getOutputColor() const {
  return m_cleanupStyle->getColorParamValue(1);
}

//------------------------------------------------------------

void CleanupColorField::setOutputColor(const TPixel32 &color) {
  if (getOutputColor() == color) return;

  m_cleanupStyle->setColorParamValue(1, color);
  m_cleanupStyle->invalidateIcon();
  m_colorSample->setStyle(*m_cleanupStyle);
  m_ph->notifyColorStyleChanged();
}

//------------------------------------------------------------

void CleanupColorField::setStyle(TColorStyle *style) {
  if (getColor() == style->getMainColor() &&
      getOutputColor() == style->getColorParamValue(1))
    return;

  m_cleanupStyle->setMainColor(style->getMainColor());
  m_cleanupStyle->setColorParamValue(1, style->getColorParamValue(1));
  m_cleanupStyle->invalidateIcon();
  m_colorSample->setStyle(*m_cleanupStyle);
  m_ph->notifyColorStyleChanged();
}

//------------------------------------------------------------

CleanupColorField::CleanupColorFieldEditorController
    *CleanupColorField::m_editorController = 0;

CleanupColorField::CleanupColorFieldEditorController *
CleanupColorField::getEditorController() {
  return m_editorController;
}

//-----------------------------------------------------------------------------

void CleanupColorField::setEditorController(
    CleanupColorFieldEditorController *editorController) {
  m_editorController = editorController;
}

//------------------------------------------------------------------

void CleanupColorField::mouseDoubleClickEvent(QMouseEvent *event) {
  QPoint p = event->pos();
  if (!m_colorSample->visibleRegion().contains(p)) return;
  emit StyleSelected(m_cleanupStyle);
  if (!getEditorController()) return;

  CommandManager::instance()->execute("MI_OpenStyleControl");
  getEditorController()->edit(this);
}

//-----------------------------------------------------------

void CleanupColorField::hideEvent(QHideEvent *) {
  if (!getEditorController()) return;
  getEditorController()->edit(0);
  getEditorController()->hide();
  // setEditorController(0);
}

//-----------------------------------------------------------

void CleanupColorField::setContrastEnabled(bool enable) {
  m_contrastChannel->setEnabled(enable);
  m_cleanupStyle->enableContrast(enable);
}
