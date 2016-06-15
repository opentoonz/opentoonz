#pragma once

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QToolBar>

class QToolButton;

class Toolbar : public QToolBar {
  Q_OBJECT

  QToolButton *m_expandButton;
  QAction *m_sep1, *m_sep2;
  bool m_isExpanded;

public:
  Toolbar(QWidget *parent, bool isVertical = true);
  ~Toolbar();

  bool isExpanded(){ return m_isExpanded; }

protected:
  bool addAction(QAction *act);

  void showEvent(QShowEvent *e);
  void hideEvent(QHideEvent *e);

protected slots:
  void onToolChanged();
  void updateToolbar(bool expand);
  void setIsExpanded(bool expand);
};

#endif  // TOOLBAR_H
