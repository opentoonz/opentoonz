#pragma once

#ifndef HISTORYPANE_H
#define HISTORYPANE_H

#include <QScrollArea>

class HistoryField final : public QFrame {
  Q_OBJECT

  QScrollArea *m_scrollArea;

public:
  HistoryField(QScrollArea *parent   = 0,
               Qt::WindowFlags flags = Qt::WindowFlags());
  ~HistoryField(){};

  void updateContentHeight(int minimumHeight = -1);
  int y2index(int y) const;
  int index2y(int index) const;

  void exposeCurrent();

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *event) override;
};

class HistoryPane final : public QWidget {
  Q_OBJECT

  HistoryField *m_field;
  QScrollArea *m_frameArea;

public:
  HistoryPane(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~HistoryPane(){};

protected:
  void resizeEvent(QResizeEvent *) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public slots:
  void onHistoryChanged();
};

#endif