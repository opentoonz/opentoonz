

#include "pane.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "tenv.h"
#include "saveloadqsettings.h"
#include "custompanelmanager.h"

#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tscenehandle.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QPainter>
#include <QStyleOptionDockWidget>
#include <QMouseEvent>
#include <QMainWindow>
#include <QSettings>
#include <QToolBar>
#include <QMap>
#include <QApplication>
#include <QFile>
#include <qdrawutil.h>
#include <assert.h>
#include <QDesktopWidget>
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QScreen>

extern TEnv::StringVar EnvSafeAreaName;
extern TEnv::IntVar EnvViewerPreviewBehavior;

//=============================================================================
// TPanel
//-----------------------------------------------------------------------------

TPanel::TPanel(QWidget *parent, Qt::WindowFlags flags,
               TDockWidget::Orientation orientation)
    : TDockWidget(parent, flags)
    , m_panelType("")
    , m_isMaximizable(true)
    , m_isMaximized(false)
    , m_panelTitleBar(0)
    , m_multipleInstancesAllowed(true) {
  // setFeatures(QDockWidget::DockWidgetMovable |
  // QDockWidget::DockWidgetFloatable);
  // setFloating(false);
  m_panelTitleBar = new TPanelTitleBar(this, orientation);
  setTitleBarWidget(m_panelTitleBar);
  // connect(m_panelTitleBar,SIGNAL(doubleClick()),this,SLOT(onDoubleClick()));
  connect(m_panelTitleBar, SIGNAL(doubleClick(QMouseEvent *)), this,
          SIGNAL(doubleClick(QMouseEvent *)));
  connect(m_panelTitleBar, SIGNAL(closeButtonPressed()), this,
          SLOT(onCloseButtonPressed()));
  setOrientation(orientation);
}

//-----------------------------------------------------------------------------

TPanel::~TPanel() {
  // On quitting, save the floating panel's geometry and state in order to
  // restore them when opening the floating panel next time
  if (isFloating()) {
    TFilePath savePath =
        ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
    QSettings settings(QString::fromStdWString(savePath.getWideString()),
                       QSettings::IniFormat);
    settings.beginGroup("Panels");
    settings.beginGroup(QString::fromStdString(m_panelType));
    settings.setValue("geometry", geometry());
    if (SaveLoadQSettings *persistent =
            dynamic_cast<SaveLoadQSettings *>(widget()))
      persistent->save(settings);
  }
}

//-----------------------------------------------------------------------------

void TPanel::paintEvent(QPaintEvent *e) {
  QPainter painter(this);

  if (widget()) {
    QRect dockRect = widget()->geometry();

    dockRect.adjust(0, 0, -1, -1);
    painter.fillRect(dockRect, m_bgcolor);
    painter.setPen(Qt::black);
    painter.drawRect(dockRect);
  }

  painter.end();
}

//-----------------------------------------------------------------------------

void TPanel::onCloseButtonPressed() {
  emit closeButtonPressed();

  // Currently, Toonz panels that get closed indeed just remain hidden -
  // ready to reappair if they are needed again. However, the user expects
  // a new panel to be created - so we just reset the panel here.
  // reset();    //Moved to panel invocation in floatingpanelcommand.cpp

  // Also, remove widget from its dock layout control
  if (parentLayout()) parentLayout()->removeWidget(this);
}

//-----------------------------------------------------------------------------
/*! activate the panel and set focus specified widget when mouse enters
 */
void TPanel::enterEvent(QEvent *event) {
  // Only when Toonz application is active
  QWidget *w = qApp->activeWindow();
  if (w) {
    // grab the focus, unless a line-edit is focused currently
    QWidget *focusWidget = qApp->focusWidget();
    if (focusWidget && (dynamic_cast<QLineEdit *>(focusWidget) ||
                        dynamic_cast<QTextEdit *>(focusWidget))) {
      event->accept();
      return;
    }

    widgetFocusOnEnter();

    // Some panels (e.g. Viewer, StudioPalette, Palette, ColorModel) are
    // activated when mouse enters. Viewer is activatable only when being
    // docked.
    // Active windows will NOT switch when the current active window is dialog.
    if (qobject_cast<QDialog *>(w) == 0 && isActivatableOnEnter())
      activateWindow();
    event->accept();
  } else
    event->accept();
}

//-----------------------------------------------------------------------------
/*! clear focus when mouse leaves
 */
void TPanel::leaveEvent(QEvent *event) {
  QWidget *focusWidget = qApp->focusWidget();
  if (focusWidget && dynamic_cast<QLineEdit *>(focusWidget)) {
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
  TFilePath savePath = ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup("Panels");

  if (!settings.childGroups().contains(QString::fromStdString(m_panelType)))
    return;

  settings.beginGroup(QString::fromStdString(m_panelType));

  QRect geom = settings.value("geometry", saveGeometry()).toRect();
  // check if it can be visible in the current screen
  if (!(geom & QApplication::desktop()->availableGeometry(this)).isEmpty())
    setGeometry(geom);
  // load optional settings
  if (SaveLoadQSettings *persistent =
          dynamic_cast<SaveLoadQSettings *>(widget()))
    persistent->load(settings);
}

//-----------------------------------------------------------------------------
// if the panel has no contents to be zoomed, simply resize the panel here
// currently only Flipbook and Color Model panels support resizing of contents
void TPanel::zoomContentsAndFitGeometry(bool forward) {
  if (!m_floating) return;

  auto getScreen = [&]() {
    QScreen *ret = nullptr;
    ret          = QGuiApplication::screenAt(geometry().topLeft());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().topRight());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().center());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().bottomLeft());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().bottomRight());
    return ret;
  };

  // Get screen geometry
  QScreen *screen = getScreen();
  if (!screen) return;
  QRect screenGeom = screen->availableGeometry();

  QSize newSize;
  if (forward)
    // x1.2 scale
    newSize = QSize(width() * 6 / 5, height() * 6 / 5);
  else
    // 1/1.2 scale
    newSize = QSize(width() * 5 / 6, height() * 5 / 6);

  QRect newGeom(geometry().topLeft(), newSize);
  if (!screenGeom.contains(newGeom)) {
    if (newGeom.width() > screenGeom.width())
      newGeom.setWidth(screenGeom.width());
    if (newGeom.right() > screenGeom.right())
      newGeom.moveRight(screenGeom.right());
    else if (newGeom.left() < screenGeom.left())
      newGeom.moveLeft(screenGeom.left());

    if (newGeom.height() > screenGeom.height())
      newGeom.setHeight(screenGeom.height());
    if (newGeom.bottom() > screenGeom.bottom())
      newGeom.moveBottom(screenGeom.bottom());
    else if (newGeom.top() < screenGeom.top())
      newGeom.moveTop(screenGeom.top());
  }
  setGeometry(newGeom);
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
    , m_buttonSet(0)
    , m_id(0) {
  updatePixmaps();
  setFixedSize(m_onPixmap.size() / m_onPixmap.devicePixelRatio());
}

//-----------------------------------------------------------------------------

TPanelTitleBarButton::TPanelTitleBarButton(QWidget *parent,
                                           const QPixmap &standardPixmap)
    : QWidget(parent)
    , m_onPixmap(standardPixmap)
    , m_offPixmap(standardPixmap)
    , m_overPixmap(standardPixmap)
    , m_rollover(false)
    , m_pressed(false)
    , m_buttonSet(0)
    , m_id(0) {
  setFixedSize(m_onPixmap.size() / m_onPixmap.devicePixelRatio());
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

void TPanelTitleBarButton::setOverColor(const QColor &color) {
  if (m_overColor != color) {
    m_overColor = color;
    updatePixmaps();
  }
}

QColor TPanelTitleBarButton::getOverColor() const { return m_overColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPressedColor(const QColor &color) {
  if (m_pressedColor != color) {
    m_pressedColor = color;
    updatePixmaps();
  }
}

QColor TPanelTitleBarButton::getPressedColor() const { return m_pressedColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setFreezeColor(const QColor &color) {
  if (m_freezeColor != color) {
    m_freezeColor = color;
    updatePixmaps();
  }
}

QColor TPanelTitleBarButton::getFreezeColor() const { return m_freezeColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPreviewColor(const QColor &color) {
  if (m_previewColor != color) {
    m_previewColor = color;
    updatePixmaps();
  }
}

QColor TPanelTitleBarButton::getPreviewColor() const { return m_previewColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::updatePixmaps() {
  // Get background color used by some icons and states
  QColor bgColor;
  if (m_standardPixmapName.contains("freeze", Qt::CaseInsensitive)) {
    bgColor = getFreezeColor();
  } else if (m_standardPixmapName.contains("preview", Qt::CaseInsensitive)) {
    bgColor = getPreviewColor();
  } else {
    bgColor = getPressedColor();
  }

  ThemeManager &themeManager = ThemeManager::getInstance();
  const qreal offOpacity     = themeManager.getOffOpacity();

  // Compute icon
  QImage baseImg = svgToImage(m_standardPixmapName);
  baseImg        = themeManager.recolorBlackPixels(baseImg);
  QImage onImg   = compositeImage(baseImg, QSize(), false, bgColor);
  QImage offImg  = adjustImageOpacity(baseImg, offOpacity);
  QImage overImg = compositeImage(baseImg, QSize(), false, getOverColor());

  // Store in member variables
  m_onPixmap   = convertImageToPixmap(onImg);
  m_offPixmap  = convertImageToPixmap(offImg);
  m_overPixmap = convertImageToPixmap(overImg);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.drawPixmap(0, 0,
                     m_pressed    ? m_onPixmap
                     : m_rollover ? m_overPixmap
                                  : m_offPixmap);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::mouseMoveEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::enterEvent(QEvent *) {
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

void TPanelTitleBarButton::mousePressEvent(QMouseEvent *e) {
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
// TPanelTitleBarButtonForSafeArea
//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::getSafeAreaNameList(
    QList<QString> &nameList) {
  TFilePath fp                = TEnv::getConfigDir();
  QString currentSafeAreaName = QString::fromStdString(EnvSafeAreaName);

  std::string safeAreaFileName = "safearea.ini";

  while (!TFileStatus(fp + safeAreaFileName).doesExist() && !fp.isRoot() &&
         fp.getParentDir() != TFilePath())
    fp = fp.getParentDir();

  fp = fp + safeAreaFileName;

  if (TFileStatus(fp).doesExist()) {
    QSettings settings(toQString(fp), QSettings::IniFormat);

    // find the current safearea name from the list
    QStringList groups = settings.childGroups();
    for (int g = 0; g < groups.size(); g++) {
      settings.beginGroup(groups.at(g));
      nameList.push_back(settings.value("name", "").toString());
      settings.endGroup();
    }
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::RightButton) {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);

  QList<QString> safeAreaNameList;
  getSafeAreaNameList(safeAreaNameList);
  for (int i = 0; i < safeAreaNameList.size(); i++) {
    QAction *action = new QAction(safeAreaNameList.at(i), this);
    action->setData(safeAreaNameList.at(i));
    connect(action, SIGNAL(triggered()), this, SLOT(onSetSafeArea()));
    if (safeAreaNameList.at(i) == QString::fromStdString(EnvSafeAreaName)) {
      action->setCheckable(true);
      action->setChecked(true);
    }
    menu.addAction(action);
  }

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::onSetSafeArea() {
  QString safeAreaName = qobject_cast<QAction *>(sender())->data().toString();
  // change safearea if the different one is selected
  if (QString::fromStdString(EnvSafeAreaName) != safeAreaName) {
    EnvSafeAreaName = safeAreaName.toStdString();
    // emit sceneChanged without setting dirty flag
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  }
}

//-----------------------------------------------------------------------------

//=============================================================================
// TPanelTitleBarButtonForPreview
//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForPreview::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::RightButton) {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForPreview::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);

  // 0: current frame
  // 1: all frames in the preview range
  // 2: selected cell, auto play once & stop
  QStringList behaviorsStrList = {tr("Current frame"),
                                  tr("All preview range frames"),
                                  tr("Selected cells - Auto play")};

  QActionGroup *behaviorGroup = new QActionGroup(this);

  for (int i = 0; i < behaviorsStrList.size(); i++) {
    QAction *action = menu.addAction(behaviorsStrList.at(i));
    action->setData(i);
    connect(action, SIGNAL(triggered()), this, SLOT(onSetPreviewBehavior()));
    action->setCheckable(true);
    behaviorGroup->addAction(action);
    if (i == EnvViewerPreviewBehavior) action->setChecked(true);
  }

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForPreview::onSetPreviewBehavior() {
  int behaviorId = qobject_cast<QAction *>(sender())->data().toInt();
  // change safearea if the different one is selected
  if (EnvViewerPreviewBehavior != behaviorId) {
    EnvViewerPreviewBehavior = behaviorId;
    // emit sceneChanged without setting dirty flag
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  }
}

//-----------------------------------------------------------------------------

//=============================================================================
// TPanelTitleBarButtonSet
//-----------------------------------------------------------------------------

TPanelTitleBarButtonSet::TPanelTitleBarButtonSet() {}

TPanelTitleBarButtonSet::~TPanelTitleBarButtonSet() {}

void TPanelTitleBarButtonSet::add(TPanelTitleBarButton *button) {
  m_buttons.push_back(button);
}

void TPanelTitleBarButtonSet::select(TPanelTitleBarButton *button) {
  int i;
  for (i = 0; i < (int)m_buttons.size(); i++)
    m_buttons[i]->setPressed(button == m_buttons[i]);
  emit selected(button->getId());
}

//=============================================================================
// PaneTitleBar
//-----------------------------------------------------------------------------

TPanelTitleBar::TPanelTitleBar(QWidget *parent,
                               TDockWidget::Orientation orientation)
    : QFrame(parent), m_closeButtonHighlighted(false) {
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
  generateCloseButtonPixmaps();
}

//-----------------------------------------------------------------------------

QSize TPanelTitleBar::minimumSizeHint() const { return QSize(20, 18); }

//-----------------------------------------------------------------------------

void TPanelTitleBar::generateCloseButtonPixmaps() {
  // Icon theme vars
  ThemeManager &themeManager = ThemeManager::getInstance();
  const qreal offOpacity     = themeManager.getOffOpacity();

  // Use overColor from stylesheet for bgColor of rollover
  QColor overColor = getOverColor();

  // Generate base icon image
  QImage baseImg = generateIconImage("pane_close");
  baseImg        = compositeImage(baseImg, QSize(20, 18));

  // Off icon image
  QImage offImg = adjustImageOpacity(baseImg, offOpacity);

  // Over icon image
  QImage overImg = compositeImage(baseImg, QSize(), false, overColor);

  m_closeButtonPixmap     = convertImageToPixmap(offImg);
  m_closeButtonOverPixmap = convertImageToPixmap(overImg);
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  QRect rect = this->rect();

  bool isPanelActive;

  TPanel *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != 0);

  if (!dw->isFloating()) {  // docked panel
    isPanelActive = dw->widgetInThisPanelIsFocused();
  } else {                  // floating panel
    isPanelActive = isActiveWindow();
  }

  if (dw->getOrientation() == TDockWidget::vertical) {
    QString titleText = painter.fontMetrics().elidedText(
        dw->windowTitle(), Qt::ElideRight, rect.width() - 50);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(isPanelActive ? m_activeTitleColor : m_titleColor);
    painter.drawText(QPointF(8, 13), titleText);
  }

  if (dw->isFloating()) {
    QPoint closeButtonPos(rect.right() - 19, rect.top());

    if (m_closeButtonHighlighted)
      painter.drawPixmap(closeButtonPos, m_closeButtonOverPixmap);
    else
      painter.drawPixmap(closeButtonPos, m_closeButtonPixmap);
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::setOverColor(const QColor &color) {
  if (m_overColor != color) {
    m_overColor = color;
    generateCloseButtonPixmaps();
  }
}

QColor TPanelTitleBar::getOverColor() const { return m_overColor; }

//-----------------------------------------------------------------------------

void TPanelTitleBar::leaveEvent(QEvent *) {
  TPanel *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != 0);

  // Mouse left the widget, reset the highlighted flag
  if (dw->isFloating()) {
    m_closeButtonHighlighted = false;
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mousePressEvent(QMouseEvent *event) {
  TDockWidget *dw = static_cast<TDockWidget *>(parentWidget());

  QPoint pos = event->pos();

  if (dw->isFloating()) {
    QRect rect = this->rect();
    QRect closeButtonRect(rect.right() - 20, rect.top() + 1, 20, 18);
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
  TDockWidget *dw = static_cast<TDockWidget *>(parentWidget());

  if (dw->isFloating()) {
    QPoint pos = event->pos();
    QRect rect = this->rect();
    QRect closeButtonRect(rect.right() - 18, rect.top() + 1, 18, 18);

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
  m_buttons.push_back(std::make_pair(pos, widget));
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  int i;
  for (i = 0; i < (int)m_buttons.size(); i++) {
    QPoint p   = m_buttons[i].first;
    QWidget *w = m_buttons[i].second;
    if (p.x() < 0) p.setX(p.x() + width());
    w->move(p);
  }
}

//=============================================================================
// TPanelFactory
//-----------------------------------------------------------------------------

TPanelFactory::TPanelFactory(QString panelType) : m_panelType(panelType) {
  assert(tableInstance().count(panelType) == 0);
  tableInstance()[m_panelType] = this;
}

//-----------------------------------------------------------------------------

TPanelFactory::~TPanelFactory() { tableInstance().remove(m_panelType); }

//-----------------------------------------------------------------------------

QMap<QString, TPanelFactory *> &TPanelFactory::tableInstance() {
  static QMap<QString, TPanelFactory *> table;
  return table;
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent, QString panelType) {
  TPanel *panel = 0;

  QMap<QString, TPanelFactory *>::iterator it = tableInstance().find(panelType);
  if (it == tableInstance().end()) {
    if (panelType.startsWith("Custom_")) {
      panelType = panelType.right(panelType.size() - 7);
      return CustomPanelManager::instance()->createCustomPanel(panelType,
                                                               parent);
    }

    TPanel *panel = new TPanel(parent);
    panel->setPanelType(panelType.toStdString());
    return panel;
  } else {
    TPanelFactory *factory = it.value();
    TPanel *panel          = factory->createPanel(parent);
    panel->setPanelType(panelType.toStdString());
    return panel;
  }
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent) {
  TPanel *panel = new TPanel(parent);
  panel->setObjectName(getPanelType());
  panel->setWindowTitle(getPanelType());
  initialize(panel);
  return panel;
}

//-----------------------------------------------------------------------------
