#pragma once

#ifndef TNZ_TW_CONTROL_PANEL_INCLUDED
#define TNZ_TW_CONTROL_PANEL_INCLUDED

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

class DVAPI TControlPanel : public TWidget {
  class Layout;
  Layout *m_layout;

public:
  TControlPanel(TWidget *parent, string name = "control panel");
  ~TControlPanel();

  void beginRow(Alignment align = BEGIN);
  void endRow();

  void add(TWidget *w, bool canResize = false);
  void addSeparator(string title = "");

  void setTab(int x);

  void draw();
  void configureNotify(const TDimension &d);

  TDimension getMinimumSize();
};

#endif
