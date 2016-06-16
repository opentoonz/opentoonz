

#include "toonzqt/spectrumfield.h"
#include "toonzqt/styleeditor.h"
#include "tconvert.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

#include "tcolorstyles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QImage>

using namespace DVGui;

//=============================================================================
/*! \class DVGui::SpectrumBar
                \brief The SpectrumBar class provides a bar to display a
   spectrum

                Inherits \b QWidget.

                The object allows to manage a spectrum, permit you to insert,
   move and
                remove key.

                You can set spectrum default color in constructor, or set color
   of current
                key, \b getCurrentColor(), using \b setCurrentColor(). It's
   possible to change
                current key, \b getCurrentKeyIndex(), through its index, \b
   setCurrentKeyIndex().
                You can also set current key position, \b getCurrentPos(), using
   \b setCurrentPos()
                or add a new key in a specific position, addKeyAt().
*/
/*!	\fn void DVGui::SpectrumBar::currentPosChanged()
                The signal is emitted when current key position change.
*/
/*!	\fn void DVGui::SpectrumBar::currentKeyChanged()
                The signal is emitted when current key index change.
*/
SpectrumBar::SpectrumBar(QWidget *parent, TPixel32 color)
    : QWidget(parent)
    , m_x0(10)
    , m_currentKeyIndex(0)
    , m_spectrum(color, color)
    , m_chessBg(":Resources/backg.png") {
  setMinimumWidth(200);
  setFixedHeight(WidgetHeight);
}

//-----------------------------------------------------------------------------

SpectrumBar::~SpectrumBar() {}

//-----------------------------------------------------------------------------
/*! Return current key position.
*/
int SpectrumBar::getCurrentPos() {
  if (m_currentKeyIndex == -1) return -1;
  return spectrumValueToPos(m_spectrum.getKey(m_currentKeyIndex).first);
}

//-----------------------------------------------------------------------------
/*! Return current key color.
*/
TPixel32 SpectrumBar::getCurrentColor() {
  if (m_currentKeyIndex == -1)  // C'e' sicuramente una key perche' lo spectrum
                                // e' strutturato in modo da non avere mai size
                                // nulla.
    return m_spectrum.getKey(getMaxPosKeyIndex()).second;
  return m_spectrum.getKey(m_currentKeyIndex).second;
}

//-----------------------------------------------------------------------------
/*! Set current key index to \b index. Update spectrum bar and emit a signal to
                notify current key changed \b currentKeyChanged().
*/
void SpectrumBar::setCurrentKeyIndex(int index) {
  if (m_currentKeyIndex == index) return;
  m_currentKeyIndex = index;
  update();
  emit currentKeyChanged();
}

//-----------------------------------------------------------------------------
/*! Add a new key to spectrum. Key position is computed \b x using
                \b posToSpectrumValue() and color come from position.
*/
void SpectrumBar::addKeyAt(int x) {
  TPixel32 color = m_spectrum.getValue(posToSpectrumValue(x));
  TSpectrum::ColorKey key(posToSpectrumValue(x), color);
  m_spectrum.addKey(key);
  int newIndex = m_spectrum.getKeyCount() - 1;
  setCurrentKeyIndex(newIndex);
  emit currentKeyAdded(newIndex);
}

//-----------------------------------------------------------------------------
/*! Set current key position to \b pos. Update spectrum bar and emit a signal to
                notify current key changed \b currentKeyChanged().
*/
void SpectrumBar::setCurrentPos(int pos, bool isDragging) {
  TSpectrum::ColorKey key = m_spectrum.getKey(m_currentKeyIndex);
  key.first               = posToSpectrumValue(pos);
  m_spectrum.setKey(m_currentKeyIndex, key);
  update();
  emit currentPosChanged(isDragging);
}

//-----------------------------------------------------------------------------
/*! Set current key color to \b color.  Update spectrum bar.
*/
void SpectrumBar::setCurrentColor(const TPixel32 &color) {
  if (m_currentKeyIndex == -1) return;
  TSpectrum::ColorKey key = m_spectrum.getKey(m_currentKeyIndex);
  TPixel32 oldColor       = key.second;
  if (oldColor == color) return;
  key.second = color;
  m_spectrum.setKey(m_currentKeyIndex, key);
  update();
}

//-----------------------------------------------------------------------------
/*! Return spectrum value from widget position \b pos.
*/
double SpectrumBar::posToSpectrumValue(int pos) {
  return (double)(pos - m_x0) / (double)(width() - 2 * m_x0 - 1);
}

//-----------------------------------------------------------------------------
/*! Return widget position from spectrum value \b spectrumValue.
*/
int SpectrumBar::spectrumValueToPos(double spectrumValue) {
  return m_x0 + int(0.5 + spectrumValue * (width() - 2 * m_x0));
}

//-----------------------------------------------------------------------------
/*! Paint widget object:
                \li a linear gradient computed with spectrum value;
                \li arrows for each spectrum key.
*/
void SpectrumBar::paintEvent(QPaintEvent *e) {
  QPainter p(this);

  int h          = height() - 1;
  int y1         = height() / 2;
  int x1         = width() - m_x0;
  QRectF rectBar = QRectF(m_x0, 0, x1 - m_x0 + 1, y1);

  // Build the linear gradient
  QLinearGradient linearGrad(QPointF(m_x0, h), QPointF(x1, h));
  int spectrumSize = m_spectrum.getKeyCount();
  int i;
  for (i = 0; i < spectrumSize; i++) {
    TSpectrum::ColorKey key = m_spectrum.getKey(i);
    // Linear Gradient
    TPixel32 pix         = key.second;
    double spectrumValue = key.first;
    linearGrad.setColorAt(spectrumValue, QColor(pix.r, pix.g, pix.b, pix.m));
    // Frecce delle key
    int pos = spectrumValueToPos(spectrumValue);
    int f   = 4;
    drawArrow(p, QPointF(pos - f, y1 + f), QPointF(pos, y1),
              QPointF(pos + f, y1 + f), true,
              (m_currentKeyIndex == i) ? Qt::black : Qt::white);
  }

  p.setPen(Qt::NoPen);

  // Draw chess bg
  QBrush bg;
  bg.setTexture(m_chessBg);
  p.setBrush(bg);
  p.drawRect(rectBar);

  // Draw the gradient
  p.setBrush(linearGrad);
  p.drawRect(rectBar);
}

//-----------------------------------------------------------------------------
/*! Manage mouse press event. Search exsisting key near click position, if
                there is set it as current key; otherwise add a new key in click
   position.
*/
void SpectrumBar::mousePressEvent(QMouseEvent *e) {
  QPoint pos = e->pos();
  int x      = pos.x();

  // Verifico se esiste una key vicino alla posizione in cui ho cliccato
  int index        = -1;
  int spectrumSize = m_spectrum.getKeyCount();
  if (x < m_x0)
    index = getMinPosKeyIndex();
  else if (x > width() - m_x0)
    index = getMaxPosKeyIndex();
  else
    index = getNearPosKeyIndex(x);

  // Se x e' vicino a una key esistente setto questa come corrente
  if (index != -1)
    setCurrentKeyIndex(index);
  else  // Altrimenti aggiungo una nuova key
    addKeyAt(x);
}

//-----------------------------------------------------------------------------
/*! Manage mouse move event. If y position value greater than widget height
                erase current key; otherwise move current key position.
*/
void SpectrumBar::mouseMoveEvent(QMouseEvent *e) {
  QPoint pos = e->pos();
  int x      = pos.x();
  int y      = pos.y();
  if (x < m_x0 || x > width() - m_x0 - 1) return;

  // Se il valore della y e' maggiore dell'altezza widget, cioe' va fuori dal
  // widget
  // elimino la key corrente
  if (y > height()) {
    if (m_currentKeyIndex == -1 || m_spectrum.getKeyCount() == 1) return;

    m_spectrum.removeKey(m_currentKeyIndex);
    int keyIndex = m_currentKeyIndex;
    setCurrentKeyIndex(-1);
    emit currentKeyRemoved(keyIndex);
    return;
  }

  // Se il valore della y e' interno al widget
  // Se l'indice della key corrente e' -1 avevo eliminato un key; ora devo
  // riaggiungerla.
  if (m_currentKeyIndex == -1) addKeyAt(x);

  // Setto la nuova posizione x alla chiave corrente
  setCurrentPos(x, true);
}
//-----------------------------------------------------------------------------

void SpectrumBar::mouseReleaseEvent(QMouseEvent *e) {
  if (m_currentKeyIndex == -1) return;
  QPoint pos = e->pos();
  int x      = pos.x();
  int y      = pos.y();
  if (x < m_x0 || x > width() - m_x0 - 1) return;
  if (y > height()) return;

  setCurrentPos(x, false);
}

//-----------------------------------------------------------------------------
/*! Return index of key with maximum position.
*/
int SpectrumBar::getMaxPosKeyIndex() {
  int spectrumSize = m_spectrum.getKeyCount();
  if (!spectrumSize) return -1;
  int maxPosIndex = 0;
  int maxPos      = spectrumValueToPos(m_spectrum.getKey(0).first);
  int i;
  for (i = 0; i < spectrumSize; i++) {
    int newPos = spectrumValueToPos(m_spectrum.getKey(i).first);
    if (maxPos < newPos) {
      maxPos      = newPos;
      maxPosIndex = i;
    }
  }
  return maxPosIndex;
}

//-----------------------------------------------------------------------------
/*! Return index of key with minimum position.
*/
int SpectrumBar::getMinPosKeyIndex() {
  int spectrumSize = m_spectrum.getKeyCount();
  if (!spectrumSize) return -1;
  int minPosIndex = 0;
  int minPos      = spectrumValueToPos(m_spectrum.getKey(0).first);
  int i;
  for (i = 0; i < spectrumSize; i++) {
    int newPos = spectrumValueToPos(m_spectrum.getKey(i).first);
    if (minPos > newPos) {
      minPos      = newPos;
      minPosIndex = i;
    }
  }
  return minPosIndex;
}

//-----------------------------------------------------------------------------
/*! Return index of key "near" \b pos. "near" mean with a dinstace less than
                a constant size. If there aren't key near pos return \b -1.
*/
int SpectrumBar::getNearPosKeyIndex(int pos) {
  int i;
  int gap = 20;
  for (i = 0; i < m_spectrum.getKeyCount(); i++)
    if (areAlmostEqual(double(pos),
                       double(spectrumValueToPos(m_spectrum.getKey(i).first)),
                       gap))
      return i;
  return -1;
}

//=============================================================================
/*! \class DVGui::SpectrumField
                \brief The SpectrumField class provides an object to display and
   manage a spectrum.

                Inherits \b QWidget.

                The object is composed of a vertical layout \b QVBoxLayout which
   contains a
                spectrum bar \b SpectrumBar and a color field \b ColorField.
   Using color field
                it's possible to set spectrum current key color.

                \b Example: Spectrum Field.
                \image html SpectrumField.jpg
*/
SpectrumField::SpectrumField(QWidget *parent, TPixel32 color)
    : QWidget(parent), m_margin(0), m_spacing(4) {
  setFixedHeight(60);

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setMargin(m_margin);
  layout->setSpacing(m_spacing);

  m_spectrumbar = new SpectrumBar(this, color);
  connect(m_spectrumbar, SIGNAL(currentPosChanged(bool)),
          SLOT(onCurrentPosChanged(bool)));
  connect(m_spectrumbar, SIGNAL(currentKeyChanged()),
          SLOT(onCurrentKeyChanged()));
  connect(m_spectrumbar, SIGNAL(currentKeyAdded(int)), SIGNAL(keyAdded(int)));
  connect(m_spectrumbar, SIGNAL(currentKeyRemoved(int)),
          SIGNAL(keyRemoved(int)));

  layout->addWidget(m_spectrumbar);

  m_colorField = new ColorField(this, true, color, 36);
  connect(m_colorField, SIGNAL(colorChanged(const TPixel32 &, bool)), this,
          SLOT(onColorChanged(const TPixel32 &, bool)));
  layout->addWidget(m_colorField, 0, Qt::AlignLeft);

  setLayout(layout);
}

//-----------------------------------------------------------------------------
/*! Update all widget and emit keyPositionChanged() signal.
*/
void SpectrumField::onCurrentPosChanged(bool isDragging) {
  if (m_spectrumbar->getCurrentKeyIndex() == -1) return;
  update();
  emit keyPositionChanged(isDragging);
}

//-----------------------------------------------------------------------------
/*! Set color field to spectrum current key color and update.
*/
void SpectrumField::onCurrentKeyChanged() {
  if (m_spectrumbar->getCurrentKeyIndex() != -1)
    m_colorField->setColor(m_spectrumbar->getCurrentColor());
  update();
}

//-----------------------------------------------------------------------------
/*! Set spectrum current key color to \b color and emit keyColorChanged().
*/
void SpectrumField::onColorChanged(const TPixel32 &color, bool isDragging) {
  m_spectrumbar->setCurrentColor(color);
  emit keyColorChanged(isDragging);
}

//-----------------------------------------------------------------------------
/*! Paint an arrow to connect current spectrum key and square color field.
*/
void SpectrumField::paintEvent(QPaintEvent *e) {
  int curPos = m_spectrumbar->getCurrentPos();
  if (curPos == -1) return;
  QPainter p(this);
  int x0 = 18 + m_margin;
  int y0 = 2 * m_margin + WidgetHeight + m_spacing;
  int y  = m_margin + m_spacing +
          (WidgetHeight * 0.5 -
           4);  // 4 e' l'altezza della freccia nella spectrum bar
  int y1 = y0 - y * 0.5 + 1;
  int y2 = y0 - y;
  curPos += m_margin;
  p.setPen(Qt::black);
  p.drawLine(x0, y0, x0, y1);
  p.drawLine(x0, y1, curPos, y1);
  p.drawLine(curPos, y1, curPos, y2);
}

//-----------------------------------------------------------------------------

SpectrumField::~SpectrumField() {}
