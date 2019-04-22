

#include "tasksviewer.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "batches.h"
#include "floatingpanelcommand.h"
#include "tsystem.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvscrollwidget.h"

// TnzCore includes
#include "tconvert.h"

// Qt includes
#include <QTreeWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QMenu>
#include <QTextEdit>
#include <QListWidget>
#include <QToolBar>
#include <QTimer>
#include <QComboBox>

using namespace DVGui;

//=============================================================================

namespace {
bool isMovieType(std::string type) {
  return (type == "mov" || type == "avi" || type == "3gp" || type == "mp4" ||
          type == "webm");
}
};

//=============================================================================

const std::vector<QAction *> &TasksViewer::getActions() const {
  return m_actions;
}

//----------------------------------------------------------------------

void TasksViewer::add(const QString &iconName, QString text, QToolBar *toolBar,
                      const char *slot, QString iconText) {
#if QT_VERSION >= 0x050500
  QAction *action = new QAction(
      createQIconOnOff(iconName.toLatin1().constData(), false), text, this);
#else
  QAction *action = new QAction(
      createQIconOnOff(iconName.toAscii().constData(), false), text, this);
#endif
  action->setIconText(iconText);
  bool ret = connect(action, SIGNAL(triggered(bool)),
                     (TaskTreeModel *)m_treeView->model(), slot);
  assert(ret);
  toolBar->addAction(action);
  m_actions.push_back(action);
}

//----------------------------------------------------------------------

void TasksViewer::showEvent(QShowEvent *) { startTimer(); }

//----------------------------------------------------------------------

void TasksViewer::hideEvent(QHideEvent *) { stopTimer(); }

//----------------------------------------------------------------------

QWidget *TasksViewer::createToolBar() {
  // Create toolbar. It is an horizontal layout with three internal toolbar.
  QWidget *toolBarWidget = new QWidget(this);
  QToolBar *cmdToolbar   = new QToolBar(toolBarWidget);
  cmdToolbar->setIconSize(QSize(21, 17));
  cmdToolbar->clear();
  cmdToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  add("start", tr("&Start"), cmdToolbar, SLOT(start(bool)), tr("Start"));
  add("stop", tr("&Stop"), cmdToolbar, SLOT(stop(bool)), tr("Stop"));
  cmdToolbar->addSeparator();
  add("addrender", tr("&Add Render Task"), cmdToolbar,
      SLOT(addRenderTask(bool)), tr("Add Render"));
  add("addcleanup", tr("&Add Cleanup Task"), cmdToolbar,
      SLOT(addCleanupTask(bool)), tr("Add Cleanup"));

  QToolBar *saveToolbar = new QToolBar(toolBarWidget);
  saveToolbar->setIconSize(QSize(21, 17));
  saveToolbar->clear();
  saveToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  add("save", tr("&Save Task List"), saveToolbar, SLOT(save(bool)), tr("Save"));
  add("saveas", tr("&Save Task List As"), saveToolbar, SLOT(saveas(bool)),
      tr("Save As"));
  add("load", tr("&Load Task List"), saveToolbar, SLOT(load(bool)), tr("Load"));
  saveToolbar->addSeparator();
  add("delete", tr("&Remove"), saveToolbar, SLOT(remove(bool)), tr("Remove"));

  QVBoxLayout *toolbarLayout = new QVBoxLayout(toolBarWidget);
  toolbarLayout->setMargin(0);
  toolbarLayout->setSpacing(0);
  {
    toolbarLayout->addWidget(cmdToolbar);
    toolbarLayout->addWidget(saveToolbar);
  }
  toolBarWidget->setLayout(toolbarLayout);

  return toolBarWidget;
}
//=============================================================================
//=============================================================================
/*! \class TasksViewer
                Inherits \b QSplitter.
*/
#if QT_VERSION >= 0x050500
TasksViewer::TasksViewer(QWidget *parent, Qt::WindowFlags flags)
#else
TasksViewer::TasksViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QSplitter(parent) {
  QFrame *box;

  BatchesController::instance()->attach(this);

  // style sheet
  setObjectName("Tasks");
  setFrameStyle(QFrame::StyledPanel);

  //-------begin left side (the tree + the toolbar)

  // If there is another TasksViewer instance already, share the same TasksTree
  TaskTreeModel *treeModel = BatchesController::instance()->getTasksTree();
  m_treeView               = new TaskTreeView(this, treeModel);
  BatchesController::instance()->setTasksTree(
      (TaskTreeModel *)m_treeView->model());

  m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

  box                  = new QFrame(this);
  QVBoxLayout *vLayout = new QVBoxLayout(box);
  box->setLayout(vLayout);
  vLayout->setMargin(0);
  vLayout->setSpacing(0);

  vLayout->addWidget(createToolBar());
  vLayout->addWidget(m_treeView);

  addWidget(box);

  //------end left side (the tree + the toolbar)
  //------begin right side (the task sheet)

  m_taskSheet = new TaskSheet(this);
  addWidget(m_taskSheet);

  //------end right side (the task sheet)

  setStretchFactor(1, 2);

  m_timer  = new QTimer(this);
  bool ret = connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
  assert(ret);
  m_timer->start(3000);
}

//-----------------------------------------------------------------------------

void TasksViewer::update() {
  //	((TaskTreeModel*)m_treeView->model())->update();
  m_taskSheet->update(m_taskSheet->getCurrentTask());
}

TasksViewer::~TasksViewer() {}

//-----------------------------------------------------------------------------

void TasksViewer::onTimer() {
  if (BatchesController::instance()
          ->getController())  // si sta utilizzando la farm
    BatchesController::instance()->update();
}

//------------------------------------------------------------------------------------------------------------------

void TasksViewer::startTimer() {
  onTimer();
  if (!m_timer->isActive()) m_timer->start(3000);
}

//------------------------------------------------------------------------------------------------------------------

void TasksViewer::stopTimer() {
  if (m_timer->isActive()) m_timer->stop();
}

//-----------------------------------------------------------------------------
namespace {
QString getStatusString(TaskState status) {
  switch (status) {
  case Suspended:
    return TaskSheet::tr("Suspended");
  case Waiting:
    return TaskSheet::tr("Waiting");
  case Running:
    return TaskSheet::tr("Running");
  case Completed:
    return TaskSheet::tr("Completed");
  case Aborted:
    return TaskSheet::tr("Failed");
  case TaskUnknown:
    return TaskSheet::tr("TaskUnknown");
  }
  return "";
}

bool containsTask(const TFarmTask::Dependencies &dependencies,
                  TFarmTask::Id id) {
  int i;
  for (i = 0; i < dependencies.getTaskCount(); i++)
    if (dependencies.getTaskId(i) == id) return true;
  return false;
}

}  // namespace

//-----------------------------------------------------------------------------

class DependencyItem final : public QListWidgetItem {
public:
  TFarmTask *m_task;

  DependencyItem(TFarmTask *task, QListWidget *w)
      : QListWidgetItem(task->m_name, w), m_task(task) {}
};

//-----------------------------------------------------------------------------

void TaskSheet::onAdded(bool) {
  QList<QListWidgetItem *> l;
  if (!m_task) return;

  l = m_notAddedBox->selectedItems();
  if (l.isEmpty()) return;

  int i;
  for (i = 0; i < l.size(); i++) {
    TFarmTask *t = ((DependencyItem *)l.at(i))->m_task;
    assert(t);
    m_task->m_dependencies->add(t->m_id);
  }
  BatchesController::instance()->setDirtyFlag(true);
  update(m_task);
}

//-----------------------------------------------------------------------------

void TaskSheet::onRemoved(bool) {
  QList<QListWidgetItem *> l;
  if (!m_task) return;

  l = m_addedBox->selectedItems();
  if (l.isEmpty()) return;
  int i;
  for (i = 0; i < l.size(); i++) {
    TFarmTask *t = ((DependencyItem *)l.at(i))->m_task;
    assert(t);
    m_task->m_dependencies->remove(t->m_id);
  }
  BatchesController::instance()->setDirtyFlag(true);
  update(m_task);
}

//-----------------------------------------------------------------------------

void TaskSheet::onAddedItemDoubleClicked(QListWidgetItem *item) {
  if (!m_task || !item) return;

  TFarmTask *t = ((DependencyItem *)item)->m_task;
  assert(t);
  m_task->m_dependencies->add(t->m_id);

  BatchesController::instance()->setDirtyFlag(true);
  update(m_task);
}

//-----------------------------------------------------------------------------

void TaskSheet::onRemovedItemDoubleClicked(QListWidgetItem *item) {
  if (!m_task || !item) return;

  TFarmTask *t = ((DependencyItem *)item)->m_task;
  assert(t);
  m_task->m_dependencies->remove(t->m_id);

  BatchesController::instance()->setDirtyFlag(true);
  update(m_task);
}

//-----------------------------------------------------------------------------

void TaskSheet::update(TFarmTask *task) {
  m_task = task;

  while (m_addedBox->count()) delete m_addedBox->takeItem(0);
  while (m_notAddedBox->count()) delete m_notAddedBox->takeItem(0);

  BatchesController *bc = BatchesController::instance();

  if (!task) {
    widget()->hide();
    return;
  }

  widget()->show();

  m_name->setText(task->m_name);
  m_status->setText(getStatusString(task->m_status));
  m_commandLine->setText(task->getCommandLine());
  m_server->setText(task->m_server);
  m_submittedBy->setText(task->m_user);
  m_submittedOn->setText(task->m_submissionDate.toString());
  m_priority->setText(QString::number(task->m_priority));
  m_submitDate->setText(task->m_submissionDate.toString());
  m_startDate->setText(task->m_startDate.toString());
  m_complDate->setText(task->m_completionDate.toString());
  if (task->m_completionDate > task->m_startDate)
    m_duration->setText(
        QString::number(task->m_startDate.secsTo(task->m_completionDate)));
  else
    m_duration->clear();
  m_stepCount->setText(QString::number(task->m_stepCount));
  if (task->m_failedSteps >= 0)
    m_failedSteps->setText(QString::number(task->m_failedSteps));
  else
    m_failedSteps->clear();
  if (task->m_successfullSteps >= 0)
    m_succSteps->setText(QString::number(task->m_successfullSteps));
  else
    m_succSteps->clear();

  if (task->m_isComposerTask) {
    m_boxCleanup->hide();

    if (dynamic_cast<TFarmTaskGroup *>(task)) {
      m_boxComposer->show();

      m_outputPath->setText(
          QString::fromStdWString(task->m_outputPath.getWideString()));
      m_from->setText(QString::number(task->m_from));
      m_to->setText(QString::number(task->m_to));
      m_step->setText(QString::number(task->m_step));
      m_shrink->setText(QString::number(task->m_shrink));
      m_multimedia->setCurrentIndex(task->m_multimedia);
      m_threadsCombo->setCurrentIndex(task->m_threadsIndex);
      m_rasterGranularityCombo->setCurrentIndex(task->m_maxTileSizeIndex);

      if (!isMovieType(task->m_outputPath.getType())) {
        m_chunkSize->setEnabled(true);
        m_chunkSize->setText(QString::number(task->m_chunkSize));
      } else {
        m_chunkSize->setEnabled(false);
        m_chunkSize->setText(QString::number(task->m_stepCount));
      }
    } else
      m_boxComposer->hide();
  } else {
    m_boxComposer->hide();
    m_boxCleanup->show();
    m_overwrite->setCurrentIndex((int)task->m_overwrite);
    m_visible->setChecked(task->m_onlyVisible);
  }

  int i;

  for (i = 0; i < task->m_dependencies->getTaskCount(); i++)
    new DependencyItem(bc->getTask(task->m_dependencies->getTaskId(i)),
                       m_addedBox);

  for (i = 0; i < bc->getTaskCount(); i++) {
    TFarmTaskGroup *tg;
    TFarmTask *t = bc->getTask(i);
    if (!(tg = dynamic_cast<TFarmTaskGroup *>(t))) continue;
    if (tg->m_id == task->m_id) continue;
    if (containsTask(*task->m_dependencies, tg->m_id)) continue;
    new DependencyItem(tg, m_notAddedBox);
  }
}

//-----------------------------------------------------------------------------

void TaskSheet::updateChunks(TFarmTask *task) {
  TFarmTaskGroup *t = dynamic_cast<TFarmTaskGroup *>(task);

  if (t) {
    while (t->getTaskCount())
      BatchesController::instance()->removeTask(t->getTask(0)->m_id);

    t->changeChunkSize(t->m_chunkSize);
    int i;
    for (i = 0; i < t->getTaskCount(); i++)
      BatchesController::instance()->addTask(t->getTask(i)->m_id,
                                             t->getTask(i));

    ((TaskTreeModel *)m_viewer->m_treeView->model())->setupModelData();
    ((TaskTreeModel *)m_viewer->m_treeView->model())->update();
  }
}

//-----------------------------------------------------------------------------
namespace {

void inline create(QTextEdit *&ret, QGridLayout *layout, QString name, int row,
                   int span = 5) {
  ret = new QTextEdit();
  ret->setReadOnly(true);
  ret->setObjectName("TaskSheetItem");
  QLabel *label = new QLabel(name);
  label->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label, row, 0, 1, 2, Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(ret, row, 2, 1, span);
}

//-----------------------------------------------------------------------------

void inline create(LineEdit *&ret, QGridLayout *layout, QString name, int row,
                   int span = 3) {
  ret           = new LineEdit();
  QLabel *label = new QLabel(name);
  label->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label, row, 0, 1, 2, Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(ret, row, 2, 1, span);
}

//-----------------------------------------------------------------------------

void inline create(QComboBox *&ret, QGridLayout *layout, QString name, int row,
                   int span = 3) {
  ret           = new QComboBox();
  QLabel *label = new QLabel(name);
  label->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label, row, 0);
  layout->addWidget(ret, row, 2, 1, span);
}

//-----------------------------------------------------------------------------

void inline create(QLabel *&ret, QGridLayout *layout, QString name, int row,
                   int span = 3) {
  ret = new QLabel();
  ret->setObjectName("TaskSheetItem");
  QLabel *label = new QLabel(name);
  label->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label, row, 0, 1, 2, Qt::AlignRight | Qt::AlignVCenter);
  layout->addWidget(ret, row, 2, 1, span);
}

//-----------------------------------------------------------------------------

void inline create(CheckBox *&ret, QGridLayout *layout, QString name, int row) {
  ret = new CheckBox(name);
  layout->addWidget(ret, row, 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
}

//-----------------------------------------------------------------------------

template <typename A, typename B>
void inline create(A *&ret1, B *&ret2, QGridLayout *layout, QString name1,
                   QString name2, QFlags<Qt::AlignmentFlag> align1,
                   QFlags<Qt::AlignmentFlag> align2, int row) {
  ret1           = new A();
  ret2           = new B();
  QLabel *label1 = new QLabel(name1);
  QLabel *label2 = new QLabel(name2);
  ret1->setObjectName("TaskSheetItem");
  ret2->setObjectName("TaskSheetItem");
  label1->setObjectName("TaskSheetItemLabel");
  label2->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label1, row, 0, 1, 2, align1);
  layout->addWidget(ret1, row, 2, 1, 1, Qt::AlignLeft);
  layout->addWidget(label2, row, 3, align2);
  layout->addWidget(ret2, row, 4, 1, 1, Qt::AlignLeft);
}

}  // namespace

//-----------------------------------------------------------------------------

void TaskSheet::setChunkSize() {
  if (!m_task) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  int val = m_chunkSize->text().toInt();
  if (val < 1) {
    val = 1;
    m_chunkSize->setText(QString::number(val));
  } else if (val > m_task->m_stepCount) {
    val = m_task->m_stepCount;
    m_chunkSize->setText(QString::number(val));
  }

  if (m_task->m_chunkSize != val) {
    m_task->m_chunkSize = val;
    BatchesController::instance()->setDirtyFlag(true);

    updateChunks(m_task);
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::onFocusIn() { m_viewer->stopTimer(); }

//-----------------------------------------------------------------------------

void TaskSheet::setName() {
  if (!m_task) return;

  m_task->m_name = m_name->text();
  m_commandLine->setText(m_task->getCommandLine());

  BatchesController::instance()->setDirtyFlag(true);

  ((TaskTreeModel *)m_viewer->m_treeView->model())->update();
  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setFrom() {
  if (!m_task) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  int val = m_from->text().toInt();

  if (val < 1) {
    val = 1;
    m_from->setText(QString::number(val));
  } else if (val > m_task->m_to) {
    val = m_task->m_to;
    m_from->setText(QString::number(val));
  }

  if (m_task->m_from != val) {
    m_task->m_from = val;
    m_commandLine->setText(m_task->getCommandLine());
    BatchesController::instance()->setDirtyFlag(true);

    updateChunks(m_task);
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setTo() {
  if (!m_task) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  int val = m_to->text().toInt();

  if (val < m_task->m_from) {
    val = m_task->m_from;
    m_to->setText(QString::number(val));
  } else if (val > m_task->m_stepCount) {
    val = m_task->m_stepCount;
    m_to->setText(QString::number(val));
  }

  if (m_task->m_to != val) {
    m_task->m_to = val;
    m_commandLine->setText(m_task->getCommandLine());
    BatchesController::instance()->setDirtyFlag(true);

    updateChunks(m_task);
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setShrink() {
  if (!m_task) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  int val = m_shrink->text().toInt();
  if (val < 1)
    m_shrink->setText(QString::number(m_task->m_shrink));
  else if (m_task->m_shrink != val) {
    m_task->m_shrink = val;
    m_commandLine->setText(m_task->getCommandLine());
    BatchesController::instance()->setDirtyFlag(true);

    // Update children tasks, if present.
    TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);
    if (taskGroup) {
      for (int i                        = 0; i < taskGroup->getTaskCount(); ++i)
        taskGroup->getTask(i)->m_shrink = taskGroup->m_shrink;
    }
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setStep() {
  if (!m_task) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  int val = m_step->text().toInt();
  if (val < 1)
    m_step->setText(QString::number(m_task->m_step));
  else if (m_task->m_step != val) {
    m_task->m_step = val;
    m_commandLine->setText(m_task->getCommandLine());
    BatchesController::instance()->setDirtyFlag(true);

    // Update children tasks, if present.
    TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);
    if (taskGroup) {
      for (int i                      = 0; i < taskGroup->getTaskCount(); ++i)
        taskGroup->getTask(i)->m_step = taskGroup->m_step;
    }
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setPriority() {
  if (!m_task) return;

  int val = m_priority->text().toInt();
  if (val < 0)
    m_from->setText(QString::number(m_task->m_priority));
  else {
    m_task->m_priority = val;
    BatchesController::instance()->setDirtyFlag(true);
  }
  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setVisible(int) {
  if (!m_task) return;
  if (m_task->m_onlyVisible == (m_visible->checkState() == Qt::Checked)) return;

  m_task->m_onlyVisible = (m_visible->checkState() == Qt::Checked);
  m_commandLine->setText(m_task->getCommandLine());
  BatchesController::instance()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void TaskSheet::setOverwrite(int index) {
  if (!m_task) return;
  if (m_task->m_overwrite == (OverwriteBehavior)index) return;

  m_task->m_overwrite = (OverwriteBehavior)index;
  m_commandLine->setText(m_task->getCommandLine());
  BatchesController::instance()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void TaskSheet::setMultimedia(int) {
  if (!m_task) return;
  if (m_task->m_multimedia == m_multimedia->currentIndex()) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  m_task->m_multimedia = (m_multimedia->currentIndex());
  m_commandLine->setText(m_task->getCommandLine());
  BatchesController::instance()->setDirtyFlag(true);

  // Update children tasks, if present.
  TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);
  if (taskGroup) {
    for (int i = 0; i < taskGroup->getTaskCount(); ++i)
      taskGroup->getTask(i)->m_multimedia = taskGroup->m_multimedia;
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setThreadsCombo(int) {
  if (!m_task) return;
  if (m_task->m_threadsIndex == m_threadsCombo->currentIndex()) return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  m_task->m_threadsIndex = (m_threadsCombo->currentIndex());
  m_commandLine->setText(m_task->getCommandLine());
  BatchesController::instance()->setDirtyFlag(true);

  // Update children tasks, if present.
  TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);
  if (taskGroup) {
    for (int i = 0; i < taskGroup->getTaskCount(); ++i)
      taskGroup->getTask(i)->m_threadsIndex = taskGroup->m_threadsIndex;
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setGranularityCombo(int) {
  if (!m_task) return;
  if (m_task->m_maxTileSizeIndex == m_rasterGranularityCombo->currentIndex())
    return;
  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  m_task->m_maxTileSizeIndex = (m_rasterGranularityCombo->currentIndex());
  m_commandLine->setText(m_task->getCommandLine());
  BatchesController::instance()->setDirtyFlag(true);

  // Update children tasks, if present.
  TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);
  if (taskGroup) {
    for (int i = 0; i < taskGroup->getTaskCount(); ++i)
      taskGroup->getTask(i)->m_maxTileSizeIndex = taskGroup->m_maxTileSizeIndex;
  }

  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

void TaskSheet::setOutput() {
  if (!m_task) return;
  if (m_task->m_outputPath == TFilePath(m_outputPath->text().toStdWString()))
    return;

  if (m_task->m_completionDate.isNull() && !m_task->m_startDate.isNull()) {
    update(m_task);
    return;
  }

  m_task->m_outputPath = TFilePath(m_outputPath->text().toStdWString());
  m_commandLine->setText(m_task->getCommandLine());

  TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(m_task);

  // Check chunk size consistency
  if (taskGroup && !isMovieType(m_task->m_outputPath.getType())) {
    m_chunkSize->setEnabled(true);
    m_chunkSize->setText(QString::number(m_task->m_chunkSize));

    // Update children outputPaths
    for (int i = 0; i < taskGroup->getTaskCount(); ++i)
      taskGroup->getTask(i)->m_outputPath = taskGroup->m_outputPath;
  } else {
    m_chunkSize->setEnabled(false);
    m_chunkSize->setText(QString::number(m_task->m_stepCount));

    setChunkSize();
  }

  BatchesController::instance()->setDirtyFlag(true);
  m_viewer->startTimer();
}

//-----------------------------------------------------------------------------

TaskSheet::TaskSheet(TasksViewer *owner) : QScrollArea(owner) {
  QFrame *contentWidget = new QFrame(this);
  contentWidget->setMinimumWidth(300);
  contentWidget->setMinimumHeight(400);

  setWidget(contentWidget);
  setWidgetResizable(true);

  m_task   = 0;
  int row  = 0;
  m_viewer = owner;

  QGridLayout *layout = new QGridLayout(contentWidget);
  layout->setMargin(15);
  layout->setSpacing(8);
  layout->setColumnStretch(3, 1);
  layout->setColumnStretch(5, 1);
  layout->setColumnStretch(6, 4);
  layout->setColumnMinimumWidth(2, 70);
  layout->setColumnMinimumWidth(3, 70);
  layout->setColumnMinimumWidth(4, 40);
  layout->setColumnMinimumWidth(5, 100);
  contentWidget->setLayout(layout);
  ::create(m_name, layout, tr("Name:"), row++);
  ::create(m_status, layout, tr("Status:"), row++);
  ::create(m_commandLine, layout, tr("Command Line:"), row++);
  ::create(m_server, layout, tr("Server:"), row++);
  ::create(m_submittedBy, layout, tr("Submitted By:"), row++);
  ::create(m_submittedOn, layout, tr("Submitted On:"), row++);
  ::create(m_submitDate, layout, tr("Submission Date:"), row++);
  ::create(m_startDate, layout, tr("Start Date:"), row++);
  ::create(m_complDate, layout, tr("Completion Date:"), row++);
  ::create(m_duration, layout, tr("Duration:"), row++);
  // m_duration->setMaximumWidth(38);
  ::create(m_stepCount, layout, tr("Step Count:"), row++);
  // m_stepCount->setMaximumWidth(38);
  ::create(m_failedSteps, layout, tr("Failed Steps:"), row++);
  // m_failedSteps->setMaximumWidth(38);
  ::create(m_succSteps, layout, tr("Successful Steps:"), row++);
  // m_succSteps->setMaximumWidth(38);
  ::create(m_priority, layout, tr("Priority:"), row++);
  // m_priority->setMaximumWidth(40);

  layout->setColumnStretch(0, 0);
  layout->setColumnStretch(1, 0);
  layout->setColumnStretch(2, 1);
  layout->setColumnStretch(3, 1);
  layout->setColumnStretch(4, 1);
  layout->setColumnStretch(5, 1);

  int row1 = 0;

  m_boxComposer = new QFrame(contentWidget);
  m_boxComposer->setMinimumHeight(150);
  QGridLayout *layout1 = new QGridLayout(m_boxComposer);
  layout1->setMargin(0);
  layout1->setSpacing(8);
  m_boxComposer->setLayout(layout1);
  ::create(m_outputPath, layout1, tr("Output:"), row1++, 4);

  ::create(m_chunkSize, m_multimedia, layout1, tr("Frames per Chunk:"),
           tr("Multimedia:"), Qt::AlignRight | Qt::AlignTop,
           Qt::AlignLeft | Qt::AlignTop, row1++);
  ::create(m_from, m_to, layout1, tr("From:"), tr("To:"),
           Qt::AlignRight | Qt::AlignTop, Qt::AlignRight | Qt::AlignTop,
           row1++);
  ::create(m_step, m_shrink, layout1, tr("Step:"), tr("Shrink:"),
           Qt::AlignRight | Qt::AlignTop, Qt::AlignRight | Qt::AlignTop,
           row1++);

  layout1->setColumnMinimumWidth(2, 40);
  QStringList multimediaTypes;
  multimediaTypes << tr("None") << tr("Fx Schematic Flows")
                  << tr("Fx Schematic Terminal Nodes");
  m_multimedia->addItems(multimediaTypes);

  ::create(m_threadsCombo, layout1, tr("Dedicated CPUs:"), row1++, 3);
  QStringList threadsTypes;
  threadsTypes << tr("Single") << tr("Half") << tr("All");
  m_threadsCombo->addItems(threadsTypes);

  ::create(m_rasterGranularityCombo, layout1, tr("Render Tile:"), row1++, 3);
  QStringList granularityTypes;
  granularityTypes << tr("None") << tr("Large") << tr("Medium") << tr("Small");
  m_rasterGranularityCombo->addItems(granularityTypes);

  layout1->addWidget(new QWidget(), 0, 5);

  layout1->setColumnStretch(0, 0);
  layout1->setColumnStretch(1, 0);
  layout1->setColumnStretch(2, 0);
  layout1->setColumnStretch(3, 0);
  layout1->setColumnStretch(4, 0);
  layout1->setColumnStretch(5, 1);

  layout->addWidget(m_boxComposer, row++, 0, 1,
                    6 /*, Qt::AlignLeft|Qt::AlignTop*/);

  // tcleanupper Box
  m_boxCleanup         = new QFrame(contentWidget);
  QGridLayout *layout2 = new QGridLayout(m_boxCleanup);
  layout2->setMargin(0);
  layout2->setSpacing(8);
  m_boxCleanup->setLayout(layout2);
  ::create(m_visible, layout2, tr("Visible Only"), 0);
  ::create(m_overwrite, layout2, tr("Overwrite"), 1, 1);

  QStringList overwriteOptions;
  overwriteOptions << tr("All") << tr("NoPaint") << tr("Off");
  m_overwrite->addItems(overwriteOptions);
  m_overwrite->setCurrentIndex(2);  // do not overwrite by default

  layout2->setColumnStretch(0, 0);
  layout2->setColumnStretch(1, 0);
  layout2->setColumnStretch(2, 1);

  layout->addWidget(m_boxCleanup, row++, 2, 1, 1, Qt::AlignLeft | Qt::AlignTop);

  QLabel *label = new QLabel(tr("Dependencies:"), contentWidget);
  label->setObjectName("TaskSheetItemLabel");
  layout->addWidget(label, row, 0, 1, 2, Qt::AlignRight | Qt::AlignTop);

  m_addedBox = new QListWidget(contentWidget);
  m_addedBox->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_addedBox->setObjectName("tasksRemoveBox");
  m_addedBox->setFrameStyle(QFrame::StyledPanel);
  layout->addWidget(m_addedBox, row, 2, 1, 2, Qt::AlignLeft | Qt::AlignTop);

  m_notAddedBox = new QListWidget(contentWidget);
  m_notAddedBox->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_notAddedBox->setObjectName("tasksAddBox");
  m_notAddedBox->setFrameStyle(QFrame::StyledPanel);
  layout->addWidget(m_notAddedBox, row++, 4, 1, 2,
                    Qt::AlignLeft | Qt::AlignTop);

  QPushButton *removeBtn = new QPushButton(tr("Remove >>"));
  layout->addWidget(removeBtn, row, 2, 1, 2, Qt::AlignRight | Qt::AlignTop);

  QPushButton *addBtn = new QPushButton(tr("<< Add"));
  layout->addWidget(addBtn, row++, 4, 1, 2, Qt::AlignLeft | Qt::AlignTop);

  bool ret = true;

  ret =
      ret && connect(m_name, SIGNAL(editingFinished()), this, SLOT(setName()));
  ret =
      ret && connect(m_from, SIGNAL(editingFinished()), this, SLOT(setFrom()));
  ret = ret && connect(m_to, SIGNAL(editingFinished()), this, SLOT(setTo()));
  ret =
      ret && connect(m_step, SIGNAL(editingFinished()), this, SLOT(setStep()));
  ret = ret &&
        connect(m_shrink, SIGNAL(editingFinished()), this, SLOT(setShrink()));
  ret = ret && connect(m_outputPath, SIGNAL(editingFinished()), this,
                       SLOT(setOutput()));
  ret = ret && connect(m_chunkSize, SIGNAL(editingFinished()), this,
                       SLOT(setChunkSize()));
  ret = ret && connect(m_priority, SIGNAL(editingFinished()), this,
                       SLOT(setPriority()));

  ret = ret && connect(m_name, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_from, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_to, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_step, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_shrink, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret =
      ret && connect(m_outputPath, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_chunkSize, SIGNAL(focusIn()), this, SLOT(onFocusIn()));
  ret = ret && connect(m_priority, SIGNAL(focusIn()), this, SLOT(onFocusIn()));

  ret = ret && connect(m_multimedia, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(setMultimedia(int)));
  ret = ret && connect(m_visible, SIGNAL(stateChanged(int)), this,
                       SLOT(setVisible(int)));

  ret = ret && connect(m_overwrite, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(setOverwrite(int)));

  ret =
      ret && connect(m_addedBox, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
                     this, SLOT(onRemovedItemDoubleClicked(QListWidgetItem *)));
  ret = ret &&
        connect(m_notAddedBox, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
                this, SLOT(onAddedItemDoubleClicked(QListWidgetItem *)));

  ret = ret &&
        connect(removeBtn, SIGNAL(clicked(bool)), this, SLOT(onRemoved(bool)));
  ret =
      ret && connect(addBtn, SIGNAL(clicked(bool)), this, SLOT(onAdded(bool)));

  ret = ret && connect(m_threadsCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(setThreadsCombo(int)));
  ret =
      ret && connect(m_rasterGranularityCombo, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(setGranularityCombo(int)));

  assert(ret);

  // The following instruction is needed in order to prevent contentWidget from
  // shrinking
  // beyond the tolerable size for its grid layout.
  contentWidget->setMinimumSize(layout->minimumSize());

  widget()->hide();
}

//=============================================================================

void TasksViewer::setSelected(TFarmTask *task) { m_taskSheet->update(task); }

//=============================================================================

OpenFloatingPanel openTasksCommand(MI_OpenTasks, "Tasks", QObject::tr("Tasks"));

//=============================================================================
//=============================================================================
//=============================================================================

TaskTreeView::TaskTreeView(TasksViewer *parent, TaskTreeModel *treeModel)
    : TreeView(parent), m_mainViewer(parent) {
  if (!treeModel) treeModel = new TaskTreeModel(this);
  setModel(treeModel);
  setObjectName("taskeditortree");
  setIconSize(QSize(21, 17));

  // connect(this, SIGNAL(pressed      (const QModelIndex &) ), this,
  // SLOT(onActivated(const QModelIndex &)));
}

//----------------------------------------------------------------------------------------------------------------
/*
void TaskTreeView::mousePressEvent ( QMouseEvent * event )
{
QTreeView::mousePressEvent(event);
if (selectedIndexes().empty()) return;
QModelIndex index = selectedIndexes().at(0);
if (!index.isValid()) return;

((TaskTreeModel*)model())->setSelected(index);

m_mainViewer->setSelected(((TaskTreeModel::Item*)index.internalPointer())->getTask());

if (event->button()==Qt::RightButton)
  ((TaskTreeModel*)model())->openContextMenu(event->globalPos());

}
*/

void TaskTreeView::onClick(TreeModel::Item *gItem, const QPoint &pos,
                           QMouseEvent *e) {
  TaskTreeModel::Item *item = static_cast<TaskTreeModel::Item *>(gItem);
  m_mainViewer->setSelected(item->getTask());
  // ((TaskTreeModel*)model())->setSelected(gItem->createIndex());
}

void TaskTreeView::openContextMenu(TreeModel::Item *gItem,
                                   const QPoint &globalPos) {
  TaskTreeModel::Item *item = static_cast<TaskTreeModel::Item *>(gItem);
  TaskTreeModel *m          = static_cast<TaskTreeModel *>(model());
  // m->setSelected(item->createIndex());
  if (item->getDepth() == 1) {
    static QMenu globalMenu;
    if (globalMenu.isEmpty()) {
      const std::vector<QAction *> &actions = m_mainViewer->getActions();
      assert(!actions.empty());
      int i;
      for (i = 0; i < actions.size(); i++) {
        globalMenu.addAction(actions[i]);
        if (i == 1 || i == 3 || i == 6) globalMenu.addSeparator();
      }
    }
    globalMenu.exec(globalPos);
  } else if (item->getDepth() > 1) {
    static QMenu menu;
    if (menu.isEmpty()) {
      bool ret = true;
      QAction *action;
      action = new QAction(tr("Start"), this);
      ret =
          ret && connect(action, SIGNAL(triggered(bool)), m, SLOT(start(bool)));
      menu.addAction(action);
      action = new QAction(tr("Stop"), this);
      ret =
          ret && connect(action, SIGNAL(triggered(bool)), m, SLOT(stop(bool)));
      menu.addAction(action);
      action = new QAction(tr("Remove"), this);
      menu.addAction(action);
      ret = ret &&
            connect(action, SIGNAL(triggered(bool)), m, SLOT(remove(bool)));
      assert(ret);
    }
    menu.exec(globalPos);
  }
}

//----------------------------------------------------------------------------------------------------------------

/*
 void TaskTreeView::onActivated(const QModelIndex &index)
 {
 if (!index.isValid())
   return;

if (!isExpanded(index))
  {
  setExpanded(index, true);
  ((TaskTreeModel*)model())->onExpanded(index);
  }

((FunctionTreeModel*)model())->onActivated(index);
}
*/

//----------------------------------------------------------------------------------------------------------------

TaskTreeModel::TaskTreeModel(TaskTreeView *parent)
    : TreeModel(parent), m_view(parent) {
  setupModelData();
  emit layoutChanged();
}

//------------------------------------------------------------------------------------------------------------------

QVariant TaskTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  Item *item = (Item *)index.internalPointer();

  if (role == Qt::DecorationRole) {
    switch (item->getDepth()) {
    case 0:
      return QVariant();
    case 1:
      return QIcon(":Resources/farm_tasks.svg");
    case 2:
    case 3:
      TFarmTask *t         = item->getTask();
      bool sourceFileIsCLN = (t->m_taskFilePath.getType() == "cln");
      switch (t->m_status) {
      case Suspended:
        return QIcon(t->m_isComposerTask
                         ? ":Resources/render_suspended.svg"
                         : (sourceFileIsCLN
                                ? ":Resources/cln_suspended.svg"
                                : ":Resources/cleanup_suspended.svg"));
      case Waiting:
        return QIcon(t->m_isComposerTask
                         ? ":Resources/render_done_with_errors.svg"
                         : (sourceFileIsCLN
                                ? ":Resources/cln_done_with_errors.svg"
                                : ":Resources/cleanup_done_with_errors.svg"));
      case Running:
        return QIcon(t->m_isComposerTask
                         ? ":Resources/render_computing.svg"
                         : (sourceFileIsCLN
                                ? ":Resources/cln_computing.svg"
                                : ":Resources/cleanup_computing.svg"));
      case Completed:
        return QIcon(t->m_isComposerTask
                         ? ":Resources/render_done.svg"
                         : (sourceFileIsCLN ? ":Resources/cln_done.svg"
                                            : ":Resources/cleanup_done.svg"));
      case Aborted:
      case TaskUnknown:
        return QIcon(t->m_isComposerTask
                         ? ":Resources/render_failed.svg"
                         : (sourceFileIsCLN ? ":Resources/cln_failed.svg"
                                            : ":Resources/cleanup_failed.svg"));
      default:
        assert(false);
      }
    }
    assert(false);
  } else if (role == Qt::DisplayRole)
    return item->m_task ? item->m_task->m_name : item->m_name;

  return QVariant();
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::save(bool) {
  BatchesController::instance()->save();
  // SaveTaskListPopup popup;
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::saveas(bool) { BatchesController::instance()->saveas(); }

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::load(bool) { BatchesController::instance()->load(); }

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::addRenderTask(bool) {
  BatchesController::instance()->loadTask(true);
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::addCleanupTask(bool) {
  BatchesController::instance()->loadTask(false);
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::start(bool) {
  try {
    TMsgCore::instance()->openConnection();
    QModelIndexList indexList = m_view->getSelectedIndexes();
    int i;

    if (indexList.size() == 0)
      BatchesController::instance()->startAll();
    else
      for (i = 0; i < indexList.size(); i++) {
        QModelIndex modelIndex = indexList.at(i);
        if (!modelIndex.isValid()) continue;
        TaskTreeModel::Item *item = (Item *)modelIndex.internalPointer();
        if (item->getDepth() == 1)  // significa che ho selezionato la radice!
        {
          assert(indexList.size() == 1);
          BatchesController::instance()->startAll();
          break;
        }

        TFarmTask *task = item->getTask();
        if (!task) continue;
        BatchesController::instance()->start(task->m_id);
      }
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  }

  emit layoutChanged();
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::stop(bool) {
  try {
    QModelIndexList indexList = m_view->getSelectedIndexes();
    int i;
    if (indexList.empty())
      BatchesController::instance()->stopAll();
    else
      for (i = 0; i < indexList.size(); i++) {
        QModelIndex modelIndex = indexList.at(i);
        if (!modelIndex.isValid()) continue;
        TaskTreeModel::Item *item = (Item *)modelIndex.internalPointer();
        if (item->getDepth() == 1)  // significa che ho selezionato la radice!
        {
          assert(indexList.size() == 1);
          BatchesController::instance()->stopAll();
          break;
        }
        TFarmTask *task = item->getTask();
        if (!task) continue;
        BatchesController::instance()->stop(task->m_id);
      }
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  }

  emit layoutChanged();
}

//------------------------------------------------------------------------------------------------------------------

void TaskTreeModel::remove(bool) {
  QModelIndexList indexList = m_view->getSelectedIndexes();
  int i;

  if (indexList.size() == 0) {
    if (DVGui::MsgBox(tr("Are you sure you want to remove ALL tasks?"),
                      tr("Remove All"), tr("Cancel")) > 1)
      return;

    BatchesController::instance()->removeAllTasks();
  } else {
    for (i = 0; i < indexList.size(); i++) {
      QModelIndex modelIndex = indexList.at(i);
      if (!modelIndex.isValid()) continue;
      TaskTreeModel::Item *item = (Item *)modelIndex.internalPointer();
      if (item->getDepth() == 1)  // significa che ho selezionato la radice!
      {
        assert(indexList.size() == 1);
        BatchesController::instance()->removeAllTasks();
        break;
      }
      TFarmTask *task = item->getTask();
      if (!task) continue;
      BatchesController::instance()->removeTask(task->m_id);
    }
    TaskTreeView *p = (TaskTreeView *)QObject::parent();

    p->m_mainViewer->setSelected(0);
  }

  setupModelData();
  emit layoutChanged();
}

//------------------------------------------------------------------------------------------------------------------

// WARNING: probabilmente non e' piu' necessaria
// Se serve bisogna stare attenti: e' necessario emettere
// prima layoutAboutToBeChanged() e poi aggiornare gli indici
// permanenti

void TaskTreeModel::update() {
  // assert(0);
  emit layoutChanged();
}

//---------------------------------------------------------------------------------------------------------------

//! setupModelData() : refresh the model
void TaskTreeModel::setupModelData() {
  beginRefresh();

  BatchesController *controller = BatchesController::instance();
  QString taskListName          = controller->getListName();

  // setup root and tasklist. set the taskList's name
  Item *root     = 0;
  Item *taskList = 0;
  if (!getRootItem()) {
    setRootItem(root = new Item("Root"));
    root->appendChild(taskList = new Item(taskListName));
  } else {
    root     = static_cast<Item *>(getRootItem());
    taskList = static_cast<Item *>(root->getChild(0));
    taskList->setName(taskListName);
    // TODO: emettere un dataChanged se il nome e' diverso?
  }
  assert(root);
  assert(root->getChildCount() == 1);

  // update the list of tasks
  int i, j;
  QList<TreeModel::Item *> tasks;
  for (i = 0; i < controller->getTaskCount(); i++) {
    TFarmTaskGroup *group =
        dynamic_cast<TFarmTaskGroup *>(controller->getTask(i));
    if (group) tasks.push_back(new Item(group));
  }
  taskList->setChildren(tasks);

  // foreach task (the new ones, but also the old ones) update
  // the list of sub-tasks
  for (i = 0; i < taskList->getChildCount(); i++) {
    TreeModel::Item *item = taskList->getChild(i);
    TFarmTaskGroup *group =
        static_cast<TFarmTaskGroup *>(item->getInternalPointer());
    QList<TreeModel::Item *> subTasks;
    for (j = 0; j < group->getTaskCount(); j++)
      subTasks.push_back(new Item(group->getTask(j)));
    item->setChildren(subTasks);
  }
  endRefresh();
  m_view->expandAll();
}

//------------------------------------------------------------------------------------------------------------------

TaskTreeModel::Item::Item(const QString &name)
    : TreeModel::Item(), m_name(name), m_task(0) {
  assert(!m_name.isEmpty());
}
/*
  TFarmTask*t = BatchesController::instance()->getTask(i);
//------------------------------------------------------------------------------------------------------------------
  if (group = dynamic_cast<TFarmTaskGroup*>(t))
*/

TaskTreeModel::Item::Item(TFarmTask *task)
    : TreeModel::Item(), m_name(), m_task(task) {
  assert(m_task);
}
