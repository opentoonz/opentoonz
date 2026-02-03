#pragma once

#ifndef TNZ_TOOLBAR_INCLUDED
#define TNZ_TOOLBAR_INCLUDED

//#include "tw/action.h"
#include "traster.h"

#include "tw/tw.h"

class TGenericCommandAction;

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TButtonSet;

//---------------------------------------------------------------

class DVAPI TToolButtonInfo {
  string m_name;
  TRaster32P m_downIcon, m_upIcon;
  TGenericCommandAction *m_action;
  TButtonSet *m_buttonSet;

public:
  TToolButtonInfo(string name);
  ~TToolButtonInfo();

  void setAction(TGenericCommandAction *action);
  void setIcon(TRaster32P upIcon, TRaster32P downIcon);
  void setIcon(TRaster32P icon);
  void setButtonSet(TButtonSet *bs);

  TWidget *createToolButton(TWidget *parent);
  string getName() const { return m_name; };
};

//---------------------------------------------------------------

class DVAPI TToolbar : public TWidget {
  TWidget *m_dockWidget;

public:
  // static TGuiColor ToolbarColor;

  class DVAPI Space : public TWidget {
  public:
    Space(TWidget *parent);
    void draw();
  };

  TToolbar(TWidget *parent, string name);

  void draw();
  void configureNotify(const TDimension &size);

  void leftButtonDown(const TMouseEvent &);
  void close();
};

#endif
