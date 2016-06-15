#pragma once

#ifndef TNZ_SCROLLVIEW_INCLUDED
#define TNZ_SCROLLVIEW_INCLUDED

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

class TScrollbar;

class DVAPI TScrollView : public TWidget {
  TPoint m_delta;
  TScrollbar *m_vsb, *m_hsb;
  TDimension m_contentSize;
  bool m_autopanning;
  TPoint m_autopanningPos;
  int m_autopanningOldT;

public:
  TScrollView(TWidget *parent, std::string name = "scrollview");
  ~TScrollView();

  void middleButtonDown(const TMouseEvent &e);
  void middleButtonDrag(const TMouseEvent &e);
  void middleButtonUp(const TMouseEvent &e);

  void configureNotify(const TDimension &d);

  void doScrollTo(const TPoint &p);
  inline void doScrollTo(int x, int y) { doScrollTo(TPoint(x, y)); }
  inline void doScrollTo_x(int x) { doScrollTo(TPoint(x, m_yoff)); }
  inline void doScrollTo_y(int y) { doScrollTo(TPoint(m_xoff, y)); }

  virtual void scrollTo(const TPoint &p) { doScrollTo(p); }
  inline void scrollTo(int x, int y) { scrollTo(TPoint(x, y)); }

  inline TPoint getScrollPosition() const { return TPoint(m_xoff, m_yoff); }

  void setScrollbars(TScrollbar *hsb, TScrollbar *vsb);
  void setContentSize(const TDimension &d);

  TDimension getContentSize() const { return m_contentSize; }

  void onVsbChange(int value);
  void onHsbChange(int value);

  void onTimer(int t);

  void startAutopanning();
  void stopAutopanning();

  bool isAutopanning() const { return m_autopanning; }

  virtual void autopanningDrag(const TPoint &pos){};

  // provvisorio
  void updateScrollbars();
};

#endif
