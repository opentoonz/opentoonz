#pragma once

#ifndef HISTORYPANE_H
#define HISTORYPANE_H

#include <QScrollArea>

class HistoryField final : public QFrame {
  Q_OBJECT

  QScrollArea *m_scrollArea;

public:
#if QT_VERSION >= 0x050500
  HistoryField(QScrollArea *parent = 0, Qt::WindowFlags flags = 0);
#else
  HistoryField(QScrollArea *parent = 0, Qt::WFlags flags = 0);
#endif
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
#if QT_VERSION >= 0x050500
  HistoryPane(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  HistoryPane(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~HistoryPane(){};

protected:
  void resizeEvent(QResizeEvent *) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public slots:
  void onHistoryChanged();
};

#endif