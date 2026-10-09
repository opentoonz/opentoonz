// Panel and icons adapted from manongjohn, Tahoma2D PR #1275.
#include "alignmentpane.h"
#include "menubarcommandids.h"
#include "tapp.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"

#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include "toonzqt/selection.h"

AlignmentPane::AlignmentPane(QWidget* parent, Qt::WindowFlags flags)
    : QFrame(parent, flags) {
  setObjectName("AlignmentPanel");

  m_alignMethodCB = new QComboBox();
  m_alignMethodCB->addItem(tr("Selection Area"));
  m_alignMethodCB->setCurrentIndex(0);

  QAction* action;
  m_alignLeftBtn = new QPushButton(createQIcon("select_align_left"), 0, this);
  action         = CommandManager::instance()->getAction(MI_AlignLeft);
  m_alignLeftBtn->addAction(action);
  m_alignLeftBtn->setToolTip(tr("Align Left"));
  m_alignLeftBtn->setFixedSize(30, 30);
  connect(m_alignLeftBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignRightBtn = new QPushButton(createQIcon("select_align_right"), 0, this);
  action          = CommandManager::instance()->getAction(MI_AlignRight);
  m_alignRightBtn->addAction(action);
  m_alignRightBtn->setToolTip(tr("Align Right"));
  m_alignRightBtn->setFixedSize(30, 30);
  connect(m_alignRightBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignTopBtn = new QPushButton(createQIcon("select_align_top"), 0, this);
  action        = CommandManager::instance()->getAction(MI_AlignTop);
  m_alignTopBtn->addAction(action);
  m_alignTopBtn->setToolTip(tr("Align Top"));
  m_alignTopBtn->setFixedSize(30, 30);
  connect(m_alignTopBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignBottomBtn =
      new QPushButton(createQIcon("select_align_bottom"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignBottom);
  m_alignBottomBtn->addAction(action);
  m_alignBottomBtn->setToolTip(tr("Align Bottom"));
  m_alignBottomBtn->setFixedSize(30, 30);
  connect(m_alignBottomBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignCenterHBtn =
      new QPushButton(createQIcon("select_align_center_h"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignCenterHorizontal);
  m_alignCenterHBtn->addAction(action);
  m_alignCenterHBtn->setToolTip(tr("Align Center Horizontally"));
  m_alignCenterHBtn->setFixedSize(30, 30);
  connect(m_alignCenterHBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignCenterVBtn =
      new QPushButton(createQIcon("select_align_center_v"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignCenterVertical);
  m_alignCenterVBtn->addAction(action);
  m_alignCenterVBtn->setToolTip(tr("Align Center Vertically"));
  m_alignCenterVBtn->setFixedSize(30, 30);
  connect(m_alignCenterVBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_distributeHBtn =
      new QPushButton(createQIcon("select_distribute_h"), 0, this);
  action = CommandManager::instance()->getAction(MI_DistributeHorizontal);
  m_distributeHBtn->addAction(action);
  m_distributeHBtn->setToolTip(tr("Distribute Horizontally"));
  m_distributeHBtn->setFixedSize(30, 30);
  connect(m_distributeHBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_distributeVBtn =
      new QPushButton(createQIcon("select_distribute_v"), 0, this);
  action = CommandManager::instance()->getAction(MI_DistributeVertical);
  m_distributeVBtn->addAction(action);
  m_distributeVBtn->setToolTip(tr("Distribute Vertically"));
  m_distributeVBtn->setFixedSize(30, 30);
  connect(m_distributeVBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  QGridLayout* mainlayout = new QGridLayout();
  mainlayout->setMargin(5);
  mainlayout->setSpacing(2);
  {
    mainlayout->addWidget(new QLabel(tr("Relative to: ")), 0, 0,
                          Qt::AlignRight);
    mainlayout->addWidget(m_alignMethodCB, 0, 1);

    QGroupBox* alignBox      = new QGroupBox(tr("Align"), this);
    QGridLayout* alignLayout = new QGridLayout();
    alignLayout->setMargin(1);
    alignLayout->setSpacing(1);
    {
      alignLayout->addWidget(m_alignLeftBtn, 0, 0, Qt::AlignCenter);
      alignLayout->addWidget(m_alignCenterVBtn, 0, 1, Qt::AlignCenter);
      alignLayout->addWidget(m_alignRightBtn, 0, 2, Qt::AlignCenter);

      alignLayout->addWidget(m_alignTopBtn, 1, 0, Qt::AlignCenter);
      alignLayout->addWidget(m_alignCenterHBtn, 1, 1, Qt::AlignCenter);
      alignLayout->addWidget(m_alignBottomBtn, 1, 2, Qt::AlignCenter);
    }
    alignBox->setLayout(alignLayout);
    mainlayout->addWidget(alignBox, 1, 0, 1, 2);

    QGroupBox* distributeBox      = new QGroupBox(tr("Distribute"), this);
    QGridLayout* distributeLayout = new QGridLayout();
    distributeLayout->setMargin(1);
    distributeLayout->setSpacing(1);
    {
      distributeLayout->addWidget(m_distributeHBtn, 0, 0, Qt::AlignCenter);
      distributeLayout->addWidget(m_distributeVBtn, 0, 1, Qt::AlignCenter);
    }
    distributeBox->setLayout(distributeLayout);
    mainlayout->addWidget(distributeBox, 2, 0, 1, 2);
  }

  setLayout(mainlayout);

  bool ret = true;

  ret = ret && connect(m_alignMethodCB, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onAlignMethodChanged(int)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void AlignmentPane::updateButtons() {
  TTool* tool  = TApp::instance()->getCurrentTool()->getTool();
  bool strokes = tool && tool->getName() == T_Selection;
  bool points  = tool && tool->getName() == T_ControlPointEditor;
  int method   = tool ? tool->getAlignMethod() : VectorAlignment::SELECT_AREA;
  QStringList inputs;
  inputs << tr("Selection Area") << tr("First Selected") << tr("Last Selected");
  if (strokes)
    inputs << tr("Smallest Object") << tr("Largest Object")
           << tr("Camera Area");
  if (method >= inputs.size()) method = VectorAlignment::SELECT_AREA;
  {
    QSignalBlocker blocker(m_alignMethodCB);
    m_alignMethodCB->clear();
    m_alignMethodCB->addItems(inputs);
    m_alignMethodCB->setCurrentIndex(method);
  }
  TSelection* selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  bool enabled = (strokes || points) && tool->isEnabled() && selection &&
                 !selection->isEmpty();
  m_alignMethodCB->setEnabled(strokes || points);
  QPushButton* buttons[] = {
      m_alignLeftBtn,    m_alignRightBtn,   m_alignTopBtn,    m_alignBottomBtn,
      m_alignCenterHBtn, m_alignCenterVBtn, m_distributeHBtn, m_distributeVBtn};
  for (int i = 0; i < 8; ++i) {
    QAction* action = buttons[i]->actions().front();
    bool available  = enabled && action->isEnabled();
    if (i >= 6)
      available =
          available && (method == VectorAlignment::SELECT_AREA ||
                        (strokes && method == VectorAlignment::CAMERA_AREA));
    buttons[i]->setEnabled(available);
  }
}

void AlignmentPane::showEvent(QShowEvent*) {
  TApp* app = TApp::instance();

  bool ret = true;

  ret = ret &&
        connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel*)),
                this, SLOT(onLevelSwitched(TXshLevel*)));
  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()), this,
                       SLOT(onToolSwitched()));
  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolChanged()), this,
                       SLOT(onToolSwitched()));
  ret =
      ret && connect(app->getCurrentSelection(),
                     SIGNAL(selectionSwitched(TSelection*, TSelection*)), this,
                     SLOT(onSelectionSwitched(TSelection*, TSelection*)));

  ret = ret && connect(app->getCurrentSelection(),
                       SIGNAL(selectionChanged(TSelection*)), this,
                       SLOT(onSelectionChanged()));
  assert(ret);
  updateButtons();
}

//-----------------------------------------------------------------------------

void AlignmentPane::hideEvent(QHideEvent*) {
  TApp* app = TApp::instance();

  disconnect(app->getCurrentLevel(), 0, this, 0);
  disconnect(app->getCurrentTool(), 0, this, 0);
  disconnect(app->getCurrentSelection(), 0, this, 0);
}

//-----------------------------------------------------------------------------

void AlignmentPane::onLevelSwitched(TXshLevel* oldLvl) { updateButtons(); }

//-----------------------------------------------------------------------------

void AlignmentPane::onToolSwitched() { updateButtons(); }

//-----------------------------------------------------------------------------

void AlignmentPane::onAlignMethodChanged(int index) {
  TTool* tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool) return;
  tool->setAlignMethod(static_cast<VectorAlignment::Method>(index));
  TApp::instance()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void AlignmentPane::onSelectionSwitched(TSelection* oldSelection,
                                        TSelection* newSelection) {
  updateButtons();
}

void AlignmentPane::onSelectionChanged() { updateButtons(); }
