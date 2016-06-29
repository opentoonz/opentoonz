#pragma once

#ifndef TASKSVIEWER_H
#define TASKSVIEWER_H

#include <QSplitter>
#include <QScrollArea>

#include "toonzqt/treemodel.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonz/observer.h"
#include "batches.h"

class TFarmTask;
class TaskTreeView;
class QListWidgetItem;
class QComboBox;

class TaskTreeModel final : public TreeModel {
  Q_OBJECT

  // QModelIndex m_selectedIndex;
  TaskTreeView *m_view;

public:
  class Item final : public TreeModel::Item {
    friend class TaskTreeModel;

  public:
    Item(const QString &name);
    Item(TFarmTask *task);
    //~Item();
    void *getInternalPointer() const override { return m_task; }
    TFarmTask *getTask() const { return m_task; }
    void setName(QString name) { m_name = name; }
    QString getName() const { return m_name; }

  private:
    TFarmTask *m_task;
    QString m_name;
  };  // class TaskItem

  TaskTreeModel(TaskTreeView *parent = 0);
  //~TaskTreeModel();
  // void setSelected(const QModelIndex& index) {m_selectedIndex=index;}
  // void openContextMenu(const QPoint& p);

  void update();
public slots:
  void start(bool);
  void stop(bool);
  void remove(bool);
  void save(bool);
  void saveas(bool);
  void load(bool);
  void addCleanupTask(bool);
  void addRenderTask(bool);

  void setupModelData();

protected:
  QVariant data(const QModelIndex &index, int role) const override;

private:
  // void setLayout(Item *oldPegs);
};

//------------------------------------------------------------------------------------------
class TasksViewer;

class TaskTreeView final : public TreeView {
  Q_OBJECT

public:
  TasksViewer *m_mainViewer;
  TaskTreeView(TasksViewer *parent, TaskTreeModel *treeModel);

  QModelIndexList getSelectedIndexes() const { return selectedIndexes(); }

protected:
  void onClick(TreeModel::Item *item, const QPoint &pos,
               QMouseEvent *e) override;
  void openContextMenu(TreeModel::Item *item, const QPoint &globalPos) override;
};

//------------------------------------------------------------------------------------------

//=============================================================================
class QLabel;
class QGridLayout;
class TFarmTask;
class QTextEdit;
class QListWidget;

class TaskSheet final : public QScrollArea {
  Q_OBJECT

  TasksViewer *m_viewer;

  QFrame *m_boxCleanup;
  QFrame *m_boxComposer;

  TFarmTask *m_task;
  DVGui::LineEdit *m_name;
  DVGui::LineEdit *m_priority;
  DVGui::LineEdit *m_from;
  DVGui::LineEdit *m_to;
  DVGui::LineEdit *m_step;
  DVGui::LineEdit *m_shrink;
  DVGui::LineEdit *m_chunkSize;
  DVGui::LineEdit *m_outputPath;
  QLabel *m_id;
  QLabel *m_status;
  QTextEdit *m_commandLine;
  QLabel *m_server;
  QLabel *m_submittedBy;
  QLabel *m_submittedOn;
  QLabel *m_submitDate;
  QLabel *m_startDate;
  QLabel *m_complDate;
  QLabel *m_duration;
  QLabel *m_stepCount;
  QLabel *m_failedSteps;
  QLabel *m_succSteps;
  QListWidget *m_addedBox;
  QListWidget *m_notAddedBox;
  DVGui::CheckBox *m_visible;
  QComboBox *m_overwrite;
  QComboBox *m_multimedia;
  QComboBox *m_threadsCombo;
  QComboBox *m_rasterGranularityCombo;

protected slots:

  void onAdded(bool);
  void onRemoved(bool);
  void onRemovedItemDoubleClicked(QListWidgetItem *item);
  void onAddedItemDoubleClicked(QListWidgetItem *item);
  void onFocusIn();
  void setName();
  void setFrom();
  void setTo();
  void setShrink();
  void setStep();
  void setOutput();
  void setChunkSize();
  void setVisible(int);
  void setOverwrite(int);
  void setMultimedia(int);
  void setThreadsCombo(int);
  void setGranularityCombo(int);
  void setPriority();

public:
  TaskSheet(TasksViewer *owner);
  void update(TFarmTask *task);
  void updateChunks(TFarmTask *task);
  TFarmTask *getCurrentTask() { return m_task; }
};

//=============================================================================
class QToolBar;
class TasksViewer final : public QSplitter, public BatchesController::Observer {
  Q_OBJECT

public:
  TaskSheet *m_taskSheet;
  TaskTreeView *m_treeView;
  QTimer *m_timer;

#if QT_VERSION >= 0x050500
  TasksViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  TasksViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~TasksViewer();

  void update() override;

  void setSelected(TFarmTask *task);
  const std::vector<QAction *> &getActions() const;

  void startTimer();
  void stopTimer();

public slots:
  void onTimer();

protected:
  QWidget *createToolBar();
  std::vector<QAction *> m_actions;
  void add(const QString &iconName, QString text, QToolBar *toolBar,
           const char *slot, QString iconText);

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

//=============================================================================

#endif  // TASKSVIEWER_H
