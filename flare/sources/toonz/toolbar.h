#pragma once

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QToolBar>

class QToolButton;

class Toolbar final : public QToolBar {
  Q_OBJECT

  std::map<std::string, QAction *> m_toolbarList;
  QToolButton *m_expandButton;
  QAction *m_expandAction;
  bool m_isExpanded;
  int m_toolbarLevel;

public:
  Toolbar(QWidget *parent, bool isVertical = true);
  ~Toolbar();

protected:
  bool addAction(QAction *act);

  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:
  void onToolChanged();
  void onPreferenceChanged(const QString &prefName);
  void setIsExpanded(bool expand);
  void updateToolbar();
};

#endif  // TOOLBAR_H
