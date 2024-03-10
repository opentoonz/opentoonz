#include "xshbreadcrumbs.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/childstack.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvscrollwidget.h"

// Qt includes
#include <QWidgetAction>
#include <QLabel>
#include <QComboBox>

//=============================================================================

BreadcrumbClickableLabel::BreadcrumbClickableLabel(QString labelName,
                                                   QWidget *parent,
                                                   Qt::WindowFlags f)
    : QLabel(labelName, parent) {
  setStyleSheet("text-decoration: underline;");
}

BreadcrumbClickableLabel::~BreadcrumbClickableLabel() {}

void BreadcrumbClickableLabel::mousePressEvent(QMouseEvent *event) {
  emit clicked();
}

namespace XsheetGUI {
//=============================================================================

Breadcrumb::Breadcrumb(CrumbType crumbType, QString crumbName,
                       CrumbWidgetType crumbWidgetType, QWidget *parent)
    : QWidget(parent)
    , m_crumbType(crumbType)
    , m_col(-1)
    , m_colList(0)
    , m_distanceFromCurrent(0) {
  if (crumbWidgetType == CrumbWidgetType::LABEL)
    m_crumbWidget = new QLabel(crumbName, parent);
  else if (crumbWidgetType == CrumbWidgetType::BUTTON) {
    BreadcrumbClickableLabel *crumbButton =
        new BreadcrumbClickableLabel(crumbName, parent);
    m_crumbWidget = crumbButton;
    connect(crumbButton, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
  } else if (crumbWidgetType == CrumbWidgetType::COMBOBOX) {
    QComboBox *crumbCB = new QComboBox(parent);
    m_crumbWidget      = crumbCB;
    connect(crumbCB, SIGNAL(currentIndexChanged(int)), this,
            SLOT(onComboBoxIndexChanged(int)));
  }
}

void Breadcrumb::onButtonClicked() {
  TApp *app              = TApp::instance();
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  ChildStack *childStack = scene->getChildStack();

  if (m_crumbType == CrumbType::CHILD) {
    TXsheet *xsh = childStack->getXsheet();

    int r = app->getCurrentFrame()->getFrameIndex();
    int r0, r1;
    xsh->getCellRange(m_col, r0, r1);
    if (r1 < r0) r1 = r0;
    if (r < r0)
      r = r0;
    else if (r > r1)
      r = r1;
    else {
      TXshCell cell               = xsh->getCell(r, m_col);
      while (cell.isEmpty()) cell = xsh->getCell(++r, m_col);
    }

    app->getCurrentColumn()->setColumnIndex(m_col);
    app->getCurrentFrame()->setFrameIndex(r);
    if (app->getCurrentSelection()->getSelection())
      app->getCurrentSelection()->getSelection()->selectNone();

    QAction *openChildAction =
        CommandManager::instance()->getAction(MI_OpenChild);
    if (!openChildAction) return;
    openChildAction->trigger();
  } else if (m_crumbType == CrumbType::ANCESTOR) {
    QAction *closeChildAction =
        CommandManager::instance()->getAction(MI_CloseChild);
    if (!closeChildAction) return;
    TUndoManager::manager()->beginBlock();
    for (int i = 0; i > m_distanceFromCurrent; i--) closeChildAction->trigger();
    TUndoManager::manager()->endBlock();
  }
}

void Breadcrumb::onComboBoxIndexChanged(int index) {
  if (m_crumbType != CrumbType::CHILD || !index) return;

  TApp *app              = TApp::instance();
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  ChildStack *childStack = scene->getChildStack();
  TXsheet *xsh           = childStack->getXsheet();

  int col = m_colList[index - 1];
  int r   = app->getCurrentFrame()->getFrameIndex();
  int r0, r1;
  xsh->getCellRange(col, r0, r1);
  if (r1 < r0) r1 = r0;
  if (r < r0)
    r = r0;
  else if (r > r1)
    r = r1;
  else {
    TXshCell cell               = xsh->getCell(r, col);
    while (cell.isEmpty()) cell = xsh->getCell(++r, col);
  }

  app->getCurrentColumn()->setColumnIndex(col);
  app->getCurrentFrame()->setFrameIndex(r);
  if (app->getCurrentSelection()->getSelection())
    app->getCurrentSelection()->getSelection()->selectNone();

  QAction *openChildAction =
      CommandManager::instance()->getAction(MI_OpenChild);
  if (!openChildAction) return;
  openChildAction->trigger();
}

//=============================================================================
// BreadcrumbArea
//-----------------------------------------------------------------------------

BreadcrumbArea::BreadcrumbArea(XsheetViewer *parent, Qt::WindowFlags flags)
    : m_viewer(parent) {
  setObjectName("cornerWidget");
  setFixedHeight(29);
  setObjectName("XsheetBreadcrumbs");

  m_breadcrumbWidgets.clear();
  updateBreadcrumbs();
}

//-----------------------------------------------------------------------------

void BreadcrumbArea::showBreadcrumbs(bool show) {
  show ? this->show() : this->hide();
}

//-----------------------------------------------------------------------------

void BreadcrumbArea::toggleBreadcrumbArea() {
  bool breadcrumbsEnabled =
      Preferences::instance()->isShowXsheetBreadcrumbsEnabled();
  Preferences::instance()->setValue(showXsheetBreadcrumbs, !breadcrumbsEnabled);
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "XsheetBreadcrumbs");
}

//-----------------------------------------------------------------------------

void BreadcrumbArea::showEvent(QShowEvent *e) {
  TApp *app = TApp::instance();
  connect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), this,
          SLOT(updateBreadcrumbs()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
          SLOT(updateBreadcrumbs()));

  updateBreadcrumbs();
}

//-----------------------------------------------------------------------------

void BreadcrumbArea::hideEvent(QHideEvent *e) {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), this,
             SLOT(updateBreadcrumbs()));
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
             SLOT(updateBreadcrumbs()));
}

//-----------------------------------------------------------------------------

void BreadcrumbArea::updateBreadcrumbs() {
  if (isHidden()) return;

  // Remove the current layout
  QLayout *currentLayout = layout();

  if (currentLayout) {
    QLayoutItem *item;
    while ((item = currentLayout->takeAt(0)) != nullptr) {
      currentLayout->removeWidget(item->widget());
      item->widget()->deleteLater();
    }
    delete currentLayout;
  }

  m_breadcrumbWidgets.clear();

  // Rebuild the breadcrumb widget list
  TApp *app = TApp::instance();

  ToonzScene *scene      = app->getCurrentScene()->getScene();
  ChildStack *childStack = scene->getChildStack();
  TXsheet *xsh           = childStack->getXsheet();
  int ancestorCount      = childStack->getAncestorCount();

  // Look for any sub-xsheets in current xsheet
  std::vector<int> childCol;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    if (xsh->isColumnEmpty(col)) continue;

    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    TXshCell cell = xsh->getCell(r0, col);
    TXshLevel *xl = cell.m_level.getPointer();
    if (!xl) continue;
    TXshChildLevel *cl = xl->getChildLevel();
    if (!cl) continue;
    childCol.push_back(col);
  }

  QString separator  = tr("  >  ");
  QString separator2 = tr("  |  ");

  Breadcrumb *crumb;

  QString childName;
  if (childCol.size() == 1) {
    TStageObjectId columnId    = TStageObjectId::ColumnId(childCol[0]);
    TStageObject *columnObject = xsh->getStageObject(columnId);
    childName = QString::fromStdString(columnObject->getName());

    crumb = new Breadcrumb(CrumbType::CHILD, childName, CrumbWidgetType::BUTTON,
                           this);
    crumb->setColumnNumber(childCol[0]);
    m_breadcrumbWidgets.push_back(crumb);
  } else if (childCol.size() > 1) {
    crumb =
        new Breadcrumb(CrumbType::CHILD, 0, CrumbWidgetType::COMBOBOX, this);
    crumb->setColumnNumberList(childCol);
    QComboBox *childCB = dynamic_cast<QComboBox *>(crumb->getCrumbWidget());
    childCB->blockSignals(true);
    childCB->addItem(tr("---"));
    for (int i = 0; i < childCol.size(); i++) {
      TStageObjectId columnId    = TStageObjectId::ColumnId(childCol[i]);
      TStageObject *columnObject = xsh->getStageObject(columnId);
      childName = QString::fromStdString(columnObject->getName());
      childCB->addItem(childName);
    }
    childCB->blockSignals(false);
    m_breadcrumbWidgets.push_back(crumb);
  }

  if (m_breadcrumbWidgets.size())
    m_breadcrumbWidgets.push_back(new Breadcrumb(
        CrumbType::SEPARATOR, separator2, CrumbWidgetType::LABEL, this));

  QString ancestorName;
  for (int i = ancestorCount; i > 0; i--) {
    AncestorNode *ancestor = childStack->getAncestorInfo(i - 1);
    if (!ancestor || !ancestor->m_cl || !ancestor->m_xsheet) break;

    TStageObjectId columnId    = TStageObjectId::ColumnId(ancestor->m_col);
    TStageObject *columnObject = ancestor->m_xsheet->getStageObject(columnId);
    ancestorName = QString::fromStdString(columnObject->getName());

    // Add label for current xsheet, button for everything else
    if (i == ancestorCount)
      crumb = new Breadcrumb(CrumbType::ANCESTOR, ancestorName,
                             CrumbWidgetType::LABEL, this);
    else {
      crumb = new Breadcrumb(CrumbType::ANCESTOR, ancestorName,
                             CrumbWidgetType::BUTTON, this);
      crumb->setDistanceFromCurrent(i - ancestorCount);
    }
    m_breadcrumbWidgets.push_back(crumb);

    m_breadcrumbWidgets.push_back(new Breadcrumb(
        CrumbType::SEPARATOR, separator, CrumbWidgetType::LABEL, this));
  }

  ancestorName = tr("Main");
  if (!ancestorCount)
    crumb = new Breadcrumb(CrumbType::ANCESTOR, ancestorName,
                           CrumbWidgetType::LABEL, this);
  else {
    crumb = new Breadcrumb(CrumbType::ANCESTOR, ancestorName,
                           CrumbWidgetType::BUTTON, this);
    crumb->setDistanceFromCurrent(-ancestorCount);
  }
  m_breadcrumbWidgets.push_back(crumb);

  // Now let's put everything in a layout
  m_breadcrumbLayout = new QHBoxLayout();
  m_breadcrumbLayout->setMargin(0);
  m_breadcrumbLayout->setSpacing(0);
  {
    if (!m_viewer->orientation()->isVerticalTimeline())
      m_breadcrumbLayout->addSpacing(215);
    m_breadcrumbLayout->addWidget(new QLabel(tr("Xsheet Depth:"), this), 0,
                                  Qt::AlignCenter);
    m_breadcrumbLayout->addSpacing(5);

    std::vector<Breadcrumb *>::reverse_iterator rit;
    for (rit = m_breadcrumbWidgets.rbegin(); rit != m_breadcrumbWidgets.rend();
         ++rit) {
      m_breadcrumbLayout->addWidget((*rit)->getCrumbWidget(), 0,
                                    Qt::AlignCenter);
    }
  }
  m_breadcrumbLayout->addStretch(1);

  QHBoxLayout *hLayout = new QHBoxLayout;
  hLayout->setMargin(0);
  hLayout->setSpacing(0);
  setLayout(hLayout);

  DvScrollWidget *scrollWidget = new DvScrollWidget;
  hLayout->addWidget(scrollWidget);

  QWidget *crumbContainer = new QWidget;
  scrollWidget->setWidget(crumbContainer);

  crumbContainer->setSizePolicy(QSizePolicy::MinimumExpanding,
                                QSizePolicy::Fixed);
  crumbContainer->setFixedHeight(24);
  crumbContainer->setLayout(m_breadcrumbLayout);
}

//============================================================

class ToggleXsheetBreadcrumbsCommand final : public MenuItemHandler {
public:
  ToggleXsheetBreadcrumbsCommand()
      : MenuItemHandler(MI_ToggleXsheetBreadcrumbs) {}
  void execute() override { BreadcrumbArea::toggleBreadcrumbArea(); }
} ToggleXsheetBreadcrumbsCommand;

}  // namespace XsheetGUI
