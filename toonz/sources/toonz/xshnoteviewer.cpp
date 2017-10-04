

#include "xshnoteviewer.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txshnoteset.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheethandle.h"
#include "orientation.h"

// Qt includes
#include <QVariant>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QMainWindow>
#include <QButtonGroup>
#include <QComboBox>

namespace {

QString computeBackgroundStyleSheetString(QColor color) {
  QVariant vR(color.red());
  QVariant vG(color.green());
  QVariant vB(color.blue());
  QVariant vA(color.alpha());
  return QString("#noteTextEdit { border-image: 0; background: rgbm(") +
         vR.toString() + QString(",") + vG.toString() + QString(",") +
         vB.toString() + QString(",") + vA.toString() + QString("); }");
}
}

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// NotePopup
//-----------------------------------------------------------------------------

NotePopup::NotePopup(XsheetViewer *viewer, int noteIndex)
    : DVGui::Dialog(TApp::instance()->getMainWindow(), false, false,
                    "NotePopup")
    , m_viewer(viewer)
    , m_noteIndex(noteIndex)
    , m_currentColorIndex(0)
    , m_isOneButtonPressed(false) {
  setWindowTitle(tr("Memo"));
  bool ret = true;

  setMinimumSize(330, 180);

  setTopMargin(0);

  beginVLayout();

  QGridLayout *layout = new QGridLayout(this);
  layout->setMargin(1);
  layout->setColumnStretch(7, 10);
  layout->setColumnStretch(8, 10);
  int row = 0;
  int col = 0;

  QString text;
  TPixel32 color;
  int colorIndex = 0;
  if (m_noteIndex < 0) {
    text  = QString();
    color = getColors().at(0);
  } else {
    text                = getNotes()->getNoteHtmlText(m_noteIndex);
    m_currentColorIndex = getNotes()->getNoteColorIndex(m_noteIndex);
    color               = getColors().at(m_currentColorIndex);
  }

  m_textEditor = new DVGui::DvTextEdit(this);
  m_textEditor->setObjectName("noteTextEdit");
  m_textEditor->setHtml(text);
  m_textEditor->setStyleSheet(computeBackgroundStyleSheetString(
      QColor(color.r, color.g, color.b, color.m)));
  layout->addWidget(m_textEditor, row, col, 1, 9);
  layout->setRowStretch(row, 10);
  ret = ret && connect(m_textEditor, SIGNAL(focusIn()), this,
                       SLOT(onTextEditFocusIn()));
  row++;

  int index                 = 0;
  DVGui::ColorField *color1 = createColorField(index);
  ret = ret && connect(color1, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor1Switched(const TPixel32 &, bool)));
  layout->addWidget(color1, row, index, 1, 1);
  m_colorFields.push_back(color1);
  index++;
  DVGui::ColorField *color2 = createColorField(index);
  ret = ret && connect(color2, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor2Switched(const TPixel32 &, bool)));
  layout->addWidget(color2, row, index, 1, 1);
  m_colorFields.push_back(color2);
  index++;
  DVGui::ColorField *color3 = createColorField(index);
  ret = ret && connect(color3, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor3Switched(const TPixel32 &, bool)));
  layout->addWidget(color3, row, index, 1, 1);
  m_colorFields.push_back(color3);
  index++;
  DVGui::ColorField *color4 = createColorField(index);
  ret = ret && connect(color4, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor4Switched(const TPixel32 &, bool)));
  layout->addWidget(color4, row, index, 1, 1);
  m_colorFields.push_back(color4);
  index++;
  DVGui::ColorField *color5 = createColorField(index);
  ret = ret && connect(color5, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor5Switched(const TPixel32 &, bool)));
  layout->addWidget(color5, row, index, 1, 1);
  m_colorFields.push_back(color5);
  index++;
  DVGui::ColorField *color6 = createColorField(index);
  ret = ret && connect(color6, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor6Switched(const TPixel32 &, bool)));
  layout->addWidget(color6, row, index, 1, 1);
  m_colorFields.push_back(color6);
  index++;
  DVGui::ColorField *color7 = createColorField(index);
  ret = ret && connect(color7, SIGNAL(editingChanged(const TPixel32 &, bool)),
                       SLOT(onColor7Switched(const TPixel32 &, bool)));
  layout->addWidget(color7, row, index, 1, 1);
  m_colorFields.push_back(color7);

  col = index;
  col++;

  QPushButton *addNoteButton = new QPushButton(tr("Post"));
  addNoteButton->setMinimumSize(50, 20);
  ret = ret &&
        connect(addNoteButton, SIGNAL(pressed()), this, SLOT(onNoteAdded()));
  layout->addWidget(addNoteButton, row, col, 1, 1);
  col++;
  QPushButton *discardNoteButton = new QPushButton(tr("Discard"));
  discardNoteButton->setMinimumSize(50, 20);
  ret = ret && connect(discardNoteButton, SIGNAL(pressed()), this,
                       SLOT(onNoteDiscarded()));
  layout->addWidget(discardNoteButton, row, col, 1, 1);
  addLayout(layout, false);

  endVLayout();

  // bad patch to bug:
  // when you write  some text to an existing note, appending it, the resulting
  // html is corrupted and nothing is shown. setting a font (for example) fix
  // it!
  m_textEditor->changeFont(QFont("Arial", 10));

  assert(ret);
}

//-----------------------------------------------------------------------------

void NotePopup::setCurrentNoteIndex(int index) { m_noteIndex = index; }

//-----------------------------------------------------------------------------

void NotePopup::update() {
  TPixel32 color = getColors().at(m_currentColorIndex);
  m_textEditor->setStyleSheet(computeBackgroundStyleSheetString(
      QColor(color.r, color.g, color.b, color.m)));
  DVGui::Dialog::update();
}

//-----------------------------------------------------------------------------

DVGui::ColorField *NotePopup::createColorField(int index) {
  TPixel32 color                = getColors().at(index);
  DVGui::ColorField *colorField = new DVGui::ColorField(this, true, color, 20);
  colorField->hideChannelsFields(true);
  bool ret = connect(colorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                     SLOT(onColorChanged(const TPixel32 &, bool)));
  assert(ret);
  return colorField;
}

//-----------------------------------------------------------------------------

void NotePopup::updateColorField() {
  int i;
  for (i = 0; i < m_colorFields.size(); i++) {
    DVGui::ColorField *colorField = m_colorFields.at(i);
    colorField->setEditingChangeNotified(false);
    if (m_currentColorIndex == i)
      colorField->setIsEditing(true);
    else
      colorField->setIsEditing(false);
    colorField->setEditingChangeNotified(true);
  }
}

//-----------------------------------------------------------------------------

void NotePopup::showEvent(QShowEvent *) {
  if (m_noteIndex == -1) m_textEditor->clear();
  m_isOneButtonPressed = false;
  updateColorField();
  onXsheetSwitched();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  bool ret = connect(xsheetHandle, SIGNAL(xsheetSwitched()), this,
                     SLOT(onXsheetSwitched()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void NotePopup::hideEvent(QHideEvent *) {
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  disconnect(xsheetHandle, SIGNAL(xsheetSwitched()), this,
             SLOT(onXsheetSwitched()));
}

//-----------------------------------------------------------------------------

void NotePopup::onColorChanged(const TPixel32 &color, bool isDragging) {
  if (!isDragging)
    m_textEditor->setStyleSheet(computeBackgroundStyleSheetString(
        QColor(color.r, color.g, color.b, color.m)));

  TSceneProperties *sp = m_viewer->getXsheet()->getScene()->getProperties();
  sp->setNoteColor(color, m_currentColorIndex);
  m_viewer->updateNoteWidgets();
}

//-----------------------------------------------------------------------------

TXshNoteSet *NotePopup::getNotes() { return m_viewer->getXsheet()->getNotes(); }

//-----------------------------------------------------------------------------

QList<TPixel32> NotePopup::getColors() {
  return m_viewer->getXsheet()->getScene()->getProperties()->getNoteColors();
}

//-----------------------------------------------------------------------------

void NotePopup::onColorFieldEditingChanged(const TPixel32 &color,
                                           bool isEditing, int index) {
  if (!isEditing) return;
  m_textEditor->setStyleSheet(computeBackgroundStyleSheetString(
      QColor(color.r, color.g, color.b, color.m)));
  DVGui::ColorField *colorField = m_colorFields.at(m_currentColorIndex);
  colorField->setEditingChangeNotified(false);
  colorField->setIsEditing(false);
  colorField->setEditingChangeNotified(true);
  m_currentColorIndex = index;
}

//-----------------------------------------------------------------------------

void NotePopup::onColor1Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 0);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor2Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 1);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor3Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 2);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor4Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 3);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor5Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 4);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor6Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 5);
}

//-----------------------------------------------------------------------------

void NotePopup::onColor7Switched(const TPixel32 &color, bool isEditing) {
  onColorFieldEditingChanged(color, isEditing, 6);
}

//-----------------------------------------------------------------------------

void NotePopup::onNoteAdded() {
  if (m_isOneButtonPressed) return;
  m_isOneButtonPressed = true;
  int newNoteIndex     = m_noteIndex;
  if (m_noteIndex < 0) {
    TXshNoteSet::Note note;
    note.m_col   = m_viewer->getCurrentColumn();
    note.m_row   = m_viewer->getCurrentRow();
    newNoteIndex = getNotes()->addNote(note);
  }

  getNotes()->setNoteHtmlText(newNoteIndex, m_textEditor->toHtml());
  getNotes()->setNoteColorIndex(newNoteIndex, m_currentColorIndex);

  m_viewer->setCurrentNoteIndex(newNoteIndex);
  m_viewer->updateCells();

  close();
}

//-----------------------------------------------------------------------------

void NotePopup::onNoteDiscarded() {
  if (m_isOneButtonPressed) return;
  m_isOneButtonPressed = true;
  if (m_noteIndex < 0) {
    hide();
    return;
  }
  getNotes()->removeNote(m_noteIndex);
  hide();

  m_viewer->discardNoteWidget();
}

//-----------------------------------------------------------------------------

void NotePopup::onTextEditFocusIn() {
  m_viewer->setCurrentNoteIndex(m_noteIndex);
}

//-----------------------------------------------------------------------------

void NotePopup::onXsheetSwitched() {
  TXshNoteSet *notes = getNotes();
  if (notes->getCount() == 0 || m_noteIndex == -1) return;
  m_currentColorIndex = getNotes()->getNoteColorIndex(m_noteIndex);
  TPixel32 color      = getColors().at(m_currentColorIndex);
  m_textEditor->setStyleSheet(computeBackgroundStyleSheetString(
      QColor(color.r, color.g, color.b, color.m)));
  m_textEditor->setHtml(notes->getNoteHtmlText(m_noteIndex));
}

//=============================================================================
// NoteWidget
//-----------------------------------------------------------------------------

NoteWidget::NoteWidget(XsheetViewer *parent, int noteIndex)
    : QWidget(parent)
    , m_viewer(parent)
    , m_noteIndex(noteIndex)
    , m_isHovered(false) {
  int width = (m_noteIndex < 0) ? 40 : NoteWidth;
  setFixedSize(width, NoteHeight);
}

//-----------------------------------------------------------------------------

void NoteWidget::paint(QPainter *painter, QPoint pos, bool isCurrent) {
  painter->translate(pos);
  QRect rect = m_viewer->orientation()->rect(PredefinedRect::NOTE_ICON);

  TXshNoteSet *notes   = m_viewer->getXsheet()->getNotes();
  TSceneProperties *sp = m_viewer->getXsheet()->getScene()->getProperties();
  TPixel32 c;
  if (m_noteIndex < 0)
    c.m = 0;
  else
    c = sp->getNoteColor(notes->getNoteColorIndex(m_noteIndex));
  QColor color(c.r, c.g, c.b, c.m);
  QPoint topLeft = rect.topLeft();
  int d          = 8;
  if (isCurrent)
    painter->setPen(Qt::white);
  else
    painter->setPen(Qt::black);
  QPainterPath rectPath(topLeft + QPoint(d, 0));
  rectPath.lineTo(rect.topRight());
  rectPath.lineTo(rect.bottomRight());
  rectPath.lineTo(rect.bottomLeft());
  rectPath.lineTo(topLeft + QPoint(0, d));
  rectPath.lineTo(topLeft + QPoint(d, 0));
  painter->fillPath(rectPath, QBrush(color));
  painter->drawPath(rectPath);

  color = QColor(tcrop(c.r - 20, 0, 255), tcrop(c.g - 20, 0, 255),
                 tcrop(c.b - 20, 0, 255), tcrop(c.m - 20, 0, 255));
  QPainterPath path(topLeft + QPoint(d, 0));
  path.lineTo(topLeft + QPoint(d, d));
  path.lineTo(topLeft + QPoint(0, d));
  path.lineTo(topLeft + QPoint(d, 0));
  painter->fillPath(path, QBrush(color));
  painter->drawPath(path);

  painter->setPen(Qt::black);
  if (m_noteIndex >= 0) {
    QTextDocument document;
    document.setHtml(notes->getNoteHtmlText(m_noteIndex));
    painter->translate(d, -1);
    document.drawContents(painter, rect.adjusted(0, 0, -2 * d, 0));
    painter->translate(-d, 1);
  } else
    painter->drawText(rect.adjusted(d + 1, 1, -d, 0),
                      Qt::AlignCenter | Qt::AlignTop, QString("+"));

  painter->translate(-pos);
}

//-----------------------------------------------------------------------------

void NoteWidget::openNotePopup() {
  if (!m_noteEditor) {
    m_noteEditor.reset(new XsheetGUI::NotePopup(m_viewer, m_noteIndex));
  }

  if (m_noteEditor->isVisible()) {
    m_noteEditor->activateWindow();
  } else {
    m_noteEditor->show();
  }
}

//-----------------------------------------------------------------------------

void NoteWidget::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  paint(&p);
}

//=============================================================================
// NoteArea
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
NoteArea::NoteArea(XsheetViewer *parent, Qt::WindowFlags flags)
#else
NoteArea::NoteArea(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QFrame(parent)
    , m_viewer(parent)
    , m_flipOrientationButton(nullptr)
    , m_noteButton(nullptr)
    , m_precNoteButton(nullptr)
    , m_nextNoteButton(nullptr)
    , m_frameDisplayStyleCombo(nullptr)
    , m_layerHeaderPanel(nullptr) {

  setFrameStyle(QFrame::StyledPanel);
  setObjectName("cornerWidget");

  m_flipOrientationButton =
      new QPushButton(m_viewer->orientation()->name(), this);
  m_noteButton             = new QToolButton(this);
  m_precNoteButton         = new QToolButton(this);
  m_nextNoteButton         = new QToolButton(this);
  m_frameDisplayStyleCombo = new QComboBox(this);
  m_layerHeaderPanel       = new LayerHeaderPanel(m_viewer, this);

  //-----

  m_flipOrientationButton->setObjectName("flipOrientationButton");
  m_flipOrientationButton->setFocusPolicy(Qt::FocusPolicy::NoFocus);

  m_noteButton->setObjectName("ToolbarToolButton");
  m_noteButton->setFixedSize(44, 26);
  m_noteButton->setIconSize(QSize(38, 20));
  QIcon addNoteIcon = createQIcon("newmemo");
  addNoteIcon.addFile(QString(":Resources/newmemo_disabled.svg"), QSize(),
                      QIcon::Disabled);
  m_noteButton->setIcon(addNoteIcon);

  m_precNoteButton->setObjectName("ToolbarToolButton");
  m_precNoteButton->setFixedSize(22, 22);
  m_precNoteButton->setIconSize(QSize(17, 17));
  QIcon precNoteIcon = createQIcon("prevkey");
  precNoteIcon.addFile(QString(":Resources/prevkey_disabled.svg"), QSize(),
                       QIcon::Disabled);
  m_precNoteButton->setIcon(precNoteIcon);

  m_nextNoteButton->setObjectName("ToolbarToolButton");
  m_nextNoteButton->setFixedSize(22, 22);
  m_nextNoteButton->setIconSize(QSize(17, 17));
  QIcon nextNoteIcon = createQIcon("nextkey");
  nextNoteIcon.addFile(QString(":Resources/nextkey_disabled.svg"), QSize(),
                       QIcon::Disabled);
  m_nextNoteButton->setIcon(nextNoteIcon);

  QStringList frameDisplayStyles;
  frameDisplayStyles << tr("Frame") << tr("Sec Frame") << tr("6sec Sheet")
                     << tr("3sec Sheet");
  m_frameDisplayStyleCombo->addItems(frameDisplayStyles);
  m_frameDisplayStyleCombo->setCurrentIndex(
      (int)m_viewer->getFrameDisplayStyle());

  // layout
  createLayout();

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_flipOrientationButton, SIGNAL(clicked()),
                       SLOT(flipOrientation()));

  ret = ret && connect(m_noteButton, SIGNAL(clicked()), SLOT(toggleNewNote()));
  ret = ret &&
        connect(m_precNoteButton, SIGNAL(clicked()), this, SLOT(precNote()));
  ret = ret &&
        connect(m_nextNoteButton, SIGNAL(clicked()), this, SLOT(nextNote()));

  ret =
      ret && connect(m_frameDisplayStyleCombo, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(onFrameDisplayStyleChanged(int)));

  ret = ret && connect(m_viewer, &XsheetViewer::orientationChanged, this,
                       &NoteArea::onXsheetOrientationChanged);

  updateButtons();

  assert(ret);
}

//-----------------------------------------------------------------------------

void NoteArea::removeLayout() {
  QLayout *currentLayout = layout();
  if (!currentLayout) return;

  currentLayout->removeWidget(m_flipOrientationButton);
  currentLayout->removeWidget(m_noteButton);
  currentLayout->removeWidget(m_precNoteButton);
  currentLayout->removeWidget(m_nextNoteButton);
  currentLayout->removeWidget(m_frameDisplayStyleCombo);
  currentLayout->removeWidget(m_layerHeaderPanel);
  delete currentLayout;
}

void NoteArea::createLayout() {
  const Orientation *o = m_viewer->orientation();
  QRect rect           = o->rect(PredefinedRect::NOTE_AREA);

  setFixedSize(rect.size());

  if (o->isVerticalTimeline())
    m_noteButton->setFixedSize(44, 26);
  else
    m_noteButton->setFixedSize(44, 22);

  // has two elements: main layout and header panel
  QVBoxLayout *panelLayout = new QVBoxLayout();
  panelLayout->setMargin(1);
  panelLayout->setSpacing(0);
  {
    QBoxLayout *mainLayout = new QBoxLayout(QBoxLayout::Direction(
        o->dimension(PredefinedDimension::QBOXLAYOUT_DIRECTION)));
    Qt::AlignmentFlag centerAlign =
        Qt::AlignmentFlag(o->dimension(PredefinedDimension::CENTER_ALIGN));
    mainLayout->setMargin(1);
    mainLayout->setSpacing(0);
    {
      mainLayout->addWidget(m_flipOrientationButton, 0, centerAlign);

      mainLayout->addStretch(1);

      mainLayout->addWidget(m_noteButton, 0, centerAlign);

      QHBoxLayout *buttonsLayout = new QHBoxLayout();
      buttonsLayout->setMargin(0);
      buttonsLayout->setSpacing(0);
      {
        buttonsLayout->addStretch(1);
        buttonsLayout->addWidget(m_precNoteButton, 0);
        buttonsLayout->addWidget(m_nextNoteButton, 0);
        buttonsLayout->addStretch(1);
      }
      mainLayout->addLayout(buttonsLayout, 0);

      mainLayout->addStretch(1);

      mainLayout->addWidget(m_frameDisplayStyleCombo, 0);
    }
    panelLayout->addLayout(mainLayout);

    panelLayout->addWidget(m_layerHeaderPanel);
  }
  setLayout(panelLayout);

  m_layerHeaderPanel->showOrHide(o);
}

//-----------------------------------------------------------------------------

void NoteArea::updateButtons() {
  TXshNoteSet *notes = m_viewer->getXsheet()->getNotes();

  int count = notes->getCount();
  m_nextNoteButton->setEnabled(false);
  m_precNoteButton->setEnabled(false);
  if (count == 0) return;
  int currentNoteIndex = m_viewer->getCurrentNoteIndex();
  if (currentNoteIndex == -1 || count == 1)
    m_nextNoteButton->setEnabled(true);
  else {
    if (count > currentNoteIndex + 1) m_nextNoteButton->setEnabled(true);
    if (currentNoteIndex > 0) m_precNoteButton->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void NoteArea::flipOrientation() { m_viewer->flipOrientation(); }

void NoteArea::onXsheetOrientationChanged(const Orientation *newOrientation) {
  m_flipOrientationButton->setText(newOrientation->caption());
  removeLayout();
  createLayout();
}

//-----------------------------------------------------------------------------

void NoteArea::toggleNewNote() {
  if (!m_newNotePopup)
    m_newNotePopup.reset(new XsheetGUI::NotePopup(m_viewer, -1));

  if (m_newNotePopup->isVisible()) {
    m_newNotePopup->activateWindow();
  } else {
    m_newNotePopup->show();
  }
}

//-----------------------------------------------------------------------------

void NoteArea::nextNote() {
  int currentNoteIndex = m_viewer->getCurrentNoteIndex();
  TXshNoteSet *notes   = m_viewer->getXsheet()->getNotes();
  // Se ho una sola nota la rendo corrente e la visualizzo
  if (notes->getCount() == 1) {
    m_viewer->setCurrentNoteIndex(0);
    return;
  }

  m_viewer->setCurrentNoteIndex(++currentNoteIndex);
}

//-----------------------------------------------------------------------------

void NoteArea::precNote() {
  int currentNoteIndex = m_viewer->getCurrentNoteIndex();
  m_viewer->setCurrentNoteIndex(--currentNoteIndex);
}

//-----------------------------------------------------------------------------

void NoteArea::onFrameDisplayStyleChanged(int id) {
  m_viewer->setFrameDisplayStyle((XsheetViewer::FrameDisplayStyle)id);
  m_viewer->updateRows();
}

//-----------------------------------------------------------------------------

}  // namespace XsheetGUI;
