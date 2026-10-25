

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
#include <QShowEvent>

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible, bool isXsheetToolbar)
    : QToolBar(parent)
    , m_isCollapsible(isCollapsible)
    , m_isXsheetToolbar(isXsheetToolbar) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");
  fillToolbar(this, isXsheetToolbar);
  setIconSize(QSize(20, 20));

  if (!m_isXsheetToolbar)
    connect(this, &QToolBar::orientationChanged, this,
            &CommandBar::onOrientationChanged);
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
  QMenu menu(this);
  QAction *customizeCommandBar = menu.addAction(tr("Customize Command Bar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  if (!m_isXsheetToolbar) {
    menu.addSeparator();
    QMenu *orientationMenu         = menu.addMenu(tr("Orientation"));
    QActionGroup *orientationGroup = new QActionGroup(orientationMenu);

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
  }

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CommandBar::showEvent(QShowEvent *event) {
  QToolBar::showEvent(event);
  // The toolbar is constructed before setWidget() attaches it to its panel.
  // Apply the grip constraints once that parent and its geometry are available.
  if (!m_isXsheetToolbar) onOrientationChanged(orientation());
}

//-----------------------------------------------------------------------------

void CommandBar::save(QSettings &settings) const {
  if (m_isXsheetToolbar) return;
  settings.setValue(QStringLiteral("orientation"),
                    orientation() == Qt::Vertical
                        ? QStringLiteral("Vertical")
                        : QStringLiteral("Horizontal"));
}

//-----------------------------------------------------------------------------

void CommandBar::load(QSettings &settings) {
  if (m_isXsheetToolbar) return;
  const QString savedOrientation =
      settings
          .value(QStringLiteral("orientation"), QStringLiteral("Horizontal"))
          .toString();
  const Qt::Orientation saved =
      savedOrientation.compare(QStringLiteral("Vertical"),
                               Qt::CaseInsensitive) == 0
          ? Qt::Vertical
          : Qt::Horizontal;
  if (orientation() == saved)
    onOrientationChanged(saved);
  else
    setOrientation(saved);

  // Floating panel geometry is restored before these orientation constraints.
  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (panel && settings.contains(QStringLiteral("geometry")))
    panel->resize(settings.value(QStringLiteral("geometry")).toRect().size());
}

//-----------------------------------------------------------------------------

void CommandBar::onOrientationChanged(Qt::Orientation orientation) {
  if (m_isXsheetToolbar) return;
  const bool vertical = orientation == Qt::Vertical;

  // QToolBar resets its size policy when orientation changes.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // The existing themes use horizontal toolbar spacing.
  setStyleSheet(
      vertical ? QStringLiteral(
                     "QToolBar#CommandBar { margin: 0; padding: 0; border: 0; }"
                     "QToolBar#CommandBar::separator:vertical { margin: 2px "
                     "2px 0 2px; }"
                     "QToolBar#CommandBar QToolButton {"
                     "  margin: 2px 0 0 0; padding: 0; min-width: 20px; "
                     "min-height: 20px; }"
                     "QToolBar#CommandBar QToolButton#qt_toolbar_ext_button {"
                     "  margin: 0; padding: 0; min-height: 16px; max-height: "
                     "16px; }")
               : QString());

  TPanel *panel = qobject_cast<TPanel *>(parentWidget());
  if (!panel) return;

  const bool wasVertical = panel->getOrientation() == TDockWidget::vertical;
  const int length       = wasVertical ? panel->height() : panel->width();
  const int frame = panel->isFloating() ? 2 * panel->getFloatingMargin() : 0;
  const int thickness = 26 + frame;
  panel->setOrientation(vertical ? TDockWidget::vertical
                                 : TDockWidget::horizontal);

  TPanelTitleBar *titleBar = panel->getTitleBar();
  if (vertical) {
    panel->setMinimumHeight(frame);
    panel->setMaximumHeight(QWIDGETSIZE_MAX);
    panel->setFixedWidth(thickness);
    if (titleBar) {
      titleBar->setStyleSheet(
          QStringLiteral(
              "TPanelTitleBar {"
              " min-width: 0; max-width: %1;"
              " min-height: 18px; max-height: 18px;"
              " border-right: 0; border-bottom: 0; border-radius: 0; }")
              .arg(QWIDGETSIZE_MAX));
      titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      titleBar->setMinimumWidth(0);
      titleBar->setMaximumWidth(QWIDGETSIZE_MAX);
      titleBar->setFixedHeight(18);
    }
  } else {
    panel->setMinimumWidth(frame);
    panel->setMaximumWidth(QWIDGETSIZE_MAX);
    panel->setFixedHeight(thickness);
    if (titleBar) {
      titleBar->setStyleSheet(
          QStringLiteral("TPanelTitleBar {"
                         " min-width: 20px; max-width: 20px;"
                         " min-height: 0; max-height: %1; }")
              .arg(QWIDGETSIZE_MAX));
      titleBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      titleBar->setFixedWidth(20);
      titleBar->setMinimumHeight(0);
      titleBar->setMaximumHeight(QWIDGETSIZE_MAX);
    }
  }

  if (panel->isFloating() && wasVertical != vertical)
    panel->resize(vertical ? QSize(thickness, length)
                           : QSize(length, thickness));
  if (titleBar) titleBar->updateGeometry();
  updateGeometry();
  panel->updateGeometry();
  // Orientation changes alter both the toolbar and the panel's title-bar
  // constraints. Resolve both layouts before the next paint.
  if (layout()) {
    layout()->invalidate();
    layout()->activate();
  }
  if (panel->layout()) {
    panel->layout()->invalidate();
    panel->layout()->activate();
  }
}

//-----------------------------------------------------------------------------

void CommandBar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup();

  if (cbPopup->exec()) {
    fillToolbar(this);
  }
  delete cbPopup;
}
