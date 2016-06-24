#pragma once

#ifndef BATCHSERVERSVIEWER_H
#define BATCHSERVERSVIEWER_H

#include <memory>

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/lineedit.h"

#include <QFrame>
#include <QListWidget>
#include <QMenu>

class QComboBox;
class FarmServerListView;
class QListWidgetItem;

//=============================================================================
// BatchServersViewer

class FarmServerListView final : public QListWidget {
  Q_OBJECT
public:
  FarmServerListView(QWidget *parent);

  ~FarmServerListView(){};

  void update();

protected slots:

  void activate();
  void deactivate();

private:
  void openContextMenu(const QPoint &p);
  void mousePressEvent(QMouseEvent *event) override;
  std::unique_ptr<QMenu> m_menu;
};

class BatchServersViewer final : public QFrame {
  Q_OBJECT

public:
#if QT_VERSION >= 0x050500
  BatchServersViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  BatchServersViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~BatchServersViewer();

  void updateSelected();

protected slots:
  void setGRoot();
  void onProcessWith(int index);
  void onCurrentItemChanged(QListWidgetItem *);

private:
  QString m_serverId;
  DVGui::LineEdit *m_farmRootField;
  QComboBox *m_processWith;
  FarmServerListView *m_serverList;

  DVGui::LineEdit *m_name;
  DVGui::LineEdit *m_ip;
  DVGui::LineEdit *m_port;
  DVGui::LineEdit *m_tasks;
  DVGui::LineEdit *m_state;
  DVGui::LineEdit *m_cpu;
  DVGui::LineEdit *m_mem;
  void updateServerInfo(const QString &id);
};

#endif  // BATCHSERVERSVIEWER_H
