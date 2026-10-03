

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
#include <QMouseEvent>
#include <QShowEvent>
#include <QMainWindow>
#include <QSettings>
#include <QMap>
#include <QMenu>
#include <QApplication>
#include <qdrawutil.h>
#include <assert.h>
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QActionGroup>
#include <QChildEvent>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QHoverEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QVariant>
#include <QGlobalStatic>
#include <memory>
#include <utility>

extern TEnv::StringVar EnvSafeAreaName;
extern TEnv::IntVar EnvViewerPreviewBehavior;

namespace {

constexpr int kFloatingMargin   = 5;
constexpr int kCompactBarHeight = 18;
constexpr int kCompactBarGap    = 4;

bool compactSurfaceExempt(const QWidget *widget) {
  return qobject_cast<const TPanelTitleBar *>(widget) ||
         qobject_cast<const TPanelTitleBarButton *>(widget) ||
         qobject_cast<const QAbstractSlider *>(widget) ||
         qobject_cast<const QAbstractSpinBox *>(widget) ||
         qobject_cast<const QLineEdit *>(widget) ||
         qobject_cast<const QTextEdit *>(widget) ||
         qobject_cast<const QPlainTextEdit *>(widget) ||
         qobject_cast<const QComboBox *>(widget) ||
         qobject_cast<const QAbstractItemView *>(widget);
}

}  // namespace

//=============================================================================
// TPanel
//-----------------------------------------------------------------------------

TPanel::TPanel(QWidget *parent, Qt::WindowFlags flags,
               TDockWidget::Orientation orientation)
    : TDockWidget(parent, flags)
    , m_panelType("")
    , m_isMaximizable(true)
    , m_isMaximized(false)
    , m_panelTitleBar(nullptr)
    , m_multipleInstancesAllowed(true)
    , m_isRoomBound(false)
    , m_boundRoomName("")
    , m_roomBindButton(nullptr)
    , m_compactFloating(false)
    , m_showTitleBar(true)
    , m_compactTransparentBg(false)
    , m_transparentLookApplied(false)
    , m_savedContentAutoFill(false)
    , m_compactApplied(false)
    , m_inFloatingChrome(false)
    , m_displaySyncPending(false)
    , m_contentPress(false)
    , m_forwardingMouse(false)
    , m_hasSavedPanelLimits(false)
    , m_restoreSizePending(false)
    , m_gripCursor(0)
    , m_gripCursorTimer(nullptr) {
  m_panelTitleBar = new TPanelTitleBar(this, orientation);
  setTitleBarWidget(m_panelTitleBar);
  connect(m_panelTitleBar, &TPanelTitleBar::doubleClick, this,
          &TPanel::doubleClick);
  connect(m_panelTitleBar, &TPanelTitleBar::closeButtonPressed, this,
          &TPanel::onCloseButtonPressed);
  setOrientation(orientation);

  // Enable context menu for room binding
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &QWidget::customContextMenuRequested, this,
          &TPanel::onCustomContextMenuRequested);
}

//-----------------------------------------------------------------------------

TPanel::~TPanel() {
  updateGripCursor(0);
  // On quitting, save the floating panel's geometry and state in order to
  // restore them when opening the floating panel next time
  if (isFloating()) {
    const TFilePath savePath =
        ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
    QSettings settings(QString::fromStdWString(savePath.getWideString()),
                       QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Panels"));
    settings.beginGroup(QString::fromStdString(m_panelType));
    settings.setValue(QStringLiteral("geometry"), geometry());
    if (auto *persistent = dynamic_cast<SaveLoadQSettings *>(widget()))
      persistent->save(settings);
  }
}

//-----------------------------------------------------------------------------

void TPanel::paintEvent(QPaintEvent *e) {
  if (m_compactApplied) return;

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
  // ready to reappear if they are needed again. However, the user expects
  // a new panel to be created - so we just reset the panel here.
  // reset();    //Moved to panel invocation in floatingpanelcommand.cpp

  // Also, remove widget from its dock layout control
  if (parentLayout()) parentLayout()->removeWidget(this);
}

//-----------------------------------------------------------------------------

void TPanel::onCustomContextMenuRequested(const QPoint &pos) {
  execContextMenu(mapToGlobal(pos));
}

//-----------------------------------------------------------------------------

bool TPanel::isCustomPanel() const {
  return m_panelType.rfind("Custom_", 0) == 0;
}

//-----------------------------------------------------------------------------

void TPanel::execContextMenu(const QPoint &globalPos) {
  QMenu menu(this);

  QAction *bindAction = menu.addAction(tr("Bind to Current Room"));
  bindAction->setCheckable(true);
  bindAction->setChecked(m_isRoomBound);

  connect(bindAction, &QAction::triggered, [this](bool checked) {
    auto *mw = dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
    if (!mw) return;

    Room *currentRoom = mw->getCurrentRoom();
    if (!currentRoom) return;

    setRoomBound(checked);
    if (checked) {
      setBoundRoomName(currentRoom->getName());
      mw->updatePanelVisibility();
    } else {
      setBoundRoomName(QString());
      if (isHidden()) show();
    }
  });

  if (isCustomPanel() && isFloating()) {
    QMenu *compactMenu         = menu.addMenu(tr("Compact Mode"));
    QActionGroup *compactGroup = new QActionGroup(compactMenu);
    compactGroup->setExclusive(true);

    QAction *offAction   = compactMenu->addAction(tr("Off"));
    QAction *miniAction  = compactMenu->addAction(tr("Mini Title Bar"));
    QAction *noBarAction = compactMenu->addAction(tr("No Title Bar"));
    for (QAction *action : {offAction, miniAction, noBarAction}) {
      action->setCheckable(true);
      compactGroup->addAction(action);
    }
    if (!m_compactFloating)
      offAction->setChecked(true);
    else if (m_showTitleBar)
      miniAction->setChecked(true);
    else
      noBarAction->setChecked(true);

    connect(offAction, &QAction::triggered,
            [this](bool) { setCompactMode(false, m_showTitleBar); });
    connect(miniAction, &QAction::triggered,
            [this](bool) { setCompactMode(true, true); });
    connect(noBarAction, &QAction::triggered,
            [this](bool) { setCompactMode(true, false); });

    compactMenu->addSeparator();
    QAction *transparentAction =
        compactMenu->addAction(tr("Transparent Background"));
    transparentAction->setCheckable(true);
    transparentAction->setChecked(m_compactTransparentBg);
    transparentAction->setEnabled(m_compactFloating);
    connect(transparentAction, &QAction::triggered,
            [this](bool checked) { setCompactTransparentBackground(checked); });

    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close"));
    connect(closeAction, &QAction::triggered, [this](bool) {
      hide();
      onCloseButtonPressed();
    });
  }

  menu.exec(globalPos);
}

//-----------------------------------------------------------------------------

void TPanel::watchContextMenu(QWidget *root) {
  if (!root) return;
  const auto watch = [this](QWidget *widget) {
    widget->installEventFilter(this);
    widget->setMouseTracking(true);
    widget->setAttribute(Qt::WA_Hover, true);
  };
  watch(root);
  const QList<QWidget *> children = root->findChildren<QWidget *>();
  for (QWidget *child : children) watch(child);
}

//-----------------------------------------------------------------------------

bool TPanel::eventFilter(QObject *watched, QEvent *event) {
  if (m_forwardingMouse) return TDockWidget::eventFilter(watched, event);

  if (event->type() == QEvent::ChildAdded && isCustomPanel()) {
    if (auto *child =
            qobject_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()))
      watchContextMenu(child);
  }

  if (event->type() == QEvent::ContextMenu && isCustomPanel()) {
    execContextMenu(static_cast<QContextMenuEvent *>(event)->globalPos());
    return true;
  }
  if (auto *widget = qobject_cast<QWidget *>(watched)) {
    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::HoverMove ||
        event->type() == QEvent::MouseButtonPress) {
      const QPoint local   = event->type() == QEvent::HoverMove
                                 ? static_cast<QHoverEvent *>(event)->pos()
                                 : static_cast<QMouseEvent *>(event)->pos();
      const int marginType = compactResizeMargin(widget->mapTo(this, local));
      updateGripCursor(marginType);
      if (marginType && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
          const QPoint panelPos = widget->mapTo(this, me->pos());
          m_forwardingMouse     = true;
          QMouseEvent press(QEvent::MouseButtonPress, panelPos, me->globalPos(),
                            Qt::LeftButton, Qt::LeftButton, me->modifiers());
          QApplication::sendEvent(this, &press);
          m_forwardingMouse = false;
          if (m_resizing) grabMouse();
          return true;
        }
      }
    }
  }
  if (handleCompactDrag(watched, event)) return true;
  return TDockWidget::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------

void TPanel::loadCompactFloating() {
  if (!isCustomPanel()) return;
  const TFilePath savePath =
      ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("Panels"));
  settings.beginGroup(QString::fromStdString(m_panelType));
  m_showTitleBar =
      settings.value(QStringLiteral("showTitleBar"), true).toBool();
  m_compactTransparentBg =
      settings.value(QStringLiteral("compactTransparentBg"), false).toBool();
  m_compactFloating =
      settings.value(QStringLiteral("compactFloating"), false).toBool();

  ensureCompactTranslucency(m_compactFloating);
  scheduleDisplaySync();
}

//-----------------------------------------------------------------------------

void TPanel::loadCompactState(QSettings &settings) {
  if (!isCustomPanel() || !settings.contains(QStringLiteral("compactMode")))
    return;
  m_compactFloating =
      settings.value(QStringLiteral("compactMode"), false).toBool();
  m_showTitleBar =
      settings.value(QStringLiteral("compactTitleBar"), true).toBool();
  m_compactTransparentBg =
      settings.value(QStringLiteral("compactTransparent"), false).toBool();
  const QSize content =
      settings.value(QStringLiteral("floatingContent")).toSize();
  if (content.isValid()) m_lastFloatingContent = content;
  scheduleDisplaySync();
}

//-----------------------------------------------------------------------------

void TPanel::saveCompactState(QSettings &settings) const {
  if (!isCustomPanel()) return;
  settings.setValue(QStringLiteral("compactMode"), m_compactFloating);
  settings.setValue(QStringLiteral("compactTitleBar"), m_showTitleBar);
  settings.setValue(QStringLiteral("compactTransparent"),
                    m_compactTransparentBg);
  if (!isFloating() && m_lastFloatingContent.isValid())
    settings.setValue(QStringLiteral("floatingContent"), m_lastFloatingContent);
}

//-----------------------------------------------------------------------------

void TPanel::saveCompactFloating() const {
  const TFilePath savePath =
      ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("Panels"));
  settings.beginGroup(QString::fromStdString(m_panelType));
  settings.setValue(QStringLiteral("compactFloating"), m_compactFloating);
  settings.setValue(QStringLiteral("showTitleBar"), m_showTitleBar);
  settings.setValue(QStringLiteral("compactTransparentBg"),
                    m_compactTransparentBg);
}

//-----------------------------------------------------------------------------

QSize TPanel::chromeSize(bool compact) const {
  if (compact)
    return QSize(0, m_showTitleBar ? kCompactBarHeight + kCompactBarGap : 0);
  const int titleH =
      qMax(kCompactBarHeight,
           m_panelTitleBar ? m_panelTitleBar->minimumSizeHint().height() : 0);
  return QSize(2 * kFloatingMargin, 2 * kFloatingMargin + titleH);
}

//-----------------------------------------------------------------------------

void TPanel::applyDisplayState(bool floating, bool keepContentSize) {
  if (!isCustomPanel() || !m_panelTitleBar) return;
  auto *box = qobject_cast<QBoxLayout *>(layout());
  if (!box) return;

  const bool compact    = floating && m_compactFloating;
  const QSize oldChrome = m_appliedChrome;
  QWidget *content      = widget();

  const bool track =
      keepContentSize && content && isVisible() && m_inFloatingChrome;
  const QPoint before = track ? content->mapToGlobal(QPoint(0, 0)) : QPoint();

  QSize newChrome;
  if (compact) {
    if (box->indexOf(m_panelTitleBar) >= 0) box->removeWidget(m_panelTitleBar);
    int barW = 28;
    if (m_roomBindButton) barW += m_roomBindButton->width();
    m_panelTitleBar->setFixedSize(barW, kCompactBarHeight);
    setFloatingChromeMargin(0);
    newChrome = chromeSize(true);
    box->setContentsMargins(0, newChrome.height(), 0, 0);
    m_panelTitleBar->setSuppressed(!m_showTitleBar);
  } else {
    m_panelTitleBar->setSuppressed(false);
    if (m_compactApplied) {
      m_panelTitleBar->setMinimumSize(20, kCompactBarHeight);
      m_panelTitleBar->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
      m_panelTitleBar->setSizePolicy(QSizePolicy::Preferred,
                                     QSizePolicy::Fixed);
    }
    if (box->indexOf(m_panelTitleBar) < 0)
      box->insertWidget(
          0, m_panelTitleBar, 0,
          getOrientation() == vertical ? Qt::AlignTop : Qt::AlignLeft);
    if (floating) {
      setFloatingChromeMargin(kFloatingMargin);
      box->setContentsMargins(kFloatingMargin, kFloatingMargin, kFloatingMargin,
                              kFloatingMargin);
      newChrome = chromeSize(false);
    } else {
      setFloatingMargin(kFloatingMargin);
      box->setContentsMargins(0, 0, 0, 0);
    }
  }

  m_panelTitleBar->setCompact(compact);
  ensureCompactTranslucency(compact);
  m_compactApplied   = compact;
  m_inFloatingChrome = floating;
  m_appliedChrome    = newChrome;
  applyPanelLimits(compact, floating, newChrome);

  if (keepContentSize && floating && oldChrome.isValid() &&
      newChrome != oldChrome)
    resize(qMax(1, width() + newChrome.width() - oldChrome.width()),
           qMax(1, height() + newChrome.height() - oldChrome.height()));

  if (compact) {
    positionCompactTitleBar();
    if (m_showTitleBar) m_panelTitleBar->raise();
  }
  syncCompactTransparentLook();

  if (track) {
    box->activate();
    const QPoint after = content->mapToGlobal(QPoint(0, 0));
    if (after != before) move(pos() + before - after);
  }
  update();
}

//-----------------------------------------------------------------------------

void TPanel::scheduleDisplaySync() {
  if (m_displaySyncPending || !isCustomPanel()) return;
  m_displaySyncPending = true;
  QTimer::singleShot(0, this, [this]() {
    m_displaySyncPending = false;
    applyDisplayState(isFloating());
    if (m_restoreSizePending && isFloating()) restoreFloatingSize();
    m_restoreSizePending = false;
  });
}

//-----------------------------------------------------------------------------

void TPanel::applyPanelLimits(bool compact, bool floating,
                              const QSize &chrome) {
  if (compact) {
    if (!m_hasSavedPanelLimits) {
      m_savedPanelMin       = minimumSize();
      m_savedPanelMax       = maximumSize();
      m_hasSavedPanelLimits = true;
    }
    if (!m_compactFloor.isValid()) {
      if (QWidget *content = widget()) {
        const QSize hint =
            content->minimumSize().expandedTo(content->minimumSizeHint());
        if (hint.width() >= 8 && hint.height() >= 8) m_compactFloor = hint;
      }
    }
    const QSize limit = m_compactFloor.isValid() ? m_compactFloor : QSize(1, 1);
    setMinimumSize(limit.width(), limit.height() + chrome.height());
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  } else if (m_hasSavedPanelLimits) {
    const int extra = floating ? 2 * kFloatingMargin : 0;
    setMinimumSize(m_savedPanelMin.width() + extra,
                   m_savedPanelMin.height() + extra);
    setMaximumSize(qMin(QWIDGETSIZE_MAX, m_savedPanelMax.width() + extra),
                   qMin(QWIDGETSIZE_MAX, m_savedPanelMax.height() + extra));
    m_hasSavedPanelLimits = false;
  }
}

//-----------------------------------------------------------------------------

void TPanel::rememberFloatingSize() {
  if (!m_inFloatingChrome || !m_appliedChrome.isValid()) return;
  const QSize content = size() - m_appliedChrome;
  if (content.width() >= 1 && content.height() >= 1)
    m_lastFloatingContent = content;
}

//-----------------------------------------------------------------------------

void TPanel::restoreFloatingSize() {
  if (!m_lastFloatingContent.isValid() || !m_appliedChrome.isValid()) return;
  const QSize wanted = m_lastFloatingContent + m_appliedChrome;
  if (size() != wanted) resize(wanted);
}

//-----------------------------------------------------------------------------

void TPanel::setFloatingChromeMargin(int margin) {
  const int old = getFloatingMargin();
  if (old == margin) return;
  setFloatingMargin(margin);
  if (!m_inFloatingChrome) return;
  const int delta = 2 * (margin - old);
  setMinimumSize(qMax(0, minimumWidth() + delta),
                 qMax(0, minimumHeight() + delta));
  setMaximumSize(qMin(QWIDGETSIZE_MAX, maximumWidth() + delta),
                 qMin(QWIDGETSIZE_MAX, maximumHeight() + delta));
}

//-----------------------------------------------------------------------------

void TPanel::positionCompactTitleBar() {
  if (!m_compactApplied || !m_panelTitleBar) return;
  m_panelTitleBar->move(qMax(0, width() - m_panelTitleBar->width()), 0);
}

//-----------------------------------------------------------------------------
void TPanel::ensureCompactTranslucency(bool on) {
  if (!isCustomPanel()) return;
  if (testAttribute(Qt::WA_TranslucentBackground) == on &&
      testAttribute(Qt::WA_NoSystemBackground) == on)
    return;
  setAttribute(Qt::WA_TranslucentBackground, on);
  setAttribute(Qt::WA_NoSystemBackground, on);
  if (!testAttribute(Qt::WA_WState_Created)) return;
  const QRect geom = geometry();
  const bool vis   = isVisible();
  setWindowFlags(windowFlags());
  if (geom.isValid() && geom.width() > 1 && geom.height() > 1)
    setGeometry(geom);
  if (vis) show();
}

//-----------------------------------------------------------------------------

int TPanel::compactResizeMargin(const QPoint &panelPos) const {
  if (!isCustomPanel() || !m_compactApplied || !isFloating()) return 0;
  const int grip = 8;
  if (panelPos.x() < 0 || panelPos.y() < 0 || panelPos.x() >= width() ||
      panelPos.y() >= height())
    return 0;
  int marginType = 0;
  if (panelPos.x() < grip) marginType |= leftMargin;
  if (panelPos.y() < grip) marginType |= topMargin;
  if (panelPos.x() >= width() - grip) marginType |= rightMargin;
  if (panelPos.y() >= height() - grip) marginType |= bottomMargin;
  return marginType;
}

//-----------------------------------------------------------------------------

void TPanel::updateGripCursor(int marginType) {
  if (marginType == m_gripCursor) return;
  const bool wasActive = m_gripCursor != 0;
  m_gripCursor         = marginType;
  if (!marginType) {
    QGuiApplication::restoreOverrideCursor();
    if (m_gripCursorTimer) m_gripCursorTimer->stop();
    return;
  }
  const bool left       = marginType & leftMargin;
  const bool top        = marginType & topMargin;
  const bool horizontal = marginType & (leftMargin | rightMargin);
  const bool vertical   = marginType & (topMargin | bottomMargin);
  Qt::CursorShape shape = Qt::ArrowCursor;
  if (horizontal && vertical)
    shape = (left == top) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
  else if (horizontal)
    shape = Qt::SizeHorCursor;
  else
    shape = Qt::SizeVerCursor;
  if (wasActive)
    QGuiApplication::changeOverrideCursor(shape);
  else
    QGuiApplication::setOverrideCursor(shape);

  if (!m_gripCursorTimer) {
    m_gripCursorTimer = new QTimer(this);
    m_gripCursorTimer->setInterval(50);
    connect(m_gripCursorTimer, &QTimer::timeout, this, [this]() {
      if (m_resizing) return;
      QWidget *under    = QApplication::widgetAt(QCursor::pos());
      const bool inside = under && (under == this || isAncestorOf(under));
      updateGripCursor(
          inside ? compactResizeMargin(mapFromGlobal(QCursor::pos())) : 0);
    });
  }
  m_gripCursorTimer->start();
}

//-----------------------------------------------------------------------------

void TPanel::setCompactTransparentBackground(bool on) {
  if (!isCustomPanel() || on == m_compactTransparentBg) return;
  m_compactTransparentBg = on;
  saveCompactFloating();
  syncCompactTransparentLook();
  update();
}

//-----------------------------------------------------------------------------

void TPanel::clearCompactSurfaces() {
  for (const CompactSurface &saved : m_compactSurfaces) {
    QWidget *w = saved.widget.data();
    if (!w) continue;
    w->setAutoFillBackground(saved.autoFillBackground);
    w->setPalette(saved.palette);
    w->setAttribute(Qt::WA_TranslucentBackground, saved.translucentBackground);
    w->setAttribute(Qt::WA_NoSystemBackground, saved.noSystemBackground);
    w->update();
  }
  m_compactSurfaces.clear();
}

//-----------------------------------------------------------------------------

void TPanel::syncCompactTransparentLook() {
  const bool want =
      isCustomPanel() && m_compactApplied && m_compactTransparentBg;
  if (want == m_transparentLookApplied) return;

  const QRect geom = geometry();
  QWidget *content = widget();

  auto polish = [](QWidget *w) {
    if (!w || !w->style()) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
  };

  if (want) {
    m_savedPanelStyleSheet = styleSheet();
    setProperty("compactTransparent", true);
    QString panelSheet = m_savedPanelStyleSheet;
    if (!panelSheet.isEmpty() && !panelSheet.endsWith(QLatin1Char('\n')))
      panelSheet += QLatin1Char('\n');
    panelSheet += QStringLiteral(
        "TPanel[compactTransparent=\"true\"] {"
        " background: transparent; background-color: transparent; }\n");
    setStyleSheet(panelSheet);
    setAutoFillBackground(false);
    polish(this);

    if (content) {
      m_savedContentStyleSheet = content->styleSheet();
      m_savedContentAutoFill   = content->autoFillBackground();
      content->setProperty("compactTransparent", true);
      QString contentSheet = m_savedContentStyleSheet;
      if (!contentSheet.isEmpty() && !contentSheet.endsWith(QLatin1Char('\n')))
        contentSheet += QLatin1Char('\n');
      contentSheet += QStringLiteral(
          "QWidget[compactTransparent=\"true\"] {"
          " background: transparent; background-color: transparent; }\n");
      content->setStyleSheet(contentSheet);
      content->setAutoFillBackground(false);
      polish(content);

      QList<QWidget *> targets;
      const QList<QWidget *> children = content->findChildren<QWidget *>(
          QString(), Qt::FindChildrenRecursively);
      for (QWidget *child : children) {
        if (!compactSurfaceExempt(child)) targets.append(child);
      }
      for (QWidget *target : targets) {
        CompactSurface saved;
        saved.widget             = target;
        saved.palette            = target->palette();
        saved.autoFillBackground = target->autoFillBackground();
        saved.translucentBackground =
            target->testAttribute(Qt::WA_TranslucentBackground);
        saved.noSystemBackground =
            target->testAttribute(Qt::WA_NoSystemBackground);
        m_compactSurfaces.append(saved);

        target->setAutoFillBackground(false);
        target->setAttribute(Qt::WA_TranslucentBackground, true);
        target->setAttribute(Qt::WA_NoSystemBackground, true);
        QPalette pal = target->palette();
        pal.setColor(QPalette::Window, Qt::transparent);
        pal.setColor(QPalette::Base, Qt::transparent);
        pal.setColor(QPalette::Button, Qt::transparent);
        target->setPalette(pal);
        target->update();
      }
    }
    m_transparentLookApplied = true;
  } else {
    setProperty("compactTransparent", QVariant());
    setStyleSheet(m_savedPanelStyleSheet);
    polish(this);
    if (content) {
      content->setProperty("compactTransparent", QVariant());
      content->setStyleSheet(m_savedContentStyleSheet);
      content->setAutoFillBackground(m_savedContentAutoFill);
      polish(content);
    }
    clearCompactSurfaces();
    m_transparentLookApplied = false;
  }

  if (geom.isValid() && geometry() != geom) setGeometry(geom);
}

//-----------------------------------------------------------------------------

void TPanel::setCompactMode(bool compact, bool showTitleBar) {
  if (!isCustomPanel()) return;
  if (compact == m_compactFloating && showTitleBar == m_showTitleBar) return;
  m_compactFloating = compact;
  m_showTitleBar    = showTitleBar;
  m_contentPress    = false;
  m_contentPressWidget.clear();
  saveCompactFloating();
  if (isFloating()) applyDisplayState(true, true);
}

//-----------------------------------------------------------------------------

bool TPanel::compactDragExempt(QWidget *widget) const {
  return qobject_cast<MyScroller *>(widget) ||
         qobject_cast<QAbstractSlider *>(widget) ||
         qobject_cast<QAbstractSpinBox *>(widget) ||
         qobject_cast<QLineEdit *>(widget) ||
         qobject_cast<QTextEdit *>(widget) ||
         qobject_cast<QPlainTextEdit *>(widget) ||
         qobject_cast<QComboBox *>(widget) ||
         qobject_cast<QAbstractItemView *>(widget);
}

//-----------------------------------------------------------------------------

bool TPanel::beginCompactDrag(const QPoint &globalPos) {
  if (!m_panelTitleBar || !m_panelTitleBar->geometry().isValid()) return false;
  const QPoint grip = m_panelTitleBar->geometry().center();
  m_forwardingMouse = true;
  QMouseEvent press(QEvent::MouseButtonPress, grip, globalPos, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(this, &press);
  m_forwardingMouse = false;
  if (!m_dragging) return false;
  grabMouse();
  return true;
}

//-----------------------------------------------------------------------------

bool TPanel::handleCompactDrag(QObject *watched, QEvent *event) {
  const bool armed =
      isCustomPanel() && isFloating() && m_compactApplied && !m_showTitleBar;
  if (!armed) {
    m_contentPress = false;
    m_contentPressWidget.clear();
    return false;
  }

  auto *widget = qobject_cast<QWidget *>(watched);
  if (!widget) return false;

  if (event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton || compactDragExempt(widget))
      return false;
    m_contentPress       = true;
    m_contentPressGlobal = me->globalPos();
    m_contentPressWidget = widget;
    return true;
  }

  if (!m_contentPress) return false;

  if (event->type() == QEvent::MouseMove) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (!(me->buttons() & Qt::LeftButton)) {
      m_contentPress = false;
      m_contentPressWidget.clear();
      return false;
    }
    if ((me->globalPos() - m_contentPressGlobal).manhattanLength() <
        QApplication::startDragDistance())
      return true;

    const QPoint global              = me->globalPos();
    const Qt::KeyboardModifiers mods = me->modifiers();
    const Qt::MouseButtons buttons   = me->buttons();
    const QPoint pressGlobal         = m_contentPressGlobal;
    m_contentPress                   = false;
    m_contentPressWidget.clear();
    if (!beginCompactDrag(pressGlobal)) return true;

    m_forwardingMouse = true;
    QMouseEvent move(QEvent::MouseMove, mapFromGlobal(global), global,
                     Qt::NoButton, buttons, mods);
    QApplication::sendEvent(this, &move);
    m_forwardingMouse = false;
    return true;
  }

  if (event->type() == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton) return false;
    QWidget *target          = m_contentPressWidget;
    const QPoint pressGlobal = m_contentPressGlobal;
    m_contentPress           = false;
    m_contentPressWidget.clear();
    if (!target) return true;

    m_forwardingMouse = true;
    QMouseEvent press(QEvent::MouseButtonPress,
                      target->mapFromGlobal(pressGlobal), pressGlobal,
                      Qt::LeftButton, Qt::LeftButton, me->modifiers());
    QMouseEvent release(QEvent::MouseButtonRelease,
                        target->mapFromGlobal(me->globalPos()), me->globalPos(),
                        Qt::LeftButton, Qt::NoButton, me->modifiers());
    QApplication::sendEvent(target, &press);
    QApplication::sendEvent(target, &release);
    m_forwardingMouse = false;
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void TPanel::showEvent(QShowEvent *event) {
  TDockWidget::showEvent(event);
  if (!isCustomPanel()) return;
  applyDisplayState(isFloating());
  if (m_restoreSizePending && isFloating()) restoreFloatingSize();
  scheduleDisplaySync();
}

//-----------------------------------------------------------------------------

void TPanel::hideEvent(QHideEvent *event) {
  TDockWidget::hideEvent(event);
  updateGripCursor(0);
}

//-----------------------------------------------------------------------------

QPoint TPanel::undockGrabOffset(const QPoint &offset) {
  if (!isCustomPanel() || !m_panelTitleBar) return offset;
  const QPoint origin = m_panelTitleBar->pos();
  const QPoint wanted = m_compactApplied ? offset : origin + offset;
  return nearestDragPoint(wanted) - origin;
}

//-----------------------------------------------------------------------------

QPoint TPanel::nearestDragPoint(const QPoint &p) const {
  constexpr int grip = 8;
  const auto clampTo = [](const QRect &r, const QPoint &pt) {
    return QPoint(qBound(r.left(), pt.x(), r.right()),
                  qBound(r.top(), pt.y(), r.bottom()));
  };
  QRect inner =
      m_compactApplied ? rect().adjusted(grip, grip, -grip, -grip) : rect();
  if (!inner.isValid()) inner = rect();

  if (m_panelTitleBar->isVisibleTo(this)) {
    QRect zone = m_panelTitleBar->geometry().intersected(inner);
    if (!zone.isValid()) zone = m_panelTitleBar->geometry();
    return clampTo(zone, p);
  }

  QRect best;
  int bestDistance = -1;
  QWidget *content = widget();
  const QList<QWidget *> children =
      content ? content->findChildren<QWidget *>() : QList<QWidget *>();
  for (QWidget *child : children) {
    if (!child->isVisibleTo(this) || compactDragExempt(child)) continue;
    bool leaf = true;
    for (QWidget *sub :
         child->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
      if (sub->isVisibleTo(this)) leaf = false;
    if (!leaf) continue;
    const QRect r = QRect(child->mapTo(this, QPoint(0, 0)), child->size())
                        .intersected(inner);
    if (r.width() < 4 || r.height() < 4) continue;
    const int distance = (clampTo(r, p) - p).manhattanLength();
    if (bestDistance < 0 || distance < bestDistance) {
      bestDistance = distance;
      best         = r;
    }
  }
  return clampTo(best.isValid() ? best : inner, p);
}

//-----------------------------------------------------------------------------
void TPanel::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  positionCompactTitleBar();
}

//-----------------------------------------------------------------------------

int TPanel::isResizeGrip(QPoint p) {
  if (!isCustomPanel() || !m_compactApplied || !isFloating() || m_dragging)
    return TDockWidget::isResizeGrip(p);
  return compactResizeMargin(p);
}

//-----------------------------------------------------------------------------
/*! activate the panel and set focus specified widget when mouse enters
 */
void TPanel::enterEvent(QEvent *event) {
  // Only when Toonz application is active
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

    // Some panels (e.g. Viewer, StudioPalette, Palette, ColorModel) are
    // activated when mouse enters. Viewer is activatable only when being
    // docked.
    // Active windows will NOT switch when the current active window is dialog.
    if (qobject_cast<QDialog *>(w) == nullptr && isActivatableOnEnter())
      activateWindow();
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
  const TFilePath savePath =
      ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("Panels"));

  if (!settings.childGroups().contains(QString::fromStdString(m_panelType)))
    return;

  settings.beginGroup(QString::fromStdString(m_panelType));

  QRect geom = settings.value(QStringLiteral("geometry"), geometry()).toRect();

  // Check if the geometry is visible on any available screen (modern API)
  bool visible = false;
  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen->availableGeometry().intersects(geom)) {
      visible = true;
      break;
    }
  }
  if (visible) setGeometry(geom);

  // load optional settings
  if (auto *persistent = dynamic_cast<SaveLoadQSettings *>(widget()))
    persistent->load(settings);
}

//-----------------------------------------------------------------------------
// if the panel has no contents to be zoomed, simply resize the panel here
// currently only Flipbook and Color Model panels support resizing of contents
void TPanel::zoomContentsAndFitGeometry(bool forward) {
  if (!isFloating()) return;

  auto getScreen = [&]() -> QScreen * {
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
  const QRect screenGeom = screen->availableGeometry();

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

//-----------------------------------------------------------------------------

void TPanel::setRoomBound(bool bound) {
  m_isRoomBound = bound;
  if (m_roomBindButton) m_roomBindButton->setPressed(bound);
}

//-----------------------------------------------------------------------------

void TPanel::setBoundRoomName(const QString &roomName) {
  m_boundRoomName = roomName;
}

//-----------------------------------------------------------------------------

void TPanel::addRoomBindButton() {
  // Prevent adding button twice
  if (m_roomBindButton) return;

  // Check preference to see if room bind buttons should be shown
  Preferences *prefs = Preferences::instance();
  if (!prefs->getBoolValue(showRoomBindButtons)) return;

  auto *titleBar = getTitleBar();
  if (!titleBar) return;

  // Create toggle button with simple circle indicator
  m_roomBindButton = new TPanelTitleBarButtonForBindToRoom(titleBar);
  m_roomBindButton->setToolTip(QObject::tr("Bind to Current Room"));
  m_roomBindButton->setPressed(
      m_isRoomBound);  // Restore state if loaded from file

  // Position: -60 pixels from right edge (before close button)
  titleBar->add(QPoint(-60, 0), m_roomBindButton);
  // Sync initial visibility with current dock state at button creation time.
  m_roomBindButton->setVisible(isFloating());

  // Connect toggle signal to handle room binding state changes (modern lambda)
  connect(m_roomBindButton, &TPanelTitleBarButton::toggled,
          [this](bool checked) {
            auto *mw =
                dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
            if (!mw) return;

            Room *currentRoom = mw->getCurrentRoom();
            if (!currentRoom) return;

            // Update binding state
            setRoomBound(checked);
            if (checked) {
              // Bind panel to current room
              setBoundRoomName(currentRoom->getName());
              // Update visibility immediately for all bound panels
              mw->updatePanelVisibility();
            } else {
              // Unbind panel from room
              setBoundRoomName(QString());
              // Show panel if it was hidden
              if (isHidden()) show();
            }
          });
}

//-----------------------------------------------------------------------------

void TPanel::setFloatingAppearance() {
  if (!isCustomPanel()) {
    setFloatingMargin(kFloatingMargin);
    TDockWidget::setFloatingAppearance();
    if (m_roomBindButton) m_roomBindButton->setVisible(true);
    return;
  }

  const bool fromDock = !m_inFloatingChrome;
  const int margin    = m_compactFloating ? 0 : kFloatingMargin;
  if (m_inFloatingChrome)
    setFloatingChromeMargin(margin);
  else
    setFloatingMargin(margin);
  TDockWidget::setFloatingAppearance();
  if (m_roomBindButton) m_roomBindButton->setVisible(true);
  applyDisplayState(true);
  if (fromDock) {
    m_restoreSizePending = true;
    restoreFloatingSize();
  }
  scheduleDisplaySync();
}

//-----------------------------------------------------------------------------

void TPanel::setDockedAppearance() {
  if (isCustomPanel()) {
    rememberFloatingSize();
    m_restoreSizePending = false;
  }
  TDockWidget::setDockedAppearance();
  setFloatingMargin(kFloatingMargin);
  if (m_roomBindButton) m_roomBindButton->setVisible(false);
  if (!isCustomPanel()) return;
  applyDisplayState(false);
  scheduleDisplaySync();
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

//=============================================================================
// Implementation of the Constructors of the Specialized Classes
//-----------------------------------------------------------------------------

// Constructor for BindToRoom
TPanelTitleBarButtonForBindToRoom::TPanelTitleBarButtonForBindToRoom(
    QWidget *parent)
    : TPanelTitleBarButton(parent, QStringLiteral()) {}

// Constructor for SafeArea
TPanelTitleBarButtonForSafeArea::TPanelTitleBarButtonForSafeArea(
    QWidget *parent, const QString &standardPixmapName)
    : TPanelTitleBarButton(parent, standardPixmapName) {}

// Constructor for Preview
TPanelTitleBarButtonForPreview::TPanelTitleBarButtonForPreview(
    QWidget *parent, const QString &standardPixmapName)
    : TPanelTitleBarButton(parent, standardPixmapName) {}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::paintEvent(QPaintEvent *event) {
  if (m_standardPixmapName.isEmpty()) return;

  QPainter p(this);

  // Determine icon state
  QIcon::Mode mode   = m_rollover ? QIcon::Active : QIcon::Normal;
  QIcon::State state = m_pressed ? QIcon::On : QIcon::Off;

  // Get DPR and sizes
  const qreal dpr    = getDevicePixelRatio(this);
  auto &tm           = ThemeManager::getInstance();
  const QSize iconSz = tm.getIconSize(m_standardPixmapName);
  const QSize btnSz  = size() * dpr;

  // Cache key with DPR
  const QString cacheKey =
      generateCacheKey(m_standardPixmapName, btnSz, mode, state) +
      QString::number(dpr);

  // Try cache first
  QPixmap finalPm = getFromPixmapCache(cacheKey);
  if (finalPm.isNull()) {
    QPixmap pm = SvgIconEngine(m_standardPixmapName, false, dpr, iconSz)
                     .pixmap(iconSz, mode, state);

    if (!pm.isNull()) {
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
        // Only overwrite if valid, or keep the safe transparent color
        if (overColor.isValid()) bgColor = overColor;
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
// TPanelTitleBarButtonForBindToRoom
//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForBindToRoom::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Get title bar to access theme colors
  auto *titleBar = qobject_cast<TPanelTitleBar *>(parentWidget());

  // Determine circle color based on state
  QColor circleColor;
  if (m_pressed) {
    // When bound, use active title color
    circleColor =
        titleBar ? titleBar->getActiveTitleColor() : QColor(0, 150, 255);
  } else {
    // When not bound, use dimmed title color
    circleColor = titleBar ? titleBar->getTitleColor() : QColor(160, 160, 160);
    circleColor.setAlpha(m_rollover ? 200 : 120);  // Fade when not hovering
  }

  // Draw small circle in the center of the button
  const QRect rect   = this->rect();
  const int diameter = 8;  // Small circle (8px diameter)
  const int centerX  = rect.center().x();
  const int centerY  = rect.center().y();

  p.setPen(Qt::NoPen);
  p.setBrush(circleColor);
  p.drawEllipse(QPoint(centerX, centerY), diameter / 2, diameter / 2);
}

//=============================================================================
// TPanelTitleBarButtonForSafeArea
//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::getSafeAreaNameList(
    QList<QString> &nameList) const {
  TFilePath fp                      = TEnv::getConfigDir();
  const QString currentSafeAreaName = QString::fromStdString(EnvSafeAreaName);

  const std::string safeAreaFileName = "safearea.ini";

  while (!TFileStatus(fp + safeAreaFileName).doesExist() && !fp.isRoot() &&
         fp.getParentDir() != TFilePath())
    fp = fp.getParentDir();

  fp = fp + safeAreaFileName;

  if (TFileStatus(fp).doesExist()) {
    QSettings settings(toQString(fp), QSettings::IniFormat);

    // find the current safearea name from the list
    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
      settings.beginGroup(group);
      nameList.push_back(
          settings.value(QStringLiteral("name"), QString()).toString());
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
  for (const QString &name : safeAreaNameList) {
    QAction *action = menu.addAction(name);
    action->setData(name);
    connect(action, &QAction::triggered, this,
            &TPanelTitleBarButtonForSafeArea::onSetSafeArea);
    if (name == QString::fromStdString(EnvSafeAreaName)) {
      action->setCheckable(true);
      action->setChecked(true);
    }
  }

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::onSetSafeArea() {
  const QString safeAreaName =
      qobject_cast<QAction *>(sender())->data().toString();
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
  const QStringList behaviorsStrList = {tr("Current frame"),
                                        tr("All preview range frames"),
                                        tr("Selected cells - Auto play")};

  // QActionGroup with menu as parent ensures proper cleanup
  auto *behaviorGroup = new QActionGroup(&menu);
  behaviorGroup->setExclusive(true);

  for (int i = 0; i < behaviorsStrList.size(); ++i) {
    QAction *action = menu.addAction(behaviorsStrList.at(i));
    action->setData(i);
    connect(action, &QAction::triggered, this,
            &TPanelTitleBarButtonForPreview::onSetPreviewBehavior);
    action->setCheckable(true);
    behaviorGroup->addAction(action);
    if (i == EnvViewerPreviewBehavior) action->setChecked(true);
  }

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForPreview::onSetPreviewBehavior() {
  const int behaviorId = qobject_cast<QAction *>(sender())->data().toInt();
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

TPanelTitleBarButtonSet::TPanelTitleBarButtonSet() = default;

void TPanelTitleBarButtonSet::add(TPanelTitleBarButton *button) {
  m_buttons.push_back(button);
}

void TPanelTitleBarButtonSet::select(TPanelTitleBarButton *button) {
  for (auto *btn : m_buttons) btn->setPressed(button == btn);
  emit selected(button->getId());
}

bool TPanelTitleBarButtonSet::select(int id) {
  for (auto *btn : m_buttons) {
    if (btn->getId() == id) {
      select(btn);
      return true;
    }
  }
  return false;
}

//=============================================================================
// PaneTitleBar
//-----------------------------------------------------------------------------

TPanelTitleBar::TPanelTitleBar(QWidget *parent,
                               TDockWidget::Orientation orientation)
    : QFrame(parent), m_closeButtonHighlighted(false), m_compact(false) {
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
}

//-----------------------------------------------------------------------------

QSize TPanelTitleBar::minimumSizeHint() const { return QSize(20, 18); }

//-----------------------------------------------------------------------------

void TPanelTitleBar::setCompact(bool compact) {
  if (m_compact == compact) return;
  m_compact = compact;
  update();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::setSuppressed(bool suppressed) {
  if (m_suppressed == suppressed) return;
  m_suppressed = suppressed;
  QFrame::setVisible(!suppressed);
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::setVisible(bool visible) {
  QFrame::setVisible(visible && !m_suppressed);
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  const QRect rect = this->rect();

  bool isPanelActive = false;
  auto *dw           = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != nullptr);

  if (m_compact && dw->isFloating()) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(32, 32, 32, 210));
    painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 3, 3);
  } else if (!dw->isFloating()) {  // docked panel
    isPanelActive = dw->widgetInThisPanelIsFocused();
    qDrawBorderPixmap(&painter, rect, QMargins(3, 3, 3, 3),
                      isPanelActive ? m_activeBorderPm : m_borderPm);
  } else {  // floating panel
    isPanelActive = isActiveWindow();
    qDrawBorderPixmap(&painter, rect, QMargins(3, 3, 3, 3),
                      isPanelActive ? m_floatActiveBorderPm : m_floatBorderPm);
  }

  if (dw->getOrientation() == TDockWidget::vertical && !m_compact) {
    const QString titleText = painter.fontMetrics().elidedText(
        dw->windowTitle(), Qt::ElideRight, rect.width() - 50);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(isPanelActive ? m_activeTitleColor : m_titleColor);
    painter.drawText(QPointF(8, 13), titleText);
  }

  if (dw->isFloating()) {
    const QPoint btnPos(rect.right() - 19, rect.top());
    const qreal dpr    = getDevicePixelRatio(this);
    const QSize btnSz  = minimumSizeHint() * dpr;
    const QSize iconSz = QSize(16, 16);

    // Icon mode and background color
    const bool highlighted = m_closeButtonHighlighted;
    const QIcon::Mode mode = highlighted ? QIcon::Active : QIcon::Normal;
    const QColor bgColor = highlighted ? getCloseOverColor() : Qt::transparent;

    // Cache key
    const QString cacheKey = generateCacheKey(QStringLiteral("closewindow"),
                                              btnSz, mode, QIcon::Off) +
                             QString::number(dpr);

    // Check cache first
    QPixmap finalPm = getFromPixmapCache(cacheKey);
    if (!finalPm.isNull()) {
      painter.drawPixmap(btnPos, finalPm);
      return;
    }

    // Load icon at logical size
    QPixmap pm =
        SvgIconEngine(QStringLiteral("closewindow"), false, dpr, iconSz)
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

void TPanelTitleBar::contextMenuEvent(QContextMenuEvent *event) {
  auto *panel = qobject_cast<TPanel *>(parentWidget());
  if (panel && panel->isCustomPanel()) {
    panel->execContextMenu(event->globalPos());
    event->accept();
    return;
  }
  event->ignore();
}

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
    const QRect rect = this->rect();
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
  if (m_compact) {
    int x = 0;
    for (const auto &[pos, widget] : m_buttons) {
      Q_UNUSED(pos)
      widget->move(x, 0);
      x += widget->width();
    }
    return;
  }
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

  if (panelType.startsWith(QStringLiteral("Custom_"))) {
    QString customType = panelType.mid(7);
    return CustomPanelManager::instance()->createCustomPanel(customType,
                                                             parent);
  }

  // Fallback: generic panel
  TPanel *panel = new TPanel(parent);
  panel->setPanelType(panelType.toStdString());
  panel->addRoomBindButton();
  return panel;
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent) {
  auto *panel = new TPanel(parent);
  panel->setObjectName(getPanelType());
  panel->setWindowTitle(getPanelType());
  initialize(panel);

  // Enable room binding feature for all native panels
  panel->addRoomBindButton();

  return panel;
}

//-----------------------------------------------------------------------------
