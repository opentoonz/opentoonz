#pragma once

#ifndef TNZ_TABBEDWINDOW_INCLUDED
#define TNZ_TABBEDWINDOW_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/*
class DVAPI TTabbedWindow : public TWidget {
  class Data;
  Data *m_data;

public:
  TTabbedWindow(TWidget *parent, string name = "tabbedWindow");
  ~TTabbedWindow();

  void addPanel(TWidget *panel);

  int getTabCount() const;
  string getTabName(int i) const;

  TWidget *getCurrentPanel() const;

  void repaint();

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp  (const TMouseEvent &e);

  void configureNotify(const TDimension &d);

};

*/

class DVAPI TTabbedWidget : public TWidget {
  int m_currentPageIndex;
  TGuiColor m_tabColor, m_tabHlColor;

public:
  TTabbedWidget(TWidget *parent, string name = "tabbedwidget");

  void configureNotify(const TDimension &d);

  int page2x0(int index) const;
  int page2x1(int index) const;
  int x2page(int x) const;

  virtual void drawTab(const TRect &rect, int index);
  virtual void onPageChange(TWidget *oldPage, TWidget *newPage) {}

  TWidget *getCurrentPage() const;

  void repaint();

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);
  void mouseMove(const TMouseEvent &e);
  void enter(const TPoint &p);
  void leave(const TPoint &p);
};

#endif
