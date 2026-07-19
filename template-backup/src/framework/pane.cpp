#include "pane.h"
#include "appcontext.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QPainter>
#include <QMouseEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

//=============================================================================
// TPanel
//-----------------------------------------------------------------------------

TPanel::TPanel(QWidget *parent, Qt::WindowFlags flags,
               TDockWidget::Orientation orientation)
    : TDockWidget(parent, flags) {
  m_panelTitleBar = new TPanelTitleBar(this, orientation);
  setTitleBarWidget(m_panelTitleBar);
  connect(m_panelTitleBar, &TPanelTitleBar::doubleClick, this,
          &TPanel::doubleClick);
  connect(m_panelTitleBar, &TPanelTitleBar::closeButtonPressed, this,
          &TPanel::onCloseButtonPressed);
  setOrientation(orientation);
}

//-----------------------------------------------------------------------------

TPanel::~TPanel() {
  // On quitting, save the floating panel's geometry and state in order to
  // restore them when opening the floating panel next time
  if (isFloating() && !isHidden()) {
    QString popupPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/popups.ini";
    QSettings settings(popupPath, QSettings::IniFormat);
    settings.beginGroup("Popups");
    settings.setValue(QString::fromStdString(m_panelType),
                      saveGeometry().toBase64());
    settings.endGroup();
  }
}

//-----------------------------------------------------------------------------

void TPanel::paintEvent(QPaintEvent *) {
  QPainter painter(this);

  if (widget()) {
    QRect dockRect = widget()->geometry();
    dockRect.adjust(0, 0, -1, -1);
    painter.setPen(Qt::black);
    painter.drawRect(dockRect);
  }

  painter.end();
}

//-----------------------------------------------------------------------------

void TPanel::onCloseButtonPressed() {
  emit closeButtonPressed();
  // Hide panel so OpenFloatingPanel can restore it later
  hide();
  if (parentLayout()) parentLayout()->removeWidget(this);
}

//-----------------------------------------------------------------------------

void TPanel::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);

  QAction* bindAction = menu.addAction(
      m_isRoomBound ? tr("Unbind from Room") : tr("Bind to Current Room"));
  QAction* selected = menu.exec(event->globalPos());

  if (selected == bindAction) {
    if (m_isRoomBound) {
      setRoomBound(false);
    } else {
      setRoomBound(true);
      m_boundRoomName = AppContext::instance()->currentRoomName();
    }
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanel::setRoomBound(bool bound) {
  m_isRoomBound = bound;
  if (!bound) m_boundRoomName.clear();
}

void TPanel::setBoundRoomName(const QString& roomName) {
  m_boundRoomName = roomName;
}

//-----------------------------------------------------------------------------
/*! activate the panel and set focus specified widget when mouse enters
 */
void TPanel::enterEvent(QEnterEvent *event) {
  // Only when application is active
  QWidget *w = QApplication::activeWindow();
  if (w) {
    // grab the focus, unless a line-edit is focused currently
    QWidget *focusWidget = QApplication::focusWidget();
    if (focusWidget && (qobject_cast<QLineEdit *>(focusWidget) ||
                        qobject_cast<QTextEdit *>(focusWidget))) {
      event->accept();
      return;
    }

    widgetFocusOnEnter();
    event->accept();
  } else
    event->accept();
}

//-----------------------------------------------------------------------------
/*! clear focus when mouse leaves
 */
void TPanel::leaveEvent(QEvent *event) {
  QWidget *focusWidget = QApplication::focusWidget();
  if (focusWidget && (qobject_cast<QLineEdit *>(focusWidget) ||
                      qobject_cast<QTextEdit *>(focusWidget))) {
    return;
  }
  widgetClearFocusOnLeave();
}

//-----------------------------------------------------------------------------
/*! load and restore previous geometry and state of the floating panel.
    called from the function OpenFloatingPanel::getOrOpenFloatingPanel()
    in floatingpanelcommand.cpp
*/
void TPanel::restoreFloatingPanelState() {
  QString popupPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/popups.ini";
  QSettings settings(popupPath, QSettings::IniFormat);
  settings.beginGroup("Popups");
  QByteArray geom = QByteArray::fromBase64(
      settings.value(QString::fromStdString(m_panelType)).toByteArray());
  settings.endGroup();

  if (!geom.isEmpty()) {
    restoreGeometry(geom);
  }
}

//-----------------------------------------------------------------------------

void TPanel::setFloatingAppearance() {
  TDockWidget::setFloatingAppearance();
}

//-----------------------------------------------------------------------------

void TPanel::setDockedAppearance() {
  TDockWidget::setDockedAppearance();
}

//=============================================================================
// TPanelTitleBarButton
//-----------------------------------------------------------------------------

TPanelTitleBarButton::TPanelTitleBarButton(QWidget *parent,
                                           const QString &standardPixmapName)
    : QWidget(parent)
    , m_standardPixmapName(standardPixmapName)
    , m_rollover(false)
    , m_pressed(false)
    , m_buttonSet(nullptr)
    , m_id(0) {
  setMouseTracking(true);

  // Determine button size
  m_baseSize = QSize(20, 18);
  if (m_standardPixmapName.contains(QLatin1String("preview"),
                                    Qt::CaseInsensitive)) {
    m_baseSize = QSize(30, 18);
  }

  // Set initial size
  setFixedSize(m_baseSize);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setButtonSet(TPanelTitleBarButtonSet *buttonSet,
                                        int id) {
  m_buttonSet = buttonSet;
  m_id        = id;
  m_buttonSet->add(this);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPressed(bool pressed) {
  if (pressed != m_pressed) {
    m_pressed = pressed;
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setOffColor(const QColor &color) {
  if (m_offColor != color) {
    m_offColor = color;
  }
}

QColor TPanelTitleBarButton::getOffColor() const {
  return m_offColor.isValid() ? m_offColor : Qt::transparent;
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setOverColor(const QColor &color) {
  if (m_overColor != color) {
    m_overColor = color;
  }
}

QColor TPanelTitleBarButton::getOverColor() const { return m_overColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPressedColor(const QColor &color) {
  if (m_pressedColor != color) {
    m_pressedColor = color;
  }
}

QColor TPanelTitleBarButton::getPressedColor() const { return m_pressedColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setFreezeColor(const QColor &color) {
  if (m_freezeColor != color) {
    m_freezeColor = color;
  }
}

QColor TPanelTitleBarButton::getFreezeColor() const { return m_freezeColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPreviewColor(const QColor &color) {
  if (m_previewColor != color) {
    m_previewColor = color;
  }
}

QColor TPanelTitleBarButton::getPreviewColor() const { return m_previewColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::paintEvent(QPaintEvent *) {
  if (m_standardPixmapName.isEmpty()) return;

  QPainter p(this);

  // Determine background color based on state
  QColor bgColor = getOffColor();

  if (m_pressed) {
    QColor tgtColor;
    if (m_standardPixmapName.contains(QLatin1String("freeze"))) {
      tgtColor = getFreezeColor();
    } else if (m_standardPixmapName.contains(QLatin1String("preview"))) {
      tgtColor = getPreviewColor();
    } else {
      tgtColor = getPressedColor();
    }

    // Only overwrite if stylesheet actually provided valid color
    if (tgtColor.isValid()) bgColor = tgtColor;
  } else if (m_rollover) {
    QColor overColor = getOverColor();
    // Only overwrite if valid
    if (overColor.isValid()) bgColor = overColor;
  }

  p.fillRect(rect(), bgColor);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::mouseMoveEvent(QMouseEvent *) {}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::enterEvent(QEnterEvent *) {
  if (!m_rollover) {
    m_rollover = true;
    if (!m_pressed) update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::leaveEvent(QEvent *) {
  if (m_rollover) {
    m_rollover = false;
    if (!m_pressed) update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::mousePressEvent(QMouseEvent *) {
  if (m_buttonSet) {
    if (m_pressed) return;
    m_buttonSet->select(this);
  } else {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

//=============================================================================
// TPanelTitleBarButtonSet
//-----------------------------------------------------------------------------

TPanelTitleBarButtonSet::TPanelTitleBarButtonSet() = default;

void TPanelTitleBarButtonSet::add(TPanelTitleBarButton *button) {
  m_buttons.push_back(button);
}

void TPanelTitleBarButtonSet::select(TPanelTitleBarButton *button) {
  for (auto *btn : m_buttons) btn->setPressed(button == btn);
  emit selected(button->getId());
}

//=============================================================================
// TPanelTitleBar
//-----------------------------------------------------------------------------

TPanelTitleBar::TPanelTitleBar(QWidget *parent,
                               TDockWidget::Orientation orientation)
    : QFrame(parent), m_closeButtonHighlighted(false) {
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

QSize TPanelTitleBar::minimumSizeHint() const { return QSize(20, 18); }

//-----------------------------------------------------------------------------

void TPanelTitleBar::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  const QRect r = this->rect();

  auto *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != nullptr);

  bool isPanelActive;
  if (!dw->isFloating()) {  // docked panel
    isPanelActive = dw->widgetInThisPanelIsFocused();
  } else {  // floating panel
    isPanelActive = isActiveWindow();
  }

  // Draw flat background
  QColor bgColor =
      isPanelActive ? QColor(55, 55, 55) : QColor(40, 40, 40);
  painter.fillRect(r, bgColor);

  if (dw->getOrientation() == TDockWidget::vertical) {
    const QString titleText = painter.fontMetrics().elidedText(
        dw->windowTitle(), Qt::ElideRight, r.width() - 50);

    QColor titleColor =
        isPanelActive ? m_activeTitleColor : m_titleColor;
    if (!titleColor.isValid())
      titleColor =
          isPanelActive ? QColor(200, 200, 200) : QColor(140, 140, 140);
    painter.setPen(titleColor);
    painter.drawText(QPointF(8, 13), titleText);
  }

  if (dw->isFloating()) {
    // Draw close button as a simple "X"
    const QPoint btnPos(r.right() - 19, r.top());

    if (m_closeButtonHighlighted) {
      QColor closeBg =
          m_closeOverColor.isValid() ? m_closeOverColor : QColor(200, 60, 60);
      painter.fillRect(QRect(btnPos.x(), btnPos.y(), 18, 18), closeBg);
    }

    painter.setPen(QColor(200, 200, 200));
    painter.drawText(QRect(btnPos.x(), btnPos.y(), 18, 18), Qt::AlignCenter,
                     QStringLiteral("✕"));
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::setCloseOverColor(const QColor &color) {
  if (m_closeOverColor != color) {
    m_closeOverColor = color;
  }
}

QColor TPanelTitleBar::getCloseOverColor() const { return m_closeOverColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBar::leaveEvent(QEvent *) {
  auto *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != nullptr);

  // Mouse left the widget, reset the highlighted flag
  if (dw->isFloating()) {
    m_closeButtonHighlighted = false;
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mousePressEvent(QMouseEvent *event) {
  auto *dw = static_cast<TDockWidget *>(parentWidget());

  const QPoint pos = event->pos();

  if (dw->isFloating()) {
    const QRect rect     = this->rect();
    const QRect closeButtonRect(rect.right() - 20, rect.top() + 1, 20, 18);
    if (closeButtonRect.contains(pos) && dw->isFloating()) {
      event->accept();
      dw->hide();
      m_closeButtonHighlighted = false;
      emit closeButtonPressed();
      return;
    }
  }
  event->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mouseMoveEvent(QMouseEvent *event) {
  auto *dw = static_cast<TDockWidget *>(parentWidget());

  if (dw->isFloating()) {
    const QPoint pos = event->pos();
    const QRect rect = this->rect();
    const QRect closeButtonRect(rect.right() - 18, rect.top() + 1, 18, 18);

    if (closeButtonRect.contains(pos) && dw->isFloating())
      m_closeButtonHighlighted = true;
    else
      m_closeButtonHighlighted = false;
  }

  update();
  event->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mouseDoubleClickEvent(QMouseEvent *me) {
  emit doubleClick(me);
  me->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::add(const QPoint &pos, QWidget *widget) {
  m_buttons.emplace_back(pos, widget);
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  for (const auto &[pos, widget] : m_buttons) {
    QPoint p = pos;
    if (p.x() < 0) p.setX(p.x() + width());
    widget->move(p);
  }
}

//=============================================================================
// TPanelFactory - using Q_GLOBAL_STATIC for thread-safe singleton map
//-----------------------------------------------------------------------------

typedef QMap<QString, TPanelFactory *> FactoryTable;
Q_GLOBAL_STATIC(FactoryTable, factoryTable)

//-----------------------------------------------------------------------------

QMap<QString, TPanelFactory *> &TPanelFactory::tableInstance() {
  return *factoryTable();
}

//-----------------------------------------------------------------------------

TPanelFactory::TPanelFactory(const QString &panelType)
    : m_panelType(panelType) {
  assert(tableInstance().count(panelType) == 0);
  tableInstance()[m_panelType] = this;
}

//-----------------------------------------------------------------------------

TPanelFactory::~TPanelFactory() { tableInstance().remove(m_panelType); }

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent, const QString &panelType) {
  auto it = tableInstance().find(panelType);
  if (it != tableInstance().end()) {
    TPanel *panel = it.value()->createPanel(parent);
    panel->setPanelType(panelType.toStdString());
    return panel;
  }

  // Fallback: generic panel
  TPanel *panel = new TPanel(parent);
  panel->setPanelType(panelType.toStdString());
  return panel;
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent) {
  auto *panel = new TPanel(parent);
  panel->setObjectName(getPanelType());
  panel->setWindowTitle(getPanelType());
  initialize(panel);

  return panel;
}

//-----------------------------------------------------------------------------
