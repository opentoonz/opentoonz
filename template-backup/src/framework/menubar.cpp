#include "menubar.h"

#include "menubarcommand.h"
#include "appcontext.h"
#include "mainwindow.h"

#include <QIcon>
#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QShortcut>
#include <QCheckBox>
#include <QActionGroup>
#include <QKeySequence>
#include <QtDebug>
#include <QMessageBox>

//=============================================================================
// RoomTabWidget
//-----------------------------------------------------------------------------

RoomTabWidget::RoomTabWidget(QWidget *parent)
    : QTabBar(parent)
    , m_clickedTabIndex(-1)
    , m_tabToDeleteIndex(-1)
    , m_renameTabIndex(-1)
    , m_renameTextField(new QLineEdit(this))
    , m_isLocked(false) {
  m_renameTextField->hide();
  connect(m_renameTextField, &QLineEdit::editingFinished, this,
          &RoomTabWidget::updateTabName);
}

//-----------------------------------------------------------------------------

RoomTabWidget::~RoomTabWidget() {}

//-----------------------------------------------------------------------------

void RoomTabWidget::swapIndex(int firstIndex, int secondIndex) {
  QString firstText = tabText(firstIndex);
  removeTab(firstIndex);
  insertTab(secondIndex, firstText);
  emit indexSwapped(firstIndex, secondIndex);

  setCurrentIndex(secondIndex);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mousePressEvent(QMouseEvent *event) {
  m_renameTextField->hide();
  if (event->button() == Qt::LeftButton) {
    m_clickedTabIndex = tabAt(event->pos());
    if (m_clickedTabIndex < 0) return;
    setCurrentIndex(m_clickedTabIndex);
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  if (event->buttons()) {
    int tabIndex = tabAt(event->pos());
    if (tabIndex == m_clickedTabIndex || tabIndex < 0 || tabIndex >= count() ||
        m_clickedTabIndex < 0)
      return;
    swapIndex(m_clickedTabIndex, tabIndex);
    m_clickedTabIndex = tabIndex;
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseReleaseEvent(QMouseEvent *event) {
  m_clickedTabIndex = -1;
}

//-----------------------------------------------------------------------------
/*! Set a text field with focus in event position to edit tab name.
 */
void RoomTabWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  int index = tabAt(event->pos());
  if (index < 0) return;
  m_renameTabIndex     = index;
  QLineEdit *fld = m_renameTextField;
  fld->setText(tabText(index));
  fld->setGeometry(tabRect(index));
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (m_isLocked) return;
  m_tabToDeleteIndex = -1;
  QMenu *menu        = new QMenu(this);
  QAction *newRoom   = menu->addAction(tr("New Room"));
  connect(newRoom, &QAction::triggered, this, &RoomTabWidget::addNewTab);

  int index = tabAt(event->pos());
  if (index >= 0) {
    m_tabToDeleteIndex = index;
    if (index != currentIndex()) {
      QAction *deleteRoom =
          menu->addAction(tr("Delete Room \"%1\"").arg(tabText(index)));
      connect(deleteRoom, &QAction::triggered, this, &RoomTabWidget::deleteTab);
    }
  }
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void RoomTabWidget::updateTabName() {
  int index = m_renameTabIndex;
  if (index < 0) return;
  m_renameTabIndex = -1;
  QString newName  = m_renameTextField->text();
  setTabText(index, newName);
  m_renameTextField->hide();
  emit renameTabRoom(index, newName);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::addNewTab() {
  insertTab(0, tr("Room"));
  emit insertNewTabRoom();
}

//-----------------------------------------------------------------------------

void RoomTabWidget::deleteTab() {
  assert(m_tabToDeleteIndex != -1);

  QString question(tr("Are you sure you want to remove room %1")
                       .arg(tabText(m_tabToDeleteIndex)));
  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Delete Room"), question, QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes) return;

  emit deleteTabRoom(m_tabToDeleteIndex);
  removeTab(m_tabToDeleteIndex);
  m_tabToDeleteIndex = -1;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::setIsLocked(bool lock) {
  m_isLocked = lock;
}

//=============================================================================
// StackedMenuBar
//-----------------------------------------------------------------------------

StackedMenuBar::StackedMenuBar(QWidget *parent) : QStackedWidget(parent) {
  setObjectName("StackedMenuBar");
  connect(AppContext::instance(), &AppContext::languageChanged,
          this, &StackedMenuBar::refreshCurrentMenu);
}

//-----------------------------------------------------------------------------

void StackedMenuBar::refreshCurrentMenu() {
    if (!m_currentRoomName.isEmpty())
        createMenuBarForRoom(m_currentRoomName);
}

void StackedMenuBar::createMenuBarForRoom(const QString &roomName) {
  // Remove old widget for this room
  for (int i = 0; i < count(); i++) {
    if (widget(i) && widget(i)->objectName() == roomName) {
      QWidget *w = widget(i);
      removeWidget(w);
      delete w;
      break;
    }
  }

  m_currentRoomName = roomName;
  QMenuBar *bar = buildDefaultMenuBar();
  bar->setObjectName(roomName);
  addWidget(bar);
  setCurrentWidget(bar);
}

//-----------------------------------------------------------------------------

QMenuBar *StackedMenuBar::buildDefaultMenuBar() {
  QMenuBar *bar = new QMenuBar(this);

  //---- File Menu
  QMenu *fileMenu = bar->addMenu(tr("File"));
  fileMenu->addAction(
      CommandManager::instance()->getAction("MI_SaveLayout", true));
  fileMenu->addAction(
      CommandManager::instance()->getAction("MI_LoadLayout", true));
  fileMenu->addSeparator();
  fileMenu->addAction(
      CommandManager::instance()->getAction("MI_Quit", true));

  //---- View Menu
  QMenu *viewMenu = bar->addMenu(tr("View"));

  // Auto-fill: all panel toggle commands (MenuWindowsCommandType)
  std::vector<QAction*> winActions;
  CommandManager::instance()->getActions(MenuWindowsCommandType, winActions);
  for (QAction* act : winActions)
      viewMenu->addAction(act);

  viewMenu->addSeparator();

  // Maximize panel
  QAction* maxAct = CommandManager::instance()->getAction("MI_MaximizePanel", true);
  if (maxAct) viewMenu->addAction(maxAct);

  viewMenu->addSeparator();

  // Fullscreen modes
  QAction* fullScreenAct =
      CommandManager::instance()->getAction("MI_FullScreenWindow", true);
  QAction* seeThroughAct =
      CommandManager::instance()->getAction("MI_SeeThroughWindow", true);
  if (fullScreenAct) viewMenu->addAction(fullScreenAct);
  if (seeThroughAct) viewMenu->addAction(seeThroughAct);

  //---- Settings Menu
  QMenu *settingsMenu = bar->addMenu(tr("Settings"));
  QMenu *themeMenu    = settingsMenu->addMenu(tr("Theme"));
  AppContext* ctx = AppContext::instance();

  QActionGroup* themeGroup = new QActionGroup(themeMenu);
  themeGroup->setExclusive(true);
  QStringList themes = ctx->availableThemes();
  for (const QString& t : themes) {
      QAction* action = themeMenu->addAction(t);
      action->setCheckable(true);
      action->setChecked(t == ctx->currentTheme());
      themeGroup->addAction(action);
      QString theme = t;
      QObject::connect(action, &QAction::triggered, [ctx, theme]() {
          ctx->setCurrentTheme(theme);
      });
  }

  QMenu *langMenu = settingsMenu->addMenu(tr("Language"));
  QActionGroup* langGroup = new QActionGroup(langMenu);
  langGroup->setExclusive(true);
  QStringList languages = ctx->availableLanguages();
  for (const QString& l : languages) {
      QAction* action = langMenu->addAction(l);
      action->setCheckable(true);
      action->setChecked(l == ctx->currentLanguage());
      langGroup->addAction(action);
      QString lang = l;
      QObject::connect(action, &QAction::triggered, [ctx, lang]() {
          ctx->setCurrentLanguage(lang);
      });
  }

  //---- Help Menu
  QMenu *helpMenu = bar->addMenu(tr("Help"));
  helpMenu->addAction(
      CommandManager::instance()->getAction("MI_About", true));

  return bar;
}

//=============================================================================
// TopBar
//-----------------------------------------------------------------------------

TopBar::TopBar(QWidget *parent) : QToolBar(parent) {
  setAllowedAreas(Qt::TopToolBarArea);
  setMovable(false);
  setFloatable(false);
  setObjectName("TopBar");

  m_containerFrame = new QFrame(this);
  m_roomTabBar     = new RoomTabWidget(this);
  m_stackedMenuBar = new StackedMenuBar(this);
  m_lockRoomCB     = new QCheckBox(this);

  m_containerFrame->setObjectName("TopBarTabContainer");
  m_roomTabBar->setObjectName("TopBarTab");
  m_roomTabBar->setDrawBase(false);
  m_lockRoomCB->setObjectName("EditToolLockButton");
  m_lockRoomCB->setToolTip(tr("Lock Rooms Tab"));
  m_lockRoomCB->setChecked(m_roomTabBar->isLocked());

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setSpacing(0);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  {
    QVBoxLayout *menuLayout = new QVBoxLayout();
    menuLayout->setSpacing(0);
    menuLayout->setContentsMargins(0, 0, 0, 0);
    {
      menuLayout->addStretch(1);
      menuLayout->addWidget(m_stackedMenuBar, 0);
      menuLayout->addStretch(1);
    }
    mainLayout->addLayout(menuLayout);
    mainLayout->addStretch(1);
    mainLayout->addWidget(m_roomTabBar, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(m_lockRoomCB, 0);
  }
  m_containerFrame->setLayout(mainLayout);
  addWidget(m_containerFrame);

  bool ret = true;
  ret = ret && connect(m_roomTabBar, &QTabBar::currentChanged,
                       m_stackedMenuBar, &QStackedWidget::setCurrentIndex);
  ret = ret && connect(m_lockRoomCB, &QCheckBox::toggled, m_roomTabBar,
                       &RoomTabWidget::setIsLocked);
  assert(ret);
}
