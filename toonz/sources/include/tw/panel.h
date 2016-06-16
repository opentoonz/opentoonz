#pragma once

#ifndef TNZ_PANEL_INCLUDED
#define TNZ_PANEL_INCLUDED

#include "tw/tw.h"
#include "tw/button.h"

class TPanel : public TWidget {
  int m_border, m_resizeHandleWidth;
  TWidget *m_dockWidget;
  TButton *m_maximizeButton;
  bool m_maximized;
  int m_status;

public:
  enum Side { Center, Top, Right, Bottom, Left };

public:
  TPanel(TWidget *parent, string name);

  void draw();
  void keyDown(int key, TUINT32 flags, const TPoint &p);
  void close();

  void dockToggle();
  void maximizeToggle();

  void configureNotify(const TDimension &d);
  void mouseMove(const TPoint &p);
  void leftButtonDown(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void leave(const TPoint &p);
  void enter(const TPoint &p);

  Side findSide(const TPoint &p);
  bool canMoveSide(Side side);

  static TPanel *createPanel(TWidget *parent, string name);
  void setParent(TWidget *p) {
    TWidget::setParent(p);
    m_dockWidget = p;
  };
};

class TPanelResizer {
public:
  TPanelResizer(){};
  virtual bool canMoveSide(TPanel *panel, TPanel::Side side) = 0;
  virtual void moveSide(TPanel *panel, TPanel::Side side)    = 0;
};

#endif
