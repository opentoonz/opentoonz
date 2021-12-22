

#include "toonzqt/dvtextedit.h"
#include "toonzqt/gutil.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPainter>
#include <QToolBar>
#include <QFontComboBox>
#include <QAction>
#include <QMouseEvent>
#include <QTextCursor>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>

#include "toonzqt/colorfield.h"

namespace {

//-----------------------------------------------------------------------------

qreal distanceBetweenPointandRect(const QRect &rect, const QPoint &point) {
  if (rect.contains(point)) return 0;

  qreal topLeftDistance =
      sqrt(pow((double)(point.x() - rect.topLeft().x()), 2) +
           pow((double)(point.y() - rect.topLeft().y()), 2));
  qreal topRightDistance =
      sqrt(pow((double)(point.x() - rect.topRight().x()), 2) +
           pow((double)(point.y() - rect.topRight().y()), 2));
  qreal bottomLeftDistance =
      sqrt(pow((double)(point.x() - rect.bottomLeft().x()), 2) +
           pow((double)(point.y() - rect.bottomLeft().y()), 2));
  qreal bottomRightDistance =
      sqrt(pow((double)(point.x() - rect.bottomRight().x()), 2) +
           pow((double)(point.y() - rect.bottomRight().y()), 2));

  qreal topMax    = std::min(topLeftDistance, topRightDistance);
  qreal bottomMax = std::min(bottomLeftDistance, bottomRightDistance);

  return std::min(topMax, bottomMax);
}
}

using namespace DVGui;

//=============================================================================
// DvMiniToolBar
//-----------------------------------------------------------------------------

DvMiniToolBar::DvMiniToolBar(QWidget *parent)
    : QFrame(parent), m_dragPos(0, 0) {
  setObjectName("DvMiniToolBar");
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
}

//-----------------------------------------------------------------------------

DvMiniToolBar::~DvMiniToolBar() {}

//-----------------------------------------------------------------------------

void DvMiniToolBar::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton)
    m_dragPos = e->globalPos() - frameGeometry().topLeft();

  QWidget::mousePressEvent(e);
}

//-----------------------------------------------------------------------------

void DvMiniToolBar::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::LeftButton) move(e->globalPos() - m_dragPos);

  QWidget::mouseMoveEvent(e);
}

//=============================================================================
// DvTextEditButton
//-----------------------------------------------------------------------------

DvTextEditButton::DvTextEditButton(QWidget *parent) : QWidget(parent) {
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setFixedSize(16, 16);
}

//-----------------------------------------------------------------------------

DvTextEditButton::~DvTextEditButton() {}

//-----------------------------------------------------------------------------

void DvTextEditButton::paintEvent(QPaintEvent *) {
  QPainter p(this);
  static QPixmap pixmap(":Resources/text_edit_button.png");
  p.drawPixmap(0, 0, width(), height(), pixmap);
}

//-----------------------------------------------------------------------------

void DvTextEditButton::mousePressEvent(QMouseEvent *) {
  emit clicked();
  hide();
}

//=============================================================================
// DvTextEdit
//-----------------------------------------------------------------------------

DvTextEdit::DvTextEdit(QWidget *parent)
    : QTextEdit(parent), m_miniToolBarEnabled(true), m_mousePos(0, 0) {
  setMouseTracking(true);
  createActions();
  createMiniToolBar();

  m_button = new DvTextEditButton(0);
  m_button->hide();
  connect(m_button, SIGNAL(clicked()), this, SLOT(onShowMiniToolBarClicked()));

  fontChanged(font());
  setTextColor(TPixel32::Black, false);
  alignmentChanged(alignment());

  connect(this, SIGNAL(currentCharFormatChanged(const QTextCharFormat &)), this,
          SLOT(onCurrentCharFormatChanged(const QTextCharFormat &)));

  connect(this, SIGNAL(cursorPositionChanged()), this,
          SLOT(onCursorPositionChanged()));

  connect(this, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
}

//-----------------------------------------------------------------------------
void DvTextEdit::changeFont(const QFont &f) {
  QTextCharFormat fmt = currentCharFormat();
  // bool aux = fmt.fontUnderline();
  fmt.setFont(f);
  // fmt.setFontHintingPreference(QFont::PreferFullHinting);
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------

DvTextEdit::~DvTextEdit() {}

//-----------------------------------------------------------------------------

void DvTextEdit::createActions() {
  m_boldAction = new QAction(createQIcon("bold"), tr("Bold"), this);
  m_boldAction->setCheckable(true);
  connect(m_boldAction, SIGNAL(triggered()), this, SLOT(setTextBold()));

  m_italicAction = new QAction(createQIcon("italic"), tr("Italic"), this);
  m_italicAction->setCheckable(true);
  connect(m_italicAction, SIGNAL(triggered()), this, SLOT(setTextItalic()));

  m_underlineAction =
      new QAction(createQIcon("underline"), tr("Underline"), this);
  m_underlineAction->setCheckable(true);
  connect(m_underlineAction, SIGNAL(triggered()), this,
          SLOT(setTextUnderline()));

  m_colorField = new DVGui::ColorField(this, true, TPixel32(), 30);
  m_colorField->hideChannelsFields(true);
  connect(m_colorField, SIGNAL(colorChanged(const TPixel32 &, bool)), this,
          SLOT(setTextColor(const TPixel32 &, bool)));

  m_alignActionGroup = new QActionGroup(this);
  connect(m_alignActionGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(setTextAlign(QAction *)));

  m_alignLeftAction = new QAction(createQIcon("align_left"), tr("Align Left"),
                                  m_alignActionGroup);
  m_alignLeftAction->setCheckable(true);
  m_alignCenterAction = new QAction(createQIcon("align_center"),
                                    tr("Align Center"), m_alignActionGroup);
  m_alignCenterAction->setCheckable(true);
  m_alignRightAction = new QAction(createQIcon("align_right"),
                                   tr("Align Right"), m_alignActionGroup);
  m_alignRightAction->setCheckable(true);
}

//-----------------------------------------------------------------------------

void DvTextEdit::createMiniToolBar() {
  m_miniToolBar = new DvMiniToolBar();

  QToolBar *toolBarUp = new QToolBar(m_miniToolBar);
  toolBarUp->setIconSize(QSize(16, 16));
  toolBarUp->setObjectName("toolOptionBar");
  toolBarUp->setFixedHeight(30);

  m_fontComboBox = new QFontComboBox(toolBarUp);
  m_fontComboBox->setMaximumHeight(20);
  m_fontComboBox->setMinimumWidth(140);

  connect(m_fontComboBox, SIGNAL(activated(const QString &)), this,
          SLOT(setTextFamily(const QString &)));

  m_sizeComboBox = new QComboBox(toolBarUp);
  m_sizeComboBox->setEditable(true);
  m_sizeComboBox->setMaximumHeight(20);
  m_sizeComboBox->setMinimumWidth(44);

  QFontDatabase db;
  for (int size : db.standardSizes())
    m_sizeComboBox->addItem(QString::number(size));

  connect(m_sizeComboBox, SIGNAL(activated(const QString &)), this,
          SLOT(setTextSize(const QString &)));

  toolBarUp->addWidget(m_fontComboBox);
  toolBarUp->addWidget(m_sizeComboBox);

  QToolBar *toolBarDown = new QToolBar(m_miniToolBar);
  toolBarDown->setIconSize(QSize(30, 30));
  toolBarDown->setObjectName("toolOptionBar");
  toolBarDown->setFixedHeight(30);

  toolBarDown->setIconSize(QSize(30, 30));

  toolBarDown->addWidget(m_colorField);
  toolBarDown->addSeparator();
  toolBarDown->addAction(m_boldAction);
  toolBarDown->addAction(m_italicAction);
  toolBarDown->addAction(m_underlineAction);
  toolBarDown->addSeparator();
  toolBarDown->addActions(m_alignActionGroup->actions());

  QVBoxLayout *m_mainLayout = new QVBoxLayout(m_miniToolBar);
  m_mainLayout->setSizeConstraint(QLayout::SetFixedSize);
  m_mainLayout->setMargin(2);
  m_mainLayout->setSpacing(2);
  m_mainLayout->addWidget(toolBarUp);
  m_mainLayout->addWidget(toolBarDown);

  m_miniToolBar->setLayout(m_mainLayout);
}

//-----------------------------------------------------------------------------

void DvTextEdit::mouseReleaseEvent(QMouseEvent *e) {
  QTextEdit::mouseReleaseEvent(e);

  if (e->button() == Qt::LeftButton && m_miniToolBarEnabled) {
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection()) {
      cursor.setPosition(cursor.selectionEnd(), QTextCursor::MoveAnchor);
      QRect r          = cursorRect(cursor);
      QPoint buttonPos = mapToGlobal(r.topRight());
      buttonPos.setX(buttonPos.x() - r.width() * 0.5 + 2);
      buttonPos.setY(buttonPos.y() - (m_button->height() - r.height()) * 0.5);
      m_button->move(buttonPos);
      m_button->show();
    }
  }
}

//-----------------------------------------------------------------------------

void DvTextEdit::mousePressEvent(QMouseEvent *e) {
  QTextEdit::mousePressEvent(e);
  if (m_miniToolBarEnabled && m_miniToolBar->isVisible()) hideMiniToolBar();
}

//-----------------------------------------------------------------------------

void DvTextEdit::mouseMoveEvent(QMouseEvent *event) {
  QTextEdit::mouseMoveEvent(event);

  m_mousePos = event->pos();
}

//-----------------------------------------------------------------------------

void DvTextEdit::wheelEvent(QWheelEvent *e) {
  QTextEdit::wheelEvent(e);
  m_button->hide();
}

//-----------------------------------------------------------------------------

void DvTextEdit::focusOutEvent(QFocusEvent *e) {
  QTextEdit::focusOutEvent(e);
  if (m_miniToolBar->isVisible() && !m_miniToolBar->underMouse()) {
    hideMiniToolBar();
  }
  m_button->hide();
}

//-----------------------------------------------------------------------------

void DvTextEdit::focusInEvent(QFocusEvent *e) {
  QTextEdit::focusInEvent(e);
  emit focusIn();
}

//-----------------------------------------------------------------------------

void DvTextEdit::dragMoveEvent(QDragMoveEvent *e) {
  // Se inizio un drag e la toolbar è visibile la nascondo
  if (m_miniToolBar->isVisible()) hideMiniToolBar();

  m_button->hide();
  QTextEdit::dragMoveEvent(e);
}

//-----------------------------------------------------------------------------

void DvTextEdit::showMiniToolBar(const QPoint &pos) {
  if (m_miniToolBarEnabled) {
    m_miniToolBar->move(pos);
    m_miniToolBar->show();
  }
}

//-----------------------------------------------------------------------------

void DvTextEdit::hideMiniToolBar() {
  // Hide the miniToolBar
  m_miniToolBar->hide();
  // Store the current Font in the application
  QTextCursor cursor = textCursor();
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextBold() {
  QTextCharFormat fmt;
  fmt.setFontWeight(m_boldAction->isChecked() ? QFont::Bold : QFont::Normal);
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextItalic() {
  QTextCharFormat fmt;
  fmt.setFontItalic(m_italicAction->isChecked());
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextUnderline() {
  QTextCharFormat fmt;
  fmt.setFontUnderline(m_underlineAction->isChecked());
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextColor(const TPixel32 &color, bool isDragging) {
  if (isDragging) return;
  QColor col(color.r, color.g, color.b, color.m);
  QTextCharFormat fmt;
  fmt.setForeground(col);
  mergeFormatOnWordOrSelection(fmt);
  colorChanged(col);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextFamily(const QString &f) {
  QTextCharFormat fmt;
  fmt.setFontFamily(f);
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextSize(const QString &p) {
  QTextCharFormat fmt;
  fmt.setFontPointSize(p.toFloat());
  mergeFormatOnWordOrSelection(fmt);
}

//-----------------------------------------------------------------------------

void DvTextEdit::setTextAlign(QAction *a) {
  if (a == m_alignLeftAction)
    setAlignment(Qt::AlignLeft);
  else if (a == m_alignCenterAction)
    setAlignment(Qt::AlignHCenter);
  else if (a == m_alignRightAction)
    setAlignment(Qt::AlignRight);
}

//-----------------------------------------------------------------------------

void DvTextEdit::mergeFormatOnWordOrSelection(const QTextCharFormat &format) {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
  cursor.mergeCharFormat(format);
  mergeCurrentCharFormat(format);
  emit textChanged();
}

//-----------------------------------------------------------------------------

void DvTextEdit::fontChanged(const QFont &f) {
  m_fontComboBox->setCurrentIndex(
      m_fontComboBox->findText(QFontInfo(f).family()));
  m_sizeComboBox->setCurrentIndex(
      m_sizeComboBox->findText(QString::number(f.pointSize())));
  m_boldAction->setChecked(f.bold());
  m_italicAction->setChecked(f.italic());
  m_underlineAction->setChecked(f.underline());
}

//-----------------------------------------------------------------------------

void DvTextEdit::colorChanged(const QColor &c) {
  m_colorField->setColor(TPixel32(c.red(), c.green(), c.blue(), c.alpha()));
}

//-----------------------------------------------------------------------------

void DvTextEdit::alignmentChanged(Qt::Alignment a) {
  if (a & Qt::AlignLeft) {
    m_alignLeftAction->setChecked(true);
  } else if (a & Qt::AlignHCenter) {
    m_alignCenterAction->setChecked(true);
  } else if (a & Qt::AlignRight) {
    m_alignRightAction->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void DvTextEdit::onCurrentCharFormatChanged(const QTextCharFormat &format) {
  fontChanged(format.font());
  colorChanged(format.foreground().color());
}

//-----------------------------------------------------------------------------

void DvTextEdit::onCursorPositionChanged() { alignmentChanged(alignment()); }

//-----------------------------------------------------------------------------

void DvTextEdit::onShowMiniToolBarClicked() {
  showMiniToolBar(m_button->pos());
}

//-----------------------------------------------------------------------------

void DvTextEdit::onSelectionChanged() {
  QTextCursor cursor = textCursor();
  if (!cursor.hasSelection()) m_button->hide();
}

//-----------------------------------------------------------------------------
