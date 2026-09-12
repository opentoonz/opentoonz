

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "tsystem.h"
#include "commandbarpopup.h"
#include "pane.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/toonzfolders.h"
// Qt includes
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QtDebug>
#include <QMenuBar>
#include <QContextMenuEvent>
#include <QActionGroup>
#include <QLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QResizeEvent>
#include <QStyleOptionToolBar>
#include <QTimer>
#include <QToolButton>

namespace {
constexpr int kCommandBarFloatingThickness       = 36;
constexpr int kCommandBarDockedThickness         = 26;
constexpr int kCommandBarFloatingFrameDelta      = 10;
constexpr int kCommandBarTitleBarThickness       = 18;
constexpr int kCommandBarHorizontalGripWidth     = 20;
constexpr int kCommandBarVerticalExtensionHeight = 16;
constexpr int kCommandBarCompactButtonSize       = 20;
constexpr int kCommandBarCompactGripSize         = 4;
}

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible, bool isXsheetToolbar)
    : QToolBar(parent)
    , m_isCollapsible(isCollapsible)
    , m_isXsheetToolbar(isXsheetToolbar)
    , m_roomStateLoaded(false)
    , m_initialFloatingSizeApplied(false)
    , m_userCompact(false)
    , m_autoCompact(false)
    , m_compactPresentation(false)
    , m_compactTransition(false)
    , m_compactUpdatePending(false)
    , m_compactThreshold(0)
    , m_expandedLongSide(0)
    , m_compactMenu(nullptr)
    , m_compactButton(nullptr)
    , m_compactWidgetAction(nullptr) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");
  fillToolbar(this, isXsheetToolbar);
  setIconSize(QSize(20, 20));

  connect(this, &QToolBar::orientationChanged, this,
          &CommandBar::onOrientationChanged);
  onOrientationChanged(orientation());
}

//-----------------------------------------------------------------------------

void CommandBar::fillToolbar(CommandBar *toolbar, bool isXsheetToolbar) {
  toolbar->clear();
  TFilePath personalPath;
  if (isXsheetToolbar) {
    personalPath =
        ToonzFolder::getMyModuleDir() + TFilePath("xsheettoolbar.xml");
  } else {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  }
  if (!TSystem::doesExistFileOrLevel(personalPath)) {
    if (isXsheetToolbar) {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("xsheettoolbar.xml");
    } else {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }
  QFile file(toQString(personalPath));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    buildDefaultToolbar(toolbar);
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName    = reader.readElementText();
          std::string cmdStr = cmdName.toStdString();
          QAction *action =
              CommandManager::instance()->getAction(cmdStr.c_str());
          if (action) toolbar->addAction(action);
        } else if (reader.name() == "separator") {
          toolbar->addSeparator();
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  } else {
    reader.raiseError(QObject::tr("Cannot Read XML File"));
  }

  if (reader.hasError()) {
    buildDefaultToolbar(toolbar);
    return;
  }
}

//-----------------------------------------------------------------------------

void CommandBar::buildDefaultToolbar(CommandBar *toolbar) {
  toolbar->clear();
  TApp *app = TApp::instance();
  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    toolbar->addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    toolbar->addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    toolbar->addAction(newRasterLevel);
    toolbar->addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    toolbar->addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    toolbar->addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    toolbar->addAction(reframeThrees);

    toolbar->addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    toolbar->addAction(repeat);

    toolbar->addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    toolbar->addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    toolbar->addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    toolbar->addAction(leave);
    QAction *editInPlace =
        CommandManager::instance()->getAction("MI_ToggleEditInPlace");
    toolbar->addAction(editInPlace);
  }
}

//-----------------------------------------------------------------------------

void CommandBar::contextMenuEvent(QContextMenuEvent *event) {
  showContextMenu(event->globalPos());
}

//-----------------------------------------------------------------------------

void CommandBar::showContextMenu(const QPoint &globalPos) {
  QMenu menu(this);
  QAction *customizeCommandBar = menu.addAction(tr("Customize Command Bar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  if (!m_isXsheetToolbar) {
    menu.addSeparator();

    QMenu *orientationMenu = menu.addMenu(tr("Orientation"));
    QActionGroup *orientationGroup = new QActionGroup(orientationMenu);
    orientationGroup->setExclusive(true);

    QAction *horizontalAction = orientationMenu->addAction(tr("Horizontal"));
    horizontalAction->setCheckable(true);
    horizontalAction->setChecked(orientation() == Qt::Horizontal);
    orientationGroup->addAction(horizontalAction);

    QAction *verticalAction = orientationMenu->addAction(tr("Vertical"));
    verticalAction->setCheckable(true);
    verticalAction->setChecked(orientation() == Qt::Vertical);
    orientationGroup->addAction(verticalAction);

    TPanel *panel = qobject_cast<TPanel *>(parentWidget());
    orientationMenu->setEnabled(!panel || panel->isFloating());

    connect(horizontalAction, &QAction::triggered, this,
            [this]() { setOrientation(Qt::Horizontal); });
    connect(verticalAction, &QAction::triggered, this,
            [this]() { setOrientation(Qt::Vertical); });

    QAction *compactAction = menu.addAction(tr("Compact"));
    compactAction->setCheckable(true);
    compactAction->setChecked(m_userCompact);
    connect(compactAction, &QAction::toggled, this,
            [this](bool checked) { setUserCompact(checked); });
  }

  menu.exec(globalPos);
}

//-----------------------------------------------------------------------------

void CommandBar::save(QSettings &settings) const {
  if (m_isXsheetToolbar) return;

  settings.setValue(QStringLiteral("orientation"),
                    orientation() == Qt::Vertical
                        ? QStringLiteral("Vertical")
                        : QStringLiteral("Horizontal"));
  settings.setValue(QStringLiteral("compact"), m_userCompact);
  if (m_expandedLongSide > 0)
    settings.setValue(QStringLiteral("compactExpandedLength"),
                      m_expandedLongSide);
  else
    settings.remove(QStringLiteral("compactExpandedLength"));
}

//-----------------------------------------------------------------------------

void CommandBar::load(QSettings &settings) {
  if (m_isXsheetToolbar) return;

  m_roomStateLoaded = settings.contains(QStringLiteral("roomBound"));
  m_userCompact = settings.value(QStringLiteral("compact"), false).toBool();
  m_expandedLongSide =
      settings.value(QStringLiteral("compactExpandedLength"), 0).toInt();

  const QString savedOrientation =
      settings.value(QStringLiteral("orientation"),
                     QStringLiteral("Horizontal"))
          .toString();
  const Qt::Orientation saved =
      savedOrientation.compare(QStringLiteral("Vertical"), Qt::CaseInsensitive) ==
              0
          ? Qt::Vertical
          : Qt::Horizontal;

  if (orientation() == saved)
    onOrientationChanged(saved);
  else
    setOrientation(saved);

  if (!m_roomStateLoaded) applyInitialFloatingSize();
  scheduleCompactUpdate();
}

//-----------------------------------------------------------------------------

void CommandBar::resizeEvent(QResizeEvent *event) {
  QToolBar::resizeEvent(event);
  scheduleCompactUpdate();
}

//-----------------------------------------------------------------------------

void CommandBar::showEvent(QShowEvent *event) {
  QToolBar::showEvent(event);
  if (!m_roomStateLoaded && !m_initialFloatingSizeApplied) {
    onOrientationChanged(orientation());
    applyInitialFloatingSize();
  }
  scheduleCompactUpdate();
}

//-----------------------------------------------------------------------------

void CommandBar::applyInitialFloatingSize() {
  if (m_initialFloatingSizeApplied || m_roomStateLoaded) return;

  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (!panel || !panel->isFloating()) return;

  QScreen *screen = QGuiApplication::screenAt(panel->frameGeometry().center());
  if (!screen) screen = QGuiApplication::primaryScreen();
  if (!screen) return;

  QSize panelSize       = panel->size();
  const QRect available = screen->availableGeometry();
  if (orientation() == Qt::Vertical)
    panelSize.setHeight(available.height() / 2);
  else
    panelSize.setWidth(available.width() / 2);

  panel->resize(panelSize);
  m_initialFloatingSizeApplied = true;
}

//-----------------------------------------------------------------------------

void CommandBar::applyCompactPanelSize(bool compact) {
  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (!panel) return;

  const bool vertical = orientation() == Qt::Vertical;
  const int frameDelta = panel->isFloating() ? kCommandBarFloatingFrameDelta : 0;
  const int compactLongSide = kCommandBarCompactButtonSize +
                              kCommandBarCompactGripSize + frameDelta;
  const int currentLongSide = vertical ? panel->height() : panel->width();

  if (compact) {
    if (m_expandedLongSide <= 0 && currentLongSide > compactLongSide)
      m_expandedLongSide = qMax(currentLongSide, m_compactThreshold);

    if (vertical)
      panel->setFixedHeight(compactLongSide);
    else
      panel->setFixedWidth(compactLongSide);
    return;
  }

  const int minimumLongSide = panel->isFloating() ? kCommandBarFloatingFrameDelta : 0;
  if (vertical) {
    panel->setMinimumHeight(minimumLongSide);
    panel->setMaximumHeight(QWIDGETSIZE_MAX);
  } else {
    panel->setMinimumWidth(minimumLongSide);
    panel->setMaximumWidth(QWIDGETSIZE_MAX);
  }

  int restoreLongSide = m_expandedLongSide;
  if (restoreLongSide <= compactLongSide)
    restoreLongSide = qMax(m_compactThreshold, compactLongSide + 1);

  if (restoreLongSide > 0) {
    QSize panelSize = panel->size();
    if (vertical)
      panelSize.setHeight(restoreLongSide);
    else
      panelSize.setWidth(restoreLongSide);
    panel->resize(panelSize);
  }
  m_expandedLongSide = 0;
}

//-----------------------------------------------------------------------------

int CommandBar::compactThreshold() const {
  if (m_compactPresentation) return m_compactThreshold;

  QStyleOptionToolBar option;
  initStyleOption(&option);

  const int margin =
      style()->pixelMetric(QStyle::PM_ToolBarItemMargin, &option, this) +
      style()->pixelMetric(QStyle::PM_ToolBarFrameWidth, &option, this);
  const int spacing =
      style()->pixelMetric(QStyle::PM_ToolBarItemSpacing, &option, this);
  const int extension =
      style()->pixelMetric(QStyle::PM_ToolBarExtensionExtent, &option, this);

  const QList<QAction *> toolbarActions = actions();
  int totalItems                       = 0;
  for (QAction *action : toolbarActions) {
    if (action->isVisible() && widgetForAction(action)) ++totalItems;
  }

  int extent   = margin * 2;
  int items    = 0;
  int commands = 0;
  for (QAction *action : toolbarActions) {
    if (!action->isVisible()) continue;
    QWidget *widget = widgetForAction(action);
    if (!widget) continue;

    if (items) extent += spacing;
    const QSize hint = widget->sizeHint();
    extent += orientation() == Qt::Horizontal ? hint.width() : hint.height();
    ++items;

    if (!action->isSeparator()) ++commands;
    if (commands == 2) {
      if (totalItems > items) extent += spacing + extension;
      return extent;
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

void CommandBar::scheduleCompactUpdate() {
  if (m_isXsheetToolbar || m_compactUpdatePending) return;

  m_compactUpdatePending = true;
  QTimer::singleShot(0, this, [this]() {
    m_compactUpdatePending = false;
    updateCompactState();
  });
}

//-----------------------------------------------------------------------------

void CommandBar::updateCompactState() {
  if (m_isXsheetToolbar || m_compactTransition) return;

  if (!m_compactPresentation) {
    const int threshold = compactThreshold();
    if (threshold > 0) m_compactThreshold = threshold;
  }

  int extent = orientation() == Qt::Horizontal ? width() : height();
  if (m_compactPresentation && !m_userCompact) {
    const int normalGripDelta =
        orientation() == Qt::Horizontal
            ? kCommandBarHorizontalGripWidth - kCommandBarCompactGripSize
            : kCommandBarTitleBarThickness - kCommandBarCompactGripSize;
    extent = qMax(0, extent - normalGripDelta);
  }

  m_autoCompact = !m_userCompact && m_compactThreshold > 0 &&
                  extent < m_compactThreshold;

  setCompactPresentation(m_userCompact || m_autoCompact);
  if (m_userCompact && m_compactPresentation) applyCompactPanelSize(true);
}

//-----------------------------------------------------------------------------

void CommandBar::setUserCompact(bool compact) {
  if (m_userCompact == compact) return;
  m_userCompact = compact;

  if (compact) {
    updateCompactState();
    applyCompactPanelSize(true);
    if (m_compactPresentation) rebuildCompactMenu();
  } else {
    applyCompactPanelSize(false);
    updateCompactState();
  }
}

//-----------------------------------------------------------------------------

void CommandBar::rebuildCompactMenu() {
  if (!m_compactMenu) return;

  m_compactMenu->clear();
  for (QAction *action : m_compactActions) m_compactMenu->addAction(action);

  if (!m_compactActions.isEmpty() &&
      !m_compactActions.last()->isSeparator())
    m_compactMenu->addSeparator();

  if (m_userCompact) {
    QAction *expandAction = m_compactMenu->addAction(tr("Expand"));
    connect(expandAction, &QAction::triggered, this,
            [this]() { setUserCompact(false); }, Qt::QueuedConnection);
  }

  QAction *customizeAction =
      m_compactMenu->addAction(tr("Customize Command Bar"));
  connect(customizeAction, &QAction::triggered, this,
          &CommandBar::doCustomizeCommandBar);
}

//-----------------------------------------------------------------------------

void CommandBar::setCompactPresentation(bool compact) {
  if (m_compactPresentation == compact || m_compactTransition) return;

  m_compactTransition   = true;
  m_compactPresentation = compact;

  if (compact) {
    if (m_compactThreshold <= 0) m_compactThreshold = compactThreshold();

    m_compactActions = actions();
    m_compactMenu    = new QMenu(this);
    rebuildCompactMenu();

    for (QAction *action : m_compactActions) removeAction(action);

    m_compactButton = new QToolButton(this);
    m_compactButton->setObjectName(QStringLiteral("CommandBarCompactButton"));
    m_compactButton->setArrowType(Qt::DownArrow);
    m_compactButton->setPopupMode(QToolButton::InstantPopup);
    m_compactButton->setMenu(m_compactMenu);
    m_compactButton->setAutoRaise(true);
    m_compactButton->setFixedSize(kCommandBarCompactButtonSize,
                                  kCommandBarCompactButtonSize);
    m_compactButton->setStyleSheet(
        "QToolButton#CommandBarCompactButton { margin: 0; padding: 0; }");
    m_compactButton->setToolTip(tr("Command Bar"));
    m_compactButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_compactButton, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
              if (m_compactButton)
                showContextMenu(m_compactButton->mapToGlobal(pos));
            });

    m_compactWidgetAction = addWidget(m_compactButton);
  } else {
    const QList<QAction *> restoreActions = m_compactActions;
    m_compactActions.clear();

    if (m_compactWidgetAction) {
      removeAction(m_compactWidgetAction);
      m_compactWidgetAction->deleteLater();
      m_compactWidgetAction = nullptr;
      m_compactButton       = nullptr;
    }
    if (m_compactMenu) {
      m_compactMenu->deleteLater();
      m_compactMenu = nullptr;
    }

    for (QAction *action : restoreActions) addAction(action);
  }

  onOrientationChanged(orientation());

  if (layout()) {
    layout()->invalidate();
    layout()->activate();
  }
  updateGeometry();
  m_compactTransition = false;
}

//-----------------------------------------------------------------------------

void CommandBar::onOrientationChanged(Qt::Orientation orientation) {
  if (m_isXsheetToolbar) return;

  const bool vertical = orientation == Qt::Vertical;

  // QToolBar resets its size policy when orientation changes.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Vertical mode overrides horizontal-biased theme spacing.
  if (vertical) {
    setStyleSheet(
        QStringLiteral(
            "QToolBar#CommandBar { margin: 0; padding: 0; border: 0; }"
            "QToolBar#CommandBar::separator:vertical {"
            "  margin: 2px 2px 0 2px;"
            "}"
            "QToolBar#CommandBar QToolButton {"
            "  margin: 2px 0 0 0;"
            "  padding: 0;"
            "  min-width: 20px;"
            "  min-height: 20px;"
            "}"
            "QToolBar#CommandBar QToolButton#qt_toolbar_ext_button {"
            "  margin: 0;"
            "  padding: 0;"
            "  min-height: %1px;"
            "  max-height: %1px;"
            "}")
            .arg(kCommandBarVerticalExtensionHeight));
  } else {
    setStyleSheet(QString());
  }

  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (!panel) return;

  panel->setOrientation(vertical ? TDockWidget::vertical
                                 : TDockWidget::horizontal);

  const bool floating = panel->isFloating();
  const int thickness = floating ? kCommandBarFloatingThickness
                                 : kCommandBarDockedThickness;
  const int minimumLongSide = floating ? kCommandBarFloatingFrameDelta : 0;
  const int titleBarThickness =
      m_compactPresentation ? kCommandBarCompactGripSize
                            : kCommandBarTitleBarThickness;
  const int horizontalGripWidth =
      m_compactPresentation ? kCommandBarCompactGripSize
                            : kCommandBarHorizontalGripWidth;

  TPanelTitleBar *titleBar = panel->getTitleBar();
  if (vertical) {
    panel->setMinimumHeight(minimumLongSide);
    panel->setMaximumHeight(QWIDGETSIZE_MAX);
    panel->setFixedWidth(thickness);

    if (titleBar) {
      titleBar->setStyleSheet(
          QStringLiteral(
              "TPanelTitleBar {"
              "  min-width: 0;"
              "  max-width: %1;"
              "  min-height: %2;"
              "  max-height: %2;"
              "  border-right: 0;"
              "  border-bottom: 0;"
              "  border-radius: 0;"
              "}")
              .arg(QWIDGETSIZE_MAX)
              .arg(titleBarThickness));
      titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      titleBar->setMinimumWidth(0);
      titleBar->setMaximumWidth(QWIDGETSIZE_MAX);
      titleBar->setFixedHeight(titleBarThickness);
    }
  } else {
    panel->setMinimumWidth(minimumLongSide);
    panel->setMaximumWidth(QWIDGETSIZE_MAX);
    panel->setFixedHeight(thickness);

    if (titleBar) {
      titleBar->setStyleSheet(
          QStringLiteral(
              "TPanelTitleBar {"
              "  min-width: %1;"
              "  max-width: %1;"
              "  min-height: 0;"
              "  max-height: %2;"
              "}")
              .arg(horizontalGripWidth)
              .arg(QWIDGETSIZE_MAX));
      titleBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      titleBar->setFixedWidth(horizontalGripWidth);
      titleBar->setMinimumHeight(0);
      titleBar->setMaximumHeight(QWIDGETSIZE_MAX);
    }
  }

  if (titleBar) titleBar->updateGeometry();
  updateGeometry();
  panel->updateGeometry();
  if (layout()) {
    layout()->invalidate();
    layout()->activate();
  }
  if (panel->layout()) {
    panel->layout()->invalidate();
    panel->layout()->activate();
  }
  if (m_userCompact && m_compactPresentation) applyCompactPanelSize(true);
  scheduleCompactUpdate();
}

//-----------------------------------------------------------------------------

void CommandBar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup();

  if (cbPopup->exec()) {
    if (m_compactPresentation) setCompactPresentation(false);
    fillToolbar(this);
    m_compactThreshold = 0;
    updateCompactState();
  }
  delete cbPopup;
}
