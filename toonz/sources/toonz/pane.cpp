

#include "pane.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "tenv.h"
#include "saveloadqsettings.h"
#include "custompanelmanager.h"
#include "layoutUtils.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "layoutPresetsEditorPopup.h"

// TnzCore includes
#include "tsystem.h"
#include "tmsgcore.h"

// Qt includes
#include <QPainter>
#include <QLineEdit>
#include <QTextEdit>
#include <QScreen>
#include <QDesktopServices>

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
  setMouseTracking(true);

  // Determine button size
  m_baseSize = QSize(20, 18);
  if (m_standardPixmapName.contains("preview", Qt::CaseInsensitive)) {
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

QColor TPanelTitleBarButton::getOffColor() const { return m_offColor; }

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

void TPanelTitleBarButton::paintEvent(QPaintEvent *event) {
  if (m_standardPixmapName.isEmpty()) return;

  QPainter p(this);

  // Determine icon state
  QIcon::Mode mode   = m_rollover ? QIcon::Active : QIcon::Normal;
  QIcon::State state = m_pressed ? QIcon::On : QIcon::Off;

  // Get DPR and sizes
  qreal dpr    = getDevicePixelRatio(this);
  auto &tm     = ThemeManager::getInstance();
  QSize iconSz = tm.getIconSize(m_standardPixmapName);
  QSize physSz = iconSz * dpr;
  QSize btnSz  = size() * dpr;

  // Cache key with DPR
  QString cacheKey =
      generateCacheKey(m_standardPixmapName, btnSz, mode, state) +
      QString::number(dpr);

  // Try cache first
  QPixmap finalPm = getFromPixmapCache(cacheKey);
  if (finalPm.isNull()) {
    QPixmap pm = SvgIconEngine(m_standardPixmapName, false, dpr, iconSz)
                     .pixmap(iconSz, mode, state);
    pm.scaled(iconSz, Qt::KeepAspectRatio);

    if (!pm.isNull()) {
      QColor bgColor = getOffColor();
      if (m_pressed) {
        if (m_standardPixmapName.contains("freeze")) {
          bgColor = getFreezeColor();
        } else if (m_standardPixmapName.contains("preview")) {
          bgColor = getPreviewColor();
        } else {
          bgColor = getPressedColor();
        }
      } else if (m_rollover) {
        bgColor = getOverColor();
      }

      pm.setDevicePixelRatio(dpr);
      finalPm = expandPixmap(pm, btnSz, bgColor);
      finalPm.setDevicePixelRatio(dpr);

      addToPixmapCache(cacheKey, finalPm);
    } else
      return;
  }

  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.drawPixmap(0, 0, finalPm);
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
// TPanelTitleBarButtonForLayoutGuide
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForLayoutGuide::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::RightButton) {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

void TPanelTitleBarButtonForLayoutGuide::onEditLayouts() {
  QWidget *parent = TApp::instance()->getMainWindow();

  LayoutPresetsEditorPopup *popup = LayoutPresetsEditorPopup::instance(parent);

  QPoint globalMousePos = QCursor::pos();

  popup->showAndPosition(globalMousePos);

  return;
}

void TPanelTitleBarButtonForLayoutGuide::onAddToXsheet() {
  layoutUtils::addLayoutToXsheet();
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForLayoutGuide::contextMenuEvent(
    QContextMenuEvent *e) {
  QMenu menu(this);

  QString currentLayoutPresetName = TApp::instance()
                                        ->getCurrentScene()
                                        ->getScene()
                                        ->getProperties()
                                        ->getLayoutPresetName();
  QList<QString> layoutNameList;
  layoutUtils::loadLayoutPresetList(layoutNameList);
  QActionGroup *behaviorGroup = new QActionGroup(this);

  m_originalLayoutPresetName = currentLayoutPresetName;

  bool framesPresetFound = false;

  for (int i = 0; i < layoutNameList.size(); i++) {
    QAction *action = new QAction(layoutNameList.at(i), this);
    action->setData(layoutNameList.at(i));
    connect(action, SIGNAL(triggered()), this, SLOT(onSetLayout()));
    action->setCheckable(true);
    behaviorGroup->addAction(action);
    menu.addAction(action);
    if (layoutNameList.at(i) == currentLayoutPresetName) {
      framesPresetFound = true;
      action->setChecked(true);
    }
  }

  if (!framesPresetFound && !currentLayoutPresetName.isEmpty()) {
    QString framesPresetMissingTemplate = tr("%1 (Missing)");
    QString invalidActionText =
        framesPresetMissingTemplate.arg(currentLayoutPresetName);
    QAction *invalidAction = new QAction(invalidActionText, this);

    QPixmap redDot(16, 16);
    redDot.fill(Qt::transparent);
    QPainter p(&redDot);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QBrush(Qt::red));
    p.setPen(Qt::NoPen);
    p.drawEllipse(2, 2, 12, 12);
    p.end();
    invalidAction->setIcon(redDot);

    invalidAction->setEnabled(true);

    connect(invalidAction, &QAction::triggered, this,
            &TPanelTitleBarButtonForLayoutGuide::onEditLayouts);

    invalidAction->setChecked(true);
    behaviorGroup->addAction(invalidAction);
    menu.addAction(invalidAction);
  }

  menu.addSeparator();
  QAction *clearAction = new QAction(tr("Clear current"), this);
  connect(clearAction, &QAction::triggered, this, []() {
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->setLayoutPresetName(QString());
    layoutUtils::invalidateCurrent();
    TApp::instance()->getCurrentScene()->notifySceneChanged(true);
  });
  menu.addAction(clearAction);

  QAction *editAction = new QAction(tr("Edit Layout Presets..."), this);
  connect(editAction, SIGNAL(triggered()), this, SLOT(onEditLayouts()));
  menu.addAction(editAction);

  QAction *addLayoutAction = new QAction(tr("Add Layout to XSheet"), this);
  connect(addLayoutAction, SIGNAL(triggered()), this, SLOT(onAddToXsheet()));
  menu.addAction(addLayoutAction);

  connect(&menu, SIGNAL(hovered(QAction *)), this,
          SLOT(onMenuActionHovered(QAction *)));
  connect(&menu, SIGNAL(aboutToHide()), this, SLOT(onMenuAboutToHide()));

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForLayoutGuide::onSetLayout() {
  QString framesPresetName =
      qobject_cast<QAction *>(sender())->data().toString();
  // change framesPreset if the different one is selected
  QString currentframesPresetName = TApp::instance()
                                        ->getCurrentScene()
                                        ->getScene()
                                        ->getProperties()
                                        ->getLayoutPresetName();
  if (currentframesPresetName != framesPresetName) {
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->setLayoutPresetName(framesPresetName);
    layoutUtils::invalidateCurrent();
    TApp::instance()->getCurrentScene()->notifySceneChanged(true);
  }
}

void TPanelTitleBarButtonForLayoutGuide::onMenuActionHovered(QAction *action) {
  if (!action || action->data().isNull() ||
      action->text() == tr("Edit Frames Presets...")) {
    if (m_isPreviewing) {
      TApp::instance()
          ->getCurrentScene()
          ->getScene()
          ->getProperties()
          ->setLayoutPresetName(m_originalLayoutPresetName);
      layoutUtils::invalidateCurrent();
      TApp::instance()->getCurrentScene()->notifySceneChanged(false);
      m_isPreviewing = false;
    }
    return;
  }

  QString framesPresetName = action->data().toString();

  TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getProperties()
      ->setLayoutPresetName(framesPresetName);
  layoutUtils::invalidateCurrent();
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  m_isPreviewing = true;
}

void TPanelTitleBarButtonForLayoutGuide::onMenuAboutToHide() {
  if (m_isPreviewing) {
    TApp::instance()
        ->getCurrentScene()
        ->getScene()
        ->getProperties()
        ->setLayoutPresetName(m_originalLayoutPresetName);
    layoutUtils::invalidateCurrent();
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
    m_isPreviewing = false;
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
  // change framesPreset if the different one is selected
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
}

//-----------------------------------------------------------------------------

QSize TPanelTitleBar::minimumSizeHint() const { return QSize(20, 18); }

//-----------------------------------------------------------------------------

void TPanelTitleBar::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  QRect rect = this->rect();

  bool isPanelActive;
  TPanel *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != 0);

  if (!dw->isFloating()) {  // docked panel
    isPanelActive = dw->widgetInThisPanelIsFocused();
  } else {  // floating panel
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
    QPoint btnPos(rect.right() - 19, rect.top());
    qreal dpr    = getDevicePixelRatio(this);
    QSize btnSz  = minimumSizeHint() * dpr;
    QSize iconSz = QSize(16, 16);

    // Icon mode and background color
    bool highlighted = m_closeButtonHighlighted;
    QIcon::Mode mode = highlighted ? QIcon::Active : QIcon::Normal;
    QColor bgColor   = highlighted ? getCloseOverColor() : Qt::transparent;

    // Cache key
    QString cacheKey =
        generateCacheKey("closewindow", btnSz, mode, QIcon::Off) +
        QString::number(dpr);

    // Check cache first
    QPixmap finalPm = getFromPixmapCache(cacheKey);
    if (!finalPm.isNull()) {
      painter.drawPixmap(btnPos, finalPm);
      return;
    }

    // Load icon at logical size
    QPixmap pm = SvgIconEngine("closewindow", false, dpr, iconSz)
                     .pixmap(iconSz, mode, QIcon::Off);
    if (pm.isNull()) return;

    pm.setDevicePixelRatio(dpr);
    finalPm = expandPixmap(pm, btnSz, bgColor);
    finalPm.setDevicePixelRatio(dpr);

    // Cache and draw
    addToPixmapCache(cacheKey, finalPm);
    painter.drawPixmap(btnPos, finalPm);
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
